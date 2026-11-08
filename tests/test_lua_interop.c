#include "lonejson_lua.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int luaopen_lonejson_core(lua_State *L);

typedef struct chunk_reader {
  const char *const *chunks;
  size_t count;
  size_t index;
  size_t offset;
  size_t calls;
  size_t bytes;
} chunk_reader;

static int failures;

static void expect_true(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}

static lonejson_read_result chunk_reader_read(void *user, unsigned char *buffer,
                                              size_t capacity) {
  chunk_reader *reader = (chunk_reader *)user;
  lonejson_read_result result = lonejson_default_read_result();
  const char *chunk;
  size_t available;

  ++reader->calls;
  if (reader->index >= reader->count) {
    result.eof = 1;
    return result;
  }
  chunk = reader->chunks[reader->index];
  available = strlen(chunk) - reader->offset;
  if (available > capacity) {
    available = capacity;
  }
  if (available != 0u) {
    memcpy(buffer, chunk + reader->offset, available);
    reader->offset += available;
    reader->bytes += available;
  }
  if (chunk[reader->offset] == '\0') {
    ++reader->index;
    reader->offset = 0u;
  }
  result.bytes_read = available;
  result.eof = reader->index >= reader->count;
  return result;
}

static int run_lua(lua_State *L, const char *source) {
  if (luaL_loadstring(L, source) != LUA_OK) {
    fprintf(stderr, "Lua load failed: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;
  }
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    fprintf(stderr, "Lua run failed: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;
  }
  return 0;
}

static void require_lonejson(lua_State *L, const char *source_dir) {
  luaL_openlibs(L);
  luaL_requiref(L, "lonejson.core", luaopen_lonejson_core, 1);
  lua_pop(L, 1);
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "path");
  lua_pushfstring(L, "%s/lua/?.lua;%s/lua/?/init.lua;%s", source_dir,
                  source_dir, lua_tostring(L, -1));
  lua_remove(L, -2);
  lua_setfield(L, -2, "path");
  lua_pop(L, 1);
}

static void *json_stack_worker(void *user) {
  const char *source_dir = (const char *)user;
  static const int depths[] = {1, 64, 127, 128, 129};
  size_t depth_index;
  int shape;

  for (shape = 0; shape < 3; ++shape) {
    for (depth_index = 0u; depth_index < sizeof(depths) / sizeof(depths[0]);
         ++depth_index) {
      lua_State *L = luaL_newstate();
      if (L == NULL) {
        expect_true(0, "fresh worker Lua state");
        return NULL;
      }
      require_lonejson(L, source_dir);
      fprintf(stderr, "JSON worker shape=%d depth=%d\n", shape,
              depths[depth_index]);
      lua_pushinteger(L, shape);
      lua_setglobal(L, "json_shape");
      lua_pushinteger(L, depths[depth_index]);
      lua_setglobal(L, "json_depth");
      /* Deliberately do not reserve VM slots in the embedding application. */
      expect_true(
          run_lua(
              L,
              "local j = require('lonejson')\n"
              "local text = '0'\n"
              "for i = 1, json_depth do\n"
              "  if json_shape == 1 or (json_shape == 2 and i % 2 == 0) then\n"
              "    text = '{\"x\":' .. text .. '}'\n"
              "  else text = '[' .. text .. ']' end\n"
              "end\n"
              "for attempt = 1, 3 do\n"
              "  local ok, value = pcall(j.decode_json, text)\n"
              "  if json_depth > 128 then\n"
              "    assert(not ok and tostring(value):find('nesting exceeds', "
              "1, true))\n"
              "  else\n"
              "    assert(ok, value)\n"
              "    assert(j.encode_json(value, {pretty = false}) == text)\n"
              "    local pretty = j.new({write_pretty = true})\n"
              "    "
              "assert(j.encode_json(j.decode_json(pretty.encode_json(value))) "
              "== text)\n"
              "    local bad = pcall(j.decode_json, text:sub(1, -2))\n"
              "    assert(not bad)\n"
              "  end\n"
              "  assert(j.decode_json('{\"ok\":true}').ok)\n"
              "  assert(not pcall(j.decode_json, string.rep('[', 129) .. "
              "string.rep(']', 129)))\n"
              "  local too_deep = {}; for i = 1, 128 do too_deep = {too_deep} "
              "end\n"
              "  assert(not pcall(j.encode_json, too_deep))\n"
              "  collectgarbage('collect')\n"
              "  local nested = { [string.rep('long', 64)] = 1 }\n"
              "  for i = 1, 40 do nested = {x = nested, [string.rep('key', i)] "
              "= i} end\n"
              "  local encoded = j.encode_json(nested)\n"
              "  assert(j.encode_json(j.decode_json(encoded)) == encoded)\n"
              "  nested.self = nested; assert(not pcall(j.encode_json, "
              "nested))\n"
              "  collectgarbage('collect')\n"
              "end\n") == 0,
          "fresh-state worker JSON round trip and recovery");
      lua_close(L);
      if (failures != 0) {
        return NULL;
      }
    }
  }
  return NULL;
}

