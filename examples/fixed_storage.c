#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct event_log {
  char name[8];
  lj_i64_array codes;
} event_log;

static const lj_field event_log_fields[] = {
    LJ_FIELD_STRING_FIXED(event_log, name, "name", LJ_OVERFLOW_TRUNCATE),
    LJ_FIELD_I64_ARRAY(event_log, codes, "codes", LJ_OVERFLOW_FAIL)};
LJ_MAP_DEFINE(event_log_map, event_log, event_log_fields);

int main(void) {
  const char *json = "{\"name\":\"long-event-name\",\"codes\":[5,8,13]}";
  lj *runtime;
  lj_int64 codes_storage[4] = {0};
  event_log log;
  lj_error error;
  lj_status status;

  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(runtime, &event_log_map, &log, NULL);
  log.codes.items = codes_storage;
  log.codes.capacity = 4;
  log.codes.flags = LJ_ARRAY_FIXED_CAPACITY;

  status = lj_parse_cstr(runtime, &event_log_map, &log, json, &error);
  if (status != LJ_STATUS_OK && status != LJ_STATUS_TRUNCATED) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }

  printf("name=%s codes=%lu first=%ld\n", log.name,
         (unsigned long)log.codes.count, (long)log.codes.items[0]);
  lj_cleanup(&event_log_map, &log, NULL);
  lj_free(runtime);
  return 0;
}
