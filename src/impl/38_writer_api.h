typedef enum lonejson__writer_mode {
  LONEJSON__WRITER_MODE_SINK = 1,
  LONEJSON__WRITER_MODE_GENERATOR = 2
} lonejson__writer_mode;

typedef enum lonejson__writer_frame_kind {
  LONEJSON__WRITER_FRAME_OBJECT = 1,
  LONEJSON__WRITER_FRAME_ARRAY = 2
} lonejson__writer_frame_kind;

typedef struct lonejson__writer_frame {
  lonejson__writer_frame_kind kind;
  size_t count;
  int after_key;
} lonejson__writer_frame;

typedef struct lonejson__writer_value_frame {
  lonejson__writer_frame_kind kind;
  size_t count;
  int after_key;
} lonejson__writer_value_frame;

typedef enum lonejson__writer_event_kind {
  LONEJSON__WRITER_EVENT_NONE = 0,
  LONEJSON__WRITER_EVENT_STRING = 1,
  LONEJSON__WRITER_EVENT_STRING_CHUNK = 2,
  LONEJSON__WRITER_EVENT_KEY = 3,
  LONEJSON__WRITER_EVENT_BYTES = 4,
  LONEJSON__WRITER_EVENT_CHILD = 5,
  LONEJSON__WRITER_EVENT_SCALAR_BYTES = 6
} lonejson__writer_event_kind;

typedef struct lonejson__writer_state {
  lonejson__writer_mode mode;
  const lonejson_runtime *runtime;
  lonejson_runtime runtime_storage;
  lonejson__write_options options;
  lonejson_allocator allocator;
  lonejson_sink_fn sink;
  void *sink_user;
  lonejson_error *external_error;
  lonejson__writer_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  int root_written;
  int finished;
  int string_open;
  int number_open;
  lonejson__writer_event_kind event_kind;
  const void *event_data;
  const void *event_data2;
  size_t event_len;
  size_t event_off;
  unsigned event_phase;
  unsigned char event_buffer[64];
  lonejson_generator event_child;
  unsigned char event_child_buffer[512];
  int event_child_active;
  int failed;
  int value_stream_active;
  int string_reader_active;
  lonejson_reader_fn string_reader;
  void *string_reader_user;
  unsigned char string_reader_buffer[4096];
  size_t string_reader_buffer_len;
  size_t string_reader_buffer_off;
  int string_reader_eof;
  lonejson__byte_buffer number;
  size_t max_number_bytes;
} lonejson__writer_state;

typedef struct lonejson__writer_value_stream_state {
  lonejson_writer *writer;
  lonejson__writer_state *writer_state;
  lonejson_parser *parser;
  lonejson_json_value json_value;
  lonejson_value_visitor visitor;
  lonejson__writer_value_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  size_t total_bytes;
  size_t max_total_bytes;
  int root_written;
  int number_open;
  int string_open;
  int closed;
  int failed;
  lonejson_allocator allocator;
} lonejson__writer_value_stream_state;

static void lonejson__writer_assign_methods(lonejson_writer *writer) {
  if (writer == NULL) {
    return;
  }
  writer->cleanup = lonejson_writer_cleanup;
  writer->begin_object = lonejson_writer_begin_object;
  writer->end_object = lonejson_writer_end_object;
  writer->begin_array = lonejson_writer_begin_array;
  writer->end_array = lonejson_writer_end_array;
  writer->key = lonejson_writer_key;
  writer->string = lonejson_writer_string;
  writer->string_begin = lonejson_writer_string_begin;
  writer->string_chunk = lonejson_writer_string_chunk;
  writer->string_end = lonejson_writer_string_end;
  writer->string_reader = lonejson_writer_string_reader;
  writer->string_spooled = lonejson_writer_string_spooled;
  writer->source_text = lonejson_writer_source_text;
  writer->spooled_base64 = lonejson_writer_spooled_base64;
  writer->source_base64 = lonejson_writer_source_base64;
  writer->number_text = lonejson_writer_number_text;
  writer->number_begin = lonejson_writer_number_begin;
  writer->number_chunk = lonejson_writer_number_chunk;
  writer->number_end = lonejson_writer_number_end;
  writer->i64 = lonejson_writer_i64;
  writer->u64 = lonejson_writer_u64;
  writer->f64 = lonejson_writer_f64;
  writer->bool_fn = lonejson_writer_bool;
  writer->null_fn = lonejson_writer_null;
  writer->json_value = lonejson_writer_json_value;
  writer->json_value_reader = lonejson_writer_json_value_reader;
  writer->json_value_buffer = lonejson_writer_json_value_buffer;
  writer->json_value_file = lonejson_writer_json_value_file;
  writer->json_value_fd = lonejson_writer_json_value_fd;
  writer->json_value_path = lonejson_writer_json_value_path;
  writer->json_value_spooled = lonejson_writer_json_value_spooled;
  writer->array_items_reader = lonejson_writer_array_items_reader;
  writer->array_items_buffer = lonejson_writer_array_items_buffer;
  writer->array_items_filep = lonejson_writer_array_items_filep;
  writer->array_items_fd = lonejson_writer_array_items_fd;
  writer->array_items_path = lonejson_writer_array_items_path;
  writer->array_items_spooled = lonejson_writer_array_items_spooled;
  writer->mapped = lonejson_writer_mapped;
  writer->finish = lonejson_writer_finish;
}

static void lonejson__writer_value_stream_assign_methods(
    lonejson_writer_value_stream *stream) {
  if (stream == NULL) {
    return;
  }
  stream->open = lonejson_writer_value_stream_open;
  stream->push = lonejson_writer_value_stream_push;
  stream->close = lonejson_writer_value_stream_close;
  stream->cleanup = lonejson_writer_value_stream_cleanup;
}

static const lonejson_runtime *
lonejson__writer_runtime(const lonejson__writer_state *state) {
  if (state == NULL) {
    return NULL;
  }
  return state->runtime;
}

static const lonejson__parse_options *
lonejson__writer_runtime_parse_options(const lonejson_writer *writer) {
  const lonejson__writer_state *state;
  const lonejson_runtime *runtime;

  state = writer != NULL ? (const lonejson__writer_state *)writer->state : NULL;
  runtime = lonejson__writer_runtime(state);
  return runtime != NULL ? &runtime->parse_options : NULL;
}

static lonejson_error *lonejson__writer_error_target(lonejson_writer *writer,
                                                     lonejson_error *error) {
  lonejson__writer_state *state;

  state = writer != NULL ? (lonejson__writer_state *)writer->state : NULL;
  if (error != NULL) {
    return error;
  }
  if (writer != NULL) {
    return &writer->error;
  }
  return state != NULL ? state->external_error : NULL;
}

static lonejson_status lonejson__writer_set_error(lonejson_writer *writer,
                                                  lonejson_error *error,
                                                  lonejson_status status,
                                                  const char *message) {
  return lonejson__set_error(lonejson__writer_error_target(writer, error),
                             status, 0u, 0u, 0u, "%s", message);
}

static lonejson_status lonejson__writer_fail(lonejson_writer *writer,
                                             lonejson_error *error,
                                             lonejson_status status,
                                             const char *message) {
  lonejson__writer_state *state;

  state = writer != NULL ? (lonejson__writer_state *)writer->state : NULL;
  if (state != NULL) {
    state->failed = 1;
  }
  return lonejson__writer_set_error(writer, error, status, message);
}

static lonejson_status
lonejson__writer_require_available(lonejson_writer *writer,
                                   lonejson_error *error) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is still open");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_before_value(lonejson_writer *writer,
                                                     lonejson_error *error);

static lonejson_status lonejson__writer_emit(lonejson_writer *writer,
                                             const void *data, size_t len,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (writer == NULL || writer->state == NULL || data == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and data are required");
  }
  state = (lonejson__writer_state *)writer->state;
  status = state->sink(state->sink_user, data, len,
                       error != NULL ? error : &writer->error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    state->failed = 1;
    if (error == NULL && writer->error.code == LONEJSON_STATUS_OK) {
      lonejson__set_error(&writer->error, status, 0u, 0u, 0u,
                          "writer sink failed");
    }
  }
  return status;
}

static lonejson_status lonejson__writer_emit_cstr(lonejson_writer *writer,
                                                  const char *text,
                                                  lonejson_error *error) {
  return lonejson__writer_emit(writer, text, LONEJSON__TEXT_LEN(text), error);
}

static int lonejson__writer_output_blocked(lonejson_writer *writer) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return 0;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->mode != LONEJSON__WRITER_MODE_GENERATOR) {
    return 0;
  }
  return lonejson__writer_generator_output_blocked(state->sink_user);
}

static void lonejson__writer_clear_event(lonejson__writer_state *state) {
  if (state->event_child_active) {
    lonejson_generator_cleanup(&state->event_child);
    state->event_child_active = 0;
  }
  state->event_kind = LONEJSON__WRITER_EVENT_NONE;
  state->event_data = NULL;
  state->event_data2 = NULL;
  state->event_len = 0u;
  state->event_off = 0u;
  state->event_phase = 0u;
}

