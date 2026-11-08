#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_json_value_doc {
  char id[16];
  lonejson_json_value selector;
  lonejson_json_value fields;
  lonejson_json_value last_error;
} fuzz_json_value_doc;

typedef struct fuzz_sink_state {
  size_t total;
} fuzz_sink_state;

typedef struct fuzz_path_sink_state {
  size_t callback_count;
  size_t fail_after;
  size_t segment_count_total;
  size_t segment_bytes_total;
  size_t chunk_bytes_total;
} fuzz_path_sink_state;

static lonejson_status fuzz_count_sink(void *user, const void *data, size_t len,
                                       lonejson_error *error) {
  fuzz_sink_state *state = (fuzz_sink_state *)user;
  (void)data;
  (void)error;
  state->total += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_path_maybe_fail(fuzz_path_sink_state *state) {
  ++state->callback_count;
  if (state->fail_after != 0u && state->callback_count >= state->fail_after) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_path_observe(fuzz_path_sink_state *state,
                                         const lonejson_value_path *path) {
  size_t i;

  if (path == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (path->segment_count != 0u && path->segments == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  state->segment_count_total += path->segment_count;
  for (i = 0u; i < path->segment_count; ++i) {
    if (path->segments[i].len != 0u && path->segments[i].data == NULL) {
      return LONEJSON_STATUS_CALLBACK_FAILED;
    }
    state->segment_bytes_total += path->segments[i].len;
  }
  return fuzz_path_maybe_fail(state);
}

static lonejson_status fuzz_path_event(void *user,
                                       const lonejson_value_path *path,
                                       lonejson_error *error) {
  (void)error;
  return fuzz_path_observe((fuzz_path_sink_state *)user, path);
}

static lonejson_status fuzz_path_chunk(void *user,
                                       const lonejson_value_path *path,
                                       const char *data, size_t len,
                                       lonejson_error *error) {
  fuzz_path_sink_state *state = (fuzz_path_sink_state *)user;
  lonejson_status status;

  (void)data;
  (void)error;
  state->chunk_bytes_total += len;
  status = fuzz_path_observe(state, path);
  return status;
}

static lonejson_status fuzz_path_bool(void *user,
                                      const lonejson_value_path *path,
                                      int value, lonejson_error *error) {
  (void)value;
  (void)error;
  return fuzz_path_observe((fuzz_path_sink_state *)user, path);
}

static lonejson_path_value_visitor fuzz_path_visitor(void) {
  lonejson_path_value_visitor visitor = lonejson_default_path_value_visitor();

  visitor.object_begin = fuzz_path_event;
  visitor.object_end = fuzz_path_event;
  visitor.object_key_begin = fuzz_path_event;
  visitor.object_key_chunk = fuzz_path_chunk;
  visitor.object_key_end = fuzz_path_event;
  visitor.array_begin = fuzz_path_event;
  visitor.array_end = fuzz_path_event;
  visitor.string_begin = fuzz_path_event;
  visitor.string_chunk = fuzz_path_chunk;
  visitor.string_end = fuzz_path_event;
  visitor.number_begin = fuzz_path_event;
  visitor.number_chunk = fuzz_path_chunk;
  visitor.number_end = fuzz_path_event;
  visitor.boolean_value = fuzz_path_bool;
  visitor.null_value = fuzz_path_event;
  return visitor;
}

static const lonejson_field fuzz_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(fuzz_json_value_doc, id, "id",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_JSON_VALUE(fuzz_json_value_doc, selector, "selector"),
    LONEJSON_FIELD_JSON_VALUE(fuzz_json_value_doc, fields, "fields"),
    LONEJSON_FIELD_JSON_VALUE(fuzz_json_value_doc, last_error, "last_error")};
LONEJSON_MAP_DEFINE(fuzz_json_value_doc_map, fuzz_json_value_doc,
                    fuzz_json_value_doc_fields);

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  lonejson *runtime;
  lonejson *runtime_pretty;
  lonejson *runtime_streamed;
  lonejson_config config;
  fuzz_json_value_doc doc;
  fuzz_json_value_doc streamed;
  lonejson_error error;
  lonejson_status status;
  fuzz_sink_state selector_sink;
  fuzz_sink_state fields_sink;
  fuzz_sink_state error_sink;
  fuzz_json_value_doc path_doc;
  lonejson_path_value_visitor path_visitor;
  fuzz_path_sink_state selector_path_state;
  fuzz_path_sink_state fields_path_state;
  fuzz_path_sink_state error_path_state;

  memset(&doc, 0, sizeof(doc));
  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  status = lonejson_parse_buffer(runtime, &fuzz_json_value_doc_map, &doc, data,
                                 size, &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    char compact[2048];
    char pretty[4096];
    size_t needed = 0u;
    config = lonejson_default_config();
    config.write_pretty = 1;
    runtime_pretty = lonejson_new(&config, &error);
    if (runtime_pretty == NULL) {
      runtime_pretty = NULL;
    }

    (void)lonejson_serialize_buffer(runtime, &fuzz_json_value_doc_map, &doc,
                                    compact, sizeof(compact), &needed, &error);
    if (runtime_pretty != NULL) {
      (void)lonejson_serialize_buffer(runtime_pretty, &fuzz_json_value_doc_map,
                                      &doc, pretty, sizeof(pretty), &needed,
                                      &error);
      lonejson_free(runtime_pretty);
    }
  }
  lonejson_cleanup(&fuzz_json_value_doc_map, &doc, NULL);
  lonejson_free(runtime);

  memset(&streamed, 0, sizeof(streamed));
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  runtime_streamed = lonejson_new(&config, &error);
  if (runtime_streamed == NULL) {
    return 0;
  }
  lonejson_init(runtime_streamed, &fuzz_json_value_doc_map, &streamed, NULL);
  memset(&selector_sink, 0, sizeof(selector_sink));
  memset(&fields_sink, 0, sizeof(fields_sink));
  memset(&error_sink, 0, sizeof(error_sink));
  lonejson_json_value_set_parse_sink(&streamed.selector, fuzz_count_sink,
                                     &selector_sink, &error);
  lonejson_json_value_set_parse_sink(&streamed.fields, fuzz_count_sink,
                                     &fields_sink, &error);
  lonejson_json_value_set_parse_sink(&streamed.last_error, fuzz_count_sink,
                                     &error_sink, &error);
  (void)lonejson_parse_buffer(runtime_streamed, &fuzz_json_value_doc_map,
                              &streamed, data, size, &error);
  lonejson_cleanup(&fuzz_json_value_doc_map, &streamed, NULL);
  lonejson_free(runtime_streamed);

  memset(&path_doc, 0, sizeof(path_doc));
  memset(&selector_path_state, 0, sizeof(selector_path_state));
  memset(&fields_path_state, 0, sizeof(fields_path_state));
  memset(&error_path_state, 0, sizeof(error_path_state));
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  if (size > 0u && (data[0] & 1u) != 0u) {
    config.json_value_max_key_bytes = 8u + (size_t)(data[0] & 7u);
    config.json_value_max_string_bytes = 8u + (size_t)(data[0] & 15u);
    config.json_value_max_number_bytes = 8u + (size_t)(data[0] & 7u);
  }
  if (size > 1u && (data[1] & 1u) != 0u) {
    selector_path_state.fail_after = 1u + (size_t)(data[1] & 31u);
    fields_path_state.fail_after = 1u + (size_t)((data[1] >> 1) & 31u);
    error_path_state.fail_after = 1u + (size_t)((data[1] >> 2) & 31u);
  }
  runtime_streamed = lonejson_new(&config, &error);
  if (runtime_streamed == NULL) {
    return 0;
  }
  lonejson_init(runtime_streamed, &fuzz_json_value_doc_map, &path_doc, NULL);
  path_visitor = fuzz_path_visitor();
  (void)lonejson_json_value_set_parse_path_visitor(
      &path_doc.selector, &path_visitor, &selector_path_state, &error);
  (void)lonejson_json_value_set_parse_path_visitor(
      &path_doc.fields, &path_visitor, &fields_path_state, &error);
  (void)lonejson_json_value_set_parse_path_visitor(
      &path_doc.last_error, &path_visitor, &error_path_state, &error);
  (void)lonejson_parse_buffer(runtime_streamed, &fuzz_json_value_doc_map,
                              &path_doc, data, size, &error);
  lonejson_cleanup(&fuzz_json_value_doc_map, &path_doc, NULL);
  lonejson_free(runtime_streamed);
  return 0;
}
