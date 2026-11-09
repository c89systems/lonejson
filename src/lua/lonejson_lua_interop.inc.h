#include "lonejson_lua.h"

static int ljlua_schema_view_is_ready(const lonejson_schema_view *view) {
  return view != NULL && view->size == sizeof(*view) &&
         view->abi_version == LONEJSON_VIEW_ABI_VERSION;
}

static int ljlua_record_view_is_ready(const lonejson_record_view *view) {
  return view != NULL && view->size == sizeof(*view) &&
         view->abi_version == LONEJSON_VIEW_ABI_VERSION;
}

static lonejson_status ljlua_schema_view_error(lonejson_error *error,
                                               const char *message) {
  return ljlua_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, "%s",
                         message);
}

static ljlua_schema_ud *ljlua_test_schema(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_schema_ud *ud = (ljlua_schema_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_SCHEMA_MAGIC && ud->schema != NULL) {
      return ud;
    }
  }
  return NULL;
}

static void ljlua_fill_schema_view(ljlua_schema *schema,
                                   lonejson_schema_view *out) {
  out->runtime = schema->runtime;
  out->map = &schema->map;
  out->record_size = schema->record_size;
  out->flags = 0u;
}

static void ljlua_fill_record_view(ljlua_record_ud *record,
                                   lonejson_record_view *out) {
  ljlua_fill_schema_view(record->schema, &out->schema);
  out->record = ljlua_record_data(record);
  out->flags = record->cleared ? 1u : 0u;
}

