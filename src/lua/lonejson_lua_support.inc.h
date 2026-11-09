#if !defined(_WIN32) && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

#if !defined(_WIN32) && !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE 1
#endif

#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../include/lonejson.h"

#include <lauxlib.h>
#include <lua.h>

#define LJLUA_SCHEMA_MT "lonejson.schema"
#define LJLUA_RUNTIME_MT "lonejson.runtime"
#define LJLUA_RECORD_MT "lonejson.record"
#define LJLUA_STREAM_MT "lonejson.stream"
#define LJLUA_ARRAY_STREAM_MT "lonejson.array_stream"
#define LJLUA_SPOOL_MT "lonejson.spool"
#define LJLUA_PATH_MT "lonejson.path"
#define LJLUA_FIXED_STRING_SCRATCH_MT "lonejson.fixed_string_scratch"
#define LJLUA_SCHEMA_MAGIC 0x4c4a5343u
#define LJLUA_RUNTIME_MAGIC 0x4c4a5254u
#define LJLUA_RECORD_MAGIC 0x4c4a5243u
#define LJLUA_STREAM_MAGIC 0x4c4a5354u
#define LJLUA_ARRAY_STREAM_MAGIC 0x4c4a4153u
#define LJLUA_SPOOL_MAGIC 0x4c4a5350u
#define LJLUA_PATH_MAGIC 0x4c4a5048u
#define LJLUA_FIXED_STRING_SCRATCH_MAGIC 0x4c4a4653u

#if !defined(LONEJSON_TEST_FORCE_LUA_LEGACY_USERVALUE) && LUA_VERSION_NUM >= 504
#define LJLUA_HAS_INDEXED_USERVALUE 1
#else
#define LJLUA_HAS_INDEXED_USERVALUE 0
#endif

typedef enum ljlua_field_kind {
  LJLUA_FIELD_STRING = 1,
  LJLUA_FIELD_SPOOLED_TEXT,
  LJLUA_FIELD_SPOOLED_BYTES,
  LJLUA_FIELD_JSON_VALUE,
  LJLUA_FIELD_I64,
  LJLUA_FIELD_U64,
  LJLUA_FIELD_F64,
  LJLUA_FIELD_BOOL,
  LJLUA_FIELD_OBJECT,
  LJLUA_FIELD_STRING_ARRAY,
  LJLUA_FIELD_I64_ARRAY,
  LJLUA_FIELD_U64_ARRAY,
  LJLUA_FIELD_F64_ARRAY,
  LJLUA_FIELD_BOOL_ARRAY,
  LJLUA_FIELD_OBJECT_ARRAY
} ljlua_field_kind;

typedef struct ljlua_schema ljlua_schema;
typedef struct ljlua_compiled_path ljlua_compiled_path;
typedef struct ljlua_json_value_decode_state ljlua_json_value_decode_state;

typedef struct ljlua_runtime_ud {
  unsigned int magic;
  lua_State *L;
  lonejson *runtime;
  lonejson *capture_runtime;
  int clear_destination;
  int reject_duplicate_keys;
  int write_pretty;
  size_t write_max_output_bytes;
  int fixed_string_scratch_ref;
#if defined(LONEJSON_WITH_JWT) && defined(LONEJSON_WITH_OPENSSL)
  lonejson_auth_provider auth_provider;
#endif
#if defined(LONEJSON_WITH_OIDC)
  int http_provider_ref;
  char *http_user_agent;
  lonejson_http_provider http_provider;
#endif
} ljlua_runtime_ud;

typedef struct ljlua_field_meta {
  char *name;
  ljlua_field_kind lua_kind;
  lonejson_field field;
  size_t member_size;
  size_t fixed_array_capacity;
  size_t fixed_array_offset;
  ljlua_schema *subschema;
} ljlua_field_meta;

typedef struct ljlua_path_step {
  ljlua_field_meta *meta;
  size_t index;
} ljlua_path_step;

struct ljlua_compiled_path {
  char *path;
  size_t step_count;
  ljlua_path_step *steps;
  ljlua_compiled_path *next;
};

struct ljlua_schema {
  lonejson *runtime;
  ljlua_runtime_ud *runtime_ud;
  lonejson_map map;
  char *name;
  size_t record_size;
  size_t record_align;
  size_t record_offset;
  size_t field_count;
  int has_json_value;
  int needs_record_init;
  int stack_record_safe;
  lonejson_field *fields;
  ljlua_field_meta *metas;
  ljlua_compiled_path *path_cache;
  unsigned char *scratch_record;
  char *encode_buffer;
  size_t encode_capacity;
  int scratch_in_use;
};

typedef struct ljlua_schema_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int runtime_ref;
} ljlua_schema_ud;

typedef struct ljlua_record_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int schema_ref;
  int cleared;
  unsigned char data[1];
} ljlua_record_ud;

typedef struct ljlua_stream_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int schema_ref;
  int input_ref;
  lonejson_stream *stream;
  void *prepared_record;
  int clear_destination;
  const unsigned char *input;
  size_t input_len;
  size_t input_offset;
} ljlua_stream_ud;

typedef struct ljlua_decode_context {
  lua_State *L;
  ljlua_schema *schema;
  ljlua_json_value_decode_state *json_values;
} ljlua_decode_context;

typedef struct ljlua_array_stream_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int schema_ref;
  int input_ref;
  lonejson_array_stream *stream;
  int clear_destination;
  unsigned char *pending_record;
  ljlua_decode_context pending_ctx;
  const unsigned char *input;
  size_t input_len;
  size_t input_offset;
} ljlua_array_stream_ud;

typedef struct ljlua_spool_ud {
  unsigned int magic;
  lonejson_spooled *spool;
  lonejson_spooled owned;
  int owns_data;
  int owner_ref;
  int kind;
} ljlua_spool_ud;

typedef struct ljlua_fixed_string_scratch_ud {
  unsigned int magic;
  unsigned char *data;
  size_t size;
} ljlua_fixed_string_scratch_ud;

typedef struct ljlua_path_ud {
  unsigned int magic;
  ljlua_schema *schema;
  ljlua_compiled_path *compiled;
  int schema_ref;
} ljlua_path_ud;

static void *ljlua_newuserdata_slots(lua_State *L, size_t size,
                                     int slot_count) {
#if LJLUA_HAS_INDEXED_USERVALUE
  return lua_newuserdatauv(L, size, slot_count);
#else
  void *ud = lua_newuserdata(L, size);
  if (slot_count > 0) {
    lua_newtable(L);
    lua_setuservalue(L, -2);
  }
  return ud;
#endif
}

static void ljlua_set_uservalue_ref(lua_State *L, int ud_index, int slot,
                                    int value_index) {
  ud_index = lua_absindex(L, ud_index);
  value_index = lua_absindex(L, value_index);
#if LJLUA_HAS_INDEXED_USERVALUE
  lua_pushvalue(L, value_index);
  lua_setiuservalue(L, ud_index, slot);
#else
  lua_getuservalue(L, ud_index);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setuservalue(L, ud_index);
  }
  lua_pushvalue(L, value_index);
  lua_rawseti(L, -2, slot);
  lua_pop(L, 1);
#endif
}

static ljlua_fixed_string_scratch_ud *
ljlua_check_fixed_string_scratch(lua_State *L, int index);

static ljlua_runtime_ud *ljlua_check_runtime(lua_State *L, int index) {
  ljlua_runtime_ud *ud =
      (ljlua_runtime_ud *)luaL_checkudata(L, index, LJLUA_RUNTIME_MT);
  luaL_argcheck(
      L, ud != NULL && ud->magic == LJLUA_RUNTIME_MAGIC && ud->runtime != NULL,
      index, "invalid lonejson runtime");
  return ud;
}

static lonejson_overflow_policy
ljlua_parse_runtime_overflow_policy(lua_State *L, int index, const char *key,
                                    lonejson_overflow_policy fallback) {
  lonejson_overflow_policy value = fallback;

  lua_getfield(L, index, key);
  if (!lua_isnil(L, -1)) {
    const char *name = luaL_checkstring(L, -1);

    if (strcmp(name, "fail") == 0) {
      value = LONEJSON_OVERFLOW_FAIL;
    } else if (strcmp(name, "truncate") == 0) {
      value = LONEJSON_OVERFLOW_TRUNCATE;
    } else if (strcmp(name, "truncate_silent") == 0) {
      value = LONEJSON_OVERFLOW_TRUNCATE_SILENT;
    } else {
      luaL_error(L, "unsupported overflow policy '%s'", name);
    }
  }
  lua_pop(L, 1);
  return value;
}

static void ljlua_parse_runtime_spool_policy(lua_State *L, int index,
                                             const char *key,
                                             lonejson_spool_policy *policy) {
  int policy_index;
  lua_Integer value;

  lua_getfield(L, index, key);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return;
  }
  luaL_checktype(L, -1, LUA_TTABLE);
  policy_index = lua_absindex(L, -1);

  lua_getfield(L, policy_index, "memory_limit");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "spool memory_limit must be non-negative");
    policy->memory_limit = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, policy_index, "max_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index, "spool max_bytes must be non-negative");
    policy->max_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, policy_index, "temp_dir");
  if (!lua_isnil(L, -1)) {
    policy->temp_dir = luaL_checkstring(L, -1);
  }
  lua_pop(L, 1);
  lua_pop(L, 1);
}

