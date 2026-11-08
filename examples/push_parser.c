#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct build_info {
  char version[16];
  bool ok;
} build_info;

typedef struct build_reader_state {
  const char *json;
  size_t offset;
} build_reader_state;

static const lonejson_field build_info_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(build_info, version, "version",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BOOL_REQ(build_info, ok, "ok")};
LONEJSON_MAP_DEFINE(build_info_map, build_info, build_info_fields);

static lonejson_read_result build_reader(void *user, unsigned char *buffer,
                                         size_t capacity) {
  build_reader_state *st = (build_reader_state *)user;
  lonejson_read_result rr = lonejson_default_read_result();
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = remaining < 6u ? remaining : 6u;

  if (chunk > capacity) {
    chunk = capacity;
  }
  if (chunk != 0u) {
    memcpy(buffer, st->json + st->offset, chunk);
    st->offset += chunk;
    rr.bytes_read = chunk;
  }
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

int main(void) {
  build_reader_state state = {"{\"version\":\"1.2.3\",\"ok\":true}", 0u};
  lonejson *lj;
  build_info info;
  lonejson_error error;
  lonejson_stream *stream;
  lonejson_stream_result result;

  lj = lonejson_new(NULL, &error);
  if (lj == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lonejson_init(lj, &build_info_map, &info, NULL);

  stream = lonejson_stream_open_reader(lj, &build_info_map, build_reader,
                                       &state, &error);
  if (stream == NULL) {
    fprintf(stderr, "stream open failed: %s\n", error.message);
    lonejson_free(lj);
    return 1;
  }

  result = lonejson_stream_next(stream, &info, &error);
  if (result != LONEJSON_STREAM_OBJECT) {
    fprintf(stderr, "stream parse failed: %s\n", error.message);
    lonejson_stream_close(stream);
    lonejson_free(lj);
    return 1;
  }

  printf("version=%s ok=%s\n", info.version, info.ok ? "true" : "false");
  lonejson_stream_close(stream);
  lonejson_free(lj);
  return 0;
}
