static void test_spooled_fields_roundtrip(void) {
  unsigned char raw_bytes[180];
  unsigned char read_back[256];
  test_spool_doc doc;
  test_spool_doc roundtrip;
  lonejson_error error;
  lonejson_status status;
  char *text;
  char *base64_text;
  char *json;
  char *serialized;
  size_t serialized_len;
  size_t read_len;
  size_t i;

  for (i = 0u; i < sizeof(raw_bytes); ++i) {
    raw_bytes[i] = (unsigned char)((i * 13u) & 0xFFu);
  }
  text = make_repeating_text(220u);
  base64_text = base64_encode_bytes(raw_bytes, sizeof(raw_bytes));
  json = make_spool_json("spool-1", text, base64_text);
  EXPECT(text != NULL);
  EXPECT(base64_text != NULL);
  EXPECT(json != NULL);
  if (text == NULL || base64_text == NULL || json == NULL) {
    free(text);
    free(base64_text);
    free(json);
    return;
  }

  test_init_map(&test_spool_doc_map, &doc);
  status = test_parse_cstr(&test_spool_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.id, "spool-1") == 0);
  EXPECT(lonejson_spooled_size(&doc.text) == strlen(text));
  EXPECT(lonejson_spooled_size(&doc.bytes) == sizeof(raw_bytes));
  EXPECT(lonejson_spooled_spilled(&doc.text) != 0);
  EXPECT(lonejson_spooled_spilled(&doc.bytes) != 0);

  read_len = read_spooled_all(&doc.text, read_back, sizeof(read_back));
  EXPECT(read_len == strlen(text));
  EXPECT(memcmp(read_back, text, read_len) == 0);

  memset(read_back, 0, sizeof(read_back));
  read_len = read_spooled_all(&doc.bytes, read_back, sizeof(read_back));
  EXPECT(read_len == sizeof(raw_bytes));
  EXPECT(memcmp(read_back, raw_bytes, sizeof(raw_bytes)) == 0);

  serialized = test_serialize_alloc(&test_spool_doc_map, &doc, &serialized_len,
                                    NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(serialized_len > strlen(text));
    EXPECT(test_validate_cstr(serialized, &error) == LONEJSON_STATUS_OK);
    test_expect_compact_buffer_matches_sink(&test_spool_doc_map, &doc,
                                            serialized);
    test_init_map(&test_spool_doc_map, &roundtrip);
    status = test_parse_cstr(&test_spool_doc_map, &roundtrip, serialized, NULL,
                             &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(lonejson_spooled_size(&roundtrip.text) == strlen(text));
    EXPECT(lonejson_spooled_size(&roundtrip.bytes) == sizeof(raw_bytes));
    lonejson_cleanup(&test_spool_doc_map, &roundtrip, NULL);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_spool_doc_map, &doc, NULL);
  free(text);
  free(base64_text);
  free(json);
}

