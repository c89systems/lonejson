static void ljlua_schema_destroy(ljlua_schema *schema);

typedef struct ljlua_align_char_ptr_probe {
  char c;
  char *value;
} ljlua_align_char_ptr_probe;

typedef struct ljlua_align_i64_probe {
  char c;
  lonejson_int64 value;
} ljlua_align_i64_probe;

typedef struct ljlua_align_u64_probe {
  char c;
  lonejson_uint64 value;
} ljlua_align_u64_probe;

typedef struct ljlua_align_f64_probe {
  char c;
  double value;
} ljlua_align_f64_probe;

typedef struct ljlua_align_bool_probe {
  char c;
  bool value;
} ljlua_align_bool_probe;

typedef struct ljlua_align_spooled_probe {
  char c;
  lonejson_spooled value;
} ljlua_align_spooled_probe;

typedef struct ljlua_align_json_value_probe {
  char c;
  lonejson_json_value value;
} ljlua_align_json_value_probe;

typedef struct ljlua_align_string_array_probe {
  char c;
  lonejson_string_array value;
} ljlua_align_string_array_probe;

typedef struct ljlua_align_i64_array_probe {
  char c;
  lonejson_i64_array value;
} ljlua_align_i64_array_probe;

typedef struct ljlua_align_u64_array_probe {
  char c;
  lonejson_u64_array value;
} ljlua_align_u64_array_probe;

typedef struct ljlua_align_f64_array_probe {
  char c;
  lonejson_f64_array value;
} ljlua_align_f64_array_probe;

typedef struct ljlua_align_bool_array_probe {
  char c;
  lonejson_bool_array value;
} ljlua_align_bool_array_probe;

typedef struct ljlua_align_object_array_probe {
  char c;
  lonejson_object_array value;
} ljlua_align_object_array_probe;

static size_t ljlua_max_size(size_t a, size_t b) { return a > b ? a : b; }

static lonejson_uint64 ljlua_next_map_cookie(void) {
  static unsigned int counter = 1u;
  unsigned int value;

  value = ++counter;
  if (value == 0u ||
      value == (unsigned int)(LONEJSON_MAP_COOKIE_VALUE & 0xffffffffu)) {
    value = ++counter;
  }
  return (LONEJSON_MAP_COOKIE_VALUE & ~((lonejson_uint64)0xffffffffu)) |
         ((lonejson_uint64)value & 0xffffffffu);
}

static size_t ljlua_member_size_for_kind(const ljlua_field_meta *meta) {
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    return (meta->field.storage == LONEJSON_STORAGE_FIXED)
               ? meta->field.fixed_capacity
               : sizeof(char *);
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES:
    return sizeof(lonejson_spooled);
  case LJLUA_FIELD_JSON_VALUE:
    return sizeof(lonejson_json_value);
  case LJLUA_FIELD_I64:
    return sizeof(lonejson_int64);
  case LJLUA_FIELD_U64:
    return sizeof(lonejson_uint64);
  case LJLUA_FIELD_F64:
    return sizeof(double);
  case LJLUA_FIELD_BOOL:
    return sizeof(bool);
  case LJLUA_FIELD_OBJECT:
    return meta->subschema ? meta->subschema->record_size : 0u;
  case LJLUA_FIELD_STRING_ARRAY:
    return sizeof(lonejson_string_array);
  case LJLUA_FIELD_I64_ARRAY:
    return sizeof(lonejson_i64_array);
  case LJLUA_FIELD_U64_ARRAY:
    return sizeof(lonejson_u64_array);
  case LJLUA_FIELD_F64_ARRAY:
    return sizeof(lonejson_f64_array);
  case LJLUA_FIELD_BOOL_ARRAY:
    return sizeof(lonejson_bool_array);
  case LJLUA_FIELD_OBJECT_ARRAY:
    return sizeof(lonejson_object_array);
  default:
    return 0u;
  }
}