static void ljlua_parse_runtime_config(lua_State *L, int index,
                                       lonejson_config *config) {
  lua_Integer value;

  *config = lonejson_default_config();
  if (index == 0 || lua_isnoneornil(L, index)) {
    return;
  }
  luaL_checktype(L, index, LUA_TTABLE);

  lua_getfield(L, index, "max_alloc_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index, "max_alloc_bytes must be non-negative");
    config->max_alloc_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "max_dynamic_string_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "max_dynamic_string_bytes must be non-negative");
    config->max_dynamic_string_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "max_depth");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value > 0, index, "max_depth must be positive");
    config->max_depth = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "json_value_max_total_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "json_value_max_total_bytes must be non-negative");
    config->json_value_max_total_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "json_value_max_string_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "json_value_max_string_bytes must be non-negative");
    config->json_value_max_string_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "json_value_max_key_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "json_value_max_key_bytes must be non-negative");
    config->json_value_max_key_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "json_value_max_number_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "json_value_max_number_bytes must be non-negative");
    config->json_value_max_number_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "json_value_max_depth");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value > 0, index, "json_value_max_depth must be positive");
    config->json_value_max_depth = (size_t)value;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "clear_destination_by_default");
  if (!lua_isnil(L, -1)) {
    config->clear_destination_by_default = lua_toboolean(L, -1) ? 1 : 0;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "reject_duplicate_keys_by_default");
  if (!lua_isnil(L, -1)) {
    config->reject_duplicate_keys_by_default = lua_toboolean(L, -1) ? 1 : 0;
  }
  lua_pop(L, 1);

  config->write_overflow_policy = ljlua_parse_runtime_overflow_policy(
      L, index, "write_overflow_policy", config->write_overflow_policy);

  lua_getfield(L, index, "write_pretty");
  if (!lua_isnil(L, -1)) {
    config->write_pretty = lua_toboolean(L, -1) ? 1 : 0;
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "write_max_output_bytes");
  if (!lua_isnil(L, -1)) {
    value = luaL_checkinteger(L, -1);
    luaL_argcheck(L, value >= 0, index,
                  "write_max_output_bytes must be non-negative");
    config->write_max_output_bytes = (size_t)value;
  }
  lua_pop(L, 1);

  ljlua_parse_runtime_spool_policy(L, index, "spool_default",
                                   &config->spool_default);
  ljlua_parse_runtime_spool_policy(L, index, "spool_blob", &config->spool_blob);
  ljlua_parse_runtime_spool_policy(L, index, "spool_large_text",
                                   &config->spool_large_text);

  lua_getfield(L, index, "fixed_string_scratch");
  if (!lua_isnil(L, -1)) {
    ljlua_fixed_string_scratch_ud *scratch =
        ljlua_check_fixed_string_scratch(L, -1);
    config->fixed_string_scratch = scratch->data;
    config->fixed_string_scratch_size = scratch->size;
  }
  lua_pop(L, 1);
}

static int ljlua_runtime_config_fixed_string_scratch_ref(lua_State *L,
                                                         int index) {
  if (index == 0 || lua_isnoneornil(L, index)) {
    return LUA_NOREF;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  lua_getfield(L, index, "fixed_string_scratch");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return LUA_NOREF;
  }
  (void)ljlua_check_fixed_string_scratch(L, -1);
  return luaL_ref(L, LUA_REGISTRYINDEX);
}

static ljlua_fixed_string_scratch_ud *
ljlua_check_fixed_string_scratch(lua_State *L, int index) {
  ljlua_fixed_string_scratch_ud *ud =
      (ljlua_fixed_string_scratch_ud *)luaL_checkudata(
          L, index, LJLUA_FIXED_STRING_SCRATCH_MT);
  luaL_argcheck(L,
                ud != NULL && ud->magic == LJLUA_FIXED_STRING_SCRATCH_MAGIC &&
                    ud->data != NULL && ud->size != 0u,
                index, "invalid fixed string scratch");
  return ud;
}

typedef struct ljlua_json_buf {
  char *data;
  size_t len;
  size_t cap;
  size_t max_cap;
} ljlua_json_buf;

#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
typedef struct ljlua_test_encode_stats {
  size_t json_buf_peak_capacity;
  size_t json_key_bytes_live;
  size_t json_key_peak_bytes_live;
} ljlua_test_encode_stats;

static ljlua_test_encode_stats g_ljlua_test_encode_stats;
static size_t g_ljlua_test_json_buf_allocations_before_failure;
static int g_ljlua_test_json_buf_failure_enabled;

static void ljlua_test_reset_encode_stats(void) {
  memset(&g_ljlua_test_encode_stats, 0, sizeof(g_ljlua_test_encode_stats));
}

static void ljlua_test_note_json_buf_capacity(size_t cap) {
  if (cap > g_ljlua_test_encode_stats.json_buf_peak_capacity) {
    g_ljlua_test_encode_stats.json_buf_peak_capacity = cap;
  }
}

static void ljlua_test_note_json_key_alloc(size_t size) {
  g_ljlua_test_encode_stats.json_key_bytes_live += size;
  if (g_ljlua_test_encode_stats.json_key_bytes_live >
      g_ljlua_test_encode_stats.json_key_peak_bytes_live) {
    g_ljlua_test_encode_stats.json_key_peak_bytes_live =
        g_ljlua_test_encode_stats.json_key_bytes_live;
  }
}

static void ljlua_test_note_json_key_free(size_t size) {
  if (size > g_ljlua_test_encode_stats.json_key_bytes_live) {
    g_ljlua_test_encode_stats.json_key_bytes_live = 0u;
  } else {
    g_ljlua_test_encode_stats.json_key_bytes_live -= size;
  }
}
#endif

typedef struct ljlua_json_key {
  char *text;
  size_t len;
  int heap_allocated;
} ljlua_json_key;

typedef struct ljlua_json_parser {
  const unsigned char *data;
  size_t len;
  size_t off;
} ljlua_json_parser;

static lonejson_read_result
ljlua_stream_mem_read(void *user, unsigned char *buffer, size_t capacity) {
  ljlua_stream_ud *ud = (ljlua_stream_ud *)user;
  lonejson_read_result rr = lonejson_default_read_result();
  size_t remaining;

  if (ud->input_offset >= ud->input_len) {
    rr.eof = 1;
    return rr;
  }
  remaining = ud->input_len - ud->input_offset;
  if (remaining > capacity) {
    remaining = capacity;
  }
  if (remaining != 0u) {
    memcpy(buffer, ud->input + ud->input_offset, remaining);
    ud->input_offset += remaining;
  }
  rr.bytes_read = remaining;
  rr.eof = ud->input_offset >= ud->input_len;
  return rr;
}

static lonejson_status ljlua_set_error(lonejson_error *error,
                                       lonejson_status status, const char *fmt,
                                       ...) {
  if (error != NULL) {
    va_list ap;

    lonejson_error_init(error);
    error->code = status;
    if (fmt != NULL) {
      va_start(ap, fmt);
      vsnprintf(error->message, sizeof(error->message), fmt, ap);
      va_end(ap);
    }
  }
  return status;
}

typedef struct ljlua_escape_chunk {
  lonejson_sink_fn sink;
  void *user;
  lonejson_error *error;
  unsigned char data[256];
  size_t len;
} ljlua_escape_chunk;

