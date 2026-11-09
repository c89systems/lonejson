#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_item {
  lonejson_int64 id;
  char label[16];
} fuzz_item;

typedef struct fuzz_reader {
  const uint8_t *data;
  size_t len;
  size_t offset;
  size_t chunk_size;
  size_t empty_every;
  size_t would_block_every;
  size_t would_blocks;
  size_t max_would_blocks;
  size_t read_calls;
} fuzz_reader;

typedef struct fuzz_push_state {
  size_t items;
  size_t fail_after;
} fuzz_push_state;

static const lonejson_field fuzz_item_fields[] = {
    LONEJSON_FIELD_I64(fuzz_item, id, "id"),
    LONEJSON_FIELD_STRING_FIXED(fuzz_item, label, "label",
                                LONEJSON_OVERFLOW_TRUNCATE)};
LONEJSON_MAP_DEFINE(fuzz_item_map, fuzz_item, fuzz_item_fields);

static lonejson_status fuzz_push_item(void *user, void *dst) {
  fuzz_push_state *state = (fuzz_push_state *)user;

  lonejson_cleanup(&fuzz_item_map, dst, NULL);
  state->items++;
  if (state->fail_after != 0u && state->items >= state->fail_after) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_read_result
fuzz_array_stream_reader(void *user, unsigned char *buffer, size_t capacity) {
  fuzz_reader *reader = (fuzz_reader *)user;
  lonejson_read_result result;
  size_t remaining;
  size_t n;

  memset(&result, 0, sizeof(result));
  if (reader->offset >= reader->len) {
    result.eof = 1;
    return result;
  }
  reader->read_calls++;
  if (reader->empty_every != 0u &&
      reader->read_calls % reader->empty_every == 0u) {
    return result;
  }
  if (reader->would_block_every != 0u &&
      reader->read_calls % reader->would_block_every == 0u &&
      reader->would_blocks < reader->max_would_blocks) {
    reader->would_blocks++;
    result.would_block = 1;
    return result;
  }
  remaining = reader->len - reader->offset;
  n = reader->chunk_size;
  if (n == 0u || n > remaining) {
    n = remaining;
  }
  if (n > capacity) {
    n = capacity;
  }
  memcpy(buffer, reader->data + reader->offset, n);
  reader->offset += n;
  result.bytes_read = n;
  result.eof = reader->offset == reader->len ? 1 : 0;
  return result;
}

static void fuzz_drive_mapped(const char *path, const uint8_t *json,
                              size_t json_len, size_t chunk_size,
                              int reject_duplicate_keys,
                              size_t would_block_every) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_error error;
  fuzz_reader reader;
  size_t count;

  config = lonejson_default_config();
  config.reject_duplicate_keys_by_default = reject_duplicate_keys;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }
  reader.data = json;
  reader.len = json_len;
  reader.offset = 0u;
  reader.chunk_size = chunk_size;
  reader.empty_every = reject_duplicate_keys ? 5u : 0u;
  reader.would_block_every = would_block_every;
  reader.would_blocks = 0u;
  reader.max_would_blocks = 2u;
  reader.read_calls = 0u;
  stream = lonejson_array_stream_open_reader(
      runtime, path, fuzz_array_stream_reader, &reader, &error);
  if (stream == NULL) {
    lonejson_free(runtime);
    return;
  }
  for (count = 0u; count < 16u; ++count) {
    lonejson_array_stream_result result;
    fuzz_item item;

    memset(&item, 0, sizeof(item));
    result = lonejson_array_stream_next(stream, &fuzz_item_map, &item, &error);
    lonejson_cleanup(&fuzz_item_map, &item, NULL);
    if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
      continue;
    }
    if (result != LONEJSON_ARRAY_STREAM_ITEM) {
      break;
    }
  }
  lonejson_array_stream_close(stream);
  lonejson_free(runtime);
}

