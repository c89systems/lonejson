#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct query_request {
  char namespace_[16];
  lj_json_value selector;
  lj_json_value fields;
  lj_json_value last_error;
} query_request;

static const lj_field query_request_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(query_request, namespace_, "namespace",
                              LJ_OVERFLOW_FAIL),
    LJ_FIELD_JSON_VALUE_REQ(query_request, selector, "selector"),
    LJ_FIELD_JSON_VALUE(query_request, fields, "fields"),
    LJ_FIELD_JSON_VALUE(query_request, last_error, "last_error")};
LJ_MAP_DEFINE(query_request_map, query_request, query_request_fields);

int main(void) {
  static const char selector_json[] =
      "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}";
  static const char fields_json[] = "[\"id\",\"name\",\"created_at\"]";
  lj *runtime;
  query_request req;
  lj_error error;
  lj_status status;
  char *json;

  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(runtime, &query_request_map, &req, NULL);
  strcpy(req.namespace_, "default");

  status = lj_json_value_set_buffer(&req.selector, selector_json,
                                    strlen(selector_json), &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "selector setup failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }
  status = lj_json_value_set_buffer(&req.fields, fields_json,
                                    strlen(fields_json), &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "fields setup failed: %s\n", error.message);
    lj_cleanup(&query_request_map, &req, NULL);
    lj_free(runtime);
    return 1;
  }

  json = lj_serialize_alloc(runtime, &query_request_map, &req, NULL, &error);
  if (json == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    lj_cleanup(&query_request_map, &req, NULL);
    lj_free(runtime);
    return 1;
  }

  printf("json=%s\n", json);

  LONEJSON_FREE(json);
  lj_cleanup(&query_request_map, &req, NULL);
  lj_free(runtime);
  return 0;
}