static lonejson_status ljlua_escape_chunk_flush(ljlua_escape_chunk *chunk) {
  lonejson_status status;
  if (chunk->len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  status = chunk->sink(chunk->user, chunk->data, chunk->len, chunk->error);
  chunk->len = 0u;
  return status;
}

static lonejson_status ljlua_escape_chunk_append(ljlua_escape_chunk *chunk,
                                                 const void *data, size_t len) {
  lonejson_status status;
  if (len > sizeof(chunk->data) - chunk->len) {
    status = ljlua_escape_chunk_flush(chunk);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  if (len >= sizeof(chunk->data)) {
    return chunk->sink(chunk->user, data, len, chunk->error);
  }
  memcpy(chunk->data + chunk->len, data, len);
  chunk->len += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_emit_escaped_fragment_from(
    lonejson_sink_fn sink, void *user, lonejson_error *error,
    const unsigned char *data, size_t len, size_t first_escape) {
  static const char hex[] = "0123456789abcdef";
  size_t start = 0u;
  size_t i;
  ljlua_escape_chunk chunk;

  chunk.sink = sink;
  chunk.user = user;
  chunk.error = error;
  chunk.len = 0u;
  for (i = first_escape; i < len; ++i) {
    unsigned char ch = data[i];
    const char *replacement = NULL;
    char unicode_escape[6];
    size_t replacement_len = 2u;
    lonejson_status status;

    switch (ch) {
    case '"':
      replacement = "\\\"";
      break;
    case '\\':
      replacement = "\\\\";
      break;
    case '\b':
      replacement = "\\b";
      break;
    case '\f':
      replacement = "\\f";
      break;
    case '\n':
      replacement = "\\n";
      break;
    case '\r':
      replacement = "\\r";
      break;
    case '\t':
      replacement = "\\t";
      break;
    default:
      if (ch < 0x20u) {
        unicode_escape[0] = '\\';
        unicode_escape[1] = 'u';
        unicode_escape[2] = '0';
        unicode_escape[3] = '0';
        unicode_escape[4] = hex[(ch >> 4u) & 0x0fu];
        unicode_escape[5] = hex[ch & 0x0fu];
        replacement = unicode_escape;
        replacement_len = sizeof(unicode_escape);
      }
      break;
    }
    if (replacement == NULL) {
      continue;
    }
    if (i > start) {
      status = ljlua_escape_chunk_append(&chunk, data + start, i - start);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    status = ljlua_escape_chunk_append(&chunk, replacement, replacement_len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    start = i + 1u;
  }
  if (len > start) {
    lonejson_status status;
    if (chunk.len == 0u) {
      return sink(user, data + start, len - start, error);
    }
    status = ljlua_escape_chunk_append(&chunk, data + start, len - start);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return ljlua_escape_chunk_flush(&chunk);
}

static lonejson_status ljlua_emit_escaped_fragment(lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error,
                                                   const unsigned char *data,
                                                   size_t len) {
  return ljlua_emit_escaped_fragment_from(sink, user, error, data, len, 0u);
}

static int ljlua_is_finite_f64(double value) {
  return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}

static int ljlua_is_json_space(int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static int ljlua_is_digit(int ch) { return ch >= '0' && ch <= '9'; }

static int ljlua_hex_value(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

static int ljlua_decode_unicode_quad(const unsigned char *data,
                                     lonejson_uint32 *out) {
  int a = ljlua_hex_value(data[0]);
  int b = ljlua_hex_value(data[1]);
  int c = ljlua_hex_value(data[2]);
  int d = ljlua_hex_value(data[3]);

  if (a < 0 || b < 0 || c < 0 || d < 0) {
    return 0;
  }
  *out = ((lonejson_uint32)a << 12u) | ((lonejson_uint32)b << 8u) |
         ((lonejson_uint32)c << 4u) | (lonejson_uint32)d;
  return 1;
}

static int ljlua_parse_u64_token(const char *text, lonejson_uint64 *out) {
  lonejson_uint64 value = 0u;
  const unsigned char *p = (const unsigned char *)text;

  if (p == NULL || *p == '\0') {
    return 0;
  }
  while (*p != '\0') {
    unsigned digit;
    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (unsigned)(*p - '0');
    if (value > (((lonejson_uint64)-1) - (lonejson_uint64)digit) /
                    (lonejson_uint64)10u) {
      return 0;
    }
    value = value * (lonejson_uint64)10u + (lonejson_uint64)digit;
    ++p;
  }
  *out = value;
  return 1;
}

static int ljlua_parse_i64_token(const char *text, lonejson_int64 *out) {
  const char *digits = text;
  lonejson_uint64 value;
  lonejson_uint64 limit = (lonejson_uint64)9223372036854775807ull;
  int negative = 0;

  if (text == NULL || *text == '\0') {
    return 0;
  }
  if (*digits == '-') {
    negative = 1;
    ++digits;
    limit = limit + 1u;
  }
  if (!ljlua_parse_u64_token(digits, &value) || value > limit) {
    return 0;
  }
  if (negative) {
    if (value == limit) {
      *out = (lonejson_int64)(-9223372036854775807ll - 1ll);
    } else {
      *out = -(lonejson_int64)value;
    }
  } else {
    *out = (lonejson_int64)value;
  }
  return 1;
}

static lonejson_read_result ljlua_file_reader(void *user, unsigned char *buffer,
                                              size_t capacity) {
  FILE *fp = (FILE *)user;
  lonejson_read_result result = lonejson_default_read_result();

  if (capacity == 0u) {
    return result;
  }
  result.bytes_read = fread(buffer, 1u, capacity, fp);
  if (result.bytes_read < capacity) {
    if (ferror(fp)) {
      result.error_code = errno != 0 ? errno : EIO;
    } else if (feof(fp)) {
      result.eof = 1;
    }
  }
  return result;
}

static lonejson_status ljlua_file_sink(void *user, const void *data, size_t len,
                                       lonejson_error *error) {
  FILE *fp = (FILE *)user;

  if (len != 0u && fwrite(data, 1u, len, fp) != len) {
    if (error != NULL) {
      error->system_errno = errno != 0 ? errno : EIO;
    }
    return ljlua_set_error(error, LONEJSON_STATUS_IO_ERROR,
                           "failed to write output file");
  }
  return LONEJSON_STATUS_OK;
}

static char ljlua_json_null_sentinel;

typedef enum ljlua_json_builder_token_mode {
  LJLUA_JSON_TOKEN_NONE = 0,
  LJLUA_JSON_TOKEN_STRING,
  LJLUA_JSON_TOKEN_NUMBER,
  LJLUA_JSON_TOKEN_TRUE,
  LJLUA_JSON_TOKEN_FALSE,
  LJLUA_JSON_TOKEN_NULL
} ljlua_json_builder_token_mode;

typedef enum ljlua_json_builder_frame_kind {
  LJLUA_JSON_FRAME_OBJECT = 1,
  LJLUA_JSON_FRAME_ARRAY = 2
} ljlua_json_builder_frame_kind;

typedef enum ljlua_json_builder_frame_state {
  LJLUA_JSON_OBJECT_KEY_OR_END = 1,
  LJLUA_JSON_OBJECT_COLON,
  LJLUA_JSON_OBJECT_VALUE,
  LJLUA_JSON_OBJECT_COMMA_OR_END,
  LJLUA_JSON_ARRAY_VALUE_OR_END,
  LJLUA_JSON_ARRAY_COMMA_OR_END
} ljlua_json_builder_frame_state;

typedef struct ljlua_json_builder_frame {
  ljlua_json_builder_frame_kind kind;
  ljlua_json_builder_frame_state state;
  int table_ref;
  size_t array_index;
  char *pending_key;
} ljlua_json_builder_frame;

typedef struct ljlua_json_builder {
  lua_State *L;
  ljlua_json_buf token;
  ljlua_json_builder_frame *frames;
  size_t frame_count;
  size_t frame_cap;
  ljlua_json_builder_token_mode token_mode;
  int token_is_key;
  int token_escape;
  unsigned unicode_digits_needed;
  unsigned unicode_accum;
  unsigned unicode_pending_high;
  int root_ref;
  int finished;
} ljlua_json_builder;

struct ljlua_json_value_decode_state {
  ljlua_json_builder builder;
  lonejson_value_visitor visitor;
  int active;
  int complete;
};

static int ljlua_record_to_table_method(lua_State *L);
static int ljlua_record_get(lua_State *L);
static int ljlua_record_count(lua_State *L);
static int ljlua_record_clear(lua_State *L);
static int ljlua_path_get(lua_State *L);
static int ljlua_path_count(lua_State *L);
static int ljlua_array_stream_close(lua_State *L);
static int ljlua_encode_json(lua_State *L);
static int ljlua_encode_json_to_sink(lua_State *L);
static int ljlua_decode_json(lua_State *L);
static int ljlua_getter_call(lua_State *L);
static int ljlua_counter_call(lua_State *L);
static void ljlua_cleanup_record_storage(ljlua_schema *schema, void *record);
static int
ljlua_record_to_table_with_overrides(lua_State *L, ljlua_schema *schema,
                                     void *record,
                                     const ljlua_decode_context *ctx);

static int ljlua_push_error(lua_State *L, const lonejson_error *error) {
  lua_newtable(L);
  lua_pushinteger(L, (lua_Integer)(error ? error->code : 0));
  lua_setfield(L, -2, "code");
  lua_pushstring(L, error ? lonejson_status_string(error->code) : "unknown");
  lua_setfield(L, -2, "status");
  lua_pushinteger(L, (lua_Integer)(error ? error->line : 0));
  lua_setfield(L, -2, "line");
  lua_pushinteger(L, (lua_Integer)(error ? error->column : 0));
  lua_setfield(L, -2, "column");
  lua_pushinteger(L, (lua_Integer)(error ? error->offset : 0));
  lua_setfield(L, -2, "offset");
  lua_pushinteger(L, (lua_Integer)(error ? error->system_errno : 0));
  lua_setfield(L, -2, "system_errno");
  lua_pushboolean(L, error ? error->truncated : 0);
  lua_setfield(L, -2, "truncated");
  lua_pushstring(L, error ? error->message : "unknown error");
  lua_setfield(L, -2, "message");
  return 1;
}

static void *ljlua_xmalloc(size_t size) {
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL) {
    abort();
  }
  return ptr;
}

static char *ljlua_strdup(const char *src) {
  size_t len;
  char *dst;

  len = strlen(src);
  dst = (char *)ljlua_xmalloc(len + 1u);
  memcpy(dst, src, len + 1u);
  return dst;
}

static void ljlua_u64_to_decimal(char *buf, size_t capacity,
                                 lonejson_uint64 value) {
  size_t idx;

  idx = capacity;
  buf[--idx] = '\0';
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    buf[--idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  memmove(buf, buf + idx, capacity - idx);
}

static int ljlua_push_u64(lua_State *L, lonejson_uint64 value) {
#if defined(LUA_MAXINTEGER) &&                                                 \
    !defined(LONEJSON_TEST_LUA_SIMULATE_NO_MAXINTEGER)
  if (value <= (lonejson_uint64)LUA_MAXINTEGER) {
    lua_pushinteger(L, (lua_Integer)value);
    return 1;
  }
#else
#if DBL_MANT_DIG >= 64
  if (1) {
#else
  if (value <= (((lonejson_uint64)1u) << DBL_MANT_DIG)) {
#endif
    lua_pushnumber(L, (lua_Number)value);
    return 1;
  }
#endif
  {
    char buf[32];
    ljlua_u64_to_decimal(buf, sizeof(buf), value);
    lua_pushstring(L, buf);
  }
  return 1;
}

static int ljlua_json_buf_reserve(ljlua_json_buf *buf, size_t extra) {
  size_t needed;
  size_t cap;
  char *data;

  if (buf->len == SIZE_MAX || extra > SIZE_MAX - buf->len - 1u) {
    return 0;
  }
  needed = buf->len + extra + 1u;
  if (buf->max_cap != 0u && needed > buf->max_cap) {
    return 0;
  }
  if (needed <= buf->cap) {
    return 1;
  }
  cap = buf->cap == 0u ? 128u : buf->cap;
  if (buf->max_cap != 0u && cap > buf->max_cap) {
    cap = buf->max_cap;
  }
  while (cap < needed) {
    size_t next = cap > SIZE_MAX / 2u ? needed : cap * 2u;
    if (buf->max_cap != 0u && next > buf->max_cap) {
      next = buf->max_cap;
    }
    cap = next;
  }
#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
  if (g_ljlua_test_json_buf_failure_enabled) {
    if (g_ljlua_test_json_buf_allocations_before_failure == 0u) {
      g_ljlua_test_json_buf_failure_enabled = 0;
      return 0;
    }
    --g_ljlua_test_json_buf_allocations_before_failure;
  }
#endif
  data = (char *)realloc(buf->data, cap);
  if (data == NULL) {
    return 0;
  }
  buf->data = data;
  buf->cap = cap;
#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
  ljlua_test_note_json_buf_capacity(cap);
#endif
  return 1;
}

static int ljlua_json_buf_append(ljlua_json_buf *buf, const void *data,
                                 size_t len) {
  if (!ljlua_json_buf_reserve(buf, len)) {
    return 0;
  }
  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
  return 1;
}

static lonejson_status ljlua_json_buf_sink(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  if (!ljlua_json_buf_append((ljlua_json_buf *)user, data, len)) {
    return ljlua_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                           "failed to grow JSON buffer");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_buf_sink_limited(void *user, const void *data,
                                                   size_t len,
                                                   lonejson_error *error) {
  ljlua_json_buf *buf = (ljlua_json_buf *)user;

  if (buf->max_cap != 0u &&
      (buf->len >= buf->max_cap || len > (buf->max_cap - 1u) - buf->len)) {
    return ljlua_set_error(error, LONEJSON_STATUS_OVERFLOW,
                           "serializer-owned output exceeds max_output_bytes");
  }
  return ljlua_json_buf_sink(user, data, len, error);
}

typedef struct ljlua_scalar_output {
  lua_State *L;
  ljlua_json_buf *buf;
} ljlua_scalar_output;

static int ljlua_push_json_buf_pcall(lua_State *L) {
  ljlua_json_buf *buf = (ljlua_json_buf *)lua_touserdata(L, 1);
  lua_pushlstring(L, buf->data, buf->len);
  return 1;
}

static lonejson_status ljlua_scalar_output_sink(void *user, const void *data,
                                                size_t len,
                                                lonejson_error *error) {
  ljlua_scalar_output *output = (ljlua_scalar_output *)user;
  lonejson_status status =
      ljlua_json_buf_sink_limited(output->buf, data, len, error);

  if (status != LONEJSON_STATUS_OK) {
    free(output->buf->data);
    output->buf->data = NULL;
    luaL_error(output->L, "failed to write JSON output: %s", error->message);
  }
  return status;
}

typedef struct ljlua_json_out {
  lonejson_sink_fn sink;
  void *user;
} ljlua_json_out;

static int ljlua_json_out_write(lua_State *L, ljlua_json_out *out,
                                const void *data, size_t len) {
  lonejson_error error;
  lonejson_status status;

  memset(&error, 0, sizeof(error));
  status = out->sink(out->user, data, len, &error);
  if (status != LONEJSON_STATUS_OK) {
    const char *message = error.message[0] != '\0'
                              ? error.message
                              : lonejson_status_string(status);
    return luaL_error(L, "failed to write JSON output: %s", message);
  }
  return 1;
}

static int ljlua_json_out_write_cstr(lua_State *L, ljlua_json_out *out,
                                     const char *text) {
  return ljlua_json_out_write(L, out, text, strlen(text));
}

static void ljlua_json_mark_shape(lua_State *L, int index, const char *kind) {
  index = lua_absindex(L, index);
  lua_createtable(L, 0, 1);
  lua_pushstring(L, kind);
  lua_setfield(L, -2, "__lonejson_json_kind");
  lua_setmetatable(L, index);
}

static const char *ljlua_json_shape(lua_State *L, int index) {
  const char *shape = NULL;

  index = lua_absindex(L, index);
  if (!lua_getmetatable(L, index)) {
    return NULL;
  }
  lua_getfield(L, -1, "__lonejson_json_kind");
  if (lua_isstring(L, -1)) {
    shape = lua_tostring(L, -1);
  }
  lua_pop(L, 2);
  return shape;
}

static void ljlua_push_json_null(lua_State *L) {
  lua_pushlightuserdata(L, (void *)&ljlua_json_null_sentinel);
}

static int ljlua_is_json_null(lua_State *L, int index) {
  index = lua_absindex(L, index);
  return lua_type(L, index) == LUA_TLIGHTUSERDATA &&
         lua_touserdata(L, index) == (void *)&ljlua_json_null_sentinel;
}

static int ljlua_json_key_compare(const void *lhs, const void *rhs) {
  const ljlua_json_key *a = (const ljlua_json_key *)lhs;
  const ljlua_json_key *b = (const ljlua_json_key *)rhs;
  size_t shared = a->len < b->len ? a->len : b->len;
  int cmp = memcmp(a->text, b->text, shared);

  if (cmp != 0) {
    return cmp;
  }
  if (a->len < b->len) {
    return -1;
  }
  if (a->len > b->len) {
    return 1;
  }
  return 0;
}

static int ljlua_json_buffer_add_utf8(luaL_Buffer *buffer,
                                      lonejson_uint32 codepoint) {
  char out[4];
  size_t len;

  if (codepoint <= 0x7Fu) {
    out[0] = (char)codepoint;
    len = 1u;
  } else if (codepoint <= 0x7FFu) {
    out[0] = (char)(0xC0u | ((codepoint >> 6u) & 0x1Fu));
    out[1] = (char)(0x80u | (codepoint & 0x3Fu));
    len = 2u;
  } else if (codepoint <= 0xFFFFu) {
    out[0] = (char)(0xE0u | ((codepoint >> 12u) & 0x0Fu));
    out[1] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
    out[2] = (char)(0x80u | (codepoint & 0x3Fu));
    len = 3u;
  } else if (codepoint <= 0x10FFFFu) {
    out[0] = (char)(0xF0u | ((codepoint >> 18u) & 0x07u));
    out[1] = (char)(0x80u | ((codepoint >> 12u) & 0x3Fu));
    out[2] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
    out[3] = (char)(0x80u | (codepoint & 0x3Fu));
    len = 4u;
  } else {
    return 0;
  }
  luaL_addlstring(buffer, out, len);
  return 1;
}

static lonejson_uint64 ljlua_check_u64(lua_State *L, int index) {
  int type;
  lonejson_uint64 value = 0u;

  type = lua_type(L, index);
  if (type == LUA_TNUMBER) {
    lua_Integer signed_value;
#if defined(LUA_MAXINTEGER)
    if (!lua_isinteger(L, index)) {
      luaL_error(L, "expected unsigned 64-bit integer");
    }
#endif
    signed_value = luaL_checkinteger(L, index);
    if (signed_value < 0) {
      luaL_error(L, "expected unsigned 64-bit integer");
    }
    return (lonejson_uint64)signed_value;
  }
  if (type == LUA_TSTRING) {
    const char *text = lua_tostring(L, index);
    if (!ljlua_parse_u64_token(text, &value)) {
      luaL_error(L, "expected unsigned 64-bit integer");
    }
    return value;
  }
  luaL_error(L, "expected unsigned 64-bit integer");
  return 0u;
}

static size_t ljlua_align(size_t value, size_t align) {
  return (value + align - 1u) & ~(align - 1u);
}

static lonejson_overflow_policy
ljlua_parse_overflow_policy(lua_State *L, int index, const char *field_name) {
  const char *name;

  lua_getfield(L, index, field_name);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return LONEJSON_OVERFLOW_FAIL;
  }
  name = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  if (strcmp(name, "fail") == 0) {
    return LONEJSON_OVERFLOW_FAIL;
  }
  if (strcmp(name, "truncate") == 0) {
    return LONEJSON_OVERFLOW_TRUNCATE;
  }
  if (strcmp(name, "truncate_silent") == 0) {
    return LONEJSON_OVERFLOW_TRUNCATE_SILENT;
  }
  luaL_error(L, "unsupported overflow policy '%s'", name);
  return LONEJSON_OVERFLOW_FAIL;
}

static int ljlua_json_parse_value(lua_State *L, ljlua_json_parser *parser,
                                  size_t depth);

static void ljlua_json_skip_space(ljlua_json_parser *parser) {
  while (parser->off < parser->len &&
         ljlua_is_json_space(parser->data[parser->off])) {
    parser->off++;
  }
}

static int ljlua_json_parse_string(lua_State *L, ljlua_json_parser *parser) {
  luaL_Buffer buffer;

  luaL_buffinit(L, &buffer);
  while (parser->off < parser->len) {
    unsigned char ch = parser->data[parser->off++];
    if (ch == '"') {
      luaL_pushresult(&buffer);
      return 1;
    }
    if (ch == '\\') {
      if (parser->off >= parser->len) {
        return luaL_error(L, "invalid JSON string escape");
      }
      ch = parser->data[parser->off++];
      switch (ch) {
      case '"':
      case '\\':
      case '/':
        luaL_addchar(&buffer, (char)ch);
        break;
      case 'b':
        luaL_addchar(&buffer, '\b');
        break;
      case 'f':
        luaL_addchar(&buffer, '\f');
        break;
      case 'n':
        luaL_addchar(&buffer, '\n');
        break;
      case 'r':
        luaL_addchar(&buffer, '\r');
        break;
      case 't':
        luaL_addchar(&buffer, '\t');
        break;
      case 'u': {
        lonejson_uint32 codepoint;

        if (parser->off + 4u > parser->len ||
            !ljlua_decode_unicode_quad(parser->data + parser->off,
                                       &codepoint)) {
          return luaL_error(L, "invalid JSON unicode escape");
        }
        parser->off += 4u;
        if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
          lonejson_uint32 low;

          if (parser->off + 6u > parser->len ||
              parser->data[parser->off] != '\\' ||
              parser->data[parser->off + 1u] != 'u' ||
              !ljlua_decode_unicode_quad(parser->data + parser->off + 2u,
                                         &low) ||
              low < 0xDC00u || low > 0xDFFFu) {
            return luaL_error(L, "invalid JSON unicode surrogate pair");
          }
          parser->off += 6u;
          codepoint =
              0x10000u + (((codepoint - 0xD800u) << 10u) | (low - 0xDC00u));
        } else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
          return luaL_error(L, "invalid JSON unicode surrogate pair");
        }
        if (!ljlua_json_buffer_add_utf8(&buffer, codepoint)) {
          return luaL_error(L, "failed to encode JSON unicode codepoint");
        }
        break;
      }
      default:
        return luaL_error(L, "invalid JSON string escape");
      }
      continue;
    }
    if (ch < 0x20u) {
      return luaL_error(L, "control character in JSON string");
    }
    luaL_addchar(&buffer, (char)ch);
  }
  return luaL_error(L, "unterminated JSON string");
}

static int ljlua_json_parse_number(lua_State *L, ljlua_json_parser *parser) {
  size_t start = parser->off;
  size_t len;
  char stack_buf[128];
  char *text;
  int integer_like = 1;
  size_t i;
  char *end = NULL;
  double number;

  while (parser->off < parser->len) {
    unsigned char ch = parser->data[parser->off];
    if (!(ljlua_is_digit(ch) || ch == '-' || ch == '+' || ch == '.' ||
          ch == 'e' || ch == 'E')) {
      break;
    }
    parser->off++;
  }
  len = parser->off - start;
  text = (len + 1u <= sizeof(stack_buf)) ? stack_buf
                                         : (char *)ljlua_xmalloc(len + 1u);
  memcpy(text, parser->data + start, len);
  text[len] = '\0';
  for (i = 0u; i < len; ++i) {
    if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') {
      integer_like = 0;
      break;
    }
  }
  if (integer_like) {
    lonejson_int64 i64;

    if (ljlua_parse_i64_token(text, &i64)) {
      lua_pushinteger(L, (lua_Integer)i64);
      if (text != stack_buf) {
        free(text);
      }
      return 1;
    }
  }
  errno = 0;
  number = strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' ||
      !ljlua_is_finite_f64(number)) {
    if (text != stack_buf) {
      free(text);
    }
    return luaL_error(L, "invalid JSON number");
  }
  lua_pushnumber(L, (lua_Number)number);
  if (text != stack_buf) {
    free(text);
  }
  return 1;
}

static int ljlua_json_parse_array(lua_State *L, ljlua_json_parser *parser,
                                  size_t depth) {
  size_t index = 1u;

  lua_createtable(L, 0, 0);
  ljlua_json_skip_space(parser);
  if (parser->off < parser->len && parser->data[parser->off] == ']') {
    parser->off++;
    ljlua_json_mark_shape(L, -1, "array");
    return 1;
  }
  for (;;) {
    ljlua_json_parse_value(L, parser, depth + 1u);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      ljlua_push_json_null(L);
    }
    lua_rawseti(L, -2, (lua_Integer)index++);
    ljlua_json_skip_space(parser);
    if (parser->off >= parser->len) {
      return luaL_error(L, "unterminated JSON array");
    }
    if (parser->data[parser->off] == ']') {
      parser->off++;
      ljlua_json_mark_shape(L, -1, "array");
      return 1;
    }
    if (parser->data[parser->off] != ',') {
      return luaL_error(L, "expected ',' in JSON array");
    }
    parser->off++;
    ljlua_json_skip_space(parser);
  }
}

static int ljlua_json_parse_object(lua_State *L, ljlua_json_parser *parser,
                                   size_t depth) {
  lua_createtable(L, 0, 0);
  ljlua_json_skip_space(parser);
  if (parser->off < parser->len && parser->data[parser->off] == '}') {
    parser->off++;
    ljlua_json_mark_shape(L, -1, "object");
    return 1;
  }
  for (;;) {
    if (parser->off >= parser->len || parser->data[parser->off] != '"') {
      return luaL_error(L, "expected JSON object key");
    }
    parser->off++;
    ljlua_json_parse_string(L, parser);
    ljlua_json_skip_space(parser);
    if (parser->off >= parser->len || parser->data[parser->off] != ':') {
      return luaL_error(L, "expected ':' after JSON object key");
    }
    parser->off++;
    ljlua_json_skip_space(parser);
    ljlua_json_parse_value(L, parser, depth + 1u);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      ljlua_push_json_null(L);
    }
    lua_settable(L, -3);
    ljlua_json_skip_space(parser);
    if (parser->off >= parser->len) {
      return luaL_error(L, "unterminated JSON object");
    }
    if (parser->data[parser->off] == '}') {
      parser->off++;
      ljlua_json_mark_shape(L, -1, "object");
      return 1;
    }
    if (parser->data[parser->off] != ',') {
      return luaL_error(L, "expected ',' in JSON object");
    }
    parser->off++;
    ljlua_json_skip_space(parser);
  }
}

static int ljlua_json_parse_value(lua_State *L, ljlua_json_parser *parser,
                                  size_t depth) {
  if (depth > 128u) {
    return luaL_error(L, "JSON value nesting exceeds Lua binding limit");
  }
  luaL_checkstack(L, 4, "JSON decoding");
  ljlua_json_skip_space(parser);
  if (parser->off >= parser->len) {
    return luaL_error(L, "expected JSON value");
  }
  switch (parser->data[parser->off++]) {
  case '"':
    return ljlua_json_parse_string(L, parser);
  case '{':
    if (depth == 128u)
      return luaL_error(L, "JSON value nesting exceeds Lua binding limit");
    return ljlua_json_parse_object(L, parser, depth);
  case '[':
    if (depth == 128u)
      return luaL_error(L, "JSON value nesting exceeds Lua binding limit");
    return ljlua_json_parse_array(L, parser, depth);
  case 't':
    if (parser->off + 2u > parser->len ||
        memcmp(parser->data + parser->off, "rue", 3u) != 0) {
      return luaL_error(L, "invalid JSON literal");
    }
    parser->off += 3u;
    lua_pushboolean(L, 1);
    return 1;
  case 'f':
    if (parser->off + 3u > parser->len ||
        memcmp(parser->data + parser->off, "alse", 4u) != 0) {
      return luaL_error(L, "invalid JSON literal");
    }
    parser->off += 4u;
    lua_pushboolean(L, 0);
    return 1;
  case 'n':
    if (parser->off + 2u > parser->len ||
        memcmp(parser->data + parser->off, "ull", 3u) != 0) {
      return luaL_error(L, "invalid JSON literal");
    }
    parser->off += 3u;
    lua_pushnil(L);
    return 1;
  default:
    parser->off--;
    return ljlua_json_parse_number(L, parser);
  }
}

static int ljlua_push_json_from_text(lua_State *L, const char *json,
                                     size_t len) {
  ljlua_json_parser parser;

  parser.data = (const unsigned char *)json;
  parser.len = len;
  parser.off = 0u;
  ljlua_json_parse_value(L, &parser, 0u);
  ljlua_json_skip_space(&parser);
  if (parser.off != parser.len) {
    return luaL_error(L, "trailing data after JSON value");
  }
  return 1;
}

static void ljlua_json_builder_cleanup(lua_State *L,
                                       ljlua_json_builder *builder) {
  size_t i;

  if (builder == NULL) {
    return;
  }
  if (L != NULL) {
    if (builder->root_ref != LUA_NOREF && builder->root_ref != LUA_REFNIL) {
      luaL_unref(L, LUA_REGISTRYINDEX, builder->root_ref);
    }
    for (i = 0u; i < builder->frame_count; ++i) {
      if (builder->frames[i].table_ref != LUA_NOREF &&
          builder->frames[i].table_ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, builder->frames[i].table_ref);
      }
      free(builder->frames[i].pending_key);
      builder->frames[i].pending_key = NULL;
    }
  } else {
    for (i = 0u; i < builder->frame_count; ++i) {
      free(builder->frames[i].pending_key);
      builder->frames[i].pending_key = NULL;
    }
  }
  free(builder->frames);
  free(builder->token.data);
  memset(builder, 0, sizeof(*builder));
  builder->root_ref = LUA_NOREF;
}

static void ljlua_json_builder_init(ljlua_json_builder *builder, lua_State *L) {
  memset(builder, 0, sizeof(*builder));
  builder->L = L;
  builder->root_ref = LUA_NOREF;
}

static int ljlua_json_builder_push_frame(ljlua_json_builder *builder,
                                         ljlua_json_builder_frame_kind kind,
                                         int table_ref) {
  if (builder->frame_count == builder->frame_cap) {
    size_t next_cap = builder->frame_cap == 0u ? 4u : builder->frame_cap * 2u;
    void *next = realloc(builder->frames, next_cap * sizeof(*builder->frames));
    if (next == NULL) {
      return 0;
    }
    builder->frames = (ljlua_json_builder_frame *)next;
    builder->frame_cap = next_cap;
  }
  memset(&builder->frames[builder->frame_count], 0,
         sizeof(builder->frames[builder->frame_count]));
  builder->frames[builder->frame_count].kind = kind;
  builder->frames[builder->frame_count].table_ref = table_ref;
  builder->frames[builder->frame_count].array_index = 1u;
  builder->frames[builder->frame_count].state =
      (kind == LJLUA_JSON_FRAME_OBJECT) ? LJLUA_JSON_OBJECT_KEY_OR_END
                                        : LJLUA_JSON_ARRAY_VALUE_OR_END;
  builder->frames[builder->frame_count].pending_key = NULL;
  builder->frame_count++;
  return 1;
}

static ljlua_json_builder_frame *
ljlua_json_builder_top(ljlua_json_builder *builder) {
  if (builder->frame_count == 0u) {
    return NULL;
  }
  return &builder->frames[builder->frame_count - 1u];
}

static int ljlua_json_builder_attach_value(lua_State *L,
                                           ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);

  if (frame == NULL) {
    if (builder->root_ref != LUA_NOREF) {
      lua_pop(L, 1);
      return luaL_error(L, "multiple JSON root values in Lua builder");
    }
    builder->root_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    builder->finished = 1;
    return 1;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, frame->table_ref);
  lua_insert(L, -2);
  if (frame->kind == LJLUA_JSON_FRAME_ARRAY) {
    lua_rawseti(L, -2, (lua_Integer)frame->array_index++);
    frame->state = LJLUA_JSON_ARRAY_COMMA_OR_END;
  } else {
    if (frame->pending_key == NULL) {
      lua_pop(L, 1);
      lua_pop(L, 1);
      return luaL_error(L, "object value missing key in Lua builder");
    }
    lua_setfield(L, -2, frame->pending_key);
    free(frame->pending_key);
    frame->pending_key = NULL;
    frame->state = LJLUA_JSON_OBJECT_COMMA_OR_END;
  }
  lua_pop(L, 1);
  return 1;
}

static int ljlua_json_builder_finish_string(lua_State *L,
                                            ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);
  int was_key = builder->token_is_key;

  builder->token_mode = LJLUA_JSON_TOKEN_NONE;
  builder->token_escape = 0;
  builder->unicode_digits_needed = 0u;
  builder->unicode_accum = 0u;
  builder->unicode_pending_high = 0u;
  if (was_key) {
    char *copy = (char *)malloc(builder->token.len + 1u);
    if (copy == NULL) {
      return luaL_error(L, "failed to allocate JSON object key");
    }
    memcpy(copy, builder->token.data ? builder->token.data : "",
           builder->token.len);
    copy[builder->token.len] = '\0';
    builder->token.len = 0u;
    builder->token_is_key = 0;
    if (frame == NULL || frame->kind != LJLUA_JSON_FRAME_OBJECT ||
        frame->state != LJLUA_JSON_OBJECT_KEY_OR_END) {
      free(copy);
      return luaL_error(L, "object key outside object context");
    }
    free(frame->pending_key);
    frame->pending_key = copy;
    frame->state = LJLUA_JSON_OBJECT_COLON;
    return 1;
  }
  lua_pushlstring(L, builder->token.data ? builder->token.data : "",
                  builder->token.len);
  builder->token.len = 0u;
  builder->token_is_key = 0;
  return ljlua_json_builder_attach_value(L, builder);
}

