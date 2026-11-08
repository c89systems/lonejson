#define _POSIX_C_SOURCE 200809L
#include "lonejson.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);          \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

typedef struct allocation_watch {
  size_t live;
  size_t calls;
} allocation_watch;

static void *watch_malloc(void *user, size_t size) {
  allocation_watch *watch = (allocation_watch *)user;
  void *ptr = malloc(size);
  if (ptr != NULL) {
    ++watch->live;
    ++watch->calls;
  }
  return ptr;
}
static void *watch_realloc(void *user, void *ptr, size_t size) {
  allocation_watch *watch = (allocation_watch *)user;
  int was_null = ptr == NULL;
  void *next = realloc(ptr, size);
  if (next != NULL) {
    if (was_null)
      ++watch->live;
    ++watch->calls;
  }
  return next;
}
static void watch_free(void *user, void *ptr) {
  allocation_watch *watch = (allocation_watch *)user;
  if (ptr != NULL) {
    CHECK(watch->live != 0u);
    --watch->live;
    free(ptr);
  }
}

typedef struct record {
  lonejson_i64_array values;
  char *name;
  lonejson_int64 marker;
  lonejson_string_array_stream words;
} record;
static const lonejson_field fields[] = {
    LONEJSON_FIELD_I64_ARRAY(record, values, "values", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ALLOC(record, name, "name"),
    LONEJSON_FIELD_I64(record, marker, "marker"),
    LONEJSON_FIELD_STRING_ARRAY_STREAM(record, words, "words")};
LONEJSON_MAP_DEFINE(record_map, record, fields);

typedef struct reader_state {
  const char *json;
  size_t offset;
  int block;
} reader_state;
static lonejson_read_result read_byte(void *user, unsigned char *dst,
                                      size_t cap) {
  reader_state *reader = (reader_state *)user;
  lonejson_read_result result;
  memset(&result, 0, sizeof(result));
  if (reader->block) {
    reader->block = 0;
    result.would_block = 1;
  } else if (reader->json[reader->offset] == '\0')
    result.eof = 1;
  else if (cap != 0u) {
    dst[0] = (unsigned char)reader->json[reader->offset++];
    result.bytes_read = 1u;
  }
  return result;
}
typedef struct words_seen {
  char text[16];
  size_t size;
} words_seen;
static lonejson_status word_chunk(void *user, const char *bytes, size_t len,
                                  lonejson_error *error) {
  words_seen *seen = (words_seen *)user;
  (void)error;
  CHECK(len < sizeof(seen->text) - seen->size);
  if (len >= sizeof(seen->text) - seen->size)
    return LONEJSON_STATUS_OVERFLOW;
  memcpy(seen->text + seen->size, bytes, len);
  seen->size += len;
  seen->text[seen->size] = '\0';
  return LONEJSON_STATUS_OK;
}
static void prepare(lonejson *runtime, record *doc, lonejson_int64 *backing,
                    words_seen *words) {
  lonejson_array_stream_string_handler handler;
  lonejson_error error;
  lonejson_init(runtime, &record_map, doc);
  doc->values.items = backing;
  doc->values.capacity = 1u;
  doc->values.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  memset(&handler, 0, sizeof(handler));
  handler.chunk = word_chunk;
  CHECK(lonejson_string_array_stream_set_handler(&doc->words, &handler, words,
                                                 &error) == LONEJSON_STATUS_OK);
}
static void check_backing(const record *doc, const lonejson_int64 *backing) {
  CHECK(doc->values.items == backing);
  CHECK(doc->values.capacity == 1u);
  CHECK(doc->values.flags == LONEJSON_ARRAY_FIXED_CAPACITY);
}
static const char document[] =
    "{\"items\":[{\"name\":\"one\",\"marker\":9,\"values\":[7],\"words\":["
    "\"ab\"]},"
    "{},{\"name\":\"two\",\"values\":[8],\"words\":[\"c\"]}]}";
static void check_item(const record *doc, size_t index) {
  if (index == 1u) {
    CHECK(doc->name == NULL && doc->values.count == 0u && doc->marker == 0);
  } else {
    CHECK(doc->name != NULL);
    if (doc->name != NULL)
      CHECK(strcmp(doc->name, index == 0u ? "one" : "two") == 0);
    CHECK(doc->values.count == 1u);
    if (doc->values.count == 1u)
      CHECK(doc->values.items[0] == (index == 0u ? 7 : 8));
    CHECK(doc->marker == (index == 0u ? 9 : 0));
  }
}

static void test_pull_sources(lonejson *runtime, allocation_watch *watch) {
  int source;
  for (source = 0; source != 4; ++source) {
    char path[] = LONEJSON_TEST_TMP_DIR "/record-reuse-XXXXXX";
    reader_state reader = {document, 0u, 0};
    lonejson_array_stream *stream = NULL;
    lonejson_error error;
    lonejson_int64 backing[1];
    record doc;
    words_seen words = {{0}, 0u};
    FILE *file = NULL;
    int fd = -1;
    size_t index;
    size_t calls;
    prepare(runtime, &doc, backing, &words);
    if (source == 0)
      stream = lonejson_array_stream_open_reader(runtime, "items", read_byte,
                                                 &reader, &error);
    else {
      fd = mkstemp(path);
      CHECK(fd >= 0);
      if (fd < 0)
        continue;
      CHECK(write(fd, document, strlen(document)) == (ssize_t)strlen(document));
      CHECK(lseek(fd, 0, SEEK_SET) == 0);
      if (source == 1) {
        file = fdopen(fd, "r+");
        CHECK(file != NULL);
        if (file != NULL) {
          fd = -1;
          stream =
              lonejson_array_stream_open_filep(runtime, "items", file, &error);
        }
      } else if (source == 2)
        stream = lonejson_array_stream_open_fd(runtime, "items", fd, &error);
      else
        stream =
            lonejson_array_stream_open_path(runtime, "items", path, &error);
    }
    CHECK(stream != NULL);
    if (stream != NULL) {
      calls = watch->calls;
      CHECK(lj_array_stream_set_record_reuse(stream, 1, &error) ==
            LONEJSON_STATUS_OK);
      CHECK(watch->calls == calls && reader.offset == 0u);
      CHECK(lonejson_array_stream_set_record_reuse(stream, 2, &error) ==
            LONEJSON_STATUS_INVALID_ARGUMENT);
      for (index = 0u; index != 3u; ++index) {
        CHECK(stream->next(stream, &record_map, &doc, &error) ==
              LONEJSON_ARRAY_STREAM_ITEM);
        check_backing(&doc, backing);
        check_item(&doc, index);
        if (source == 0 && index == 0u)
          CHECK(reader.offset < strlen(document));
        lonejson_reset(runtime, &record_map, &doc);
        check_backing(&doc, backing);
      }
      CHECK(strcmp(words.text, "abc") == 0);
      CHECK(stream->next(stream, &record_map, &doc, &error) ==
            LONEJSON_ARRAY_STREAM_EOF);
      CHECK(lonejson_array_stream_set_record_reuse(stream, 0, &error) ==
            LONEJSON_STATUS_INVALID_ARGUMENT);
      stream->close(stream);
    }
    lonejson_cleanup(&record_map, &doc);
    if (file != NULL) {
      CHECK(fseek(file, 0, SEEK_SET) == 0);
      fclose(file);
    }
    if (fd >= 0) {
      CHECK(fcntl(fd, F_GETFD) >= 0);
      close(fd);
    }
    if (source != 0)
      unlink(path);
  }
}

typedef struct callback_state {
  lonejson *runtime;
  lonejson_int64 *backing;
  size_t count;
  size_t supplied;
  size_t first_delivery;
  int cancel;
} callback_state;
static lonejson_status item_callback(void *user, void *value) {
  callback_state *seen = (callback_state *)user;
  record *doc = (record *)value;
  if (seen->count == 0u)
    seen->first_delivery = seen->supplied;
  check_backing(doc, seen->backing);
  check_item(doc, seen->count++);
  if (seen->cancel)
    return LONEJSON_STATUS_IO_ERROR;
  lonejson_reset(seen->runtime, &record_map, doc);
  return LONEJSON_STATUS_OK;
}
static void test_push(lonejson *runtime) {
  size_t chunks[] = {1u, 7u, sizeof(document) - 1u};
  size_t mode;
  for (mode = 0u; mode != 3u; ++mode) {
    lonejson_int64 backing[1];
    record doc;
    words_seen words = {{0}, 0u};
    callback_state seen;
    lonejson_error error;
    lonejson_array_stream *stream =
        lonejson_array_stream_open_push(runtime, "items", &error);
    size_t offset;
    seen.runtime = runtime;
    seen.backing = backing;
    seen.count = 0u;
    seen.cancel = 0;
    seen.supplied = 0u;
    seen.first_delivery = 0u;
    prepare(runtime, &doc, backing, &words);
    CHECK(stream != NULL);
    if (stream == NULL)
      continue;
    CHECK(stream->set_record_reuse(stream, 1, &error) == LONEJSON_STATUS_OK);
    for (offset = 0u; offset < strlen(document);) {
      size_t length = strlen(document) - offset;
      if (length > chunks[mode])
        length = chunks[mode];
      seen.supplied = offset + length;
      CHECK(stream->push(stream, &record_map, &doc, document + offset, length,
                         item_callback, &seen, &error) == LONEJSON_STATUS_OK);
      offset += length;
    }
    CHECK(stream->finish(stream, &record_map, &doc, item_callback, &seen,
                         &error) == LONEJSON_STATUS_OK);
    CHECK(seen.count == 3u && strcmp(words.text, "abc") == 0);
    if (mode != 2u)
      CHECK(seen.first_delivery < strlen(document));
    check_backing(&doc, backing);
    stream->close(stream);
    lonejson_cleanup(&record_map, &doc);
  }
}
static lonejson_status count_callback(void *user, void *value) {
  size_t *count = (size_t *)user;
  (void)value;
  ++*count;
  return LONEJSON_STATUS_OK;
}
static void test_errors(lonejson *runtime) {
  const char *cases[] = {
      "{\"items\":[{\"name\":\"owned\",\"values\":[7,8]}]}",
      "{\"items\":[{\"name\":\"owned\",\"values\":[7],\"marker\":!}]}",
      "{\"items\":[{\"name\":\"owned\",\"values\":[7",
      "{\"items\":[{\"name\":\"too-long-name\"}]}"};
  lonejson_status expected[] = {
      LONEJSON_STATUS_OVERFLOW, LONEJSON_STATUS_INVALID_JSON,
      LONEJSON_STATUS_INVALID_JSON, LONEJSON_STATUS_OVERFLOW};
  int push;
  size_t index;
  for (push = 0; push != 2; ++push)
    for (index = 0u; index != 4u; ++index) {
      reader_state reader = {NULL, 0u, 0};
      lonejson_error error;
      lonejson_error configuration_error;
      lonejson_int64 backing[1];
      record doc;
      words_seen words = {{0}, 0u};
      size_t count = 0u;
      lonejson_array_stream *stream;
      reader.json = cases[index];
      prepare(runtime, &doc, backing, &words);
      stream = push ? lonejson_array_stream_open_push(runtime, "items", &error)
                    : lonejson_array_stream_open_reader(
                          runtime, "items", read_byte, &reader, &error);
      CHECK(stream != NULL);
      if (stream == NULL)
        continue;
      CHECK(lonejson_array_stream_set_record_reuse(stream, 1, &error) ==
            LONEJSON_STATUS_OK);
      if (push) {
        lonejson_status status = lonejson_array_stream_push(
            stream, &record_map, &doc, cases[index], strlen(cases[index]),
            count_callback, &count, &error);
        if (status == LONEJSON_STATUS_OK)
          status = lonejson_array_stream_finish(stream, &record_map, &doc,
                                                count_callback, &count, &error);
        CHECK(status == expected[index] && count == 0u);
      } else
        CHECK(lonejson_array_stream_next(stream, &record_map, &doc, &error) ==
              LONEJSON_ARRAY_STREAM_ERROR);
      CHECK(error.code == expected[index]);
      CHECK(lonejson_array_stream_set_record_reuse(stream, 0,
                                                   &configuration_error) ==
            LONEJSON_STATUS_INVALID_ARGUMENT);
      CHECK(stream->error_fn(stream)->code == expected[index]);
      lonejson_array_stream_close(stream);
      check_backing(&doc, backing);
      if (index != 3u) {
        CHECK(doc.name != NULL);
        if (doc.name != NULL)
          CHECK(strcmp(doc.name, "owned") == 0);
      }
      lonejson_reset(runtime, &record_map, &doc);
      check_backing(&doc, backing);
      lonejson_cleanup(&record_map, &doc);
    }
}
static void test_callback_cancellation(lonejson *runtime) {
  lonejson_int64 backing[1];
  record doc;
  words_seen words = {{0}, 0u};
  callback_state seen;
  lonejson_error error;
  lonejson_array_stream *stream =
      lonejson_array_stream_open_push(runtime, "items", &error);
  seen.runtime = runtime;
  seen.backing = backing;
  seen.count = 0u;
  seen.cancel = 1;
  seen.supplied = strlen(document);
  seen.first_delivery = 0u;
  prepare(runtime, &doc, backing, &words);
  CHECK(stream != NULL);
  if (stream == NULL)
    return;
  CHECK(lonejson_array_stream_set_record_reuse(stream, 1, &error) ==
        LONEJSON_STATUS_OK);
  CHECK(lonejson_array_stream_push(stream, &record_map, &doc, document,
                                   strlen(document), item_callback, &seen,
                                   &error) == LONEJSON_STATUS_IO_ERROR);
  CHECK(seen.count == 1u);
  lonejson_array_stream_close(stream);
  check_backing(&doc, backing);
  CHECK(doc.name != NULL);
  if (doc.name != NULL)
    CHECK(strcmp(doc.name, "one") == 0);
  lonejson_cleanup(&record_map, &doc);
}

static void test_policy_isolation(lonejson *runtime) {
  const char json[] = "{\"values\":[1,2]}";
  reader_state readers[2] = {{NULL, 0u, 0}, {NULL, 0u, 0}};
  lonejson_array_stream *streams[2];
  lonejson_config config = lonejson_default_config();
  lonejson_error error;
  int mode;
  config.clear_destination_by_default = 0;
  for (mode = 0; mode != 4; ++mode) {
    lonejson_int64 backing[1];
    record doc;
    words_seen words = {{0}, 0u};
    lonejson *selected = runtime;
    if (mode == 3)
      selected = lonejson_new(&config, &error);
    CHECK(selected != NULL);
    if (selected == NULL)
      continue;
    readers[0].json = readers[1].json = "[{\"values\":[1,2]}]";
    readers[0].offset = readers[1].offset = 0u;
    streams[0] = lonejson_array_stream_open_reader(selected, "", read_byte,
                                                   &readers[0], &error);
    streams[1] = lonejson_array_stream_open_reader(selected, "", read_byte,
                                                   &readers[1], &error);
    CHECK(streams[0] != NULL && streams[1] != NULL);
    if (mode != 3)
      CHECK(lonejson_array_stream_set_record_reuse(streams[0], 1, &error) ==
            LONEJSON_STATUS_OK);
    if (mode == 1)
      CHECK(lonejson_array_stream_set_record_reuse(streams[0], 0, &error) ==
            LONEJSON_STATUS_OK);
    prepare(selected, &doc, backing, &words);
    if (mode == 2) {
      CHECK(lonejson_parse_cstr(selected, &record_map, &doc, json, &error) ==
            LONEJSON_STATUS_OK);
      CHECK(doc.values.count == 2u && doc.values.items != backing);
    } else {
      int reuse = mode == 0 || mode == 3;
      CHECK(lonejson_array_stream_next(streams[0], &record_map, &doc, &error) ==
            (reuse ? LONEJSON_ARRAY_STREAM_ERROR : LONEJSON_ARRAY_STREAM_ITEM));
      if (reuse) {
        CHECK(error.code == LONEJSON_STATUS_OVERFLOW);
        check_backing(&doc, backing);
      } else
        CHECK(doc.values.count == 2u && doc.values.items != backing);
    }
    lonejson_cleanup(&record_map, &doc);
    if (mode != 3) {
      prepare(selected, &doc, backing, &words);
      CHECK(lonejson_array_stream_next(streams[1], &record_map, &doc, &error) ==
            LONEJSON_ARRAY_STREAM_ITEM);
      CHECK(doc.values.count == 2u && doc.values.items != backing);
      lonejson_cleanup(&record_map, &doc);
    }
    lonejson_array_stream_close(streams[0]);
    lonejson_array_stream_close(streams[1]);
    if (mode == 3)
      lonejson_free(selected);
  }
}
static void test_configuration_lifecycle(lonejson *runtime) {
  lonejson_error error;
  lonejson_int64 backing[1];
  record doc;
  words_seen words = {{0}, 0u};
  reader_state reader = {"[]", 0u, 1};
  size_t count = 0u;
  lonejson_array_stream *stream;
  CHECK(lonejson_array_stream_set_record_reuse(NULL, 1, &error) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
  stream = lonejson_array_stream_open_reader(runtime, "", read_byte, &reader,
                                             &error);
  CHECK(stream != NULL);
  if (stream == NULL)
    return;
  CHECK(lonejson_array_stream_set_record_reuse(stream, -1, &error) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
  prepare(runtime, &doc, backing, &words);
  CHECK(lonejson_array_stream_next(stream, &record_map, &doc, &error) ==
        LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
  CHECK(lonejson_array_stream_set_record_reuse(stream, 1, &error) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
  CHECK(error.message[0] != '\0');
  CHECK(lonejson_array_stream_next(stream, &record_map, &doc, &error) ==
        LONEJSON_ARRAY_STREAM_EOF);
  lonejson_array_stream_close(stream);
  lonejson_cleanup(&record_map, &doc);
  stream = lonejson_array_stream_open_push(runtime, "", &error);
  CHECK(lonejson_array_stream_push(stream, &record_map, &doc, NULL, 0u,
                                   count_callback, &count,
                                   &error) == LONEJSON_STATUS_OK);
  CHECK(lonejson_array_stream_set_record_reuse(stream, 1, &error) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_array_stream_close(stream);
  stream = lonejson_array_stream_open_push(runtime, "", &error);
  (void)lonejson_array_stream_finish(stream, &record_map, &doc, count_callback,
                                     &count, &error);
  CHECK(lonejson_array_stream_set_record_reuse(stream, 0, NULL) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_array_stream_close(stream);
}
static lonejson_status raw_sink(void *user, const void *bytes, size_t len,
                                lonejson_error *error) {
  return word_chunk(user, (const char *)bytes, len, error);
}
static void test_raw_and_strings(lonejson *runtime) {
  int enabled;
  lonejson_json_value *value = (lonejson_json_value *)malloc(sizeof(*value));
  CHECK(value != NULL);
  if (value == NULL)
    return;
  for (enabled = 0; enabled <= 1; ++enabled) {
    reader_state reader = {"[1,{\"a\":2},\"hi\"]", 0u, 0};
    const char *expected[] = {"1", "{\"a\":2}", "\"hi\""};
    lonejson_error error;
    lonejson_array_stream *stream = lonejson_array_stream_open_reader(
        runtime, "", read_byte, &reader, &error);
    size_t index;
    int items;
    CHECK(stream != NULL);
    if (stream == NULL)
      continue;
    lonejson_json_value_init(runtime, value);
    CHECK(lonejson_array_stream_set_record_reuse(stream, enabled, NULL) ==
          LONEJSON_STATUS_OK);
    for (index = 0u; index != 3u; ++index) {
      words_seen raw = {{0}, 0u};
      CHECK(lonejson_array_stream_next_value(stream, value, &error) ==
            LONEJSON_ARRAY_STREAM_ITEM);
      CHECK(lonejson_json_value_write_to_sink(value, raw_sink, &raw, &error) ==
            LONEJSON_STATUS_OK);
      CHECK(strcmp(raw.text, expected[index]) == 0);
      CHECK(lonejson_array_stream_set_record_reuse(stream, enabled, &error) ==
            LONEJSON_STATUS_INVALID_ARGUMENT);
      lonejson_json_value_reset(value);
    }
    CHECK(lonejson_array_stream_next_value(stream, value, &error) ==
          LONEJSON_ARRAY_STREAM_EOF);
    lonejson_array_stream_close(stream);
    lonejson_json_value_cleanup(value);
    for (items = 0; items <= 1; ++items) {
      static const char strings[] = "[\"a\",\"bc\"]";
      lonejson_array_stream_string_handler handler;
      words_seen words = {{0}, 0u};
      memset(&handler, 0, sizeof(handler));
      handler.chunk = word_chunk;
      stream = lonejson_array_stream_open_push(runtime, "", &error);
      CHECK(stream != NULL);
      if (stream == NULL)
        continue;
      CHECK(lonejson_array_stream_set_record_reuse(stream, enabled, &error) ==
            LONEJSON_STATUS_OK);
      if (items)
        CHECK(lonejson_array_stream_push_string_items(
                  stream, NULL, 0u, word_chunk, &words, &error) ==
              LONEJSON_STATUS_OK);
      else
        CHECK(lonejson_array_stream_push_string(stream, NULL, 0u, &handler,
                                                &words,
                                                &error) == LONEJSON_STATUS_OK);
      CHECK(lonejson_array_stream_set_record_reuse(stream, !enabled, &error) ==
            LONEJSON_STATUS_INVALID_ARGUMENT);
      for (index = 0u; index < strlen(strings); ++index) {
        if (items)
          CHECK(lonejson_array_stream_push_string_items(
                    stream, strings + index, 1u, word_chunk, &words, &error) ==
                LONEJSON_STATUS_OK);
        else
          CHECK(lonejson_array_stream_push_string(stream, strings + index, 1u,
                                                  &handler, &words, &error) ==
                LONEJSON_STATUS_OK);
      }
      if (items)
        CHECK(lonejson_array_stream_finish_string_items(
                  stream, word_chunk, &words, &error) == LONEJSON_STATUS_OK);
      else
        CHECK(lonejson_array_stream_finish_string(
                  stream, &handler, &words, &error) == LONEJSON_STATUS_OK);
      CHECK(strcmp(words.text, "abc") == 0);
      lonejson_array_stream_close(stream);
    }
  }
  free(value);
}
static void test_caller_reset_and_legacy_push(lonejson *runtime) {
  reader_state reader = {"[{\"name\":\"one\",\"values\":[7]},{}]", 0u, 0};
  lonejson_error error;
  lonejson_int64 backing[1];
  record doc;
  words_seen words = {{0}, 0u};
  lonejson_array_stream *stream = lonejson_array_stream_open_reader(
      runtime, "", read_byte, &reader, &error);
  char *owned;
  int reuse;
  prepare(runtime, &doc, backing, &words);
  CHECK(stream != NULL);
  if (stream == NULL)
    return;
  CHECK(lonejson_array_stream_set_record_reuse(stream, 1, &error) ==
        LONEJSON_STATUS_OK);
  CHECK(lonejson_array_stream_next(stream, &record_map, &doc, &error) ==
        LONEJSON_ARRAY_STREAM_ITEM);
  owned = doc.name;
  CHECK(owned != NULL);
  CHECK(lonejson_array_stream_next(stream, &record_map, &doc, &error) ==
        LONEJSON_ARRAY_STREAM_ITEM);
  CHECK(doc.name == owned && doc.values.count == 1u);
  check_backing(&doc, backing);
  lonejson_array_stream_close(stream);
  lonejson_cleanup(&record_map, &doc);
  for (reuse = 0; reuse <= 1; ++reuse) {
    static const char json[] = "[{\"name\":\"one\",\"values\":[7]}]";
    lonejson_config config = lonejson_default_config();
    lonejson *selected = runtime;
    size_t count = 0u;
    config.clear_destination_by_default = 0;
    if (reuse)
      selected = lonejson_new(&config, &error);
    CHECK(selected != NULL);
    if (selected == NULL)
      continue;
    prepare(selected, &doc, backing, &words);
    stream = lonejson_array_stream_open_push(selected, "", &error);
    CHECK(stream != NULL);
    if (stream != NULL) {
      CHECK(lonejson_array_stream_push(stream, &record_map, &doc, json,
                                       strlen(json), count_callback, &count,
                                       &error) == LONEJSON_STATUS_OK);
      CHECK(count == 1u && doc.name == NULL);
      CHECK(lonejson_array_stream_finish(stream, &record_map, &doc,
                                         count_callback, &count,
                                         &error) == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    lonejson_cleanup(&record_map, &doc);
    if (reuse)
      lonejson_free(selected);
  }
}
int main(void) {
  allocation_watch watch = {0u, 0u};
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_config config = lonejson_default_config();
  lonejson_error error;
  lonejson *runtime;
  allocator.ctx = &watch;
  allocator.malloc_fn = watch_malloc;
  allocator.realloc_fn = watch_realloc;
  allocator.free_fn = watch_free;
  config.allocator = &allocator;
  config.max_dynamic_string_bytes = 8u;
  runtime = lonejson_new(&config, &error);
  CHECK(runtime != NULL);
  if (runtime != NULL) {
    test_pull_sources(runtime, &watch);
    test_push(runtime);
    test_errors(runtime);
    test_callback_cancellation(runtime);
    test_policy_isolation(runtime);
    test_configuration_lifecycle(runtime);
    test_raw_and_strings(runtime);
    test_caller_reset_and_legacy_push(runtime);
    lonejson_free(runtime);
  }
  CHECK(watch.live == 0u);
  return failures != 0;
}