static void test_json_worker_stack(const char *source_dir) {
  pthread_t worker;
  int status =
      pthread_create(&worker, NULL, json_stack_worker, (void *)source_dir);
  expect_true(status == 0, "create default-stack Lua worker");
  if (status == 0) {
    expect_true(pthread_join(worker, NULL) == 0, "join Lua worker");
  }
}

static void push_global(lua_State *L, const char *name) {
  lua_getglobal(L, name);
  expect_true(!lua_isnil(L, -1), "expected Lua global");
}

static void test_interop(lua_State *L) {
  lonejson_schema_view schema;
  lonejson_schema_view other_schema;
  lonejson_record_view record;
  lonejson_record_view checked;
  lonejson_error error;
  lonejson_status status;
  int schema_index;
  int record_index;
  int table_index;
  int registry_ref;
  const char *chunks[] = {"{\"company\":\"Pkt", "\",\"email\":\"ops@",
                          "pkt.systems\"}"};
  chunk_reader reader;

  if (run_lua(
          L, "local lonejson = require('lonejson')\n"
             "local lj = lonejson.new({overflow = 'fail'})\n"
             "schema = lj:schema('Contact', {\n"
             "  lonejson.field('company', lonejson.string({max = 16})),\n"
             "  lonejson.field('email', lonejson.string({max = 32})),\n"
             "})\n"
             "other_schema = lj:schema('Other', {\n"
             "  lonejson.field('company', lonejson.string({max = 16})),\n"
             "})\n"
             "tiny_schema = lj:schema('Tiny', {\n"
             "  lonejson.field('company', lonejson.string({fixed_capacity = 3, "
             "overflow = 'fail'})),\n"
             "})\n") != 0) {
    ++failures;
    return;
  }

  schema.size = sizeof(schema);
  schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  push_global(L, "schema");
  schema_index = lua_absindex(L, -1);
  status = lonejson_lua_check_schema(L, schema_index, &schema, &error);
  expect_true(status == LONEJSON_STATUS_OK, "schema check succeeds");
  expect_true(schema.runtime != NULL, "schema view has runtime");
  expect_true(schema.map != NULL, "schema view has map");
  expect_true(schema.record_size != 0u, "schema view has record size");

  registry_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_gc(L, LUA_GCCOLLECT, 0);
  lua_rawgeti(L, LUA_REGISTRYINDEX, registry_ref);
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_OK,
              "registry reference keeps schema usable");
  lua_pop(L, 1);

  record.size = sizeof(record);
  record.abi_version = LONEJSON_VIEW_ABI_VERSION;
  lua_rawgeti(L, LUA_REGISTRYINDEX, registry_ref);
  status = lonejson_lua_new_record(L, -1, &record_index, &record, &error);
  expect_true(status == LONEJSON_STATUS_OK, "new record succeeds");
  expect_true(record.record != NULL, "record view has storage");
  expect_true(record.schema.map == schema.map, "record view carries schema");

  memset(&reader, 0, sizeof(reader));
  reader.chunks = chunks;
  reader.count = sizeof(chunks) / sizeof(chunks[0]);
  status =
      schema.runtime->parse_reader(schema.runtime, schema.map, record.record,
                                   chunk_reader_read, &reader, &error);
  expect_true(status == LONEJSON_STATUS_OK, "manual fragmented parse succeeds");
  expect_true(reader.calls > 1u, "reader was called in fragments");
  table_index = lonejson_lua_record_to_table(L, record_index);
  expect_true(table_index != 0, "record converts to table");
  lua_getfield(L, table_index, "company");
  expect_true(strcmp(lua_tostring(L, -1), "Pkt") == 0, "table company matches");
  lua_pop(L, 1);
  lua_getfield(L, table_index, "email");
  expect_true(strcmp(lua_tostring(L, -1), "ops@pkt.systems") == 0,
              "table email matches");
  lua_pop(L, 1);
  lua_pop(L, 1);

  checked.size = sizeof(checked);
  checked.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status =
      lonejson_lua_check_record(L, record_index, &schema, &checked, &error);
  expect_true(status == LONEJSON_STATUS_OK, "record check succeeds");

  push_global(L, "other_schema");
  other_schema.size = sizeof(other_schema);
  other_schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status = lonejson_lua_check_schema(L, -1, &other_schema, &error);
  expect_true(status == LONEJSON_STATUS_OK, "other schema check succeeds");
  status = lonejson_lua_check_record(L, record_index, &other_schema, &checked,
                                     &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "mismatched record/schema pair fails");
  lua_pop(L, 1);

  status = lonejson_lua_clear_record(L, record_index, &error);
  expect_true(status == LONEJSON_STATUS_OK, "record clear succeeds");

  schema.size = 1u;
  schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  push_global(L, "schema");
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "bad schema view size fails");
  lua_pop(L, 1);
  schema.size = sizeof(schema);
  schema.abi_version = 999u;
  push_global(L, "schema");
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "bad schema view ABI fails");
  lua_pop(L, 1);

  lua_pushstring(L, "not a schema");
  schema.size = sizeof(schema);
  schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "wrong userdata type returns status");
  lua_pop(L, 1);

  {
    const char *tiny_chunks[] = {"{\"company\":\"toolong\"}"};
    chunk_reader tiny_reader;
    int tiny_record_index = 0;
    memset(&tiny_reader, 0, sizeof(tiny_reader));
    tiny_reader.chunks = tiny_chunks;
    tiny_reader.count = 1u;
    push_global(L, "tiny_schema");
    status = lonejson_lua_parse_reader_to_record(
        L, -1, chunk_reader_read, &tiny_reader, &tiny_record_index, &error);
    expect_true(status == LONEJSON_STATUS_OVERFLOW,
                "overflow policy fails through interop helper");
    lua_pop(L, 1);
  }

  {
    const char *table_chunks[] = {"{\"company\":\"ACME\",\"email\":\"a@b.c\"}"};
    chunk_reader table_reader;
    memset(&table_reader, 0, sizeof(table_reader));
    table_reader.chunks = table_chunks;
    table_reader.count = 1u;
    push_global(L, "schema");
    status = lonejson_lua_parse_reader_to_table(
        L, -1, chunk_reader_read, &table_reader, &table_index, &error);
    expect_true(status == LONEJSON_STATUS_OK, "parse reader to table succeeds");
    lua_getfield(L, table_index, "company");
    expect_true(strcmp(lua_tostring(L, -1), "ACME") == 0,
                "high-level table helper company matches");
    lua_pop(L, 2);
  }

  lua_pop(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, registry_ref);
}

