#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct consumer_person {
  char name[16];
  lonejson_int64 age;
  bool active;
} consumer_person;

typedef struct consumer_log {
  consumer_person person;
  lonejson_string_array tags;
} consumer_log;

typedef struct consumer_sink {
  char *buffer;
  size_t len;
  size_t capacity;
} consumer_sink;

typedef struct consumer_response_json {
  char *response_json;
} consumer_response_json;

typedef struct consumer_rewrite_item {
  char *name;
} consumer_rewrite_item;

typedef struct consumer_rewrite_counter {
  size_t count;
} consumer_rewrite_counter;

static const lonejson_field consumer_person_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(consumer_person, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(consumer_person, age, "age"),
    LONEJSON_FIELD_BOOL_REQ(consumer_person, active, "active")};
LONEJSON_MAP_DEFINE(consumer_person_map, consumer_person,
                    consumer_person_fields);

static const lonejson_field consumer_log_fields[] = {
    LONEJSON_FIELD_OBJECT_REQ(consumer_log, person, "person",
                              &consumer_person_map),
    LONEJSON_FIELD_STRING_ARRAY(consumer_log, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(consumer_log_map, consumer_log, consumer_log_fields);

static const lonejson_field consumer_response_json_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(consumer_response_json, response_json,
                                    "response_json")};
LONEJSON_MAP_DEFINE(consumer_response_json_map, consumer_response_json,
                    consumer_response_json_fields);

static const lonejson_field consumer_rewrite_item_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(consumer_rewrite_item, name, "name")};
LONEJSON_MAP_DEFINE(consumer_rewrite_item_map, consumer_rewrite_item,
                    consumer_rewrite_item_fields);

static lonejson *consumer_runtime(void) {
  static lonejson *runtime = NULL;

  if (runtime == NULL) {
    lonejson_config config = lonejson_default_config();
    config.write_pretty = 1;
    runtime = lonejson_new(&config, NULL);
  }
  return runtime;
}