static void fuzz_drive_raw(const char *path, const uint8_t *json,
                           size_t json_len, size_t chunk_size,
                           int reject_duplicate_keys,
                           size_t would_block_every) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_error error;
  fuzz_reader reader;
  size_t count;

  config = lonejson_default_config();
  config.reject_duplicate_keys_by_default = reject_duplicate_keys;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }
  reader.data = json;
  reader.len = json_len;
  reader.offset = 0u;
  reader.chunk_size = chunk_size;
  reader.empty_every = reject_duplicate_keys ? 7u : 0u;
  reader.would_block_every = would_block_every;
  reader.would_blocks = 0u;
  reader.max_would_blocks = 2u;
  reader.read_calls = 0u;
  stream = lonejson_array_stream_open_reader(
      runtime, path, fuzz_array_stream_reader, &reader, &error);
  if (stream == NULL) {
    lonejson_free(runtime);
    return;
  }
  for (count = 0u; count < 16u; ++count) {
    lonejson_array_stream_result result;
    lonejson_json_value value;

    lonejson_json_value_init(runtime, &value);
    result = lonejson_array_stream_next_value(stream, &value, &error);
    lonejson_json_value_cleanup(&value);
    if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
      continue;
    }
    if (result != LONEJSON_ARRAY_STREAM_ITEM) {
      break;
    }
  }
  lonejson_array_stream_close(stream);
  lonejson_free(runtime);
}

static void fuzz_drive_push(const char *path, const uint8_t *json,
                            size_t json_len, size_t chunk_size,
                            int reject_duplicate_keys, size_t fail_after) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_error error;
  fuzz_push_state state;
  fuzz_item item;
  size_t offset;

  config = lonejson_default_config();
  config.reject_duplicate_keys_by_default = reject_duplicate_keys;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return;
  }
  state.items = 0u;
  state.fail_after = fail_after;
  stream = lonejson_array_stream_open_push(runtime, path, &error);
  if (stream == NULL) {
    lonejson_free(runtime);
    return;
  }
  offset = 0u;
  while (offset < json_len) {
    size_t n = chunk_size;
    lonejson_status status;

    if (n == 0u || n > json_len - offset) {
      n = json_len - offset;
    }
    memset(&item, 0, sizeof(item));
    status =
        lonejson_array_stream_push(stream, &fuzz_item_map, &item, json + offset,
                                   n, fuzz_push_item, &state, &error);
    lonejson_cleanup(&fuzz_item_map, &item, NULL);
    offset += n;
    if (status != LONEJSON_STATUS_OK) {
      lonejson_array_stream_close(stream);
      lonejson_free(runtime);
      return;
    }
    if (state.items > 16u) {
      break;
    }
  }
  memset(&item, 0, sizeof(item));
  (void)lonejson_array_stream_finish(stream, &fuzz_item_map, &item,
                                     fuzz_push_item, &state, &error);
  lonejson_cleanup(&fuzz_item_map, &item, NULL);
  lonejson_array_stream_close(stream);
  lonejson_free(runtime);
}

static void fuzz_expect_raw_duplicate(const char *json, size_t json_len,
                                      size_t chunk_size) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  fuzz_reader reader;

  config = lonejson_default_config();
  config.reject_duplicate_keys_by_default = 1;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    abort();
  }
  reader.data = (const uint8_t *)json;
  reader.len = json_len;
  reader.offset = 0u;
  reader.chunk_size = chunk_size;
  reader.empty_every = 3u;
  reader.would_block_every = 0u;
  reader.would_blocks = 0u;
  reader.max_would_blocks = 0u;
  reader.read_calls = 0u;
  stream = lonejson_array_stream_open_reader(
      runtime, "", fuzz_array_stream_reader, &reader, &error);
  if (stream == NULL) {
    abort();
  }
  lonejson_json_value_init(runtime, &value);
  result = lonejson_array_stream_next_value(stream, &value, &error);
  lonejson_json_value_cleanup(&value);
  lonejson_array_stream_close(stream);
  lonejson_free(runtime);
  if (result != LONEJSON_ARRAY_STREAM_ERROR ||
      error.code != LONEJSON_STATUS_DUPLICATE_FIELD) {
    abort();
  }
}