typedef struct failing_lua_allocator {
  lua_Alloc delegate;
  void *delegate_user;
  size_t remaining;
} failing_lua_allocator;

static void *fail_lua_allocation(void *user, void *ptr, size_t old_size,
                                 size_t new_size) {
  failing_lua_allocator *allocator = (failing_lua_allocator *)user;
  if (new_size != 0u && (ptr == NULL || new_size > old_size)) {
    if (allocator->remaining == 0u) {
      return NULL;
    }
    --allocator->remaining;
  }
  return allocator->delegate(allocator->delegate_user, ptr, old_size, new_size);
}

static void test_encoder_lua_allocation_failure(lua_State *L) {
  failing_lua_allocator allocator;
  size_t budget;
  int case_index;

  if (run_lua(L, "local lj = require('lonejson')\n"
                 "encode_oom_cases = {\n"
                 "  {lj.encode_json, string.rep('x', 64)},\n"
                 "  {lj.encode_value, string.rep('x', 1024)},\n"
                 "  {lj.new({write_pretty = true}).encode_json, "
                 "string.rep('x', 1024)},\n"
                 "}\n"
                 "local wide = {}\n"
                 "for i = 1, 17 do wide[string.rep('k', 64) .. i] = "
                 "string.rep('x', 64) end\n"
                 "table.insert(encode_oom_cases, {lj.encode_json, wide})\n"
                 "table.insert(encode_oom_cases, {lj.new({write_pretty = "
                 "true}).encode_json, wide})\n"
                 "local deep = string.rep('x', 1024)\n"
                 "for i = 1, 64 do deep = {deep} end\n"
                 "table.insert(encode_oom_cases, {lj.encode_json, deep})\n") !=
      0) {
    ++failures;
    return;
  }
  allocator.delegate = lua_getallocf(L, &allocator.delegate_user);
  for (case_index = 1; case_index <= 6; ++case_index) {
    int saw_failure = 0;
    int saw_success = 0;
    for (budget = 0u; budget <= 32u; ++budget) {
      int status;
      int stack_allocation_failed;
      int stack_top = lua_gettop(L);

      lua_getglobal(L, "encode_oom_cases");
      lua_rawgeti(L, -1, case_index);
      lua_rawgeti(L, -1, 1);
      lua_rawgeti(L, -2, 2);
      /* Collect before arming the allocator so result allocation is exercised.
       */
      lua_gc(L, LUA_GCCOLLECT, 0);
      allocator.remaining = budget;
      lua_setallocf(L, fail_lua_allocation, &allocator);
      status = lua_pcall(L, 1, 1, 0);
      lua_setallocf(L, allocator.delegate, allocator.delegate_user);
      stack_allocation_failed =
          status == LUA_ERRRUN && allocator.remaining == 0u &&
          lua_isstring(L, -1) &&
          strstr(lua_tostring(L, -1), "stack overflow") != NULL;
      if (status != LUA_OK && status != LUA_ERRMEM && !stack_allocation_failed)
        fprintf(stderr, "encoder OOM case %d budget %lu: %s\n", case_index,
                (unsigned long)budget, lua_tostring(L, -1));
      expect_true(status == LUA_OK || status == LUA_ERRMEM ||
                      stack_allocation_failed,
                  "encoder returns a result or Lua memory error");
      if (status == LUA_ERRMEM || stack_allocation_failed) {
        saw_failure = 1;
      } else if (status == LUA_OK) {
        saw_success = 1;
        expect_true(lua_type(L, -1) == LUA_TSTRING,
                    "encoder returns JSON after allocation recovery");
      }
      lua_settop(L, stack_top);
    }
    expect_true(saw_failure,
                "Lua allocator fault injection reached encoder errors");
    expect_true(saw_success,
                "encoder succeeds with sufficient Lua allocations");
  }
  lua_pushnil(L);
  lua_setglobal(L, "encode_oom_cases");
}

