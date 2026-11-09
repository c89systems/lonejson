static void test_array_stream_mapped_items_from_root_key(void) {
  test_reader_state state = {
      "{\"version\":1,\"items\":[{\"id\":1,\"label\":\"one\"},"
      "{\"id\":2,\"label\":\"two\"}],\"tail\":{\"ok\":true}}",
      0u, 5u};
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;

  stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                         NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  poison_bytes(&item, sizeof(item), 0xA5u);
  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 1);
  EXPECT(strcmp(item.label, "one") == 0);
  lonejson_cleanup(&test_item_map, &item, NULL);

  poison_bytes(&item, sizeof(item), 0x5Au);
  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 2);
  EXPECT(strcmp(item.label, "two") == 0);
  lonejson_cleanup(&test_item_map, &item, NULL);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_raw_values_from_root_array(void) {
  test_reader_state state = {"[ {\"keep\":[1,true,null]}, \"text\", 42 ]", 0u,
                             3u};
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_json_value value;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char raw[128];

  lonejson_json_value_init(test_default_runtime(), &value);
  stream = test_array_stream_open_reader("", test_state_reader, &state, NULL,
                                         &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_json_value_cleanup(&value);
    return;
  }

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&value, test_buffer_sink_write,
                                           &sink,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)raw, "{\"keep\":[1,true,null]}") == 0);

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&value, test_buffer_sink_write,
                                           &sink,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)raw, "\"text\"") == 0);

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&value, test_buffer_sink_write,
                                           &sink,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)raw, "42") == 0);
  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);

  lonejson_array_stream_close(stream);
  lonejson_json_value_cleanup(&value);
}

static void test_array_stream_open_path_and_fd_sources(void) {
  char path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-array-stream-XXXXXX";
  const char *json = "{\"items\":[{\"id\":7,\"label\":\"path\"}]}";
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;
  int fd;
  int pipe_fds[2];
  int flags;

  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);
  stream = test_array_stream_open_path("items", path, NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
    EXPECT(item.id == 7);
    lonejson_cleanup(&test_item_map, &item, NULL);
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
    EXPECT(error.code == LONEJSON_STATUS_OK);
    lonejson_array_stream_close(stream);
  }

  fd = open(path, O_RDONLY);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    stream = test_array_stream_open_fd("items", fd, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(item.id == 7);
      lonejson_cleanup(&test_item_map, &item, NULL);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    close(fd);
  }

  if (pipe(pipe_fds) == 0) {
    flags = fcntl(pipe_fds[0], F_GETFL, 0);
    EXPECT(flags >= 0);
    if (flags >= 0) {
      EXPECT(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
    }

    stream = test_array_stream_open_fd("items", pipe_fds[0], NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);

      EXPECT(write(pipe_fds[1], json, strlen(json)) == (ssize_t)strlen(json));
      close(pipe_fds[1]);
      pipe_fds[1] = -1;

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 7);
        lonejson_cleanup(&test_item_map, &item, NULL);
      }
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    if (pipe_fds[1] >= 0) {
      close(pipe_fds[1]);
    }
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }
  unlink(path);
}

static void test_array_stream_failure_modes(void) {
  const char *cases[] = {"{\"items\":{}}",
                         "{\"boards\":[{\"items\":[]}]}",
                         "{\"items\":[{\"id\":1,\"id\":2}]}",
                         "{\"items\":[{\"label\":\"missing id\"}]}",
                         "{\"items\":[{\"id\":1}],\"items\":[]}",
                         "{\"items\":[{\"id\":1}],\"tail\":}",
                         "{\"items\":[{\"id\":1},]}",
                         NULL};
  lonejson_status expected[] = {
      LONEJSON_STATUS_TYPE_MISMATCH,   LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
      LONEJSON_STATUS_DUPLICATE_FIELD, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
      LONEJSON_STATUS_DUPLICATE_FIELD, LONEJSON_STATUS_INVALID_JSON,
      LONEJSON_STATUS_INVALID_JSON};
  size_t i;
  lonejson_error error;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  test_item item;

  {
    test_reader_state dotted_state = {"{}", 0u, 8u};
    stream = test_array_stream_open_reader("boards.items", test_state_reader,
                                           &dotted_state, NULL, &error);
  }
  EXPECT(stream == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  for (i = 0u; cases[i] != NULL; ++i) {
    test_reader_state state;
    state.json = cases[i];
    state.offset = 0u;
    state.chunk_size = 4u;
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      continue;
    }
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    if (result == LONEJSON_ARRAY_STREAM_ITEM) {
      lonejson_cleanup(&test_item_map, &item, NULL);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    }
    EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
    EXPECT(error.code == expected[i]);
    lonejson_array_stream_close(stream);
  }
}

static lonejson_read_result
test_error_after_prefix_reader(void *user, unsigned char *buffer,
                               size_t capacity) {
  test_reader_state *st = (test_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;

  memset(&rr, 0, sizeof(rr));
  if (st->offset >= st->chunk_size) {
    rr.error_code = EIO;
    return rr;
  }
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (remaining > capacity) {
    remaining = capacity;
  }
  if (remaining > st->chunk_size - st->offset) {
    remaining = st->chunk_size - st->offset;
  }
  memcpy(buffer, st->json + st->offset, remaining);
  st->offset += remaining;
  rr.bytes_read = remaining;
  return rr;
}

static void test_array_stream_streaming_invariants(void) {
  const char *json =
      "{\"items\":[{\"id\":1,\"label\":\"one\"},{bad}],"
      "\"tail\":\"this must not be parsed before the first item\"}";
  test_reader_state state;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;
  size_t after_first_item;

  state.json = json;
  state.offset = 0u;
  state.chunk_size = 1u;
  stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                         NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 1);
  EXPECT(strcmp(item.label, "one") == 0);
  after_first_item = state.offset;
  EXPECT(after_first_item < strlen(json));
  EXPECT(strstr(json + after_first_item, "{bad}") != NULL);
  EXPECT(lonejson_array_stream_error(stream)->code == LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_item_map, &item, NULL);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_delimiter_validation_before_yield(void) {
  static const char *raw_root_cases[] = {
      "[1x]",
      "[true?]",
      "[{}foo]",
  };
  size_t i;

  for (i = 0u; i < sizeof(raw_root_cases) / sizeof(raw_root_cases[0]); ++i) {
    test_reader_state state;
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    lonejson_json_value value;

    state.json = raw_root_cases[i];
    state.offset = 0u;
    state.chunk_size = 1u;
    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state, NULL,
                                           &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{}foo]}", 0u, 1u};
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    lonejson_json_value value;

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":1}foo]}", 0u, 1u};
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
  }
}

static void test_array_stream_mapped_items_do_not_materialize_item(void) {
  test_reader_state state = {
      "[{\"id\":9,\"label\":\"large-label-that-truncates\"}]", 0u, 1u};
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;

  allocator.malloc_fn = test_no_realloc_malloc;
  allocator.realloc_fn = test_no_realloc_realloc;
  allocator.free_fn = test_no_realloc_free;
  options.allocator = &allocator;
  stream = test_array_stream_open_reader("", test_state_reader, &state,
                                         &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 9);
  EXPECT(strcmp(item.label, "large-label") == 0);
  lonejson_cleanup(&test_item_map, &item, NULL);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  lonejson_array_stream_close(stream);
}

