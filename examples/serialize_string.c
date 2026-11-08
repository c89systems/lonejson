#include <stdio.h>
#include <stdlib.h>

#include "lonejson.h"

typedef struct api_response {
  char *message;
  lonejson_int64 code;
  bool retry;
} api_response;

static const lonejson_field api_response_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(api_response, message, "message"),
    LONEJSON_FIELD_I64(api_response, code, "code"),
    LONEJSON_FIELD_BOOL(api_response, retry, "retry")};
LONEJSON_MAP_DEFINE(api_response_map, api_response, api_response_fields);

int main(void) {
  lonejson *lj;
  api_response response;
  lonejson_error error;
  char *json;

  lj = lonejson_new(NULL, &error);
  if (lj == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lonejson_init(lj, &api_response_map, &response, NULL);
  response.message = "created";
  response.code = 201;
  response.retry = false;

  json =
      lonejson_serialize_alloc(lj, &api_response_map, &response, NULL, &error);
  if (json == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    lonejson_free(lj);
    return 1;
  }

  puts(json);
  LONEJSON_FREE(json);
  lonejson_free(lj);
  return 0;
}