static lonejson_status
lonejson__writer_begin_event(lonejson_writer *writer,
                             lonejson__writer_event_kind kind, const void *data,
                             size_t len, lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  status = lonejson__writer_require_available(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (state->event_kind != LONEJSON__WRITER_EVENT_NONE) {
    if (state->event_kind != kind || state->event_data != data ||
        state->event_len != len) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
          "active writer event must be resumed with the same arguments");
    }
    return LONEJSON_STATUS_OK;
  }
  state->event_kind = kind;
  state->event_data = data;
  state->event_data2 = NULL;
  state->event_len = len;
  state->event_off = 0u;
  state->event_phase = 0u;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_string_reader_emit_byte(
    lonejson_writer *writer, unsigned char byte, lonejson_error *error) {
  char escape_buf[7];

  if (byte >= 0x20u && byte != '"' && byte != '\\') {
    return lonejson__writer_emit(writer, &byte, 1u, error);
  }
  switch (byte) {
  case '"':
    return lonejson__writer_emit_cstr(writer, "\\\"", error);
  case '\\':
    return lonejson__writer_emit_cstr(writer, "\\\\", error);
  case '\b':
    return lonejson__writer_emit_cstr(writer, "\\b", error);
  case '\f':
    return lonejson__writer_emit_cstr(writer, "\\f", error);
  case '\n':
    return lonejson__writer_emit_cstr(writer, "\\n", error);
  case '\r':
    return lonejson__writer_emit_cstr(writer, "\\r", error);
  case '\t':
    return lonejson__writer_emit_cstr(writer, "\\t", error);
  default:
    escape_buf[0] = '\\';
    escape_buf[1] = 'u';
    escape_buf[2] = '0';
    escape_buf[3] = '0';
    escape_buf[4] = "0123456789abcdef"[(byte >> 4u) & 0x0Fu];
    escape_buf[5] = "0123456789abcdef"[byte & 0x0Fu];
    escape_buf[6] = '\0';
    return lonejson__writer_emit(writer, escape_buf, 6u, error);
  }
}

static lonejson_status
lonejson__writer_emit_escaped_event(lonejson_writer *writer,
                                    lonejson_error *error) {
  lonejson__writer_state *state;
  const unsigned char *data;
  lonejson_status status;

  state = (lonejson__writer_state *)writer->state;
  data = (const unsigned char *)state->event_data;
  while (state->event_off < state->event_len) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_string_reader_emit_byte(
        writer, data[state->event_off], error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_off++;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_emit_bytes_event(lonejson_writer *writer,
                                  lonejson_error *error) {
  lonejson__writer_state *state;
  const unsigned char *data;
  lonejson_status status;

  state = (lonejson__writer_state *)writer->state;
  data = (const unsigned char *)state->event_data;
  while (state->event_off < state->event_len) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit(writer, data + state->event_off, 1u, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_off++;
  }
  return LONEJSON_STATUS_OK;
}

static size_t lonejson__writer_format_u64(char *buffer, size_t capacity,
                                          lonejson_uint64 value) {
  size_t idx;

  idx = capacity;
  do {
    lonejson_uint64 digit = value % 10u;
    value /= 10u;
    --idx;
    buffer[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  memmove(buffer, buffer + idx, capacity - idx);
  return capacity - idx;
}

static size_t lonejson__writer_format_i64(char *buffer, size_t capacity,
                                          lonejson_int64 value) {
  lonejson_uint64 magnitude;
  size_t len;

  if (value >= 0) {
    return lonejson__writer_format_u64(buffer, capacity,
                                       (lonejson_uint64)value);
  }
  buffer[0] = '-';
  magnitude = (lonejson_uint64)(-(value + 1)) + 1u;
  len = lonejson__writer_format_u64(buffer + 1u, capacity - 1u, magnitude);
  return len + 1u;
}

static lonejson_status
lonejson__writer_emit_scalar_bytes(lonejson_writer *writer, const void *data,
                                   size_t len, lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL || (data == NULL && len != 0u) ||
      len > 64u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer scalar bytes are invalid");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_kind == LONEJSON__WRITER_EVENT_SCALAR_BYTES) {
    if (state->event_len != len ||
        memcmp(state->event_buffer, data, len) != 0) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
          "active writer event must be resumed with the same arguments");
    }
  } else if (state->event_kind == LONEJSON__WRITER_EVENT_NONE) {
    memcpy(state->event_buffer, data, len);
    status = lonejson__writer_begin_event(writer,
                                          LONEJSON__WRITER_EVENT_SCALAR_BYTES,
                                          state->event_buffer, len, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else {
    return lonejson__writer_begin_event(writer,
                                        LONEJSON__WRITER_EVENT_SCALAR_BYTES,
                                        state->event_buffer, len, error);
  }
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 1u;
  }
  status = lonejson__writer_emit_bytes_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
  }
  return status;
}

static lonejson_status
lonejson__writer_push_frame(lonejson_writer *writer,
                            lonejson__writer_frame_kind kind,
                            lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *next;
  size_t next_capacity;

  state = (lonejson__writer_state *)writer->state;
  if (state->frame_count == state->frame_capacity) {
    next_capacity =
        state->frame_capacity == 0u ? 8u : state->frame_capacity * 2u;
    next = (lonejson__writer_frame *)lonejson__buffer_realloc(
        &state->allocator, state->frames,
        state->frame_capacity * sizeof(*state->frames),
        next_capacity * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_ALLOCATION_FAILED,
                                        "failed to grow writer stack");
    }
    state->frames = next;
    state->frame_capacity = next_capacity;
  }
  memset(&state->frames[state->frame_count], 0,
         sizeof(state->frames[state->frame_count]));
  state->frames[state->frame_count].kind = kind;
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson__writer_frame *
lonejson__writer_top(lonejson__writer_state *state) {
  if (state->frame_count == 0u) {
    return NULL;
  }
  return &state->frames[state->frame_count - 1u];
}

static lonejson_status lonejson__writer_before_value(lonejson_writer *writer,
                                                     lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is still open");
  }
  if (state->finished) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is already finished");
  }
  if (lonejson__writer_output_blocked(writer)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL) {
    if (state->root_written) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer already has a root value");
    }
    state->root_written = 1;
    return LONEJSON_STATUS_OK;
  }
  if (frame->kind == LONEJSON__WRITER_FRAME_ARRAY) {
    if (frame->count != 0u) {
      status = lonejson__writer_emit_cstr(writer, ",", error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    frame->count++;
    return LONEJSON_STATUS_OK;
  }
  if (!frame->after_key) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_JSON,
        "object values must be preceded by a writer key");
  }
  frame->after_key = 0;
  frame->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_emit_value_bytes(lonejson_writer *writer, const void *data,
                                  size_t len, lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_emit(writer, data, len, error);
}