static void
test_array_stream_reused_mapped_destination_resets_allocations(void) {
  test_reader_state state = {
      "[{\"first\":\"aaaaaaaa\",\"second\":\"bbbbbbbb\"},"
      "{\"first\":\"cc\",\"second\":\"dd\"}]",
      0u, 7u};
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_two_alloc_string_doc doc;

  reset_lonejson_alloc_stats();
  lonejson_init(test_default_runtime(), &test_two_alloc_string_doc_map, &doc,
                NULL);
  stream = test_array_stream_open_reader("", test_state_reader, &state, NULL,
                                         &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_two_alloc_string_doc_map, &doc, NULL);
    return;
  }

  result = lonejson_array_stream_next(stream, &test_two_alloc_string_doc_map,
                                      &doc, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(doc.first != NULL && strcmp(doc.first, "aaaaaaaa") == 0);
  EXPECT(doc.second != NULL && strcmp(doc.second, "bbbbbbbb") == 0);

  result = lonejson_array_stream_next(stream, &test_two_alloc_string_doc_map,
                                      &doc, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(doc.first != NULL && strcmp(doc.first, "cc") == 0);
  EXPECT(doc.second != NULL && strcmp(doc.second, "dd") == 0);

  result = lonejson_array_stream_next(stream, &test_two_alloc_string_doc_map,
                                      &doc, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);

  lonejson_array_stream_close(stream);
  lonejson_cleanup(&test_two_alloc_string_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_array_stream_skips_do_not_materialize_values(void) {
  char json[1024];
  size_t pos = 0u;
  size_t i;
  test_reader_state state;
  test_realloc_limit_state limit = {128u, 0u};
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;

  memcpy(json + pos, "{\"meta\":\"", 9u);
  pos += 9u;
  for (i = 0u; i < 700u; ++i) {
    json[pos++] = 'x';
  }
  memcpy(json + pos, "\",\"items\":[{\"id\":5,\"label\":\"ok\"}],\"tail\":\"",
         42u);
  pos += 42u;
  for (i = 0u; i < 120u; ++i) {
    json[pos++] = 'y';
  }
  memcpy(json + pos, "\"}", 3u);

  allocator.ctx = &limit;
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  options.allocator = &allocator;

  state.json = json;
  state.offset = 0u;
  state.chunk_size = 3u;
  stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                         &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 5);
  EXPECT(strcmp(item.label, "ok") == 0);
  lonejson_cleanup(&test_item_map, &item, NULL);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_escaped_key_path_matching(void) {
  static const struct {
    const char *path;
    const char *json;
    int id;
  } cases[] = {
      {"items", "{\"\\u0069tems\":[{\"id\":10,\"label\":\"u\"}]}", 10},
      {"items", "{\"it\\u0065ms\":[{\"id\":11,\"label\":\"u\"}]}", 11},
      {"a/b", "{\"a\\/b\":[{\"id\":12,\"label\":\"s\"}]}", 12},
      {"a\\b", "{\"a\\\\b\":[{\"id\":13,\"label\":\"b\"}]}", 13},
      {"it\tms", "{\"it\\tms\":[{\"id\":14,\"label\":\"t\"}]}", 14},
      {"\xF0\x9D\x84\x9E", "{\"\\uD834\\uDD1E\":[{\"id\":15,\"label\":\"p\"}]}",
       15},
  };
  size_t i;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    test_reader_state state;
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    state.json = cases[i].json;
    state.offset = 0u;
    state.chunk_size = 1u;
    stream = test_array_stream_open_reader(cases[i].path, test_state_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      continue;
    }
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
    EXPECT(item.id == cases[i].id);
    lonejson_cleanup(&test_item_map, &item, NULL);
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_escaped_key_failure_modes(void) {
  static const char *invalid_keys[] = {
      "{\"\\uD834x\":0,\"items\":[]}",
      "{\"\\uDD1E\":0,\"items\":[]}",
      "{\"\\u12X4\":0,\"items\":[]}",
      "{\"bad\\q\":0,\"items\":[]}",
  };
  size_t i;

  {
    lonejson__parse_options options = lonejson__default_parse_options();
    test_reader_state state = {"{\"items\":[],\"it\\u0065ms\":[]}", 0u, 1u};
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    options.reject_duplicate_keys = 1;
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  for (i = 0u; i < sizeof(invalid_keys) / sizeof(invalid_keys[0]); ++i) {
    test_reader_state state;
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    state.json = invalid_keys[i];
    state.offset = 0u;
    state.chunk_size = 1u;
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      continue;
    }
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
    EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_duplicate_rejection_invariants(void) {
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  options.reject_duplicate_keys = 1;

  {
    test_reader_state state = {"[{\"a\":1,\"\\u0061\":2}]", 0u, 1u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"[{\"\":1,\"\":2}]", 0u, 1u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"[{\"a\":1,\"nested\":{\"a\":2}}]", 0u, 1u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {
        "{\"meta\":1,\"\\u006deta\":2,\"items\":[{\"id\":1}]}", 0u, 1u};

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {
        "{\"meta\":{\"x\":1,\"\\u0078\":2},\"items\":[{\"id\":1}]}", 0u, 1u};

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {
        "{\"meta\":[{\"x\":1,\"x\":2}],\"items\":[{\"id\":1}]}", 0u, 1u};

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"{\"\":1,\"\":2,\"items\":[{\"id\":1}]}", 0u,
                               1u};

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {
        "{\"items\":[{\"id\":2}],\"meta\":{\"x\":1,\"x\":2}}", 0u, 1u};

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(item.id == 2);
      lonejson_cleanup(&test_item_map, &item, NULL);

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":2}],\"meta\":1,\"meta\":2}",
                               0u, 1u};

    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(item.id == 2);
      lonejson_cleanup(&test_item_map, &item, NULL);

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"[{\"a\":1,\"a\":2}]", 0u, 1u};

    options.reject_duplicate_keys = 0;
    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state,
                                           &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }
}

static void test_array_stream_raw_duplicate_check_allows_large_values(void) {
  static const size_t big_string_len = 1024u * 1024u + 1024u;
  static const size_t big_number_len = 300u;
  static const size_t big_key_len = 64u * 1024u + 128u;
  lonejson_config config;
  lonejson *large_value_lj;
  char *json;
  test_reader_state state;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  size_t pos;

  config = lonejson_default_config();
  config.json_value_max_string_bytes = big_string_len + 2u;
  config.json_value_max_number_bytes = big_number_len;
  config.json_value_max_key_bytes = big_key_len;
  config.json_value_max_total_bytes = big_string_len + 2u;
  large_value_lj = lonejson_new(&config, &error);
  EXPECT(large_value_lj != NULL);
  if (large_value_lj == NULL) {
    return;
  }

  json = (char *)malloc(big_string_len + 5u);
  EXPECT(json != NULL);
  if (json != NULL) {
    json[0] = '[';
    json[1] = '"';
    memset(json + 2u, 's', big_string_len);
    json[big_string_len + 2u] = '"';
    json[big_string_len + 3u] = ']';
    json[big_string_len + 4u] = '\0';

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 4096u;
    lonejson_json_value_init(large_value_lj, &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state, NULL,
                                           &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == big_string_len + 2u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
    free(json);
  }

  json = (char *)malloc(big_number_len + 3u);
  EXPECT(json != NULL);
  if (json != NULL) {
    json[0] = '[';
    json[1] = '1';
    memset(json + 2u, '2', big_number_len - 1u);
    json[big_number_len + 1u] = ']';
    json[big_number_len + 2u] = '\0';

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 17u;
    lonejson_json_value_init(large_value_lj, &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state, NULL,
                                           &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == big_number_len);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
    free(json);
  }

  json = (char *)malloc(big_key_len + 9u);
  EXPECT(json != NULL);
  if (json != NULL) {
    pos = 0u;
    json[pos++] = '[';
    json[pos++] = '{';
    json[pos++] = '"';
    memset(json + pos, 'k', big_key_len);
    pos += big_key_len;
    json[pos++] = '"';
    json[pos++] = ':';
    json[pos++] = '1';
    json[pos++] = '}';
    json[pos++] = ']';
    json[pos] = '\0';

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 97u;
    lonejson_json_value_init(large_value_lj, &value);
    stream = test_array_stream_open_reader("", test_state_reader, &state, NULL,
                                           &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == big_key_len + 6u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
    free(json);
  }

  lonejson_free(large_value_lj);
}

static void test_array_stream_reader_tolerates_empty_reads(void) {
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  {
    test_empty_read_state state = {{"[]", 0u, 1u}, 2u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_empty_then_state_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(state.empty_reads_remaining == 0u);
      EXPECT(state.inner.offset == 2u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_empty_read_state state = {
        {"{\"items\":[{\"id\":42,\"label\":\"ok\"}]}", 0u, 1u}, 3u};

    stream = test_array_stream_open_reader(
        "items", test_empty_then_state_reader, &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 42);
      EXPECT(strcmp(item.label, "ok") == 0);
      EXPECT(state.empty_reads_remaining == 0u);
      lonejson_cleanup(&test_item_map, &item, NULL);

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_intermittent_empty_read_state state = {
        {"[\"ab\",{\"k\":[1,2]},true]", 0u, 1u}, 1, 0u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_intermittent_empty_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.len == 4u);
      lonejson_json_value_cleanup(&value);

      lonejson_json_value_init(test_default_runtime(), &value);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.len == 11u);
      lonejson_json_value_cleanup(&value);

      lonejson_json_value_init(test_default_runtime(), &value);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.len == 4u);
      lonejson_json_value_cleanup(&value);

      lonejson_json_value_init(test_default_runtime(), &value);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(state.inner.offset == strlen(state.inner.json));
      EXPECT(state.empty_reads > 10u);
      lonejson_array_stream_close(stream);
      lonejson_json_value_cleanup(&value);
    } else {
      lonejson_json_value_cleanup(&value);
    }
  }

  {
    test_intermittent_empty_read_state state = {
        {"{\"meta\":{\"skip\":[1,2,3]},"
         "\"items\":[{\"id\":7,\"label\":\"ok\"},"
         "{\"id\":8,\"label\":\"two\"}],"
         "\"tail\":{\"done\":true}}",
         0u, 1u},
        1,
        0u};

    stream = test_array_stream_open_reader(
        "items", test_intermittent_empty_reader, &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 7);
      EXPECT(strcmp(item.label, "ok") == 0);
      lonejson_cleanup(&test_item_map, &item, NULL);

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 8);
      EXPECT(strcmp(item.label, "two") == 0);
      lonejson_cleanup(&test_item_map, &item, NULL);

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(state.inner.offset == strlen(state.inner.json));
      EXPECT(state.empty_reads > 20u);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_intermittent_empty_read_state state = {{"[1x]", 0u, 1u}, 1, 0u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_intermittent_empty_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      EXPECT(state.empty_reads > 0u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }
}

static void test_array_stream_additional_failure_modes(void) {
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  {
    test_reader_state state = {"{\"meta\":{bad},\"items\":[]}", 0u, 2u};
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"{\"items\":[01]}", 0u, 1u};
    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":1}", 0u, 8u};
    stream = test_array_stream_open_reader(
        "items", test_error_after_prefix_reader, &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_IO_ERROR);
      EXPECT(error.system_errno == EIO);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"[{\"id\":1}]", 0u, 6u};

    stream = test_array_stream_open_reader("", test_error_after_prefix_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_IO_ERROR);
      EXPECT(error.system_errno == EIO);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"[{\"id\":1}]", 0u, 6u};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_error_after_prefix_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_IO_ERROR);
      EXPECT(error.system_errno == EIO);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":1}]}", 0u, 16u};
    stream = test_array_stream_open_reader("items", test_state_reader, &state,
                                           NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, NULL, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
      lonejson_array_stream_close(stream);
    }
  }
}

static void test_array_stream_would_block_boundaries(void) {
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;
  size_t block_at;

  {
    test_would_block_read_state state = {"[{\"id\":1}]", 0u, 8u, 0u, 0};

    stream = test_array_stream_open_reader("", test_would_block_once_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      item.id = 99;
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 99);
      EXPECT(state.blocked);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 1);
        lonejson_cleanup(&test_item_map, &item, NULL);
      }
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[{\"id\":2}]", 0u, 8u, 0u, 0};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_would_block_once_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_would_block_read_state state = {"{\"meta\":1,\"items\":[{\"id\":1}]}",
                                         0u, 4u, 8u, 0};

    stream = test_array_stream_open_reader(
        "items", test_would_block_once_reader, &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 1);
        lonejson_cleanup(&test_item_map, &item, NULL);
      }
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[{\"id\":1}]", 0u, 4u, 6u, 0};

    stream = test_array_stream_open_reader("", test_would_block_once_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 1);
        lonejson_cleanup(&test_item_map, &item, NULL);
      }
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[{\"id\":1}]", 0u, 32u, 9u, 0};
    test_item first;
    test_item second;

    first.id = 99;
    second.id = 77;
    stream = test_array_stream_open_reader("", test_would_block_once_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &first, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(first.id == 1);
      EXPECT(second.id == 77);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &second, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
      EXPECT(second.id == 77);
      lonejson_cleanup(&test_item_map, &first, NULL);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[1,2]", 0u, 4u, 2u, 0};

    lonejson_json_value_init(test_default_runtime(), &value);
    stream = test_array_stream_open_reader("", test_would_block_once_reader,
                                           &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == 1u);
      if (value.json != NULL) {
        EXPECT(memcmp(value.json, "1", 1u) == 0);
      }
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    const char *json = "{\"meta\":{\"nested\":[1,2,{\"x\":\"y\"}]},"
                       "\"items\":[{\"id\":7,\"label\":\"ok\"},"
                       "{\"id\":8,\"label\":\"two\"}],\"tail\":true}";

    for (block_at = 0u; block_at <= strlen(json); ++block_at) {
      test_would_block_read_state state;
      state.json = json;
      state.offset = 0u;
      state.chunk_size = 3u;
      state.block_after = block_at;
      state.blocked = 0;
      stream = test_array_stream_open_reader(
          "items", test_would_block_once_reader, &state, NULL, &error);
      EXPECT(stream != NULL);
      if (stream == NULL) {
        continue;
      }

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result =
            lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 7);
        EXPECT(strcmp(item.label, "ok") == 0);
        lonejson_cleanup(&test_item_map, &item, NULL);
      }

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result =
            lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 8);
        EXPECT(strcmp(item.label, "two") == 0);
        lonejson_cleanup(&test_item_map, &item, NULL);
      }

      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result =
            lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
  }

  {
    const char *json = "[{\"keep\":[1,true,null]},\"text\",42]";
    const size_t expected_lens[] = {22u, 6u, 2u};
    size_t item_index;

    for (block_at = 0u; block_at <= strlen(json); ++block_at) {
      test_would_block_read_state state;
      state.json = json;
      state.offset = 0u;
      state.chunk_size = 2u;
      state.block_after = block_at;
      state.blocked = 0;
      lonejson_json_value_init(test_default_runtime(), &value);
      stream = test_array_stream_open_reader("", test_would_block_once_reader,
                                             &state, NULL, &error);
      EXPECT(stream != NULL);
      if (stream == NULL) {
        lonejson_json_value_cleanup(&value);
        continue;
      }

      for (item_index = 0u; item_index < 3u; ++item_index) {
        lonejson_json_value_reset(&value);
        result = lonejson_array_stream_next_value(stream, &value, &error);
        while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
          EXPECT(error.code == LONEJSON_STATUS_OK);
          result = lonejson_array_stream_next_value(stream, &value, &error);
        }
        EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
        EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
        EXPECT(value.len == expected_lens[item_index]);
      }

      result = lonejson_array_stream_next_value(stream, &value, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result = lonejson_array_stream_next_value(stream, &value, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
      lonejson_json_value_cleanup(&value);
    }
  }
}

