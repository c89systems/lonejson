typedef struct test_sse_large_event_state {
  size_t count;
  size_t delta_count;
  size_t finalization_count;
  size_t max_text_len;
  int invalid;
} test_sse_large_event_state;

static lonejson_status test_protocol_append_literal(lonejson__byte_buffer *buf,
                                                    const char *text,
                                                    lonejson_error *error) {
  return lonejson__byte_append(buf, text, strlen(text), SIZE_MAX - 1u, NULL,
                               error);
}

static lonejson_status test_protocol_append_repeat(lonejson__byte_buffer *buf,
                                                   char ch, size_t count,
                                                   lonejson_error *error) {
  size_t i;

  for (i = 0u; i < count; ++i) {
    lonejson_status status =
        lonejson__byte_append_char(buf, ch, SIZE_MAX - 1u, NULL, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_sse_build_large_json_payload(
    lonejson__byte_buffer *payload, const char *type, const char *status_text,
    size_t text_len, char fill, lonejson_error *error) {
  lonejson_status status_code;

  lonejson__byte_reset(payload);
  status_code = test_protocol_append_literal(payload, "{\"type\":\"", error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  status_code = test_protocol_append_literal(payload, type, error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  status_code = test_protocol_append_literal(payload, "\"", error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  if (status_text != NULL) {
    status_code =
        test_protocol_append_literal(payload, ",\"status\":\"", error);
    if (status_code != LONEJSON_STATUS_OK) {
      return status_code;
    }
    status_code = test_protocol_append_literal(payload, status_text, error);
    if (status_code != LONEJSON_STATUS_OK) {
      return status_code;
    }
    status_code = test_protocol_append_literal(payload, "\"", error);
    if (status_code != LONEJSON_STATUS_OK) {
      return status_code;
    }
  }
  status_code = test_protocol_append_literal(payload, ",\"text\":\"", error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  status_code = test_protocol_append_repeat(payload, fill, text_len, error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  return test_protocol_append_literal(payload, "\"}", error);
}

static lonejson_status
test_sse_build_large_json_string_payload(lonejson__byte_buffer *payload,
                                         size_t text_len, char fill,
                                         lonejson_error *error) {
  lonejson_status status_code;

  lonejson__byte_reset(payload);
  status_code = test_protocol_append_literal(payload, "\"", error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  status_code = test_protocol_append_repeat(payload, fill, text_len, error);
  if (status_code != LONEJSON_STATUS_OK) {
    return status_code;
  }
  return test_protocol_append_literal(payload, "\"", error);
}

static lonejson_status test_sse_push_json_event_chunked(
    lonejson_sse *sse, const lonejson_map *map, void *dst, const char *event,
    const char *id, const lonejson__byte_buffer *payload, size_t chunk_bytes,
    const lonejson_sse_json_options *options,
    const lonejson__parse_options *parse_options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  lonejson__byte_buffer framed;
  size_t pos;
  size_t chunk;
  lonejson_status status;

  memset(&framed, 0, sizeof(framed));
  status = test_protocol_append_literal(&framed, "event: ", error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, event, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\nid: ", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, id, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\n", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "data: ", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__byte_append(&framed, payload->data, payload->len,
                                   SIZE_MAX - 1u, NULL, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\n", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\n", error);
  }
  for (pos = 0u; status == LONEJSON_STATUS_OK && pos < framed.len;
       pos += chunk_bytes) {
    chunk = framed.len - pos;
    if (chunk > chunk_bytes) {
      chunk = chunk_bytes;
    }
    status = test_sse_push_json(sse, map, dst, framed.data + pos, chunk,
                                options, parse_options, event_cb, user, error);
  }
  lonejson__byte_free(&framed, NULL);
  return status;
}

static lonejson_status test_sse_push_json_value_event_chunked(
    lonejson_sse *sse, lonejson_json_value *value, const char *event,
    const char *id, const lonejson__byte_buffer *payload, size_t chunk_bytes,
    const lonejson_sse_json_options *options,
    const lonejson__parse_options *parse_options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error) {
  lonejson__byte_buffer framed;
  size_t pos;
  size_t chunk;
  lonejson_status status;

  memset(&framed, 0, sizeof(framed));
  status = test_protocol_append_literal(&framed, "event: ", error);
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, event, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\nid: ", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, id, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\n", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "data: ", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__byte_append(&framed, payload->data, payload->len,
                                   SIZE_MAX - 1u, NULL, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\n", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = test_protocol_append_literal(&framed, "\n", error);
  }
  for (pos = 0u; status == LONEJSON_STATUS_OK && pos < framed.len;
       pos += chunk_bytes) {
    chunk = framed.len - pos;
    if (chunk > chunk_bytes) {
      chunk = chunk_bytes;
    }
    status =
        test_sse_push_json_value(sse, value, framed.data + pos, chunk, options,
                                 parse_options, event_cb, user, error);
  }
  lonejson__byte_free(&framed, NULL);
  return status;
}

static lonejson_status test_sse_large_event_cb(void *user,
                                               const lonejson_sse_event *event,
                                               void *dst,
                                               lonejson_error *error) {
  test_sse_large_event_state *state = (test_sse_large_event_state *)user;
  test_sse_large_event_doc *doc = (test_sse_large_event_doc *)dst;
  size_t text_len = doc->text ? strlen(doc->text) : 0u;

  (void)error;
  state->count++;
  if (doc->type == NULL || strcmp(doc->type, event->event) != 0) {
    state->invalid = 1;
  }
  if (strcmp(event->event, "response.output_text.delta") == 0) {
    state->delta_count++;
    if (text_len != 1u) {
      state->invalid = 1;
    }
  } else {
    state->finalization_count++;
    if (text_len < 9000u) {
      state->invalid = 1;
    }
  }
  if (text_len > state->max_text_len) {
    state->max_text_len = text_len;
  }
  return LONEJSON_STATUS_OK;
}

typedef struct test_sse_json_value_state {
  size_t count;
  size_t sink_total;
  int saw_failure;
  char captured[32768];
  size_t captured_len;
  test_visit_chunk_state visit;
} test_sse_json_value_state;

static lonejson_status test_sse_json_value_sink_write(void *user,
                                                      const void *data,
                                                      size_t len,
                                                      lonejson_error *error) {
  test_sse_json_value_state *state = (test_sse_json_value_state *)user;
  (void)data;
  (void)error;
  state->sink_total += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_sse_json_value_capture_event_cb(
    void *user, const lonejson_sse_event *event, lonejson_json_value *value,
    lonejson_error *error) {
  test_sse_json_value_state *state = (test_sse_json_value_state *)user;
  test_buffer_sink sink;
  lonejson_status status;

  (void)event;
  memset(&sink, 0, sizeof(sink));
  sink.buffer = (unsigned char *)state->captured;
  sink.capacity = sizeof(state->captured);
  status = lonejson_json_value_write_to_sink(value, test_buffer_sink_write,
                                             &sink, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->captured_len = sink.length;
  state->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_sse_json_value_count_event_cb(void *user, const lonejson_sse_event *event,
                                   lonejson_json_value *value,
                                   lonejson_error *error) {
  test_sse_json_value_state *state = (test_sse_json_value_state *)user;

  (void)event;
  (void)value;
  (void)error;
  state->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_sse_json_value_fail_cb(void *user, const lonejson_sse_event *event,
                            lonejson_json_value *value, lonejson_error *error) {
  test_sse_json_value_state *state = (test_sse_json_value_state *)user;

  (void)event;
  (void)value;
  state->saw_failure = 1;
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "intentional SSE JSON value callback failure");
}

static void test_sse_incremental_events_and_json_selection(void) {
  const char *chunks[] = {
      "id: abc\r",     "event: update\r\n", ": ignored\n", "retry: 1500\n",
      "data: first\r", "data: second\n",    "\n"};
  const char *json_chunks[] = {
      "event: keep\nid: j1\ndata: {\"id\":\"evtjsn\",\"ok\":true}\n\n",
      "event: skip\ndata: {\"id\":\"bad\",\"ok\":false}\n\n"};
  const char *selected[] = {"keep"};
  lonejson_sse_json_options json_options;
  test_sse_state state;
  test_sse_json_state json_state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  test_event event;
  lonejson_error error;
  lonejson_status status;
  size_t i;

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  for (i = 0u; i < sizeof(chunks) / sizeof(chunks[0]); ++i) {
    status = lonejson_sse_push(sse, chunks[i], strlen(chunks[i]), &handler,
                               &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_sse_finish(sse, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.data_count == 3u);
  EXPECT(strcmp(state.event, "update") == 0);
  EXPECT(strcmp(state.id, "abc") == 0);
  EXPECT(strcmp(state.data, "first\nsecond") == 0);
  EXPECT(state.data_len == strlen("first\nsecond"));
  EXPECT(state.has_retry);
  EXPECT(state.retry_ms == 1500u);
  lonejson_sse_close(sse);

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  for (i = 0u; i < sizeof(json_chunks) / sizeof(json_chunks[0]); ++i) {
    status = test_sse_push_json(sse, &test_event_map, &event, json_chunks[i],
                                strlen(json_chunks[i]), &json_options, NULL,
                                test_sse_json_event_cb, &json_state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status =
      test_sse_finish_json(sse, &test_event_map, &event, &json_options, NULL,
                           test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 1u);
  EXPECT(json_state.invalid == 0);
  EXPECT(strcmp(json_state.event, "keep") == 0);
  EXPECT(strcmp(json_state.id, "j1") == 0);
  EXPECT(json_state.data_len == strlen("{\"id\":\"evtjsn\",\"ok\":true}"));
  lonejson_sse_close(sse);
}

static void test_sse_json_streams_data_without_event_buffering(void) {
  static const char chunk0[] = "event: keep\nid: j2\ndata: {\"id\":\"";
  static const char chunk1[] = "evtjsn\",\n";
  static const char chunk2[] = "data: \"ok\":true}\n\n";
  static const char late_event[] =
      "data: {\"id\":\"evtjsn\",\"ok\":true}\nevent: keep\n\n";
  const char *empty_selected[] = {""};
  const char *selected[] = {"keep"};
  lonejson_sse_options sse_options;
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_event event;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = 64u;

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = test_sse_push_json(sse, &test_event_map, &event, chunk0,
                              sizeof(chunk0) - 1u, &json_options, NULL,
                              test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(sse, &test_event_map, &event, chunk1,
                              sizeof(chunk1) - 1u, &json_options, NULL,
                              test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(sse, &test_event_map, &event, chunk2,
                              sizeof(chunk2) - 1u, &json_options, NULL,
                              test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      test_sse_finish_json(sse, &test_event_map, &event, &json_options, NULL,
                           test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 1u);
  EXPECT(json_state.invalid == 0);
  EXPECT(strcmp(json_state.event, "keep") == 0);
  EXPECT(strcmp(json_state.id, "j2") == 0);
  EXPECT(json_state.data_len == strlen("{\"id\":\"evtjsn\",\n\"ok\":true}"));
  lonejson_sse_close(sse);

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  json_options.event_names = empty_selected;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = test_sse_push_json(sse, &test_event_map, &event, late_event,
                              sizeof(late_event) - 1u, &json_options, NULL,
                              test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_sse_close(sse);
}

static void test_sse_json_event_limits_and_boundary_errors(void) {
  static const char header[] = "event: keep\nid: ok\ndata: ";
  static const char payload[] = "{\"id\":\"evtjsn\",\"ok\":true}";
  static const char line_terminator[] = "\n";
  static const char event_terminator[] = "\n\n";
  static const char split_line[] = "data:\n";
  const char *selected[] = {"keep"};
  lonejson_sse_options sse_options;
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_event event;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  size_t payload_len;

  payload_len = strlen(payload);
  json_options.event_names = selected;
  json_options.event_name_count = 1u;

  memset(&event, 0, sizeof(event));
  memset(&json_state, 0, sizeof(json_state));
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = payload_len - 1u;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = test_sse_push_json(
      sse, &test_event_map, &event, header, strlen(header), &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, payload, payload_len, &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, event_terminator, strlen(event_terminator),
      &json_options, NULL, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(json_state.count == 0u);
  lonejson_sse_close(sse);

  memset(&event, 0, sizeof(event));
  memset(&json_state, 0, sizeof(json_state));
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = payload_len;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = test_sse_push_json(
      sse, &test_event_map, &event, header, strlen(header), &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, payload, payload_len, &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, line_terminator, strlen(line_terminator),
      &json_options, NULL, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      test_sse_finish_json(sse, &test_event_map, &event, &json_options, NULL,
                           test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 1u);
  EXPECT(json_state.data_len == payload_len);
  EXPECT(strcmp(json_state.event, "keep") == 0);
  EXPECT(strcmp(json_state.id, "ok") == 0);
  lonejson_sse_close(sse);

  memset(&event, 0, sizeof(event));
  memset(&json_state, 0, sizeof(json_state));
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = payload_len;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = test_sse_push_json(
      sse, &test_event_map, &event, header, strlen(header), &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, payload, payload_len, &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, split_line, strlen(split_line),
      &json_options, NULL, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(json_state.count == 0u);
  lonejson_sse_close(sse);
}

static void test_sse_json_event_limits_alloc_guarded_e2e(void) {
  static const char header[] = "event: keep\nid: ok\ndata: ";
  static const char payload[] = "{\"name\":\"overflow\",\"tags\":[\"x\",\"y\","
                                "\"z\"],\"body\":\"payload\"}";
  static const char terminator[] = "\n\n";
  test_allocator_state parse_alloc;
  lonejson__parse_options parse_options;
  lonejson_sse_options sse_options;
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_alloc_parse_doc doc;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  memset(&json_state, 0, sizeof(json_state));
  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&parse_alloc);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &parse_alloc.allocator;
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;

  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = strlen(payload) - 10u;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status =
      test_sse_push_json(sse, &test_alloc_parse_doc_map, &doc, header,
                         strlen(header), &json_options, &parse_options,
                         test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      test_sse_push_json(sse, &test_alloc_parse_doc_map, &doc, payload,
                         strlen(payload), &json_options, &parse_options,
                         test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      test_sse_push_json(sse, &test_alloc_parse_doc_map, &doc, terminator,
                         strlen(terminator), &json_options, &parse_options,
                         test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(json_state.count == 0u);
  EXPECT(parse_alloc.stats.alloc_calls == 0u);
  EXPECT(parse_alloc.stats.realloc_calls == 0u);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
  lonejson_sse_close(sse);
}

static void test_sse_json_large_event_sequence_and_cleanup(void) {
  static const size_t chunk_sizes[] = {1u, 7u, 97u, 1024u};
  const size_t delta_events = 64u;
  const size_t large_text_len = 10000u;
  size_t chunk_index;

  for (chunk_index = 0u;
       chunk_index < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]);
       ++chunk_index) {
    test_allocator_state parse_alloc;
    lonejson__parse_options parse_options;
    lonejson_sse_options sse_options;
    lonejson_sse_json_options json_options;
    test_sse_large_event_state state;
    test_sse_large_event_doc doc;
    lonejson__byte_buffer payload;
    lonejson_sse *sse;
    lonejson_error error;
    lonejson_status status;
    size_t i;

    memset(&doc, 0, sizeof(doc));
    memset(&payload, 0, sizeof(payload));
    memset(&state, 0, sizeof(state));
    test_allocator_init(&parse_alloc);
    parse_options = lonejson__default_parse_options();
    parse_options.allocator = &parse_alloc.allocator;
    json_options.event_names = NULL;
    json_options.event_name_count = 0u;
    sse_options = lonejson_default_sse_options();
    sse_options.max_event_data_bytes = 256u * 1024u;
    sse = lonejson_sse_open(&sse_options, &error);
    EXPECT(sse != NULL);
    if (sse == NULL) {
      return;
    }

    status = test_sse_build_large_json_payload(
        &payload, "response.created", "created", large_text_len, 'a', &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_sse_push_json_event_chunked(
        sse, &test_sse_large_event_doc_map, &doc, "response.created", "evt-1",
        &payload, chunk_sizes[chunk_index], &json_options, &parse_options,
        test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    status = test_sse_build_large_json_payload(&payload, "response.in_progress",
                                               "in_progress", large_text_len,
                                               'b', &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_sse_push_json_event_chunked(
        sse, &test_sse_large_event_doc_map, &doc, "response.in_progress",
        "evt-2", &payload, chunk_sizes[chunk_index], &json_options,
        &parse_options, test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    for (i = 0u; i < delta_events; ++i) {
      status = test_sse_build_large_json_payload(
          &payload, "response.output_text.delta", NULL, 1u, 'x', &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      status = test_sse_push_json_event_chunked(
          sse, &test_sse_large_event_doc_map, &doc,
          "response.output_text.delta", "evt-delta", &payload,
          chunk_sizes[chunk_index], &json_options, &parse_options,
          test_sse_large_event_cb, &state, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
    }

    status =
        test_sse_build_large_json_payload(&payload, "response.output_text.done",
                                          NULL, large_text_len, 'c', &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_sse_push_json_event_chunked(
        sse, &test_sse_large_event_doc_map, &doc, "response.output_text.done",
        "evt-3", &payload, chunk_sizes[chunk_index], &json_options,
        &parse_options, test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    status = test_sse_build_large_json_payload(
        &payload, "response.content_part.done", NULL, large_text_len, 'd',
        &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_sse_push_json_event_chunked(
        sse, &test_sse_large_event_doc_map, &doc, "response.content_part.done",
        "evt-4", &payload, chunk_sizes[chunk_index], &json_options,
        &parse_options, test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    status =
        test_sse_build_large_json_payload(&payload, "response.output_item.done",
                                          NULL, large_text_len, 'e', &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_sse_push_json_event_chunked(
        sse, &test_sse_large_event_doc_map, &doc, "response.output_item.done",
        "evt-5", &payload, chunk_sizes[chunk_index], &json_options,
        &parse_options, test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    status = test_sse_build_large_json_payload(&payload, "response.completed",
                                               "completed", large_text_len, 'f',
                                               &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = test_sse_push_json_event_chunked(
        sse, &test_sse_large_event_doc_map, &doc, "response.completed", "evt-6",
        &payload, chunk_sizes[chunk_index], &json_options, &parse_options,
        test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    status = test_sse_finish_json(sse, &test_sse_large_event_doc_map, &doc,
                                  &json_options, &parse_options,
                                  test_sse_large_event_cb, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(state.invalid == 0);
    EXPECT(state.delta_count == delta_events);
    EXPECT(state.finalization_count == 6u);
    EXPECT(state.count == delta_events + 6u);
    EXPECT(state.max_text_len == large_text_len);
    EXPECT(parse_alloc.stats.bytes_live != 0u);

    lonejson_sse_close(sse);
    lonejson__byte_free(&payload, NULL);
    lonejson_cleanup(&test_sse_large_event_doc_map, &doc, NULL);
    EXPECT(parse_alloc.stats.bytes_live == 0u);
  }
}

static void test_sse_json_parse_failure_cleans_current_destination(void) {
  test_allocator_state parse_alloc;
  lonejson__parse_options parse_options;
  lonejson_sse_json_options json_options;
  lonejson_sse_options sse_options;
  test_sse_large_event_doc doc;
  lonejson__byte_buffer payload;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  memset(&payload, 0, sizeof(payload));
  test_allocator_init(&parse_alloc);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &parse_alloc.allocator;
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = 256u * 1024u;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }

  status = test_sse_build_large_json_payload(&payload, "response.created",
                                             "created", 10000u, 'a', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_event_chunked(
      sse, &test_sse_large_event_doc_map, &doc, "response.created", "evt-1",
      &payload, 19u, &json_options, &parse_options, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.bytes_live != 0u);

  status = test_sse_build_large_json_payload(
      &payload, "response.output_item.done", NULL, 10000u, 'b', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(payload.len >= 2u);
  payload.len -= 2u;
  payload.data[payload.len] = '\0';
  status = test_sse_push_json_event_chunked(
      sse, &test_sse_large_event_doc_map, &doc, "response.output_item.done",
      "evt-2", &payload, 23u, &json_options, &parse_options, NULL, NULL,
      &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(parse_alloc.stats.bytes_live == 0u);

  lonejson_sse_close(sse);
  lonejson__byte_free(&payload, NULL);
  lonejson_cleanup(&test_sse_large_event_doc_map, &doc, NULL);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static lonejson_status test_sse_json_fail_cb(void *user,
                                             const lonejson_sse_event *event,
                                             void *dst, lonejson_error *error) {
  (void)user;
  (void)event;
  (void)dst;
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "intentional SSE JSON callback failure");
}

static void test_sse_json_callback_failure_cleans_destination(void) {
  test_allocator_state parse_alloc;
  lonejson__parse_options parse_options;
  lonejson_sse_json_options json_options;
  lonejson_sse_options sse_options;
  test_sse_large_event_doc doc;
  lonejson__byte_buffer payload;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  memset(&payload, 0, sizeof(payload));
  test_allocator_init(&parse_alloc);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &parse_alloc.allocator;
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = 256u * 1024u;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }

  status = test_sse_build_large_json_payload(&payload, "response.completed",
                                             "completed", 10000u, 'z', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_event_chunked(
      sse, &test_sse_large_event_doc_map, &doc, "response.completed", "evt-1",
      &payload, 31u, &json_options, &parse_options, test_sse_json_fail_cb, NULL,
      &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(parse_alloc.stats.bytes_live == 0u);

  lonejson_sse_close(sse);
  lonejson__byte_free(&payload, NULL);
  lonejson_cleanup(&test_sse_large_event_doc_map, &doc, NULL);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static void test_sse_json_value_capture_large_sequence_and_filtering(void) {
  const char *selected[] = {"keep", "done"};
  lonejson__parse_options parse_options;
  lonejson_sse_json_options json_options;
  lonejson_sse_options sse_options;
  lonejson_json_value value;
  lonejson__byte_buffer payload;
  test_sse_json_value_state state;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  memset(&payload, 0, sizeof(payload));
  memset(&state, 0, sizeof(state));
  parse_options = lonejson__default_parse_options();
  json_options.event_names = selected;
  json_options.event_name_count = 2u;
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = 256u * 1024u;
  lonejson_json_value_init(test_default_runtime(), &value);
  status = lonejson_json_value_enable_parse_capture(&value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    lonejson_json_value_cleanup(&value);
    return;
  }

  status = test_sse_build_large_json_payload(&payload, "response.created",
                                             "created", 64u, 'a', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "skip", "evt-0", &payload, 5u, &json_options, &parse_options,
      test_sse_json_value_capture_event_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 0u);

  status =
      test_sse_build_large_json_string_payload(&payload, 9000u, 'b', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "keep", "evt-1", &payload, 7u, &json_options, &parse_options,
      test_sse_json_value_capture_event_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(state.captured_len == payload.len);
  EXPECT(memcmp(state.captured, payload.data, payload.len) == 0);

  status = test_sse_build_large_json_payload(&payload, "response.completed",
                                             "done", 12000u, 'c', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "done", "evt-2", &payload, 17u, &json_options,
      &parse_options, test_sse_json_value_capture_event_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 2u);
  EXPECT(state.captured_len == payload.len);
  EXPECT(memcmp(state.captured, payload.data, payload.len) == 0);

  status = test_sse_finish_json_value(
      sse, &value, &json_options, &parse_options,
      test_sse_json_value_capture_event_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  lonejson_sse_close(sse);
  lonejson__byte_free(&payload, NULL);
  lonejson_json_value_cleanup(&value);
}

static void test_sse_json_value_sink_and_visitor_root_modes(void) {
  static const char *selected[] = {"keep"};
  static const struct {
    const char *payload;
    size_t chunk_bytes;
    size_t expect_sink_bytes;
    size_t expect_objects;
    size_t expect_arrays;
    size_t expect_strings;
    size_t expect_numbers;
    size_t expect_bools;
    size_t expect_nulls;
  } cases[] = {
      {"\"hello\"", 1u, 7u, 0u, 0u, 1u, 0u, 0u, 0u},
      {"42", 1u, 2u, 0u, 0u, 0u, 1u, 0u, 0u},
      {"true", 2u, 4u, 0u, 0u, 0u, 0u, 1u, 0u},
      {"null", 3u, 4u, 0u, 0u, 0u, 0u, 0u, 1u},
      {"[1,\"x\",false]", 2u, 13u, 0u, 1u, 1u, 1u, 1u, 0u},
      {"{\"k\":\"v\"}", 2u, 9u, 1u, 0u, 1u, 0u, 0u, 0u},
  };
  size_t i;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    lonejson__parse_options parse_options;
    lonejson_sse_json_options json_options;
    lonejson_value_visitor visitor;
    lonejson_error error;
    lonejson_status status;
    lonejson__byte_buffer payload;
    test_sse_json_value_state visit_state;
    test_sse_json_value_state sink_state;
    lonejson_json_value visit_value;
    lonejson_json_value sink_value;
    lonejson_sse *sse;

    memset(&payload, 0, sizeof(payload));
    memset(&visit_state, 0, sizeof(visit_state));
    memset(&sink_state, 0, sizeof(sink_state));
    parse_options = lonejson__default_parse_options();
    json_options.event_names = selected;
    json_options.event_name_count = 1u;
    status = lonejson__byte_append(&payload, cases[i].payload,
                                   strlen(cases[i].payload), SIZE_MAX - 1u,
                                   NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);

    lonejson_json_value_init(test_default_runtime(), &visit_value);
    visitor = test_counting_value_visitor();
    status = lonejson_json_value_set_parse_visitor(&visit_value, &visitor,
                                                   &visit_state.visit, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    sse = lonejson_sse_open(NULL, &error);
    EXPECT(sse != NULL);
    if (sse != NULL) {
      status = test_sse_push_json_value_event_chunked(
          sse, &visit_value, "keep", "evt-1", &payload, cases[i].chunk_bytes,
          &json_options, &parse_options, test_sse_json_value_count_event_cb,
          &visit_state, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(visit_state.count == 1u);
      lonejson_sse_close(sse);
    }

    lonejson_json_value_init(test_default_runtime(), &sink_value);
    status = lonejson_json_value_set_parse_sink(
        &sink_value, test_sse_json_value_sink_write, &sink_state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    sse = lonejson_sse_open(NULL, &error);
    EXPECT(sse != NULL);
    if (sse != NULL) {
      status = test_sse_push_json_value_event_chunked(
          sse, &sink_value, "keep", "evt-2", &payload, cases[i].chunk_bytes,
          &json_options, &parse_options, test_sse_json_value_count_event_cb,
          &sink_state, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(sink_state.count == 1u);
      EXPECT(sink_state.sink_total == cases[i].expect_sink_bytes);
      EXPECT(sink_value.kind == LONEJSON_JSON_VALUE_NULL);
      lonejson_sse_close(sse);
    }

    EXPECT(visit_state.visit.object_count == cases[i].expect_objects);
    EXPECT(visit_state.visit.array_count == cases[i].expect_arrays);
    EXPECT(visit_state.visit.string_begin_count == cases[i].expect_strings);
    EXPECT(visit_state.visit.number_begin_count == cases[i].expect_numbers);
    EXPECT(visit_state.visit.bool_count == cases[i].expect_bools);
    EXPECT(visit_state.visit.null_count == cases[i].expect_nulls);

    lonejson__byte_free(&payload, NULL);
    lonejson_json_value_cleanup(&visit_value);
    lonejson_json_value_cleanup(&sink_value);
  }
}

static void test_sse_json_value_failure_cleanup_and_reuse(void) {
  lonejson__parse_options parse_options;
  lonejson_sse_json_options json_options;
  lonejson_json_value value;
  lonejson__byte_buffer payload;
  test_sse_json_value_state state;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  memset(&payload, 0, sizeof(payload));
  memset(&state, 0, sizeof(state));
  parse_options = lonejson__default_parse_options();
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;
  lonejson_json_value_init(test_default_runtime(), &value);
  status = lonejson_json_value_enable_parse_capture(&value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    lonejson_json_value_cleanup(&value);
    return;
  }

  status = test_sse_build_large_json_payload(&payload, "response.completed",
                                             "done", 4096u, 'q', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "keep", "evt-1", &payload, 11u, &json_options,
      &parse_options, test_sse_json_value_fail_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.saw_failure);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(value.len == 0u);

  state.saw_failure = 0;
  status = test_sse_build_large_json_payload(&payload, "response.completed",
                                             "done", 4096u, 'r', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(payload.len >= 1u);
  payload.len -= 1u;
  payload.data[payload.len] = '\0';
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "keep", "evt-2", &payload, 13u, &json_options,
      &parse_options, test_sse_json_value_capture_event_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(value.len == 0u);

  status = test_sse_build_large_json_payload(&payload, "response.completed",
                                             "done", 4096u, 's', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "keep", "evt-3", &payload, 19u, &json_options,
      &parse_options, test_sse_json_value_capture_event_cb, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.captured_len == payload.len);
  EXPECT(memcmp(state.captured, payload.data, payload.len) == 0);

  lonejson_sse_close(sse);
  lonejson__byte_free(&payload, NULL);
  lonejson_json_value_cleanup(&value);
}

static void test_sse_json_rejects_runtime_changes_mid_event(void) {
  static const char chunk0[] = "event: data\ndata: {\"id\":\"evt-1\",\n";
  static const char chunk1[] = "data: \"ok\":true}\n\n";
  lonejson_sse *sse;
  lonejson_config config;
  lonejson *first_lj;
  lonejson *second_lj;
  lonejson_error error;
  lonejson_status status;
  test_event event;
  lonejson_json_value value;

  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }

  config = lonejson_default_config();
  first_lj = lonejson_new(&config, &error);
  EXPECT(first_lj != NULL);
  second_lj = lonejson_new(&config, &error);
  EXPECT(second_lj != NULL);
  if (first_lj == NULL || second_lj == NULL) {
    lonejson_free(first_lj);
    lonejson_free(second_lj);
    lonejson_sse_close(sse);
    return;
  }

  memset(&event, 0, sizeof(event));
  status =
      lonejson_sse_push_json(first_lj, sse, &test_event_map, &event, chunk0,
                             strlen(chunk0), NULL, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      lonejson_sse_push_json(second_lj, sse, &test_event_map, &event, chunk1,
                             strlen(chunk1), NULL, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "arguments changed during an event") != NULL);

  lonejson_json_value_init(second_lj, &value);
  EXPECT(lonejson_json_value_enable_parse_capture(&value, &error) ==
         LONEJSON_STATUS_OK);
  lonejson__sse_json_reset_event(lonejson__sse_state_mut(sse));
  status = lonejson_sse_push_json_value(
      first_lj, sse, &value, chunk0, strlen(chunk0), NULL, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json_value(
      second_lj, sse, &value, chunk1, strlen(chunk1), NULL, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "arguments changed during an event") != NULL);

  lonejson_json_value_cleanup(&value);
  lonejson_free(first_lj);
  lonejson_free(second_lj);
  lonejson_sse_close(sse);
}

static void
test_sse_json_mixed_map_and_value_failures_preserve_prior_results(void) {
  lonejson__parse_options parse_options;
  lonejson_sse_json_options json_options;
  test_allocator_state parse_alloc;
  test_sse_large_event_doc doc;
  lonejson_json_value value;
  lonejson__byte_buffer payload;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  size_t mapped_bytes_live;
  size_t value_bytes_live;
  size_t captured_value_len;

  memset(&doc, 0, sizeof(doc));
  memset(&payload, 0, sizeof(payload));
  test_allocator_init(&parse_alloc);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &parse_alloc.allocator;
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;
  lonejson_json_value_init(test_default_runtime(), &value);
  status = lonejson_json_value_enable_parse_capture(&value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    lonejson_json_value_cleanup(&value);
    return;
  }

  status = test_sse_build_large_json_payload(&payload, "response.completed",
                                             "done", 256u, 'm', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_event_chunked(
      sse, &test_sse_large_event_doc_map, &doc, "mapped", "evt-map", &payload,
      13u, &json_options, &parse_options, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.type != NULL && strcmp(doc.type, "response.completed") == 0);
  EXPECT(doc.status != NULL && strcmp(doc.status, "done") == 0);
  EXPECT(doc.text != NULL && strlen(doc.text) == 256u);
  mapped_bytes_live = parse_alloc.stats.bytes_live;
  EXPECT(mapped_bytes_live != 0u);

  status = test_sse_build_large_json_string_payload(&payload, 96u, 'v', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(payload.len >= 1u);
  payload.len -= 1u;
  payload.data[payload.len] = '\0';
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "value", "evt-value-bad", &payload, 11u, &json_options,
      &parse_options, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(doc.type != NULL && strcmp(doc.type, "response.completed") == 0);
  EXPECT(doc.status != NULL && strcmp(doc.status, "done") == 0);
  EXPECT(doc.text != NULL && strlen(doc.text) == 256u);
  EXPECT(parse_alloc.stats.bytes_live == mapped_bytes_live);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(value.len == 0u);

  status = test_sse_build_large_json_string_payload(&payload, 96u, 'w', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json_value_event_chunked(
      sse, &value, "value", "evt-value", &payload, 17u, &json_options,
      &parse_options, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(value.len == payload.len);
  captured_value_len = value.len;
  value_bytes_live = parse_alloc.stats.bytes_live;
  EXPECT(value_bytes_live >= mapped_bytes_live);

  status = test_sse_build_large_json_payload(&payload, "response.failed",
                                             "broken", 192u, 'x', &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(payload.len >= 2u);
  payload.len -= 2u;
  payload.data[payload.len] = '\0';
  status = test_sse_push_json_event_chunked(
      sse, &test_sse_large_event_doc_map, &doc, "mapped", "evt-map-bad",
      &payload, 19u, &json_options, &parse_options, NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(value.len == captured_value_len);

  lonejson_sse_close(sse);
  lonejson__byte_free(&payload, NULL);
  lonejson_json_value_cleanup(&value);
  lonejson_cleanup(&test_sse_large_event_doc_map, &doc, NULL);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static void test_sse_retry_and_json_metadata_only_events(void) {
  static const char valid_retry[] = "retry: 1500\n\n";
  static const char invalid_retries[] =
      "retry: -1\n\n"
      "retry: +1\n\n"
      "retry:  1\n\n"
      "retry: 1844674407370955161518446744073709551615\n\n";
  static const char data_event[] = "data: x\n\n";
  static const char json_metadata_only[] = "event: keep\nretry: 1500\n\n";
  static const char json_invalid_retry_only[] = "event: keep\nretry: -1\n\n";
  const char *selected[] = {"keep"};
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  test_event event;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push(sse, valid_retry, sizeof(valid_retry) - 1u,
                             &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(state.has_retry);
  EXPECT(state.retry_ms == 1500u);
  status = lonejson_sse_push(sse, invalid_retries, sizeof(invalid_retries) - 1u,
                             &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  status = lonejson_sse_push(sse, data_event, sizeof(data_event) - 1u, &handler,
                             &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 2u);
  EXPECT(!state.has_retry);
  lonejson_sse_close(sse);

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status =
      test_sse_push_json(sse, &test_event_map, &event, json_metadata_only,
                         sizeof(json_metadata_only) - 1u, &json_options, NULL,
                         test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_sse_push_json(
      sse, &test_event_map, &event, json_invalid_retry_only,
      sizeof(json_invalid_retry_only) - 1u, &json_options, NULL,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      test_sse_finish_json(sse, &test_event_map, &event, &json_options, NULL,
                           test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 0u);
  lonejson_sse_close(sse);
}

static void test_sse_callback_failure_status(void) {
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  static const char event[] = "data: x\n\n";

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  state.fail = 1;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push(sse, event, sizeof(event) - 1u, &handler, &state,
                             &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_sse_close(sse);
}

static void test_sse_limits_and_allocation_contract(void) {
  static const char event[] = "data: hello\n\n";
  lonejson_sse_options options;
  lonejson_allocator partial_allocator;
  test_allocator_state alloc;
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  size_t alloc_calls_after_open;
  size_t realloc_calls_after_open;

  memset(&partial_allocator, 0, sizeof(partial_allocator));
  partial_allocator.malloc_fn = test_allocator_malloc;
  options = lonejson_default_sse_options();
  options.allocator = &partial_allocator;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  test_allocator_init(&alloc);
  options = lonejson_default_sse_options();
  options.allocator = &alloc.allocator;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push(sse, ": warmup\n", strlen(": warmup\n"), NULL,
                             NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  alloc_calls_after_open = alloc.stats.alloc_calls;
  realloc_calls_after_open = alloc.stats.realloc_calls;
  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  status = lonejson_sse_push(sse, event, sizeof(event) - 1u, &handler, &state,
                             &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(alloc.stats.alloc_calls == alloc_calls_after_open);
  EXPECT(alloc.stats.realloc_calls == realloc_calls_after_open);
  status = lonejson_sse_finish(sse, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push(sse, event, sizeof(event) - 1u, &handler, &state,
                             &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_sse_close(sse);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_sse_limits_and_invalid_usage(void) {
  lonejson_sse_options options;
  lonejson_sse *sse;
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  options = lonejson_default_sse_options();
  options.max_line_bytes = 4u;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse != NULL) {
    status = lonejson_sse_push(sse, "data: abc\n", strlen("data: abc\n"),
                               &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
    lonejson_sse_close(sse);
  }

  options = lonejson_default_sse_options();
  options.max_event_data_bytes = 3u;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse != NULL) {
    status = lonejson_sse_push(sse, "data: abcd\n\n", strlen("data: abcd\n\n"),
                               &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
    lonejson_sse_close(sse);
  }

  status = lonejson_sse_push(NULL, "", 0u, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  options = lonejson_default_sse_options();
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse != NULL) {
    status = lonejson_sse_push(sse, NULL, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_sse_close(sse);
  }
}

static void test_multipart_incremental_parts_and_raw_payloads(void) {
  static const char payload[] = "raw\r\n--abc\r\nbytes\npayload";
  static const char prefix[] =
      "--abc\r\n"
      "Content-Disposition: form-data; name=\"meta\"\r\n"
      "Content-Type: application/json\r\n"
      "\r\n"
      "{\"ok\":true}\r\n"
      "--abc\r\n"
      "Content-Disposition: form-data; name=\"payload\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Length: 25\r\n"
      "\r\n";
  static const char suffix[] = "\r\n--abc--\r\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;
  size_t i;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("Multipart/Related; boundary = \"abc\"", NULL,
                               &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  for (i = 0u; i < sizeof(prefix) - 1u; ++i) {
    status =
        lonejson_multipart_push(mp, prefix + i, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  for (i = 0u; i < sizeof(payload) - 1u; ++i) {
    status =
        lonejson_multipart_push(mp, payload + i, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  for (i = 0u; i < sizeof(suffix) - 1u; ++i) {
    status =
        lonejson_multipart_push(mp, suffix + i, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(state.names[0], "meta") == 0);
  EXPECT(strcmp(state.content_types[0], "application/json") == 0);
  EXPECT(state.lengths[0] == -1);
  EXPECT(strcmp(state.names[1], "payload") == 0);
  EXPECT(strcmp(state.content_types[1], "application/octet-stream") == 0);
  EXPECT(state.lengths[1] == 25);
  EXPECT(strcmp(state.data, "{\"ok\":true}raw\r\n--abc\r\nbytes\npayload") ==
         0);
  lonejson_multipart_close(mp);
}

static void test_multipart_no_length_preserves_body_bytes(void) {
  static const char body[] = "--b\n"
                             "Content-Disposition: form-data; name=text\n"
                             "\n"
                             "line1\r\nline2\n"
                             "--b--\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=b", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, body, sizeof(body) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(strcmp(state.names[0], "text") == 0);
  EXPECT(strcmp(state.data, "line1\r\nline2") == 0);
  lonejson_multipart_close(mp);
}

static void test_multipart_no_length_streams_long_lines_with_lookahead(void) {
  static const char prefix[] = "--x\r\n\r\n";
  static const char chunk0[] = "aaaaaaaaaaaaaaaa";
  static const char chunk1[] = "bbbbbbbbbbbbbbbb";
  static const char chunk2[] = "cccccccccccccccc";
  static const char suffix[] = "\r\n--x--\r\n";
  static const char false_boundary_body[] = "--xnot-a-delimiter";
  lonejson_multipart_options options;
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  options = lonejson_default_multipart_options();
  options.max_part_buffered_bytes = 9u;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, prefix, sizeof(prefix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk0, sizeof(chunk0) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk1, sizeof(chunk1) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk2, sizeof(chunk2) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.data_len == strlen(chunk0) + strlen(chunk1) + strlen(chunk2));
  EXPECT(strcmp(state.data,
                "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbcccccccccccccccc") == 0);
  status = lonejson_multipart_push(mp, suffix, sizeof(suffix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, prefix, sizeof(prefix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, false_boundary_body,
                                   sizeof(false_boundary_body) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(state.data, false_boundary_body) == 0);
  status = lonejson_multipart_push(mp, suffix, sizeof(suffix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 1u);
  lonejson_multipart_close(mp);
}

static void
test_multipart_content_length_streams_body_without_body_allocations(void) {
  static const char prefix[] =
      "--stream\r\n"
      "Content-Disposition: form-data; name=\"payload\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Length: 17\r\n"
      "\r\n";
  static const char chunk0[] = "abc";
  static const char chunk1[] = "defgh";
  static const char chunk2[] = "ijklmnopq";
  static const char suffix[] = "\r\n--stream--\r\n";
  lonejson_multipart_options options;
  lonejson_multipart_handler handler;
  test_allocator_state alloc;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  test_allocator_init(&alloc);
  options = lonejson_default_multipart_options();
  options.allocator = &alloc.allocator;
  memset(&state, 0, sizeof(state));
  state.stats = &alloc.stats;
  mp = lonejson_multipart_open("multipart/form-data; boundary=stream", &options,
                               &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, prefix, sizeof(prefix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.data_count == 0u);
  state.alloc_calls_after_headers = alloc.stats.alloc_calls;
  state.realloc_calls_after_headers = alloc.stats.realloc_calls;
  state.check_no_body_alloc = 1;

  status = lonejson_multipart_push(mp, chunk0, sizeof(chunk0) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk1, sizeof(chunk1) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk2, sizeof(chunk2) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls == state.alloc_calls_after_headers);
  EXPECT(alloc.stats.realloc_calls == state.realloc_calls_after_headers);
  EXPECT(state.saw_body_alloc == 0);
  EXPECT(state.data_count == 3u);
  EXPECT(state.data_chunks[0] == sizeof(chunk0) - 1u);
  EXPECT(state.data_chunks[1] == sizeof(chunk1) - 1u);
  EXPECT(state.data_chunks[2] == sizeof(chunk2) - 1u);
  EXPECT(strcmp(state.data, "abcdefghijklmnopq") == 0);
  state.check_no_body_alloc = 0;

  status = lonejson_multipart_push(mp, suffix, sizeof(suffix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 1u);
  EXPECT(strcmp(state.names[0], "payload") == 0);
  EXPECT(strcmp(state.content_types[0], "application/octet-stream") == 0);
  EXPECT(state.lengths[0] == 17);
  lonejson_multipart_close(mp);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_multipart_content_disposition_name_parameters(void) {
  static const char body[] =
      "--d\n"
      "Content-Disposition: form-data; filename=\"wrong\"; NAME=\"right\"\n"
      "\n"
      "one\n"
      "--d\n"
      "Content-Disposition: form-data; filename=\"wrong\"; "
      "name=\"pay\\\"load\"\n"
      "\n"
      "two\n"
      "--d--\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/form-data; boundary=d", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, body, sizeof(body) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(state.names[0], "right") == 0);
  EXPECT(strcmp(state.names[1], "pay\"load") == 0);
  lonejson_multipart_close(mp);
}

static void test_multipart_limits_and_invalid_usage(void) {
  static const char invalid_header[] = "--x\nnot-a-header\n\nvalue\n--x--\n";
  static const char too_many_headers[] = "--x\nA: 1\nB: 2\n\nvalue\n--x--\n";
  static const char bad_length[] = "--x\nContent-Length: 12x\n\nvalue\n--x--\n";
  static const char huge_length[] =
      "--x\nContent-Length: 999999999999999999999999999999\n\nvalue\n--x--\n";
  static const char too_small_boundary_lookahead[] = "--x\n\n--x--\n";
  lonejson_multipart_options options;
  lonejson_multipart_handler handler;
  lonejson_allocator partial_allocator;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&partial_allocator, 0, sizeof(partial_allocator));
  partial_allocator.malloc_fn = test_allocator_malloc;
  options = lonejson_default_multipart_options();
  options.allocator = &partial_allocator;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  mp = lonejson_multipart_open("multipart/mixed", NULL, &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  mp = lonejson_multipart_open("multipart/mixed; boundary=\"unterminated", NULL,
                               &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  options = lonejson_default_multipart_options();
  options.max_boundary_bytes = 3u;
  mp = lonejson_multipart_open("multipart/mixed; boundary=abcd", &options,
                               &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status =
        lonejson_multipart_push(mp, invalid_header, sizeof(invalid_header) - 1u,
                                &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    lonejson_multipart_close(mp);
  }

  options = lonejson_default_multipart_options();
  options.max_header_count = 1u;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, too_many_headers,
                                     sizeof(too_many_headers) - 1u, &handler,
                                     &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_multipart_close(mp);
  }

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, bad_length, sizeof(bad_length) - 1u,
                                     &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    lonejson_multipart_close(mp);
  }

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, huge_length, sizeof(huge_length) - 1u,
                                     &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_multipart_close(mp);
  }

  options = lonejson_default_multipart_options();
  options.max_part_buffered_bytes = 4u;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, too_small_boundary_lookahead,
                                     sizeof(too_small_boundary_lookahead) - 1u,
                                     &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_multipart_close(mp);
  }

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, "--x--\n", strlen("--x--\n"), &handler,
                                     &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_multipart_finish(mp, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_multipart_push(mp, "--x--\n", strlen("--x--\n"), &handler,
                                     &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_multipart_close(mp);
  }
}

static void test_multipart_failures_are_reported(void) {
  static const char incomplete[] = "--x\nContent-Type: text/plain\n\nvalue\n";
  static const char with_data[] =
      "--x\nContent-Type: text/plain\nContent-Length: 5\n\nvalue\n--x--\n";
  static const char empty_part[] = "--x\nContent-Type: text/plain\n\n\n--x--\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, incomplete, sizeof(incomplete) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  state.fail_data = 1;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, with_data, sizeof(with_data) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  state.fail_begin = 1;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, empty_part, sizeof(empty_part) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  state.fail_end = 1;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, empty_part, sizeof(empty_part) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_multipart_close(mp);
}
