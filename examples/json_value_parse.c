#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct parsed_envelope {
  char id[16];
  lj_json_value selector;
  lj_json_value fields;
  lj_json_value last_error;
} parsed_envelope;

static const lj_field parsed_envelope_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(parsed_envelope, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_JSON_VALUE(parsed_envelope, selector, "selector"),
    LJ_FIELD_JSON_VALUE(parsed_envelope, fields, "fields"),
    LJ_FIELD_JSON_VALUE(parsed_envelope, last_error, "last_error")};
LJ_MAP_DEFINE(parsed_envelope_map, parsed_envelope, parsed_envelope_fields);

int main(void) {
  static const char inbound_json[] =
      "{\"id\":\"evt-1\","
      "\"selector\":{\"and\":[{\"eq\":{\"field\":\"/"
      "status\",\"value\":\"open\"}},"
      "{\"exists\":{\"field\":\"/meta/etag\"}}]},"
      "\"fields\":{\"include\":[\"/id\",\"/meta/etag\"],\"strict\":true},"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"offset\":17}}}";
  parsed_envelope doc;
  lj *runtime;
  lj_error error;
  lj_status status;
  lj_config config;
  char *pretty;

  config = lj_default_config();
  config.clear_destination_by_default = 0;
  config.write_pretty = 1;
  runtime = lj_new(&config, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(runtime, &parsed_envelope_map, &doc, NULL);
  status = lj_json_value_enable_parse_capture(&doc.selector, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "selector capture setup failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }
  status = lj_json_value_enable_parse_capture(&doc.fields, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "fields capture setup failed: %s\n", error.message);
    lj_cleanup(&parsed_envelope_map, &doc, NULL);
    lj_free(runtime);
    return 1;
  }
  status = lj_json_value_enable_parse_capture(&doc.last_error, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "last_error capture setup failed: %s\n", error.message);
    lj_cleanup(&parsed_envelope_map, &doc, NULL);
    lj_free(runtime);
    return 1;
  }

  status =
      lj_parse_cstr(runtime, &parsed_envelope_map, &doc, inbound_json, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }

  printf("selector compact=%s\n",
         doc.selector.json ? doc.selector.json : "null");
  printf("fields compact=%s\n", doc.fields.json ? doc.fields.json : "null");
  printf("last_error compact=%s\n",
         doc.last_error.json ? doc.last_error.json : "null");

  pretty =
      lj_serialize_alloc(runtime, &parsed_envelope_map, &doc, NULL, &error);
  if (pretty == NULL) {
    fprintf(stderr, "pretty serialize failed: %s\n", error.message);
    lj_cleanup(&parsed_envelope_map, &doc, NULL);
    lj_free(runtime);
    return 1;
  }

  printf("pretty=%s\n", pretty);

  LONEJSON_FREE(pretty);
  lj_cleanup(&parsed_envelope_map, &doc, NULL);
  lj_free(runtime);
  return 0;
}