static lonejson_status lonejson__writer_init_sink_with_options(
    lonejson_writer *writer, lonejson_sink_fn sink, void *sink_user,
    const lonejson__write_options *options, const lonejson_runtime *runtime,
    lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_allocator allocator;

  if (writer == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "writer and sink are required");
  }
  memset(writer, 0, sizeof(*writer));
  lonejson__writer_assign_methods(writer);
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  allocator = lonejson__allocator_resolve(options ? options->allocator : NULL);
  state = (lonejson__writer_state *)lonejson__buffer_alloc(&allocator,
                                                           sizeof(*state));
  if (state == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate writer state");
  }
  memset(state, 0, sizeof(*state));
  state->mode = sink == lonejson__writer_generator_sink
                    ? LONEJSON__WRITER_MODE_GENERATOR
                    : LONEJSON__WRITER_MODE_SINK;
  state->options = options ? *options : lonejson__default_write_options();
  state->allocator = allocator;
  if (lonejson__runtime_snapshot_init(&state->runtime_storage, runtime, 1,
                                      error) != LONEJSON_STATUS_OK) {
    lonejson__buffer_free(&allocator, state, sizeof(*state));
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  state->runtime = runtime != NULL ? &state->runtime_storage
                                   : (const lonejson_runtime *)NULL;
  if (runtime != NULL &&
      state->options.allocator == runtime->config.allocator) {
    state->options.allocator = state->runtime->config.allocator;
  }
  state->sink = sink;
  state->sink_user = sink_user;
  state->external_error = error;
  state->max_number_bytes = state->runtime != NULL
                                ? state->runtime->value_limits.max_number_bytes
                                : 256u;
  writer->state = state;
  lonejson__clear_error(&writer->error);
  lonejson__writer_assign_methods(writer);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_init_sink(lonejson *runtime,
                                          lonejson_writer *writer,
                                          lonejson_sink_fn sink,
                                          void *sink_user,
                                          lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  const lonejson__write_options *options;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  options = &runtime_state->write_options;
  status = lonejson__writer_init_sink_with_options(
      writer, sink, sink_user, options, runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

void lonejson_writer_cleanup(lonejson_writer *writer) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return;
  }
  state = (lonejson__writer_state *)writer->state;
  lonejson__writer_clear_event(state);
  lonejson__byte_free(&state->number, &state->allocator);
  lonejson__buffer_free(&state->allocator, state->frames,
                        state->frame_capacity * sizeof(*state->frames));
  lonejson__runtime_free_owned_config(&state->runtime_storage);
  lonejson__buffer_free(&state->allocator, state, sizeof(*state));
  memset(writer, 0, sizeof(*writer));
  lonejson__writer_assign_methods(writer);
}

lonejson_status lonejson_writer_begin_object(lonejson_writer *writer,
                                             lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_emit_cstr(writer, "{", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_push_frame(writer, LONEJSON__WRITER_FRAME_OBJECT,
                                     error);
}

lonejson_status lonejson_writer_end_object(lonejson_writer *writer,
                                           lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is still open");
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is not inside an object");
  }
  if (frame->after_key) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "object key is missing a value");
  }
  status = lonejson__writer_emit_cstr(writer, "}", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->frame_count--;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_begin_array(lonejson_writer *writer,
                                            lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_emit_cstr(writer, "[", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_push_frame(writer, LONEJSON__WRITER_FRAME_ARRAY,
                                     error);
}

lonejson_status lonejson_writer_end_array(lonejson_writer *writer,
                                          lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is still open");
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_ARRAY) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is not inside an array");
  }
  status = lonejson__writer_emit_cstr(writer, "]", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->frame_count--;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_key(lonejson_writer *writer, const char *key,
                                    size_t key_len, lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL ||
      (key == NULL && key_len != 0u)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and key are required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  status = lonejson__writer_begin_event(writer, LONEJSON__WRITER_EVENT_KEY, key,
                                        key_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (state->event_phase == 0u) {
    if (state->string_open) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer string is still open");
    }
    if (state->number_open) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer number is still open");
    }
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    frame = lonejson__writer_top(state);
    if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer key requires an object");
    }
    if (frame->after_key) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_JSON,
          "previous object key is missing a value");
    }
    if (frame->count != 0u) {
      status = lonejson__writer_emit_cstr(writer, ",", error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    state->event_phase = 1u;
  }
  if (state->event_phase == 1u) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit_cstr(writer, "\"", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 2u;
  }
  if (state->event_phase == 2u) {
    status = lonejson__writer_emit_escaped_event(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 3u;
  }
  if (state->event_phase == 3u) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit_cstr(writer, "\":", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  frame = lonejson__writer_top(state);
  frame->after_key = 1;
  lonejson__writer_clear_event(state);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string(lonejson_writer *writer,
                                       const void *data, size_t len,
                                       lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (data == NULL && len != 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "string data is required");
  }
  status = lonejson__writer_begin_event(writer, LONEJSON__WRITER_EVENT_STRING,
                                        data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 1u;
  }
  if (state->event_phase == 1u) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit_cstr(writer, "\"", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 2u;
  }
  if (state->event_phase == 2u) {
    status = lonejson__writer_emit_escaped_event(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 3u;
  }
  if (lonejson__writer_output_blocked(writer)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  status = lonejson__writer_emit_cstr(writer, "\"", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__writer_clear_event(state);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string_begin(lonejson_writer *writer,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  status = lonejson__writer_emit_cstr(writer, "\"", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->string_open = 1;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string_chunk(lonejson_writer *writer,
                                             const void *data, size_t len,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL || (data == NULL && len != 0u)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and string chunk are required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (!state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is not open");
  }
  status = lonejson__writer_begin_event(
      writer, LONEJSON__WRITER_EVENT_STRING_CHUNK, data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_emit_escaped_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
  }
  return status;
}

lonejson_status lonejson_writer_string_end(lonejson_writer *writer,
                                           lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (!state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is not open");
  }
  status = lonejson__writer_emit_cstr(writer, "\"", error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_open = 0;
  }
  return status;
}

lonejson_status lonejson_writer_string_reader(lonejson_writer *writer,
                                              lonejson_reader_fn reader,
                                              void *reader_user,
                                              lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_read_result rr;
  lonejson_status status;

  if (reader == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "string reader is required");
  }
  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->string_reader_active) {
    if (state->string_reader != reader ||
        state->string_reader_user != reader_user) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
          "active string reader must be resumed with the same reader");
    }
  } else {
    status = lonejson_writer_string_begin(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->string_reader_active = 1;
    state->string_reader = reader;
    state->string_reader_user = reader_user;
    state->string_reader_buffer_len = 0u;
    state->string_reader_buffer_off = 0u;
    state->string_reader_eof = 0;
  }
  for (;;) {
    while (state->string_reader_buffer_off < state->string_reader_buffer_len) {
      if (lonejson__writer_output_blocked(writer)) {
        return LONEJSON_STATUS_TRUNCATED;
      }
      status = lonejson__writer_string_reader_emit_byte(
          writer, state->string_reader_buffer[state->string_reader_buffer_off],
          error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      state->string_reader_buffer_off++;
    }
    state->string_reader_buffer_len = 0u;
    state->string_reader_buffer_off = 0u;
    if (state->string_reader_eof) {
      break;
    }
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    rr = reader(reader_user, state->string_reader_buffer,
                sizeof(state->string_reader_buffer));
    if (rr.error_code != 0) {
      return lonejson__writer_set_error(writer, error, LONEJSON_STATUS_IO_ERROR,
                                        "failed to read JSON string source");
    }
    if (rr.would_block) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_CALLBACK_FAILED,
                                        "string reader would block");
    }
    state->string_reader_buffer_len = rr.bytes_read;
    state->string_reader_eof = rr.eof;
  }
  if (lonejson__writer_output_blocked(writer)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  status = lonejson_writer_string_end(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_reader_active = 0;
    state->string_reader = NULL;
    state->string_reader_user = NULL;
    state->string_reader_buffer_len = 0u;
    state->string_reader_buffer_off = 0u;
    state->string_reader_eof = 0;
  }
  return status;
}

static lonejson_status
lonejson__writer_child_init(lonejson_writer *writer,
                            const lonejson__generator_frame *root,
                            lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__generator_state *child;

  state = (lonejson__writer_state *)writer->state;
  child = (lonejson__generator_state *)lonejson__buffer_alloc(&state->allocator,
                                                              sizeof(*child));
  if (child == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_ALLOCATION_FAILED,
        "failed to allocate writer child generator");
  }
  memset(child, 0, sizeof(*child));
  child->magic = LONEJSON__GENERATOR_MAGIC;
  child->allocator = state->allocator;
  child->options = state->options;
  if (lonejson__generator_push_frame(child, root,
                                     error != NULL ? error : &writer->error) !=
      LONEJSON_STATUS_OK) {
    lonejson__buffer_free(&state->allocator, child, sizeof(*child));
    return (error != NULL ? error->code : writer->error.code);
  }
  memset(&state->event_child, 0, sizeof(state->event_child));
  state->event_child.state = child;
  state->event_child.eof = 0;
  lonejson__clear_error(&state->event_child.error);
  state->event_child_active = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_child_event(
    lonejson_writer *writer, lonejson__writer_event_kind kind, const void *arg0,
    const void *arg1, size_t arg_len, const lonejson__generator_frame *root,
    lonejson_error *error) {
  lonejson__writer_state *state;
  size_t output_available;
  size_t read_capacity;
  size_t out_len;
  int eof;
  lonejson_status status;

  status = lonejson__writer_begin_event(writer, kind, arg0, arg_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_data2 = arg1;
    status = lonejson__writer_child_init(writer, root, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson__writer_clear_event(state);
      return status;
    }
    state->event_phase = 1u;
  } else if (state->event_data2 != arg1) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "active writer event must be resumed with the same arguments");
  }
  for (;;) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    read_capacity = sizeof(state->event_child_buffer);
    if (state->mode == LONEJSON__WRITER_MODE_GENERATOR) {
      output_available =
          lonejson__writer_generator_output_available(state->sink_user);
      if (output_available == 0u) {
        return LONEJSON_STATUS_TRUNCATED;
      }
      if (read_capacity > output_available) {
        read_capacity = output_available;
      }
    }
    status =
        lonejson_generator_read(&state->event_child, state->event_child_buffer,
                                read_capacity, &out_len, &eof);
    if (status != LONEJSON_STATUS_OK) {
      if (error != NULL) {
        *error = state->event_child.error;
      } else {
        writer->error = state->event_child.error;
      }
      return status;
    }
    if (out_len != 0u) {
      status = lonejson__writer_emit(writer, state->event_child_buffer, out_len,
                                     error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      if (lonejson__writer_output_blocked(writer)) {
        return LONEJSON_STATUS_TRUNCATED;
      }
    }
    if (eof) {
      break;
    }
    if (out_len == 0u) {
      return LONEJSON_STATUS_TRUNCATED;
    }
  }
  lonejson__writer_clear_event(state);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string_spooled(lonejson_writer *writer,
                                               const lonejson_spooled *value,
                                               lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "spooled string value is required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SPOOLED_TEXT;
  root.u.spooled.cursor = *value;
  return lonejson__writer_child_event(writer, LONEJSON__WRITER_EVENT_CHILD,
                                      value, NULL, 1u, &root, error);
}

lonejson_status lonejson_writer_source_text(lonejson_writer *writer,
                                            const lonejson_source *value,
                                            lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "source text value is required");
  }
  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__writer_emit_value_bytes(writer, "null", 4u, error);
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SOURCE_TEXT;
  root.u.source.value = value;
  return lonejson__writer_child_event(writer, LONEJSON__WRITER_EVENT_CHILD,
                                      value, NULL, 2u, &root, error);
}

lonejson_status lonejson_writer_spooled_base64(lonejson_writer *writer,
                                               const lonejson_spooled *value,
                                               lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "spooled base64 value is required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SPOOLED_BASE64;
  root.u.spooled.cursor = *value;
  return lonejson__writer_child_event(writer, LONEJSON__WRITER_EVENT_CHILD,
                                      value, NULL, 3u, &root, error);
}

lonejson_status lonejson_writer_source_base64(lonejson_writer *writer,
                                              const lonejson_source *value,
                                              lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "source base64 value is required");
  }
  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__writer_emit_value_bytes(writer, "null", 4u, error);
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SOURCE_BASE64;
  root.u.source.value = value;
  return lonejson__writer_child_event(writer, LONEJSON__WRITER_EVENT_CHILD,
                                      value, NULL, 4u, &root, error);
}