static void test_decoder_lua_allocation_failure(const char *source_dir) {
  int shape;
  for (shape = 0; shape < 3; ++shape) {
    size_t budget;
    int saw_failure = 0;
    int saw_success = 0;
    for (budget = 0u; budget <= 2048u;
         budget += budget < 16u ? 1u : (budget < 256u ? 16u : 64u)) {
      lua_State *L = luaL_newstate();
      failing_lua_allocator allocator;
      int status;
      expect_true(L != NULL, "fresh decoder OOM state");
      if (L == NULL)
        return;
      require_lonejson(L, source_dir);
      lua_pushinteger(L, shape);
      lua_setglobal(L, "oom_shape");
      if (run_lua(L,
                  "local j = require('lonejson')\n"
                  "decode_oom_function = j.decode_json\n"
                  "decode_oom_text = '0'\n"
                  "for i = 1, 128 do\n"
                  "  local object = oom_shape == 1 or (oom_shape == 2 and i % "
                  "2 == 0)\n"
                  "  decode_oom_text = object and ('{\"x\":' .. "
                  "decode_oom_text .. '}') or ('[' .. decode_oom_text .. ']')\n"
                  "end\n") != 0) {
        ++failures;
        lua_close(L);
        return;
      }
      lua_getglobal(L, "decode_oom_function");
      lua_getglobal(L, "decode_oom_text");
      lua_gc(L, LUA_GCCOLLECT, 0);
      allocator.delegate = lua_getallocf(L, &allocator.delegate_user);
      allocator.remaining = budget;
      lua_setallocf(L, fail_lua_allocation, &allocator);
      status = lua_pcall(L, 1, 1, 0);
      lua_setallocf(L, allocator.delegate, allocator.delegate_user);
      expect_true(
          status == LUA_OK || status == LUA_ERRMEM ||
              (status == LUA_ERRRUN && allocator.remaining == 0u &&
               strstr(lua_tostring(L, -1), "stack overflow") != NULL),
          "decoder reports allocation failure without corrupting state");
      if (status == LUA_OK) {
        ++saw_success;
        expect_true(lua_istable(L, -1), "decoder returns the nested table");
      } else
        ++saw_failure;
      lua_pop(L, 1);
      expect_true(run_lua(L, "local j = require('lonejson'); "
                             "assert(j.decode_json('{\"ok\":true}').ok); "
                             "collectgarbage('collect')") == 0,
                  "decoder recovers after allocation failure");
      lua_close(L);
    }
    expect_true(saw_failure && saw_success,
                "decoder OOM budgets cover failure and success");
  }
}

int main(int argc, char **argv) {
  lua_State *L;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <source-dir>\n", argv[0]);
    return 1;
  }
  test_json_worker_stack(argv[1]);
  if (failures != 0) {
    return 1;
  }
  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "failed to create Lua state\n");
    return 1;
  }
  require_lonejson(L, argv[1]);
  test_interop(L);
  test_encoder_lua_allocation_failure(L);
  test_decoder_lua_allocation_failure(argv[1]);
  lua_close(L);
  if (failures != 0) {
    fprintf(stderr, "%d Lua interop test failure(s)\n", failures);
    return 1;
  }
  printf("lua interop tests passed\n");
  return 0;
}
