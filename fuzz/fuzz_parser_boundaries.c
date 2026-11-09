#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_boundary_doc {
  lonejson_json_value payload;
} fuzz_boundary_doc;

typedef struct fuzz_visit_state {
  size_t events;
  size_t bytes;
} fuzz_visit_state;

static const lonejson_field fuzz_boundary_doc_fields[] = {
    LONEJSON_FIELD_JSON_VALUE(fuzz_boundary_doc, payload, "payload")};
LONEJSON_MAP_DEFINE(fuzz_boundary_doc_map, fuzz_boundary_doc,
                    fuzz_boundary_doc_fields);

static lonejson_status fuzz_visit_event(void *user, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_chunk(void *user, const char *data,
                                        size_t len, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)data;
  (void)error;
  state->events++;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static int fuzz_append_text(char **out, size_t *remaining, const char *text) {
  size_t len = strlen(text);
  if (len >= *remaining) {
    return 0;
  }
  memcpy(*out, text, len);
  *out += len;
  *remaining -= len;
  **out = '\0';
  return 1;
}

static int fuzz_append_repeat(char **out, size_t *remaining, const char *text,
                              size_t count) {
  size_t i;
  for (i = 0u; i < count; ++i) {
    if (!fuzz_append_text(out, remaining, text)) {
      return 0;
    }
  }
  return 1;
}

static int fuzz_append_filled_string(char **out, size_t *remaining, char ch,
                                     size_t len) {
  if (!fuzz_append_text(out, remaining, "\"")) {
    return 0;
  }
  if (len >= *remaining) {
    return 0;
  }
  memset(*out, (unsigned char)ch, len);
  *out += len;
  *remaining -= len;
  **out = '\0';
  return fuzz_append_text(out, remaining, "\"");
}

static char *fuzz_build_boundary_json(const uint8_t *data, size_t size) {
  char *json;
  char *out;
  size_t cap;
  size_t remaining;
  size_t depth = 1u + (size > 1u ? ((size_t)data[1] & 31u) : 0u);
  size_t width = 1u + (size > 2u ? ((size_t)data[2] & 15u) : 0u);
  size_t string_len = 8u + (size > 3u ? ((size_t)data[3] * 3u) : 0u);
  unsigned variant = size > 0u ? (unsigned)(data[0] & 7u) : 0u;
  size_t i;

  cap = 512u + (depth * 32u) + (width * (string_len + 32u));
  json = (char *)malloc(cap + 1u);
  if (json == NULL) {
    return NULL;
  }
  out = json;
  remaining = cap + 1u;

  switch (variant) {
  case 0u:
    for (i = 0u; i < depth; ++i) {
      if (!fuzz_append_text(&out, &remaining, "[")) {
        free(json);
        return NULL;
      }
    }
    if (!fuzz_append_filled_string(&out, &remaining, 'a', string_len)) {
      free(json);
      return NULL;
    }
    for (i = 0u; i < depth; ++i) {
      if (!fuzz_append_text(&out, &remaining, "]")) {
        free(json);
        return NULL;
      }
    }
    break;
  case 1u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_repeat(&out, &remaining, "\\uD83D\\uDE00",
                            width + depth) ||
        !fuzz_append_text(&out, &remaining, "\"}")) {
      free(json);
      return NULL;
    }
    break;
  case 2u:
    if (!fuzz_append_text(&out, &remaining, "[")) {
      free(json);
      return NULL;
    }
    for (i = 0u; i < width; ++i) {
      if (!fuzz_append_text(&out, &remaining, i == 0u ? "" : ",") ||
          !fuzz_append_text(&out, &remaining, "-0.") ||
          !fuzz_append_repeat(&out, &remaining, "0", depth) ||
          !fuzz_append_text(&out, &remaining, "1e+") ||
          !fuzz_append_text(&out, &remaining,
                            (i & 1u) != 0u ? "308" : "999999")) {
        free(json);
        return NULL;
      }
    }
    if (!fuzz_append_text(&out, &remaining, "]")) {
      free(json);
      return NULL;
    }
    break;
  case 3u:
    if (!fuzz_append_text(&out, &remaining, "{")) {
      free(json);
      return NULL;
    }
    for (i = 0u; i < width; ++i) {
      if (!fuzz_append_text(&out, &remaining,
                            i == 0u ? "\"k\":\"" : ",\"k\":\"") ||
          !fuzz_append_repeat(&out, &remaining, "\\u0000",
                              (i & 1u) != 0u ? 1u : 0u) ||
          !fuzz_append_repeat(&out, &remaining, "x", depth) ||
          !fuzz_append_text(&out, &remaining, "\"")) {
        free(json);
        return NULL;
      }
    }
    if (!fuzz_append_text(&out, &remaining, "}")) {
      free(json);
      return NULL;
    }
    break;
  case 4u:
    for (i = 0u; i < depth; ++i) {
      if (!fuzz_append_text(&out, &remaining, "{\"nest\":")) {
        free(json);
        return NULL;
      }
    }
    if (!fuzz_append_text(&out, &remaining, "[")) {
      free(json);
      return NULL;
    }
    for (i = 0u; i < width; ++i) {
      if (!fuzz_append_text(&out, &remaining, i == 0u ? "" : ",") ||
          !fuzz_append_filled_string(&out, &remaining, 'z', string_len)) {
        free(json);
        return NULL;
      }
    }
    if (!fuzz_append_text(&out, &remaining, "]")) {
      free(json);
      return NULL;
    }
    for (i = 0u; i < depth; ++i) {
      if (!fuzz_append_text(&out, &remaining, "}")) {
        free(json);
        return NULL;
      }
    }
    break;
  case 5u:
    if (!fuzz_append_text(&out, &remaining, "\"") ||
        !fuzz_append_repeat(&out, &remaining, "\\uD83D", width + 1u)) {
      free(json);
      return NULL;
    }
    break;
  case 6u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":[") ||
        !fuzz_append_repeat(&out, &remaining, "[", depth) ||
        !fuzz_append_text(&out, &remaining, "0") ||
        !fuzz_append_repeat(&out, &remaining, "]", depth) ||
        !fuzz_append_text(&out, &remaining, "]}")) {
      free(json);
      return NULL;
    }
    break;
  default:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":")) {
      free(json);
      return NULL;
    }
    if (!fuzz_append_filled_string(&out, &remaining, 'q', string_len * 2u)) {
      free(json);
      return NULL;
    }
    if (!fuzz_append_text(&out, &remaining, ",")) {
      free(json);
      return NULL;
    }
    if (!fuzz_append_text(&out, &remaining, "\"tail\":")) {
      free(json);
      return NULL;
    }
    if (!fuzz_append_filled_string(&out, &remaining, 'r', string_len)) {
      free(json);
      return NULL;
    }
    if (!fuzz_append_text(&out, &remaining, "}")) {
      free(json);
      return NULL;
    }
    break;
  }

  return json;
}

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_error error;
  lonejson_status status;
  lonejson_value_visitor visitor;
  fuzz_boundary_doc doc;
  fuzz_visit_state state;
  char *json;

  config = lonejson_default_config();
  if (size > 4u) {
    config.max_depth =
        (data[4] & 1u) != 0u ? 0u : (size_t)(1u + (data[4] & 31u));
  }
  if (size > 5u) {
    config.reject_duplicate_keys_by_default = (data[5] & 1u) != 0u ? 1 : 0;
  }
  if (size > 6u) {
    config.json_value_max_total_bytes =
        (data[6] & 1u) != 0u ? 0u : (size_t)(16u + ((size_t)data[6] * 32u));
  }
  if (size > 7u) {
    config.json_value_max_string_bytes =
        (data[7] & 1u) != 0u ? 0u : (size_t)(8u + ((size_t)data[7] * 8u));
  }

  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return 0;
  }

  json = fuzz_build_boundary_json(data, size);
  if (json == NULL) {
    lonejson_free(runtime);
    return 0;
  }

  memset(&state, 0, sizeof(state));
  memset(&visitor, 0, sizeof(visitor));
  visitor.object_begin = fuzz_visit_event;
  visitor.object_end = fuzz_visit_event;
  visitor.object_key_chunk = fuzz_visit_chunk;
  visitor.array_begin = fuzz_visit_event;
  visitor.array_end = fuzz_visit_event;
  visitor.string_begin = fuzz_visit_event;
  visitor.string_chunk = fuzz_visit_chunk;
  visitor.string_end = fuzz_visit_event;
  visitor.number_begin = fuzz_visit_event;
  visitor.number_chunk = fuzz_visit_chunk;
  visitor.number_end = fuzz_visit_event;
  visitor.boolean_value = NULL;
  visitor.null_value = fuzz_visit_event;

  (void)lonejson_validate_cstr(runtime, json, &error);
  (void)lonejson_visit_value_cstr(runtime, json, &visitor, &state, &error);

  memset(&doc, 0, sizeof(doc));
  lonejson_init(runtime, &fuzz_boundary_doc_map, &doc, NULL);
  (void)lonejson_json_value_enable_parse_capture(&doc.payload, &error);
  status =
      lonejson_parse_cstr(runtime, &fuzz_boundary_doc_map, &doc, json, &error);
  if (status == LONEJSON_STATUS_OK) {
    char sink_buffer[512];
    size_t needed = 0u;
    (void)lonejson_serialize_buffer(runtime, &fuzz_boundary_doc_map, &doc,
                                    sink_buffer, sizeof(sink_buffer), &needed,
                                    &error);
  }
  lonejson_cleanup(&fuzz_boundary_doc_map, &doc, NULL);
  free(json);
  lonejson_free(runtime);
  return 0;
}