lonejson_status lonejson_writer_number_text(lonejson_writer *writer,
                                            const char *data, size_t len,
                                            lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (data == NULL || len == 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "number text is required");
  }
  if (!lonejson__is_valid_json_number(data, len)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "number text must be a JSON number");
  }
  status = lonejson__writer_begin_event(writer, LONEJSON__WRITER_EVENT_BYTES,
                                        data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 1u;
  }
  status = lonejson__writer_emit_bytes_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
  }
  return status;
}

lonejson_status lonejson_writer_number_begin(lonejson_writer *writer,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  lonejson__byte_reset(&state->number);
  state->number_open = 1;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_number_chunk(lonejson_writer *writer,
                                             const char *data, size_t len,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL || (data == NULL && len != 0u)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and number chunk are required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (!state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is not open");
  }
  status = lonejson__byte_append(&state->number, data, len,
                                 state->max_number_bytes, &state->allocator,
                                 error != NULL ? error : &writer->error);
  if (status != LONEJSON_STATUS_OK) {
    state->failed = 1;
  }
  return status;
}

lonejson_status lonejson_writer_number_end(lonejson_writer *writer,
                                           lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (!state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is not open");
  }
  if (state->number.len == 0u) {
    return lonejson__writer_fail(writer, error, LONEJSON_STATUS_INVALID_JSON,
                                 "number text must be a JSON number");
  }
  if (!lonejson__is_valid_json_number(state->number.data, state->number.len)) {
    return lonejson__writer_fail(writer, error, LONEJSON_STATUS_INVALID_JSON,
                                 "number text must be a JSON number");
  }
  if (state->event_kind == LONEJSON__WRITER_EVENT_NONE) {
    state->event_kind = LONEJSON__WRITER_EVENT_BYTES;
    state->event_data = state->number.data;
    state->event_len = state->number.len;
    state->event_off = 0u;
    state->event_phase = 1u;
  } else if (state->event_kind != LONEJSON__WRITER_EVENT_BYTES ||
             state->event_data != state->number.data ||
             state->event_len != state->number.len) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "active writer event must be resumed with the same arguments");
  }
  status = lonejson__writer_emit_bytes_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
    lonejson__byte_reset(&state->number);
    state->number_open = 0;
  }
  return status;
}

lonejson_status lonejson_writer_i64(lonejson_writer *writer,
                                    lonejson_int64 value,
                                    lonejson_error *error) {
  char buffer[32];
  size_t len;

  len = lonejson__writer_format_i64(buffer, sizeof(buffer), value);
  return lonejson__writer_emit_scalar_bytes(writer, buffer, len, error);
}

lonejson_status lonejson_writer_u64(lonejson_writer *writer,
                                    lonejson_uint64 value,
                                    lonejson_error *error) {
  char buffer[32];
  size_t len;

  len = lonejson__writer_format_u64(buffer, sizeof(buffer), value);
  return lonejson__writer_emit_scalar_bytes(writer, buffer, len, error);
}

lonejson_status lonejson_writer_f64(lonejson_writer *writer, double value,
                                    lonejson_error *error) {
  char buffer[64];
  int len;

  if (!lonejson__is_finite_f64(value)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON numbers must be finite");
  }
  len = snprintf(buffer, sizeof(buffer), "%.17g", value);
  if (len < 0) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INTERNAL_ERROR,
                                      "failed to format number");
  }
  return lonejson__writer_emit_scalar_bytes(writer, buffer, (size_t)len, error);
}

lonejson_status lonejson_writer_bool(lonejson_writer *writer, int value,
                                     lonejson_error *error) {
  return lonejson__writer_emit_scalar_bytes(writer, value ? "true" : "false",
                                            value ? 4u : 5u, error);
}

lonejson_status lonejson_writer_null(lonejson_writer *writer,
                                     lonejson_error *error) {
  return lonejson__writer_emit_scalar_bytes(writer, "null", 4u, error);
}

lonejson_status lonejson_writer_json_value(lonejson_writer *writer,
                                           const lonejson_json_value *value,
                                           lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value is required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_JSON_VALUE;
  root.u.json_value.value = value;
  return lonejson__writer_child_event(writer, LONEJSON__WRITER_EVENT_CHILD,
                                      value, NULL, 5u, &root, error);
}

static lonejson_error *
lonejson__writer_value_error(lonejson_writer_value_stream *stream,
                             lonejson_error *error) {
  if (error != NULL) {
    return error;
  }
  return stream != NULL ? &stream->error : NULL;
}

static lonejson_status
lonejson__writer_value_set_error(lonejson_writer_value_stream *stream,
                                 lonejson_error *error, lonejson_status status,
                                 const char *message) {
  return lonejson__set_error(lonejson__writer_value_error(stream, error),
                             status, 0u, 0u, 0u, "%s", message);
}

static lonejson_status
lonejson__writer_value_fail(lonejson_writer_value_stream *stream,
                            lonejson__writer_value_stream_state *state,
                            lonejson_error *error, lonejson_status status,
                            const char *message) {
  if (state != NULL) {
    state->failed = 1;
    if (state->writer_state != NULL) {
      state->writer_state->failed = 1;
    }
  }
  return lonejson__writer_value_set_error(stream, error, status, message);
}

static lonejson__writer_value_frame *
lonejson__writer_value_top(lonejson__writer_value_stream_state *state) {
  return state->frame_count == 0u ? NULL
                                  : &state->frames[state->frame_count - 1u];
}