static size_t ljlua_fixed_array_elem_size(const ljlua_field_meta *meta) {
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    return sizeof(char *);
  case LJLUA_FIELD_I64_ARRAY:
    return sizeof(lonejson_int64);
  case LJLUA_FIELD_U64_ARRAY:
    return sizeof(lonejson_uint64);
  case LJLUA_FIELD_F64_ARRAY:
    return sizeof(double);
  case LJLUA_FIELD_BOOL_ARRAY:
    return sizeof(bool);
  case LJLUA_FIELD_OBJECT_ARRAY:
    return meta->subschema ? meta->subschema->record_size : 0u;
  default:
    return 0u;
  }
}

static size_t ljlua_member_align_for_kind(const ljlua_field_meta *meta) {
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    return (meta->field.storage == LONEJSON_STORAGE_FIXED)
               ? 1u
               : offsetof(ljlua_align_char_ptr_probe, value);
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES:
    return offsetof(ljlua_align_spooled_probe, value);
  case LJLUA_FIELD_JSON_VALUE:
    return offsetof(ljlua_align_json_value_probe, value);
  case LJLUA_FIELD_I64:
    return offsetof(ljlua_align_i64_probe, value);
  case LJLUA_FIELD_U64:
    return offsetof(ljlua_align_u64_probe, value);
  case LJLUA_FIELD_F64:
    return offsetof(ljlua_align_f64_probe, value);
  case LJLUA_FIELD_BOOL:
    return offsetof(ljlua_align_bool_probe, value);
  case LJLUA_FIELD_OBJECT:
    return meta->subschema ? meta->subschema->record_align : 1u;
  case LJLUA_FIELD_STRING_ARRAY:
    return offsetof(ljlua_align_string_array_probe, value);
  case LJLUA_FIELD_I64_ARRAY:
    return offsetof(ljlua_align_i64_array_probe, value);
  case LJLUA_FIELD_U64_ARRAY:
    return offsetof(ljlua_align_u64_array_probe, value);
  case LJLUA_FIELD_F64_ARRAY:
    return offsetof(ljlua_align_f64_array_probe, value);
  case LJLUA_FIELD_BOOL_ARRAY:
    return offsetof(ljlua_align_bool_array_probe, value);
  case LJLUA_FIELD_OBJECT_ARRAY:
    return offsetof(ljlua_align_object_array_probe, value);
  default:
    return 1u;
  }
}

static size_t ljlua_fixed_array_elem_align(const ljlua_field_meta *meta) {
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    return offsetof(ljlua_align_char_ptr_probe, value);
  case LJLUA_FIELD_I64_ARRAY:
    return offsetof(ljlua_align_i64_probe, value);
  case LJLUA_FIELD_U64_ARRAY:
    return offsetof(ljlua_align_u64_probe, value);
  case LJLUA_FIELD_F64_ARRAY:
    return offsetof(ljlua_align_f64_probe, value);
  case LJLUA_FIELD_BOOL_ARRAY:
    return offsetof(ljlua_align_bool_probe, value);
  case LJLUA_FIELD_OBJECT_ARRAY:
    return meta->subschema ? meta->subschema->record_align : 1u;
  default:
    return 1u;
  }
}

static int ljlua_field_kind_is_nullable_primitive(ljlua_field_kind kind) {
  return kind == LJLUA_FIELD_I64 || kind == LJLUA_FIELD_U64 ||
         kind == LJLUA_FIELD_F64 || kind == LJLUA_FIELD_BOOL;
}

static int ljlua_compile_field(lua_State *L, ljlua_runtime_ud *runtime_ud,
                               int index, ljlua_field_meta *meta);
static int ljlua_finalize_schema(ljlua_schema *schema);
static FILE *ljlua_check_file(lua_State *L, int index);
static ljlua_path_ud *ljlua_check_path(lua_State *L, int index);
static ljlua_compiled_path *ljlua_find_compiled_path(ljlua_schema *schema,
                                                     const char *path);
static ljlua_compiled_path *
ljlua_compile_path(lua_State *L, ljlua_schema *schema, const char *path);
static int ljlua_push_compiled_path_value(lua_State *L, void *record,
                                          ljlua_compiled_path *compiled,
                                          int owner_index);
static lonejson_read_result
ljlua_array_mem_stream_read(void *user, unsigned char *buffer, size_t capacity);
static int ljlua_monotonic_ns(lua_State *L);
static int ljlua_json_null(lua_State *L);