static void test_spooled_fields_small_and_null(void) {
  static const char *json =
      "{\"id\":\"tiny\",\"text\":\"hello\",\"bytes\":\"AQID\"}";
  static const char *null_json =
      "{\"id\":\"tiny\",\"text\":null,\"bytes\":null}";
  test_spool_doc doc;
  lonejson_error error;
  lonejson_status status;
  unsigned char read_back[16];
  size_t read_len;

  reset_lonejson_alloc_stats();
  test_init_map(&test_spool_doc_map, &doc);
  status = test_parse_cstr(&test_spool_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_spilled(&doc.text) == 0);
  EXPECT(lonejson_spooled_spilled(&doc.bytes) == 0);
  EXPECT(lonejson_spooled_size(&doc.text) == 5u);
  EXPECT(lonejson_spooled_size(&doc.bytes) == 3u);
  read_len = read_spooled_all(&doc.bytes, read_back, sizeof(read_back));
  EXPECT(read_len == 3u);
  EXPECT(read_back[0] == 1u && read_back[1] == 2u && read_back[2] == 3u);

  status = test_parse_cstr(&test_spool_doc_map, &doc, null_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&doc.text) == 0u);
  EXPECT(lonejson_spooled_size(&doc.bytes) == 0u);
  lonejson_cleanup(&test_spool_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_spooled_append_api(void) {
  lonejson__spool_options options;
  lonejson_spooled value;
  lonejson_error error;
  unsigned char read_back[32];
  lonejson_read_result chunk;
  size_t read_len;
  size_t total;

  options = lonejson__default_spool_options();
  options.memory_limit = 4u;
  options.max_bytes = 0u;
  options.temp_dir = NULL;
  lonejson_error_init(&error);
  lonejson_spooled_init_with_allocator(&value, &options, NULL);

  EXPECT(lonejson_spooled_append(&value, "ab", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&value, "cdef", 4u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 6u);
  EXPECT(lonejson_spooled_spilled(&value) != 0);

  read_len = read_spooled_all(&value, read_back, sizeof(read_back));
  EXPECT(read_len == 6u);
  EXPECT(memcmp(read_back, "abcdef", 6u) == 0);

  EXPECT(lonejson_spooled_rewind(&value, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&value, "gh", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 8u);

  read_len = read_spooled_all(&value, read_back, sizeof(read_back));
  EXPECT(read_len == 8u);
  EXPECT(memcmp(read_back, "abcdefgh", 8u) == 0);

  EXPECT(lonejson_spooled_rewind(&value, &error) == LONEJSON_STATUS_OK);
  chunk = lonejson_spooled_read(&value, read_back, 4u);
  EXPECT(chunk.error_code == 0);
  EXPECT(chunk.bytes_read == 4u);
  EXPECT(chunk.eof == 0);
  chunk = lonejson_spooled_read(&value, read_back + 4u, 1u);
  EXPECT(chunk.error_code == 0);
  EXPECT(chunk.bytes_read == 1u);
  EXPECT(chunk.eof == 0);
  EXPECT(memcmp(read_back, "abcde", 5u) == 0);
  EXPECT(lonejson_spooled_append(&value, "ij", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 10u);

  total = 5u;
  while (total < sizeof(read_back)) {
    chunk = lonejson_spooled_read(&value, read_back + total,
                                  sizeof(read_back) - total);
    EXPECT(chunk.error_code == 0);
    total += chunk.bytes_read;
    if (chunk.eof) {
      break;
    }
  }
  EXPECT(total == 10u);
  EXPECT(memcmp(read_back, "abcdefghij", 10u) == 0);

  lonejson_spooled_cleanup(&value);

  options.memory_limit = 2u;
  options.max_bytes = 4u;
  lonejson_spooled_init_with_allocator(&value, &options, NULL);
  EXPECT(lonejson_spooled_append(NULL, "x", 1u, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_spooled_append(&value, NULL, 1u, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_spooled_append(&value, NULL, 0u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&value, "abcd", 4u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 4u);
  EXPECT(lonejson_spooled_append(&value, "e", 1u, &error) ==
         LONEJSON_STATUS_OVERFLOW);
  EXPECT(lonejson_spooled_size(&value) == 4u);
  lonejson_spooled_cleanup(&value);
}

static void test_spooled_field_failures(void) {
  static const char *bad_base64 =
      "{\"id\":\"bad\",\"text\":\"ok\",\"bytes\":\"###\"}";
  char *too_large_text;
  char *too_large_json;
  test_spool_doc doc;
  test_spool_limits_doc limited;
  lonejson_error error;
  lonejson_status status;

  test_init_map(&test_spool_doc_map, &doc);
  status = test_parse_cstr(&test_spool_doc_map, &doc, bad_base64, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  lonejson_cleanup(&test_spool_doc_map, &doc, NULL);

  too_large_text = make_repeating_text(140u);
  EXPECT(too_large_text != NULL);
  if (too_large_text == NULL) {
    return;
  }
  {
    const char *fmt = "{\"text\":\"%s\"}";
    size_t need = snprintf(NULL, 0, fmt, too_large_text);
    too_large_json = (char *)malloc(need + 1u);
    EXPECT(too_large_json != NULL);
    if (too_large_json == NULL) {
      free(too_large_text);
      return;
    }
    snprintf(too_large_json, need + 1u, fmt, too_large_text);
  }
  test_init_map(&test_spool_limits_doc_map, &limited);
  status = test_parse_cstr(&test_spool_limits_doc_map, &limited, too_large_json,
                           NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  lonejson_cleanup(&test_spool_limits_doc_map, &limited, NULL);
  free(too_large_text);
  free(too_large_json);
}

static void test_source_fields_path_and_raw_sink(void) {
  static const char text_payload[] = "alpha \"quoted\"\nline\\tab\tend";
  static const unsigned char bytes_payload[] = {0x00u, 0x01u, 0x7fu,
                                                0x80u, 0xffu, 0x42u};
  test_source_doc doc;
  lonejson_error error;
  lonejson_status status;
  char text_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-source-text-XXXXXX";
  char bytes_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-source-bytes-XXXXXX";
  int text_fd;
  int bytes_fd;
  char *json;
  char *encoded;
  size_t expected_len;
  char serialized[256];
  unsigned char raw_buffer[128];
  test_buffer_sink sink;

  lonejson_init(test_default_runtime(), &test_source_doc_map, &doc, NULL);
  strcpy(doc.id, "src-1");
  text_fd = mkstemp(text_path);
  EXPECT(text_fd >= 0);
  if (text_fd < 0) {
    return;
  }
  bytes_fd = mkstemp(bytes_path);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    close(text_fd);
    unlink(text_path);
    return;
  }
  EXPECT(write(text_fd, text_payload, sizeof(text_payload) - 1u) ==
         (ssize_t)(sizeof(text_payload) - 1u));
  EXPECT(write(bytes_fd, bytes_payload, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  close(text_fd);
  close(bytes_fd);

  status = lonejson_source_set_path(&doc.text, text_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_source_set_path(&doc.bytes, bytes_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  encoded = base64_encode_bytes(bytes_payload, sizeof(bytes_payload));
  EXPECT(encoded != NULL);
  if (encoded == NULL) {
    lonejson_cleanup(&test_source_doc_map, &doc, NULL);
    unlink(text_path);
    unlink(bytes_path);
    return;
  }
  expected_len = snprintf(NULL, 0,
                          "{\"id\":\"src-1\",\"text\":\"alpha "
                          "\\\"quoted\\\"\\nline\\\\tab\\tend\","
                          "\"bytes\":\"%s\"}",
                          encoded);
  json = (char *)malloc(expected_len + 1u);
  EXPECT(json != NULL);
  if (json != NULL) {
    snprintf(json, expected_len + 1u,
             "{\"id\":\"src-1\",\"text\":\"alpha "
             "\\\"quoted\\\"\\nline\\\\tab\\tend\","
             "\"bytes\":\"%s\"}",
             encoded);
    test_expect_compact_buffer_matches_sink(&test_source_doc_map, &doc, json);
    EXPECT(test_serialize_buffer(&test_source_doc_map, &doc, serialized,
                                 sizeof(raw_buffer), NULL, NULL,
                                 &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp(serialized, json) == 0);
  }

  memset(&sink, 0, sizeof(sink));
  sink.buffer = raw_buffer;
  sink.capacity = sizeof(raw_buffer);
  status = lonejson_source_write_to_sink(&doc.text, test_buffer_sink_write,
                                         &sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)raw_buffer, text_payload) == 0);

  lonejson_cleanup(&test_source_doc_map, &doc, NULL);
  unlink(text_path);
  unlink(bytes_path);
  free(encoded);
  free(json);
}

static void test_source_fields_file_and_fd(void) {
  static const char text_payload[] = "streamed from FILE*";
  static const unsigned char bytes_payload[] = {0xdeu, 0xadu, 0xbeu, 0xefu};
  test_source_doc doc;
  lonejson_error error;
  lonejson_status status;
  char text_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-source-file-XXXXXX";
  char bytes_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-source-fd-XXXXXX";
  int text_fd;
  int bytes_fd;
  FILE *fp;
  unsigned char check[32];
  char *json;
  char *encoded;
  size_t expected_len;
  char serialized[256];

  lonejson_init(test_default_runtime(), &test_source_doc_map, &doc, NULL);
  strcpy(doc.id, "src-2");
  text_fd = mkstemp(text_path);
  EXPECT(text_fd >= 0);
  if (text_fd < 0) {
    return;
  }
  bytes_fd = mkstemp(bytes_path);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    close(text_fd);
    unlink(text_path);
    return;
  }
  EXPECT(write(text_fd, text_payload, sizeof(text_payload) - 1u) ==
         (ssize_t)(sizeof(text_payload) - 1u));
  EXPECT(write(bytes_fd, bytes_payload, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  close(text_fd);
  close(bytes_fd);

  fp = fopen(text_path, "rb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    unlink(text_path);
    unlink(bytes_path);
    return;
  }
  bytes_fd = open(bytes_path, O_RDONLY);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    fclose(fp);
    unlink(text_path);
    unlink(bytes_path);
    return;
  }

  status = lonejson_source_set_file(&doc.text, fp, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_source_set_fd(&doc.bytes, bytes_fd, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  encoded = base64_encode_bytes(bytes_payload, sizeof(bytes_payload));
  EXPECT(encoded != NULL);
  if (encoded != NULL) {
    expected_len =
        snprintf(NULL, 0, "{\"id\":\"src-2\",\"text\":\"%s\",\"bytes\":\"%s\"}",
                 text_payload, encoded);
    json = (char *)malloc(expected_len + 1u);
    EXPECT(json != NULL);
    if (json != NULL) {
      snprintf(json, expected_len + 1u,
               "{\"id\":\"src-2\",\"text\":\"%s\",\"bytes\":\"%s\"}",
               text_payload, encoded);
      status = test_serialize_buffer(&test_source_doc_map, &doc, serialized,
                                     sizeof(serialized), NULL, NULL, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(strcmp(serialized, json) == 0);
      free(json);
    }
    free(encoded);
  }

  EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
  EXPECT(fread(check, 1u, sizeof(text_payload) - 1u, fp) ==
         sizeof(text_payload) - 1u);
  EXPECT(memcmp(check, text_payload, sizeof(text_payload) - 1u) == 0);

  EXPECT(lseek(bytes_fd, 0, SEEK_SET) == 0);
  EXPECT(read(bytes_fd, check, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  EXPECT(memcmp(check, bytes_payload, sizeof(bytes_payload)) == 0);

  lonejson_cleanup(&test_source_doc_map, &doc, NULL);
  fclose(fp);
  close(bytes_fd);
  unlink(text_path);
  unlink(bytes_path);
}

static void test_source_field_parse_and_seek_failures(void) {
  static const char *null_json =
      "{\"id\":\"src-3\",\"text\":null,\"bytes\":null}";
  static const char *bad_json = "{\"id\":\"src-3\",\"text\":\"inline\"}";
  test_source_doc doc;
  test_counting_sink sink;
  lonejson_error error;
  lonejson_status status;
  int pipe_fds[2];

  status = test_parse_cstr(&test_source_doc_map, &doc, null_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.text.kind == LONEJSON_SOURCE_NONE);
  EXPECT(doc.bytes.kind == LONEJSON_SOURCE_NONE);
  lonejson_cleanup(&test_source_doc_map, &doc, NULL);

  status = test_parse_cstr(&test_source_doc_map, &doc, bad_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_cleanup(&test_source_doc_map, &doc, NULL);

  if (pipe(pipe_fds) != 0) {
    EXPECT(0);
    return;
  }
  EXPECT(write(pipe_fds[1], "pipe-bytes", 10u) == 10);
  close(pipe_fds[1]);
  memset(&sink, 0, sizeof(sink));
  lonejson_source_init(&doc.text);
  status = lonejson_source_set_fd(&doc.text, pipe_fds[0], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_source_write_to_sink(&doc.text, test_counting_sink_write,
                                         &sink, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  lonejson_source_cleanup(&doc.text);
  close(pipe_fds[0]);
}

static void test_source_fields_do_not_mutate_sink_on_open_failure(void) {
  lonejson_source source;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  char out[64];

  lonejson_source_init(&source);
  status = lonejson_source_set_path(
      &source, LONEJSON_SOURCE_DIR "/build/test-tmp/does-not-exist-lonejson",
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  memset(&sink, 0, sizeof(sink));
  memset(out, 0, sizeof(out));
  sink.buffer = (unsigned char *)out;
  sink.capacity = sizeof(out);
  status = lonejson__serialize_source_text(&source, test_buffer_sink_write,
                                           &sink, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  EXPECT(sink.length == 0u);
  EXPECT(out[0] == '\0');

  status = lonejson__serialize_source_base64(&source, test_buffer_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  EXPECT(sink.length == 0u);
  EXPECT(out[0] == '\0');

  lonejson_source_cleanup(&source);
}

static void test_json_value_parse_and_roundtrip(void) {
  const char *json =
      "{"
      "\"id\":\"lql-1\","
      "\"selector\":{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"},"
      "\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}"
      "}";
  const char *expected_compact =
      "{\"id\":\"lql-1\",\"selector\":{\"kind\":\"term\",\"field\":\"name\","
      "\"value\":\"alice\"},\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}}";
  test_json_value_doc doc;
  test_json_value_doc streamed;
  test_reader_state stream_state;
  lonejson__parse_options parse_options = lonejson__default_parse_options();
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_stream *stream;
  lonejson_stream_result result;
  char *serialized;

  poison_bytes(&doc, sizeof(doc), 0xA5u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  parse_options.clear_destination = 0;
  status = test_parse_cstr(&test_json_value_doc_map, &doc, json, &parse_options,
                           &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.selector.json,
                "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}") ==
         0);
  EXPECT(strcmp(doc.fields.json, "[\"id\",\"name\",\"created_at\"]") == 0);
  EXPECT(
      strcmp(doc.last_error.json,
             "{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/name\"}}") ==
      0);

  serialized =
      test_serialize_alloc(&test_json_value_doc_map, &doc, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, expected_compact) == 0);
    test_expect_compact_buffer_matches_sink(&test_json_value_doc_map, &doc,
                                            expected_compact);
    LONEJSON_FREE(serialized);
  }

  options.pretty = 1;
  serialized = test_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                    &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(test_validate_cstr(serialized, &error) == LONEJSON_STATUS_OK);
    EXPECT(strstr(serialized, "\n  \"selector\": {\n") != NULL);
    EXPECT(strstr(serialized, "\"kind\": \"term\"") != NULL);
    EXPECT(strstr(serialized, "\n  \"fields\": [\n") != NULL);
    EXPECT(strstr(serialized, "\"created_at\"") != NULL);
    EXPECT(strstr(serialized, "\n  \"last_error\": {\n") != NULL);
    LONEJSON_FREE(serialized);
  }

  stream_state.json = json;
  stream_state.offset = 0u;
  stream_state.chunk_size = 5u;
  stream = test_stream_open_reader(&test_json_value_doc_map, test_state_reader,
                                   &stream_state, &parse_options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    poison_bytes(&streamed, sizeof(streamed), 0x5Au);
    test_init_map(&test_json_value_doc_map, &streamed);
    status =
        lonejson_json_value_enable_parse_capture(&streamed.selector, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_json_value_enable_parse_capture(&streamed.fields, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        lonejson_json_value_enable_parse_capture(&streamed.last_error, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    result = lonejson_stream_next(stream, &streamed, &error);
    EXPECT(result == LONEJSON_STREAM_OBJECT);
    if (result == LONEJSON_STREAM_OBJECT) {
      EXPECT(strcmp(streamed.selector.json, doc.selector.json) == 0);
      lonejson_cleanup(&test_json_value_doc_map, &streamed, NULL);
    }
    lonejson_stream_close(stream);
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_parse_capture_escapes(void) {
  const char *json =
      "{"
      "\"id\":\"escape\","
      "\"selector\":{\"te\\u0078t\":\"line\\n\\t\\\"\\\\\\u0001\\u0061 "
      "\xc3\xa9\"},"
      "\"fields\":[\"quote\\\"\",\"slash\\\\\",\"ctrl\\u0002\"]"
      "}";
  const char *expected_selector =
      "{\"text\":\"line\\n\\t\\\"\\\\\\u0001a \xc3\xa9\"}";
  const char *expected_fields = "[\"quote\\\"\",\"slash\\\\\",\"ctrl\\u0002\"]";
  test_json_value_doc doc;
  test_json_value_doc streamed;
  test_reader_state stream_state;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_stream *stream;
  lonejson_stream_result result;
  size_t chunk_size;

  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.selector.json, expected_selector) == 0);
  EXPECT(strcmp(doc.fields.json, expected_fields) == 0);

  for (chunk_size = 1u; chunk_size <= strlen(json); ++chunk_size) {
    stream_state.json = json;
    stream_state.offset = 0u;
    stream_state.chunk_size = chunk_size;
    stream =
        test_stream_open_reader(&test_json_value_doc_map, test_state_reader,
                                &stream_state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      test_init_map(&test_json_value_doc_map, &streamed);
      status =
          lonejson_json_value_enable_parse_capture(&streamed.selector, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      status =
          lonejson_json_value_enable_parse_capture(&streamed.fields, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      result = lonejson_stream_next(stream, &streamed, &error);
      EXPECT(result == LONEJSON_STREAM_OBJECT);
      if (result == LONEJSON_STREAM_OBJECT) {
        EXPECT(strcmp(streamed.selector.json, expected_selector) == 0);
        EXPECT(strcmp(streamed.fields.json, expected_fields) == 0);
      }
      lonejson_cleanup(&test_json_value_doc_map, &streamed, NULL);
      lonejson_stream_close(stream);
    }
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_stream_reuse_releases_capture_budget(void) {
  test_reader_state state = {
      "{\"id\":\"one\",\"selector\":\"a\"}{\"id\":\"two\",\"selector\":\"b\"}",
      0u, 2u};
  test_json_value_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_status status;
  lonejson_stream_result result;
  char fixed_scratch[16];

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.max_alloc_bytes = 16u;
  options.allocator = &alloc.allocator;
  options.fixed_string_scratch = fixed_scratch;
  options.fixed_string_scratch_size = sizeof(fixed_scratch);

  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  stream = test_stream_open_reader(&test_json_value_doc_map, test_framed_reader,
                                   &state, &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
    return;
  }

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.id, "one") == 0);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.selector.json, "\"a\"") == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.id, "two") == 0);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.selector.json, "\"b\"") == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_json_value_capture_small_budget_uses_required_capacity(void) {
  const char *json =
      "{\"id\":\"low\",\"selector\":1,\"fields\":2,\"last_error\":3}";
  test_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  lonejson__parse_options options = lonejson__default_parse_options();

  options.clear_destination = 0;
  options.max_alloc_bytes = 12u;
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.json != NULL && strcmp(doc.selector.json, "1") == 0);
  EXPECT(doc.fields.json != NULL && strcmp(doc.fields.json, "2") == 0);
  EXPECT(doc.last_error.json != NULL && strcmp(doc.last_error.json, "3") == 0);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_nested_json_value_stream_reuse_releases_capture_budget(void) {
  test_reader_state state = {
      "{\"type\":\"one\",\"response\":{\"payload\":\"a\"}}"
      "{\"type\":\"two\",\"response\":{\"payload\":\"b\"}}",
      0u, 3u};
  test_nested_json_value_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_status status;
  lonejson_stream_result result;
  char fixed_scratch[32];

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.max_alloc_bytes = 16u;
  options.allocator = &alloc.allocator;
  options.fixed_string_scratch = fixed_scratch;
  options.fixed_string_scratch_size = sizeof(fixed_scratch);

  test_init_map(&test_nested_json_value_doc_map, &doc);
  status =
      lonejson_json_value_enable_parse_capture(&doc.response.payload, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  stream =
      test_stream_open_reader(&test_nested_json_value_doc_map,
                              test_framed_reader, &state, &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
    return;
  }

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.type, "one") == 0);
  EXPECT(strcmp(doc.response.payload.json, "\"a\"") == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.type, "two") == 0);
  EXPECT(strcmp(doc.response.payload.json, "\"b\"") == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_json_value_parse_stream_sink(void) {
  const char *json =
      "{"
      "\"id\":\"sink-1\","
      "\"selector\":{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"},"
      "\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}"
      "}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char selector_buf[128];
  unsigned char fields_buf[128];
  unsigned char error_buf[128];
  test_buffer_sink selector_sink;
  test_buffer_sink fields_sink;
  test_buffer_sink error_sink;

  memset(&selector_sink, 0, sizeof(selector_sink));
  memset(&fields_sink, 0, sizeof(fields_sink));
  memset(&error_sink, 0, sizeof(error_sink));
  selector_sink.buffer = selector_buf;
  selector_sink.capacity = sizeof(selector_buf);
  fields_sink.buffer = fields_buf;
  fields_sink.capacity = sizeof(fields_buf);
  error_sink.buffer = error_buf;
  error_sink.capacity = sizeof(error_buf);

  poison_bytes(&doc, sizeof(doc), 0xCCu);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_sink(
      &doc.selector, test_buffer_sink_write, &selector_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_sink(
      &doc.fields, test_buffer_sink_write, &fields_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_sink(
      &doc.last_error, test_buffer_sink_write, &error_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)selector_buf,
                "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}") ==
         0);
  EXPECT(strcmp((const char *)fields_buf, "[\"id\",\"name\",\"created_at\"]") ==
         0);
  EXPECT(
      strcmp((const char *)error_buf,
             "{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/name\"}}") ==
      0);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.fields.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.last_error.kind == LONEJSON_JSON_VALUE_NULL);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_nested_object_parse_sink_and_reuse(void) {
  const char *json =
      "{\"type\":\"response.completed\","
      "\"response\":{\"status\":\"ok\",\"payload\":{\"a\":[1,2,3],"
      "\"b\":\"large\"}}}";
  const char *json_reuse = "{\"type\":\"response.delta\","
                           "\"response\":{\"payload\":{\"second\":true}}}";
  test_nested_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char payload_buf[128];
  test_buffer_sink payload_sink;

  memset(&payload_sink, 0, sizeof(payload_sink));
  payload_sink.buffer = payload_buf;
  payload_sink.capacity = sizeof(payload_buf);

  poison_bytes(&doc, sizeof(doc), 0xC1u);
  test_init_map(&test_nested_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_sink(
      &doc.response.payload, test_buffer_sink_write, &payload_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;

  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.type, "response.completed") == 0);
  EXPECT(strcmp(doc.response.status, "ok") == 0);
  EXPECT(strcmp((const char *)payload_buf, "{\"a\":[1,2,3],\"b\":\"large\"}") ==
         0);
  EXPECT(doc.response.payload.kind == LONEJSON_JSON_VALUE_NULL);

  payload_sink.length = 0u;
  payload_buf[0] = '\0';
  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json_reuse,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.type, "response.delta") == 0);
  EXPECT(doc.response.status[0] == '\0');
  EXPECT(strcmp((const char *)payload_buf, "{\"second\":true}") == 0);
  EXPECT(doc.response.payload.kind == LONEJSON_JSON_VALUE_NULL);

  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
}

static void test_json_value_nested_object_parse_sink_failure_matrix(void) {
  const char *broken_json =
      "{\"type\":\"response.completed\","
      "\"response\":{\"status\":\"ok\",\"payload\":{\"a\":[1,2,}}}";
  const char *valid_json = "{\"type\":\"response.completed\","
                           "\"response\":{\"payload\":{\"a\":1}}}";
  test_nested_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char payload_buf[128];
  test_buffer_sink payload_sink;

  memset(&payload_sink, 0, sizeof(payload_sink));
  payload_sink.buffer = payload_buf;
  payload_sink.capacity = sizeof(payload_buf);

  poison_bytes(&doc, sizeof(doc), 0xC2u);
  test_init_map(&test_nested_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_sink(
      &doc.response.payload, test_buffer_sink_write, &payload_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;

  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, broken_json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(strstr((const char *)payload_buf, "{\"a\":[1,2") != NULL);

  payload_sink.length = 0u;
  payload_buf[0] = '\0';
  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, valid_json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)payload_buf, "{\"a\":1}") == 0);

  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);

  poison_bytes(&doc, sizeof(doc), 0xC3u);
  test_init_map(&test_nested_json_value_doc_map, &doc);
  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, valid_json,
                           NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
}

static void test_json_value_parse_visitor(void) {
  const char *json =
      "{"
      "\"id\":\"visit-1\","
      "\"selector\":{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"},"
      "\"fields\":[\"id\",\"name\",{\"nested\":[true,null,3.5]}],"
      "\"last_error\":null"
      "}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_state selector_state;
  test_visit_state fields_state;
  test_visit_state error_state;
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson__value_limits limits = lonejson__default_value_limits();

  memset(&selector_state, 0, sizeof(selector_state));
  memset(&fields_state, 0, sizeof(fields_state));
  memset(&error_state, 0, sizeof(error_state));
  poison_bytes(&doc, sizeof(doc), 0xCCu);
  test_init_map(&test_json_value_doc_map, &doc);
  doc.selector.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor,
                                                 &selector_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  doc.fields.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(&doc.fields, &visitor,
                                                 &fields_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  doc.last_error.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(&doc.last_error, &visitor,
                                                 &error_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(selector_state.log,
                "{K(kind)S(term)K(field)S(name)K(value)S(alice)}") == 0);
  EXPECT(strcmp(fields_state.log, "[S(id)S(name){K(nested)[TZN(3.5)]}]") == 0);
  EXPECT(strcmp(error_state.log, "Z") == 0);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.fields.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.last_error.kind == LONEJSON_JSON_VALUE_NULL);

  limits.max_string_bytes = 3u;
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  poison_bytes(&doc, sizeof(doc), 0xCCu);
  test_init_map(&test_json_value_doc_map, &doc);
  memset(&selector_state, 0, sizeof(selector_state));
  doc.selector.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor,
                                                 &selector_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc,
                      "{\"id\":\"visit-2\",\"selector\":{\"value\":\"alice\"}}",
                      &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "JSON string exceeds maximum decoded byte limit") != NULL);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

typedef struct test_json_value_path_visit_state {
  char log[8192];
  size_t len;
  size_t callback_count;
  size_t fail_after;
  int truncate_container_events;
  size_t mismatched_chunk_paths;
  char active_string_path[256];
  char active_number_path[256];
} test_json_value_path_visit_state;

static lonejson_status
test_json_value_path_visit_append(test_json_value_path_visit_state *state,
                                  const char *data, size_t len) {
  size_t copy_len;

  if (state == NULL || data == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
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

static lonejson_status
test_json_value_path_visit_append_cstr(test_json_value_path_visit_state *state,
                                       const char *text) {
  return test_json_value_path_visit_append(state, text, strlen(text));
}

static lonejson_status
test_json_value_path_visit_format(char *out, size_t out_size,
                                  const lonejson_value_path *path) {
  size_t i;
  size_t len = 0u;

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

static lonejson_status
test_json_value_path_visit_maybe_fail(test_json_value_path_visit_state *state,
                                      lonejson_error *error) {
  ++state->callback_count;
  if (state->fail_after != 0u && state->callback_count >= state->fail_after) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u,
                               "intentional JSON_VALUE path visitor failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_json_value_path_visit_event(
    test_json_value_path_visit_state *state, const char *name,
    const lonejson_value_path *path, lonejson_error *error) {
  lonejson_status status;
  char path_text[256];

  status = test_json_value_path_visit_maybe_fail(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      test_json_value_path_visit_format(path_text, sizeof(path_text), path);
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, name);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, "(");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, path_text);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, ")");
  }
  return status;
}

static lonejson_status test_json_value_path_visit_container_event(
    test_json_value_path_visit_state *state, const char *name,
    const lonejson_value_path *path, lonejson_error *error) {
  lonejson_status status;

  status = test_json_value_path_visit_event(state, name, path, error);
  if (status == LONEJSON_STATUS_OK && state->truncate_container_events) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

static lonejson_status test_json_value_path_visit_object_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_container_event(
      (test_json_value_path_visit_state *)user, "{", path, error);
}

static lonejson_status test_json_value_path_visit_object_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_container_event(
      (test_json_value_path_visit_state *)user, "}", path, error);
}

static lonejson_status test_json_value_path_visit_key_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_event(
      (test_json_value_path_visit_state *)user, "K<", path, error);
}

static lonejson_status test_json_value_path_visit_key_chunk(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  lonejson_status status;
  test_json_value_path_visit_state *state;

  state = (test_json_value_path_visit_state *)user;
  status = test_json_value_path_visit_event(state, "K", path, error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, "=");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append(state, data, len);
  }
  return status;
}

static lonejson_status
test_json_value_path_visit_key_end(void *user, const lonejson_value_path *path,
                                   lonejson_error *error) {
  return test_json_value_path_visit_event(
      (test_json_value_path_visit_state *)user, "K>", path, error);
}

static lonejson_status test_json_value_path_visit_array_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_container_event(
      (test_json_value_path_visit_state *)user, "[", path, error);
}

static lonejson_status test_json_value_path_visit_array_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_container_event(
      (test_json_value_path_visit_state *)user, "]", path, error);
}

static lonejson_status test_json_value_path_visit_string_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson_status status;
  test_json_value_path_visit_state *state;

  state = (test_json_value_path_visit_state *)user;
  status = test_json_value_path_visit_format(
      state->active_string_path, sizeof(state->active_string_path), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return test_json_value_path_visit_event(state, "S<", path, error);
}

static lonejson_status test_json_value_path_visit_string_chunk(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  lonejson_status status;
  test_json_value_path_visit_state *state;
  char path_text[256];

  state = (test_json_value_path_visit_state *)user;
  status =
      test_json_value_path_visit_format(path_text, sizeof(path_text), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strcmp(path_text, state->active_string_path) != 0) {
    ++state->mismatched_chunk_paths;
  }
  status = test_json_value_path_visit_event(state, "S", path, error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, "=");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append(state, data, len);
  }
  return status;
}

static lonejson_status test_json_value_path_visit_string_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_event(
      (test_json_value_path_visit_state *)user, "S>", path, error);
}

static lonejson_status test_json_value_path_visit_number_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson_status status;
  test_json_value_path_visit_state *state;

  state = (test_json_value_path_visit_state *)user;
  status = test_json_value_path_visit_format(
      state->active_number_path, sizeof(state->active_number_path), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return test_json_value_path_visit_event(state, "N<", path, error);
}

static lonejson_status test_json_value_path_visit_number_chunk(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  lonejson_status status;
  test_json_value_path_visit_state *state;
  char path_text[256];

  state = (test_json_value_path_visit_state *)user;
  status =
      test_json_value_path_visit_format(path_text, sizeof(path_text), path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strcmp(path_text, state->active_number_path) != 0) {
    ++state->mismatched_chunk_paths;
  }
  status = test_json_value_path_visit_event(state, "N", path, error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append_cstr(state, "=");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_json_value_path_visit_append(state, data, len);
  }
  return status;
}

static lonejson_status test_json_value_path_visit_number_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  return test_json_value_path_visit_event(
      (test_json_value_path_visit_state *)user, "N>", path, error);
}

static lonejson_status
test_json_value_path_visit_bool(void *user, const lonejson_value_path *path,
                                int value, lonejson_error *error) {
  return test_json_value_path_visit_event(
      (test_json_value_path_visit_state *)user, value ? "T" : "F", path, error);
}

static lonejson_status
test_json_value_path_visit_null(void *user, const lonejson_value_path *path,
                                lonejson_error *error) {
  return test_json_value_path_visit_event(
      (test_json_value_path_visit_state *)user, "Z", path, error);
}

static lonejson_path_value_visitor test_json_value_path_visitor(void) {
  lonejson_path_value_visitor visitor;

  visitor = lonejson_default_path_value_visitor();
  visitor.object_begin = test_json_value_path_visit_object_begin;
  visitor.object_end = test_json_value_path_visit_object_end;
  visitor.object_key_begin = test_json_value_path_visit_key_begin;
  visitor.object_key_chunk = test_json_value_path_visit_key_chunk;
  visitor.object_key_end = test_json_value_path_visit_key_end;
  visitor.array_begin = test_json_value_path_visit_array_begin;
  visitor.array_end = test_json_value_path_visit_array_end;
  visitor.string_begin = test_json_value_path_visit_string_begin;
  visitor.string_chunk = test_json_value_path_visit_string_chunk;
  visitor.string_end = test_json_value_path_visit_string_end;
  visitor.number_begin = test_json_value_path_visit_number_begin;
  visitor.number_chunk = test_json_value_path_visit_number_chunk;
  visitor.number_end = test_json_value_path_visit_number_end;
  visitor.boolean_value = test_json_value_path_visit_bool;
  visitor.null_value = test_json_value_path_visit_null;
  return visitor;
}

static void test_json_value_parse_path_visitor_paths(void) {
  const char *json =
      "{\"id\":\"path-1\","
      "\"selector\":{\"a/b\":{\"~key\":\"v\"},\"items\":[10,{\"sku\":\"ABC\"},"
      "false,null],\"\":\"empty\"},"
      "\"fields\":\"root scalar\","
      "\"last_error\":[true]}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_json_value_path_visit_state selector_state;
  test_json_value_path_visit_state fields_state;
  test_json_value_path_visit_state error_state;
  lonejson_path_value_visitor visitor;

  memset(&selector_state, 0, sizeof(selector_state));
  memset(&fields_state, 0, sizeof(fields_state));
  memset(&error_state, 0, sizeof(error_state));
  visitor = test_json_value_path_visitor();
  poison_bytes(&doc, sizeof(doc), 0xCFu);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_path_visitor(&doc.selector, &visitor,
                                                      &selector_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = doc.fields.methods->set_parse_path_visitor(&doc.fields, &visitor,
                                                      &fields_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lj_json_value_set_parse_path_visitor(&doc.last_error, &visitor,
                                                &error_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(selector_state.log, "{($)K<($)K($)=a/bK>($){($/3:a/b)") !=
         NULL);
  EXPECT(strstr(selector_state.log,
                "S<($/3:a/b/4:~key)S($/3:a/b/4:~key)=vS>($/3:a/b/4:~key)") !=
         NULL);
  EXPECT(strstr(selector_state.log,
                "[($/5:items)N<($/5:items/1:0)N($/5:items/1:0)=10N>($/5:"
                "items/1:0)") != NULL);
  EXPECT(strstr(selector_state.log,
                "{($/5:items/1:1)K<($/5:items/1:1)K($/5:items/1:1)=skuK>("
                "$/5:items/1:1)") != NULL);
  EXPECT(strstr(selector_state.log, "F($/5:items/1:2)Z($/5:items/1:3)](") !=
         NULL);
  EXPECT(strstr(selector_state.log, "K<($)K>($)S<($/0:)S($/0:)=empty") != NULL);
  EXPECT(strcmp(fields_state.log, "S<($)S($)=root scalarS>($)") == 0);
  EXPECT(strcmp(error_state.log, "[($)T($/1:0)]($)") == 0);
  EXPECT(selector_state.mismatched_chunk_paths == 0u);
  EXPECT(fields_state.mismatched_chunk_paths == 0u);
  EXPECT(error_state.mismatched_chunk_paths == 0u);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.fields.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.last_error.kind == LONEJSON_JSON_VALUE_NULL);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_parse_path_visitor_truncated_containers(void) {
  const char *json =
      "{\"id\":\"path-truncated\","
      "\"selector\":{\"items\":[{\"sku\":\"ABC\"},false],\"tail\":true}}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_json_value_path_visit_state state;
  lonejson_path_value_visitor visitor;

  memset(&state, 0, sizeof(state));
  state.truncate_container_events = 1;
  visitor = test_json_value_path_visitor();
  poison_bytes(&doc, sizeof(doc), 0xD1u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_path_visitor(&doc.selector, &visitor,
                                                      &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strstr(state.log, "[($/5:items)") != NULL);
  EXPECT(strstr(state.log,
                "{($/5:items/1:0)K<($/5:items/1:0)K($/5:items/1:0)=skuK>("
                "$/5:items/1:0)") != NULL);
  EXPECT(strstr(state.log,
                "S<($/5:items/1:0/3:sku)S($/5:items/1:0/3:sku)=ABCS>("
                "$/5:items/1:0/3:sku)") != NULL);
  EXPECT(strstr(state.log, "F($/5:items/1:1)") != NULL);
  EXPECT(strstr(state.log, "T($/4:tail)") != NULL);
  EXPECT(state.mismatched_chunk_paths == 0u);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_parse_path_visitor_zero_depth(void) {
  const char *json =
      "{\"id\":\"path-depth\","
      "\"selector\":{\"outer\":{\"items\":[{\"sku\":\"ABC\"},null]}}}";
  lonejson_config config;
  lonejson *runtime;
  test_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  test_json_value_path_visit_state state;
  lonejson_path_value_visitor visitor;

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  config.json_value_max_depth = 0u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  memset(&state, 0, sizeof(state));
  visitor = test_json_value_path_visitor();
  poison_bytes(&doc, sizeof(doc), 0xD2u);
  lonejson_init(runtime, &test_json_value_doc_map, &doc, NULL);
  status = lonejson_json_value_set_parse_path_visitor(&doc.selector, &visitor,
                                                      &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_parse_cstr(runtime, &test_json_value_doc_map, &doc, json,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log,
                "S<($/5:outer/5:items/1:0/3:sku)S($/5:outer/5:items/1:0/"
                "3:sku)=ABCS>($/5:outer/5:items/1:0/3:sku)") != NULL);
  EXPECT(strstr(state.log, "Z($/5:outer/5:items/1:1)") != NULL);
  EXPECT(state.mismatched_chunk_paths == 0u);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  lonejson_free(runtime);
}

static void test_json_value_parse_path_visitor_failure_cleanup(void) {
  const char *long_key_json =
      "{\"id\":\"path-fail\",\"selector\":{\"abcdefghijklmnopqrstuvwxyzabcdef"
      "ghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\":1}}";
  const char *valid_json =
      "{\"id\":\"path-ok\",\"selector\":{\"after\":[true,null]}}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_json_value_path_visit_state state;
  lonejson_path_value_visitor visitor;

  reset_lonejson_alloc_stats();
  memset(&state, 0, sizeof(state));
  state.fail_after = 6u;
  visitor = test_json_value_path_visitor();
  poison_bytes(&doc, sizeof(doc), 0xD0u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_path_visitor(&doc.selector, &visitor,
                                                      &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = test_parse_cstr(&test_json_value_doc_map, &doc, long_key_json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "intentional JSON_VALUE path visitor failure") !=
         NULL);

  memset(&state, 0, sizeof(state));
  status = test_parse_cstr(&test_json_value_doc_map, &doc, valid_json, &options,
                           &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(state.log,
                "{($)K<($)K($)=afterK>($)[($/5:after)T($/5:after/1:0)Z($/5:"
                "after/1:1)]($/5:after)}($)") == 0);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);

  memset(&state, 0, sizeof(state));
  poison_bytes(&doc, sizeof(doc), 0xD1u);
  test_init_map(&test_json_value_doc_map, &doc);
  doc.selector.parse_visitor_limits.max_key_bytes = 3u;
  status = lonejson_json_value_set_parse_path_visitor(&doc.selector, &visitor,
                                                      &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_parse_cstr(&test_json_value_doc_map, &doc,
                           "{\"id\":\"path-limit\",\"selector\":{\"abcd\":1}}",
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "JSON object key exceeds maximum decoded byte limit") != NULL);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_json_value_parse_path_visitor_api_guards(void) {
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  lonejson_path_value_visitor visitor;

  visitor = test_json_value_path_visitor();
  lonejson_json_value_init(test_default_runtime(), &value);
  EXPECT(value.methods->set_parse_path_visitor != NULL);
  status =
      lonejson_json_value_set_parse_path_visitor(NULL, &visitor, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status =
      lonejson_json_value_set_parse_path_visitor(&value, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  status =
      value.methods->set_parse_path_visitor(&value, &visitor, &value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.parse_mode == LONEJSON_JSON_VALUE_PARSE_PATH_VISITOR);
  EXPECT(value.parse_path_visitor == &visitor);
  EXPECT(value.parse_path_visitor_user == &value);
  EXPECT(value.parse_visitor == NULL);
  EXPECT(value.parse_sink == NULL);
  status = lonejson_json_value_enable_parse_capture(&value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.parse_mode == LONEJSON_JSON_VALUE_PARSE_CAPTURE);
  EXPECT(value.parse_path_visitor == NULL);
  status = lj_json_value_set_parse_path_visitor(&value, &visitor, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.parse_mode == LONEJSON_JSON_VALUE_PARSE_PATH_VISITOR);
  lonejson_json_value_cleanup(&value);
}

static void test_json_value_nested_object_parse_visitor(void) {
  const char *json =
      "{\"type\":\"response.completed\","
      "\"response\":{\"status\":\"ok\",\"payload\":{\"nested\":[true,null,"
      "3.5]}}}";
  test_nested_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_state payload_state;
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson__value_limits limits = lonejson__default_value_limits();

  memset(&payload_state, 0, sizeof(payload_state));
  poison_bytes(&doc, sizeof(doc), 0xC4u);
  test_init_map(&test_nested_json_value_doc_map, &doc);
  doc.response.payload.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(
      &doc.response.payload, &visitor, &payload_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.type, "response.completed") == 0);
  EXPECT(strcmp(doc.response.status, "ok") == 0);
  EXPECT(strcmp(payload_state.log, "{K(nested)[TZN(3.5)]}") == 0);
  EXPECT(doc.response.payload.kind == LONEJSON_JSON_VALUE_NULL);

  limits.max_string_bytes = 3u;
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
  memset(&payload_state, 0, sizeof(payload_state));
  poison_bytes(&doc, sizeof(doc), 0xC5u);
  test_init_map(&test_nested_json_value_doc_map, &doc);
  doc.response.payload.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(
      &doc.response.payload, &visitor, &payload_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = test_parse_cstr(
      &test_nested_json_value_doc_map, &doc,
      "{\"type\":\"response.completed\",\"response\":{\"payload\":"
      "{\"value\":\"alice\"}}}",
      &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "JSON string exceeds maximum decoded byte limit") != NULL);

  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
}

static void test_json_value_nested_object_parse_capture(void) {
  const char *json = "{\"type\":\"response.completed\","
                     "\"response\":{\"status\":\"ok\",\"payload\":{\"x\":1}}}";
  const char *json_reuse = "{\"type\":\"response.completed\","
                           "\"response\":{\"payload\":[false,null,2]}}";
  test_nested_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&doc, sizeof(doc), 0xC6u);
  test_init_map(&test_nested_json_value_doc_map, &doc);
  status =
      lonejson_json_value_enable_parse_capture(&doc.response.payload, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;

  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.response.payload.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.response.payload.json, "{\"x\":1}") == 0);
  EXPECT(strcmp(doc.response.status, "ok") == 0);

  status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json_reuse,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.response.payload.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.response.payload.json, "[false,null,2]") == 0);
  EXPECT(doc.response.status[0] == '\0');

  lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);
}

static lonejson_status test_spooled_append_sink(void *user, const void *data,
                                                size_t len,
                                                lonejson_error *error) {
  return lonejson_spooled_append((lonejson_spooled *)user, data, len, error);
}

static char *test_make_repeating_json(const char *prefix, size_t payload_len,
                                      const char *suffix) {
  char *payload;
  char *json;
  size_t prefix_len;
  size_t suffix_len;

  payload = make_repeating_text(payload_len);
  if (payload == NULL) {
    return NULL;
  }
  prefix_len = strlen(prefix);
  suffix_len = strlen(suffix);
  json = (char *)malloc(prefix_len + payload_len + suffix_len + 1u);
  if (json == NULL) {
    free(payload);
    return NULL;
  }
  memcpy(json, prefix, prefix_len);
  memcpy(json + prefix_len, payload, payload_len);
  memcpy(json + prefix_len + payload_len, suffix, suffix_len + 1u);
  free(payload);
  return json;
}

static void test_json_value_large_string_scalar_regressions(void) {
  const size_t lengths[] = {4012u, 4013u, 65537u, 196608u};
  size_t i;
  char *json;
  test_json_value_doc doc;
  lonejson_spooled selector_storage;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson__spool_options spool_options = lonejson__default_spool_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char *read_back;

  options.clear_destination = 0;
  spool_options.memory_limit = 64u * 1024u;

  for (i = 0u; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
    json = test_make_repeating_json("{\"id\":\"big\",\"selector\":\"",
                                    lengths[i], "\"}");
    EXPECT(json != NULL);
    if (json == NULL) {
      return;
    }

    test_init_map(&test_json_value_doc_map, &doc);
    lonejson_spooled_init_with_allocator(&selector_storage, &spool_options,
                                         NULL);
    status = lonejson_json_value_set_parse_sink(
        &doc.selector, test_spooled_append_sink, &selector_storage, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(lonejson_spooled_size(&selector_storage) == lengths[i] + 2u);
    if (lengths[i] > spool_options.memory_limit) {
      EXPECT(lonejson_spooled_spilled(&selector_storage) != 0);
    }
    read_back = (unsigned char *)malloc(lengths[i] + 3u);
    EXPECT(read_back != NULL);
    if (read_back != NULL) {
      size_t got =
          read_spooled_all(&selector_storage, read_back, lengths[i] + 3u);
      EXPECT(got == lengths[i] + 2u);
      EXPECT(read_back[0] == '"');
      EXPECT(read_back[got - 1u] == '"');
      EXPECT(memcmp(read_back + 1u,
                    json + strlen("{\"id\":\"big\",\"selector\":\""),
                    lengths[i]) == 0);
      free(read_back);
    }
    lonejson_spooled_cleanup(&selector_storage);
    lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);

    test_init_map(&test_json_value_doc_map, &doc);
    status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
    EXPECT(doc.selector.len == lengths[i] + 2u);
    EXPECT(doc.selector.json[0] == '"');
    EXPECT(doc.selector.json[doc.selector.len - 1u] == '"');
    lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);

    free(json);
  }
}

static void test_json_value_large_string_object_regressions(void) {
  const size_t lengths[] = {4013u, 65537u};
  size_t i;
  char *json;
  test_json_value_doc doc;
  lonejson_spooled selector_storage;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson__spool_options spool_options = lonejson__default_spool_options();
  lonejson_error error;
  lonejson_status status;

  options.clear_destination = 0;
  spool_options.memory_limit = 64u * 1024u;

  for (i = 0u; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
    json = test_make_repeating_json("{\"id\":\"obj\",\"selector\":{\"text\":\"",
                                    lengths[i], "\"}}");
    EXPECT(json != NULL);
    if (json == NULL) {
      return;
    }

    test_init_map(&test_json_value_doc_map, &doc);
    lonejson_spooled_init_with_allocator(&selector_storage, &spool_options,
                                         NULL);
    status = lonejson_json_value_set_parse_sink(
        &doc.selector, test_spooled_append_sink, &selector_storage, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(lonejson_spooled_size(&selector_storage) == lengths[i] + 11u);
    EXPECT(lonejson_spooled_spilled(&selector_storage) ==
           (lengths[i] > spool_options.memory_limit));
    lonejson_spooled_cleanup(&selector_storage);
    lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);

    test_init_map(&test_json_value_doc_map, &doc);
    status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
    EXPECT(strncmp(doc.selector.json, "{\"text\":\"", 9u) == 0);
    EXPECT(doc.selector.len == lengths[i] + 11u);
    lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);

    free(json);
  }
}

static void test_json_value_large_string_nested_regressions(void) {
  const size_t lengths[] = {4013u, 65537u};
  size_t i;
  char *json;
  test_nested_json_value_doc doc;
  lonejson_spooled payload_storage;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson__spool_options spool_options = lonejson__default_spool_options();
  lonejson_error error;
  lonejson_status status;

  options.clear_destination = 0;
  spool_options.memory_limit = 64u * 1024u;

  for (i = 0u; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
    json = test_make_repeating_json(
        "{\"type\":\"response.completed\",\"response\":{\"status\":\"ok\","
        "\"payload\":{\"text\":\"",
        lengths[i], "\"}}}");
    EXPECT(json != NULL);
    if (json == NULL) {
      return;
    }

    poison_bytes(&doc, sizeof(doc), 0xC9u);
    test_init_map(&test_nested_json_value_doc_map, &doc);
    lonejson_spooled_init_with_allocator(&payload_storage, &spool_options,
                                         NULL);
    status = lonejson_json_value_set_parse_sink(&doc.response.payload,
                                                test_spooled_append_sink,
                                                &payload_storage, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json,
                             &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(doc.type, "response.completed") == 0);
    EXPECT(strcmp(doc.response.status, "ok") == 0);
    EXPECT(lonejson_spooled_size(&payload_storage) == lengths[i] + 11u);
    EXPECT(lonejson_spooled_spilled(&payload_storage) ==
           (lengths[i] > spool_options.memory_limit));
    lonejson_spooled_cleanup(&payload_storage);
    lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);

    poison_bytes(&doc, sizeof(doc), 0xCAu);
    test_init_map(&test_nested_json_value_doc_map, &doc);
    status =
        lonejson_json_value_enable_parse_capture(&doc.response.payload, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_parse_cstr(&test_nested_json_value_doc_map, &doc, json,
                             &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(doc.response.payload.kind == LONEJSON_JSON_VALUE_BUFFER);
    EXPECT(doc.response.payload.len == lengths[i] + 11u);
    EXPECT(strcmp(doc.response.status, "ok") == 0);
    lonejson_cleanup(&test_nested_json_value_doc_map, &doc, NULL);

    free(json);
  }
}

static void test_json_value_parse_visitor_fast_path_regressions(void) {
  const char *json =
      "{"
      "\"id\":\"visit-fast\","
      "\"selector\":{\"outer\":\"abcdefghijklmnopqrstuvwxyz0123456789\","
      "\"nested\":{\"inner\":\"plain-fast-path-string\",\"arr\":[\"alpha\","
      "{\"deep\":\"omega\"}]},\"tail\":\"done\"}"
      "}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_chunk_state state;
  lonejson_value_visitor visitor = test_counting_value_visitor();

  memset(&state, 0, sizeof(state));
  poison_bytes(&doc, sizeof(doc), 0xCCu);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor,
                                                 &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.object_count == 3u);
  EXPECT(state.array_count == 1u);
  EXPECT(state.key_begin_count == 6u);
  EXPECT(state.key_end_count == 6u);
  EXPECT(state.string_begin_count == 5u);
  EXPECT(state.string_end_count == 5u);
  EXPECT(state.string_chunk_count >= 5u);
  EXPECT(state.number_begin_count == 0u);
  EXPECT(strcmp(state.last_string, "done") == 0);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void
test_json_value_parse_visitor_total_byte_reset_for_string_regressions(void) {
  const char *json = "{"
                     "\"id\":\"visit-total\","
                     "\"selector\":{\"k\":\"abcd\"},"
                     "\"fields\":\"wxyz\""
                     "}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_state selector_state;
  test_visit_state fields_state;
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson__value_limits limits = lonejson__default_value_limits();

  memset(&selector_state, 0, sizeof(selector_state));
  memset(&fields_state, 0, sizeof(fields_state));
  poison_bytes(&doc, sizeof(doc), 0xCDu);
  test_init_map(&test_json_value_doc_map, &doc);
  limits.max_total_bytes = 8u;
  doc.selector.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor,
                                                 &selector_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  doc.fields.parse_visitor_limits = limits;
  status = lonejson_json_value_set_parse_visitor(&doc.fields, &visitor,
                                                 &fields_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(selector_state.log, "{K(k)S(abcd)}") == 0);
  EXPECT(strcmp(fields_state.log, "S(wxyz)") == 0);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_parse_visitor_scalar_string_regressions(void) {
  const char *json = "{"
                     "\"id\":\"visit-scalar\","
                     "\"selector\":null,"
                     "\"fields\":\"abcdefghijklmnopqrstuvwxyz0123456789\""
                     "}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_chunk_state state;
  lonejson_value_visitor visitor = test_counting_value_visitor();

  memset(&state, 0, sizeof(state));
  poison_bytes(&doc, sizeof(doc), 0xCEu);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_visitor(&doc.fields, &visitor, &state,
                                                 &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.string_begin_count == 1u);
  EXPECT(state.string_end_count == 1u);
  EXPECT(state.string_chunk_count >= 1u);
  EXPECT(strcmp(state.last_string, "abcdefghijklmnopqrstuvwxyz0123456789") ==
         0);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_setters_and_failures(void) {
  const char *selector_pretty = "{\n"
                                "  \"kind\": \"term\",\n"
                                "  \"field\": \"name\",\n"
                                "  \"value\": \"alice\"\n"
                                "}";
  const char *fields_pretty = "[\"id\", \"name\", \"created_at\"]";
  const char *error_pretty =
      "{ \"code\": \"bad_selector\", \"detail\": { \"path\": \"/name\" } }";
  const char *expected_compact =
      "{\"id\":\"emit-1\",\"selector\":{\"kind\":\"term\",\"field\":\"name\","
      "\"value\":\"alice\"},\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}}";
  char selector_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-selector-XXXXXX";
  char error_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-error-XXXXXX";
  test_json_value_doc doc;
  test_reader_state fields_reader;
  lonejson_error error;
  lonejson_status status;
  int selector_fd = -1;
  int error_fd = -1;
  FILE *error_fp = NULL;
  char *serialized;

  selector_fd = mkstemp(selector_path);
  EXPECT(selector_fd >= 0);
  error_fd = mkstemp(error_path);
  EXPECT(error_fd >= 0);
  if (selector_fd < 0 || error_fd < 0) {
    if (selector_fd >= 0) {
      close(selector_fd);
      unlink(selector_path);
    }
    if (error_fd >= 0) {
      close(error_fd);
      unlink(error_path);
    }
    return;
  }
  EXPECT(write(selector_fd, selector_pretty, strlen(selector_pretty)) ==
         (ssize_t)strlen(selector_pretty));
  EXPECT(write(error_fd, error_pretty, strlen(error_pretty)) ==
         (ssize_t)strlen(error_pretty));
  error_fp = fdopen(error_fd, "rb");
  EXPECT(error_fp != NULL);
  if (error_fp == NULL) {
    close(selector_fd);
    close(error_fd);
    unlink(selector_path);
    unlink(error_path);
    return;
  }

  poison_bytes(&doc, sizeof(doc), 0xABu);
  test_init_map(&test_json_value_doc_map, &doc);
  strcpy(doc.id, "emit-1");
  fields_reader.json = fields_pretty;
  fields_reader.offset = 0u;
  fields_reader.chunk_size = 4u;
  status = lonejson_json_value_set_path(&doc.selector, selector_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_reader(&doc.fields, test_state_reader,
                                          &fields_reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_file(&doc.last_error, error_fp, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  serialized =
      test_serialize_alloc(&test_json_value_doc_map, &doc, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, expected_compact) == 0);
    LONEJSON_FREE(serialized);
  }

  status = lonejson_json_value_set_buffer(&doc.selector, "{bad", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  status = lonejson_json_value_set_buffer(&doc.selector, selector_pretty,
                                          strlen(selector_pretty), &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.selector.json,
                "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}") ==
         0);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  fclose(error_fp);
  close(selector_fd);
  unlink(selector_path);
  unlink(error_path);
}

static void test_json_value_scalars_null_and_reset(void) {
  const char *json =
      "{\"id\":\"scalar-1\",\"selector\":null,"
      "\"fields\":\"line\\nquoted\\\"value\",\"last_error\":false}";
  test_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  char *serialized;

  lonejson__parse_options options = lonejson__default_parse_options();

  poison_bytes(&doc, sizeof(doc), 0xC1u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_json_value_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.selector.json, "null") == 0);
  EXPECT(strcmp(doc.fields.json, "\"line\\nquoted\\\"value\"") == 0);
  EXPECT(strcmp(doc.last_error.json, "false") == 0);

  serialized =
      test_serialize_alloc(&test_json_value_doc_map, &doc, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, json) == 0);
    LONEJSON_FREE(serialized);
  }

  reset_lonejson_alloc_stats();
  status =
      lonejson_json_value_set_buffer(&doc.selector, "{\"a\":1}", 7u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count == 1u);
  lonejson_json_value_reset(&doc.selector);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(g_alloc_record_count == 0u);

  serialized =
      test_serialize_alloc(&test_json_value_doc_map, &doc, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strstr(serialized, "\"selector\":null") != NULL);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_json_value_reuse_and_cleanup_ownership(void) {
  char selector_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-reuse-XXXXXX";
  test_json_value_doc doc;
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  char *serialized;
  int fd;
  lonejson__parse_options options = lonejson__default_parse_options();

  reset_lonejson_alloc_stats();
  lonejson_json_value_init(NULL, &value);
  fd = write_temp_text_file(selector_path, "{\"from\":\"path\"}");
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);

  status = lonejson_json_value_set_path(&value, selector_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count == 1u);

  status = lonejson_json_value_set_buffer(
      &value, "{\"from\":\"buffer\"}", strlen("{\"from\":\"buffer\"}"), &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(g_alloc_record_count == 1u);
  unlink(selector_path);

  lonejson_init(test_default_runtime(), &test_json_value_doc_map, &doc, NULL);
  strcpy(doc.id, "reuse");
  doc.selector = value;
  lonejson_json_value_init(NULL, &value);
  serialized =
      test_serialize_alloc(&test_json_value_doc_map, &doc, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strstr(serialized, "\"selector\":{\"from\":\"buffer\"}") != NULL);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  lonejson_json_value_cleanup(&value);
  EXPECT(g_alloc_record_count == 0u);

  poison_bytes(&doc, sizeof(doc), 0xB7u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_path(&doc.selector, selector_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_PATH);
  EXPECT(doc.selector.path != NULL);

  lonejson_json_value_cleanup(&doc.selector);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.selector.path == NULL);
  EXPECT(doc.selector.fp == NULL);
  EXPECT(doc.selector.fd == -1);

  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = test_parse_cstr(&test_json_value_doc_map, &doc,
                           "{\"id\":\"reuse-parse\",\"selector\":1,"
                           "\"fields\":2,\"last_error\":3}",
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.selector.json, "1") == 0);
  EXPECT(strcmp(doc.fields.json, "2") == 0);
  EXPECT(strcmp(doc.last_error.json, "3") == 0);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_reset_releases_existing_json_value_storage(void) {
  lonejson_error error;
  test_json_value_doc doc;

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_json_value_doc_map, &doc, NULL);
  reset_lonejson_alloc_stats();
  EXPECT(lonejson_json_value_enable_parse_capture(&doc.selector, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_buffer(&doc.selector, "{\"ok\":true}", 11u,
                                        &error) == LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count == 1u);
  reset_lonejson_alloc_stats();
  lonejson_reset(test_default_runtime(), &test_json_value_doc_map, &doc, NULL);
  EXPECT(g_lonejson_free_calls > 0u);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_reset_releases_existing_spooled_storage(void) {
  lonejson_error error;
  test_spool_doc doc;

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_spool_doc_map, &doc, NULL);
  reset_lonejson_alloc_stats();
  EXPECT(lonejson_spooled_append(&doc.text, "abc", 3u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count > 0u);
  reset_lonejson_alloc_stats();
  lonejson_reset(test_default_runtime(), &test_spool_doc_map, &doc, NULL);
  EXPECT(g_lonejson_free_calls > 0u);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(lonejson_spooled_size(&doc.text) == 0u);
  lonejson_cleanup(&test_spool_doc_map, &doc, NULL);
}

static void test_init_preserves_shallow_copied_source_owner(void) {
  lonejson_error error;
  test_source_doc owner;
  test_source_doc alias;
  char path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-source-owner-XXXXXX";
  int fd;
  test_buffer_sink sink;
  unsigned char buffer[64];

  fd = write_temp_text_file(path, "owned payload");
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);

  memset(&owner, 0, sizeof(owner));
  lonejson_init(test_default_runtime(), &test_source_doc_map, &owner, NULL);
  reset_lonejson_alloc_stats();
  EXPECT(lonejson_source_set_path(&owner.text, path, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count == 1u);
  alias = owner;
  reset_lonejson_alloc_stats();
  lonejson_init(test_default_runtime(), &test_source_doc_map, &alias, NULL);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(alias.text.kind == LONEJSON_SOURCE_NONE);
  EXPECT(owner.text.kind == LONEJSON_SOURCE_PATH);

  memset(&sink, 0, sizeof(sink));
  memset(buffer, 0, sizeof(buffer));
  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  EXPECT(lonejson_source_write_to_sink(&owner.text, test_buffer_sink_write,
                                       &sink, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)buffer, "owned payload") == 0);

  reset_lonejson_alloc_stats();
  lonejson_cleanup(&test_source_doc_map, &owner, NULL);
  EXPECT(g_lonejson_free_calls > 0u);
  EXPECT(g_alloc_record_count == 0u);
  lonejson_cleanup(&test_source_doc_map, &alias, NULL);
  unlink(path);
}

static void test_json_value_source_validation_failures(void) {
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  char trailing_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-trailing-XXXXXX";
  char invalid_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-invalid-XXXXXX";
  char trailing_path2[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-trailing-XXXXXX";
  char invalid_path2[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-invalid-XXXXXX";
  int trailing_fd;
  int invalid_fd;
  FILE *fp;
  test_reader_state trailing_reader;
  test_reader_state invalid_reader;
  test_counting_sink sink;

  lonejson_json_value_init(test_default_runtime(), &value);

  status = lonejson_json_value_set_buffer(&value, "{} trailing",
                                          strlen("{} trailing"), &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  status = lonejson_json_value_set_buffer(&value, "{bad", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  trailing_reader.json = "{} trailing";
  trailing_reader.offset = 0u;
  trailing_reader.chunk_size = 3u;
  status = lonejson_json_value_set_reader(&value, test_state_reader,
                                          &trailing_reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  invalid_reader.json = "{bad";
  invalid_reader.offset = 0u;
  invalid_reader.chunk_size = 2u;
  status = lonejson_json_value_set_reader(&value, test_state_reader,
                                          &invalid_reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  trailing_fd = write_temp_text_file(trailing_path, "{} trailing");
  EXPECT(trailing_fd >= 0);
  invalid_fd = write_temp_text_file(invalid_path, "{bad");
  EXPECT(invalid_fd >= 0);
  if (trailing_fd >= 0) {
    close(trailing_fd);
    status = lonejson_json_value_set_path(&value, trailing_path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    unlink(trailing_path);
  }
  if (invalid_fd >= 0) {
    close(invalid_fd);
    status = lonejson_json_value_set_path(&value, invalid_path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    unlink(invalid_path);
  }

  trailing_fd = write_temp_text_file(trailing_path2, "{} trailing");
  EXPECT(trailing_fd >= 0);
  if (trailing_fd >= 0) {
    fp = fdopen(trailing_fd, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      status = lonejson_json_value_set_file(&value, fp, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
      fclose(fp);
    } else {
      close(trailing_fd);
    }
    unlink(trailing_path2);
  }

  invalid_fd = write_temp_text_file(invalid_path2, "{bad");
  EXPECT(invalid_fd >= 0);
  if (invalid_fd >= 0) {
    status = lonejson_json_value_set_fd(&value, invalid_fd, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    close(invalid_fd);
    unlink(invalid_path2);
  }

  lonejson_json_value_cleanup(&value);
}

static void test_json_value_reader_source_rejects_would_block(void) {
  static const char json[] = "{\"nested\":true}";
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  test_would_block_tripwire_read_state reader;
  test_counting_sink sink;

  lonejson_json_value_init(test_default_runtime(), &value);
  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.chunk_size = 8u;
  reader.block_after = 8u;
  status = lonejson_json_value_set_reader(
      &value, test_would_block_tripwire_reader, &reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(reader.blocked);
  EXPECT(!reader.reentered_after_block);
  EXPECT(reader.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);

  memset(&reader, 0, sizeof(reader));
  reader.json = json;
  reader.chunk_size = 8u;
  reader.block_after = 8u;
  status = value.methods->set_reader(&value, test_would_block_tripwire_reader,
                                     &reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  status = value.methods->write_to_sink(&value, test_counting_sink_write, &sink,
                                        &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(reader.blocked);
  EXPECT(!reader.reentered_after_block);
  EXPECT(reader.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);
  lonejson_json_value_cleanup(&value);
}

static void
test_json_value_parse_requires_destination_and_partial_failure(void) {
  const char *missing_config_json =
      "{\"id\":\"missing\",\"selector\":{\"a\":1}}";
  const char *broken_json =
      "{\"id\":\"broken\",\"selector\":{\"a\":[1,2,},\"fields\":null}";
  test_json_value_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char selector_buf[128];
  test_buffer_sink selector_sink;

  poison_bytes(&doc, sizeof(doc), 0xD3u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = test_parse_cstr(&test_json_value_doc_map, &doc, missing_config_json,
                           NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);

  memset(&selector_sink, 0, sizeof(selector_sink));
  selector_sink.buffer = selector_buf;
  selector_sink.capacity = sizeof(selector_buf);
  poison_bytes(&doc, sizeof(doc), 0xD4u);
  test_init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_sink(
      &doc.selector, test_buffer_sink_write, &selector_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = test_parse_cstr(&test_json_value_doc_map, &doc, broken_json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(strstr((const char *)selector_buf, "{\"a\":[1,2") != NULL);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_json_value_nonseekable_and_sink_failures(void) {
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  int pipe_fds[2];
  FILE *fp;
  test_failing_sink failing_sink;
  test_counting_sink sink;

  lonejson_json_value_init(test_default_runtime(), &value);

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "{\"x\":1}", 7u) == 7);
    close(pipe_fds[1]);
    status = lonejson_json_value_set_fd(&value, pipe_fds[0], &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_IO_ERROR);
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "{\"x\":1}", 7u) == 7);
    close(pipe_fds[1]);
    fp = fdopen(pipe_fds[0], "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      status = lonejson_json_value_set_file(&value, fp, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_IO_ERROR);
      fclose(fp);
    } else {
      close(pipe_fds[0]);
    }
  } else {
    EXPECT(0);
  }

  status = lonejson_json_value_set_buffer(
      &value, "{\"kind\":\"term\",\"field\":\"name\"}",
      strlen("{\"kind\":\"term\",\"field\":\"name\"}"), &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  failing_sink.fail_after = 8u;
  failing_sink.total = 0u;
  status = lonejson_json_value_write_to_sink(&value, test_failing_sink_write,
                                             &failing_sink, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);

  lonejson_json_value_cleanup(&value);
}

static void test_json_value_large_source_backed_serialization(void) {
  test_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  test_counting_sink sink;
  char large_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-large-XXXXXX";
  char *large_array;
  size_t payload_len;
  int fd;
  size_t i;

  large_array = (char *)malloc(300000u);
  EXPECT(large_array != NULL);
  if (large_array == NULL) {
    return;
  }
  payload_len = 0u;
  large_array[payload_len++] = '[';
  for (i = 0u; i < 40000u; ++i) {
    int wrote;
    wrote = snprintf(large_array + payload_len, 300000u - payload_len, "%s%lu",
                     (i == 0u) ? "" : ",", (unsigned long)i);
    EXPECT(wrote > 0);
    if (wrote <= 0) {
      free(large_array);
      return;
    }
    payload_len += (size_t)wrote;
  }
  large_array[payload_len++] = ']';
  large_array[payload_len] = '\0';

  fd = write_temp_text_file(large_path, large_array);
  EXPECT(fd >= 0);
  if (fd < 0) {
    free(large_array);
    return;
  }
  close(fd);

  poison_bytes(&doc, sizeof(doc), 0xCDu);
  test_init_map(&test_json_value_doc_map, &doc);
  strcpy(doc.id, "large-1");
  status = lonejson_json_value_set_path(&doc.fields, large_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  reset_lonejson_alloc_stats();
  memset(&sink, 0, sizeof(sink));
  status = test_serialize_sink(&test_json_value_doc_map, &doc,
                               test_counting_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total > payload_len);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);

  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  unlink(large_path);
  free(large_array);
}

static void test_json_value_nested_failure_matrix(void) {
  static const char *invalid_cases[] = {
      "{\"a\":[1,2,}",
      "{\"a\":{\"b\":}}",
      "{\"a\":\"unterminated}",
      "{\"a\":tru}",
      "{\"a\":01}",
      "[{\"a\":1},]",
      "{\"a\":{\"b\":[null,false,true,{\"c\":}]}}",
      "{\"a\":[{\"b\":\"x\"}]} trailing"};
  static const char *invalid_parse_docs[] = {
      "{\"id\":\"bad-1\",\"selector\":{\"a\":[1,2,},\"fields\":null}",
      "{\"id\":\"bad-2\",\"selector\":[{\"a\":1},],\"fields\":null}",
      "{\"id\":\"bad-3\",\"selector\":{\"a\":{\"b\":}},\"fields\":null}"};
  size_t i;
  lonejson_error error;
  lonejson_status status;
  lonejson_json_value value;
  test_counting_sink sink;
  char path_template[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-json-value-matrix-XXXXXX";
  int fd;
  FILE *fp;
  test_reader_state reader_state;
  test_json_value_doc doc;

  for (i = 0u; i < sizeof(invalid_parse_docs) / sizeof(invalid_parse_docs[0]);
       ++i) {
    lonejson__parse_options options = lonejson__default_parse_options();
    poison_bytes(&doc, sizeof(doc), 0xE7u);
    test_init_map(&test_json_value_doc_map, &doc);
    status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    options.clear_destination = 0;
    status = test_parse_cstr(&test_json_value_doc_map, &doc,
                             invalid_parse_docs[i], &options, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
  }

  lonejson_json_value_init(test_default_runtime(), &value);
  for (i = 0u; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    status = lonejson_json_value_set_buffer(&value, invalid_cases[i],
                                            strlen(invalid_cases[i]), &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

    reader_state.json = invalid_cases[i];
    reader_state.offset = 0u;
    reader_state.chunk_size = 3u;
    status = lonejson_json_value_set_reader(&value, test_state_reader,
                                            &reader_state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

    strcpy(path_template, LONEJSON_SOURCE_DIR
           "/build/test-tmp/lonejson-json-value-matrix-XXXXXX");
    fd = write_temp_text_file(path_template, invalid_cases[i]);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      close(fd);
      status = lonejson_json_value_set_path(&value, path_template, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
      unlink(path_template);
    }

    strcpy(path_template, LONEJSON_SOURCE_DIR
           "/build/test-tmp/lonejson-json-value-matrix-XXXXXX");
    fd = write_temp_text_file(path_template, invalid_cases[i]);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      fp = fdopen(fd, "rb");
      EXPECT(fp != NULL);
      if (fp != NULL) {
        status = lonejson_json_value_set_file(&value, fp, &error);
        EXPECT(status == LONEJSON_STATUS_OK);
        memset(&sink, 0, sizeof(sink));
        status = lonejson_json_value_write_to_sink(
            &value, test_counting_sink_write, &sink, &error);
        EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
        fclose(fp);
      } else {
        close(fd);
      }
      unlink(path_template);
    }

    strcpy(path_template, LONEJSON_SOURCE_DIR
           "/build/test-tmp/lonejson-json-value-matrix-XXXXXX");
    fd = write_temp_text_file(path_template, invalid_cases[i]);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      status = lonejson_json_value_set_fd(&value, fd, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
      close(fd);
      unlink(path_template);
    }
  }
  lonejson_json_value_cleanup(&value);
}
