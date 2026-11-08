static int ljlua_runtime_gc(lua_State *L) {
  ljlua_runtime_ud *ud = ljlua_check_runtime(L, 1);

#ifdef LONEJSON_WITH_OIDC
  ljlua_auth_clear_http_provider(L, ud);
#endif
  if (ud->runtime != NULL) {
    lonejson_free(ud->runtime);
    ud->runtime = NULL;
  }
  if (ud->capture_runtime != NULL) {
    lonejson_free(ud->capture_runtime);
    ud->capture_runtime = NULL;
  }
  if (ud->fixed_string_scratch_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->fixed_string_scratch_ref);
    ud->fixed_string_scratch_ref = LUA_NOREF;
  }
  ud->magic = 0u;
  return 0;
}

static int ljlua_runtime_new(lua_State *L) {
  lonejson_config config;
  lonejson_config capture_config;
  lonejson_error error;
  ljlua_runtime_ud *ud;
  int fixed_string_scratch_ref;

  ljlua_parse_runtime_config(L, 1, &config);
  capture_config = config;
  capture_config.clear_destination_by_default = 0;
  fixed_string_scratch_ref =
      ljlua_runtime_config_fixed_string_scratch_ref(L, 1);
  ud = (ljlua_runtime_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->L = L;
  ud->fixed_string_scratch_ref = LUA_NOREF;
#ifdef LONEJSON_WITH_OIDC
  ud->http_provider_ref = LUA_NOREF;
#endif
  ud->runtime = lonejson_new(lua_isnoneornil(L, 1) ? NULL : &config, &error);
  if (ud->runtime == NULL) {
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->capture_runtime = lonejson_new(&capture_config, &error);
  if (ud->capture_runtime == NULL) {
    lonejson_free(ud->runtime);
    ud->runtime = NULL;
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->magic = LJLUA_RUNTIME_MAGIC;
  ud->clear_destination = config.clear_destination_by_default ? 1 : 0;
  ud->reject_duplicate_keys = config.reject_duplicate_keys_by_default ? 1 : 0;
  ud->write_pretty = config.write_pretty ? 1 : 0;
  ud->write_max_output_bytes = config.write_max_output_bytes != 0u
                                   ? config.write_max_output_bytes
                                   : LONEJSON_WRITE_MAX_OUTPUT_BYTES;
  ud->fixed_string_scratch_ref = fixed_string_scratch_ref;
  luaL_setmetatable(L, LJLUA_RUNTIME_MT);
  return 1;
}

#if defined(LONEJSON_TEST_MAP_ANALYSIS_COUNTER)
static int ljlua_test_get_map_analysis_count(lua_State *L) {
  lua_pushinteger(L, 0);
  return 1;
}

static int ljlua_test_reset_map_analysis_count(lua_State *L) {
  (void)L;
  return 0;
}

static int ljlua_test_map_cache_enabled(lua_State *L) {
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_test_schema_map_cacheable(lua_State *L) {
  ljlua_schema_ud *ud = ljlua_check_schema(L, 1);

  lua_pushboolean(L, ud->schema->map._map_identity == &ud->schema->map);
  return 1;
}

static int ljlua_test_schema_map_cookie(lua_State *L) {
  ljlua_schema_ud *ud = ljlua_check_schema(L, 1);

  lua_pushinteger(L, (lua_Integer)(ud->schema->map._map_cookie &
                                   (lonejson_uint64)0xffffffffu));
  return 1;
}
#endif

#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
static int ljlua_test_fail_json_buf_allocation_after(lua_State *L) {
  lua_Integer count = luaL_checkinteger(L, 1);
  g_ljlua_test_json_buf_failure_enabled = count >= 0;
  g_ljlua_test_json_buf_allocations_before_failure =
      count >= 0 ? (size_t)count : 0u;
  return 0;
}

static int ljlua_test_json_buf_overflow(lua_State *L) {
  ljlua_json_buf buf;
  lonejson_error error;
  size_t i;
  const size_t lengths[] = {0u, SIZE_MAX - 1u, SIZE_MAX};
  const size_t extras[] = {SIZE_MAX, 1u, 0u};

  for (i = 0u; i < 3u; ++i) {
    memset(&buf, 0, sizeof(buf));
    buf.len = lengths[i];
    buf.cap = SIZE_MAX;
    memset(&error, 0xa5, sizeof(error));
    if (ljlua_json_buf_sink(&buf, "", extras[i], &error) !=
            LONEJSON_STATUS_ALLOCATION_FAILED ||
        error.code != LONEJSON_STATUS_ALLOCATION_FAILED ||
        strcmp(error.message, "failed to grow JSON buffer") != 0 ||
        buf.data != NULL || buf.len != lengths[i] || buf.cap != SIZE_MAX) {
      return luaL_error(L, "JSON buffer overflow was not rejected safely");
    }
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_test_reset_encode_stats_lua(lua_State *L) {
  (void)L;
  ljlua_test_reset_encode_stats();
  return 0;
}

static int ljlua_test_get_encode_stats_lua(lua_State *L) {
  lua_createtable(L, 0, 3);
  lua_pushinteger(
      L, (lua_Integer)g_ljlua_test_encode_stats.json_buf_peak_capacity);
  lua_setfield(L, -2, "json_buf_peak_capacity");
  lua_pushinteger(L,
                  (lua_Integer)g_ljlua_test_encode_stats.json_key_bytes_live);
  lua_setfield(L, -2, "json_key_bytes_live");
  lua_pushinteger(
      L, (lua_Integer)g_ljlua_test_encode_stats.json_key_peak_bytes_live);
  lua_setfield(L, -2, "json_key_peak_bytes_live");
  return 1;
}
#endif

static lonejson_base64_variant ljlua_base64_variant_arg(lua_State *L,
                                                        int index) {
  const char *variant;

  if (lua_isnoneornil(L, index)) {
    return LONEJSON_BASE64_STANDARD;
  }
  variant = luaL_checkstring(L, index);
  if (strcmp(variant, "standard") == 0 || strcmp(variant, "std") == 0) {
    return LONEJSON_BASE64_STANDARD;
  }
  if (strcmp(variant, "standard_raw") == 0 || strcmp(variant, "std_raw") == 0 ||
      strcmp(variant, "raw") == 0) {
    return LONEJSON_BASE64_STANDARD_RAW;
  }
  if (strcmp(variant, "url") == 0 || strcmp(variant, "base64url") == 0) {
    return LONEJSON_BASE64_URL;
  }
  if (strcmp(variant, "url_raw") == 0 ||
      strcmp(variant, "base64url_raw") == 0 || strcmp(variant, "jwt") == 0) {
    return LONEJSON_BASE64_URL_RAW;
  }
  luaL_argerror(L, index,
                "base64 variant must be standard, standard_raw, url, or "
                "url_raw");
  return LONEJSON_BASE64_STANDARD;
}

static int ljlua_base64_encode(lua_State *L) {
  const char *data;
  size_t len;
  size_t needed;
  char *out;
  lonejson_error error;
  lonejson_status status;
  lonejson_base64_variant variant;
  int arg = 1;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    (void)ljlua_check_runtime(L, 1);
    arg = 2;
  }
  data = luaL_checklstring(L, arg, &len);
  variant = ljlua_base64_variant_arg(L, arg + 1);
  lonejson_error_init(&error);
  status = lonejson_base64_encoded_len(len, variant, &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    return ljlua_push_status_result(L, status, &error);
  }
  out = (char *)malloc(needed == 0u ? 1u : needed);
  if (out == NULL) {
    lonejson_error_init(&error);
    error.code = LONEJSON_STATUS_ALLOCATION_FAILED;
    snprintf(error.message, sizeof(error.message),
             "failed to allocate Lua base64 output");
    return ljlua_push_status_result(L, LONEJSON_STATUS_ALLOCATION_FAILED,
                                    &error);
  }
  status =
      lonejson_base64_encode(data, len, variant, out, needed, &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    free(out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out, needed);
  free(out);
  return 1;
}

static int ljlua_base64_decode(lua_State *L) {
  const char *data;
  size_t len;
  size_t needed;
  unsigned char *out;
  lonejson_error error;
  lonejson_status status;
  lonejson_base64_variant variant;
  int arg = 1;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    (void)ljlua_check_runtime(L, 1);
    arg = 2;
  }
  data = luaL_checklstring(L, arg, &len);
  variant = ljlua_base64_variant_arg(L, arg + 1);
  lonejson_error_init(&error);
  status = lonejson_base64_decoded_len(data, len, variant, &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    return ljlua_push_status_result(L, status, &error);
  }
  out = (unsigned char *)malloc(needed == 0u ? 1u : needed);
  if (out == NULL) {
    lonejson_error_init(&error);
    error.code = LONEJSON_STATUS_ALLOCATION_FAILED;
    snprintf(error.message, sizeof(error.message),
             "failed to allocate Lua base64 output");
    return ljlua_push_status_result(L, LONEJSON_STATUS_ALLOCATION_FAILED,
                                    &error);
  }
  status =
      lonejson_base64_decode(data, len, variant, out, needed, &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    free(out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, (const char *)out, needed);
  free(out);
  return 1;
}

static const luaL_Reg ljlua_runtime_methods[] = {
    {"schema", ljlua_schema_new},
    {"array_rewrite_string", ljlua_array_rewrite_string},
    {"array_rewrite_path", ljlua_array_rewrite_path},
    {"encode_json", ljlua_encode_json},
    {"encode_json_to_sink", ljlua_encode_json_to_sink},
    {"encode_value", ljlua_encode_json},
    {"encode_value_to_sink", ljlua_encode_json_to_sink},
    {"decode_json", ljlua_decode_json},
    {"decode_value", ljlua_decode_json},
    {"base64_encode", ljlua_base64_encode},
    {"base64_decode", ljlua_base64_decode},
    {"visit_path_value_string", ljlua_visit_path_value_string},
    {"visit_path_value_path", ljlua_visit_path_value_path},
    {"visit_path_value_file", ljlua_visit_path_value_file},
    {"visit_path_value_fd", ljlua_visit_path_value_fd},
#ifdef LONEJSON_WITH_JWT
    {"jwt_parse_compact", ljlua_jwt_parse_compact},
    {"jwt_decode_compact", ljlua_jwt_decode_compact},
    {"jwt_validate_compact_claims", ljlua_jwt_validate_compact_claims},
    {"jwt_validate_compact_signature", ljlua_jwt_validate_compact_signature},
    {"jwk_parse_json", ljlua_jwk_parse_json},
    {"jwks_parse_json", ljlua_jwks_parse_json},
    {"jwks_select_json", ljlua_jwks_select_json},
#ifdef LONEJSON_WITH_OPENSSL
    {"set_openssl_auth_provider", ljlua_set_openssl_auth_provider},
#endif
#ifdef LONEJSON_WITH_OIDC
    {"set_http_provider", ljlua_set_http_provider},
    {"oauth2_client_credentials_body", ljlua_oauth2_client_credentials_body},
    {"oauth2_refresh_token_body", ljlua_oauth2_refresh_token_body},
    {"oauth2_token_introspection_body", ljlua_oauth2_token_introspection_body},
    {"oauth2_token_revocation_body", ljlua_oauth2_token_revocation_body},
    {"oidc_authorization_code_token_body",
     ljlua_oidc_authorization_code_token_body},
    {"oauth2_client_credentials_request",
     ljlua_oauth2_client_credentials_request},
    {"oauth2_refresh_token_request", ljlua_oauth2_refresh_token_request},
    {"oauth2_token_flow_ensure", ljlua_oauth2_token_flow_ensure},
    {"oauth2_introspect_token_request", ljlua_oauth2_introspect_token_request},
    {"oauth2_revoke_token_request", ljlua_oauth2_revoke_token_request},
    {"oidc_userinfo_request", ljlua_oidc_userinfo_request},
    {"oidc_authorization_code_token_request",
     ljlua_oidc_authorization_code_token_request},
    {"oauth2_token_response_parse_json",
     ljlua_oauth2_token_response_parse_json},
    {"oauth2_introspection_response_parse_json",
     ljlua_oauth2_introspection_response_parse_json},
    {"oidc_userinfo_response_parse_json",
     ljlua_oidc_userinfo_response_parse_json},
    {"oidc_validate_bearer_token", ljlua_oidc_validate_bearer_token},
    {"oidc_discovery_parse_json", ljlua_oidc_discovery_parse_json},
    {"oidc_fetch_discovery", ljlua_oidc_fetch_discovery},
    {"oidc_jwks_cache_select_json", ljlua_oidc_jwks_cache_select_json},
    {"oidc_jwks_cache_refresh", ljlua_oidc_jwks_cache_refresh},
#ifdef LONEJSON_WITH_OPENSSL
    {"m2m_credential_generate", ljlua_m2m_credential_generate},
    {"m2m_verify_authorization", ljlua_m2m_verify_authorization},
    {"m2m_signup_generate", ljlua_m2m_signup_generate},
    {"m2m_signup_complete", ljlua_m2m_signup_complete},
#endif
#endif
#endif
    {NULL, NULL}};

static const luaL_Reg ljlua_schema_methods[] = {
    {"new_record", ljlua_schema_new_record},
    {"compile_path", ljlua_schema_compile_path},
    {"compile_get", ljlua_schema_compile_get},
    {"compile_count", ljlua_schema_compile_count},
    {"decode", ljlua_schema_decode},
    {"decode_into", ljlua_schema_decode_into},
    {"decode_path", ljlua_schema_decode_path},
    {"decode_file", ljlua_schema_decode_file},
    {"decode_fd", ljlua_schema_decode_fd},
    {"assign", ljlua_schema_assign},
    {"encode", ljlua_schema_encode},
    {"write_path", ljlua_schema_write_path},
    {"write_file", ljlua_schema_write_file},
    {"write_fd", ljlua_schema_write_fd},
    {"stream_path", ljlua_schema_stream_path},
    {"stream_fd", ljlua_schema_stream_fd},
    {"stream_file", ljlua_schema_stream_file},
    {"stream_string", ljlua_schema_stream_string},
    {"array_stream_path", ljlua_schema_array_stream_path},
    {"array_stream_fd", ljlua_schema_array_stream_fd},
    {"array_stream_file", ljlua_schema_array_stream_file},
    {"array_stream_string", ljlua_schema_array_stream_string},
    {NULL, NULL}};

static const luaL_Reg ljlua_record_methods[] = {
    {"get", ljlua_record_get},
    {"to_table", ljlua_record_to_table_method},
    {"count", ljlua_record_count},
    {"clear", ljlua_record_clear},
    {NULL, NULL}};

static const luaL_Reg ljlua_stream_methods[] = {
    {"next", ljlua_stream_next}, {"close", ljlua_stream_close}, {NULL, NULL}};

static const luaL_Reg ljlua_array_stream_methods[] = {
    {"next", ljlua_array_stream_next},
    {"next_value", ljlua_array_stream_next_value},
    {"close", ljlua_array_stream_close},
    {NULL, NULL}};

static const luaL_Reg ljlua_spool_methods[] = {
    {"size", ljlua_spool_size},         {"spilled", ljlua_spool_spilled},
    {"path", ljlua_spool_path},         {"rewind", ljlua_spool_rewind},
    {"read", ljlua_spool_read},         {"read_all", ljlua_spool_read_all},
    {"write_to", ljlua_spool_write_to}, {NULL, NULL}};

static const luaL_Reg ljlua_path_methods[] = {
    {"get", ljlua_path_get}, {"count", ljlua_path_count}, {NULL, NULL}};

static int ljlua_fixed_string_scratch_gc(lua_State *L) {
  ljlua_fixed_string_scratch_ud *ud = ljlua_check_fixed_string_scratch(L, 1);
  free(ud->data);
  ud->data = NULL;
  ud->size = 0u;
  ud->magic = 0u;
  return 0;
}

static int ljlua_fixed_string_scratch_size(lua_State *L) {
  ljlua_fixed_string_scratch_ud *ud = ljlua_check_fixed_string_scratch(L, 1);
  lua_pushinteger(L, (lua_Integer)ud->size);
  return 1;
}

static int ljlua_fixed_string_scratch_new(lua_State *L) {
  lua_Integer size = luaL_checkinteger(L, 1);
  ljlua_fixed_string_scratch_ud *ud;

  luaL_argcheck(L, size > 0, 1, "scratch size must be positive");
  ud = (ljlua_fixed_string_scratch_ud *)ljlua_newuserdata_slots(L, sizeof(*ud),
                                                                0);
  memset(ud, 0, sizeof(*ud));
  ud->data = (unsigned char *)malloc((size_t)size);
  if (ud->data == NULL) {
    return luaL_error(L, "failed to allocate fixed string scratch");
  }
  ud->magic = LJLUA_FIXED_STRING_SCRATCH_MAGIC;
  ud->size = (size_t)size;
  luaL_setmetatable(L, LJLUA_FIXED_STRING_SCRATCH_MT);
  return 1;
}

static const luaL_Reg ljlua_fixed_string_scratch_methods[] = {
    {"size", ljlua_fixed_string_scratch_size}, {NULL, NULL}};

static void ljlua_register_metatable(lua_State *L, const char *name,
                                     const luaL_Reg *methods,
                                     lua_CFunction gc_fn,
                                     lua_CFunction index_fn) {
  luaL_newmetatable(L, name);
  if (gc_fn != NULL) {
    lua_pushcfunction(L, gc_fn);
    lua_setfield(L, -2, "__gc");
  }
  if (index_fn != NULL) {
    lua_pushcfunction(L, index_fn);
    lua_setfield(L, -2, "__index");
  } else {
    lua_newtable(L);
    if (methods != NULL) {
      luaL_setfuncs(L, methods, 0);
    }
    lua_setfield(L, -2, "__index");
  }
  if (methods != NULL && index_fn != NULL) {
    luaL_setfuncs(L, methods, 0);
  }
  lua_pop(L, 1);
}

int luaopen_lonejson_core(lua_State *L) {
  static const luaL_Reg funcs[] = {
      {"new", ljlua_runtime_new},
      {"encode_json", ljlua_encode_json},
      {"encode_json_to_sink", ljlua_encode_json_to_sink},
      {"decode_json", ljlua_decode_json},
      {"base64_encode", ljlua_base64_encode},
      {"base64_decode", ljlua_base64_decode},
      {"visit_path_value_string", ljlua_visit_path_value_string},
      {"visit_path_value_path", ljlua_visit_path_value_path},
      {"visit_path_value_file", ljlua_visit_path_value_file},
      {"visit_path_value_fd", ljlua_visit_path_value_fd},
#ifdef LONEJSON_WITH_JWT
      {"jwt_parse_compact", ljlua_jwt_parse_compact},
      {"jwt_decode_compact", ljlua_jwt_decode_compact},
      {"jwt_validate_compact_claims", ljlua_jwt_validate_compact_claims},
      {"jwt_validate_compact_signature", ljlua_jwt_validate_compact_signature},
      {"jwk_parse_json", ljlua_jwk_parse_json},
      {"jwks_parse_json", ljlua_jwks_parse_json},
      {"jwks_select_json", ljlua_jwks_select_json},
#ifdef LONEJSON_WITH_OIDC
      {"oauth2_client_credentials_body", ljlua_oauth2_client_credentials_body},
      {"oauth2_refresh_token_body", ljlua_oauth2_refresh_token_body},
      {"oauth2_token_introspection_body",
       ljlua_oauth2_token_introspection_body},
      {"oauth2_token_revocation_body", ljlua_oauth2_token_revocation_body},
      {"oidc_authorization_code_token_body",
       ljlua_oidc_authorization_code_token_body},
      {"oauth2_client_credentials_request",
       ljlua_oauth2_client_credentials_request},
      {"oauth2_refresh_token_request", ljlua_oauth2_refresh_token_request},
      {"oauth2_token_flow_update_response",
       ljlua_oauth2_token_flow_update_response},
      {"oauth2_token_flow_is_expired", ljlua_oauth2_token_flow_is_expired},
      {"oauth2_token_flow_ensure", ljlua_oauth2_token_flow_ensure},
      {"oauth2_introspect_token_request",
       ljlua_oauth2_introspect_token_request},
      {"oauth2_revoke_token_request", ljlua_oauth2_revoke_token_request},
      {"oidc_userinfo_request", ljlua_oidc_userinfo_request},
      {"oidc_authorization_code_token_request",
       ljlua_oidc_authorization_code_token_request},
      {"oauth2_token_response_parse_json",
       ljlua_oauth2_token_response_parse_json},
      {"oauth2_introspection_response_parse_json",
       ljlua_oauth2_introspection_response_parse_json},
      {"oidc_userinfo_response_parse_json",
       ljlua_oidc_userinfo_response_parse_json},
      {"oidc_pkce_challenge", ljlua_oidc_pkce_challenge},
      {"oidc_pkce_generate", ljlua_oidc_pkce_generate},
      {"oidc_authorization_url", ljlua_oidc_authorization_url},
      {"oidc_authorization_callback_parse_query",
       ljlua_oidc_authorization_callback_parse_query},
      {"oidc_validate_bearer_token", ljlua_oidc_validate_bearer_token},
      {"oidc_discovery_url", ljlua_oidc_discovery_url},
      {"oidc_discovery_parse_json", ljlua_oidc_discovery_parse_json},
      {"oidc_jwks_cache_select_json", ljlua_oidc_jwks_cache_select_json},
#endif
#endif
      {"fixed_string_scratch", ljlua_fixed_string_scratch_new},
      {"json_null", ljlua_json_null},
      {"monotonic_ns", ljlua_monotonic_ns},
#if defined(LONEJSON_TEST_MAP_ANALYSIS_COUNTER)
      {"_test_get_map_analysis_count", ljlua_test_get_map_analysis_count},
      {"_test_reset_map_analysis_count", ljlua_test_reset_map_analysis_count},
      {"_test_map_cache_enabled", ljlua_test_map_cache_enabled},
      {"_test_schema_map_cacheable", ljlua_test_schema_map_cacheable},
      {"_test_schema_map_cookie", ljlua_test_schema_map_cookie},
#endif
#if defined(LONEJSON_TEST_LUA_ENCODE_STATS)
      {"_test_reset_encode_stats", ljlua_test_reset_encode_stats_lua},
      {"_test_get_encode_stats", ljlua_test_get_encode_stats_lua},
      {"_test_fail_json_buf_allocation_after",
       ljlua_test_fail_json_buf_allocation_after},
      {"_test_json_buf_overflow", ljlua_test_json_buf_overflow},
#endif
      {NULL, NULL}};

  luaL_checkversion(L);
  ljlua_register_metatable(L, LJLUA_RUNTIME_MT, ljlua_runtime_methods,
                           ljlua_runtime_gc, NULL);
  ljlua_register_metatable(L, LJLUA_SCHEMA_MT, ljlua_schema_methods,
                           ljlua_schema_gc, NULL);
  ljlua_register_metatable(L, LJLUA_RECORD_MT, ljlua_record_methods,
                           ljlua_record_gc, ljlua_record_index);
  ljlua_register_metatable(L, LJLUA_STREAM_MT, ljlua_stream_methods,
                           ljlua_stream_gc, NULL);
  ljlua_register_metatable(L, LJLUA_ARRAY_STREAM_MT, ljlua_array_stream_methods,
                           ljlua_array_stream_gc, NULL);
  ljlua_register_metatable(L, LJLUA_SPOOL_MT, ljlua_spool_methods,
                           ljlua_spool_gc, NULL);
  ljlua_register_metatable(L, LJLUA_PATH_MT, ljlua_path_methods, ljlua_path_gc,
                           NULL);
  ljlua_register_metatable(L, LJLUA_FIXED_STRING_SCRATCH_MT,
                           ljlua_fixed_string_scratch_methods,
                           ljlua_fixed_string_scratch_gc, NULL);
  luaL_getmetatable(L, LJLUA_PATH_MT);
  lua_pushcfunction(L, ljlua_path_get);
  lua_setfield(L, -2, "__call");
  lua_pop(L, 1);
  luaL_newlib(L, funcs);
  return 1;
}

static int ljlua_monotonic_ns(lua_State *L) {
  struct timespec ts;
  lua_Integer value;

  (void)L;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return luaL_error(L, "clock_gettime failed: %s", strerror(errno));
  }
  value = (lua_Integer)ts.tv_sec * (lua_Integer)1000000000 +
          (lua_Integer)ts.tv_nsec;
  lua_pushinteger(L, value);
  return 1;
}

static int ljlua_json_null(lua_State *L) {
  ljlua_push_json_null(L);
  return 1;
}

typedef struct ljlua_pretty_encode_call {
  ljlua_json_out *out;
  int compact_mode;
  ljlua_json_buf *result;
} ljlua_pretty_encode_call;

static int ljlua_pretty_encode_pcall(lua_State *L) {
  ljlua_pretty_encode_call *call =
      (ljlua_pretty_encode_call *)lua_touserdata(L, 1);
  ljlua_encode_json_walk(L, 2, call->out, !call->compact_mode);
  if (call->result != NULL) {
    lua_pushlstring(L, call->result->data, call->result->len);
    return 1;
  }
  return 0;
}

static void
ljlua_init_json_value_decode_state(ljlua_json_value_decode_state *state,
                                   lua_State *L) {
  memset(state, 0, sizeof(*state));
  ljlua_json_builder_init(&state->builder, L);
  state->visitor = lonejson_default_value_visitor();
  state->visitor.object_begin = ljlua_json_value_object_begin;
  state->visitor.object_end = ljlua_json_value_object_end;
  state->visitor.array_begin = ljlua_json_value_array_begin;
  state->visitor.array_end = ljlua_json_value_array_end;
  state->visitor.object_key_begin = ljlua_json_value_key_begin;
  state->visitor.object_key_chunk = ljlua_json_value_key_chunk;
  state->visitor.object_key_end = ljlua_json_value_key_end;
  state->visitor.string_begin = ljlua_json_value_string_begin;
  state->visitor.string_chunk = ljlua_json_value_string_chunk;
  state->visitor.string_end = ljlua_json_value_string_end;
  state->visitor.number_begin = ljlua_json_value_number_begin;
  state->visitor.number_chunk = ljlua_json_value_number_chunk;
  state->visitor.number_end = ljlua_json_value_number_end;
  state->visitor.boolean_value = ljlua_json_value_boolean;
  state->visitor.null_value = ljlua_json_value_null;
}

static int ljlua_json_out_write_indent(lua_State *L, ljlua_json_out *out,
                                       size_t depth) {
  size_t i;

  if (!ljlua_json_out_write(L, out, "\n", 1u)) {
    return 0;
  }
  for (i = 0u; i < depth; ++i) {
    if (!ljlua_json_out_write(L, out, "  ", 2u)) {
      return 0;
    }
  }
  return 1;
}

static int ljlua_runtime_encode_json_impl(lua_State *L,
                                          ljlua_runtime_ud *runtime_ud,
                                          int value_index) {
  ljlua_json_buf buf;
  ljlua_json_out out;
  ljlua_pretty_encode_call call;
  size_t max_output_bytes = 0u;
  int status;

  value_index = lua_absindex(L, value_index);
  memset(&buf, 0, sizeof(buf));
  if (runtime_ud != NULL) {
    max_output_bytes = runtime_ud->write_max_output_bytes;
  }
  if (max_output_bytes != 0u && max_output_bytes != SIZE_MAX) {
    buf.max_cap = max_output_bytes + 1u;
  }
  out.sink = ljlua_json_buf_sink_limited;
  out.user = &buf;
  if (lua_type(L, value_index) != LUA_TTABLE) {
    ljlua_scalar_output output;

    /* Scalars own only the output buffer. Release it at the failing sink
     * without introducing a protected Lua call on this hot path. */
    output.L = L;
    output.buf = &buf;
    out.sink = ljlua_scalar_output_sink;
    out.user = &output;
    ljlua_encode_json_value(L, value_index, &out);
    if (buf.len <= 512u) {
      char result[512];
      size_t len = buf.len;

      memcpy(result, buf.data, len);
      free(buf.data);
      lua_pushlstring(L, result, len);
      return 1;
    }
    lua_pushcfunction(L, ljlua_push_json_buf_pcall);
    lua_pushlightuserdata(L, &buf);
    status = lua_pcall(L, 1, 1, 0);
    free(buf.data);
    if (status != LUA_OK) {
      return lua_error(L);
    }
    return 1;
  }
  memset(&call, 0, sizeof(call));
  call.out = &out;
  call.compact_mode = runtime_ud == NULL || !runtime_ud->write_pretty;
  call.result = &buf;
  lua_pushcfunction(L, ljlua_pretty_encode_pcall);
  lua_pushlightuserdata(L, &call);
  lua_pushvalue(L, value_index);
  status = lua_pcall(L, 2, 1, 0);
  free(buf.data);
  if (status != LUA_OK) {
    return lua_error(L);
  }
  return 1;
}

typedef struct ljlua_json_lua_sink {
  lua_State *L;
  int callback_ref;
} ljlua_json_lua_sink;

static lonejson_status ljlua_json_lua_sink_write(void *user, const void *data,
                                                 size_t len,
                                                 lonejson_error *error) {
  ljlua_json_lua_sink *sink = (ljlua_json_lua_sink *)user;

  lua_rawgeti(sink->L, LUA_REGISTRYINDEX, sink->callback_ref);
  lua_pushlstring(sink->L, (const char *)data, len);
  if (lua_pcall(sink->L, 1, 0, 0) != LUA_OK) {
    const char *msg = lua_tostring(sink->L, -1);
    lonejson_status status =
        ljlua_set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, "%s",
                        msg != NULL ? msg : "Lua sink failed");
    lua_pop(sink->L, 1);
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static int ljlua_runtime_encode_json_to_sink_impl(lua_State *L,
                                                  ljlua_runtime_ud *runtime_ud,
                                                  int value_index,
                                                  int sink_index) {
  ljlua_json_lua_sink sink;
  ljlua_json_out out;

  value_index = lua_absindex(L, value_index);
  sink_index = lua_absindex(L, sink_index);
  sink.L = L;
  lua_pushvalue(L, sink_index);
  sink.callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  if (runtime_ud != NULL && runtime_ud->write_pretty) {
    ljlua_pretty_encode_call call;
    int status;

    out.sink = ljlua_json_lua_sink_write;
    out.user = &sink;
    memset(&call, 0, sizeof(call));
    call.out = &out;
    lua_pushcfunction(L, ljlua_pretty_encode_pcall);
    lua_pushlightuserdata(L, &call);
    lua_pushvalue(L, value_index);
    status = lua_pcall(L, 2, 0, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, sink.callback_ref);
    if (status != LUA_OK) {
      return lua_error(L);
    }
    return 0;
  }
  out.sink = ljlua_json_lua_sink_write;
  out.user = &sink;
  {
    ljlua_pretty_encode_call call;
    int status;

    memset(&call, 0, sizeof(call));
    call.out = &out;
    call.compact_mode = 1;
    lua_pushcfunction(L, ljlua_pretty_encode_pcall);
    lua_pushlightuserdata(L, &call);
    lua_pushvalue(L, value_index);
    status = lua_pcall(L, 2, 0, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, sink.callback_ref);
    if (status != LUA_OK) {
      return lua_error(L);
    }
  }
  return 0;
}

static int ljlua_runtime_decode_json_impl(lua_State *L, lonejson *runtime,
                                          const char *json, size_t len) {
  ljlua_json_value_decode_state state;
  lonejson_error error;
  lonejson_status status;
  int root_ref;
  const char *message;

  ljlua_init_json_value_decode_state(&state, L);
  memset(&error, 0, sizeof(error));
  status = lonejson_visit_value_buffer(runtime, json, len, &state.visitor,
                                       &state, &error);
  if (status != LONEJSON_STATUS_OK) {
    message = error.message[0] != '\0' ? error.message
                                       : lonejson_status_string(status);
    ljlua_json_builder_cleanup(L, &state.builder);
    return luaL_error(L, "%s", message);
  }
  root_ref = state.builder.root_ref;
  if (root_ref == LUA_REFNIL) {
    ljlua_json_builder_cleanup(L, &state.builder);
    ljlua_push_json_null(L);
    return 1;
  }
  if (root_ref == LUA_NOREF || !state.complete) {
    ljlua_json_builder_cleanup(L, &state.builder);
    return luaL_error(L, "incomplete JSON value");
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, root_ref);
  ljlua_json_builder_cleanup(L, &state.builder);
  return 1;
}

static int ljlua_encode_json(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    luaL_checkany(L, 2);
    return ljlua_runtime_encode_json_impl(L, runtime_ud, 2);
  }
  luaL_checkany(L, 1);
  return ljlua_runtime_encode_json_impl(L, NULL, 1);
}

static int ljlua_encode_json_to_sink(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;

  if (lua_gettop(L) >= 2 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    luaL_checkany(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    return ljlua_runtime_encode_json_to_sink_impl(L, runtime_ud, 2, 3);
  }
  luaL_checkany(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  return ljlua_runtime_encode_json_to_sink_impl(L, NULL, 1, 2);
}

static int ljlua_decode_json(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  const char *json;
  size_t len;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    json = luaL_checklstring(L, 2, &len);
    return ljlua_runtime_decode_json_impl(L, runtime_ud->runtime, json, len);
  }
  json = luaL_checklstring(L, 1, &len);
  ljlua_push_json_from_text(L, json, len);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    ljlua_push_json_null(L);
  }
  return 1;
}