static lonejson_status
lonejson__writer_value_note_bytes(lonejson_writer_value_stream *stream,
                                  lonejson__writer_value_stream_state *state,
                                  lonejson_error *error, size_t len) {
  if (state->max_total_bytes != 0u &&
      (len > state->max_total_bytes ||
       state->total_bytes > state->max_total_bytes - len)) {
    return lonejson__writer_value_fail(
        stream, state, error, LONEJSON_STATUS_OVERFLOW,
        "writer value stream exceeds maximum total byte limit");
  }
  state->total_bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_value_emit_cstr(lonejson__writer_value_stream_state *state,
                                 const char *text, lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_emit_cstr(state->writer, text, error);
  if (status == LONEJSON_STATUS_TRUNCATED) {
    return lonejson__writer_value_fail(
        NULL, state, error, LONEJSON_STATUS_OVERFLOW,
        "writer value stream sink truncated output");
  }
  return status;
}

static lonejson_status
lonejson__writer_value_emit(lonejson__writer_value_stream_state *state,
                            const void *data, size_t len,
                            lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_emit(state->writer, data, len, error);
  if (status == LONEJSON_STATUS_TRUNCATED) {
    return lonejson__writer_value_fail(
        NULL, state, error, LONEJSON_STATUS_OVERFLOW,
        "writer value stream sink truncated output");
  }
  return status;
}

static lonejson_status
lonejson__writer_value_before(lonejson__writer_value_stream_state *state,
                              lonejson_error *error) {
  lonejson__writer_value_frame *frame;
  lonejson_status status;

  frame = lonejson__writer_value_top(state);
  if (frame == NULL) {
    if (state->root_written) {
      return lonejson__writer_fail(state->writer, error,
                                   LONEJSON_STATUS_INVALID_JSON,
                                   "writer value stream already has a root");
    }
    state->root_written = 1;
    return LONEJSON_STATUS_OK;
  }
  if (frame->kind == LONEJSON__WRITER_FRAME_ARRAY) {
    if (frame->count != 0u) {
      status = lonejson__writer_value_emit_cstr(state, ",", error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    frame->count++;
    return LONEJSON_STATUS_OK;
  }
  if (!frame->after_key) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "object value is missing a key");
  }
  frame->after_key = 0;
  frame->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_value_push_frame(lonejson__writer_value_stream_state *state,
                                  lonejson__writer_frame_kind kind,
                                  lonejson_error *error) {
  lonejson__writer_value_frame *next;
  size_t next_capacity;

  if (state->frame_count == state->frame_capacity) {
    next_capacity =
        state->frame_capacity == 0u ? 8u : state->frame_capacity * 2u;
    next = (lonejson__writer_value_frame *)lonejson__buffer_realloc(
        &state->allocator, state->frames,
        state->frame_capacity * sizeof(*state->frames),
        next_capacity * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__writer_fail(state->writer, error,
                                   LONEJSON_STATUS_ALLOCATION_FAILED,
                                   "failed to grow writer value stream stack");
    }
    state->frames = next;
    state->frame_capacity = next_capacity;
  }
  memset(&state->frames[state->frame_count], 0,
         sizeof(state->frames[state->frame_count]));
  state->frames[state->frame_count].kind = kind;
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_value_object_begin(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status = lonejson__writer_value_before(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_value_emit_cstr(state, "{", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_value_push_frame(state, LONEJSON__WRITER_FRAME_OBJECT,
                                           error);
}

static lonejson_status
lonejson__writer_value_object_end(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson__writer_value_frame *frame = lonejson__writer_value_top(state);

  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT ||
      frame->after_key) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "invalid writer value object end");
  }
  state->frame_count--;
  return lonejson__writer_value_emit_cstr(state, "}", error);
}

static lonejson_status
lonejson__writer_value_array_begin(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status = lonejson__writer_value_before(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_value_emit_cstr(state, "[", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_value_push_frame(state, LONEJSON__WRITER_FRAME_ARRAY,
                                           error);
}

static lonejson_status lonejson__writer_value_array_end(void *user,
                                                        lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson__writer_value_frame *frame = lonejson__writer_value_top(state);

  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_ARRAY) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "invalid writer value array end");
  }
  state->frame_count--;
  return lonejson__writer_value_emit_cstr(state, "]", error);
}

static lonejson_status lonejson__writer_value_key_begin(void *user,
                                                        lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson__writer_value_frame *frame = lonejson__writer_value_top(state);
  lonejson_status status;

  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT ||
      frame->after_key) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "invalid writer value object key");
  }
  if (frame->count != 0u) {
    status = lonejson__writer_value_emit_cstr(state, ",", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__writer_value_emit_cstr(state, "\"", error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_open = 1;
  }
  return status;
}

static lonejson_status
lonejson__writer_value_string_chunk(void *user, const char *data, size_t len,
                                    lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  size_t i;
  lonejson_status status;

  i = 0u;
  while (i < len) {
    size_t plain = lonejson__plain_string_span_bulk(
        (const unsigned char *)data + i, len - i, 0);
    if (plain != 0u) {
      status = lonejson__writer_emit(state->writer, data + i, plain, error);
      i += plain;
    } else {
      status = lonejson__writer_string_reader_emit_byte(
          state->writer, (unsigned char)data[i], error);
      ++i;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return lonejson__writer_value_fail(
          NULL, state, error, LONEJSON_STATUS_OVERFLOW,
          "writer value stream sink truncated output");
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_value_return_error(
    lonejson_writer *writer, lonejson_writer_value_stream *stream,
    lonejson_error *error, lonejson_status status) {
  if (status != LONEJSON_STATUS_OK && error == NULL && writer != NULL &&
      stream != NULL && stream->error.code != LONEJSON_STATUS_OK) {
    writer->error = stream->error;
  }
  return status;
}

static lonejson_status
lonejson__writer_json_value_preflight(lonejson_writer *writer,
                                      lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->mode != LONEJSON__WRITER_MODE_SINK) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "writer value streams require a sink-mode writer");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is already open");
  }
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->event_kind != LONEJSON__WRITER_EVENT_NONE) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer event is still active");
  }
  if (state->finished) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is already finished");
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL) {
    if (state->root_written) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer already has a root value");
    }
    return LONEJSON_STATUS_OK;
  }
  if (frame->kind == LONEJSON__WRITER_FRAME_OBJECT && !frame->after_key) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_JSON,
        "object values must be preceded by a writer key");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_value_key_end(void *user,
                                                      lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson__writer_value_frame *frame = lonejson__writer_value_top(state);
  lonejson_status status;

  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT ||
      !state->string_open) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "invalid writer value object key end");
  }
  status = lonejson__writer_value_emit_cstr(state, "\":", error);
  if (status == LONEJSON_STATUS_OK) {
    frame->after_key = 1;
    state->string_open = 0;
  }
  return status;
}

static lonejson_status
lonejson__writer_value_string_begin(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status = lonejson__writer_value_before(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_value_emit_cstr(state, "\"", error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_open = 1;
  }
  return status;
}

static lonejson_status
lonejson__writer_value_string_end(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status;

  if (!state->string_open) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "invalid writer value string end");
  }
  status = lonejson__writer_value_emit_cstr(state, "\"", error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_open = 0;
  }
  return status;
}

static lonejson_status
lonejson__writer_value_number_begin(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status = lonejson__writer_value_before(state, error);
  if (status == LONEJSON_STATUS_OK) {
    state->number_open = 1;
  }
  return status;
}

static lonejson_status
lonejson__writer_value_number_chunk(void *user, const char *data, size_t len,
                                    lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  return lonejson__writer_value_emit(state, data, len, error);
}

static lonejson_status
lonejson__writer_value_number_end(void *user, lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  if (!state->number_open) {
    return lonejson__writer_fail(state->writer, error,
                                 LONEJSON_STATUS_INVALID_JSON,
                                 "invalid writer value number end");
  }
  state->number_open = 0;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_value_bool(void *user, int value,
                                                   lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status = lonejson__writer_value_before(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_value_emit_cstr(state, value ? "true" : "false",
                                          error);
}

static lonejson_status lonejson__writer_value_null(void *user,
                                                   lonejson_error *error) {
  lonejson__writer_value_stream_state *state =
      (lonejson__writer_value_stream_state *)user;
  lonejson_status status = lonejson__writer_value_before(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_value_emit_cstr(state, "null", error);
}

static void
lonejson__writer_value_destroy(lonejson_writer_value_stream *stream,
                               lonejson__writer_value_stream_state *state,
                               int poison) {
  if (state == NULL) {
    return;
  }
  if (poison && state->writer_state != NULL && !state->closed) {
    state->writer_state->failed = 1;
    state->writer_state->value_stream_active = 0;
  }
  if (state->parser != NULL) {
    lonejson_parser_destroy(state->parser);
  }
  lonejson_json_value_cleanup(&state->json_value);
  lonejson__buffer_free(&state->allocator, state->frames,
                        state->frame_capacity * sizeof(*state->frames));
  lonejson__buffer_free(&state->allocator, state, sizeof(*state));
  if (stream != NULL) {
    stream->state = NULL;
    lonejson__writer_value_stream_assign_methods(stream);
  }
}

static lonejson_status lonejson__writer_value_stream_open_with_limits(
    lonejson_writer_value_stream *stream, lonejson_writer *writer,
    const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson__writer_value_stream_state *state;
  lonejson__writer_state *writer_state;
  const lonejson_runtime *runtime;
  lonejson__parse_options parse_options;
  lonejson_allocator allocator;
  lonejson_error local_error;
  lonejson_error *open_error;
  size_t parser_bytes;
  unsigned char *workspace;
  lonejson_status status;

  if (stream == NULL || writer == NULL || writer->state == NULL) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "writer value stream and writer are required");
  }
  if (stream->state != NULL) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_JSON,
        "writer value stream is already open");
  }
  writer_state = (lonejson__writer_state *)writer->state;
  runtime = lonejson__writer_runtime(writer_state);
  if (writer_state->mode != LONEJSON__WRITER_MODE_SINK) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "writer value streams require sink-mode writers");
  }
  if (writer_state->value_stream_active) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_JSON,
        "writer value stream is already open");
  }
  if (writer_state->event_kind != LONEJSON__WRITER_EVENT_NONE) {
    return lonejson__writer_value_set_error(stream, error,
                                            LONEJSON_STATUS_INVALID_JSON,
                                            "writer event is still active");
  }
  if (writer_state->failed) {
    return lonejson__writer_value_set_error(stream, error,
                                            LONEJSON_STATUS_INVALID_JSON,
                                            "writer is in a failed state");
  }
  allocator = writer_state->allocator;
  memset(stream, 0, sizeof(*stream));
  lonejson__writer_value_stream_assign_methods(stream);
  state = (lonejson__writer_value_stream_state *)lonejson__buffer_alloc(
      &allocator, sizeof(*state));
  if (state == NULL) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_ALLOCATION_FAILED,
        "failed to allocate writer value stream");
  }
  memset(state, 0, sizeof(*state));
  state->writer = writer;
  state->writer_state = writer_state;
  state->allocator = allocator;
  state->visitor.object_begin = lonejson__writer_value_object_begin;
  state->visitor.object_end = lonejson__writer_value_object_end;
  state->visitor.object_key_begin = lonejson__writer_value_key_begin;
  state->visitor.object_key_chunk = lonejson__writer_value_string_chunk;
  state->visitor.object_key_end = lonejson__writer_value_key_end;
  state->visitor.array_begin = lonejson__writer_value_array_begin;
  state->visitor.array_end = lonejson__writer_value_array_end;
  state->visitor.string_begin = lonejson__writer_value_string_begin;
  state->visitor.string_chunk = lonejson__writer_value_string_chunk;
  state->visitor.string_end = lonejson__writer_value_string_end;
  state->visitor.number_begin = lonejson__writer_value_number_begin;
  state->visitor.number_chunk = lonejson__writer_value_number_chunk;
  state->visitor.number_end = lonejson__writer_value_number_end;
  state->visitor.boolean_value = lonejson__writer_value_bool;
  state->visitor.null_value = lonejson__writer_value_null;
  lonejson_json_value_init_with_allocator(&state->json_value,
                                          &writer_state->allocator);
  if (limits != NULL) {
    state->json_value.parse_visitor_limits = *limits;
  } else if (runtime != NULL) {
    state->json_value.parse_visitor_limits = runtime->value_limits;
  }
  status = lonejson_json_value_set_parse_visitor(&state->json_value,
                                                 &state->visitor, state, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__writer_value_destroy(stream, state, 0);
    return status;
  }
  state->max_total_bytes =
      state->json_value.parse_visitor_limits.max_total_bytes;
  parse_options = lonejson__default_parse_options();
  parse_options.max_depth = state->json_value.parse_visitor_limits.max_depth;
  parse_options.allocator = &writer_state->allocator;
  parser_bytes = sizeof(*state->parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                 LONEJSON__PARSER_WORKSPACE_SLACK;
  state->parser =
      (lonejson_parser *)lonejson__buffer_alloc(&allocator, parser_bytes);
  if (state->parser == NULL) {
    (void)lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_ALLOCATION_FAILED,
        "failed to allocate writer value stream parser");
    lonejson__writer_value_destroy(stream, state, 0);
    return LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  workspace = ((unsigned char *)state->parser) + sizeof(*state->parser);
  lonejson__parser_init_state(state->parser, NULL, NULL, &parse_options,
                              runtime, 1, 0, 0, 0u, workspace,
                              LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                                  LONEJSON__PARSER_WORKSPACE_SLACK);
  state->parser->self_alloc_size = parser_bytes;
  state->parser->owns_self = 1;
  lonejson__parser_set_json_stream_value(state->parser, &state->json_value);
  lonejson__clear_error(&local_error);
  open_error = error != NULL ? error : &local_error;
  status = lonejson__writer_before_value(writer, open_error);
  if (status != LONEJSON_STATUS_OK) {
    if (error == NULL) {
      stream->error = local_error;
      writer->error = local_error;
    }
    lonejson__writer_value_destroy(stream, state, 0);
    return status;
  }
  stream->state = state;
  writer_state->value_stream_active = 1;
  lonejson__clear_error(&stream->error);
  return LONEJSON_STATUS_OK;
}

