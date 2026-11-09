#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct query_envelope {
  char id[16];
  lj_json_value selector;
  lj_json_value fields;
} query_envelope;

typedef struct visit_log {
  char text[2048];
  size_t len;
} visit_log;

static const lj_field query_envelope_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(query_envelope, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_JSON_VALUE_REQ(query_envelope, selector, "selector"),
    LJ_FIELD_JSON_VALUE(query_envelope, fields, "fields")};
LJ_MAP_DEFINE(query_envelope_map, query_envelope, query_envelope_fields);

static lj_status visit_append(visit_log *log, const char *text) {
  size_t len = strlen(text);

  if (log->len + len + 1u >= sizeof(log->text)) {
    return LJ_STATUS_OVERFLOW;
  }
  memcpy(log->text + log->len, text, len);
  log->len += len;
  log->text[log->len] = '\0';
  return LJ_STATUS_OK;
}

static lj_status on_object_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "{");
}

static lj_status on_object_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "}");
}

static lj_status on_array_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "[");
}

static lj_status on_array_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "]");
}

static lj_status on_key_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "K(");
}

static lj_status on_string_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "S(");
}

static lj_status on_chunk(void *user, const char *data, size_t len,
                          lj_error *error) {
  visit_log *log = (visit_log *)user;
  (void)error;

  if (log->len + len + 1u >= sizeof(log->text)) {
    return LJ_STATUS_OVERFLOW;
  }
  memcpy(log->text + log->len, data, len);
  log->len += len;
  log->text[log->len] = '\0';
  return LJ_STATUS_OK;
}

static lj_status on_key_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, ")");
}

static lj_status on_string_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, ")");
}

static lj_status on_number_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "N(");
}

static lj_status on_number_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, ")");
}

static lj_status on_boolean(void *user, int value, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, value ? "T" : "F");
}

static lj_status on_null(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "Z");
}

int main(void) {
  static const char inbound_json[] =
      "{\"id\":\"evt-2\","
      "\"selector\":{\"op\":\"and\",\"clauses\":[{\"field\":\"/status\","
      "\"value\":\"open\"},{\"field\":\"/priority\",\"gte\":3}]},"
      "\"fields\":[\"/id\",{\"include\":[true,null,\"wide-string\"]}]}";
  lonejson *lj;
  query_envelope doc;
  visit_log selector_log;
  visit_log fields_log;
  lj_value_visitor visitor;
  lj_config config;
  lj_error error;
  lj_status status;

  config = lj_default_config();
  config.clear_destination_by_default = 0;
  lj = lj_new(&config, &error);
  if (lj == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(lj, &query_envelope_map, &doc, NULL);
  memset(&selector_log, 0, sizeof(selector_log));
  memset(&fields_log, 0, sizeof(fields_log));
  visitor = lj_default_value_visitor();
  visitor.object_begin = on_object_begin;
  visitor.object_end = on_object_end;
  visitor.object_key_begin = on_key_begin;
  visitor.object_key_chunk = on_chunk;
  visitor.object_key_end = on_key_end;
  visitor.array_begin = on_array_begin;
  visitor.array_end = on_array_end;
  visitor.string_begin = on_string_begin;
  visitor.string_chunk = on_chunk;
  visitor.string_end = on_string_end;
  visitor.number_begin = on_number_begin;
  visitor.number_chunk = on_chunk;
  visitor.number_end = on_number_end;
  visitor.boolean_value = on_boolean;
  visitor.null_value = on_null;

  status = lj_json_value_set_parse_visitor(&doc.selector, &visitor,
                                           &selector_log, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "selector visitor setup failed: %s\n", error.message);
    lj_free(lj);
    return 1;
  }
  status = lj_json_value_set_parse_visitor(&doc.fields, &visitor, &fields_log,
                                           &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "fields visitor setup failed: %s\n", error.message);
    lj_cleanup(&query_envelope_map, &doc, NULL);
    lj_free(lj);
    return 1;
  }

  status = lj_parse_cstr(lj, &query_envelope_map, &doc, inbound_json, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    lj_cleanup(&query_envelope_map, &doc, NULL);
    lj_free(lj);
    return 1;
  }

  printf("selector events=%s\n", selector_log.text);
  printf("fields events=%s\n", fields_log.text);

  lj_cleanup(&query_envelope_map, &doc, NULL);
  lj_free(lj);
  return 0;
}