static FILE *ljlua_dup_fd_to_file(lua_State *L, int fd, const char *mode) {
  int dup_fd;
  FILE *fp;

  dup_fd = dup(fd);
  if (dup_fd < 0) {
    luaL_error(L, "failed to duplicate fd %d: %s", fd, strerror(errno));
  }
  fp = fdopen(dup_fd, mode);
  if (fp == NULL) {
    close(dup_fd);
    luaL_error(L, "failed to fdopen duplicated fd %d: %s", fd, strerror(errno));
  }
  return fp;
}

static int ljlua_check_fd_like(lua_State *L, int index) {
  if (lua_isinteger(L, index)) {
    return (int)lua_tointeger(L, index);
  }
  return fileno(ljlua_check_file(L, index));
}

static int ljlua_compile_nested_schema(lua_State *L,
                                       ljlua_runtime_ud *runtime_ud, int index,
                                       ljlua_schema **out_schema) {
  ljlua_schema *schema;
  size_t count;
  size_t i;

  luaL_checktype(L, index, LUA_TTABLE);
  count = (size_t)lua_rawlen(L, index);
  schema = (ljlua_schema *)calloc(1u, sizeof(*schema));
  if (schema == NULL) {
    return luaL_error(L, "failed to allocate schema");
  }
  *out_schema = schema;
  schema->runtime = runtime_ud->runtime;
  schema->runtime_ud = runtime_ud;
  schema->name = ljlua_strdup("nested");
  schema->map.name = schema->name;
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
    lua_rawgeti(L, index, (lua_Integer)i + 1);
    if (ljlua_compile_field(L, runtime_ud, lua_gettop(L), &schema->metas[i]) !=
        0) {
      lua_pop(L, 1);
      return lua_error(L);
    }
    lua_pop(L, 1);
  }
  ljlua_finalize_schema(schema);
  return 0;
}