static int ljlua_json_builder_close_container(lua_State *L,
                                              ljlua_json_builder *builder) {
  ljlua_json_builder_frame frame;

  if (builder->frame_count == 0u) {
    return luaL_error(L, "unexpected JSON container terminator");
  }
  frame = builder->frames[builder->frame_count - 1u];
  builder->frame_count--;
  lua_rawgeti(L, LUA_REGISTRYINDEX, frame.table_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, frame.table_ref);
  free(frame.pending_key);
  return ljlua_json_builder_attach_value(L, builder);
}

static lonejson_status
ljlua_json_builder_callback_failed(lua_State *L, lonejson_error *error) {
  const char *msg = lua_tostring(L, -1);
  ljlua_set_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                  msg ? msg : "Lua JSON visitor callback failed");
  lua_pop(L, 1);
  return LONEJSON_STATUS_CALLBACK_FAILED;
}

static int ljlua_json_builder_begin_token(ljlua_json_builder *builder,
                                          ljlua_json_builder_token_mode mode,
                                          int is_key) {
  builder->token_mode = mode;
  builder->token_is_key = is_key;
  builder->token.len = 0u;
  return 1;
}

static int ljlua_json_builder_prepare_key(ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);

  if (frame != NULL && frame->kind == LJLUA_JSON_FRAME_OBJECT &&
      frame->state == LJLUA_JSON_OBJECT_COMMA_OR_END) {
    frame->state = LJLUA_JSON_OBJECT_KEY_OR_END;
  }
  return 1;
}

