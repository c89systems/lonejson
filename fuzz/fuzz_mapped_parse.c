#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_address {
  char city[16];
  lonejson_int64 zip;
} fuzz_address;

typedef struct fuzz_person {
  char *name;
  char nickname[8];
  lonejson_int64 age;
  bool active;
  fuzz_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_string_array tags;
} fuzz_person;

typedef struct fuzz_stream_envelope {
  lonejson_string_array_stream keys;
  char cursor[16];
  lonejson_uint64 index_seq;
  lonejson_json_value metadata;
} fuzz_stream_envelope;

typedef struct fuzz_nullable_primitives {
  lonejson_int64 count;
  int has_count;
  lonejson_uint64 seed;
  int has_seed;
  double ratio;
  int has_ratio;
  bool enabled;
  int has_enabled;
  lonejson_int64 required_count;
} fuzz_nullable_primitives;

typedef struct fuzz_stream_state {
  size_t begins;
  size_t chunks;
  size_t ends;
  size_t fail_after_chunks;
} fuzz_stream_state;

typedef struct fuzz_capped_alloc_doc {
  char *name;
  lonejson_string_array tags;
  lonejson_spooled body;
} fuzz_capped_alloc_doc;

static const lonejson_field fuzz_address_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(fuzz_address, city, "city",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(fuzz_address, zip, "zip")};
LONEJSON_MAP_DEFINE(fuzz_address_map, fuzz_address, fuzz_address_fields);

