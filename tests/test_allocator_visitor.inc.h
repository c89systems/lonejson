static void test_value_visitor_success_and_limits(void) {
  const char *json = "{\"name\":\"a\\n\\u20ac\",\"items\":[1,-2.5e3,true,false,"
                     "null,{\"x\":[]}]}";
  const char *pretty_object = "{\n"
                              "  \"kind\": \"term\",\n"
                              "  \"field\": \"name\",\n"
                              "  \"value\": \"alice\"\n"
                              "}";
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson__value_limits limits = lonejson__default_value_limits();
  lonejson_error error;
  lonejson_status status;
  test_visit_state state;
  test_reader_state reader;
  char path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-value-visitor-XXXXXX";
  int fd;
  FILE *fp;

  memset(&state, 0, sizeof(state));
  status = test_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "{K(name)S(a\n") != NULL);
  EXPECT(strstr(state.log, "K(items)[N(1)N(-2.5e3)TFZ{K(x)[]}]") != NULL);

  memset(&state, 0, sizeof(state));
  status = test_visit_value_cstr(pretty_object, &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(state.log, "{K(kind)S(term)K(field)S(name)K(value)S(alice)}") ==
         0);

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = test_visit_value_reader(test_state_reader, &reader, &visitor, &state,
                                   &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(
      strcmp(state.log,
             "{K(name)S(a\n\xE2\x82\xAC)K(items)[N(1)N(-2.5e3)TFZ{K(x)[]}]}") ==
      0);

  strcpy(path,
         LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    memset(&state, 0, sizeof(state));
    status = test_visit_value_path(path, &visitor, &state, &limits, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    unlink(path);
  }

  limits.max_depth = 2u;
  status = test_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson__default_value_limits();
  limits.max_key_bytes = 3u;
  status = test_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson__default_value_limits();
  limits.max_string_bytes = 2u;
  status = test_visit_value_cstr("{\"k\":\"abcd\"}", &visitor, &state, &limits,
                                 &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson__default_value_limits();
  limits.max_number_bytes = 3u;
  status = test_visit_value_cstr("12345", &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson__default_value_limits();
  limits.max_total_bytes = 5u;
  status =
      test_visit_value_cstr("{\"k\":1}", &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  status = test_visit_value_cstr("{\"k\":1} trailing", &visitor, &state, NULL,
                                 &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  status = test_visit_value_cstr("{\"k\":tru}", &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  strcpy(path,
         LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    fp = fdopen(fd, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
      memset(&state, 0, sizeof(state));
      status = test_visit_value_filep(fp, &visitor, &state, NULL, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      fclose(fp);
    } else {
      close(fd);
    }
    unlink(path);
  }

  strcpy(path,
         LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    EXPECT(lseek(fd, 0, SEEK_SET) == 0);
    memset(&state, 0, sizeof(state));
    status = test_visit_value_fd(fd, &visitor, &state, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    close(fd);
    unlink(path);
  }
}

static void test_value_visitor_chunking_unicode_and_failures(void) {
  const char *json =
      "{\"\\u20acuro_key\":\"line\\n\\uD834\\uDD1E\","
      "\"items\":[\"alpha\",\"beta\",-12345.678e-9,true,false,null]}";
  static const char *invalid_cases[] = {
      "{\"bad\":\"\\uD834x\"}", "{\"bad\":\"\\uD834\\u0000\"}",
      "{\"bad\":\"\\uDD1E\"}",  "{\"bad\":\"\\u12\"}",
      "{\"bad\":\"\\x41\"}",    "{\"bad\":[1,2,}",
      "{\"bad\":1} trailing"};
  lonejson_value_visitor visitor = test_counting_value_visitor();
  lonejson_error error;
  lonejson_status status;
  test_visit_chunk_state state;
  test_reader_state reader;
  lonejson__value_limits limits;
  size_t i;

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = test_visit_value_reader(test_state_reader, &reader, &visitor, &state,
                                   NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.object_count == 1u);
  EXPECT(state.array_count == 1u);
  EXPECT(state.key_begin_count == 2u);
  EXPECT(state.key_chunk_count > state.key_begin_count);
  EXPECT(state.string_begin_count == 3u);
  EXPECT(state.string_chunk_count > state.string_begin_count);
  EXPECT(state.number_begin_count == 1u);
  EXPECT(state.number_chunk_count >= 1u);
  EXPECT(state.bool_count == 2u);
  EXPECT(state.null_count == 1u);
  EXPECT(strcmp(state.last_number, "-12345.678e-9") == 0);

  memset(&state, 0, sizeof(state));
  limits = lonejson__default_value_limits();
  limits.max_total_bytes = strlen(json);
  status = test_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  memset(&state, 0, sizeof(state));
  state.fail_after = 6u;
  status = test_visit_value_cstr(json, &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.callback_count == 6u);
  EXPECT(state.key_begin_count > 0u);

  status = test_visit_value_cstr(json, NULL, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&state, 0, sizeof(state));
  status = test_visit_value_cstr(json, &visitor, &state, NULL, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);

  for (i = 0u; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    memset(&state, 0, sizeof(state));
    status =
        test_visit_value_cstr(invalid_cases[i], &visitor, &state, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  }
}

typedef struct test_path_visit_state {
  char log[4096];
  size_t len;
  size_t callback_count;
  size_t fail_after;
  char active_string_path[256];
  char active_number_path[256];
  size_t string_chunk_count;
  size_t number_chunk_count;
  size_t mismatched_chunk_paths;
} test_path_visit_state;

static lonejson_status test_path_visit_append(test_path_visit_state *state,
                                              const char *data, size_t len) {
  size_t copy_len;

  if (state->len >= sizeof(state->log) - 1u) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  copy_len = len;
  if (copy_len > sizeof(state->log) - state->len - 1u) {
    copy_len = sizeof(state->log) - state->len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->log + state->len, data, copy_len);
    state->len += copy_len;
    state->log[state->len] = '\0';
  }
  return copy_len == len ? LONEJSON_STATUS_OK : LONEJSON_STATUS_TRUNCATED;
}

static lonejson_status test_path_visit_append_cstr(test_path_visit_state *state,
                                                   const char *text) {
  return test_path_visit_append(state, text, strlen(text));
}

static lonejson_status
test_path_visit_format_path(char *out, size_t out_size,
                            const lonejson_value_path *path) {
  size_t len = 0u;
  size_t i;

  if (out_size == 0u) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  out[0] = '\0';
  if (path == NULL || path->segment_count == 0u) {
    if (out_size < 2u) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    strcpy(out, "$");
    return LONEJSON_STATUS_OK;
  }
  out[len++] = '$';
  out[len] = '\0';
  for (i = 0u; i < path->segment_count; ++i) {
    int written;
    if (len >= out_size - 1u) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    out[len++] = '/';
    out[len] = '\0';
    written = snprintf(out + len, out_size - len,
                       "%lu:", (unsigned long)path->segments[i].len);
    if (written < 0 || (size_t)written >= out_size - len) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    len += (size_t)written;
    if (path->segments[i].len >= out_size - len) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    memcpy(out + len, path->segments[i].data, path->segments[i].len);
    len += path->segments[i].len;
    out[len] = '\0';
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_path_visit_maybe_fail(test_path_visit_state *state,
                                                  lonejson_error *error) {
  ++state->callback_count;
  if (state->fail_after != 0u && state->callback_count >= state->fail_after) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "intentional path visitor failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_path_visit_event_log(test_path_visit_state *state, const char *name,
                          const lonejson_value_path *path,
                          lonejson_error *error) {
  char path_text[256];
  lonejson_status status;

  status = test_path_visit_maybe_fail(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = test_path_visit_format_path(path_text, sizeof(path_text), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = test_path_visit_append_cstr(state, name);
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append_cstr(state, "(");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append_cstr(state, path_text);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append_cstr(state, ")");
  }
  return status;
}

static lonejson_status
test_path_visit_object_begin(void *user, const lonejson_value_path *path,
                             lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "{", path,
                                   error);
}

static lonejson_status
test_path_visit_object_end(void *user, const lonejson_value_path *path,
                           lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "}", path,
                                   error);
}

static lonejson_status
test_path_visit_array_begin(void *user, const lonejson_value_path *path,
                            lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "[", path,
                                   error);
}

static lonejson_status
test_path_visit_array_end(void *user, const lonejson_value_path *path,
                          lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "]", path,
                                   error);
}

static lonejson_status
test_path_visit_key_begin(void *user, const lonejson_value_path *path,
                          lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "K<", path,
                                   error);
}

static lonejson_status
test_path_visit_key_chunk(void *user, const lonejson_value_path *path,
                          const char *data, size_t len, lonejson_error *error) {
  test_path_visit_state *state = (test_path_visit_state *)user;
  lonejson_status status = test_path_visit_event_log(state, "K", path, error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append_cstr(state, "=");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append(state, data, len);
  }
  return status;
}

static lonejson_status test_path_visit_key_end(void *user,
                                               const lonejson_value_path *path,
                                               lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "K>", path,
                                   error);
}

static lonejson_status
test_path_visit_string_begin(void *user, const lonejson_value_path *path,
                             lonejson_error *error) {
  test_path_visit_state *state = (test_path_visit_state *)user;
  lonejson_status status = test_path_visit_format_path(
      state->active_string_path, sizeof(state->active_string_path), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return test_path_visit_event_log(state, "S<", path, error);
}

static lonejson_status
test_path_visit_string_chunk(void *user, const lonejson_value_path *path,
                             const char *data, size_t len,
                             lonejson_error *error) {
  test_path_visit_state *state = (test_path_visit_state *)user;
  char path_text[256];
  lonejson_status status =
      test_path_visit_format_path(path_text, sizeof(path_text), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strcmp(path_text, state->active_string_path) != 0) {
    ++state->mismatched_chunk_paths;
  }
  ++state->string_chunk_count;
  status = test_path_visit_event_log(state, "S", path, error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append_cstr(state, "=");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append(state, data, len);
  }
  return status;
}

static lonejson_status
test_path_visit_string_end(void *user, const lonejson_value_path *path,
                           lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "S>", path,
                                   error);
}

static lonejson_status
test_path_visit_number_begin(void *user, const lonejson_value_path *path,
                             lonejson_error *error) {
  test_path_visit_state *state = (test_path_visit_state *)user;
  lonejson_status status = test_path_visit_format_path(
      state->active_number_path, sizeof(state->active_number_path), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return test_path_visit_event_log(state, "N<", path, error);
}

static lonejson_status
test_path_visit_number_chunk(void *user, const lonejson_value_path *path,
                             const char *data, size_t len,
                             lonejson_error *error) {
  test_path_visit_state *state = (test_path_visit_state *)user;
  char path_text[256];
  lonejson_status status =
      test_path_visit_format_path(path_text, sizeof(path_text), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strcmp(path_text, state->active_number_path) != 0) {
    ++state->mismatched_chunk_paths;
  }
  ++state->number_chunk_count;
  status = test_path_visit_event_log(state, "N", path, error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append_cstr(state, "=");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_path_visit_append(state, data, len);
  }
  return status;
}

static lonejson_status
test_path_visit_number_end(void *user, const lonejson_value_path *path,
                           lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "N>", path,
                                   error);
}

static lonejson_status test_path_visit_bool(void *user,
                                            const lonejson_value_path *path,
                                            int value, lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user,
                                   value ? "T" : "F", path, error);
}

static lonejson_status test_path_visit_null(void *user,
                                            const lonejson_value_path *path,
                                            lonejson_error *error) {
  return test_path_visit_event_log((test_path_visit_state *)user, "Z", path,
                                   error);
}

static lonejson_path_value_visitor test_path_value_visitor(void) {
  lonejson_path_value_visitor visitor = lonejson_default_path_value_visitor();
  visitor.object_begin = test_path_visit_object_begin;
  visitor.object_end = test_path_visit_object_end;
  visitor.object_key_begin = test_path_visit_key_begin;
  visitor.object_key_chunk = test_path_visit_key_chunk;
  visitor.object_key_end = test_path_visit_key_end;
  visitor.array_begin = test_path_visit_array_begin;
  visitor.array_end = test_path_visit_array_end;
  visitor.string_begin = test_path_visit_string_begin;
  visitor.string_chunk = test_path_visit_string_chunk;
  visitor.string_end = test_path_visit_string_end;
  visitor.number_begin = test_path_visit_number_begin;
  visitor.number_chunk = test_path_visit_number_chunk;
  visitor.number_end = test_path_visit_number_end;
  visitor.boolean_value = test_path_visit_bool;
  visitor.null_value = test_path_visit_null;
  return visitor;
}

static void test_path_index_formatter_uses_full_size_t(void) {
  char text[32];
  size_t n;

  memset(text, 0xA5, sizeof(text));
  n = lonejson__format_size_decimal(text, sizeof(text), 0u);
  EXPECT(n == 1u);
  EXPECT(strcmp(text, "0") == 0);

  n = lonejson__format_size_decimal(text, sizeof(text), 1234567890u);
  EXPECT(n == 10u);
  EXPECT(strcmp(text, "1234567890") == 0);

  n = lonejson__format_size_decimal(text, 4u, 12345u);
  EXPECT(n == 5u);

  n = lonejson__format_size_decimal(text, sizeof(text), (size_t)-1);
  EXPECT(n > 0u);
  EXPECT(n < sizeof(text));
  EXPECT(text[n] == '\0');
  EXPECT(text[0] >= '1' && text[0] <= '9');
}

static void test_path_value_visitor_paths_and_chunks(void) {
  const char *json =
      "{\"a/b\":{\"~key\":\"v\"},\"items\":[10,{\"sku\":\"ABC\"},false,null]}";
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_path_visit_state state;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_path_value_cstr(test_default_runtime(), "true",
                                          &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(state.log, "T($)") == 0);

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_path_value_cstr(test_default_runtime(), json,
                                          &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "K($)=a/b") != NULL);
  EXPECT(strstr(state.log, "{($/3:a/b)") != NULL);
  EXPECT(strstr(state.log, "K($/3:a/b)=~key") != NULL);
  EXPECT(strstr(state.log, "S<($/3:a/b/4:~key)") != NULL);
  EXPECT(strstr(state.log, "S($/3:a/b/4:~key)=v") != NULL);
  EXPECT(strstr(state.log, "[($/5:items)") != NULL);
  EXPECT(strstr(state.log, "N($/5:items/1:0)=10") != NULL);
  EXPECT(strstr(state.log, "{($/5:items/1:1)") != NULL);
  EXPECT(strstr(state.log, "S($/5:items/1:1/3:sku)=ABC") != NULL);
  EXPECT(strstr(state.log, "F($/5:items/1:2)") != NULL);
  EXPECT(strstr(state.log, "Z($/5:items/1:3)") != NULL);
  EXPECT(state.string_chunk_count >= 2u);
  EXPECT(state.number_chunk_count == 1u);
  EXPECT(state.mismatched_chunk_paths == 0u);
}

static void test_path_value_visitor_reader_chunk_paths(void) {
  const char *json = "{\"s\":\"alpha\\nbeta\\tgamma\",\"n\":123456789}";
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_path_visit_state state;
  test_reader_state reader;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.chunk_size = 3u;
  status = lonejson_visit_path_value_reader(test_default_runtime(),
                                            test_state_reader, &reader,
                                            &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.string_chunk_count > 1u);
  EXPECT(state.number_chunk_count >= 1u);
  EXPECT(state.mismatched_chunk_paths == 0u);
}

static void test_path_value_visitor_failure_cleans_state(void) {
  const char *json =
      "{\"long-key-name-that-spills-inline-storage\":{\"child\":[1,2,3]}}";
  test_allocator_state alloc;
  lonejson_config config;
  lonejson *runtime;
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_path_visit_state state;
  lonejson_error error;
  lonejson_status status;
  size_t base_live;

  test_allocator_init(&alloc);
  config = lonejson_default_config();
  config.allocator = &alloc.allocator;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  base_live = alloc.stats.bytes_live;
  memset(&state, 0, sizeof(state));
  state.fail_after = 7u;
  status =
      lonejson_visit_path_value_cstr(runtime, json, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(alloc.stats.bytes_live == base_live);
  lonejson_free(runtime);
}

static void test_path_value_visitor_short_keys_do_not_allocate_per_event(void) {
  const char *json = "{\"a\":true,\"b\":false,\"c\":[null,{\"d\":\"e\"}]}";
  test_allocator_state alloc;
  lonejson_config config;
  lonejson *runtime;
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_path_visit_state state;
  lonejson_error error;
  lonejson_status status;
  size_t base_alloc_calls;
  size_t base_live;

  test_allocator_init(&alloc);
  config = lonejson_default_config();
  config.allocator = &alloc.allocator;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  base_alloc_calls = alloc.stats.alloc_calls;
  base_live = alloc.stats.bytes_live;
  memset(&state, 0, sizeof(state));
  status =
      lonejson_visit_path_value_cstr(runtime, json, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls == base_alloc_calls + 2u);
  EXPECT(state.callback_count > 2u);
  EXPECT(alloc.stats.bytes_live == base_live);
  lonejson_free(runtime);
}

static void test_visit_value_reader_rejects_would_block(void) {
  static const char json[] = "{\"name\":\"visit\",\"items\":[1,2],\"ok\":true}";
  lonejson_value_visitor visitor = test_value_visitor();
  test_visit_state state;
  test_would_block_tripwire_read_state reader;
  lonejson_error error;
  lonejson_status status;
  lonejson *runtime = test_default_runtime();

  memset(&state, 0, sizeof(state));
  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.chunk_size = 12u;
  reader.block_after = 12u;
  status =
      lonejson_visit_value_reader(runtime, test_would_block_tripwire_reader,
                                  &reader, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(reader.blocked);
  EXPECT(!reader.reentered_after_block);
  EXPECT(reader.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);

  memset(&state, 0, sizeof(state));
  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.chunk_size = 12u;
  reader.block_after = 12u;
  status =
      runtime->visit_value_reader(runtime, test_would_block_tripwire_reader,
                                  &reader, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(reader.blocked);
  EXPECT(!reader.reentered_after_block);
  EXPECT(reader.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);
}

static void test_visit_value_success_clears_error(void) {
  static const char json[] = "{\"name\":\"visit\",\"items\":[1,2],\"ok\":true}";
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson_error error;
  lonejson_status status;
  test_visit_state state;
  test_reader_state reader;
  lonejson *runtime = test_default_runtime();
  char path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-visit-clear-XXXXXX";
  int fd;
  FILE *fp;

  memset(&state, 0, sizeof(state));
  error.code = LONEJSON_STATUS_INVALID_JSON;
  error.system_errno = EIO;
  snprintf(error.message, sizeof(error.message), "stale");
  status = lonejson_visit_value_buffer(runtime, json, strlen(json), &visitor,
                                       &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(error.system_errno == 0);
  EXPECT(error.message[0] == '\0');

  memset(&state, 0, sizeof(state));
  error.code = LONEJSON_STATUS_INVALID_JSON;
  error.system_errno = EIO;
  snprintf(error.message, sizeof(error.message), "stale");
  status = runtime->visit_value_cstr(runtime, json, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(error.system_errno == 0);
  EXPECT(error.message[0] == '\0');

  memset(&state, 0, sizeof(state));
  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.chunk_size = 5u;
  error.code = LONEJSON_STATUS_INVALID_JSON;
  error.system_errno = EIO;
  snprintf(error.message, sizeof(error.message), "stale");
  status = lonejson_visit_value_reader(runtime, test_state_reader, &reader,
                                       &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(error.system_errno == 0);
  EXPECT(error.message[0] == '\0');

  memset(&state, 0, sizeof(state));
  fp = tmpfile();
  EXPECT(fp != NULL);
  if (fp != NULL) {
    fwrite(json, 1u, strlen(json), fp);
    rewind(fp);
    error.code = LONEJSON_STATUS_INVALID_JSON;
    error.system_errno = EIO;
    snprintf(error.message, sizeof(error.message), "stale");
    status = lonejson_visit_value_filep(runtime, fp, &visitor, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(error.code == LONEJSON_STATUS_OK);
    EXPECT(error.system_errno == 0);
    EXPECT(error.message[0] == '\0');
    fclose(fp);
  }

  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    memset(&state, 0, sizeof(state));
    error.code = LONEJSON_STATUS_INVALID_JSON;
    error.system_errno = EIO;
    snprintf(error.message, sizeof(error.message), "stale");
    status = lonejson_visit_value_path(runtime, path, &visitor, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(error.code == LONEJSON_STATUS_OK);
    EXPECT(error.system_errno == 0);
    EXPECT(error.message[0] == '\0');

    fd = open(path, O_RDONLY);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      memset(&state, 0, sizeof(state));
      error.code = LONEJSON_STATUS_INVALID_JSON;
      error.system_errno = EIO;
      snprintf(error.message, sizeof(error.message), "stale");
      status = runtime->visit_value_fd(runtime, fd, &visitor, &state, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(error.system_errno == 0);
      EXPECT(error.message[0] == '\0');
      close(fd);
    }

    unlink(path);
  }
}

static void test_custom_allocator_parse_cleanup_and_stream(void) {
  static const char json[] =
      "{\"name\":\"alpha\",\"body\":\"hello world\",\"tags\":[\"x\",\"y\"]}";
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  lonejson_stream *stream;
  test_reader_state reader;

  test_allocator_init(&alloc);
  memset(&doc, 0, sizeof(doc));
  options.allocator = &alloc.allocator;
  status =
      test_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
  EXPECT(alloc.stats.free_calls > 0u);

  test_allocator_init(&alloc);
  options = lonejson__default_parse_options();
  options.allocator = &alloc.allocator;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 7u;
  stream = test_stream_open_reader(&test_alloc_parse_doc_map, test_state_reader,
                                   &reader, &options, &error);
  EXPECT(stream != NULL);
  EXPECT(alloc.stats.alloc_calls > 0u);
  if (stream != NULL) {
    memset(&doc, 0, sizeof(doc));
    EXPECT(lonejson_stream_next(stream, &doc, &error) ==
           LONEJSON_STREAM_OBJECT);
    lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
    lonejson_stream_close(stream);
  }
  EXPECT(alloc.stats.bytes_live == 0u);
}

#ifndef NDEBUG
static void test_custom_allocator_misaligned_owned_alloc_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\"}";
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;

  memset(&doc, 0, sizeof(doc));
  allocator.malloc_fn = test_misaligned_allocator_malloc;
  allocator.realloc_fn = test_misaligned_allocator_realloc;
  allocator.free_fn = test_misaligned_allocator_free;
  options.allocator = &allocator;
  status =
      test_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_ALLOCATION_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_ALLOCATION_FAILED);
  EXPECT(strstr(error.message, "failed to allocate string") != NULL);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
}
#endif

static void test_custom_allocator_json_value_capture_and_serialize_alloc(void) {
  static const char json[] =
      "{\"id\":\"q1\",\"selector\":{\"a\":[1,true,null]}}";
  test_allocator_state parse_alloc;
  test_allocator_state write_alloc;
  lonejson__parse_options parse_options = lonejson__default_parse_options();
  lonejson__write_options write_options = lonejson__default_write_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;
  test_event event;
  lonejson_owned_buffer serialized;

  test_allocator_init(&parse_alloc);
  test_allocator_init(&write_alloc);
  lonejson_json_value_init_with_allocator(&doc.selector,
                                          &parse_alloc.allocator);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(doc.id, 0, sizeof(doc.id));
  parse_options.clear_destination = 0;
  parse_options.allocator = &parse_alloc.allocator;
  status = test_parse_cstr(&test_alloc_json_value_doc_map, &doc, json,
                           &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.alloc_calls > 0u);
  EXPECT(parse_alloc.stats.bytes_live > 0u);
  EXPECT(strcmp(doc.selector.json, "{\"a\":[1,true,null]}") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &doc, NULL);
  EXPECT(parse_alloc.stats.bytes_live == 0u);

  write_options.allocator = &write_alloc.allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-1", 6u);
  event.ok = true;
  lonejson_owned_buffer_init(&serialized);
  status = test_serialize_owned(&test_event_map, &event, &serialized,
                                &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(serialized.data != NULL);
  EXPECT(write_alloc.stats.alloc_calls > 0u);
  EXPECT(write_alloc.stats.bytes_live > 0u);
  if (serialized.data != NULL) {
    EXPECT(strcmp(serialized.data, "{\"id\":\"evt-1\",\"ok\":true}") == 0);
    lonejson_owned_buffer_free(&serialized);
  }
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void
test_custom_allocator_json_value_default_clear_reparse_releases_capture(void) {
  static const char json[] =
      "{\"id\":\"q-clear\",\"selector\":{\"a\":[1,true,null]}}";
  test_allocator_state parse_alloc;
  lonejson__parse_options parse_options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;
  lonejson_field fields[2];
  lonejson_map default_capture_map;
  size_t bytes_after_first_parse;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&parse_alloc);
  memcpy(fields, test_alloc_json_value_doc_fields, sizeof(fields));
  fields[1].flags |= LONEJSON_FIELD_JSON_VALUE_DEFAULT_CAPTURE;
  default_capture_map = test_alloc_json_value_doc_map;
  default_capture_map.fields = fields;
  default_capture_map._map_identity = NULL;
  default_capture_map._map_cookie = 0u;
  parse_options.allocator = &parse_alloc.allocator;

  status =
      test_parse_cstr(&default_capture_map, &doc, json, &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(doc.selector.json != NULL);
  bytes_after_first_parse = parse_alloc.stats.bytes_live;
  EXPECT(bytes_after_first_parse > 0u);

  status =
      test_parse_cstr(&default_capture_map, &doc, json, &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(doc.selector.json != NULL);
  EXPECT(parse_alloc.stats.bytes_live == bytes_after_first_parse);

  lonejson_cleanup(&default_capture_map, &doc, NULL);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static void test_custom_allocator_raw_serialize_alloc_is_rejected(void) {
  test_allocator_state write_alloc;
  lonejson__write_options write_options = lonejson__default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  test_allocator_init(&write_alloc);
  write_options.allocator = &write_alloc.allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-raw", 8u);
  event.ok = true;
  serialized = test_serialize_alloc(&test_event_map, &event, NULL,
                                    &write_options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "serialize_alloc uses the default allocator only") != NULL);
  EXPECT(write_alloc.stats.alloc_calls == 0u);
  EXPECT(write_alloc.stats.realloc_calls == 0u);
  EXPECT(write_alloc.stats.free_calls == 0u);
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void
test_explicit_default_allocator_raw_serialize_alloc_is_allowed(void) {
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  test_event event;
  char *serialized;

  options.allocator = &allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-def", 8u);
  event.ok = true;
  serialized =
      test_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"evt-def\",\"ok\":true}") == 0);
    LONEJSON_FREE(serialized);
  }
}

static void
test_explicit_default_allocator_raw_serialize_jsonl_alloc_is_allowed(void) {
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  test_event events[2];
  char *serialized;

  options.allocator = &allocator;
  memset(events, 0, sizeof(events));
  memcpy(events[0].id, "evt-1", 6u);
  events[0].ok = true;
  memcpy(events[1].id, "evt-2", 6u);
  events[1].ok = false;
  serialized = test_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                          &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"evt-1\",\"ok\":true}\n"
                              "{\"id\":\"evt-2\",\"ok\":false}\n") == 0);
    LONEJSON_FREE(serialized);
  }
}

static void
test_custom_allocator_json_value_capture_preserves_handle_allocator(void) {
  static const char json[] = "{\"id\":\"q2\",\"selector\":{\"k\":[1,2,3]}}";
  test_allocator_state parse_alloc;
  lonejson__parse_options parse_options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&parse_alloc);
  lonejson_json_value_init_with_allocator(&doc.selector,
                                          &parse_alloc.allocator);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  parse_options.clear_destination = 0;
  parse_options.allocator = NULL;
  status = test_parse_cstr(&test_alloc_json_value_doc_map, &doc, json,
                           &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.alloc_calls > 0u);
  EXPECT(parse_alloc.stats.bytes_live > 0u);
  EXPECT(strcmp(doc.selector.json, "{\"k\":[1,2,3]}") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &doc, NULL);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static void test_runtime_allocator_visit_value_buffer_and_cstr(void) {
  char json[400];
  size_t i;
  test_allocator_state alloc;
  lonejson_config config;
  lonejson *runtime;
  lonejson_value_visitor visitor = test_counting_value_visitor();
  test_visit_chunk_state state;
  lonejson_error error;
  lonejson_status status;
  size_t base_live;

  json[0] = '1';
  for (i = 1u; i < sizeof(json) - 1u; ++i) {
    json[i] = (char)('0' + (i % 10u));
  }
  json[sizeof(json) - 1u] = '\0';

  test_allocator_init(&alloc);
  config = lonejson_default_config();
  config.allocator = &alloc.allocator;
  config.json_value_max_number_bytes = 1024u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  base_live = alloc.stats.bytes_live;

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_value_buffer(runtime, json, strlen(json), &visitor,
                                       &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live == base_live);

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_value_cstr(runtime, json, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live == base_live);

  lonejson_free(runtime);
}

static void test_path_value_visitor_empty_and_numeric_segments(void) {
  const char *json = "{\"\":{\"0\":[\"x\"]}}";
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_path_visit_state state;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_path_value_cstr(test_default_runtime(), json,
                                          &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "K<($)") != NULL);
  EXPECT(strstr(state.log, "K>($)") != NULL);
  EXPECT(strstr(state.log, "{($/0:)") != NULL);
  EXPECT(strstr(state.log, "K($/0:)=0") != NULL);
  EXPECT(strstr(state.log, "[($/0:/1:0)") != NULL);
  EXPECT(strstr(state.log, "S($/0:/1:0/1:0)=x") != NULL);
  EXPECT(state.mismatched_chunk_paths == 0u);
}

static void test_path_value_visitor_source_entry_points_and_args(void) {
  const char *json = "{\"a\":[true]}";
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_path_visit_state state;
  test_reader_state reader_state;
  lonejson *runtime;
  lonejson_error error;
  lonejson_status status;
  char path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-path-value-visitor-XXXXXX";
  int fd;
  FILE *fp;

  runtime = test_default_runtime();
  memset(&state, 0, sizeof(state));
  status = lonejson_visit_path_value_buffer(runtime, json, strlen(json),
                                            &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);

  memset(&state, 0, sizeof(state));
  status = runtime->visit_path_value_buffer(runtime, json, strlen(json),
                                            &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);

  memset(&state, 0, sizeof(state));
  status =
      runtime->visit_path_value_cstr(runtime, json, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);

  reader_state.json = json;
  reader_state.offset = 0u;
  reader_state.chunk_size = 2u;
  memset(&state, 0, sizeof(state));
  status = runtime->visit_path_value_reader(
      runtime, test_state_reader, &reader_state, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);

  strcpy(path, LONEJSON_SOURCE_DIR
         "/build/test-tmp/lonejson-path-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    fp = fdopen(fd, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
      memset(&state, 0, sizeof(state));
      status = lonejson_visit_path_value_filep(test_default_runtime(), fp,
                                               &visitor, &state, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);
      EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
      memset(&state, 0, sizeof(state));
      status = runtime->visit_path_value_filep(runtime, fp, &visitor, &state,
                                               &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);
      fclose(fp);
    } else {
      close(fd);
    }
    unlink(path);
  }

  strcpy(path, LONEJSON_SOURCE_DIR
         "/build/test-tmp/lonejson-path-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_path_value_path(test_default_runtime(), path,
                                            &visitor, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);
    memset(&state, 0, sizeof(state));
    status =
        runtime->visit_path_value_path(runtime, path, &visitor, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);
    unlink(path);
  }

  strcpy(path, LONEJSON_SOURCE_DIR
         "/build/test-tmp/lonejson-path-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    EXPECT(lseek(fd, 0, SEEK_SET) == 0);
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_path_value_fd(test_default_runtime(), fd, &visitor,
                                          &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);
    EXPECT(lseek(fd, 0, SEEK_SET) == 0);
    memset(&state, 0, sizeof(state));
    status =
        runtime->visit_path_value_fd(runtime, fd, &visitor, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strstr(state.log, "T($/1:a/1:0)") != NULL);
    close(fd);
    unlink(path);
  }

  status = lonejson_visit_path_value_cstr(test_default_runtime(), json, NULL,
                                          &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status = lonejson_visit_path_value_buffer(test_default_runtime(), NULL, 0u,
                                            &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status = lonejson_visit_path_value_reader(test_default_runtime(), NULL, NULL,
                                            &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status = lonejson_visit_path_value_filep(test_default_runtime(), NULL,
                                           &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status = lonejson_visit_path_value_path(test_default_runtime(), NULL,
                                          &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status = lonejson_visit_path_value_fd(test_default_runtime(), -1, &visitor,
                                        &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_path_value_visitor_limit_failures_cleanup(void) {
  const char *deep = "{\"a\":[{\"b\":true}]}";
  const char *long_key = "{\"long-key\":true}";
  lonejson_path_value_visitor visitor = test_path_value_visitor();
  test_allocator_state alloc;
  lonejson_config config;
  lonejson *runtime;
  test_path_visit_state state;
  lonejson_error error;
  lonejson_status status;
  size_t base_live;

  test_allocator_init(&alloc);
  config = lonejson_default_config();
  config.allocator = &alloc.allocator;
  config.json_value_max_depth = 2u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  base_live = alloc.stats.bytes_live;
  memset(&state, 0, sizeof(state));
  status =
      lonejson_visit_path_value_cstr(runtime, deep, &visitor, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(alloc.stats.bytes_live == base_live);
  lonejson_free(runtime);

  test_allocator_init(&alloc);
  config = lonejson_default_config();
  config.allocator = &alloc.allocator;
  config.json_value_max_key_bytes = 4u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  base_live = alloc.stats.bytes_live;
  memset(&state, 0, sizeof(state));
  status = lonejson_visit_path_value_cstr(runtime, long_key, &visitor, &state,
                                          &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(alloc.stats.bytes_live == base_live);
  lonejson_free(runtime);
}

static void test_runtime_allocator_value_rewrite_reader(void) {
  char json[512];
  char number[400];
  size_t i;
  test_allocator_state alloc;
  lonejson_config config;
  lonejson *runtime;
  test_reader_state reader;
  test_buffer_sink sink;
  unsigned char out[600];
  lonejson_value_rewrite_options options;
  lonejson_error error;
  lonejson_status status;
  const char *path_number[] = {"n"};
  size_t base_live;

  number[0] = '1';
  for (i = 1u; i < sizeof(number) - 1u; ++i) {
    number[i] = (char)('0' + (i % 10u));
  }
  number[sizeof(number) - 1u] = '\0';
  snprintf(json, sizeof(json), "{\"n\":%s}", number);

  test_allocator_init(&alloc);
  config = lonejson_default_config();
  config.allocator = &alloc.allocator;
  config.json_value_max_number_bytes = 1024u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  base_live = alloc.stats.bytes_live;

  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 17u;
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  out[0] = '\0';
  memset(&options, 0, sizeof(options));
  options.target_segments = path_number;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_KEEP;
  status = lonejson_value_rewrite_reader(runtime, test_state_reader, &reader,
                                         test_buffer_sink_write, &sink,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live == base_live);
  EXPECT(sink.length == strlen(json));
  EXPECT(memcmp(sink.buffer, json, sink.length) == 0);

  lonejson_free(runtime);
}

static void test_partial_allocator_parse_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\",\"body\":\"hello\"}";
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  status =
      test_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_parse_reader_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\",\"body\":\"hello\"}";
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  test_reader_state reader;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 5u;
  status = test_parse_reader(&test_alloc_parse_doc_map, &doc, test_state_reader,
                             &reader, &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_serialize_alloc_is_rejected(void) {
  test_allocator_state alloc;
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-2", 6u);
  event.ok = true;
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  serialized =
      test_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_generator_init_is_rejected(void) {
  test_allocator_state alloc;
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_generator generator;
  test_event event;
  lonejson_status status;

  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-3", 6u);
  event.ok = true;
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  status = test_generator_init(&generator, &test_event_map, &event, &options);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(generator.state == NULL);
  EXPECT(generator.error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(generator.error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}