static int ljlua_json_builder_prepare_value(ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);

  if (frame == NULL) {
    return 1;
  }
  if (frame->kind == LJLUA_JSON_FRAME_OBJECT &&
      frame->state == LJLUA_JSON_OBJECT_COLON) {
    frame->state = LJLUA_JSON_OBJECT_VALUE;
  } else if (frame->kind == LJLUA_JSON_FRAME_ARRAY &&
             frame->state == LJLUA_JSON_ARRAY_COMMA_OR_END) {
    frame->state = LJLUA_JSON_ARRAY_VALUE_OR_END;
  }
  return 1;
}

static int ljlua_json_builder_append_chunk(ljlua_json_builder *builder,
                                           const char *data, size_t len) {
  return ljlua_json_buf_append(&builder->token, data, len);
}

static int
ljlua_json_builder_push_container(lua_State *L, ljlua_json_builder *builder,
                                  ljlua_json_builder_frame_kind kind) {
  ljlua_json_builder_prepare_value(builder);
  lua_createtable(L, kind == LJLUA_JSON_FRAME_ARRAY ? 4 : 0,
                  kind == LJLUA_JSON_FRAME_OBJECT ? 4 : 0);
  ljlua_json_mark_shape(L, -1,
                        kind == LJLUA_JSON_FRAME_OBJECT ? "object" : "array");
  if (!ljlua_json_builder_push_frame(builder, kind,
                                     luaL_ref(L, LUA_REGISTRYINDEX))) {
    lua_pop(L, 1);
    return luaL_error(L, "failed to grow Lua JSON builder stack");
  }
  return 1;
}