lonejson_status
lonejson_writer_value_stream_open(lonejson_writer_value_stream *stream,
                                  lonejson_writer *writer,
                                  lonejson_error *error) {
  return lonejson__writer_value_stream_open_with_limits(stream, writer, NULL,
                                                        error);
}

void lonejson_writer_value_stream_init(lonejson_writer_value_stream *stream) {
  if (stream == NULL) {
    return;
  }
  memset(stream, 0, sizeof(*stream));
  lonejson__writer_value_stream_assign_methods(stream);
  lonejson__clear_error(&stream->error);
}

lonejson_status
lonejson_writer_value_stream_push(lonejson_writer_value_stream *stream,
                                  const void *data, size_t len,
                                  lonejson_error *error) {
  lonejson__writer_value_stream_state *state;
  const unsigned char *bytes;
  size_t consumed;
  size_t i;
  lonejson_status status;

  if (stream == NULL || stream->state == NULL || (data == NULL && len != 0u)) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "open writer value stream and data are required");
  }
  state = (lonejson__writer_value_stream_state *)stream->state;
  if (state->failed || state->closed) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_JSON,
        "writer value stream is not writable");
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  bytes = (const unsigned char *)data;
  status = lonejson__writer_value_note_bytes(stream, state, error, len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (lonejson__parser_root_complete(state->parser)) {
    for (i = 0u; i < len; ++i) {
      if (!lonejson__is_json_space(bytes[i])) {
        return lonejson__writer_value_fail(
            stream, state, error, LONEJSON_STATUS_INVALID_JSON,
            "writer value stream contains trailing non-whitespace");
      }
    }
    return LONEJSON_STATUS_OK;
  }
  consumed = 0u;
  status = lonejson__parser_feed_bytes(state->parser, bytes, len, &consumed, 1);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (error != NULL) {
      *error = state->parser->error;
    } else {
      stream->error = state->parser->error;
    }
    state->failed = 1;
    state->writer_state->failed = 1;
    return status;
  }
  if (status == LONEJSON_STATUS_TRUNCATED) {
    return lonejson__writer_value_fail(
        stream, state, error, LONEJSON_STATUS_OVERFLOW,
        "writer value stream sink truncated output");
  }
  if (lonejson__parser_root_complete(state->parser)) {
    for (i = consumed; i < len; ++i) {
      if (!lonejson__is_json_space(bytes[i])) {
        return lonejson__writer_value_fail(
            stream, state, error, LONEJSON_STATUS_INVALID_JSON,
            "writer value stream contains trailing non-whitespace");
      }
    }
  }
  return status;
}

lonejson_status
lonejson_writer_value_stream_close(lonejson_writer_value_stream *stream,
                                   lonejson_error *error) {
  lonejson__writer_value_stream_state *state;
  lonejson_status status;

  if (stream == NULL || stream->state == NULL) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "open writer value stream is required");
  }
  state = (lonejson__writer_value_stream_state *)stream->state;
  if (state->failed || state->closed) {
    return lonejson__writer_value_set_error(
        stream, error, LONEJSON_STATUS_INVALID_JSON,
        "writer value stream is not closable");
  }
  status = lonejson_parser_finish(state->parser);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      *error = state->parser->error;
    } else {
      stream->error = state->parser->error;
    }
    state->failed = 1;
    state->writer_state->failed = 1;
    return status;
  }
  if (state->frame_count != 0u || state->string_open || state->number_open ||
      !state->root_written) {
    return lonejson__writer_value_fail(
        stream, state, error, LONEJSON_STATUS_INVALID_JSON,
        "writer value stream did not finish one complete value");
  }
  state->closed = 1;
  state->writer_state->value_stream_active = 0;
  lonejson__writer_value_destroy(stream, state, 0);
  return LONEJSON_STATUS_OK;
}

void lonejson_writer_value_stream_cleanup(
    lonejson_writer_value_stream *stream) {
  lonejson__writer_value_stream_state *state;

  if (stream == NULL || stream->state == NULL) {
    return;
  }
  state = (lonejson__writer_value_stream_state *)stream->state;
  lonejson__writer_value_destroy(stream, state, 1);
  lonejson__clear_error(&stream->error);
  lonejson__writer_value_stream_assign_methods(stream);
}

static lonejson_status lonejson__writer_json_value_stream_reader_with_limits(
    lonejson_writer *writer, lonejson_reader_fn reader, void *reader_user,
    const lonejson__value_limits *limits, lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson_writer_value_stream stream;
  lonejson_status status;

  if (reader == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value reader is required");
  }
  memset(&stream, 0, sizeof(stream));
  status = lonejson__writer_value_stream_open_with_limits(&stream, writer,
                                                          limits, error);
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__writer_value_return_error(writer, &stream, error, status);
    return status;
  }
  for (;;) {
    lonejson_read_result chunk = reader(reader_user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      (void)lonejson__writer_fail(writer, error, LONEJSON_STATUS_IO_ERROR,
                                  "failed to read JSON value source");
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      } else {
        writer->error.system_errno = chunk.error_code;
      }
      lonejson_writer_value_stream_cleanup(&stream);
      return LONEJSON_STATUS_IO_ERROR;
    }
    if (chunk.would_block) {
      (void)lonejson__writer_fail(writer, error,
                                  LONEJSON_STATUS_CALLBACK_FAILED,
                                  "JSON value reader would block");
      lonejson_writer_value_stream_cleanup(&stream);
      return LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_writer_value_stream_push(&stream, buffer,
                                                 chunk.bytes_read, error);
      if (status != LONEJSON_STATUS_OK) {
        (void)lonejson__writer_value_return_error(writer, &stream, error,
                                                  status);
        lonejson_writer_value_stream_cleanup(&stream);
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
    if (chunk.bytes_read == 0u) {
      (void)lonejson__writer_fail(writer, error,
                                  LONEJSON_STATUS_CALLBACK_FAILED,
                                  "JSON value reader made no progress");
      lonejson_writer_value_stream_cleanup(&stream);
      return LONEJSON_STATUS_CALLBACK_FAILED;
    }
  }
  status = lonejson_writer_value_stream_close(&stream, error);
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__writer_value_return_error(writer, &stream, error, status);
    lonejson_writer_value_stream_cleanup(&stream);
  }
  return status;
}

static lonejson_status lonejson__writer_json_value_reader_with_limits(
    lonejson_writer *writer, lonejson_reader_fn reader, void *reader_user,
    const lonejson__value_limits *limits, lonejson_error *error) {
  return lonejson__writer_json_value_stream_reader_with_limits(
      writer, reader, reader_user, limits, error);
}

lonejson_status lonejson_writer_json_value_reader(lonejson_writer *writer,
                                                  lonejson_reader_fn reader,
                                                  void *reader_user,
                                                  lonejson_error *error) {
  return lonejson__writer_json_value_reader_with_limits(
      writer, reader, reader_user, NULL, error);
}

static lonejson_status lonejson__writer_json_value_buffer_with_limits(
    lonejson_writer *writer, const void *data, size_t len,
    const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson_writer_value_stream stream;
  lonejson_status status;

  if (data == NULL && len != 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value buffer is required");
  }
  memset(&stream, 0, sizeof(stream));
  status = lonejson__writer_value_stream_open_with_limits(&stream, writer,
                                                          limits, error);
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__writer_value_return_error(writer, &stream, error, status);
    return status;
  }
  status = lonejson_writer_value_stream_push(&stream, data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__writer_value_return_error(writer, &stream, error, status);
    lonejson_writer_value_stream_cleanup(&stream);
    return status;
  }
  status = lonejson_writer_value_stream_close(&stream, error);
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__writer_value_return_error(writer, &stream, error, status);
    lonejson_writer_value_stream_cleanup(&stream);
  }
  return status;
}