static int ljlua_compile_field(lua_State *L, ljlua_runtime_ud *runtime_ud,
                               int index, ljlua_field_meta *meta) {
  const char *kind_name;
  int nullable;
  size_t i;

  memset(meta, 0, sizeof(*meta));
  luaL_checktype(L, index, LUA_TTABLE);
  lua_getfield(L, index, "name");
  meta->name = ljlua_strdup(luaL_checkstring(L, -1));
  lua_pop(L, 1);
  lua_getfield(L, index, "kind");
  kind_name = luaL_checkstring(L, -1);
  meta->lua_kind = (ljlua_field_kind)ljlua_field_kind_from_name(kind_name);
  lua_pop(L, 1);
  if (meta->lua_kind == 0) {
    return luaL_error(L, "unsupported field kind '%s'", kind_name);
  }

  meta->field.json_key = meta->name;
  meta->field.json_key_len = strlen(meta->name);
  meta->field.json_key_first =
      meta->field.json_key_len != 0u ? (unsigned char)meta->name[0] : '\0';
  meta->field.json_key_last =
      meta->field.json_key_len != 0u
          ? (unsigned char)meta->name[meta->field.json_key_len - 1u]
          : '\0';
  lua_getfield(L, index, "required");
  if (lua_toboolean(L, -1)) {
    meta->field.flags |= LONEJSON_FIELD_REQUIRED;
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "nullable");
  nullable = lua_toboolean(L, -1) ? 1 : 0;
  lua_pop(L, 1);
  if (nullable) {
    if (!ljlua_field_kind_is_nullable_primitive(meta->lua_kind)) {
      return luaL_error(
          L,
          "field '%s' nullable is only supported for optional primitive scalar "
          "fields",
          meta->name);
    }
    if ((meta->field.flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      return luaL_error(L, "field '%s' cannot be both required and nullable",
                        meta->name);
    }
    meta->field.flags |=
        LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL;
  }

  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->field.storage = LONEJSON_STORAGE_FIXED;
      meta->field.fixed_capacity = (size_t)lua_tointeger(L, -1);
      meta->field.overflow_policy =
          ljlua_parse_overflow_policy(L, index, "overflow");
    } else {
      meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
      meta->field.overflow_policy = LONEJSON_OVERFLOW_FAIL;
    }
    lua_pop(L, 1);
    meta->field.kind = LONEJSON_FIELD_KIND_STRING;
    break;
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES: {
    meta->field.kind = (meta->lua_kind == LJLUA_FIELD_SPOOLED_TEXT)
                           ? LONEJSON_FIELD_KIND_STRING_STREAM
                           : LONEJSON_FIELD_KIND_BASE64_STREAM;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    meta->field.spool_class = (meta->lua_kind == LJLUA_FIELD_SPOOLED_BYTES)
                                  ? LONEJSON_SPOOL_CLASS_BLOB
                                  : LONEJSON_SPOOL_CLASS_DEFAULT;
    lua_getfield(L, index, "class");
    if (!lua_isnil(L, -1)) {
      const char *name = luaL_checkstring(L, -1);

      if (strcmp(name, "default") == 0) {
        meta->field.spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
      } else if (strcmp(name, "blob") == 0) {
        meta->field.spool_class = LONEJSON_SPOOL_CLASS_BLOB;
      } else if (strcmp(name, "large_text") == 0) {
        meta->field.spool_class = LONEJSON_SPOOL_CLASS_LARGE_TEXT;
      } else {
        return luaL_error(L, "unsupported spool class '%s'", name);
      }
    }
    lua_pop(L, 1);
    break;
  }
  case LJLUA_FIELD_JSON_VALUE:
    meta->field.kind = LONEJSON_FIELD_KIND_JSON_VALUE;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    meta->field.flags |= LONEJSON_FIELD_JSON_VALUE_DEFAULT_CAPTURE;
    break;
  case LJLUA_FIELD_I64:
    meta->field.kind = LONEJSON_FIELD_KIND_I64;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_U64:
    meta->field.kind = LONEJSON_FIELD_KIND_U64;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_F64:
    meta->field.kind = LONEJSON_FIELD_KIND_F64;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_BOOL:
    meta->field.kind = LONEJSON_FIELD_KIND_BOOL;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_OBJECT:
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_getfield(L, index, "fields");
    if (ljlua_compile_nested_schema(L, runtime_ud, lua_gettop(L),
                                    &meta->subschema) != 0) {
      lua_pop(L, 1);
      return 1;
    }
    lua_pop(L, 1);
    meta->field.kind = (meta->lua_kind == LJLUA_FIELD_OBJECT)
                           ? LONEJSON_FIELD_KIND_OBJECT
                           : LONEJSON_FIELD_KIND_OBJECT_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    meta->field.submap = &meta->subschema->map;
    if (meta->lua_kind == LJLUA_FIELD_OBJECT_ARRAY) {
      meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
      meta->field.elem_size = meta->subschema->record_size;
      meta->field.overflow_policy =
          ljlua_parse_overflow_policy(L, index, "overflow");
      lua_getfield(L, index, "fixed_capacity");
      if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
        meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
      }
      lua_pop(L, 1);
    }
    break;
  case LJLUA_FIELD_STRING_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_STRING_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_I64_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_I64_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(lonejson_int64);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_U64_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_U64_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(lonejson_uint64);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_F64_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_F64_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(double);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_BOOL_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_BOOL_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(bool);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  default:
    return luaL_error(L, "unsupported field kind");
  }

  meta->member_size = ljlua_member_size_for_kind(meta);
  if (meta->member_size == 0u) {
    return luaL_error(L, "failed to size field '%s'", meta->name);
  }
  if (meta->fixed_array_capacity != 0u) {
    i = ljlua_fixed_array_elem_size(meta);
    if (i == 0u) {
      return luaL_error(L, "invalid fixed-capacity array field '%s'",
                        meta->name);
    }
  }
  return 0;
}

