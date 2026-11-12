static int ljlua_schema_new(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = ljlua_check_runtime(L, 1);
  const char *name = luaL_checkstring(L, 2);
  size_t count;
  size_t i;
  ljlua_schema *schema;
  ljlua_schema_ud *ud;

  luaL_checktype(L, 3, LUA_TTABLE);
  count = (size_t)lua_rawlen(L, 3);
  ud = (ljlua_schema_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->magic = LJLUA_SCHEMA_MAGIC;
  ud->runtime_ref = LUA_NOREF;
  luaL_setmetatable(L, LJLUA_SCHEMA_MT);
  lua_pushvalue(L, 1);
  ud->runtime_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  schema = (ljlua_schema *)calloc(1u, sizeof(*schema));
  if (schema == NULL) {
    return luaL_error(L, "failed to allocate schema");
  }
  ud->schema = schema;
  schema->runtime = runtime_ud->runtime;
  schema->runtime_ud = runtime_ud;
  schema->name = ljlua_strdup(name);
  schema->field_count = count;
  schema->metas = (ljlua_field_meta *)calloc(count, sizeof(schema->metas[0]));
  if (schema->metas == NULL) {
    return luaL_error(L, "failed to allocate schema fields");
  }
  schema->fields = (lonejson_field *)calloc(count, sizeof(schema->fields[0]));
  if (schema->fields == NULL) {
    return luaL_error(L, "failed to allocate runtime map fields");
  }
  for (i = 0u; i < count; ++i) {
    lua_rawgeti(L, 3, (lua_Integer)i + 1);
    ljlua_compile_field(L, runtime_ud, lua_gettop(L), &schema->metas[i]);
    lua_pop(L, 1);
  }
  ljlua_finalize_schema(schema);
  if (schema->record_size != 0u) {
    schema->scratch_record = (unsigned char *)malloc(schema->record_size);
    if (schema->scratch_record == NULL) {
      return luaL_error(L, "failed to allocate schema scratch record");
    }
  }
  return 1;
}

static int ljlua_schema_gc(lua_State *L) {
  ljlua_schema_ud *ud = ljlua_check_schema(L, 1);
  ljlua_schema_destroy(ud->schema);
  ud->schema = NULL;
  if (ud->runtime_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->runtime_ref);
    ud->runtime_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_schema_new_record(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud;
  size_t record_bytes;

  record_bytes =
      schema_ud->schema->record_offset + schema_ud->schema->record_size;
  record_ud = (ljlua_record_ud *)ljlua_newuserdata_slots(L, record_bytes, 0);
  memset(record_ud, 0, record_bytes);
  record_ud->magic = LJLUA_RECORD_MAGIC;
  record_ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  record_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  record_ud->cleared = 1;
  luaL_setmetatable(L, LJLUA_RECORD_MT);
  ljlua_prepare_record_storage(L, record_ud->schema,
                               ljlua_record_data(record_ud));
  return 1;
}

static int ljlua_record_gc(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  if (ud->schema != NULL) {
    ljlua_cleanup_record_storage(ud->schema, ljlua_record_data(ud));
  }
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_record_index(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  const char *key = luaL_checkstring(L, 2);
  ljlua_compiled_path *compiled;
  lua_CFunction method = NULL;
  switch (key[0]) {
  case 'g':
    if (strcmp(key, "get") == 0)
      method = ljlua_record_get;
    break;
  case 't':
    if (strcmp(key, "to_table") == 0)
      method = ljlua_record_to_table_method;
    break;
  case 'c':
    if (strcmp(key, "count") == 0)
      method = ljlua_record_count;
    else if (strcmp(key, "clear") == 0)
      method = ljlua_record_clear;
    break;
  }
  if (method != NULL) {
    lua_pushcfunction(L, method);
    return 1;
  }
  compiled = ljlua_find_compiled_path(ud->schema, key);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, ud->schema, key);
  }
  if (compiled != NULL && compiled->step_count == 1u &&
      compiled->steps[0].index == 0u) {
    return ljlua_push_compiled_path_value(L, ljlua_record_data(ud), compiled,
                                          1);
  }
  lua_pushnil(L);
  return 1;
}

static int ljlua_record_to_table_method(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  return ljlua_record_to_table(L, ud->schema, ljlua_record_data(ud));
}

static int ljlua_record_get(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  ljlua_compiled_path *compiled = NULL;

  if (lua_type(L, 2) == LUA_TSTRING) {
    const char *path = lua_tostring(L, 2);
    compiled = ljlua_find_compiled_path(ud->schema, path);
    if (compiled == NULL) {
      compiled = ljlua_compile_path(L, ud->schema, path);
    }
  } else {
    ljlua_path_ud *path_ud = ljlua_check_path(L, 2);
    if (path_ud->schema != ud->schema) {
      return luaL_error(L, "path belongs to a different schema");
    }
    compiled = path_ud->compiled;
  }
  return ljlua_push_compiled_path_value(L, ljlua_record_data(ud), compiled, 1);
}

static int ljlua_record_count(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  ljlua_field_meta *meta = NULL;
  void *ptr;

  if (lua_type(L, 2) == LUA_TSTRING) {
    const char *key = lua_tostring(L, 2);
    ljlua_compiled_path *compiled = ljlua_find_compiled_path(ud->schema, key);
    if (compiled == NULL) {
      compiled = ljlua_compile_path(L, ud->schema, key);
    }
    if (compiled == NULL || compiled->step_count != 1u ||
        compiled->steps[0].index != 0u) {
      return luaL_error(L, "unknown field '%s'", key);
    }
    meta = compiled->steps[0].meta;
  } else {
    ljlua_path_ud *path_ud = ljlua_check_path(L, 2);
    if (path_ud->schema != ud->schema) {
      return luaL_error(L, "path belongs to a different schema");
    }
    if (path_ud->compiled->step_count != 1u ||
        path_ud->compiled->steps[0].index != 0u) {
      return luaL_error(L, "count path must reference a top-level array field");
    }
    meta = path_ud->compiled->steps[0].meta;
  }
  ptr = (unsigned char *)ljlua_record_data(ud) + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_record_clear(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  do {
    lonejson_error lifecycle_error;
    if (lonejson_reset(ud->schema->runtime, &ud->schema->map,
                       ljlua_record_data(ud),
                       &lifecycle_error) != LONEJSON_STATUS_OK)
      return luaL_error(L, "%s", lifecycle_error.message);
  } while (0);
  ud->cleared = 1;
  return 0;
}

static int ljlua_path_gc(lua_State *L) {
  ljlua_path_ud *ud = ljlua_check_path(L, 1);
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  ud->schema = NULL;
  ud->compiled = NULL;
  return 0;
}

static int ljlua_path_get(lua_State *L) {
  ljlua_path_ud *path_ud = ljlua_check_path(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);

  if (path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  return ljlua_push_compiled_path_value(L, ljlua_record_data(record_ud),
                                        path_ud->compiled, 2);
}

static int ljlua_path_count(lua_State *L) {
  ljlua_path_ud *path_ud = ljlua_check_path(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  ljlua_field_meta *meta;
  void *ptr;

  if (path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  if (path_ud->compiled->step_count != 1u ||
      path_ud->compiled->steps[0].index != 0u) {
    return luaL_error(L, "count path must reference a top-level array field");
  }
  meta = path_ud->compiled->steps[0].meta;
  ptr =
      (unsigned char *)ljlua_record_data(record_ud) + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_getter_call(lua_State *L) {
  ljlua_path_ud *path_ud =
      (ljlua_path_ud *)lua_touserdata(L, lua_upvalueindex(1));
  ljlua_record_ud *record_ud = ljlua_check_record(L, 1);

  if (path_ud == NULL || path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  return ljlua_push_compiled_path_value(L, ljlua_record_data(record_ud),
                                        path_ud->compiled, 1);
}

static int ljlua_counter_call(lua_State *L) {
  ljlua_path_ud *path_ud =
      (ljlua_path_ud *)lua_touserdata(L, lua_upvalueindex(1));
  ljlua_record_ud *record_ud = ljlua_check_record(L, 1);
  ljlua_field_meta *meta;
  void *ptr;

  if (path_ud == NULL || path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  if (path_ud->compiled->step_count != 1u ||
      path_ud->compiled->steps[0].index != 0u) {
    return luaL_error(L, "count path must reference a top-level array field");
  }
  meta = path_ud->compiled->steps[0].meta;
  ptr =
      (unsigned char *)ljlua_record_data(record_ud) + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_schema_decode_into_buffer(lua_State *L, ljlua_schema *schema,
                                           void *record, const char *json,
                                           size_t len, lonejson *runtime) {
  lonejson_error error;
  lonejson_status status;

  status =
      lonejson_parse_buffer(runtime, &schema->map, record, json, len, &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_push_error(L, &error);
    return 0;
  }
  return 1;
}

static int ljlua_schema_decode_into(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  lonejson *runtime;
  size_t len;
  const char *json = luaL_checklstring(L, 3, &len);

  ljlua_reject_legacy_options(L, 4, "schema:decode_into");
  runtime = schema_ud->schema->runtime;
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    if (schema_ud->schema->runtime_ud->clear_destination) {
      do {
        lonejson_error lifecycle_error;
        if (lonejson_reset(schema_ud->schema->runtime, &schema_ud->schema->map,
                           ljlua_record_data(record_ud),
                           &lifecycle_error) != LONEJSON_STATUS_OK)
          return luaL_error(L, "%s", lifecycle_error.message);
      } while (0);
    }
    ljlua_prepare_record_json_value_capture(
        L, schema_ud->schema, ljlua_record_data(record_ud),
        schema_ud->schema->runtime_ud->clear_destination ? 0 : 1);
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  } else {
    if (schema_ud->schema->runtime_ud->clear_destination) {
      do {
        lonejson_error lifecycle_error;
        if (lonejson_reset(schema_ud->schema->runtime, &schema_ud->schema->map,
                           ljlua_record_data(record_ud),
                           &lifecycle_error) != LONEJSON_STATUS_OK)
          return luaL_error(L, "%s", lifecycle_error.message);
      } while (0);
    } else if (record_ud->cleared) {
      runtime = schema_ud->schema->runtime_ud->capture_runtime;
    }
  }
  if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema,
                                       ljlua_record_data(record_ud), json, len,
                                       runtime)) {
    return 2;
  }
  record_ud->cleared = 0;
  lua_pushvalue(L, 2);
  return 1;
}

static int ljlua_schema_decode_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  lonejson *runtime;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  ljlua_reject_legacy_options(L, 3, "schema:decode_path");
  runtime = schema_ud->schema->runtime;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(L, schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  status = lonejson_parse_path(runtime, &schema_ud->schema->map, record, path,
                               &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 2);
  lonejson *runtime;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  ljlua_reject_legacy_options(L, 3, "schema:decode_file");
  runtime = schema_ud->schema->runtime;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(L, schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  status = lonejson_parse_filep(runtime, &schema_ud->schema->map, record, fp,
                                &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 2);
  FILE *fp;
  lonejson *runtime;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  ljlua_reject_legacy_options(L, 3, "schema:decode_fd");
  runtime = schema_ud->schema->runtime;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(L, schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  fp = ljlua_dup_fd_to_file(L, fd, "rb");
  status = lonejson_parse_filep(runtime, &schema_ud->schema->map, record, fp,
                                &error);
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  lonejson *runtime;
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  memset(&ctx, 0, sizeof(ctx));
  runtime = schema_ud->schema->runtime;
  ljlua_reject_legacy_options(L, 3, "schema:decode");
  ljlua_prepare_record_storage(L, schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
    {
      lonejson_error error;
      lonejson_status status = lonejson_parse_buffer(
          runtime, &schema_ud->schema->map, record, json, len, &error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        ljlua_decode_context_cleanup(&ctx);
        ljlua_cleanup_record_storage(schema_ud->schema, record);
        ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
        lua_pushnil(L);
        ljlua_push_error(L, &error);
        return 2;
      }
    }
  } else {
    if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record, json,
                                         len, runtime)) {
      ljlua_cleanup_record_storage(schema_ud->schema, record);
      ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
      return 2;
    }
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_assign(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  do {
    lonejson_error lifecycle_error;
    if (lonejson_reset(schema_ud->schema->runtime, &schema_ud->schema->map,
                       ljlua_record_data(record_ud),
                       &lifecycle_error) != LONEJSON_STATUS_OK)
      return luaL_error(L, "%s", lifecycle_error.message);
  } while (0);
  record_ud->cleared = 1;
  ljlua_assign_table_to_record(L, schema_ud->schema,
                               ljlua_record_data(record_ud), 3);
  record_ud->cleared = 0;
  lua_pushvalue(L, 2);
  return 1;
}

static char *ljlua_serialize_value(lua_State *L, ljlua_schema *schema,
                                   void *record, size_t *out_len) {
  lonejson_error error;
  lonejson_status status;
  size_t needed = 0u;
  size_t capacity;
  size_t serialize_capacity;
  size_t max_output_bytes;
  size_t max_buffer_capacity;
  char *buffer;

  max_output_bytes = schema->runtime_ud->write_max_output_bytes;
  if (max_output_bytes == SIZE_MAX) {
    luaL_error(L, "max_output_bytes is too large");
  }
  max_buffer_capacity = max_output_bytes + 1u;
  capacity = schema->encode_capacity;
  if (capacity == 0u) {
    capacity = 256u;
  }
  if (capacity > max_buffer_capacity) {
    capacity = max_buffer_capacity;
  }
  if (schema->encode_buffer == NULL) {
    schema->encode_buffer = (char *)malloc(capacity);
    if (schema->encode_buffer == NULL) {
      luaL_error(L, "failed to allocate encode buffer");
    }
    schema->encode_capacity = capacity;
  }
  for (;;) {
    serialize_capacity = schema->encode_capacity;
    if (serialize_capacity > max_buffer_capacity) {
      serialize_capacity = max_buffer_capacity;
    }
    status = lonejson_serialize_buffer(schema->runtime, &schema->map, record,
                                       schema->encode_buffer,
                                       serialize_capacity, &needed, &error);
    if (status != LONEJSON_STATUS_OVERFLOW) {
      break;
    }
    if (needed > max_output_bytes) {
      luaL_error(L, "serializer-owned output exceeds max_output_bytes");
    }
    capacity = schema->encode_capacity * 2u;
    if (capacity < needed + 1u) {
      capacity = needed + 1u;
    }
    if (capacity > max_buffer_capacity) {
      capacity = max_buffer_capacity;
    }
    if (capacity < 256u && max_output_bytes >= 256u) {
      capacity = 256u;
    }
    buffer = (char *)realloc(schema->encode_buffer, capacity);
    if (buffer == NULL) {
      luaL_error(L, "failed to grow encode buffer");
    }
    schema->encode_buffer = buffer;
    schema->encode_capacity = capacity;
  }
  if (needed > max_output_bytes) {
    luaL_error(L, "serializer-owned output exceeds max_output_bytes");
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    luaL_error(L, "%s", error.message);
  }
  if (out_len != NULL) {
    *out_len = needed;
  }
  return schema->encode_buffer;
}

typedef struct ljlua_scratch_call {
  ljlua_schema *schema;
  unsigned char *record;
  char *json;
  size_t out_len;
} ljlua_scratch_call;

static int ljlua_scratch_assign_pcall(lua_State *L) {
  ljlua_scratch_call *call = (ljlua_scratch_call *)lua_touserdata(L, 1);
  ljlua_assign_table_to_record(L, call->schema, call->record, 2);
  return 0;
}

static int ljlua_scratch_encode_pcall(lua_State *L) {
  ljlua_scratch_call *call = (ljlua_scratch_call *)lua_touserdata(L, 1);
  ljlua_scratch_assign_pcall(L);
  call->json =
      ljlua_serialize_value(L, call->schema, call->record, &call->out_len);
  return 0;
}

static int ljlua_scratch_protected(lua_State *L, lua_CFunction operation,
                                   ljlua_scratch_call *call, int index) {
  index = lua_absindex(L, index);
  lua_pushcfunction(L, operation);
  lua_pushlightuserdata(L, call);
  lua_pushvalue(L, index);
  return lua_pcall(L, 2, 0, 0);
}

static int ljlua_assign_scratch(lua_State *L, ljlua_schema *schema,
                                unsigned char *record, int owned, int index) {
  ljlua_scratch_call call;
  int status;

  memset(&call, 0, sizeof(call));
  call.schema = schema;
  call.record = record;
  ljlua_prepare_record_storage(L, schema, record);
  status = ljlua_scratch_protected(L, ljlua_scratch_assign_pcall, &call, index);
  if (status != LUA_OK) {
    ljlua_cleanup_record_storage(schema, record);
    ljlua_schema_release_scratch(schema, record, owned);
  }
  return status;
}

static int ljlua_schema_encode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_test_record(L, 2);
  size_t out_len = 0u;
  char *json;

  ljlua_reject_legacy_options(L, 3, "schema:encode");
  if (record_ud != NULL) {
    if (record_ud->schema != schema_ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    json = ljlua_serialize_value(L, schema_ud->schema,
                                 ljlua_record_data(record_ud), &out_len);
  } else if (schema_ud->schema->stack_record_safe &&
             schema_ud->schema->record_size <= 256u) {
    union {
      lonejson_uint64 integer_align;
      double number_align;
      void *pointer_align;
      unsigned char bytes[256];
    } record;

    /* Inline-only records need no native cleanup across Lua errors and remain
     * independent during reentrant table access. */
    ljlua_prepare_record_storage(L, schema_ud->schema, record.bytes);
    ljlua_assign_table_to_record(L, schema_ud->schema, record.bytes, 2);
    json = ljlua_serialize_value(L, schema_ud->schema, record.bytes, &out_len);
  } else {
    int owned_record;
    int call_status;
    ljlua_scratch_call call;
    unsigned char *record;
    luaL_checkstack(L, 3, "schema encoding");
    record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    memset(&call, 0, sizeof(call));
    call.schema = schema_ud->schema;
    call.record = record;
    ljlua_prepare_record_storage(L, schema_ud->schema, record);
    call_status =
        ljlua_scratch_protected(L, ljlua_scratch_encode_pcall, &call, 2);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    if (call_status != LUA_OK) {
      return lua_error(L);
    }
    json = call.json;
    out_len = call.out_len;
  }
  lua_pushlstring(L, json, out_len);
  return 1;
}

static int ljlua_schema_write_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 3);
  lonejson_error error;
  lonejson_status status;

  ljlua_reject_legacy_options(L, 4, "schema:write_path");
  luaL_checkstack(L, 3, "schema writing");
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_path(
        schema_ud->schema->runtime, &schema_ud->schema->map,
        ljlua_record_data(record_ud), path, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    if (ljlua_assign_scratch(L, schema_ud->schema, record, owned_record, 2) !=
        LUA_OK) {
      return lua_error(L);
    }
    status =
        lonejson_serialize_path(schema_ud->schema->runtime,
                                &schema_ud->schema->map, record, path, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_schema_write_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 3);
  lonejson_error error;
  lonejson_status status;

  ljlua_reject_legacy_options(L, 4, "schema:write_file");
  luaL_checkstack(L, 3, "schema writing");
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_filep(schema_ud->schema->runtime,
                                      &schema_ud->schema->map,
                                      ljlua_record_data(record_ud), fp, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    if (ljlua_assign_scratch(L, schema_ud->schema, record, owned_record, 2) !=
        LUA_OK) {
      return lua_error(L);
    }
    status =
        lonejson_serialize_filep(schema_ud->schema->runtime,
                                 &schema_ud->schema->map, record, fp, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_schema_write_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 3);
  FILE *fp = NULL;
  lonejson_error error;
  lonejson_status status;

  ljlua_reject_legacy_options(L, 4, "schema:write_fd");
  luaL_checkstack(L, 3, "schema writing");
  fp = ljlua_dup_fd_to_file(L, fd, "wb");
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_filep(schema_ud->schema->runtime,
                                      &schema_ud->schema->map,
                                      ljlua_record_data(record_ud), fp, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      fclose(fp);
      return luaL_error(L, "failed to allocate encode buffer");
    }
    if (ljlua_assign_scratch(L, schema_ud->schema, record, owned_record, 2) !=
        LUA_OK) {
      fclose(fp);
      return lua_error(L);
    }
    status =
        lonejson_serialize_filep(schema_ud->schema->runtime,
                                 &schema_ud->schema->map, record, fp, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_stream_gc(lua_State *L) {
  ljlua_stream_ud *ud = ljlua_check_stream(L, 1);
  if (ud->stream != NULL) {
    lonejson_stream_close(ud->stream);
    ud->stream = NULL;
  }
  ud->input = NULL;
  ud->input_len = 0u;
  ud->input_offset = 0u;
  return 0;
}

static int ljlua_schema_stream_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  lonejson *runtime;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_reject_legacy_options(L, 3, "schema:stream_path");
  runtime = schema_ud->schema->runtime;
  ud = (ljlua_stream_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 2);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = schema_ud->schema->runtime_ud->clear_destination;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->stream =
      lonejson_stream_open_path(runtime, &schema_ud->schema->map, path, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  ljlua_set_uservalue_ref(L, -1, 1, 1);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 2);
  lonejson *runtime;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_reject_legacy_options(L, 3, "schema:stream_fd");
  runtime = schema_ud->schema->runtime;
  ud = (ljlua_stream_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 2);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = schema_ud->schema->runtime_ud->clear_destination;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->stream =
      lonejson_stream_open_fd(runtime, &schema_ud->schema->map, fd, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  ljlua_set_uservalue_ref(L, -1, 1, 1);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 2);
  lonejson *runtime;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_reject_legacy_options(L, 3, "schema:stream_file");
  runtime = schema_ud->schema->runtime;
  ud = (ljlua_stream_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 2);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = schema_ud->schema->runtime_ud->clear_destination;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    runtime = schema_ud->schema->runtime_ud->capture_runtime;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->stream =
      lonejson_stream_open_filep(runtime, &schema_ud->schema->map, fp, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  ljlua_set_uservalue_ref(L, -1, 1, 1);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}
