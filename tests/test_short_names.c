#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LJ_IMPLEMENTATION
#include "lonejson.h"

typedef struct short_doc {
  char id[8];
  lj_source text;
} short_doc;

typedef struct short_parse_doc {
  char name[16];
  lj_int64 age;
  bool active;
} short_parse_doc;

typedef struct short_stream_doc {
  lj_string_array_stream keys;
  lj_uint64 count;
} short_stream_doc;

typedef struct short_stream_seen {
  size_t begins;
  size_t chunks;
  size_t ends;
} short_stream_seen;

static const lj_field short_doc_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(short_doc, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_STRING_SOURCE(short_doc, text, "text")};
LJ_MAP_DEFINE(short_doc_map, short_doc, short_doc_fields);

static const lj_field short_parse_doc_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(short_parse_doc, name, "name", LJ_OVERFLOW_FAIL),
    LJ_FIELD_I64(short_parse_doc, age, "age"),
    LJ_FIELD_BOOL(short_parse_doc, active, "active")};
LJ_MAP_DEFINE(short_parse_doc_map, short_parse_doc, short_parse_doc_fields);

static const lj_field short_stream_doc_fields[] = {
    LJ_FIELD_STRING_ARRAY_STREAM_REQ(short_stream_doc, keys, "keys"),
    LJ_FIELD_U64_REQ(short_stream_doc, count, "count")};
LJ_MAP_DEFINE(short_stream_doc_map, short_stream_doc, short_stream_doc_fields);

static lj_status short_stream_begin(void *user, lj_error *error) {
  short_stream_seen *seen = (short_stream_seen *)user;
  (void)error;
  seen->begins++;
  return LJ_STATUS_OK;
}

static lj_status short_stream_chunk(void *user, const char *data, size_t len,
                                    lj_error *error) {
  short_stream_seen *seen = (short_stream_seen *)user;
  (void)data;
  (void)len;
  (void)error;
  seen->chunks++;
  return LJ_STATUS_OK;
}

static lj_status short_stream_end(void *user, lj_error *error) {
  short_stream_seen *seen = (short_stream_seen *)user;
  (void)error;
  seen->ends++;
  return LJ_STATUS_OK;
}

static lonejson *short_runtime(void) {
  static lonejson *runtime = NULL;

  if (runtime == NULL) {
    runtime = lonejson_new(NULL, NULL);
  }
  return runtime;
}

int main(void) {
  static const char payload[] = "short alias text";
  static const char parse_json[] =
      "{\"name\":\"Alias\",\"age\":42,\"active\":true}";
  lj_string_array_stream macro_string_stream = LJ_STRING_ARRAY_STREAM_INIT;
  lj_mapped_array_stream macro_mapped_stream = LJ_MAPPED_ARRAY_STREAM_INIT;
  short_doc doc;
  short_parse_doc short_record;
  short_parse_doc long_record;
  short_stream_doc stream_doc;
  short_stream_seen stream_seen;
  lj_array_stream_string_handler stream_handler;
  lj_error error;
  lj_status status;
  char path[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-short-XXXXXX";
  int fd;
  char buffer[128];
  char short_json[128];
  char long_json[128];

  lj_init(short_runtime(), &short_doc_map, &doc, NULL);
  lj_init(short_runtime(), &short_parse_doc_map, &short_record, NULL);
  lonejson_init(short_runtime(), &short_parse_doc_map, &long_record, NULL);
  lj_init(short_runtime(), &short_stream_doc_map, &stream_doc, NULL);
  if (macro_string_stream.set_handler !=
          lonejson_string_array_stream_set_handler ||
      macro_mapped_stream.set_handler !=
          lonejson_mapped_array_stream_set_handler) {
    return 1;
  }
  strcpy(doc.id, "ok");

  fd = mkstemp(path);
  if (fd < 0) {
    return 1;
  }
  if (write(fd, payload, sizeof(payload) - 1u) !=
      (ssize_t)(sizeof(payload) - 1u)) {
    close(fd);
    unlink(path);
    return 1;
  }
  close(fd);

  status = lj_source_set_path(&doc.text, path, &error);
  if (status != LJ_STATUS_OK) {
    unlink(path);
    return 1;
  }
  status = lj_serialize_buffer(short_runtime(), &short_doc_map, &doc, buffer,
                               sizeof(buffer), NULL, &error);
  lj_cleanup(&short_doc_map, &doc, NULL);
  unlink(path);
  if (status != LJ_STATUS_OK) {
    return 1;
  }
  if (strstr(buffer, "\"text\":\"short alias text\"") == NULL) {
    return 1;
  }

  status = lj_parse_cstr(short_runtime(), &short_parse_doc_map, &short_record,
                         parse_json, &error);
  if (status != LJ_STATUS_OK) {
    return 1;
  }
  status = lonejson_parse_cstr(short_runtime(), &short_parse_doc_map,
                               &long_record, parse_json, &error);
  if (status != LONEJSON_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record, NULL);
    lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
    return 1;
  }
  if (strcmp(short_record.name, long_record.name) != 0 ||
      short_record.age != long_record.age ||
      short_record.active != long_record.active) {
    lj_cleanup(&short_parse_doc_map, &short_record, NULL);
    lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
    return 1;
  }

  memset(&stream_seen, 0, sizeof(stream_seen));
  memset(&stream_handler, 0, sizeof(stream_handler));
  stream_handler.begin = short_stream_begin;
  stream_handler.chunk = short_stream_chunk;
  stream_handler.end = short_stream_end;
  status = lj_string_array_stream_set_handler(&stream_doc.keys, &stream_handler,
                                              &stream_seen, &error);
  if (status != LJ_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record, NULL);
    lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
    lj_cleanup(&short_stream_doc_map, &stream_doc, NULL);
    return 1;
  }
  status = lj_parse_cstr(short_runtime(), &short_stream_doc_map, &stream_doc,
                         "{\"keys\":[\"a\",\"b\"],\"count\":2}", &error);
  if (status != LJ_STATUS_OK || stream_seen.begins != 2u ||
      stream_seen.ends != 2u || stream_seen.chunks == 0u ||
      stream_doc.count != 2u) {
    lj_cleanup(&short_parse_doc_map, &short_record, NULL);
    lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
    lj_cleanup(&short_stream_doc_map, &stream_doc, NULL);
    return 1;
  }

  status =
      lj_serialize_buffer(short_runtime(), &short_parse_doc_map, &short_record,
                          short_json, sizeof(short_json), NULL, &error);
  if (status != LJ_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record, NULL);
    lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
    return 1;
  }
  status = lonejson_serialize_buffer(short_runtime(), &short_parse_doc_map,
                                     &long_record, long_json, sizeof(long_json),
                                     NULL, &error);
  if (status != LONEJSON_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record, NULL);
    lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
    return 1;
  }
  lj_cleanup(&short_parse_doc_map, &short_record, NULL);
  lonejson_cleanup(&short_parse_doc_map, &long_record, NULL);
  lj_cleanup(&short_stream_doc_map, &stream_doc, NULL);
  return strcmp(short_json, long_json) == 0 ? 0 : 1;
}
