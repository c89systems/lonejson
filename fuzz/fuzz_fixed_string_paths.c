#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_fixed_stage_doc {
  char payload[4096];
  char note[64];
} fuzz_fixed_stage_doc;

static const lonejson_field fuzz_fixed_stage_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(fuzz_fixed_stage_doc, payload, "payload",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(fuzz_fixed_stage_doc, note, "note",
                                LONEJSON_OVERFLOW_TRUNCATE)};
LONEJSON_MAP_DEFINE(fuzz_fixed_stage_doc_map, fuzz_fixed_stage_doc,
                    fuzz_fixed_stage_doc_fields);

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

static char *fuzz_build_seed_json(size_t payload_len, size_t note_len) {
  char *json;
  char *out;
  size_t cap = 64u + payload_len + note_len;
  size_t remaining;

  json = (char *)malloc(cap + 1u);
  if (json == NULL) {
    return NULL;
  }
  out = json;
  remaining = cap + 1u;
  if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
      !fuzz_append_fill(&out, &remaining, 'a', payload_len) ||
      !fuzz_append_text(&out, &remaining, "\",\"note\":\"") ||
      !fuzz_append_fill(&out, &remaining, 'n', note_len) ||
      !fuzz_append_text(&out, &remaining, "\"}")) {
    free(json);
    return NULL;
  }
  return json;
}

static char *fuzz_build_followup_json(const uint8_t *data, size_t size,
                                      unsigned variant) {
  size_t payload_len = size > 3u ? ((size_t)data[3] * 13u) % 3584u : 0u;
  size_t note_len = size > 4u ? ((size_t)data[4] % 48u) : 0u;
  char *json;
  char *out;
  size_t cap = 128u + (payload_len * 2u) + note_len;
  size_t remaining;

  json = (char *)malloc(cap + 1u);
  if (json == NULL) {
    return NULL;
  }
  out = json;
  remaining = cap + 1u;

  switch (variant & 7u) {
  case 0u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'b', payload_len) ||
        !fuzz_append_text(&out, &remaining, "\",\"note\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'm', note_len) ||
        !fuzz_append_text(&out, &remaining, "\"}")) {
      free(json);
      return NULL;
    }
    break;
  case 1u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'c', payload_len) ||
        !fuzz_append_text(&out, &remaining, "\",\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'd', payload_len / 2u) ||
        !fuzz_append_text(&out, &remaining, "\"}")) {
      free(json);
      return NULL;
    }
    break;
  case 2u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'e', payload_len / 2u) ||
        !fuzz_append_text(&out, &remaining, "\\u0000tail\",\"note\":\"ok\"}")) {
      free(json);
      return NULL;
    }
    break;
  case 3u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'f', payload_len) ||
        !fuzz_append_text(&out, &remaining, "\"")) {
      free(json);
      return NULL;
    }
    break;
  case 4u:
    if (!fuzz_append_text(&out, &remaining,
                          "{\"payload\":false,\"note\":123}")) {
      free(json);
      return NULL;
    }
    break;
  case 5u:
    if (!fuzz_append_text(&out, &remaining,
                          "{\"payload\":\"\",\"note\":\"\"}")) {
      free(json);
      return NULL;
    }
    break;
  case 6u:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'g', payload_len) ||
        !fuzz_append_text(&out, &remaining, "\",\"note\":\"steady\"}")) {
      free(json);
      return NULL;
    }
    break;
  default:
    if (!fuzz_append_text(&out, &remaining, "{\"payload\":\"") ||
        !fuzz_append_fill(&out, &remaining, 'h', payload_len) ||
        !fuzz_append_text(&out, &remaining, "\",\"note\":\"tail") ||
        !fuzz_append_fill(&out, &remaining, 'z', note_len) ||
        !fuzz_append_text(&out, &remaining, "\"}")) {
      free(json);
      return NULL;
    }
    break;
  }
  return json;
}

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  lonejson *seed_runtime = NULL;
  lonejson *runtime = NULL;
  lonejson_config config;
  lonejson_error error;
  fuzz_fixed_stage_doc doc;
  unsigned char scratch[4096];
  char *seed_json = NULL;
  char *followup_json = NULL;
  size_t seed_payload_len = 1024u + (size > 0u ? ((size_t)data[0] * 7u) : 0u);
  size_t seed_note_len = size > 1u ? ((size_t)data[1] % 32u) : 0u;
  size_t scratch_size = 0u;

  if (seed_payload_len >= sizeof(doc.payload)) {
    seed_payload_len = sizeof(doc.payload) - 1u;
  }

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  if (size > 5u) {
    config.max_alloc_bytes =
        (data[5] & 1u) != 0u ? 0u : (size_t)(8u + ((size_t)data[5] * 8u));
  }
  if (size > 6u) {
    config.max_dynamic_string_bytes =
        (data[6] & 1u) != 0u ? 0u : (size_t)(1u + (data[6] & 127u));
  }
  if (size > 7u && (data[7] & 1u) != 0u) {
    scratch_size = size > 8u ? ((size_t)data[8] * 17u) % sizeof(scratch)
                             : (sizeof(scratch) / 2u);
    if (scratch_size == 0u) {
      scratch_size = 1u;
    }
    config.fixed_string_scratch = scratch;
    config.fixed_string_scratch_size = scratch_size;
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

  lonejson_init(seed_runtime, &fuzz_fixed_stage_doc_map, &doc, NULL);
  seed_json = fuzz_build_seed_json(seed_payload_len, seed_note_len);
  if (seed_json != NULL) {
    (void)lonejson_parse_cstr(seed_runtime, &fuzz_fixed_stage_doc_map, &doc,
                              seed_json, &error);
  }

  followup_json =
      fuzz_build_followup_json(data, size, size > 2u ? data[2] : 0u);
  if (followup_json != NULL) {
    char buffer[512];
    size_t needed = 0u;
    (void)lonejson_parse_cstr(runtime, &fuzz_fixed_stage_doc_map, &doc,
                              followup_json, &error);
    (void)lonejson_serialize_buffer(runtime, &fuzz_fixed_stage_doc_map, &doc,
                                    buffer, sizeof(buffer), &needed, &error);
  }

  free(followup_json);
  free(seed_json);
  lonejson_cleanup(&fuzz_fixed_stage_doc_map, &doc, NULL);
  lonejson_free(runtime);
  lonejson_free(seed_runtime);
  return 0;
}