static void fuzz_expect_root_duplicate(const char *json, size_t json_len,
                                       size_t chunk_size) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  fuzz_reader reader;
  fuzz_item item;
  size_t count;

  config = lonejson_default_config();
  config.reject_duplicate_keys_by_default = 1;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    abort();
  }
  reader.data = (const uint8_t *)json;
  reader.len = json_len;
  reader.offset = 0u;
  reader.chunk_size = chunk_size;
  reader.empty_every = 4u;
  reader.would_block_every = 0u;
  reader.would_blocks = 0u;
  reader.max_would_blocks = 0u;
  reader.read_calls = 0u;
  stream = lonejson_array_stream_open_reader(
      runtime, "items", fuzz_array_stream_reader, &reader, &error);
  if (stream == NULL) {
    abort();
  }
  for (count = 0u; count < 4u; ++count) {
    memset(&item, 0, sizeof(item));
    result = lonejson_array_stream_next(stream, &fuzz_item_map, &item, &error);
    lonejson_cleanup(&fuzz_item_map, &item, NULL);
    if (result != LONEJSON_ARRAY_STREAM_ITEM) {
      break;
    }
  }
  lonejson_array_stream_close(stream);
  lonejson_free(runtime);
  if (result != LONEJSON_ARRAY_STREAM_ERROR ||
      error.code != LONEJSON_STATUS_DUPLICATE_FIELD) {
    abort();
  }
}

static void fuzz_drive_duplicate_oracles(const uint8_t *data, size_t size) {
  char raw_doc[256];
  char root_before[256];
  char root_after[256];
  unsigned a = size > 1u ? (unsigned)data[1] : 0u;
  unsigned b = size > 2u ? (unsigned)data[2] : 0u;
  unsigned chunk = 1u + (size > 0u ? (unsigned)(data[0] % 17u) : 0u);
  int raw_len;
  int before_len;
  int after_len;

  raw_len = snprintf(raw_doc, sizeof(raw_doc),
                     "[{\"a\":%u,\"\\u0061\":%u,"
                     "\"nested\":{\"a\":%u,\"\\u0061\":%u}}]",
                     a % 997u, b % 997u, (a + 1u) % 997u, (b + 1u) % 997u);
  if (raw_len <= 0 || (size_t)raw_len >= sizeof(raw_doc)) {
    abort();
  }
  fuzz_expect_raw_duplicate(raw_doc, (size_t)raw_len, (size_t)chunk);

  before_len = snprintf(root_before, sizeof(root_before),
                        "{\"meta\":%u,\"\\u006deta\":%u,"
                        "\"items\":[{\"id\":1,\"label\":\"ok\"}]}",
                        a % 997u, b % 997u);
  if (before_len <= 0 || (size_t)before_len >= sizeof(root_before)) {
    abort();
  }
  fuzz_expect_root_duplicate(root_before, (size_t)before_len, (size_t)chunk);

  after_len = snprintf(root_after, sizeof(root_after),
                       "{\"items\":[{\"id\":2,\"label\":\"ok\"}],"
                       "\"tail\":%u,\"\\u0074ail\":%u}",
                       a % 997u, b % 997u);
  if (after_len <= 0 || (size_t)after_len >= sizeof(root_after)) {
    abort();
  }
  fuzz_expect_root_duplicate(root_after, (size_t)after_len, (size_t)chunk);
}

int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  char path[65];
  size_t path_len;
  const uint8_t *json;
  size_t json_len;
  size_t chunk_size;
  size_t would_block_every;
  int reject_duplicate_keys;

  if (size < 2u) {
    return 0;
  }
  path_len = data[0] % sizeof(path);
  if (path_len > size - 1u) {
    path_len = size - 1u;
  }
  memcpy(path, data + 1u, path_len);
  path[path_len] = '\0';
  json = data + 1u + path_len;
  json_len = size - 1u - path_len;
  chunk_size = 1u + (data[0] % 17u);
  would_block_every = (data[0] & 0x40u) ? (1u + (data[0] % 5u)) : 0u;
  reject_duplicate_keys = (data[0] & 0x80u) != 0u;

  fuzz_drive_mapped(path, json, json_len, chunk_size, reject_duplicate_keys,
                    would_block_every);
  fuzz_drive_raw(path, json, json_len, 1u, reject_duplicate_keys,
                 would_block_every);
  fuzz_drive_mapped("", json, json_len, chunk_size, reject_duplicate_keys,
                    would_block_every);
  fuzz_drive_raw("", json, json_len, 1u, reject_duplicate_keys,
                 would_block_every);
  fuzz_drive_push(path, json, json_len, chunk_size, reject_duplicate_keys,
                  (data[0] & 0x20u) ? 1u : 0u);
  fuzz_drive_push("", json, json_len, 1u + ((data[0] >> 2u) % 17u),
                  reject_duplicate_keys, 0u);
  fuzz_drive_duplicate_oracles(data, size);
  return 0;
}