static const lonejson_field fuzz_person_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(fuzz_person, name, "name"),
    LONEJSON_FIELD_STRING_FIXED(fuzz_person, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(fuzz_person, age, "age"),
    LONEJSON_FIELD_BOOL(fuzz_person, active, "active"),
    LONEJSON_FIELD_OBJECT(fuzz_person, address, "address", &fuzz_address_map),
    LONEJSON_FIELD_I64_ARRAY(fuzz_person, lucky_numbers, "lucky_numbers",
                             LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ARRAY(fuzz_person, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(fuzz_person_map, fuzz_person, fuzz_person_fields);

static const lonejson_field fuzz_stream_envelope_fields[] = {
    LONEJSON_FIELD_STRING_ARRAY_STREAM_REQ(fuzz_stream_envelope, keys, "keys"),
    LONEJSON_FIELD_STRING_FIXED(fuzz_stream_envelope, cursor, "cursor",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_U64(fuzz_stream_envelope, index_seq, "index_seq"),
    LONEJSON_FIELD_JSON_VALUE(fuzz_stream_envelope, metadata, "metadata")};
LONEJSON_MAP_DEFINE(fuzz_stream_envelope_map, fuzz_stream_envelope,
                    fuzz_stream_envelope_fields);

static const lonejson_field fuzz_nullable_primitives_fields[] = {
    LONEJSON_FIELD_I64_PRESENT_NULLABLE(fuzz_nullable_primitives, count,
                                        has_count, "count"),
    LONEJSON_FIELD_U64_PRESENT_NULLABLE(fuzz_nullable_primitives, seed,
                                        has_seed, "seed"),
    LONEJSON_FIELD_F64_PRESENT_NULLABLE(fuzz_nullable_primitives, ratio,
                                        has_ratio, "ratio"),
    LONEJSON_FIELD_BOOL_PRESENT_NULLABLE(fuzz_nullable_primitives, enabled,
                                         has_enabled, "enabled"),
    LONEJSON_FIELD_I64_REQ(fuzz_nullable_primitives, required_count,
                           "required_count")};
LONEJSON_MAP_DEFINE(fuzz_nullable_primitives_map, fuzz_nullable_primitives,
                    fuzz_nullable_primitives_fields);

static const lonejson_field fuzz_capped_alloc_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(fuzz_capped_alloc_doc, name, "name"),
    LONEJSON_FIELD_STRING_ARRAY(fuzz_capped_alloc_doc, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_STREAM_REQ(fuzz_capped_alloc_doc, body, "body")};
LONEJSON_MAP_DEFINE(fuzz_capped_alloc_doc_map, fuzz_capped_alloc_doc,
                    fuzz_capped_alloc_doc_fields);

static lonejson_status fuzz_stream_begin(void *user, lonejson_error *error) {
  fuzz_stream_state *state = (fuzz_stream_state *)user;
  (void)error;
  state->begins++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_stream_chunk(void *user, const char *data,
                                         size_t len, lonejson_error *error) {
  fuzz_stream_state *state = (fuzz_stream_state *)user;
  (void)data;
  (void)len;
  (void)error;
  state->chunks++;
  if (state->fail_after_chunks != 0u &&
      state->chunks >= state->fail_after_chunks) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_stream_end(void *user, lonejson_error *error) {
  fuzz_stream_state *state = (fuzz_stream_state *)user;
  (void)error;
  state->ends++;
  return LONEJSON_STATUS_OK;
}

static void fuzz_parse_stream_envelope(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  fuzz_stream_envelope envelope;
  fuzz_stream_state state;
  lonejson_array_stream_string_handler handler;
  lonejson_error error;

  memset(&envelope, 0, sizeof(envelope));
  memset(&state, 0, sizeof(state));
  memset(&handler, 0, sizeof(handler));
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  if (size > 0u) {
    config.reject_duplicate_keys_by_default = (data[0] & 1u) ? 1 : 0;
    state.fail_after_chunks = (data[0] & 8u) ? ((data[0] & 3u) + 1u) : 0u;
  }
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }
  handler.begin = fuzz_stream_begin;
  handler.chunk = fuzz_stream_chunk;
  handler.end = fuzz_stream_end;

  lonejson_init(runtime, &fuzz_stream_envelope_map, &envelope, NULL);
  (void)lonejson_string_array_stream_set_handler(&envelope.keys, &handler,
                                                 &state, &error);
  (void)lonejson_json_value_enable_parse_capture(&envelope.metadata, &error);
  (void)lonejson_parse_buffer(runtime, &fuzz_stream_envelope_map, &envelope,
                              data, size, &error);
  lonejson_cleanup(&fuzz_stream_envelope_map, &envelope, NULL);
  lonejson_free(runtime);
}

static const char *fuzz_nullable_value(unsigned selector, const char *valid,
                                       const char *wrong) {
  switch (selector & 3u) {
  case 0u:
    return NULL;
  case 1u:
    return "null";
  case 2u:
    return valid;
  default:
    return wrong;
  }
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

static int fuzz_append_field(char **out, size_t *remaining, const char *key,
                             const char *value) {
  return fuzz_append_text(out, remaining, "\"") &&
         fuzz_append_text(out, remaining, key) &&
         fuzz_append_text(out, remaining, "\":") &&
         fuzz_append_text(out, remaining, value) &&
         fuzz_append_text(out, remaining, ",");
}

static void fuzz_parse_nullable_primitives(const uint8_t *data, size_t size) {
  lonejson *runtime;
  fuzz_nullable_primitives doc;
  lonejson_error error;
  lonejson_status status;
  char json[256];
  char *out = json;
  size_t remaining = sizeof(json);
  const char *count;
  const char *seed;
  const char *ratio;
  const char *enabled;
  uint8_t b0 = size > 0u ? data[0] : 0u;
  uint8_t b1 = size > 1u ? data[1] : 0u;
  uint8_t b2 = size > 2u ? data[2] : 0u;

  count = fuzz_nullable_value(b0, "-12", "\"bad\"");
  seed = fuzz_nullable_value(b0 >> 2u, "42", "-1");
  ratio = fuzz_nullable_value(b1, "1.25", "false");
  enabled = fuzz_nullable_value(b1 >> 2u, "true", "0");

  if (!fuzz_append_text(&out, &remaining, "{")) {
    return;
  }
  if (count != NULL) {
    if (!fuzz_append_field(&out, &remaining, "count", count)) {
      return;
    }
  }
  if (seed != NULL) {
    if (!fuzz_append_field(&out, &remaining, "seed", seed)) {
      return;
    }
  }
  if (ratio != NULL) {
    if (!fuzz_append_field(&out, &remaining, "ratio", ratio)) {
      return;
    }
  }
  if (enabled != NULL) {
    if (!fuzz_append_field(&out, &remaining, "enabled", enabled)) {
      return;
    }
  }
  if (!fuzz_append_text(&out, &remaining, "\"required_count\":") ||
      !fuzz_append_text(&out, &remaining, (b2 & 1u) != 0u ? "null" : "7") ||
      !fuzz_append_text(&out, &remaining, "}")) {
    return;
  }

  memset(&doc, 0, sizeof(doc));
  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return;
  }
  status = lonejson_parse_cstr(runtime, &fuzz_nullable_primitives_map, &doc,
                               json, &error);
  if (status == LONEJSON_STATUS_OK) {
    char buffer[256];
    size_t needed;
    (void)lonejson_serialize_buffer(runtime, &fuzz_nullable_primitives_map,
                                    &doc, buffer, sizeof(buffer), &needed,
                                    &error);
  }
  lonejson_free(runtime);
}

static void fuzz_parse_capped_alloc_doc(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  fuzz_capped_alloc_doc doc;
  lonejson_error error;
  lonejson_status status;
  char *json;
  size_t name_len;
  size_t body_len;
  size_t tags;
  size_t tag_len;
  size_t cap;
  char *out;
  size_t i;
  size_t remaining;

  name_len = size > 0u ? (size_t)(data[0] & 127u) : 0u;
  body_len = size > 1u ? (size_t)(data[1] * 64u) : 0u;
  tags = size > 2u ? (size_t)(data[2] & 7u) : 0u;
  tag_len = size > 3u ? (size_t)(data[3] & 63u) : 0u;
  config = lonejson_default_config();
  if (size > 4u) {
    config.max_dynamic_string_bytes =
        (data[4] & 1u) != 0u ? 0u : (size_t)(1u + (data[4] & 63u));
  }
  if (size > 5u) {
    config.max_alloc_bytes =
        (data[5] & 1u) != 0u ? 0u : (size_t)(16u + ((size_t)data[5] * 16u));
  }
  if (size > 6u) {
    config.clear_destination_by_default = (data[6] & 1u) == 0u ? 1 : 0;
  }
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }

  cap = 64u + name_len + body_len + (tags * (tag_len + 8u));
  json = (char *)malloc(cap + 1u);
  if (json == NULL) {
    return;
  }
  out = json;
  remaining = cap + 1u;
  if (!fuzz_append_text(&out, &remaining, "{\"name\":\"")) {
    free(json);
    return;
  }
  for (i = 0u; i < name_len && remaining > 1u; ++i) {
    *out++ = (char)('a' + (i % 26u));
    --remaining;
  }
  if (!fuzz_append_text(&out, &remaining, "\",\"tags\":[")) {
    free(json);
    return;
  }
  for (i = 0u; i < tags; ++i) {
    size_t j;
    if (!fuzz_append_text(&out, &remaining, i == 0u ? "\"" : ",\"")) {
      free(json);
      return;
    }
    for (j = 0u; j < tag_len && remaining > 1u; ++j) {
      *out++ = (char)('A' + (j % 26u));
      --remaining;
    }
    if (!fuzz_append_text(&out, &remaining, "\"")) {
      free(json);
      return;
    }
  }
  if (!fuzz_append_text(&out, &remaining, "],\"body\":\"")) {
    free(json);
    return;
  }
  for (i = 0u; i < body_len && remaining > 1u; ++i) {
    *out++ = 'x';
    --remaining;
  }
  if (!fuzz_append_text(&out, &remaining, "\"}")) {
    free(json);
    return;
  }

  memset(&doc, 0, sizeof(doc));
  lonejson_init(runtime, &fuzz_capped_alloc_doc_map, &doc, NULL);
  status = lonejson_parse_cstr(runtime, &fuzz_capped_alloc_doc_map, &doc, json,
                               &error);
  (void)status;
  lonejson_cleanup(&fuzz_capped_alloc_doc_map, &doc, NULL);
  lonejson_free(runtime);
  free(json);
}

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  lonejson *runtime;
  fuzz_person person;
  lonejson_error error;
  lonejson_status status;

  fuzz_parse_stream_envelope(data, size);
  fuzz_parse_nullable_primitives(data, size);
  fuzz_parse_capped_alloc_doc(data, size);

  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  memset(&person, 0, sizeof(person));
  status = lonejson_parse_buffer(runtime, &fuzz_person_map, &person, data, size,
                                 &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    char buffer[1024];
    size_t needed;
    (void)lonejson_serialize_buffer(runtime, &fuzz_person_map, &person, buffer,
                                    sizeof(buffer), &needed, &error);
  }
  lonejson_cleanup(&fuzz_person_map, &person, NULL);
  lonejson_free(runtime);
  return 0;
}