static void test_array_stream_depth_limit_and_argument_guards(void) {
  test_reader_state state = {"[[[[]]]]", 0u, 2u};
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  options.max_depth = 2u;
  lonejson_json_value_init(test_default_runtime(), &value);
  stream = test_array_stream_open_reader("", test_state_reader, &state,
                                         &options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    result = lonejson_array_stream_next_value(stream, &value, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
    EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
    lonejson_array_stream_close(stream);
  }

  EXPECT(lonejson_array_stream_next(NULL, &test_item_map, &item, &error) ==
         LONEJSON_ARRAY_STREAM_ERROR);
  EXPECT(lonejson_array_stream_next_value(NULL, &value, &error) ==
         LONEJSON_ARRAY_STREAM_ERROR);
  lonejson_json_value_cleanup(&value);
}

static void test_array_stream_runtime_parse_depth_overrides_value_depth(void) {
  enum { test_depth = 4 };
  char json[2u * test_depth + 4u];
  size_t pos = 0u;
  size_t i;
  lonejson_config config;
  lonejson *runtime;
  test_reader_state state;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;

  json[pos++] = '[';
  for (i = 0u; i < test_depth; ++i) {
    json[pos++] = '[';
  }
  json[pos++] = '0';
  for (i = 0u; i < test_depth; ++i) {
    json[pos++] = ']';
  }
  json[pos++] = ']';
  json[pos] = '\0';

  config = lonejson_default_config();
  config.max_depth = 0u;
  config.json_value_max_depth = 1u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  state.json = json;
  state.offset = 0u;
  state.chunk_size = 3u;
  lonejson_json_value_init(runtime, &value);
  stream = lonejson_array_stream_open_reader(runtime, "", test_state_reader,
                                             &state, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_json_value_cleanup(&value);
    lonejson_free(runtime);
    return;
  }

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);

  lonejson_array_stream_close(stream);
  lonejson_json_value_cleanup(&value);
  lonejson_free(runtime);
}

typedef struct test_array_stream_push_seen {
  int ids[8];
  char labels[8][12];
  size_t count;
} test_array_stream_push_seen;

typedef struct test_array_stream_string_seen {
  char items[8][64];
  size_t item_lens[8];
  size_t begins;
  size_t chunks;
  size_t ends;
  size_t current;
  size_t fail_after_chunks;
  size_t null_items;
  size_t unterminated_items;
} test_array_stream_string_seen;