lonejson_status lonejson_writer_json_value_buffer(lonejson_writer *writer,
                                                  const void *data, size_t len,
                                                  lonejson_error *error) {
  return lonejson__writer_json_value_buffer_with_limits(writer, data, len, NULL,
                                                        error);
}

static lonejson_read_result
lonejson__writer_json_value_file_reader(void *user, unsigned char *buffer,
                                        size_t capacity) {
  FILE *fp = (FILE *)user;
  lonejson_read_result result;
  size_t got;

  memset(&result, 0, sizeof(result));
  got = fread(buffer, 1u, capacity, fp);
  result.bytes_read = got;
  result.eof = feof(fp) ? 1 : 0;
  result.error_code = ferror(fp) ? (errno != 0 ? errno : EIO) : 0;
  return result;
}

static lonejson_status lonejson__writer_json_value_file_with_limits(
    lonejson_writer *writer, FILE *fp, const lonejson__value_limits *limits,
    lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value FILE* is required");
  }
  clearerr(fp);
  return lonejson__writer_json_value_stream_reader_with_limits(
      writer, lonejson__writer_json_value_file_reader, fp, limits, error);
}

lonejson_status lonejson_writer_json_value_file(lonejson_writer *writer,
                                                FILE *fp,
                                                lonejson_error *error) {
  return lonejson__writer_json_value_file_with_limits(writer, fp, NULL, error);
}

typedef struct lonejson__writer_json_value_fd_reader_state {
  int fd;
} lonejson__writer_json_value_fd_reader_state;

static lonejson_read_result
lonejson__writer_json_value_fd_reader(void *user, unsigned char *buffer,
                                      size_t capacity) {
  lonejson__writer_json_value_fd_reader_state *state;
  lonejson_read_result result;
  ssize_t got;

  memset(&result, 0, sizeof(result));
  state = (lonejson__writer_json_value_fd_reader_state *)user;
  got = read(state->fd, buffer, capacity);
  if (got < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      result.would_block = 1;
    } else {
      result.error_code = errno != 0 ? errno : EIO;
    }
    return result;
  }
  result.bytes_read = (size_t)got;
  result.eof = got == 0 ? 1 : 0;
  return result;
}

static lonejson_read_result
lonejson__writer_spooled_reader(void *user, unsigned char *buffer,
                                size_t capacity) {
  return lonejson_spooled_read((lonejson_spooled *)user, buffer, capacity);
}

static lonejson_status
lonejson__writer_json_value_fd_with_limits(lonejson_writer *writer, int fd,
                                           const lonejson__value_limits *limits,
                                           lonejson_error *error) {
  lonejson__writer_json_value_fd_reader_state state;

  if (fd < 0) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value file descriptor is required");
  }
  state.fd = fd;
  return lonejson__writer_json_value_stream_reader_with_limits(
      writer, lonejson__writer_json_value_fd_reader, &state, limits, error);
}

lonejson_status lonejson_writer_json_value_fd(lonejson_writer *writer, int fd,
                                              lonejson_error *error) {
  return lonejson__writer_json_value_fd_with_limits(writer, fd, NULL, error);
}

static lonejson_status lonejson__writer_json_value_path_close_failed(
    lonejson_writer *writer, lonejson_error *error, int system_errno) {
  if (error != NULL) {
    error->system_errno = system_errno;
  }
  (void)lonejson__writer_fail(writer, error, LONEJSON_STATUS_IO_ERROR,
                              "failed to close JSON value path");
  if (error == NULL && writer != NULL) {
    writer->error.system_errno = system_errno;
  }
  return LONEJSON_STATUS_IO_ERROR;
}

static lonejson_status lonejson__writer_json_value_path_with_limits(
    lonejson_writer *writer, const char *path,
    const lonejson__value_limits *limits, lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL || path[0] == '\0') {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value path is required");
  }
  status = lonejson__writer_json_value_preflight(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    int open_errno = errno;
    if (error != NULL) {
      error->system_errno = open_errno;
    }
    (void)lonejson__writer_set_error(writer, error, LONEJSON_STATUS_IO_ERROR,
                                     "failed to open JSON value path");
    if (error == NULL && writer != NULL) {
      writer->error.system_errno = open_errno;
    }
    return LONEJSON_STATUS_IO_ERROR;
  }
  status =
      lonejson__writer_json_value_file_with_limits(writer, fp, limits, error);
  if (fclose(fp) != 0 && status == LONEJSON_STATUS_OK) {
    return lonejson__writer_json_value_path_close_failed(writer, error, errno);
  }
  return status;
}

lonejson_status lonejson_writer_json_value_path(lonejson_writer *writer,
                                                const char *path,
                                                lonejson_error *error) {
  return lonejson__writer_json_value_path_with_limits(writer, path, NULL,
                                                      error);
}

static lonejson_status lonejson__writer_json_value_spooled_with_limits(
    lonejson_writer *writer, const lonejson_spooled *value,
    const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson_spooled cursor;
  lonejson_status status;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "spooled JSON value is required");
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_json_value_stream_reader_with_limits(
      writer, lonejson__writer_spooled_reader, &cursor, limits, error);
}

lonejson_status
lonejson_writer_json_value_spooled(lonejson_writer *writer,
                                   const lonejson_spooled *value,
                                   lonejson_error *error) {
  return lonejson__writer_json_value_spooled_with_limits(writer, value, NULL,
                                                         error);
}

typedef struct lonejson__writer_array_items_sink_state {
  lonejson_writer *writer;
  int started;
  int emitted_any;
} lonejson__writer_array_items_sink_state;

static void lonejson__writer_poison(lonejson_writer *writer) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return;
  }
  state = (lonejson__writer_state *)writer->state;
  state->failed = 1;
}

static lonejson_status
lonejson__writer_array_items_require(lonejson_writer *writer,
                                     lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->mode != LONEJSON__WRITER_MODE_SINK) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "writer array item splicing requires sink mode");
  }
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->event_kind != LONEJSON__WRITER_EVENT_NONE) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer event is still active");
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_ARRAY) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is not inside an array");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_array_items_open_failed(lonejson_writer *writer,
                                         lonejson_error *error,
                                         const lonejson_error *source_error) {
  if (error != NULL && error->code != LONEJSON_STATUS_OK) {
    return error->code;
  }
  if (error == NULL && source_error != NULL &&
      source_error->code != LONEJSON_STATUS_OK) {
    if (writer != NULL) {
      writer->error = *source_error;
    }
    return source_error->code;
  }
  return lonejson__writer_set_error(writer, error,
                                    LONEJSON_STATUS_INVALID_ARGUMENT,
                                    "failed to open array item source");
}

static lonejson_status
lonejson__writer_array_items_sink(void *user, const void *data, size_t len,
                                  lonejson_error *error) {
  lonejson__writer_array_items_sink_state *state =
      (lonejson__writer_array_items_sink_state *)user;
  lonejson_status status;

  if (!state->started) {
    status = lonejson__writer_before_value(state->writer, error);
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return lonejson__writer_fail(state->writer, error,
                                   LONEJSON_STATUS_OVERFLOW,
                                   "writer array item sink truncated output");
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->started = 1;
  }
  status = lonejson__writer_emit(state->writer, data, len, error);
  if (status == LONEJSON_STATUS_TRUNCATED) {
    return lonejson__writer_fail(state->writer, error, LONEJSON_STATUS_OVERFLOW,
                                 "writer array item sink truncated output");
  }
  return status;
}

static lonejson_status
lonejson__writer_array_items_from_stream(lonejson_writer *writer,
                                         lonejson_array_stream *stream,
                                         lonejson_error *error) {
  lonejson__writer_array_items_sink_state sink_state;
  lonejson_array_stream_result result;
  lonejson_status status;

  status = lonejson__writer_array_items_require(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (stream == NULL) {
    lonejson__writer_poison(writer);
    return error != NULL && error->code != LONEJSON_STATUS_OK
               ? error->code
               : LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(&sink_state, 0, sizeof(sink_state));
  sink_state.writer = writer;
  for (;;) {
    result = lonejson__array_stream_next_sink(
        lonejson__array_stream_state_mut(stream),
        lonejson__writer_array_items_sink, &sink_state, error);
    if (result == LONEJSON_ARRAY_STREAM_ITEM) {
      sink_state.emitted_any = 1;
      sink_state.started = 0;
      continue;
    }
    if (result == LONEJSON_ARRAY_STREAM_EOF) {
      return LONEJSON_STATUS_OK;
    }
    if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
      if (sink_state.started || sink_state.emitted_any) {
        return lonejson__writer_fail(writer, error,
                                     LONEJSON_STATUS_CALLBACK_FAILED,
                                     "array item reader would block");
      }
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_CALLBACK_FAILED,
                                        "array item reader would block");
    }
    if (sink_state.started || sink_state.emitted_any) {
      lonejson__writer_poison(writer);
    }
    if (error != NULL && error->code != LONEJSON_STATUS_OK) {
      return error->code;
    }
    if (error == NULL && writer != NULL) {
      const lonejson_error *stream_error = lonejson_array_stream_error(stream);
      if (stream_error != NULL && stream_error->code != LONEJSON_STATUS_OK) {
        writer->error = *stream_error;
        return writer->error.code;
      }
    }
    return LONEJSON_STATUS_INVALID_JSON;
  }
}