static lonejson_status consumer_sink_write(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  consumer_sink *sink;

  (void)error;
  sink = (consumer_sink *)user;
  if (sink->len + len + 1u > sink->capacity) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(sink->buffer + sink->len, data, len);
  sink->len += len;
  sink->buffer[sink->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status consumer_rewrite_item_cb(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  consumer_rewrite_counter *counter = (consumer_rewrite_counter *)user;
  consumer_rewrite_item *rewrite_item = (consumer_rewrite_item *)item;

  (void)context;
  (void)result;
  (void)error;
  if (rewrite_item == NULL || rewrite_item->name == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (counter->count == 0u && strcmp(rewrite_item->name, "ok") != 0) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (counter->count == 1u && strcmp(rewrite_item->name, "go") != 0) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  counter->count++;
  return LONEJSON_STATUS_OK;
}

typedef struct consumer_protocol_observation {
  char data[16];
  size_t len;
  int count;
  int cancel;
} consumer_protocol_observation;

static lonejson_status consumer_protocol_bytes(void *user, const void *bytes,
                                               size_t len,
                                               lonejson_error *error) {
  consumer_protocol_observation *seen = (consumer_protocol_observation *)user;
  (void)error;
  if (seen->cancel)
    return LONEJSON_STATUS_IO_ERROR;
  if (len >= sizeof(seen->data) - seen->len)
    return LONEJSON_STATUS_OVERFLOW;
  memcpy(seen->data + seen->len, bytes, len);
  seen->len += len;
  seen->data[seen->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status consumer_protocol_mapped(void *user,
                                                const lonejson_sse_event *event,
                                                void *dst,
                                                lonejson_error *error) {
  consumer_protocol_observation *seen = (consumer_protocol_observation *)user;
  consumer_person *person = (consumer_person *)dst;
  (void)event;
  (void)error;
  ++seen->count;
  return strcmp(person->name, "Ada") == 0 && person->age == 7 && person->active
             ? LONEJSON_STATUS_OK
             : LONEJSON_STATUS_TYPE_MISMATCH;
}

static lonejson_status consumer_protocol_value(void *user,
                                               const lonejson_sse_event *event,
                                               lonejson_json_value *value,
                                               lonejson_error *error) {
  consumer_protocol_observation *seen = (consumer_protocol_observation *)user;
  (void)event;
  ++seen->count;
  return lonejson_json_value_write_to_sink(value, consumer_protocol_bytes, user,
                                           error);
}

static int consumer_protocol_receivers(void) {
  static const char raw[] = "data: abc\n\n";
  static const char mapped[] =
      "data: {\"name\":\"Ada\",\"age\":7,\"active\":true}";
  static const char part[] = "--b\r\nContent-Length: 3\r\n\r\nabc\r\n--b--\r\n";
  lonejson_sse *sse;
  lonejson_multipart *multipart;
  lonejson_sse_handler sse_handler;
  lonejson_multipart_handler part_handler;
  lonejson_json_value *value;
  lonejson_error error;
  consumer_protocol_observation seen;
  consumer_person person;
  size_t i;
  int valid = 1;
#define CONSUMER_PROTOCOL_CHECK(condition)                                     \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "protocol receiver check failed at line %d: %s\n",       \
              __LINE__, #condition);                                           \
      valid = 0;                                                               \
    }                                                                          \
  } while (0)
  memset(&seen, 0, sizeof(seen));
  memset(&sse_handler, 0, sizeof(sse_handler));
  sse_handler.data_chunk = consumer_protocol_bytes;
  sse = lonejson_sse_open(NULL, &error);
  CONSUMER_PROTOCOL_CHECK(sse != NULL);
  if (sse != NULL) {
    for (i = 0u; i < sizeof(raw) - 2u; ++i) {
      CONSUMER_PROTOCOL_CHECK(sse->push(sse, raw + i, 1u, &sse_handler, &seen,
                                        &error) == LONEJSON_STATUS_OK);
    }
    CONSUMER_PROTOCOL_CHECK(strcmp(seen.data, "abc") == 0);
    CONSUMER_PROTOCOL_CHECK(sse->finish(sse, &sse_handler, &seen, &error) ==
                            LONEJSON_STATUS_OK);
    CONSUMER_PROTOCOL_CHECK(
        sse->push(sse, raw, 1u, &sse_handler, &seen, &error) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
    sse->close(sse);
  }
  seen.cancel = 1;
  sse = lonejson_sse_open(NULL, &error);
  CONSUMER_PROTOCOL_CHECK(sse != NULL);
  if (sse != NULL) {
    CONSUMER_PROTOCOL_CHECK(
        sse->push(sse, raw, sizeof(raw) - 1u, &sse_handler, &seen, &error) ==
        LONEJSON_STATUS_CALLBACK_FAILED);
    sse->close(sse);
  }
  memset(&seen, 0, sizeof(seen));
  lonejson_init(consumer_runtime(), &consumer_person_map, &person);
  sse = lonejson_sse_open(NULL, &error);
  CONSUMER_PROTOCOL_CHECK(sse != NULL);
  if (sse != NULL) {
    for (i = 0u; i < sizeof(mapped) - 1u; ++i) {
      CONSUMER_PROTOCOL_CHECK(
          sse->push_json(sse, consumer_runtime(), &consumer_person_map, &person,
                         mapped + i, 1u, NULL, consumer_protocol_mapped, &seen,
                         &error) == LONEJSON_STATUS_OK);
    }
    CONSUMER_PROTOCOL_CHECK(seen.count == 0);
    CONSUMER_PROTOCOL_CHECK(
        sse->finish_json(sse, consumer_runtime(), &consumer_person_map, &person,
                         NULL, consumer_protocol_mapped, &seen,
                         &error) == LONEJSON_STATUS_OK);
    CONSUMER_PROTOCOL_CHECK(seen.count == 1);
    sse->close(sse);
  }
  lonejson_cleanup(&consumer_person_map, &person);
  memset(&seen, 0, sizeof(seen));
  value = (lonejson_json_value *)malloc(sizeof(*value));
  CONSUMER_PROTOCOL_CHECK(value != NULL);
  if (value != NULL) {
    lonejson_json_value_init(consumer_runtime(), value);
    CONSUMER_PROTOCOL_CHECK(lonejson_json_value_enable_parse_capture(
                                value, &error) == LONEJSON_STATUS_OK);
    sse = lonejson_sse_open(NULL, &error);
    CONSUMER_PROTOCOL_CHECK(sse != NULL);
    if (sse != NULL) {
      CONSUMER_PROTOCOL_CHECK(
          sse->push_json_value(sse, consumer_runtime(), value, "data: true",
                               10u, NULL, consumer_protocol_value, &seen,
                               &error) == LONEJSON_STATUS_OK);
      CONSUMER_PROTOCOL_CHECK(
          sse->finish_json_value(sse, consumer_runtime(), value, NULL,
                                 consumer_protocol_value, &seen,
                                 &error) == LONEJSON_STATUS_OK);
      CONSUMER_PROTOCOL_CHECK(seen.count == 1 &&
                              strcmp(seen.data, "true") == 0);
      sse->close(sse);
    }
    lonejson_json_value_cleanup(value);
    free(value);
  }
  memset(&part_handler, 0, sizeof(part_handler));
  part_handler.part_data = consumer_protocol_bytes;
  for (i = 0u; i < 2u; ++i) {
    size_t offset;
    size_t prefix = (size_t)(strstr(part, "abc") + 3 - part);
    memset(&seen, 0, sizeof(seen));
    seen.cancel = (int)i;
    multipart =
        lonejson_multipart_open("multipart/mixed; boundary=b", NULL, &error);
    CONSUMER_PROTOCOL_CHECK(multipart != NULL);
    if (multipart != NULL) {
      if (seen.cancel) {
        CONSUMER_PROTOCOL_CHECK(
            multipart->push(multipart, part, sizeof(part) - 1u, &part_handler,
                            &seen, &error) == LONEJSON_STATUS_CALLBACK_FAILED);
      } else {
        for (offset = 0u; offset < sizeof(part) - 1u; ++offset) {
          CONSUMER_PROTOCOL_CHECK(
              multipart->push(multipart, part + offset, 1u, &part_handler,
                              &seen, &error) == LONEJSON_STATUS_OK);
          if (offset + 1u == prefix)
            CONSUMER_PROTOCOL_CHECK(strcmp(seen.data, "abc") == 0);
        }
        CONSUMER_PROTOCOL_CHECK(
            multipart->finish(multipart, &part_handler, &seen, &error) ==
            LONEJSON_STATUS_OK);
        CONSUMER_PROTOCOL_CHECK(multipart->push(multipart, NULL, 0u,
                                                &part_handler, &seen, &error) ==
                                LONEJSON_STATUS_INVALID_ARGUMENT);
      }
      multipart->close(multipart);
    }
  }
  multipart =
      lonejson_multipart_open("multipart/mixed; boundary=b", NULL, &error);
  CONSUMER_PROTOCOL_CHECK(multipart != NULL);
  if (multipart != NULL) {
    CONSUMER_PROTOCOL_CHECK(multipart->finish(multipart, NULL, NULL, &error) ==
                            LONEJSON_STATUS_INVALID_JSON);
    multipart->close(multipart);
  }
#undef CONSUMER_PROTOCOL_CHECK
  return valid ? 0 : 1;
}

int main(void) {
  static const char json[] =
      "{\"person\":{\"name\":\"Alice\",\"age\":37,\"active\":true},"
      "\"tags\":[\"prod\",\"edge\"]}";
  static const char response_json[] = "{\"response_json\":\"created\"}";
  static const char rewrite_json[] =
      "{\"items\":[{\"name\":\"ok\"},{\"name\":\"go\"}]}";
  consumer_log log_record;
  consumer_response_json response;
  consumer_rewrite_item rewrite_item;
  consumer_rewrite_counter rewrite_counter;
  lonejson_string_array_stream string_stream =
      LONEJSON_STRING_ARRAY_STREAM_INIT;
  lonejson_mapped_array_stream mapped_stream =
      LONEJSON_MAPPED_ARRAY_STREAM_INIT;
  lonejson_array_rewrite_options rewrite_options;
  lonejson_json_value value;
  lonejson_spooled spool;
  lonejson_buffer_reader reader;
  lonejson_read_result rr;
  lonejson_read_result normalized;
  consumer_sink sink;
  lonejson_error error;
  lonejson_status status;
  char buffer[256];
  char value_buffer[16];
  unsigned char read_buffer[16];

  lonejson_init(consumer_runtime(), &consumer_log_map, &log_record);
  lonejson_error_init(&error);
  if (consumer_protocol_receivers() != 0)
    return 1;
  if (lonejson_array_stream_set_record_reuse(NULL, 1, &error) !=
      LONEJSON_STATUS_INVALID_ARGUMENT) {
    return 1;
  }
  {
    lonejson_array_stream *stream = consumer_runtime()->array_stream_open_push(
        consumer_runtime(), "", &error);
    if (stream == NULL) {
      return 1;
    }
    status = stream->set_record_reuse(stream, 1, &error);
    if (status != LONEJSON_STATUS_OK ||
        stream->error_fn(stream)->code != LONEJSON_STATUS_OK) {
      stream->close(stream);
      return 1;
    }
    stream->close(stream);
  }

  status = lonejson_parse_cstr(consumer_runtime(), &consumer_log_map,
                               &log_record, json, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  if (strcmp(log_record.person.name, "Alice") != 0 ||
      log_record.person.age != 37 || !log_record.person.active ||
      log_record.tags.count != 2u ||
      strcmp(log_record.tags.items[0], "prod") != 0 ||
      strcmp(log_record.tags.items[1], "edge") != 0) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }
  if (string_stream.set_handler == NULL || mapped_stream.set_handler == NULL ||
      consumer_runtime()->cleanup == NULL ||
      consumer_runtime()->serialize_sink == NULL ||
      consumer_runtime()->serialize_buffer == NULL) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }

  memset(&sink, 0, sizeof(sink));
  memset(buffer, 0, sizeof(buffer));
  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  status = consumer_runtime()->serialize_sink(
      consumer_runtime(), &consumer_log_map, &log_record, consumer_sink_write,
      &sink, &error);
  if (status != LONEJSON_STATUS_OK) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }
  if (strstr(buffer, "\"name\": \"Alice\"") == NULL ||
      strstr(buffer, "\"tags\": [") == NULL) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }

  status = consumer_runtime()->serialize_buffer(
      consumer_runtime(), &consumer_log_map, &log_record, buffer,
      sizeof(buffer), NULL, &error);
  consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                              &log_record);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  if (strstr(buffer, "\"name\": \"Alice\"") == NULL ||
      strstr(buffer, "\"tags\": [") == NULL) {
    return 1;
  }

  consumer_runtime()->json_value_init(consumer_runtime(), &value);
  if (value.methods == NULL || value.methods->set_buffer == NULL ||
      value.methods->write_to_sink == NULL || value.methods->cleanup == NULL) {
    return 1;
  }
  status = value.methods->set_buffer(&value, "true", 4u, &error);
  if (status != LONEJSON_STATUS_OK) {
    value.methods->cleanup(&value);
    return 1;
  }
  memset(value_buffer, 0, sizeof(value_buffer));
  sink.buffer = value_buffer;
  sink.len = 0u;
  sink.capacity = sizeof(value_buffer);
  status =
      value.methods->write_to_sink(&value, consumer_sink_write, &sink, &error);
  value.methods->cleanup(&value);
  if (status != LONEJSON_STATUS_OK || strcmp(value_buffer, "true") != 0) {
    return 1;
  }

  memset(&response, 0, sizeof(response));
  status = lonejson_parse_cstr(consumer_runtime(), &consumer_response_json_map,
                               &response, response_json, &error);
  if (status != LONEJSON_STATUS_OK || response.response_json == NULL ||
      strcmp(response.response_json, "created") != 0) {
    lonejson_cleanup(&consumer_response_json_map, &response);
    return 1;
  }
  lonejson_cleanup(&consumer_response_json_map, &response);

  response.response_json = value_buffer;
  value_buffer[0] = '\0';
  {
    char *allocated = lonejson_serialize_alloc(consumer_runtime(),
                                               &consumer_response_json_map,
                                               &response, NULL, &error);
    if (allocated == NULL || strstr(allocated, "\"response_json\"") == NULL ||
        strstr(allocated, "\"\"") == NULL) {
      free(allocated);
      return 1;
    }
    free(allocated);
  }

  memset(&rewrite_options, 0, sizeof(rewrite_options));
  rewrite_counter.count = 0u;
  rewrite_item.name = NULL;
  rewrite_options.item_map = &consumer_rewrite_item_map;
  rewrite_options.item_dst = &rewrite_item;
  rewrite_options.item = consumer_rewrite_item_cb;
  rewrite_options.user = &rewrite_counter;
  memset(&sink, 0, sizeof(sink));
  memset(buffer, 0, sizeof(buffer));
  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  lonejson_buffer_reader_init(&reader, rewrite_json, strlen(rewrite_json));
  status = lonejson_array_rewrite_reader(
      consumer_runtime(), "items", lonejson_buffer_reader_read, &reader,
      consumer_sink_write, &sink, &rewrite_options, &error);
  if (status != LONEJSON_STATUS_OK || rewrite_counter.count != 2u ||
      strstr(buffer, "\"items\":[{\"name\":\"ok\"},{\"name\":\"go\"}]") ==
          NULL) {
    return 1;
  }

  lonejson_spooled_init(consumer_runtime(), &spool);
  status = lonejson_spooled_append(&spool, "{}", 2u, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  status = lonejson_spooled_rewind(&spool, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  rr = lonejson_spooled_read(&spool, read_buffer, sizeof(read_buffer));
  if (rr.bytes_read > sizeof(read_buffer)) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  normalized = lonejson_default_read_result();
  normalized.bytes_read = rr.bytes_read;
  normalized.eof = rr.eof;
  normalized.would_block = rr.would_block;
  normalized.error_code = rr.error_code;
  normalized.reserved = rr.reserved;
  if (normalized.bytes_read != 2u) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  lonejson_spooled_cleanup(&spool);

  return 0;
}
