#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct metric_sample {
  char *source;
  double value;
} metric_sample;

typedef struct chunk_reader_state {
  const char *json;
  size_t offset;
} chunk_reader_state;

static const lj_field metric_sample_fields[] = {
    LJ_FIELD_STRING_ALLOC_REQ(metric_sample, source, "source"),
    LJ_FIELD_F64_REQ(metric_sample, value, "value")};
LJ_MAP_DEFINE(metric_sample_map, metric_sample, metric_sample_fields);

static lj_read_result chunk_reader(void *user, unsigned char *buffer,
                                   size_t capacity) {
  chunk_reader_state *state = (chunk_reader_state *)user;
  lj_read_result result = lj_default_read_result();
  size_t remaining = strlen(state->json) - state->offset;
  size_t chunk = remaining < 5u ? remaining : 5u;

  (void)capacity;
  if (chunk != 0u) {
    memcpy(buffer, state->json + state->offset, chunk);
    state->offset += chunk;
    result.bytes_read = chunk;
  }
  result.eof = (state->json[state->offset] == '\0') ? 1 : 0;
  return result;
}

int main(void) {
  chunk_reader_state state = {"{\"source\":\"sensor-a\",\"value\":12.75}", 0u};
  lj *runtime;
  metric_sample sample;
  lj_error error;
  lj_stream *stream;
  lj_stream_result result;

  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(runtime, &metric_sample_map, &sample, NULL);

  stream = lj_stream_open_reader(runtime, &metric_sample_map, chunk_reader,
                                 &state, &error);
  if (stream == NULL) {
    fprintf(stderr, "stream open failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }

  result = lj_stream_next(stream, &sample, &error);
  if (result != LJ_STREAM_OBJECT) {
    fprintf(stderr, "stream parse failed: %s\n", error.message);
    lj_stream_close(stream);
    lj_free(runtime);
    return 1;
  }

  printf("source=%s value=%.2f\n", sample.source, sample.value);
  lj_cleanup(&metric_sample_map, &sample, NULL);
  lj_stream_close(stream);
  lj_free(runtime);
  return 0;
}