static lonejson_status test_array_stream_push_item(void *user, void *dst) {
  test_array_stream_push_seen *seen = (test_array_stream_push_seen *)user;
  test_item *item = (test_item *)dst;

  if (seen->count >= sizeof(seen->ids) / sizeof(seen->ids[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  seen->ids[seen->count] = (int)item->id;
  strcpy(seen->labels[seen->count], item->label);
  seen->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_push_fail_item(void *user, void *dst) {
  (void)user;
  (void)dst;
  return LONEJSON_STATUS_INVALID_ARGUMENT;
}

static lonejson_status test_array_stream_push_alloc_item(void *user,
                                                         void *dst) {
  test_array_stream_push_seen *seen = (test_array_stream_push_seen *)user;
  test_alloc_parse_doc *doc = (test_alloc_parse_doc *)dst;

  seen->count++;
  if (doc->name != NULL &&
      seen->count <= sizeof(seen->labels) / sizeof(seen->labels[0])) {
    snprintf(seen->labels[seen->count - 1u],
             sizeof(seen->labels[seen->count - 1u]), "%s", doc->name);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_push_alloc_fail_item(void *user,
                                                              void *dst) {
  (void)user;
  (void)dst;
  return LONEJSON_STATUS_IO_ERROR;
}

static lonejson_status test_array_stream_string_begin(void *user,
                                                      lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  (void)error;
  if (seen->begins >= sizeof(seen->items) / sizeof(seen->items[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  seen->current = seen->begins;
  seen->item_lens[seen->current] = 0u;
  seen->items[seen->current][0] = '\0';
  seen->begins++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_string_chunk(void *user,
                                                      const char *data,
                                                      size_t len,
                                                      lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  size_t room;
  (void)error;
  seen->chunks++;
  if (seen->fail_after_chunks != 0u &&
      seen->chunks >= seen->fail_after_chunks) {
    return LONEJSON_STATUS_IO_ERROR;
  }
  if (seen->begins == 0u) {
    return LONEJSON_STATUS_INTERNAL_ERROR;
  }
  room =
      sizeof(seen->items[seen->current]) - 1u - seen->item_lens[seen->current];
  if (len > room) {
    len = room;
  }
  memcpy(seen->items[seen->current] + seen->item_lens[seen->current], data,
         len);
  seen->item_lens[seen->current] += len;
  seen->items[seen->current][seen->item_lens[seen->current]] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_string_end(void *user,
                                                    lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  (void)error;
  seen->ends++;
  return LONEJSON_STATUS_OK;
}

static lonejson_array_stream_string_handler
test_array_stream_string_handler(void) {
  lonejson_array_stream_string_handler handler;
  memset(&handler, 0, sizeof(handler));
  handler.begin = test_array_stream_string_begin;
  handler.chunk = test_array_stream_string_chunk;
  handler.end = test_array_stream_string_end;
  return handler;
}

static lonejson_status test_array_stream_string_item(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  size_t room;
  (void)error;

  if (seen->ends >= sizeof(seen->items) / sizeof(seen->items[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  if (data == NULL) {
    seen->null_items++;
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (data[len] != '\0') {
    seen->unterminated_items++;
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (seen->fail_after_chunks != 0u &&
      seen->ends + 1u >= seen->fail_after_chunks) {
    return LONEJSON_STATUS_IO_ERROR;
  }
  room = sizeof(seen->items[seen->ends]) - 1u;
  if (len > room) {
    len = room;
  }
  memcpy(seen->items[seen->ends], data, len);
  seen->items[seen->ends][len] = '\0';
  seen->item_lens[seen->ends] = len;
  seen->ends++;
  return LONEJSON_STATUS_OK;
}

static void test_array_stream_push_api(void) {
  static const char json[] = "{\"meta\":{\"skip\":[1,2,{\"x\":\"y\"}]},"
                             "\"items\":[{\"id\":1,\"label\":\"one\"},"
                             "{\"id\":2,\"label\":\"two\"}],\"tail\":true}";
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_item item;
  test_array_stream_push_seen seen;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    for (i = 0u; i < strlen(json); ++i) {
      status = lonejson_array_stream_push(
          stream, &test_item_map, &item, json + i, 1u,
          test_array_stream_push_item, &seen, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
    }
    EXPECT(seen.count == 2u);
    EXPECT(seen.ids[0] == 1);
    EXPECT(strcmp(seen.labels[0], "one") == 0);
    EXPECT(seen.ids[1] == 2);
    EXPECT(strcmp(seen.labels[1], "two") == 0);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(error.code == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 2u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, "[{\"id\":3,\"label\":\"thr\"}",
        strlen("[{\"id\":3,\"label\":\"thr\"}"), test_array_stream_push_item,
        &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 0u);
    status =
        lonejson_array_stream_push(stream, &test_item_map, &item, "x]", 2u,
                                   test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, "[{\"id\":4,\"label\":\"cb\"}]",
        strlen("[{\"id\":4,\"label\":\"cb\"}]"),
        test_array_stream_push_fail_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
    status =
        lonejson_array_stream_push(stream, &test_item_map, &item, "[]", 2u,
                                   test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_array_stream_close(stream);
  }

  stream = test_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
    lonejson_array_stream_close(stream);
  }

  stream = test_array_stream_open_push("boards.items", NULL, &error);
  EXPECT(stream == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  stream =
      test_array_stream_open_reader("", test_state_reader, NULL, NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status =
        lonejson_array_stream_push(stream, &test_item_map, &item, "[]", 2u,
                                   test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_array_stream_close(stream);
  }

  stream = test_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item, "[]", 2u,
                                        NULL, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    status =
        lonejson_array_stream_push(stream, &test_item_map, &item, NULL, 1u,
                                   test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_push_invariant_failure_modes(void) {
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_push_seen seen;
  test_item item;

  options.reject_duplicate_keys = 1;
  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", &options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] =
        "{\"meta\":{\"x\":1,\"\\u0078\":2},\"items\":[{\"id\":1}]}";
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, json, strlen(json),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"meta\":true}";
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, json, strlen(json),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", &options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] =
        "{\"items\":[{\"id\":2}],\"tail\":1,\"\\u0074ail\":2}";
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, json, strlen(json),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 2);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"items\":[{\"id\":3}],\"tail\":{bad}}";
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, json, strlen(json),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 3);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] =
        "{\"\\u0069tems\":[{\"id\":6,\"label\":\"esc\"}]}";
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, json, strlen(json),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 6);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item, "[{\"id\":7}", strlen("[{\"id\":7}"),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 0u);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item,
        "[{\"id\":8,\"label\":\"label-that-is-too-long\"}]",
        strlen("[{\"id\":8,\"label\":\"label-that-is-too-long\"}]"),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_TRUNCATED);
    EXPECT(error.truncated);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 8);
    EXPECT(strcmp(seen.labels[0], "label-that-") == 0);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_TRUNCATED);
    EXPECT(error.truncated);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_push_does_not_materialize_response(void) {
  char json[1024];
  size_t pos = 0u;
  size_t i;
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson__parse_options options = lonejson__default_parse_options();
  test_realloc_limit_state limit = {128u, 0u};
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_push_seen seen;
  test_item item;
  static const char suffix[] = "\",\"items\":[{\"id\":5,\"label\":\"ok\"}]}";

  memcpy(json + pos, "{\"meta\":\"", 9u);
  pos += 9u;
  for (i = 0u; i < 700u; ++i) {
    json[pos++] = 'x';
  }
  memcpy(json + pos, suffix, sizeof(suffix) - 1u);
  pos += sizeof(suffix) - 1u;

  allocator.ctx = &limit;
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  options.allocator = &allocator;

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("items", &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < pos; ++i) {
    status =
        lonejson_array_stream_push(stream, &test_item_map, &item, json + i, 1u,
                                   test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status =
      lonejson_array_stream_finish(stream, &test_item_map, &item,
                                   test_array_stream_push_item, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.count == 1u);
  EXPECT(seen.ids[0] == 5);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_push_cleans_reused_destination(void) {
  static const char json[] = "[{\"name\":\"one\",\"tags\":[\"a\",\"b\"]},"
                             "{\"name\":\"two\",\"tags\":[\"c\",\"d\"]}]";
  lonejson__parse_options options = lonejson__default_parse_options();
  test_allocator_state alloc_state;
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  test_array_stream_push_seen seen;

  test_allocator_init(&alloc_state);
  options.allocator = &alloc_state.allocator;
  memset(&seen, 0, sizeof(seen));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &doc, NULL);
  stream = test_array_stream_open_push("", &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  status = lonejson_array_stream_push(
      stream, &test_alloc_parse_doc_map, &doc, json, strlen(json),
      test_array_stream_push_alloc_item, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_array_stream_finish(stream, &test_alloc_parse_doc_map, &doc,
                                        test_array_stream_push_alloc_item,
                                        &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.count == 2u);
  EXPECT(strcmp(seen.labels[0], "one") == 0);
  EXPECT(strcmp(seen.labels[1], "two") == 0);
  lonejson_array_stream_close(stream);
  EXPECT(alloc_state.stats.bytes_live == 0u);

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("", &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  status = lonejson_array_stream_push(
      stream, &test_alloc_parse_doc_map, &doc,
      "[{\"name\":\"fail\",\"tags\":[\"owned\"]}]",
      strlen("[{\"name\":\"fail\",\"tags\":[\"owned\"]}]"),
      test_array_stream_push_alloc_fail_item, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  lonejson_array_stream_close(stream);
  EXPECT(alloc_state.stats.bytes_live == 0u);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
}

static void test_array_stream_push_string_items(void) {
  static const char json[] =
      "{\"cursor\":\"c1\",\"keys\":[\"alpha\",\"br\\u0061vo\",\"line\\nkey\"],"
      "\"index_seq\":7}";
  lonejson_array_stream_string_handler handler =
      test_array_stream_string_handler();
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < strlen(json); ++i) {
    status = lonejson_array_stream_push_string(stream, json + i, 1u, &handler,
                                               &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_array_stream_finish_string(stream, &handler, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.begins == 3u);
  EXPECT(seen.ends == 3u);
  EXPECT(seen.chunks >= 3u);
  EXPECT(strcmp(seen.items[0], "alpha") == 0);
  EXPECT(strcmp(seen.items[1], "bravo") == 0);
  EXPECT(strcmp(seen.items[2], "line\nkey") == 0);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_push_string_item_callback(void) {
  static const char json[] =
      "{\"cursor\":\"c1\",\"keys\":[\"\",\"alpha\",\"\",\"br\\u0061vo\","
      "\"line\\nkey\"],"
      "\"index_seq\":7}";
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < strlen(json); ++i) {
    status = lonejson_array_stream_push_string_items(
        stream, json + i, 1u, test_array_stream_string_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_array_stream_finish_string_items(
      stream, test_array_stream_string_item, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.ends == 5u);
  EXPECT(seen.null_items == 0u);
  EXPECT(seen.unterminated_items == 0u);
  EXPECT(strcmp(seen.items[0], "") == 0);
  EXPECT(strcmp(seen.items[1], "alpha") == 0);
  EXPECT(strcmp(seen.items[2], "") == 0);
  EXPECT(strcmp(seen.items[3], "bravo") == 0);
  EXPECT(strcmp(seen.items[4], "line\nkey") == 0);
  lonejson_array_stream_close(stream);

  memset(&seen, 0, sizeof(seen));
  seen.fail_after_chunks = 1u;
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string_items(
        stream, "{\"keys\":[\"fail\"]}", strlen("{\"keys\":[\"fail\"]}"),
        test_array_stream_string_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_IO_ERROR);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_push_string_item_callback_bounds(void) {
  const char prefix[] = "{\"keys\":[\"";
  const char suffix[] = "\"]}";
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;
  lonejson__value_limits limits;
  char *json;
  size_t string_len;
  size_t total;

  limits = lonejson__default_value_limits();
  string_len = limits.max_string_bytes + 1u;
  total = strlen(prefix) + string_len + strlen(suffix);
  json = (char *)malloc(total + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    return;
  }
  memcpy(json, prefix, strlen(prefix));
  memset(json + strlen(prefix), 'a', string_len);
  memcpy(json + strlen(prefix) + string_len, suffix, strlen(suffix));
  json[total] = '\0';

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string_items(
        stream, json, total, test_array_stream_string_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(seen.ends == 0u);
    lonejson_array_stream_close(stream);
  }
  free(json);
}

static void test_array_stream_push_string_runtime_value_limits(void) {
  static const char json[] = "{\"keys\":[\"alpha\"]}";
  lonejson_config config;
  lonejson *strict_lj;
  lonejson *default_lj;
  lonejson_array_stream *stream;
  lonejson_array_stream_string_handler handler =
      test_array_stream_string_handler();
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;

  config = lonejson_default_config();
  config.json_value_max_string_bytes = 4u;
  config.json_value_max_total_bytes = 4u;
  strict_lj = lonejson_new(&config, &error);
  EXPECT(strict_lj != NULL);
  default_lj = lonejson_new(NULL, &error);
  EXPECT(default_lj != NULL);
  if (strict_lj == NULL || default_lj == NULL) {
    lonejson_free(strict_lj);
    lonejson_free(default_lj);
    return;
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push(strict_lj, "keys", &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push(default_lj, "keys", &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        lonejson_array_stream_finish_string(stream, &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(seen.items[0], "alpha") == 0);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push(strict_lj, "keys", &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string_items(
        stream, json, strlen(json), test_array_stream_string_item, &seen,
        &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_array_stream_close(stream);
  }

  lonejson_free(strict_lj);
  lonejson_free(default_lj);
}

static void test_array_stream_push_string_failure_modes(void) {
  lonejson_array_stream_string_handler handler =
      test_array_stream_string_handler();
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"keys\":[\"ok\",42]}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
    EXPECT(seen.ends == 1u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"keys\":[\"ok\"x]}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.begins == 1u);
    EXPECT(seen.ends == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"meta\":true}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
    EXPECT(seen.begins == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  seen.fail_after_chunks = 1u;
  stream = test_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"keys\":[\"fail\"]}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_IO_ERROR);
    lonejson_array_stream_close(stream);
  }
}

typedef struct test_mapped_string_array_stream_doc {
  lonejson_string_array_stream keys;
  char cursor[32];
  lonejson_uint64 index_seq;
  lonejson_json_value metadata;
} test_mapped_string_array_stream_doc;

static const lonejson_field test_mapped_string_array_stream_doc_fields[] = {
    LONEJSON_FIELD_STRING_ARRAY_STREAM_REQ(test_mapped_string_array_stream_doc,
                                           keys, "keys"),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_mapped_string_array_stream_doc, cursor,
                                    "cursor", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_U64_REQ(test_mapped_string_array_stream_doc, index_seq,
                           "index_seq"),
    LONEJSON_FIELD_JSON_VALUE(test_mapped_string_array_stream_doc, metadata,
                              "metadata")};
LONEJSON_MAP_DEFINE(test_mapped_string_array_stream_doc_map,
                    test_mapped_string_array_stream_doc,
                    test_mapped_string_array_stream_doc_fields);

static void test_array_stream_mapped_string_field_envelope(void) {
  static const char json[] =
      "{\"namespace\":\"default\",\"keys\":[\"key-a\",\"key-b\",\"key-c\"],"
      "\"cursor\":\"next-page\",\"index_seq\":123}";
  test_reader_state state = {json, 0u, 3u};
  test_mapped_string_array_stream_doc doc;
  test_array_stream_string_seen seen;
  lonejson_array_stream_string_handler handler;
  lonejson_error error;

  memset(&seen, 0, sizeof(seen));
  handler = test_array_stream_string_handler();
  lonejson_init(test_default_runtime(),
                &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(lonejson_string_array_stream_set_handler(
             &doc.keys, &handler, &seen, &error) == LONEJSON_STATUS_OK);
  EXPECT(test_parse_reader(&test_mapped_string_array_stream_doc_map, &doc,
                           test_state_reader, &state, NULL,
                           &error) == LONEJSON_STATUS_OK);
  EXPECT(seen.begins == 3u);
  EXPECT(seen.ends == 3u);
  EXPECT(strcmp(seen.items[0], "key-a") == 0);
  EXPECT(strcmp(seen.items[1], "key-b") == 0);
  EXPECT(strcmp(seen.items[2], "key-c") == 0);
  EXPECT(strcmp(doc.cursor, "next-page") == 0);
  EXPECT(doc.index_seq == 123u);
  lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
}

static void test_array_stream_mapped_string_field_zeroed_set_handler(void) {
  static const char json[] =
      "{\"keys\":[\"key-a\",\"key-b\"],\"cursor\":\"next-page\","
      "\"index_seq\":123}";
  test_mapped_string_array_stream_doc doc;
  test_array_stream_string_seen seen;
  lonejson_array_stream_string_handler handler;
  lonejson_error error;

  memset(&doc, 0, sizeof(doc));
  memset(&seen, 0, sizeof(seen));
  handler = test_array_stream_string_handler();
  EXPECT(lonejson_string_array_stream_set_handler(
             &doc.keys, &handler, &seen, &error) == LONEJSON_STATUS_OK);
  EXPECT(test_parse_cstr(&test_mapped_string_array_stream_doc_map, &doc, json,
                         NULL, &error) == LONEJSON_STATUS_OK);
  EXPECT(seen.begins == 2u);
  EXPECT(seen.ends == 2u);
  EXPECT(strcmp(seen.items[0], "key-a") == 0);
  EXPECT(strcmp(seen.items[1], "key-b") == 0);
  EXPECT(strcmp(doc.cursor, "next-page") == 0);
  EXPECT(doc.index_seq == 123u);
  lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
}

static void
test_array_stream_mapped_string_field_lifecycle_preserves_set_handler_method(
    void) {
  test_mapped_string_array_stream_doc doc;
  test_array_stream_string_seen seen;
  lonejson_array_stream_string_handler handler;
  lonejson *runtime;
  lonejson_error error;

  memset(&doc, 0, sizeof(doc));
  memset(&seen, 0, sizeof(seen));
  handler = test_array_stream_string_handler();
  runtime = test_default_runtime();

  runtime->init(runtime, &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(doc.keys.set_handler == lonejson_string_array_stream_set_handler);
  EXPECT(doc.keys.set_handler(&doc.keys, &handler, &seen, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(doc.keys.handler.chunk == handler.chunk);

  runtime->reset(runtime, &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(doc.keys.set_handler == lonejson_string_array_stream_set_handler);
  EXPECT(doc.keys.handler.chunk == handler.chunk);

  runtime->cleanup(runtime, &test_mapped_string_array_stream_doc_map, &doc,
                   NULL);
  EXPECT(doc.keys.set_handler == lonejson_string_array_stream_set_handler);
  EXPECT(doc.keys.handler.chunk == handler.chunk);
}

static void
test_array_stream_mapped_string_field_reset_preserves_handler(void) {
  test_mapped_string_array_stream_doc doc;
  test_array_stream_string_seen seen;
  lonejson_array_stream_string_handler handler;
  lonejson *runtime;
  lonejson_error error;

  memset(&doc, 0, sizeof(doc));
  memset(&seen, 0, sizeof(seen));
  handler = test_array_stream_string_handler();
  runtime = test_default_runtime();

  runtime->init(runtime, &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(doc.keys.set_handler(&doc.keys, &handler, &seen, &error) ==
         LONEJSON_STATUS_OK);
  runtime->reset(runtime, &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(doc.keys.set_handler == lonejson_string_array_stream_set_handler);
  EXPECT(doc.keys.handler.chunk == handler.chunk);
  EXPECT(doc.keys.user == &seen);
}

static void
test_array_stream_mapped_string_field_init_overwrites_nonzero_storage(void) {
  test_mapped_string_array_stream_doc doc;
  lonejson *runtime;

  runtime = test_default_runtime();
  poison_bytes(&doc, sizeof(doc), 0xA5u);
  runtime->init(runtime, &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(doc.keys.set_handler == lonejson_string_array_stream_set_handler);
  EXPECT(doc.keys.handler.chunk == NULL);
  EXPECT(doc.keys.user == NULL);
  EXPECT(doc.keys.active == 0);
  EXPECT(doc.cursor[0] == '\0');
  EXPECT(doc.index_seq == 0);
}

static void test_array_stream_mapped_string_field_metadata_and_long_item(void) {
  static const char prefix[] = "{\"keys\":[\"";
  static const char suffix[] =
      "\"],\"cursor\":\"done\",\"index_seq\":9,"
      "\"metadata\":{\"source\":\"index\",\"ok\":true}}";
  test_mapped_string_array_stream_doc doc;
  test_array_stream_string_seen seen;
  lonejson_array_stream_string_handler handler;
  lonejson__parse_options options;
  test_buffer_sink sink;
  test_reader_state state;
  lonejson_error error;
  unsigned char raw[96];
  char *json;
  size_t string_len = 8192u;
  size_t total = strlen(prefix) + string_len + strlen(suffix);

  json = (char *)malloc(total + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    return;
  }
  memcpy(json, prefix, strlen(prefix));
  memset(json + strlen(prefix), 'z', string_len);
  memcpy(json + strlen(prefix) + string_len, suffix, strlen(suffix));
  json[total] = '\0';

  memset(&seen, 0, sizeof(seen));
  handler = test_array_stream_string_handler();
  options = lonejson__default_parse_options();
  options.clear_destination = 0;
  lonejson_init(test_default_runtime(),
                &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(lonejson_string_array_stream_set_handler(
             &doc.keys, &handler, &seen, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_enable_parse_capture(&doc.metadata, &error) ==
         LONEJSON_STATUS_OK);
  state.json = json;
  state.offset = 0u;
  state.chunk_size = 17u;
  EXPECT(test_parse_reader(&test_mapped_string_array_stream_doc_map, &doc,
                           test_state_reader, &state, &options,
                           &error) == LONEJSON_STATUS_OK);
  EXPECT(seen.begins == 1u);
  EXPECT(seen.ends == 1u);
  EXPECT(seen.chunks > 1u);
  EXPECT(strcmp(doc.cursor, "done") == 0);
  EXPECT(doc.index_seq == 9u);

  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&doc.metadata,
                                           test_buffer_sink_write, &sink,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)raw, "{\"source\":\"index\",\"ok\":true}") == 0);

  lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
  free(json);
}

static void test_array_stream_mapped_string_field_failure_modes(void) {
  static const char *const jsons[] = {
      "{\"cursor\":\"x\",\"index_seq\":1}",
      "{\"keys\":\"not-array\",\"cursor\":\"x\",\"index_seq\":1}",
      "{\"keys\":[1],\"cursor\":\"x\",\"index_seq\":1}",
      "{\"keys\":[{\"bad\":true}],\"cursor\":\"x\",\"index_seq\":1}",
      "{\"keys\":[\"ok\"],\"cursor\":4,\"index_seq\":1}",
      "{\"keys\":[\"ok\"],\"cursor\":\"x\",\"index_seq\":1} trailing",
      "{\"keys\":[\"ok\"],\"cursor\":\"x\",\"cursor\":\"y\","
      "\"index_seq\":1}",
      "{\"keys\":[\"ok\"],\"keys\":[\"dup\"],\"cursor\":\"x\","
      "\"index_seq\":1}"};
  static const lonejson_status expected[] = {
      LONEJSON_STATUS_MISSING_REQUIRED_FIELD, LONEJSON_STATUS_TYPE_MISMATCH,
      LONEJSON_STATUS_TYPE_MISMATCH,          LONEJSON_STATUS_TYPE_MISMATCH,
      LONEJSON_STATUS_TYPE_MISMATCH,          LONEJSON_STATUS_INVALID_JSON,
      LONEJSON_STATUS_DUPLICATE_FIELD,        LONEJSON_STATUS_DUPLICATE_FIELD};
  size_t i;

  for (i = 0u; i < sizeof(jsons) / sizeof(jsons[0]); ++i) {
    test_mapped_string_array_stream_doc doc;
    test_array_stream_string_seen seen;
    lonejson_array_stream_string_handler handler =
        test_array_stream_string_handler();
    lonejson_error error;
    lonejson_status status;

    memset(&seen, 0, sizeof(seen));
    lonejson_init(test_default_runtime(),
                  &test_mapped_string_array_stream_doc_map, &doc, NULL);
    EXPECT(lonejson_string_array_stream_set_handler(
               &doc.keys, &handler, &seen, &error) == LONEJSON_STATUS_OK);
    status = test_parse_cstr(&test_mapped_string_array_stream_doc_map, &doc,
                             jsons[i], NULL, &error);
    EXPECT(status == expected[i]);
    lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
  }

  {
    test_mapped_string_array_stream_doc doc;
    test_array_stream_string_seen seen;
    lonejson_array_stream_string_handler handler =
        test_array_stream_string_handler();
    lonejson_error error;

    memset(&seen, 0, sizeof(seen));
    seen.fail_after_chunks = 1u;
    lonejson_init(test_default_runtime(),
                  &test_mapped_string_array_stream_doc_map, &doc, NULL);
    EXPECT(lonejson_string_array_stream_set_handler(
               &doc.keys, &handler, &seen, &error) == LONEJSON_STATUS_OK);
    EXPECT(test_parse_cstr(&test_mapped_string_array_stream_doc_map, &doc,
                           "{\"keys\":[\"fail\"],\"cursor\":\"x\","
                           "\"index_seq\":1}",
                           NULL, &error) == LONEJSON_STATUS_IO_ERROR);
    lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
  }
}

static void test_array_stream_mapped_string_field_serialize_rejected(void) {
  test_mapped_string_array_stream_doc doc;
  lonejson_error error;
  char buffer[64];

  lonejson_init(test_default_runtime(),
                &test_mapped_string_array_stream_doc_map, &doc, NULL);
  strcpy(doc.cursor, "x");
  doc.index_seq = 1u;
  EXPECT(test_serialize_buffer(&test_mapped_string_array_stream_doc_map, &doc,
                               buffer, sizeof(buffer), NULL, NULL,
                               &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
}

typedef struct test_mapped_stream_board {
  char name[16];
  lonejson_mapped_array_stream items;
  lonejson_int64 seq;
} test_mapped_stream_board;

typedef struct test_mapped_stream_store {
  lonejson_mapped_array_stream boards;
  char tail[16];
} test_mapped_stream_store;

typedef struct test_mapped_stream_seen {
  const test_mapped_stream_board *active_board;
  size_t item_count;
  size_t board_count;
  size_t fail_after_items;
  size_t fail_after_boards;
  char item_board_names[4][16];
  lonejson_int64 item_ids[4];
  char item_labels[4][12];
  char board_names[4][16];
  lonejson_int64 board_seqs[4];
} test_mapped_stream_seen;

typedef struct test_alloc_mapped_child {
  char *name;
  lonejson_int64 id;
} test_alloc_mapped_child;

typedef struct test_alloc_mapped_parent {
  char *name;
  lonejson_mapped_array_stream children;
  lonejson_int64 seq;
} test_alloc_mapped_parent;

typedef struct test_alloc_mapped_doc {
  lonejson_mapped_array_stream parents;
  char tail[16];
} test_alloc_mapped_doc;

typedef struct test_alloc_mapped_seen {
  size_t child_count;
  size_t parent_count;
  char child_names[4][16];
  lonejson_int64 child_ids[4];
  char parent_names[4][16];
  lonejson_int64 parent_seqs[4];
} test_alloc_mapped_seen;

static const lonejson_field test_mapped_stream_board_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_mapped_stream_board, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(test_mapped_stream_board, items,
                                           "items"),
    LONEJSON_FIELD_I64_REQ(test_mapped_stream_board, seq, "seq")};
LONEJSON_MAP_DEFINE(test_mapped_stream_board_map, test_mapped_stream_board,
                    test_mapped_stream_board_fields);

static const lonejson_field test_mapped_stream_store_fields[] = {
    LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(test_mapped_stream_store, boards,
                                           "boards"),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_mapped_stream_store, tail, "tail",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_mapped_stream_store_map, test_mapped_stream_store,
                    test_mapped_stream_store_fields);

static const lonejson_field test_alloc_mapped_child_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_alloc_mapped_child, name, "name"),
    LONEJSON_FIELD_I64_REQ(test_alloc_mapped_child, id, "id")};
LONEJSON_MAP_DEFINE(test_alloc_mapped_child_map, test_alloc_mapped_child,
                    test_alloc_mapped_child_fields);

static const lonejson_field test_alloc_mapped_parent_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_alloc_mapped_parent, name, "name"),
    LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(test_alloc_mapped_parent, children,
                                           "children"),
    LONEJSON_FIELD_I64_REQ(test_alloc_mapped_parent, seq, "seq")};
LONEJSON_MAP_DEFINE(test_alloc_mapped_parent_map, test_alloc_mapped_parent,
                    test_alloc_mapped_parent_fields);

static const lonejson_field test_alloc_mapped_doc_fields[] = {
    LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(test_alloc_mapped_doc, parents,
                                           "parents"),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_alloc_mapped_doc, tail, "tail",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_alloc_mapped_doc_map, test_alloc_mapped_doc,
                    test_alloc_mapped_doc_fields);

static lonejson_status test_mapped_stream_item(void *user, void *item,
                                               lonejson_error *error) {
  test_mapped_stream_seen *seen = (test_mapped_stream_seen *)user;
  test_item *it = (test_item *)item;
  (void)error;
  if (seen->fail_after_items != 0u &&
      seen->item_count >= seen->fail_after_items) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (seen->item_count >= 4u) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  strcpy(seen->item_board_names[seen->item_count],
         seen->active_board != NULL ? seen->active_board->name : "");
  seen->item_ids[seen->item_count] = it->id;
  strcpy(seen->item_labels[seen->item_count], it->label);
  seen->item_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_mapped_stream_board_item(void *user, void *item,
                                                     lonejson_error *error) {
  test_mapped_stream_seen *seen = (test_mapped_stream_seen *)user;
  test_mapped_stream_board *board = (test_mapped_stream_board *)item;
  (void)error;
  if (seen->fail_after_boards != 0u &&
      seen->board_count >= seen->fail_after_boards) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (seen->board_count >= 4u) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  strcpy(seen->board_names[seen->board_count], board->name);
  seen->board_seqs[seen->board_count] = board->seq;
  seen->board_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_alloc_mapped_child_item(void *user, void *item,
                                                    lonejson_error *error) {
  test_alloc_mapped_seen *seen = (test_alloc_mapped_seen *)user;
  test_alloc_mapped_child *child = (test_alloc_mapped_child *)item;
  (void)error;
  if (seen->child_count >= 4u) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  strcpy(seen->child_names[seen->child_count],
         child->name != NULL ? child->name : "");
  seen->child_ids[seen->child_count] = child->id;
  seen->child_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_alloc_mapped_parent_item(void *user, void *item,
                                                     lonejson_error *error) {
  test_alloc_mapped_seen *seen = (test_alloc_mapped_seen *)user;
  test_alloc_mapped_parent *parent = (test_alloc_mapped_parent *)item;
  (void)error;
  if (seen->parent_count >= 4u) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  strcpy(seen->parent_names[seen->parent_count],
         parent->name != NULL ? parent->name : "");
  seen->parent_seqs[seen->parent_count] = parent->seq;
  seen->parent_count++;
  return LONEJSON_STATUS_OK;
}

typedef struct test_mapped_json_value_stream_item {
  char id[16];
  lonejson_json_value payload;
} test_mapped_json_value_stream_item;

typedef struct test_mapped_json_value_stream_doc {
  lonejson_mapped_array_stream items;
  char tail[16];
} test_mapped_json_value_stream_doc;

typedef struct test_mapped_json_value_stream_seen {
  char ids[4][16];
  char payloads[4][128];
  size_t count;
  test_buffer_sink *sink;
} test_mapped_json_value_stream_seen;

static const lonejson_field test_mapped_json_value_stream_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_mapped_json_value_stream_item, id,
                                    "id", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(test_mapped_json_value_stream_item, payload,
                                  "payload")};
LONEJSON_MAP_DEFINE(test_mapped_json_value_stream_item_map,
                    test_mapped_json_value_stream_item,
                    test_mapped_json_value_stream_item_fields);

static const lonejson_field test_mapped_json_value_stream_doc_fields[] = {
    LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(test_mapped_json_value_stream_doc,
                                           items, "items"),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_mapped_json_value_stream_doc, tail,
                                    "tail", LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_mapped_json_value_stream_doc_map,
                    test_mapped_json_value_stream_doc,
                    test_mapped_json_value_stream_doc_fields);

static lonejson_status
test_mapped_json_value_stream_item_cb(void *user, void *item,
                                      lonejson_error *error) {
  test_mapped_json_value_stream_seen *seen =
      (test_mapped_json_value_stream_seen *)user;
  test_mapped_json_value_stream_item *current =
      (test_mapped_json_value_stream_item *)item;
  (void)error;

  if (seen->count >= 4u || seen->sink == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  strcpy(seen->ids[seen->count], current->id);
  strcpy(seen->payloads[seen->count], (const char *)seen->sink->buffer);
  seen->count++;
  seen->sink->length = 0u;
  seen->sink->buffer[0] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_mapped_json_value_stream_item_ok(void *user, void *item,
                                      lonejson_error *error) {
  (void)user;
  (void)item;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static void test_array_stream_mapped_field_nested_items(void) {
  static const char json[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[{\"id\":1,"
      "\"label\":\"one\"},{\"id\":2,\"label\":\"two\"}],\"seq\":7},"
      "{\"name\":\"beta\",\"items\":[{\"id\":3,\"label\":\"three\"}],"
      "\"seq\":8}],\"tail\":\"done\"}";
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  test_item item;
  test_mapped_stream_seen seen;
  lonejson_mapped_array_stream_handler board_handler;
  lonejson_mapped_array_stream_handler item_handler;
  lonejson_error error;

  memset(&seen, 0, sizeof(seen));
  memset(&board_handler, 0, sizeof(board_handler));
  memset(&item_handler, 0, sizeof(item_handler));
  memset(&store, 0, sizeof(store));
  memset(&board, 0, sizeof(board));

  lonejson_init(test_default_runtime(), &test_mapped_stream_store_map, &store,
                NULL);
  lonejson_init(test_default_runtime(), &test_mapped_stream_board_map, &board,
                NULL);
  lonejson_init(test_default_runtime(), &test_item_map, &item, NULL);

  seen.active_board = &board;
  item_handler.item_map = &test_item_map;
  item_handler.item_dst = &item;
  item_handler.item = test_mapped_stream_item;
  item_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &board.items, &item_handler, &error) == LONEJSON_STATUS_OK);

  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &store.boards, &board_handler, &error) == LONEJSON_STATUS_OK);

  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, json, NULL,
                         &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(store.tail, "done") == 0);
  EXPECT(seen.item_count == 3u);
  EXPECT(seen.board_count == 2u);
  EXPECT(strcmp(seen.item_board_names[0], "alpha") == 0);
  EXPECT(seen.item_ids[0] == 1);
  EXPECT(strcmp(seen.item_labels[1], "two") == 0);
  EXPECT(strcmp(seen.item_board_names[2], "beta") == 0);
  EXPECT(seen.item_ids[2] == 3);
  EXPECT(strcmp(seen.board_names[0], "alpha") == 0);
  EXPECT(seen.board_seqs[0] == 7);
  EXPECT(strcmp(seen.board_names[1], "beta") == 0);
  EXPECT(seen.board_seqs[1] == 8);

  lonejson_cleanup(&test_item_map, &item, NULL);
  lonejson_cleanup(&test_mapped_stream_board_map, &board, NULL);
  lonejson_cleanup(&test_mapped_stream_store_map, &store, NULL);
}

static void test_array_stream_mapped_field_zeroed_set_handler(void) {
  static const char json[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[{\"id\":1,"
      "\"label\":\"one\"}],\"seq\":7}],\"tail\":\"done\"}";
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  test_item item;
  test_mapped_stream_seen seen;
  lonejson_mapped_array_stream_handler board_handler;
  lonejson_mapped_array_stream_handler item_handler;
  lonejson_error error;

  memset(&store, 0, sizeof(store));
  memset(&board, 0, sizeof(board));
  memset(&item, 0, sizeof(item));
  memset(&seen, 0, sizeof(seen));
  memset(&board_handler, 0, sizeof(board_handler));
  memset(&item_handler, 0, sizeof(item_handler));

  seen.active_board = &board;
  item_handler.item_map = &test_item_map;
  item_handler.item_dst = &item;
  item_handler.item = test_mapped_stream_item;
  item_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &board.items, &item_handler, &error) == LONEJSON_STATUS_OK);

  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &store.boards, &board_handler, &error) == LONEJSON_STATUS_OK);

  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, json, NULL,
                         &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(store.tail, "done") == 0);
  EXPECT(seen.item_count == 1u);
  EXPECT(seen.board_count == 1u);
  EXPECT(strcmp(seen.item_board_names[0], "alpha") == 0);
  EXPECT(seen.item_ids[0] == 1);
  EXPECT(strcmp(seen.item_labels[0], "one") == 0);
  EXPECT(strcmp(seen.board_names[0], "alpha") == 0);
  EXPECT(seen.board_seqs[0] == 7);

  lonejson_cleanup(&test_item_map, &item, NULL);
  lonejson_cleanup(&test_mapped_stream_board_map, &board, NULL);
  lonejson_cleanup(&test_mapped_stream_store_map, &store, NULL);
}

static void
test_array_stream_mapped_field_lifecycle_preserves_set_handler_method(void) {
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  test_item item;
  test_mapped_stream_seen seen;
  lonejson_mapped_array_stream_handler board_handler;
  lonejson_mapped_array_stream_handler item_handler;
  lonejson *runtime;
  lonejson_error error;

  memset(&store, 0, sizeof(store));
  memset(&board, 0, sizeof(board));
  memset(&item, 0, sizeof(item));
  memset(&seen, 0, sizeof(seen));
  memset(&board_handler, 0, sizeof(board_handler));
  memset(&item_handler, 0, sizeof(item_handler));

  runtime = test_default_runtime();
  runtime->init(runtime, &test_mapped_stream_board_map, &board, NULL);
  runtime->init(runtime, &test_mapped_stream_store_map, &store, NULL);
  EXPECT(board.items.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.set_handler == lonejson_mapped_array_stream_set_handler);

  seen.active_board = &board;
  item_handler.item_map = &test_item_map;
  item_handler.item_dst = &item;
  item_handler.item = test_mapped_stream_item;
  item_handler.user = &seen;
  EXPECT(board.items.set_handler(&board.items, &item_handler, &error) ==
         LONEJSON_STATUS_OK);

  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;
  EXPECT(store.boards.set_handler(&store.boards, &board_handler, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(store.boards.handler.item == board_handler.item);
  EXPECT(board.items.handler.item == item_handler.item);

  runtime->reset(runtime, &test_mapped_stream_board_map, &board, NULL);
  runtime->reset(runtime, &test_mapped_stream_store_map, &store, NULL);
  EXPECT(board.items.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.handler.item == board_handler.item);
  EXPECT(board.items.handler.item == item_handler.item);

  runtime->cleanup(runtime, &test_mapped_stream_board_map, &board, NULL);
  runtime->cleanup(runtime, &test_mapped_stream_store_map, &store, NULL);
  EXPECT(board.items.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.handler.item == board_handler.item);
  EXPECT(board.items.handler.item == item_handler.item);

  runtime->cleanup(runtime, &test_item_map, &item, NULL);
}

static void test_array_stream_mapped_field_reset_preserves_handler(void) {
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  test_item item;
  test_mapped_stream_seen seen;
  lonejson_mapped_array_stream_handler board_handler;
  lonejson_mapped_array_stream_handler item_handler;
  lonejson *runtime;
  lonejson_error error;

  memset(&store, 0, sizeof(store));
  memset(&board, 0, sizeof(board));
  memset(&item, 0, sizeof(item));
  memset(&seen, 0, sizeof(seen));
  memset(&board_handler, 0, sizeof(board_handler));
  memset(&item_handler, 0, sizeof(item_handler));

  runtime = test_default_runtime();
  runtime->init(runtime, &test_mapped_stream_board_map, &board, NULL);
  runtime->init(runtime, &test_mapped_stream_store_map, &store, NULL);

  seen.active_board = &board;
  item_handler.item_map = &test_item_map;
  item_handler.item_dst = &item;
  item_handler.item = test_mapped_stream_item;
  item_handler.user = &seen;
  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;

  EXPECT(board.items.set_handler(&board.items, &item_handler, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(store.boards.set_handler(&store.boards, &board_handler, &error) ==
         LONEJSON_STATUS_OK);

  runtime->reset(runtime, &test_mapped_stream_board_map, &board, NULL);
  runtime->reset(runtime, &test_mapped_stream_store_map, &store, NULL);
  EXPECT(board.items.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(board.items.handler.item == item_handler.item);
  EXPECT(board.items.handler.user == &seen);
  EXPECT(store.boards.handler.item == board_handler.item);
  EXPECT(store.boards.handler.user == &seen);

  runtime->cleanup(runtime, &test_item_map, &item, NULL);
}

static void
test_array_stream_mapped_field_init_overwrites_nonzero_storage(void) {
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  lonejson *runtime;

  runtime = test_default_runtime();
  poison_bytes(&store, sizeof(store), 0xA5u);
  poison_bytes(&board, sizeof(board), 0xA5u);
  runtime->init(runtime, &test_mapped_stream_store_map, &store, NULL);
  runtime->init(runtime, &test_mapped_stream_board_map, &board, NULL);
  EXPECT(store.boards.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(store.boards.handler.item_map == NULL);
  EXPECT(store.boards.active == 0);
  EXPECT(store.tail[0] == '\0');
  EXPECT(board.items.set_handler == lonejson_mapped_array_stream_set_handler);
  EXPECT(board.items.handler.item_map == NULL);
  EXPECT(board.items.active == 0);
  EXPECT(board.name[0] == '\0');
  EXPECT(board.seq == 0);
}

static void test_array_stream_mapped_field_nested_json_value_sink_reuse(void) {
  static const char json[] =
      "{\"items\":[{\"id\":\"one\",\"payload\":{\"a\":1}},"
      "{\"id\":\"two\",\"payload\":[true,null,2]}],\"tail\":\"done\"}";
  test_mapped_json_value_stream_doc doc;
  test_mapped_json_value_stream_item item;
  test_mapped_json_value_stream_seen seen;
  lonejson_mapped_array_stream_handler handler;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char payload_buf[128];
  test_buffer_sink payload_sink;

  memset(&doc, 0, sizeof(doc));
  memset(&item, 0, sizeof(item));
  memset(&seen, 0, sizeof(seen));
  memset(&handler, 0, sizeof(handler));
  memset(&payload_sink, 0, sizeof(payload_sink));
  payload_sink.buffer = payload_buf;
  payload_sink.capacity = sizeof(payload_buf);
  seen.sink = &payload_sink;

  lonejson_init(test_default_runtime(), &test_mapped_json_value_stream_doc_map,
                &doc, NULL);
  lonejson_init(test_default_runtime(), &test_mapped_json_value_stream_item_map,
                &item, NULL);
  status = lonejson_json_value_set_parse_sink(
      &item.payload, test_buffer_sink_write, &payload_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  handler.item_map = &test_mapped_json_value_stream_item_map;
  handler.item_dst = &item;
  handler.item = test_mapped_json_value_stream_item_cb;
  handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &doc.items, &handler, &error) == LONEJSON_STATUS_OK);

  options.clear_destination = 0;
  status = test_parse_cstr(&test_mapped_json_value_stream_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.tail, "done") == 0);
  EXPECT(seen.count == 2u);
  EXPECT(strcmp(seen.ids[0], "one") == 0);
  EXPECT(strcmp(seen.payloads[0], "{\"a\":1}") == 0);
  EXPECT(strcmp(seen.ids[1], "two") == 0);
  EXPECT(strcmp(seen.payloads[1], "[true,null,2]") == 0);

  lonejson_cleanup(&test_mapped_json_value_stream_item_map, &item, NULL);
  lonejson_cleanup(&test_mapped_json_value_stream_doc_map, &doc, NULL);
}

static void
test_array_stream_mapped_field_nested_json_value_capture_budget_reuse(void) {
  static const char json[] =
      "{\"items\":[{\"id\":\"one\",\"payload\":\"a\"},"
      "{\"id\":\"two\",\"payload\":\"b\"}],\"tail\":\"done\"}";
  test_mapped_json_value_stream_doc doc;
  test_mapped_json_value_stream_item item;
  lonejson_mapped_array_stream_handler handler;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  memset(&item, 0, sizeof(item));
  memset(&handler, 0, sizeof(handler));
  test_allocator_init(&alloc);

  lonejson_init(test_default_runtime(), &test_mapped_json_value_stream_doc_map,
                &doc, NULL);
  lonejson_init(test_default_runtime(), &test_mapped_json_value_stream_item_map,
                &item, NULL);
  status = lonejson_json_value_enable_parse_capture(&item.payload, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  handler.item_map = &test_mapped_json_value_stream_item_map;
  handler.item_dst = &item;
  handler.item = test_mapped_json_value_stream_item_ok;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &doc.items, &handler, &error) == LONEJSON_STATUS_OK);

  options.clear_destination = 0;
  options.max_alloc_bytes = 16u;
  options.allocator = &alloc.allocator;
  status = test_parse_cstr(&test_mapped_json_value_stream_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.tail, "done") == 0);

  lonejson_cleanup(&test_mapped_json_value_stream_item_map, &item, NULL);
  lonejson_cleanup(&test_mapped_json_value_stream_doc_map, &doc, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_array_stream_mapped_field_failure_modes(void) {
  static const char bad_item[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[1],\"seq\":7}],"
      "\"tail\":\"done\"}";
  static const char non_array[] =
      "{\"boards\":{\"name\":\"alpha\",\"items\":[],\"seq\":7},"
      "\"tail\":\"done\"}";
  static const char missing_boards[] = "{\"tail\":\"done\"}";
  static const char duplicate_boards[] =
      "{\"boards\":[],\"boards\":[],\"tail\":\"done\"}";
  static const char duplicate_nested_items[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[],\"items\":[],"
      "\"seq\":7}],\"tail\":\"done\"}";
  static const char callback_failure[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[{\"id\":1,"
      "\"label\":\"one\"},{\"id\":2,\"label\":\"two\"}],\"seq\":7}],"
      "\"tail\":\"done\"}";
  static const char valid_after_failure[] =
      "{\"boards\":[{\"name\":\"reused\",\"items\":[{\"id\":9,"
      "\"label\":\"again\"}],\"seq\":10}],\"tail\":\"ok\"}";
  static const char missing_handler[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[],\"seq\":7}],"
      "\"tail\":\"done\"}";
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  test_item item;
  test_mapped_stream_seen seen;
  lonejson_mapped_array_stream_handler board_handler;
  lonejson_mapped_array_stream_handler item_handler;
  lonejson__parse_options options;
  lonejson_error error;
  char buffer[128];

  options = lonejson__default_parse_options();
  options.clear_destination = 0;
  memset(&seen, 0, sizeof(seen));
  memset(&board_handler, 0, sizeof(board_handler));
  memset(&item_handler, 0, sizeof(item_handler));
  EXPECT(lonejson_mapped_array_stream_set_handler(
             NULL, &item_handler, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_mapped_array_stream_set_handler(&board.items, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_init(test_default_runtime(), &test_mapped_stream_store_map, &store,
                NULL);
  lonejson_init(test_default_runtime(), &test_mapped_stream_board_map, &board,
                NULL);
  lonejson_init(test_default_runtime(), &test_item_map, &item, NULL);

  seen.active_board = &board;
  item_handler.item_map = &test_item_map;
  item_handler.item_dst = &item;
  item_handler.item = test_mapped_stream_item;
  item_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &board.items, &item_handler, &error) == LONEJSON_STATUS_OK);
  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &store.boards, &board_handler, &error) == LONEJSON_STATUS_OK);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, bad_item, NULL,
                         &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, non_array, NULL,
                         &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, missing_boards,
                         NULL,
                         &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store,
                         duplicate_boards, NULL,
                         &error) == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store,
                         duplicate_nested_items, NULL,
                         &error) == LONEJSON_STATUS_DUPLICATE_FIELD);
  memset(&seen, 0, sizeof(seen));
  seen.active_board = &board;
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, bad_item,
                         &options, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store,
                         valid_after_failure, &options,
                         &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(store.tail, "ok") == 0);
  EXPECT(seen.item_count == 1u);
  EXPECT(strcmp(seen.item_board_names[0], "reused") == 0);
  EXPECT(seen.item_ids[0] == 9);

  memset(&seen, 0, sizeof(seen));
  seen.active_board = &board;
  seen.fail_after_items = 1u;
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store,
                         callback_failure, &options,
                         &error) == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(seen.item_count == 1u);
  EXPECT(strcmp(item.label, "") == 0);
  memset(&seen, 0, sizeof(seen));
  seen.active_board = &board;
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store,
                         valid_after_failure, &options,
                         &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(store.tail, "ok") == 0);
  EXPECT(seen.item_count == 1u);
  EXPECT(seen.board_count == 1u);

  lonejson_cleanup(&test_item_map, &item, NULL);
  lonejson_cleanup(&test_mapped_stream_board_map, &board, NULL);
  lonejson_cleanup(&test_mapped_stream_store_map, &store, NULL);

  memset(&store, 0, sizeof(store));
  memset(&board, 0, sizeof(board));
  lonejson_init(test_default_runtime(), &test_mapped_stream_store_map, &store,
                NULL);
  lonejson_init(test_default_runtime(), &test_mapped_stream_board_map, &board,
                NULL);
  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &store.boards, &board_handler, &error) == LONEJSON_STATUS_OK);
  EXPECT(test_parse_cstr(&test_mapped_stream_store_map, &store, missing_handler,
                         NULL, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  EXPECT(test_serialize_buffer(&test_mapped_stream_store_map, &store, buffer,
                               sizeof(buffer), NULL, NULL,
                               &error) == LONEJSON_STATUS_TYPE_MISMATCH);

  lonejson_cleanup(&test_mapped_stream_board_map, &board, NULL);
  lonejson_cleanup(&test_mapped_stream_store_map, &store, NULL);
}

static void test_array_stream_mapped_field_failure_cleans_alloc_items(void) {
  static const char missing_child_id[] =
      "{\"parents\":[{\"name\":\"outer\",\"children\":[{\"name\":\"inner\"}],"
      "\"seq\":1}],\"tail\":\"bad\"}";
  static const char child_type_mismatch[] =
      "{\"parents\":[{\"name\":\"outer\",\"children\":[{\"name\":\"inner\","
      "\"id\":\"bad\"}],\"seq\":1}],\"tail\":\"bad\"}";
  static const char valid_after_failure[] =
      "{\"parents\":[{\"name\":\"outer-ok\",\"children\":[{\"name\":\"inner-"
      "ok\","
      "\"id\":2}],\"seq\":3}],\"tail\":\"ok\"}";
  test_alloc_mapped_doc doc;
  test_alloc_mapped_parent parent;
  test_alloc_mapped_child child;
  test_alloc_mapped_seen seen;
  test_allocator_state alloc_state;
  lonejson_mapped_array_stream_handler parent_handler;
  lonejson_mapped_array_stream_handler child_handler;
  lonejson__parse_options options;
  lonejson_error error;

  memset(&seen, 0, sizeof(seen));
  memset(&parent_handler, 0, sizeof(parent_handler));
  memset(&child_handler, 0, sizeof(child_handler));
  test_allocator_init(&alloc_state);
  options = lonejson__default_parse_options();
  options.clear_destination = 0;
  options.allocator = &alloc_state.allocator;

  lonejson_init(test_default_runtime(), &test_alloc_mapped_doc_map, &doc, NULL);
  lonejson_init(test_default_runtime(), &test_alloc_mapped_parent_map, &parent,
                NULL);
  lonejson_init(test_default_runtime(), &test_alloc_mapped_child_map, &child,
                NULL);

  child_handler.item_map = &test_alloc_mapped_child_map;
  child_handler.item_dst = &child;
  child_handler.item = test_alloc_mapped_child_item;
  child_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &parent.children, &child_handler, &error) == LONEJSON_STATUS_OK);

  parent_handler.item_map = &test_alloc_mapped_parent_map;
  parent_handler.item_dst = &parent;
  parent_handler.item = test_alloc_mapped_parent_item;
  parent_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &doc.parents, &parent_handler, &error) == LONEJSON_STATUS_OK);

  EXPECT(test_parse_cstr(&test_alloc_mapped_doc_map, &doc, missing_child_id,
                         &options,
                         &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(alloc_state.stats.bytes_live == 0u);
  EXPECT(parent.name == NULL);
  EXPECT(child.name == NULL);

  EXPECT(test_parse_cstr(&test_alloc_mapped_doc_map, &doc, child_type_mismatch,
                         &options, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(alloc_state.stats.bytes_live == 0u);
  EXPECT(parent.name == NULL);
  EXPECT(child.name == NULL);

  memset(&seen, 0, sizeof(seen));
  EXPECT(test_parse_cstr(&test_alloc_mapped_doc_map, &doc, valid_after_failure,
                         &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(alloc_state.stats.bytes_live == 0u);
  EXPECT(strcmp(doc.tail, "ok") == 0);
  EXPECT(seen.child_count == 1u);
  EXPECT(seen.parent_count == 1u);
  EXPECT(strcmp(seen.child_names[0], "inner-ok") == 0);
  EXPECT(seen.child_ids[0] == 2);
  EXPECT(strcmp(seen.parent_names[0], "outer-ok") == 0);
  EXPECT(seen.parent_seqs[0] == 3);

  lonejson_cleanup(&test_alloc_mapped_child_map, &child, NULL);
  lonejson_cleanup(&test_alloc_mapped_parent_map, &parent, NULL);
  lonejson_cleanup(&test_alloc_mapped_doc_map, &doc, NULL);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

#ifdef LONEJSON_WITH_CURL
static void test_array_stream_mapped_field_curl_facade(void) {
  static const char json[] =
      "{\"boards\":[{\"name\":\"alpha\",\"items\":[{\"id\":1,"
      "\"label\":\"one\"}],\"seq\":7},{\"name\":\"beta\",\"items\":[],"
      "\"seq\":8}],\"tail\":\"done\"}";
  test_mapped_stream_store store;
  test_mapped_stream_board board;
  test_item item;
  test_mapped_stream_seen seen;
  lonejson_mapped_array_stream_handler board_handler;
  lonejson_mapped_array_stream_handler item_handler;
  lonejson_curl_parse parse = {0};
  lonejson_error error;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  memset(&board_handler, 0, sizeof(board_handler));
  memset(&item_handler, 0, sizeof(item_handler));
  memset(&store, 0, sizeof(store));
  memset(&board, 0, sizeof(board));
  lonejson_init(test_default_runtime(), &test_mapped_stream_store_map, &store,
                NULL);
  lonejson_init(test_default_runtime(), &test_mapped_stream_board_map, &board,
                NULL);
  lonejson_init(test_default_runtime(), &test_item_map, &item, NULL);

  seen.active_board = &board;
  item_handler.item_map = &test_item_map;
  item_handler.item_dst = &item;
  item_handler.item = test_mapped_stream_item;
  item_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &board.items, &item_handler, &error) == LONEJSON_STATUS_OK);
  board_handler.item_map = &test_mapped_stream_board_map;
  board_handler.item_dst = &board;
  board_handler.item = test_mapped_stream_board_item;
  board_handler.user = &seen;
  EXPECT(lonejson_mapped_array_stream_set_handler(
             &store.boards, &board_handler, &error) == LONEJSON_STATUS_OK);

  EXPECT(lonejson_curl_parse_init(&parse, test_default_runtime(),
                                  &test_mapped_stream_store_map,
                                  &store) == LONEJSON_STATUS_OK);
  for (i = 0u; i < strlen(json); ++i) {
    EXPECT(lonejson_curl_write_callback((char *)json + i, 1u, 1u, &parse) ==
           1u);
  }
  EXPECT(lonejson_curl_parse_finish(&parse) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(store.tail, "done") == 0);
  EXPECT(seen.item_count == 1u);
  EXPECT(seen.board_count == 2u);
  EXPECT(strcmp(seen.item_board_names[0], "alpha") == 0);
  EXPECT(seen.item_ids[0] == 1);
  EXPECT(strcmp(seen.board_names[1], "beta") == 0);
  EXPECT(seen.board_seqs[1] == 8);

  lonejson_curl_parse_cleanup(&parse);
  lonejson_cleanup(&test_item_map, &item, NULL);
  lonejson_cleanup(&test_mapped_stream_board_map, &board, NULL);
  lonejson_cleanup(&test_mapped_stream_store_map, &store, NULL);
}
#else
static void test_array_stream_mapped_field_curl_facade(void) {}
#endif

#ifdef LONEJSON_WITH_CURL
static void test_array_stream_mapped_string_field_curl_facade(void) {
  static const char json[] =
      "{\"keys\":[\"a\",\"b\"],\"cursor\":\"c\",\"index_seq\":44}";
  test_mapped_string_array_stream_doc doc;
  test_array_stream_string_seen seen;
  lonejson_array_stream_string_handler handler;
  lonejson_curl_parse parse = {0};
  lonejson_error error;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  handler = test_array_stream_string_handler();
  lonejson_init(test_default_runtime(),
                &test_mapped_string_array_stream_doc_map, &doc, NULL);
  EXPECT(lonejson_string_array_stream_set_handler(
             &doc.keys, &handler, &seen, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_parse_init(&parse, test_default_runtime(),
                                  &test_mapped_string_array_stream_doc_map,
                                  &doc) == LONEJSON_STATUS_OK);
  for (i = 0u; i < strlen(json); ++i) {
    EXPECT(lonejson_curl_write_callback((char *)json + i, 1u, 1u, &parse) ==
           1u);
  }
  EXPECT(lonejson_curl_parse_finish(&parse) == LONEJSON_STATUS_OK);
  EXPECT(seen.ends == 2u);
  EXPECT(strcmp(seen.items[0], "a") == 0);
  EXPECT(strcmp(seen.items[1], "b") == 0);
  EXPECT(strcmp(doc.cursor, "c") == 0);
  EXPECT(doc.index_seq == 44u);
  lonejson_curl_parse_cleanup(&parse);
  lonejson_cleanup(&test_mapped_string_array_stream_doc_map, &doc, NULL);
}
#else
static void test_array_stream_mapped_string_field_curl_facade(void) {}
#endif
