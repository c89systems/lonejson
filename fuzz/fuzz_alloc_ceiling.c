#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_budget_doc {
  char *name;
  char *title;
  lonejson_string_array tags;
  lonejson_json_value metadata;
  char payload[2048];
} fuzz_budget_doc;

static const lonejson_field fuzz_budget_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(fuzz_budget_doc, name, "name"),
    LONEJSON_FIELD_STRING_ALLOC(fuzz_budget_doc, title, "title"),
    LONEJSON_FIELD_STRING_ARRAY(fuzz_budget_doc, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE(fuzz_budget_doc, metadata, "metadata"),
    LONEJSON_FIELD_STRING_FIXED_REQ(fuzz_budget_doc, payload, "payload",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(fuzz_budget_doc_map, fuzz_budget_doc,
                    fuzz_budget_doc_fields);

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

static int fuzz_append_fill(char **out, size_t *remaining, char ch,
                            size_t len) {
  if (len >= *remaining) {
    return 0;
  }
  memset(*out, (unsigned char)ch, len);
  *out += len;
  *remaining -= len;
  **out = '\0';
  return 1;
}

static int fuzz_append_tags(char **out, size_t *remaining, size_t count,
                            size_t tag_len) {
  size_t i;
  if (!fuzz_append_text(out, remaining, "[")) {
    return 0;
  }
  for (i = 0u; i < count; ++i) {
    if (!fuzz_append_text(out, remaining, i == 0u ? "\"" : ",\"") ||
        !fuzz_append_fill(out, remaining, (char)('A' + (i % 26u)), tag_len) ||
        !fuzz_append_text(out, remaining, "\"")) {
      return 0;
    }
  }
  return fuzz_append_text(out, remaining, "]");
}

static int fuzz_append_metadata(char **out, size_t *remaining, size_t depth,
                                size_t width, size_t string_len) {
  size_t i;
  for (i = 0u; i < depth; ++i) {
    if (!fuzz_append_text(out, remaining, "{\"nest\":[")) {
      return 0;
    }
  }
  for (i = 0u; i < width; ++i) {
    if (!fuzz_append_text(out, remaining, i == 0u ? "\"" : ",\"") ||
        !fuzz_append_fill(out, remaining, 'm', string_len) ||
        !fuzz_append_text(out, remaining, "\"")) {
      return 0;
    }
  }
  for (i = 0u; i < depth; ++i) {
    if (!fuzz_append_text(out, remaining, "]}")) {
      return 0;
    }
  }
  return 1;
}

static char *fuzz_build_budget_json(size_t name_len, size_t title_len,
                                    size_t tag_count, size_t tag_len,
                                    size_t payload_len, size_t meta_depth,
                                    size_t meta_width, size_t meta_string_len,
                                    unsigned variant) {
  char *json;
  char *out;
  size_t cap = 256u + name_len + title_len + payload_len +
               (tag_count * (tag_len + 4u)) + (meta_depth * 16u) +
               (meta_width * (meta_string_len + 4u));
  size_t remaining;

  json = (char *)malloc(cap + 1u);
  if (json == NULL) {
    return NULL;
  }
  out = json;
  remaining = cap + 1u;

  if (!fuzz_append_text(&out, &remaining, "{\"name\":\"") ||
      !fuzz_append_fill(&out, &remaining, 'n', name_len) ||
      !fuzz_append_text(&out, &remaining, "\",\"title\":\"") ||
      !fuzz_append_fill(&out, &remaining, 't', title_len) ||
      !fuzz_append_text(&out, &remaining, "\",\"tags\":") ||
      !fuzz_append_tags(&out, &remaining, tag_count, tag_len) ||
      !fuzz_append_text(&out, &remaining, ",\"metadata\":")) {
    free(json);
    return NULL;
  }

  switch (variant & 3u) {
  case 0u:
    if (!fuzz_append_metadata(&out, &remaining, meta_depth, meta_width,
                              meta_string_len)) {
      free(json);
      return NULL;
    }
    break;
  case 1u:
    if (!fuzz_append_text(&out, &remaining, "{\"dup\":1,\"dup\":2}")) {
      free(json);
      return NULL;
    }
    break;
  case 2u:
    if (!fuzz_append_text(&out, &remaining, "\"meta\\u0000tail\"")) {
      free(json);
      return NULL;
    }
    break;
  default:
    if (!fuzz_append_text(&out, &remaining, "[")) {
      free(json);
      return NULL;
    }
    if (!fuzz_append_metadata(&out, &remaining, meta_depth, meta_width,
                              meta_string_len) ||
        !fuzz_append_text(&out, &remaining, "]")) {
      free(json);
      return NULL;
    }
    break;
  }

  if (!fuzz_append_text(&out, &remaining, ",\"payload\":\"") ||
      !fuzz_append_fill(&out, &remaining, 'p', payload_len) ||
      !fuzz_append_text(&out, &remaining, "\"}")) {
    free(json);
    return NULL;
  }
  return json;
}

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  lonejson *seed_runtime = NULL;
  lonejson *runtime = NULL;
  lonejson_config config;
  lonejson_error error;
  fuzz_budget_doc doc;
  char *seed_json = NULL;
  char *followup_json = NULL;
  size_t name_len = 32u + (size > 0u ? ((size_t)data[0] & 63u) : 0u);
  size_t title_len = 16u + (size > 1u ? ((size_t)data[1] & 63u) : 0u);
  size_t tag_count = size > 2u ? ((size_t)data[2] & 7u) : 0u;
  size_t tag_len = 8u + (size > 3u ? ((size_t)data[3] & 31u) : 0u);
  size_t payload_len = 256u + (size > 4u ? ((size_t)data[4] * 5u) : 0u);
  size_t meta_depth = 1u + (size > 5u ? ((size_t)data[5] & 3u) : 0u);
  size_t meta_width = 1u + (size > 6u ? ((size_t)data[6] & 7u) : 0u);
  size_t meta_string_len = 8u + (size > 7u ? ((size_t)data[7] & 31u) : 0u);

  if (payload_len >= sizeof(doc.payload)) {
    payload_len = sizeof(doc.payload) - 1u;
  }

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  if (size > 8u) {
    config.max_alloc_bytes =
        (data[8] & 1u) != 0u ? 0u : (size_t)(16u + ((size_t)data[8] * 8u));
  }
  if (size > 9u) {
    config.max_dynamic_string_bytes =
        (data[9] & 1u) != 0u ? 0u : (size_t)(1u + ((size_t)data[9] * 4u));
  }
  if (size > 10u) {
    config.reject_duplicate_keys_by_default = (data[10] & 1u) != 0u ? 1 : 0;
  }

  memset(&doc, 0, sizeof(doc));
  seed_runtime = lonejson_new(NULL, &error);
  if (seed_runtime == NULL) {
    return 0;
  }
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    lonejson_free(seed_runtime);
    return 0;
  }

  lonejson_init(seed_runtime, &fuzz_budget_doc_map, &doc, NULL);
  (void)lonejson_json_value_enable_parse_capture(&doc.metadata, &error);

  seed_json = fuzz_build_budget_json(name_len, title_len, tag_count, tag_len,
                                     payload_len, meta_depth, meta_width,
                                     meta_string_len, 0u);
  if (seed_json != NULL) {
    (void)lonejson_parse_cstr(seed_runtime, &fuzz_budget_doc_map, &doc,
                              seed_json, &error);
  }

  followup_json = fuzz_build_budget_json(
      1u + (name_len / 2u), title_len, tag_count + 1u, tag_len / 2u,
      payload_len, meta_depth, meta_width + 1u, meta_string_len,
      size > 11u ? data[11] : 0u);
  if (followup_json != NULL) {
    char buffer[512];
    size_t needed = 0u;
    (void)lonejson_parse_cstr(runtime, &fuzz_budget_doc_map, &doc,
                              followup_json, &error);
    (void)lonejson_serialize_buffer(runtime, &fuzz_budget_doc_map, &doc, buffer,
                                    sizeof(buffer), &needed, &error);
  }

  free(followup_json);
  free(seed_json);
  lonejson_cleanup(&fuzz_budget_doc_map, &doc, NULL);
  lonejson_free(runtime);
  lonejson_free(seed_runtime);
  return 0;
}
