#include <stdio.h>

#include "lonejson.h"

typedef struct issue_event {
  char id[24];
  char title[96];
  lonejson_int64 priority;
  bool open;
} issue_event;

static const lonejson_field issue_event_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(issue_event, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(issue_event, title, "title",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(issue_event, priority, "priority"),
    LONEJSON_FIELD_BOOL_REQ(issue_event, open, "open")};
LONEJSON_MAP_DEFINE(issue_event_map, issue_event, issue_event_fields);

int main(int argc, char **argv) {
  const char *path =
      argc > 1 ? argv[1] : "tests/fixtures/array_stream/issues.json";
  lonejson *lj;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  issue_event event;
  size_t count = 0u;
  lonejson_int64 priority_total = 0;

  lj = lonejson_new(NULL, &error);
  if (lj == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  stream = lonejson_array_stream_open_path(lj, "items", path, &error);
  if (stream == NULL) {
    fprintf(stderr, "array stream open failed: %s\n", error.message);
    lonejson_free(lj);
    return 1;
  }

  for (;;) {
    lonejson_init(lj, &issue_event_map, &event, NULL);
    result =
        lonejson_array_stream_next(stream, &issue_event_map, &event, &error);
    if (result == LONEJSON_ARRAY_STREAM_ITEM) {
      ++count;
      priority_total += event.priority;
      printf("%s %s priority=%ld title=%s\n", event.id,
             event.open ? "open" : "closed", (long)event.priority, event.title);
      lonejson_cleanup(&issue_event_map, &event, NULL);
      continue;
    }
    lonejson_cleanup(&issue_event_map, &event, NULL);
    break;
  }

  if (result != LONEJSON_ARRAY_STREAM_EOF) {
    fprintf(stderr, "array stream parse failed: %s\n", error.message);
    lonejson_array_stream_close(stream);
    lonejson_free(lj);
    return 1;
  }

  lonejson_array_stream_close(stream);
  lonejson_free(lj);
  printf("streamed %lu items priority_total=%ld\n", (unsigned long)count,
         (long)priority_total);
  return 0;
}