static int ljlua_finalize_schema(ljlua_schema *schema) {
  size_t offset;
  size_t i;

  schema->has_json_value = 0;
  schema->needs_record_init = 0;
  schema->stack_record_safe = 1;
  schema->record_align = 1u;
  offset = 0u;
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];
    size_t align = ljlua_member_align_for_kind(meta);

    /* Only flat scalar fields with inline storage can survive Lua errors
     * without native cleanup. Other layouts use protected scratch storage. */
    if (!ljlua_field_kind_is_nullable_primitive(meta->lua_kind) &&
        !(meta->lua_kind == LJLUA_FIELD_STRING &&
          meta->field.storage == LONEJSON_STORAGE_FIXED)) {
      schema->stack_record_safe = 0;
    }

    if (meta->lua_kind == LJLUA_FIELD_JSON_VALUE ||
        ((meta->lua_kind == LJLUA_FIELD_OBJECT ||
          meta->lua_kind == LJLUA_FIELD_OBJECT_ARRAY) &&
         meta->subschema != NULL && meta->subschema->has_json_value)) {
      schema->has_json_value = 1;
    }
    if (meta->fixed_array_capacity != 0u ||
        meta->lua_kind == LJLUA_FIELD_SPOOLED_TEXT ||
        meta->lua_kind == LJLUA_FIELD_SPOOLED_BYTES ||
        meta->lua_kind == LJLUA_FIELD_JSON_VALUE ||
        (meta->lua_kind == LJLUA_FIELD_OBJECT && meta->subschema != NULL &&
         meta->subschema->needs_record_init)) {
      schema->needs_record_init = 1;
    }
    schema->record_align = ljlua_max_size(schema->record_align, align);
    offset = ljlua_align(offset, align);
    meta->field.struct_offset = offset;
    schema->fields[i] = meta->field;
    schema->fields[i].struct_offset = offset;
    offset += meta->member_size;
  }
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];

    if (meta->fixed_array_capacity != 0u) {
      size_t elem_size = ljlua_fixed_array_elem_size(meta);
      size_t bytes = elem_size * meta->fixed_array_capacity;
      size_t elem_align = ljlua_fixed_array_elem_align(meta);

      schema->record_align = ljlua_max_size(schema->record_align, elem_align);
      offset = ljlua_align(offset, elem_align);
      meta->fixed_array_offset = offset;
      offset += bytes;
    }
    if (lonejson_field_has_presence(&meta->field)) {
      offset = ljlua_align(offset, sizeof(int));
      meta->field.presence_offset = offset;
      schema->fields[i].presence_offset = offset;
      offset += sizeof(int);
    }
  }
  schema->record_size = ljlua_align(offset, schema->record_align);
  schema->record_offset =
      ljlua_align(offsetof(ljlua_record_ud, data), schema->record_align);
  schema->map.name = schema->name;
  schema->map.struct_size = schema->record_size;
  schema->map.field_count = schema->field_count;
  schema->map.fields = schema->fields;
  schema->map._map_identity = &schema->map;
  schema->map._map_cookie = ljlua_next_map_cookie();
  return 0;
}

static void ljlua_schema_destroy(ljlua_schema *schema) {
  size_t i;
  ljlua_compiled_path *compiled;

  if (schema == NULL) {
    return;
  }
  for (i = 0u; schema->metas != NULL && i < schema->field_count; ++i) {
    free(schema->metas[i].name);
    if (schema->metas[i].subschema != NULL) {
      ljlua_schema_destroy(schema->metas[i].subschema);
    }
  }
  compiled = schema->path_cache;
  while (compiled != NULL) {
    ljlua_compiled_path *next = compiled->next;
    free(compiled->steps);
    free(compiled->path);
    free(compiled);
    compiled = next;
  }
  free(schema->scratch_record);
  free(schema->encode_buffer);
  free(schema->fields);
  free(schema->metas);
  free(schema->name);
  free(schema);
}

static unsigned char *ljlua_schema_borrow_scratch(ljlua_schema *schema,
                                                  int *owned) {
  if (schema != NULL && schema->scratch_record != NULL &&
      !schema->scratch_in_use) {
    schema->scratch_in_use = 1;
    if (owned != NULL) {
      *owned = 0;
    }
    return schema->scratch_record;
  }
  if (owned != NULL) {
    *owned = 1;
  }
  return (unsigned char *)malloc(schema != NULL ? schema->record_size : 0u);
}

static void ljlua_schema_release_scratch(ljlua_schema *schema,
                                         unsigned char *record, int owned) {
  if (record == NULL) {
    return;
  }
  if (owned) {
    free(record);
  } else if (schema != NULL) {
    schema->scratch_in_use = 0;
  }
}