static int ljlua_json_builder_push_boolean(lua_State *L,
                                           ljlua_json_builder *builder,
                                           int boolean_value) {
  lua_pushboolean(L, boolean_value);
  return ljlua_json_builder_attach_value(L, builder);
}

static int ljlua_json_builder_push_null(lua_State *L,
                                        ljlua_json_builder *builder) {
  if (ljlua_json_builder_top(builder) == NULL) {
    lua_pushnil(L);
  } else {
    ljlua_push_json_null(L);
  }
  return ljlua_json_builder_attach_value(L, builder);
}

static int ljlua_json_builder_finish_number(lua_State *L,
                                            ljlua_json_builder *builder) {
  builder->token_mode = LJLUA_JSON_TOKEN_NONE;
  ljlua_push_json_from_text(L, builder->token.data ? builder->token.data : "",
                            builder->token.len);
  builder->token.len = 0u;
  return ljlua_json_builder_attach_value(L, builder);
}

static lonejson_status ljlua_json_value_object_begin(void *user,
                                                     lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_push_container(state->builder.L, &state->builder,
                                        LJLUA_JSON_FRAME_OBJECT) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_object_end(void *user,
                                                   lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_close_container(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_array_begin(void *user,
                                                    lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_push_container(state->builder.L, &state->builder,
                                        LJLUA_JSON_FRAME_ARRAY) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_array_end(void *user,
                                                  lonejson_error *error) {
  return ljlua_json_value_object_end(user, error);
}

static lonejson_status ljlua_json_value_key_begin(void *user,
                                                  lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_key(&state->builder);
  ljlua_json_builder_begin_token(&state->builder, LJLUA_JSON_TOKEN_STRING, 1);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_key_chunk(void *user, const char *data,
                                                  size_t len,
                                                  lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  if (!ljlua_json_builder_append_chunk(&state->builder, data, len)) {
    return ljlua_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                           "failed to grow Lua JSON key buffer");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_key_end(void *user,
                                                lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_finish_string(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_string_begin(void *user,
                                                     lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  ljlua_json_builder_begin_token(&state->builder, LJLUA_JSON_TOKEN_STRING, 0);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_string_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  return ljlua_json_value_key_chunk(user, data, len, error);
}

static lonejson_status ljlua_json_value_string_end(void *user,
                                                   lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  state->builder.token_is_key = 0;
  if (ljlua_json_builder_finish_string(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_number_begin(void *user,
                                                     lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  ljlua_json_builder_begin_token(&state->builder, LJLUA_JSON_TOKEN_NUMBER, 0);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_number_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  return ljlua_json_value_key_chunk(user, data, len, error);
}

static lonejson_status ljlua_json_value_number_end(void *user,
                                                   lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_finish_number(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_boolean(void *user, int boolean_value,
                                                lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  if (ljlua_json_builder_push_boolean(state->builder.L, &state->builder,
                                      boolean_value) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_null(void *user,
                                             lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                           "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  if (ljlua_json_builder_push_null(state->builder.L, &state->builder) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static int ljlua_json_table_is_array(lua_State *L, int index) {
  size_t count = 0u;
  size_t n;

  index = lua_absindex(L, index);
  n = (size_t)lua_rawlen(L, index);
  lua_pushnil(L);
  while (lua_next(L, index) != 0) {
    if (!lua_isinteger(L, -2)) {
      lua_pop(L, 2);
      return 0;
    }
    {
      lua_Integer key = lua_tointeger(L, -2);
      if (key <= 0 || (size_t)key > n) {
        lua_pop(L, 2);
        return 0;
      }
    }
    count++;
    lua_pop(L, 1);
  }
  return count != 0u && count == n;
}

static int ljlua_encode_json_value(lua_State *L, int index,
                                   ljlua_json_out *out);

static void ljlua_free_json_keys(ljlua_json_key *keys, size_t count,
                                 ljlua_json_key *inline_keys) {
  size_t i;

  if (keys == NULL) {
    return;
  }
  for (i = 0u; i < count; ++i) {
    if (keys[i].text != NULL) {
      if (keys[i].heap_allocated) {
#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
        ljlua_test_note_json_key_free(keys[i].len + 1u);
#endif
        free(keys[i].text);
      }
      keys[i].text = NULL;
    }
  }
  if (keys != inline_keys) {
    free(keys);
  }
}

static void ljlua_copy_json_key(lua_State *L, ljlua_json_key *key,
                                const char *text, size_t len, char *inline_text,
                                size_t inline_capacity, size_t *inline_used) {
  key->heap_allocated = len >= inline_capacity - *inline_used;
  if (key->heap_allocated) {
    key->text = (char *)malloc(len + 1u);
    if (key->text == NULL) {
      luaL_error(L, "failed to allocate JSON object key");
      return;
    }
  } else {
    key->text = inline_text + *inline_used;
    *inline_used += len + 1u;
  }
  memcpy(key->text, text, len);
  key->text[len] = '\0';
  key->len = len;
#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
  if (key->heap_allocated) {
    ljlua_test_note_json_key_alloc(len + 1u);
  }
#endif
}

static int ljlua_encode_json_string(lua_State *L, int index,
                                    ljlua_json_out *out) {
  size_t len;
  const char *text = luaL_checklstring(L, index, &len);
  lonejson_error error;
  lonejson_status status;
  char quoted[128];
  size_t i;

  if (len > sizeof(quoted) - 2u && out->sink == ljlua_scalar_output_sink) {
    ljlua_scalar_output *output = (ljlua_scalar_output *)out->user;
    if (len > SIZE_MAX - 2u) {
      return luaL_error(L, "failed to grow JSON buffer");
    }
    if (output->buf->max_cap != 0u && len + 2u >= output->buf->max_cap) {
      return luaL_error(L, "serializer-owned output exceeds max_output_bytes");
    }
    if (!ljlua_json_buf_reserve(output->buf, len + 2u)) {
      return luaL_error(L, "failed to grow JSON buffer");
    }
  }

  for (i = 0u; i < len; ++i) {
    unsigned char ch = (unsigned char)text[i];
    if (ch < 0x20u || ch == '"' || ch == '\\') {
      break;
    }
  }
  if (i == len) {
    if (len <= sizeof(quoted) - 2u) {
      quoted[0] = '"';
      memcpy(quoted + 1u, text, len);
      quoted[len + 1u] = '"';
      return ljlua_json_out_write(L, out, quoted, len + 2u);
    }
    ljlua_json_out_write(L, out, "\"", 1u);
    ljlua_json_out_write(L, out, text, len);
    return ljlua_json_out_write(L, out, "\"", 1u);
  }

  memset(&error, 0, sizeof(error));
  ljlua_json_out_write(L, out, "\"", 1u);
  status = ljlua_emit_escaped_fragment_from(
      out->sink, out->user, &error, (const unsigned char *)text, len, i);
  if (status != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to encode JSON string: %s",
                      error.message[0] != '\0'
                          ? error.message
                          : lonejson_status_string(status));
  }
  return ljlua_json_out_write(L, out, "\"", 1u);
}

/* Keep diagnostic storage out of the recursive encoder's active frames. */
static int ljlua_json_out_write_key(lua_State *L, ljlua_json_out *out,
                                    const ljlua_json_key *key) {
  lonejson_error error;
  lonejson_status status;

  lonejson_error_init(&error);
  status = ljlua_emit_escaped_fragment(
      out->sink, out->user, &error, (const unsigned char *)key->text, key->len);
  if (status != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to encode JSON object key: %s",
                      error.message[0] != '\0'
                          ? error.message
                          : lonejson_status_string(status));
  }
  return 1;
}

typedef struct ljlua_json_encode_frame {
  int value_index;
  int object;
  size_t index;
  size_t count;
  ljlua_json_key *keys;
  ljlua_json_key inline_keys[8];
  char inline_text[128];
} ljlua_json_encode_frame;

typedef struct ljlua_json_encode_stack {
  size_t count;
  size_t capacity;
  ljlua_json_encode_frame frames[1];
} ljlua_json_encode_stack;

#define LJLUA_JSON_ENCODE_STACK_MT "lonejson.json_encode_stack"

static int ljlua_json_encode_stack_gc(lua_State *L) {
  ljlua_json_encode_stack *stack =
      (ljlua_json_encode_stack *)lua_touserdata(L, 1);
  size_t i;
  for (i = 0u; i < stack->count; ++i) {
    ljlua_json_encode_frame *frame = &stack->frames[i];
    ljlua_free_json_keys(frame->keys, frame->count, frame->inline_keys);
    frame->keys = NULL;
  }
  stack->count = 0u;
  return 0;
}

static ljlua_json_encode_stack *ljlua_json_encode_stack_new(lua_State *L,
                                                            size_t capacity) {
  size_t bytes = offsetof(ljlua_json_encode_stack, frames) +
                 capacity * sizeof(ljlua_json_encode_frame);
  ljlua_json_encode_stack *stack =
      (ljlua_json_encode_stack *)lua_newuserdatauv(L, bytes, 0);
  stack->count = 0u;
  stack->capacity = capacity;
  luaL_getmetatable(L, LJLUA_JSON_ENCODE_STACK_MT);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, ljlua_json_encode_stack_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, ljlua_json_encode_stack_gc);
    lua_setfield(L, -2, "__close");
    /* Publish only the complete metatable: allocation failure during setup
     * must not leave later calls with a non-closable workspace. */
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, LJLUA_JSON_ENCODE_STACK_MT);
  }
  lua_setmetatable(L, -2);
  return stack;
}

static ljlua_json_encode_stack *
ljlua_json_encode_stack_grow(lua_State *L, ljlua_json_encode_stack *old,
                             int stack_index) {
  ljlua_json_encode_stack *next =
      ljlua_json_encode_stack_new(L, old->capacity * 2u);
  size_t i;
  memcpy(next->frames, old->frames, old->count * sizeof(*old->frames));
  for (i = 0u; i < old->count; ++i) {
    ljlua_json_encode_frame *before = &old->frames[i];
    ljlua_json_encode_frame *after = &next->frames[i];
    size_t k;
    if (before->keys == before->inline_keys) {
      after->keys = after->inline_keys;
    }
    if (after->keys != NULL) {
      for (k = 0u; k < after->count; ++k) {
        if (!after->keys[k].heap_allocated) {
          after->keys[k].text =
              after->inline_text + (before->keys[k].text - before->inline_text);
        }
      }
    }
  }
  next->count = old->count;
  old->count = 0u;
  lua_replace(L, stack_index);
  return next;
}

static int ljlua_json_out_write_indent(lua_State *L, ljlua_json_out *out,
                                       size_t depth);

static int ljlua_encode_json_scalar(lua_State *L, int index,
                                    ljlua_json_out *out) {
  if (ljlua_is_json_null(L, index)) {
    return ljlua_json_out_write_cstr(L, out, "null");
  }
  switch (lua_type(L, index)) {
  case LUA_TNIL:
    return ljlua_json_out_write_cstr(L, out, "null");
  case LUA_TBOOLEAN:
    return ljlua_json_out_write_cstr(
        L, out, lua_toboolean(L, index) ? "true" : "false");
  case LUA_TNUMBER: {
    char text[64];
#if defined(LUA_MAXINTEGER)
    if (lua_isinteger(L, index)) {
      snprintf(text, sizeof(text), "%lld", (long long)lua_tointeger(L, index));
    } else
#endif
    {
      double number = (double)lua_tonumber(L, index);
      if (!ljlua_is_finite_f64(number)) {
        return luaL_error(L, "JSON numbers must be finite");
      }
      snprintf(text, sizeof(text), "%.17g", number);
    }
    return ljlua_json_out_write_cstr(L, out, text);
  }
  case LUA_TSTRING:
    return ljlua_encode_json_string(L, index, out);
  default:
    return luaL_error(L, "unsupported Lua type for JSON value");
  }
}

static int ljlua_encode_json_walk(lua_State *L, int index, ljlua_json_out *out,
                                  int pretty) {
  ljlua_json_encode_stack *stack;
  int stack_index;
  int pending;

  index = lua_absindex(L, index);
  if (lua_type(L, index) != LUA_TTABLE || ljlua_is_json_null(L, index)) {
    return ljlua_encode_json_scalar(L, index, out);
  }
  luaL_checkstack(L, 8, "JSON encoding");
  stack_index = lua_gettop(L) + 1;
  stack = ljlua_json_encode_stack_new(L, 4u);
  /* Close on protected-call errors as well as normal completion, so sorted
   * keys are released immediately even when a sink callback fails. */
  lua_toclose(L, stack_index);
  pending = index;
  for (;;) {
    ljlua_json_encode_frame *frame;
    luaL_checkstack(L, 8, "JSON encoding");
    if (pending != 0) {
      const char *shape;
      size_t i;
      size_t capacity = 8u;
      size_t inline_used = 0u;
      if (stack->count >= 128u) {
        return luaL_error(L, "JSON value nesting exceeds Lua binding limit");
      }
      for (i = 0u; i < stack->count; ++i) {
        if (lua_topointer(L, stack->frames[i].value_index) ==
            lua_topointer(L, pending)) {
          return luaL_error(L, "cyclic Lua tables cannot be encoded as JSON");
        }
      }
      if (stack->count == stack->capacity) {
        stack = ljlua_json_encode_stack_grow(L, stack, stack_index);
      }
      frame = &stack->frames[stack->count++];
      memset(frame, 0, sizeof(*frame));
      frame->value_index = pending;
      shape = ljlua_json_shape(L, pending);
      frame->object = (shape != NULL && strcmp(shape, "object") == 0) ||
                      !((shape != NULL && strcmp(shape, "array") == 0) ||
                        ljlua_json_table_is_array(L, pending));
      if (frame->object) {
        frame->keys = frame->inline_keys;
        lua_pushnil(L);
        while (lua_next(L, pending) != 0) {
          size_t len;
          const char *key;
          if (lua_type(L, -2) != LUA_TSTRING) {
            return luaL_error(L, "JSON object keys must be strings");
          }
          key = lua_tolstring(L, -2, &len);
          if (frame->count == capacity) {
            ljlua_json_key *keys;
            capacity *= 2u;
            if (frame->keys == frame->inline_keys) {
              keys = (ljlua_json_key *)malloc(capacity * sizeof(*keys));
              if (keys == NULL)
                return luaL_error(L, "failed to allocate JSON object keys");
              memcpy(keys, frame->keys, frame->count * sizeof(*keys));
            } else {
              keys = (ljlua_json_key *)realloc(frame->keys,
                                               capacity * sizeof(*keys));
              if (keys == NULL)
                return luaL_error(L, "failed to grow JSON object keys");
            }
            frame->keys = keys;
          }
          ljlua_copy_json_key(L, &frame->keys[frame->count], key, len,
                              frame->inline_text, sizeof(frame->inline_text),
                              &inline_used);
          ++frame->count;
          lua_pop(L, 1);
        }
        if (frame->count > 1u) {
          qsort(frame->keys, frame->count, sizeof(*frame->keys),
                ljlua_json_key_compare);
        }
      } else {
        frame->count = (size_t)lua_rawlen(L, pending);
      }
      ljlua_json_out_write(L, out, frame->object ? "{" : "[", 1u);
      pending = 0;
    }
    frame = &stack->frames[stack->count - 1u];
    if (frame->index == frame->count) {
      int value_index = frame->value_index;
      if (pretty && frame->count != 0u) {
        ljlua_json_out_write_indent(L, out, stack->count - 1u);
      }
      ljlua_json_out_write(L, out, frame->object ? "}" : "]", 1u);
      ljlua_free_json_keys(frame->keys, frame->count, frame->inline_keys);
      frame->keys = NULL;
      --stack->count;
      if (stack->count == 0u) {
        lua_settop(L, stack_index - 1);
        return 1;
      }
      lua_settop(L, value_index - 1);
      continue;
    }
    if (frame->index != 0u) {
      ljlua_json_out_write(L, out, ",", 1u);
    }
    if (pretty) {
      ljlua_json_out_write_indent(L, out, stack->count);
    }
    if (frame->object) {
      const ljlua_json_key *key = &frame->keys[frame->index];
      ljlua_json_out_write(L, out, "\"", 1u);
      ljlua_json_out_write_key(L, out, key);
      ljlua_json_out_write(L, out, pretty ? "\": " : "\":", pretty ? 3u : 2u);
      lua_pushlstring(L, key->text, key->len);
      lua_gettable(L, frame->value_index);
    } else {
      lua_rawgeti(L, frame->value_index, (lua_Integer)frame->index + 1);
    }
    ++frame->index;
    if (lua_type(L, -1) == LUA_TTABLE && !ljlua_is_json_null(L, -1)) {
      pending = lua_gettop(L);
    } else {
      ljlua_encode_json_scalar(L, -1, out);
      lua_pop(L, 1);
    }
  }
}

static int ljlua_encode_json_value(lua_State *L, int index,
                                   ljlua_json_out *out) {
  return ljlua_encode_json_walk(L, index, out, 0);
}

static int ljlua_push_json_value(lua_State *L,
                                 const lonejson_json_value *value) {
  ljlua_json_buf buf;
  lonejson_error error;
  lonejson_status status;

  if (value == NULL || value->kind == LONEJSON_JSON_VALUE_NULL) {
    lua_pushnil(L);
    return 1;
  }
  memset(&buf, 0, sizeof(buf));
  status = lonejson_json_value_write_to_sink(value, ljlua_json_buf_sink, &buf,
                                             &error);
  if (status != LONEJSON_STATUS_OK) {
    free(buf.data);
    return luaL_error(L, "failed to materialize JSON value: %s", error.message);
  }
  ljlua_push_json_from_text(L, buf.data, buf.len);
  free(buf.data);
  return 1;
}

static int ljlua_encode_json_buffer_pcall(lua_State *L) {
  ljlua_json_out out;

  out.sink = ljlua_json_buf_sink;
  out.user = lua_touserdata(L, 1);
  ljlua_encode_json_value(L, 2, &out);
  return 0;
}

/* Buffer owners outside the direct encoder must recover from Lua errors before
 * releasing their own resources or returning through a C callback boundary. */
static int ljlua_encode_json_buffer(lua_State *L, int index,
                                    ljlua_json_buf *buf) {
  int status;

  index = lua_absindex(L, index);
  memset(buf, 0, sizeof(*buf));
  lua_pushcfunction(L, ljlua_encode_json_buffer_pcall);
  lua_pushlightuserdata(L, buf);
  lua_pushvalue(L, index);
  status = lua_pcall(L, 2, 0, 0);
  if (status != LUA_OK) {
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
  }
  return status;
}

static int ljlua_assign_json_value_from_lua(lua_State *L,
                                            lonejson_json_value *value,
                                            int value_index) {
  ljlua_json_buf buf;
  lonejson_error error;
  lonejson_status status;

  if (lua_type(L, value_index) != LUA_TTABLE) {
    ljlua_scalar_output output;
    ljlua_json_out out;

    /* Scalars have no nested native owners. The sink releases partial output
     * on failure, so assignment needs no protected call for these values. */
    memset(&buf, 0, sizeof(buf));
    output.L = L;
    output.buf = &buf;
    out.sink = ljlua_scalar_output_sink;
    out.user = &output;
    ljlua_encode_json_value(L, value_index, &out);
  } else if (ljlua_encode_json_buffer(L, value_index, &buf) != LUA_OK) {
    return lua_error(L);
  }
  status = lonejson_json_value_set_buffer(value, buf.data, buf.len, &error);
  free(buf.data);
  if (status != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to assign JSON value: %s", error.message);
  }
  return 1;
}

static int ljlua_field_kind_from_name(const char *name) {
  if (strcmp(name, "string") == 0) {
    return LJLUA_FIELD_STRING;
  }
  if (strcmp(name, "spooled_text") == 0) {
    return LJLUA_FIELD_SPOOLED_TEXT;
  }
  if (strcmp(name, "spooled_bytes") == 0) {
    return LJLUA_FIELD_SPOOLED_BYTES;
  }
  if (strcmp(name, "json_value") == 0) {
    return LJLUA_FIELD_JSON_VALUE;
  }
  if (strcmp(name, "i64") == 0) {
    return LJLUA_FIELD_I64;
  }
  if (strcmp(name, "u64") == 0) {
    return LJLUA_FIELD_U64;
  }
  if (strcmp(name, "f64") == 0) {
    return LJLUA_FIELD_F64;
  }
  if (strcmp(name, "boolean") == 0) {
    return LJLUA_FIELD_BOOL;
  }
  if (strcmp(name, "object") == 0) {
    return LJLUA_FIELD_OBJECT;
  }
  if (strcmp(name, "string_array") == 0) {
    return LJLUA_FIELD_STRING_ARRAY;
  }
  if (strcmp(name, "i64_array") == 0) {
    return LJLUA_FIELD_I64_ARRAY;
  }
  if (strcmp(name, "u64_array") == 0) {
    return LJLUA_FIELD_U64_ARRAY;
  }
  if (strcmp(name, "f64_array") == 0) {
    return LJLUA_FIELD_F64_ARRAY;
  }
  if (strcmp(name, "boolean_array") == 0) {
    return LJLUA_FIELD_BOOL_ARRAY;
  }
  if (strcmp(name, "object_array") == 0) {
    return LJLUA_FIELD_OBJECT_ARRAY;
  }
  return 0;
}