static lonejson_status lonejson__writer_array_items_reader_with_options(
    lonejson_writer *writer, const char *selector, lonejson_reader_fn reader,
    void *reader_user, const lonejson__parse_options *options,
    lonejson_error *error) {
  lonejson_error local_error;
  lonejson_error *open_error;
  lonejson_array_stream *stream;
  lonejson_status status;

  lonejson__clear_error(&local_error);
  open_error = error != NULL ? error : &local_error;
  stream = lonejson__array_stream_open_reader_with_options(
      selector, reader, reader_user, options,
      lonejson__writer_runtime((const lonejson__writer_state *)writer->state),
      open_error);
  if (stream == NULL) {
    return lonejson__writer_array_items_open_failed(writer, error,
                                                    &local_error);
  }
  status = lonejson__writer_array_items_from_stream(writer, stream, error);
  lonejson_array_stream_close(stream);
  return status;
}

lonejson_status lonejson_writer_array_items_reader(lonejson_writer *writer,
                                                   const char *selector,
                                                   lonejson_reader_fn reader,
                                                   void *reader_user,
                                                   lonejson_error *error) {
  return lonejson__writer_array_items_reader_with_options(
      writer, selector, reader, reader_user,
      lonejson__writer_runtime_parse_options(writer), error);
}

static lonejson_status lonejson__writer_array_items_buffer_with_options(
    lonejson_writer *writer, const char *selector, const void *data, size_t len,
    const lonejson__parse_options *options, lonejson_error *error) {
  lonejson_buffer_reader buffer_reader;

  if (data == NULL && len != 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "array item buffer is required");
  }
  lonejson_buffer_reader_init(&buffer_reader, data, len);
  return lonejson__writer_array_items_reader_with_options(
      writer, selector, lonejson_buffer_reader_read, &buffer_reader, options,
      error);
}

lonejson_status lonejson_writer_array_items_buffer(lonejson_writer *writer,
                                                   const char *selector,
                                                   const void *data, size_t len,
                                                   lonejson_error *error) {
  return lonejson__writer_array_items_buffer_with_options(
      writer, selector, data, len,
      lonejson__writer_runtime_parse_options(writer), error);
}

static lonejson_status lonejson__writer_array_items_filep_with_options(
    lonejson_writer *writer, const char *selector, FILE *fp,
    const lonejson__parse_options *options, lonejson_error *error) {
  lonejson_error local_error;
  lonejson_error *open_error;
  lonejson_array_stream *stream;
  lonejson_status status;

  lonejson__clear_error(&local_error);
  open_error = error != NULL ? error : &local_error;
  stream = lonejson__array_stream_open_filep_with_options(
      selector, fp, options,
      lonejson__writer_runtime((const lonejson__writer_state *)writer->state),
      open_error);
  if (stream == NULL) {
    return lonejson__writer_array_items_open_failed(writer, error,
                                                    &local_error);
  }
  status = lonejson__writer_array_items_from_stream(writer, stream, error);
  lonejson_array_stream_close(stream);
  return status;
}

lonejson_status lonejson_writer_array_items_filep(lonejson_writer *writer,
                                                  const char *selector,
                                                  FILE *fp,
                                                  lonejson_error *error) {
  return lonejson__writer_array_items_filep_with_options(
      writer, selector, fp, lonejson__writer_runtime_parse_options(writer),
      error);
}

static lonejson_status lonejson__writer_array_items_fd_with_options(
    lonejson_writer *writer, const char *selector, int fd,
    const lonejson__parse_options *options, lonejson_error *error) {
  lonejson_error local_error;
  lonejson_error *open_error;
  lonejson_array_stream *stream;
  lonejson_status status;

  lonejson__clear_error(&local_error);
  open_error = error != NULL ? error : &local_error;
  stream = lonejson__array_stream_open_fd_with_options(
      selector, fd, options,
      lonejson__writer_runtime((const lonejson__writer_state *)writer->state),
      open_error);
  if (stream == NULL) {
    return lonejson__writer_array_items_open_failed(writer, error,
                                                    &local_error);
  }
  status = lonejson__writer_array_items_from_stream(writer, stream, error);
  lonejson_array_stream_close(stream);
  return status;
}

lonejson_status lonejson_writer_array_items_fd(lonejson_writer *writer,
                                               const char *selector, int fd,
                                               lonejson_error *error) {
  return lonejson__writer_array_items_fd_with_options(
      writer, selector, fd, lonejson__writer_runtime_parse_options(writer),
      error);
}

static lonejson_status lonejson__writer_array_items_path_with_options(
    lonejson_writer *writer, const char *selector, const char *path,
    const lonejson__parse_options *options, lonejson_error *error) {
  lonejson_error local_error;
  lonejson_error *open_error;
  lonejson_array_stream *stream;
  lonejson_status status;

  status = lonejson__writer_array_items_require(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__clear_error(&local_error);
  open_error = error != NULL ? error : &local_error;
  stream = lonejson__array_stream_open_path_with_options(
      selector, path, options,
      lonejson__writer_runtime((const lonejson__writer_state *)writer->state),
      open_error);
  if (stream == NULL) {
    return lonejson__writer_array_items_open_failed(writer, error,
                                                    &local_error);
  }
  status = lonejson__writer_array_items_from_stream(writer, stream, error);
  lonejson_array_stream_close(stream);
  return status;
}

lonejson_status lonejson_writer_array_items_path(lonejson_writer *writer,
                                                 const char *selector,
                                                 const char *path,
                                                 lonejson_error *error) {
  return lonejson__writer_array_items_path_with_options(
      writer, selector, path, lonejson__writer_runtime_parse_options(writer),
      error);
}

static lonejson_status lonejson__writer_array_items_spooled_with_options(
    lonejson_writer *writer, const char *selector,
    const lonejson_spooled *value, const lonejson__parse_options *options,
    lonejson_error *error) {
  lonejson_spooled cursor;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "spooled array item source is required");
  }
  cursor = *value;
  if (lonejson_spooled_rewind(&cursor, error) != LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_IO_ERROR;
  }
  return lonejson__writer_array_items_reader_with_options(
      writer, selector, lonejson__writer_spooled_reader, &cursor, options,
      error);
}

lonejson_status lonejson_writer_array_items_spooled(
    lonejson_writer *writer, const char *selector,
    const lonejson_spooled *value, lonejson_error *error) {
  return lonejson__writer_array_items_spooled_with_options(
      writer, selector, value, lonejson__writer_runtime_parse_options(writer),
      error);
}

lonejson_status lonejson_writer_mapped(lonejson_writer *writer,
                                       const lonejson_map *map, const void *src,
                                       lonejson_error *error) {
  lonejson__generator_frame root;

  if (map == NULL || src == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "map and source are required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_MAP;
  root.u.map.map = map;
  root.u.map.src = src;
  return lonejson__writer_child_event(writer, LONEJSON__WRITER_EVENT_CHILD, map,
                                      src, 6u, &root, error);
}

lonejson_status lonejson_writer_finish(lonejson_writer *writer,
                                       lonejson_error *error) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT, "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->failed) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is in a failed state");
  }
  if (state->value_stream_active) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer value stream is still open");
  }
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->number_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer number is still open");
  }
  if (!state->root_written) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer emitted no JSON value");
  }
  if (state->frame_count != 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer has unclosed containers");
  }
  state->finished = 1;
  return LONEJSON_STATUS_OK;
}

lonejson_status
lonejson_write_json_string_sink(lonejson *runtime, lonejson_reader_fn reader,
                                void *reader_user, lonejson_sink_fn sink,
                                void *sink_user, lonejson_error *error) {
  lonejson_writer writer;
  lonejson_status status;

  status = lonejson_writer_init_sink(runtime, &writer, sink, sink_user, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string_reader(&writer, reader, reader_user, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, error);
  }
  lonejson_writer_cleanup(&writer);
  return status;
}

lonejson_status
lonejson_write_json_string_buffer_sink(lonejson *runtime, const void *data,
                                       size_t len, lonejson_sink_fn sink,
                                       void *sink_user, lonejson_error *error) {
  lonejson_writer writer;
  lonejson_status status;

  status = lonejson_writer_init_sink(runtime, &writer, sink, sink_user, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string(&writer, data, len, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, error);
  }
  lonejson_writer_cleanup(&writer);
  return status;
}

lonejson_status lonejson_write_json_string_spooled_sink(
    lonejson *runtime, const lonejson_spooled *value, lonejson_sink_fn sink,
    void *sink_user, lonejson_error *error) {
  lonejson_writer writer;
  lonejson_status status;

  status = lonejson_writer_init_sink(runtime, &writer, sink, sink_user, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string_spooled(&writer, value, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, error);
  }
  lonejson_writer_cleanup(&writer);
  return status;
}