static void ljlua_prepare_record_storage(lua_State *L, ljlua_schema *schema,
                                         void *record) {
  size_t i;

  memset(record, 0, schema->record_size);
  if (!schema->needs_record_init) {
    return;
  }
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];
    unsigned char *base = (unsigned char *)record + meta->field.struct_offset;
    void *fixed_ptr = (unsigned char *)record + meta->fixed_array_offset;

    if (meta->fixed_array_capacity == 0u) {
      continue;
    }
    switch (meta->lua_kind) {
    case LJLUA_FIELD_STRING_ARRAY: {
      lonejson_string_array *arr = (lonejson_string_array *)base;
      arr->items = (char **)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_I64_ARRAY: {
      lonejson_i64_array *arr = (lonejson_i64_array *)base;
      arr->items = (lonejson_int64 *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_U64_ARRAY: {
      lonejson_u64_array *arr = (lonejson_u64_array *)base;
      arr->items = (lonejson_uint64 *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_F64_ARRAY: {
      lonejson_f64_array *arr = (lonejson_f64_array *)base;
      arr->items = (double *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_BOOL_ARRAY: {
      lonejson_bool_array *arr = (lonejson_bool_array *)base;
      arr->items = (bool *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_OBJECT_ARRAY: {
      lonejson_object_array *arr = (lonejson_object_array *)base;
      arr->items = fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->elem_size = meta->subschema->record_size;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    default:
      break;
    }
  }
  {
    lonejson_error error;
    if (lonejson_init(schema->runtime, &schema->map, record, &error) !=
        LONEJSON_STATUS_OK)
      luaL_error(L, "%s", error.message);
  }
}

static ljlua_schema_ud *ljlua_check_schema(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_schema_ud *ud = (ljlua_schema_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_SCHEMA_MAGIC) {
      return ud;
    }
  }
  return (ljlua_schema_ud *)luaL_checkudata(L, index, LJLUA_SCHEMA_MT);
}

static ljlua_record_ud *ljlua_test_record(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_record_ud *ud = (ljlua_record_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_RECORD_MAGIC) {
      return ud;
    }
  }
  return NULL;
}

static ljlua_record_ud *ljlua_check_record(lua_State *L, int index) {
  ljlua_record_ud *ud = ljlua_test_record(L, index);

  if (ud != NULL) {
    return ud;
  }
  return (ljlua_record_ud *)luaL_checkudata(L, index, LJLUA_RECORD_MT);
}

static unsigned char *ljlua_record_data(ljlua_record_ud *record) {
  if (record == NULL || record->schema == NULL) {
    return NULL;
  }
  return (unsigned char *)record + record->schema->record_offset;
}

static ljlua_stream_ud *ljlua_check_stream(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_stream_ud *ud = (ljlua_stream_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_STREAM_MAGIC) {
      return ud;
    }
  }
  return (ljlua_stream_ud *)luaL_checkudata(L, index, LJLUA_STREAM_MT);
}

static ljlua_array_stream_ud *ljlua_check_array_stream(lua_State *L,
                                                       int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_array_stream_ud *ud =
        (ljlua_array_stream_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_ARRAY_STREAM_MAGIC) {
      return ud;
    }
  }
  return (ljlua_array_stream_ud *)luaL_checkudata(L, index,
                                                  LJLUA_ARRAY_STREAM_MT);
}

static ljlua_spool_ud *ljlua_check_spool(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_spool_ud *ud = (ljlua_spool_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_SPOOL_MAGIC) {
      return ud;
    }
  }
  return (ljlua_spool_ud *)luaL_checkudata(L, index, LJLUA_SPOOL_MT);
}

static ljlua_path_ud *ljlua_check_path(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_path_ud *ud = (ljlua_path_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_PATH_MAGIC) {
      return ud;
    }
  }
  return (ljlua_path_ud *)luaL_checkudata(L, index, LJLUA_PATH_MT);
}

static FILE *ljlua_check_file(lua_State *L, int index) {
  luaL_Stream *stream =
      (luaL_Stream *)luaL_checkudata(L, index, LUA_FILEHANDLE);

  luaL_argcheck(L, stream->closef != NULL, index, "closed file");
  return stream->f;
}
