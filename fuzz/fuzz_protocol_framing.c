#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_framing_state {
  size_t events;
  size_t parts;
  size_t bytes;
} fuzz_framing_state;

typedef struct fuzz_visit_state {
  size_t events;
  size_t bytes;
} fuzz_visit_state;

typedef struct fuzz_sink_state {
  size_t total;
} fuzz_sink_state;

typedef struct fuzz_sse_json_doc {
  char *type;
  char *status;
  char *text;
} fuzz_sse_json_doc;

static const lonejson_field fuzz_sse_json_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(fuzz_sse_json_doc, type, "type"),
    LONEJSON_FIELD_STRING_ALLOC(fuzz_sse_json_doc, status, "status"),
    LONEJSON_FIELD_STRING_ALLOC(fuzz_sse_json_doc, text, "text")};
LONEJSON_MAP_DEFINE(fuzz_sse_json_doc_map, fuzz_sse_json_doc,
                    fuzz_sse_json_doc_fields);

static lonejson_status fuzz_sse_begin(void *user,
                                      const lonejson_sse_event *event,
                                      lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)event;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sse_data(void *user, const void *bytes, size_t len,
                                     lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)bytes;
  (void)error;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sse_end(void *user, const lonejson_sse_event *event,
                                    lonejson_error *error) {
  (void)user;
  (void)event;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sse_json_event(void *user,
                                           const lonejson_sse_event *event,
                                           void *dst, lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  fuzz_sse_json_doc *doc = (fuzz_sse_json_doc *)dst;
  (void)event;
  (void)error;
  state->events++;
  if (doc->type != NULL) {
    state->bytes += strlen(doc->type);
  }
  if (doc->status != NULL) {
    state->bytes += strlen(doc->status);
  }
  if (doc->text != NULL) {
    state->bytes += strlen(doc->text);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_value_event(void *user, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_value_chunk(void *user, const char *data,
                                        size_t len, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)data;
  (void)error;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_value_bool(void *user, int value,
                                       lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)value;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sink_write(void *user, const void *data, size_t len,
                                       lonejson_error *error) {
  fuzz_sink_state *state = (fuzz_sink_state *)user;
  (void)data;
  (void)error;
  state->total += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
fuzz_sse_json_value_sink_event(void *user, const lonejson_sse_event *event,
                               lonejson_json_value *value,
                               lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)event;
  (void)value;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
fuzz_sse_json_value_capture_event(void *user, const lonejson_sse_event *event,
                                  lonejson_json_value *value,
                                  lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  fuzz_sink_state sink_state;
  (void)event;
  state->events++;
  state->bytes += value->len;
  if (value->kind == LONEJSON_JSON_VALUE_BUFFER) {
    memset(&sink_state, 0, sizeof(sink_state));
    (void)lonejson_json_value_write_to_sink(value, fuzz_sink_write, &sink_state,
                                            error);
    state->bytes += sink_state.total;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
fuzz_sse_json_value_visitor_event(void *user, const lonejson_sse_event *event,
                                  lonejson_json_value *value,
                                  lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)event;
  (void)value;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_multipart_begin(void *user,
                                            const lonejson_multipart_part *part,
                                            lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)part;
  (void)error;
  state->parts++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_multipart_data(void *user, const void *bytes,
                                           size_t len, lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)bytes;
  (void)error;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_multipart_end(void *user,
                                          const lonejson_multipart_part *part,
                                          lonejson_error *error) {
  (void)user;
  (void)part;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static void fuzz_sse(const uint8_t *data, size_t size) {
  lonejson_sse_options options = lonejson_default_sse_options();
  lonejson_sse_handler handler;
  fuzz_framing_state state;
  lonejson_sse *sse;
  lonejson_error error;

  options.max_line_bytes = 4096u;
  options.max_event_data_bytes = 65536u;
  options.max_buffered_bytes = 65536u;
  memset(&handler, 0, sizeof(handler));
  handler.begin_event = fuzz_sse_begin;
  handler.data_chunk = fuzz_sse_data;
  handler.end_event = fuzz_sse_end;
  memset(&state, 0, sizeof(state));
  sse = lonejson_sse_open(&options, &error);
  if (sse == NULL) {
    return;
  }
  (void)lonejson_sse_push(sse, data, size, &handler, &state, &error);
  (void)lonejson_sse_finish(sse, &handler, &state, &error);
  lonejson_sse_close(sse);
}

static void fuzz_sse_json(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  static const char *selected_keep[] = {"keep"};
  static const char *selected_empty[] = {""};
  lonejson_sse_options sse_options = lonejson_default_sse_options();
  lonejson_sse_json_options json_options;
  fuzz_framing_state state;
  fuzz_sse_json_doc doc;
  lonejson_sse *sse;
  lonejson_error error;
  size_t offset;
  size_t chunk_size;

  sse_options.max_line_bytes = 4096u;
  sse_options.max_event_data_bytes = 65536u;
  sse_options.max_buffered_bytes = 65536u;
  memset(&json_options, 0, sizeof(json_options));
  memset(&state, 0, sizeof(state));
  memset(&doc, 0, sizeof(doc));
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;
  if (size > 1u && (data[1] & 1u) != 0u) {
    json_options.event_names = selected_keep;
    json_options.event_name_count = 1u;
  } else if (size > 1u) {
    json_options.event_names = selected_empty;
    json_options.event_name_count = 1u;
  }
  config = lonejson_default_config();
  if (size > 2u && (data[2] & 1u) != 0u) {
    config.clear_destination_by_default = 0;
  }
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }
  lonejson_init(runtime, &fuzz_sse_json_doc_map, &doc, NULL);
  sse = lonejson_sse_open(&sse_options, &error);
  if (sse == NULL) {
    lonejson_cleanup(&fuzz_sse_json_doc_map, &doc, NULL);
    lonejson_free(runtime);
    return;
  }
  chunk_size = (size > 0u) ? (1u + ((size_t)data[0] % 97u)) : 17u;
  for (offset = 0u; offset < size; offset += chunk_size) {
    size_t chunk = size - offset;
    if (chunk > chunk_size) {
      chunk = chunk_size;
    }
    (void)lonejson_sse_push_json(runtime, sse, &fuzz_sse_json_doc_map, &doc,
                                 data + offset, chunk, &json_options,
                                 fuzz_sse_json_event, &state, &error);
  }
  (void)lonejson_sse_finish_json(runtime, sse, &fuzz_sse_json_doc_map, &doc,
                                 &json_options, fuzz_sse_json_event, &state,
                                 &error);
  lonejson_sse_close(sse);
  lonejson_cleanup(&fuzz_sse_json_doc_map, &doc, NULL);
  lonejson_free(runtime);
}

static void fuzz_sse_json_value(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  static const char *selected_keep[] = {"keep"};
  static const char *selected_empty[] = {""};
  lonejson_sse_options sse_options = lonejson_default_sse_options();
  lonejson_sse_json_options json_options;
  lonejson_json_value value;
  lonejson_value_visitor visitor;
  fuzz_visit_state visit_state;
  fuzz_framing_state state;
  lonejson_sse *sse;
  lonejson_error error;
  size_t offset;
  size_t chunk_size;

  sse_options.max_line_bytes = 4096u;
  sse_options.max_event_data_bytes = 65536u;
  sse_options.max_buffered_bytes = 65536u;
  memset(&json_options, 0, sizeof(json_options));
  memset(&state, 0, sizeof(state));
  memset(&visit_state, 0, sizeof(visit_state));
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;
  if (size > 1u && (data[1] & 1u) != 0u) {
    json_options.event_names = selected_keep;
    json_options.event_name_count = 1u;
  } else if (size > 1u) {
    json_options.event_names = selected_empty;
    json_options.event_name_count = 1u;
  }
  config = lonejson_default_config();
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }
  lonejson_json_value_init(runtime, &value);
  switch (size > 2u ? (data[2] % 3u) : 0u) {
  case 0u:
    (void)lonejson_json_value_set_parse_sink(&value, fuzz_sink_write, &state,
                                             &error);
    break;
  case 1u:
    visitor = lonejson_default_value_visitor();
    visitor.object_begin = fuzz_value_event;
    visitor.object_end = fuzz_value_event;
    visitor.object_key_begin = fuzz_value_event;
    visitor.object_key_chunk = fuzz_value_chunk;
    visitor.object_key_end = fuzz_value_event;
    visitor.array_begin = fuzz_value_event;
    visitor.array_end = fuzz_value_event;
    visitor.string_begin = fuzz_value_event;
    visitor.string_chunk = fuzz_value_chunk;
    visitor.string_end = fuzz_value_event;
    visitor.number_begin = fuzz_value_event;
    visitor.number_chunk = fuzz_value_chunk;
    visitor.number_end = fuzz_value_event;
    visitor.boolean_value = fuzz_value_bool;
    visitor.null_value = fuzz_value_event;
    (void)lonejson_json_value_set_parse_visitor(&value, &visitor, &visit_state,
                                                &error);
    break;
  default:
    (void)lonejson_json_value_enable_parse_capture(&value, &error);
    break;
  }
  sse = lonejson_sse_open(&sse_options, &error);
  if (sse == NULL) {
    lonejson_json_value_cleanup(&value);
    lonejson_free(runtime);
    return;
  }
  chunk_size = (size > 0u) ? (1u + ((size_t)data[0] % 97u)) : 17u;
  for (offset = 0u; offset < size; offset += chunk_size) {
    size_t chunk = size - offset;
    if (chunk > chunk_size) {
      chunk = chunk_size;
    }
    switch (value.parse_mode) {
    case LONEJSON_JSON_VALUE_PARSE_SINK:
      (void)lonejson_sse_push_json_value(
          runtime, sse, &value, data + offset, chunk, &json_options,
          fuzz_sse_json_value_sink_event, &state, &error);
      break;
    case LONEJSON_JSON_VALUE_PARSE_VISITOR:
      (void)lonejson_sse_push_json_value(
          runtime, sse, &value, data + offset, chunk, &json_options,
          fuzz_sse_json_value_visitor_event, &state, &error);
      state.bytes += visit_state.bytes;
      state.events += visit_state.events;
      memset(&visit_state, 0, sizeof(visit_state));
      break;
    default:
      (void)lonejson_sse_push_json_value(
          runtime, sse, &value, data + offset, chunk, &json_options,
          fuzz_sse_json_value_capture_event, &state, &error);
      break;
    }
  }
  switch (value.parse_mode) {
  case LONEJSON_JSON_VALUE_PARSE_SINK:
    (void)lonejson_sse_finish_json_value(runtime, sse, &value, &json_options,
                                         fuzz_sse_json_value_sink_event, &state,
                                         &error);
    break;
  case LONEJSON_JSON_VALUE_PARSE_VISITOR:
    (void)lonejson_sse_finish_json_value(runtime, sse, &value, &json_options,
                                         fuzz_sse_json_value_visitor_event,
                                         &state, &error);
    state.bytes += visit_state.bytes;
    state.events += visit_state.events;
    break;
  default:
    (void)lonejson_sse_finish_json_value(runtime, sse, &value, &json_options,
                                         fuzz_sse_json_value_capture_event,
                                         &state, &error);
    break;
  }
  lonejson_sse_close(sse);
  lonejson_json_value_cleanup(&value);
  lonejson_free(runtime);
}

static void fuzz_multipart(const uint8_t *data, size_t size) {
  lonejson_multipart_options options = lonejson_default_multipart_options();
  lonejson_multipart_handler handler;
  fuzz_framing_state state;
  lonejson_multipart *mp;
  lonejson_error error;

  options.max_boundary_bytes = 64u;
  options.max_header_line_bytes = 4096u;
  options.max_header_count = 32u;
  options.max_part_buffered_bytes = 65536u;
  memset(&handler, 0, sizeof(handler));
  handler.begin_part = fuzz_multipart_begin;
  handler.part_data = fuzz_multipart_data;
  handler.end_part = fuzz_multipart_end;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=ljboundary", &options,
                               &error);
  if (mp == NULL) {
    return;
  }
  (void)lonejson_multipart_push(mp, data, size, &handler, &state, &error);
  (void)lonejson_multipart_finish(mp, &handler, &state, &error);
  lonejson_multipart_close(mp);
}

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  if (size > 0u && data[0] == (uint8_t)'M') {
    fuzz_multipart(data + 1u, size - 1u);
  } else if (size > 0u && data[0] == (uint8_t)'S') {
    fuzz_sse(data + 1u, size - 1u);
    fuzz_sse_json(data + 1u, size - 1u);
    fuzz_sse_json_value(data + 1u, size - 1u);
  } else if (size > 0u && data[0] == (uint8_t)'J') {
    fuzz_sse_json(data + 1u, size - 1u);
    fuzz_sse_json_value(data + 1u, size - 1u);
  } else {
    fuzz_sse(data, size);
    fuzz_sse_json(data, size);
    fuzz_sse_json_value(data, size);
  }
  return 0;
}
