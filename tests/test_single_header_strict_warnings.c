#include <string.h>

#define LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL
#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

typedef struct strict_item {
  char id[8];
} strict_item;

typedef struct strict_reader_state {
  const char *json;
  size_t offset;
} strict_reader_state;

static const lonejson_field strict_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(strict_item, id, "id",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(strict_item_map, strict_item, strict_item_fields);

static lonejson_read_result
strict_array_reader(void *user, unsigned char *buffer, size_t capacity) {
  strict_reader_state *state;
  lonejson_read_result result;
  size_t remaining;
  size_t copy_len;

  state = (strict_reader_state *)user;
  result = lonejson_default_read_result();
  remaining = strlen(state->json + state->offset);
  if (remaining == 0u) {
    result.eof = 1;
    return result;
  }
  copy_len = remaining < capacity ? remaining : capacity;
  memcpy(buffer, state->json + state->offset, copy_len);
  state->offset += copy_len;
  result.bytes_read = copy_len;
  return result;
}

int main(void) {
  strict_reader_state state;
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  strict_item item;

  state.json = "[{\"id\":\"ok\"}]";
  state.offset = 0u;
  lonejson_error_init(&error);
  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 1;
  }
  stream = lonejson_array_stream_open_reader(runtime, "", strict_array_reader,
                                             &state, &error);
  if (stream == NULL) {
    lonejson_free(runtime);
    return 1;
  }
  lonejson_init(runtime, &strict_item_map, &item, NULL);
  result = lonejson_array_stream_next(stream, &strict_item_map, &item, &error);
  if (result != LONEJSON_ARRAY_STREAM_ITEM || item.id[0] != 'o' ||
      item.id[1] != 'k' || item.id[2] != '\0') {
    lonejson_cleanup(&strict_item_map, &item, NULL);
    lonejson_array_stream_close(stream);
    lonejson_free(runtime);
    return 1;
  }
  result = lonejson_array_stream_next(stream, &strict_item_map, &item, &error);
  lonejson_cleanup(&strict_item_map, &item, NULL);
  lonejson_array_stream_close(stream);
  lonejson_free(runtime);
  return result == LONEJSON_ARRAY_STREAM_EOF ? 0 : 1;
}