lonejson_status lonejson_lua_check_schema(lua_State *L, int index,
                                          lonejson_schema_view *out,
                                          lonejson_error *error) {
  ljlua_schema_ud *ud;

  if (!ljlua_schema_view_is_ready(out)) {
    return ljlua_schema_view_error(error,
                                   "invalid lonejson schema view ABI contract");
  }
  if (L == NULL) {
    return ljlua_schema_view_error(error, "Lua state is required");
  }
  ud = ljlua_test_schema(L, index);
  if (ud == NULL) {
    return ljlua_schema_view_error(error, "Lua value is not a lonejson schema");
  }
  ljlua_fill_schema_view(ud->schema, out);
  if (error != NULL) {
    lonejson_error_init(error);
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_lua_new_record(lua_State *L, int schema_index,
                                        int *record_index,
                                        lonejson_record_view *out,
                                        lonejson_error *error) {
  ljlua_schema_ud *schema_ud;
  ljlua_record_ud *record_ud;
  size_t record_bytes;

  if (!ljlua_record_view_is_ready(out)) {
    return ljlua_schema_view_error(error,
                                   "invalid lonejson record view ABI contract");
  }
  if (L == NULL) {
    return ljlua_schema_view_error(error, "Lua state is required");
  }
  schema_index = lua_absindex(L, schema_index);
  schema_ud = ljlua_test_schema(L, schema_index);
  if (schema_ud == NULL) {
    return ljlua_schema_view_error(error, "Lua value is not a lonejson schema");
  }
  record_bytes =
      schema_ud->schema->record_offset + schema_ud->schema->record_size;
  record_ud = (ljlua_record_ud *)ljlua_newuserdata_slots(L, record_bytes, 0);
  memset(record_ud, 0, record_bytes);
  record_ud->magic = LJLUA_RECORD_MAGIC;
  record_ud->schema = schema_ud->schema;
  lua_pushvalue(L, schema_index);
  record_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  record_ud->cleared = 1;
  luaL_setmetatable(L, LJLUA_RECORD_MT);
  ljlua_prepare_record_storage(L, record_ud->schema,
                               ljlua_record_data(record_ud));
  out->schema.size = sizeof(out->schema);
  out->schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  ljlua_fill_record_view(record_ud, out);
  if (record_index != NULL) {
    *record_index = lua_absindex(L, -1);
  }
  if (error != NULL) {
    lonejson_error_init(error);
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_lua_check_record(lua_State *L, int index,
                                          const lonejson_schema_view *schema,
                                          lonejson_record_view *out,
                                          lonejson_error *error) {
  ljlua_record_ud *record_ud;

  if (!ljlua_record_view_is_ready(out)) {
    return ljlua_schema_view_error(error,
                                   "invalid lonejson record view ABI contract");
  }
  if (schema != NULL && !ljlua_schema_view_is_ready(schema)) {
    return ljlua_schema_view_error(error,
                                   "invalid lonejson schema view ABI contract");
  }
  if (L == NULL) {
    return ljlua_schema_view_error(error, "Lua state is required");
  }
  record_ud = ljlua_test_record(L, index);
  if (record_ud == NULL || record_ud->schema == NULL) {
    return ljlua_schema_view_error(error, "Lua value is not a lonejson record");
  }
  if (schema != NULL && (schema->runtime != record_ud->schema->runtime ||
                         schema->map != &record_ud->schema->map)) {
    return ljlua_schema_view_error(error,
                                   "record belongs to a different schema");
  }
  out->schema.size = sizeof(out->schema);
  out->schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  ljlua_fill_record_view(record_ud, out);
  if (error != NULL) {
    lonejson_error_init(error);
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_lua_clear_record(lua_State *L, int record_index,
                                          lonejson_error *error) {
  ljlua_record_ud *record_ud;

  if (L == NULL) {
    return ljlua_schema_view_error(error, "Lua state is required");
  }
  record_ud = ljlua_test_record(L, record_index);
  if (record_ud == NULL || record_ud->schema == NULL) {
    return ljlua_schema_view_error(error, "Lua value is not a lonejson record");
  }
  do {
    lonejson_error lifecycle_error;
    if (lonejson_reset(record_ud->schema->runtime, &record_ud->schema->map,
                       ljlua_record_data(record_ud),
                       &lifecycle_error) != LONEJSON_STATUS_OK)
      return ljlua_set_error(error, lifecycle_error.code,
                             lifecycle_error.message);
  } while (0);
  record_ud->cleared = 1;
  if (error != NULL) {
    lonejson_error_init(error);
  }
  return LONEJSON_STATUS_OK;
}

int lonejson_lua_record_to_table(lua_State *L, int record_index) {
  ljlua_record_ud *record_ud;

  if (L == NULL) {
    return 0;
  }
  record_ud = ljlua_test_record(L, record_index);
  if (record_ud == NULL || record_ud->schema == NULL) {
    return 0;
  }
  ljlua_record_to_table(L, record_ud->schema, ljlua_record_data(record_ud));
  return lua_absindex(L, -1);
}

static lonejson_status
ljlua_parse_reader_into_record_ud(lua_State *L, ljlua_record_ud *record_ud,
                                  lonejson_reader_fn reader, void *userdata,
                                  lonejson_error *error) {
  lonejson *runtime;
  lonejson_status status;

  if (reader == NULL) {
    return ljlua_schema_view_error(error, "reader callback is required");
  }
  runtime = record_ud->schema->runtime;
  if (ljlua_schema_has_json_value(record_ud->schema)) {
    if (record_ud->schema->runtime_ud->clear_destination) {
      do {
        lonejson_error lifecycle_error;
        if (lonejson_reset(record_ud->schema->runtime, &record_ud->schema->map,
                           ljlua_record_data(record_ud),
                           &lifecycle_error) != LONEJSON_STATUS_OK)
          return ljlua_set_error(error, lifecycle_error.code,
                                 lifecycle_error.message);
      } while (0);
    }
    ljlua_prepare_record_json_value_capture(
        L, record_ud->schema, ljlua_record_data(record_ud),
        record_ud->schema->runtime_ud->clear_destination ? 0 : 1);
    runtime = record_ud->schema->runtime_ud->capture_runtime;
  } else if (record_ud->schema->runtime_ud->clear_destination) {
    do {
      lonejson_error lifecycle_error;
      if (lonejson_reset(record_ud->schema->runtime, &record_ud->schema->map,
                         ljlua_record_data(record_ud),
                         &lifecycle_error) != LONEJSON_STATUS_OK)
        return ljlua_set_error(error, lifecycle_error.code,
                               lifecycle_error.message);
    } while (0);
  } else if (record_ud->cleared) {
    runtime = record_ud->schema->runtime_ud->capture_runtime;
  }
  status = lonejson_parse_reader(runtime, &record_ud->schema->map,
                                 ljlua_record_data(record_ud), reader, userdata,
                                 error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    record_ud->cleared = 0;
  }
  return status;
}

lonejson_status
lonejson_lua_parse_reader_to_record(lua_State *L, int schema_index,
                                    lonejson_reader_fn reader, void *userdata,
                                    int *record_index, lonejson_error *error) {
  lonejson_record_view view;
  lonejson_status status;
  int index = 0;
  ljlua_record_ud *record_ud;

  view.size = sizeof(view);
  view.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status = lonejson_lua_new_record(L, schema_index, &index, &view, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  record_ud = ljlua_test_record(L, index);
  status =
      ljlua_parse_reader_into_record_ud(L, record_ud, reader, userdata, error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lua_remove(L, index);
    return status;
  }
  if (record_index != NULL) {
    *record_index = index;
  }
  return status;
}

lonejson_status
lonejson_lua_parse_reader_to_table(lua_State *L, int schema_index,
                                   lonejson_reader_fn reader, void *userdata,
                                   int *table_index, lonejson_error *error) {
  ljlua_schema_ud *schema_ud;
  lonejson *runtime;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;

  if (L == NULL) {
    return ljlua_schema_view_error(error, "Lua state is required");
  }
  if (reader == NULL) {
    return ljlua_schema_view_error(error, "reader callback is required");
  }
  schema_index = lua_absindex(L, schema_index);
  schema_ud = ljlua_test_schema(L, schema_index);
  if (schema_ud == NULL) {
    return ljlua_schema_view_error(error, "Lua value is not a lonejson schema");
  }
  runtime = schema_ud->schema->runtime;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return ljlua_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, "%s",
                           "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(L, schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  status = lonejson_parse_reader(runtime, &schema_ud->schema->map, record,
                                 reader, userdata, error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    return status;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  if (table_index != NULL) {
    *table_index = lua_absindex(L, -1);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return status;
}
