#include "lonejson.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);               \
      return 0;                                                                \
    }                                                                          \
  } while (0)

typedef struct trace {
  char text[160];
  size_t length;
  size_t calls;
  lonejson_status response;
} trace;

static lonejson_status record(trace *out, const char *data, size_t length) {
  if (length > sizeof(out->text) - out->length - 1u) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(out->text + out->length, data, length);
  out->length += length;
  out->text[out->length] = '\0';
  out->calls++;
  return out->response;
}

#define EVENT(fn, marker)                                                      \
  static lonejson_status fn(void *user, lonejson_error *error) {               \
    (void)error;                                                               \
    return record((trace *)user, marker, sizeof(marker) - 1u);                 \
  }

EVENT(object_begin, "{")
EVENT(object_end, "}")
EVENT(array_begin, "[")
EVENT(array_end, "]")
EVENT(key_begin, "<")
EVENT(key_end, ">")
EVENT(string_begin, "\"")
EVENT(string_end, "\"")
EVENT(number_begin, "(")
EVENT(number_end, ")")
EVENT(null_value, "n")

static lonejson_status chunk(void *user, const char *data, size_t length,
                             lonejson_error *error) {
  (void)error;
  return record((trace *)user, data, length);
}

static lonejson_status boolean_value(void *user, int value,
                                     lonejson_error *error) {
  (void)error;
  return record((trace *)user, value ? "t" : "f", 1u);
}

static int check_chunks(lonejson *runtime, size_t chunk_size, int pause) {
  const char input[] = "{\"k\xc2\xa2\":[true,false,null,-12.5e+2,"
                       "\"ab\\n\\uD834\\uDD1E\xe2\x82\xac\xf4\x8f\xbf\xbf"
                       "cd\"],\"\":{}} \n";
  const char expected[] = "{<k\xc2\xa2>[tfn(-12.5e+2)\"ab\n\xf0\x9d\x84\x9e"
                          "\xe2\x82\xac\xf4\x8f\xbf\xbf"
                          "cd\"]<>{}}";
  char buffer[sizeof(input)];
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  lonejson_error error;
  lonejson_status status;
  trace out;
  size_t offset = 0u;
  size_t end = 0u;
  size_t iterations = 0u;

  memset(&out, 0, sizeof(out));
  out.response = pause ? LONEJSON_STATUS_PAUSED : LONEJSON_STATUS_OK;
  visitor.object_begin = object_begin;
  visitor.object_end = object_end;
  visitor.object_key_begin = key_begin;
  visitor.object_key_chunk = chunk;
  visitor.object_key_end = key_end;
  visitor.array_begin = array_begin;
  visitor.array_end = array_end;
  visitor.string_begin = string_begin;
  visitor.string_chunk = chunk;
  visitor.string_end = string_end;
  visitor.number_begin = number_begin;
  visitor.number_chunk = chunk;
  visitor.number_end = number_end;
  visitor.boolean_value = boolean_value;
  visitor.null_value = null_value;
  CHECK(runtime->value_visitor_stream_open != NULL);
  stream = runtime->value_visitor_stream_open(runtime, &visitor, &out, &error);
  CHECK(stream != NULL);
  /* The receiver owns its callback table. */
  visitor = lonejson_default_value_visitor();
  while (offset < sizeof(input) - 1u) {
    size_t consumed;
    size_t before = out.calls;

    if (offset == end) {
      end += chunk_size;
      if (end > sizeof(input) - 1u) {
        end = sizeof(input) - 1u;
      }
    }
    memcpy(buffer, input + offset, end - offset);
    status = stream->push(stream, buffer, end - offset, &consumed, &error);
    memset(buffer, '!', sizeof(buffer));
    CHECK(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_PAUSED);
    CHECK(error.code == LONEJSON_STATUS_OK);
    CHECK(consumed <= end - offset);
    if (pause) {
      CHECK(out.calls == before + (status == LONEJSON_STATUS_PAUSED ? 1u : 0u));
    }
    CHECK(++iterations < sizeof(input) * 4u);
    offset += consumed;
  }
  CHECK(stream->finish(stream, &error) == LONEJSON_STATUS_OK);
  CHECK(strcmp(out.text, expected) == 0);
  stream->close(stream);
  return 1;
}

static int check_number_at_eof(lonejson *runtime) {
  lj_value_visitor visitor = lonejson_default_value_visitor();
  lj_value_visitor_stream *stream;
  lj_error error;
  trace out;
  size_t consumed;

  memset(&out, 0, sizeof(out));
  out.response = LJ_STATUS_PAUSED;
  visitor.number_begin = number_begin;
  visitor.number_chunk = chunk;
  visitor.number_end = number_end;
  stream = lj_value_visitor_stream_open(runtime, &visitor, &out, &error);
  CHECK(stream != NULL);
  CHECK(lj_value_visitor_stream_push(stream, "12", 2u, &consumed, &error) ==
        LONEJSON_STATUS_OK);
  CHECK(consumed == 2u && out.calls == 0u);
  CHECK(lj_value_visitor_stream_finish(stream, &error) == LJ_STATUS_PAUSED);
  CHECK(out.calls == 1u);
  CHECK(lj_value_visitor_stream_push(stream, "3", 1u, &consumed, &error) ==
        LONEJSON_STATUS_INVALID_ARGUMENT);
  CHECK(consumed == 0u);
  CHECK(lj_value_visitor_stream_finish(stream, &error) == LJ_STATUS_PAUSED);
  CHECK(out.calls == 2u);
  CHECK(lj_value_visitor_stream_finish(stream, &error) == LJ_STATUS_PAUSED);
  CHECK(out.calls == 3u);
  CHECK(lj_value_visitor_stream_finish(stream, &error) == LONEJSON_STATUS_OK);
  CHECK(strcmp(out.text, "(12)") == 0);
  lj_value_visitor_stream_close(stream);
  return 1;
}

static int check_empty_resume(lonejson *runtime) {
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  trace out;
  size_t consumed;

  memset(&out, 0, sizeof(out));
  out.response = LONEJSON_STATUS_PAUSED;
  visitor.number_begin = number_begin;
  visitor.number_chunk = chunk;
  visitor.number_end = number_end;
  stream = lonejson_value_visitor_stream_open(runtime, &visitor, &out, NULL);
  CHECK(stream != NULL);
  CHECK(stream->push(stream, "123 ", 4u, &consumed, NULL) ==
        LONEJSON_STATUS_PAUSED);
  CHECK(consumed == 3u && out.calls == 1u);
  CHECK(stream->push(stream, NULL, 0u, &consumed, NULL) ==
        LONEJSON_STATUS_PAUSED);
  CHECK(consumed == 0u && out.calls == 2u);
  CHECK(stream->push(stream, NULL, 0u, &consumed, NULL) ==
        LONEJSON_STATUS_PAUSED);
  CHECK(consumed == 0u && out.calls == 3u);
  CHECK(stream->push(stream, " ", 1u, &consumed, NULL) == LONEJSON_STATUS_OK);
  CHECK(consumed == 1u && out.calls == 3u);
  CHECK(stream->finish(stream, NULL) == LONEJSON_STATUS_OK);
  CHECK(strcmp(out.text, "(123)") == 0);
  stream->close(stream);
  return 1;
}

static int check_failures(lonejson *runtime) {
  static const char *const malformed[] = {
      "", "[", "[1,]", "1 2", "01", "1e+", "true false", "\"\\uD834\""};
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  lonejson_error error;
  lonejson_status status;
  size_t consumed;
  size_t i;
  trace out;

  for (i = 0u; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
    stream =
        lonejson_value_visitor_stream_open(runtime, &visitor, NULL, &error);
    CHECK(stream != NULL);
    status = stream->push(stream, malformed[i], strlen(malformed[i]), &consumed,
                          &error);
    if (status == LONEJSON_STATUS_OK) {
      status = stream->finish(stream, &error);
    }
    CHECK(status == LONEJSON_STATUS_INVALID_JSON && error.code == status);
    CHECK(stream->push(stream, "null", 4u, &consumed, NULL) == status);
    CHECK(consumed == 0u);
    stream->close(stream);
  }

  memset(&out, 0, sizeof(out));
  out.response = LONEJSON_STATUS_TRUNCATED;
  visitor.number_chunk = chunk;
  visitor.number_end = number_end;
  stream = lonejson_value_visitor_stream_open(runtime, &visitor, &out, &error);
  CHECK(stream != NULL);
  CHECK(stream->push(stream, "12 ", 3u, &consumed, &error) ==
        LONEJSON_STATUS_CALLBACK_FAILED);
  CHECK(consumed == 2u && out.calls == 1u);
  CHECK(error.code == LONEJSON_STATUS_CALLBACK_FAILED && error.message[0]);
  CHECK(stream->finish(stream, NULL) == LONEJSON_STATUS_CALLBACK_FAILED);
  CHECK(out.calls == 1u);
  stream->close(stream);
  return 1;
}

static int check_nonadjacent_surrogates(lonejson *runtime) {
  static const char *const malformed[] = {"\"\\uD834x\\uDD1E\"",
                                          "\"\\uD834\\n\\uDD1E\"",
                                          "{\"\\uD834x\\uDD1E\":null}"};
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  size_t i;

  visitor.string_chunk = chunk;
  visitor.object_key_chunk = chunk;
  for (i = 0u; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
    size_t len = strlen(malformed[i]);
    size_t split;

    for (split = 0u; split <= len; ++split) {
      lonejson_value_visitor_stream *stream;
      lonejson_status status;
      size_t consumed;
      trace out;

      memset(&out, 0, sizeof(out));
      stream =
          lonejson_value_visitor_stream_open(runtime, &visitor, &out, NULL);
      CHECK(stream != NULL);
      status = stream->push(stream, malformed[i], split, &consumed, NULL);
      if (status == LONEJSON_STATUS_OK) {
        CHECK(consumed == split);
        status = stream->push(stream, malformed[i] + split, len - split,
                              &consumed, NULL);
      }
      CHECK(status == LONEJSON_STATUS_INVALID_JSON);
      CHECK(out.calls == 0u);
      stream->close(stream);
    }
  }
  return 1;
}

static int check_split_utf8_pause(lonejson *runtime) {
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  size_t consumed;
  trace out;

  memset(&out, 0, sizeof(out));
  out.response = LONEJSON_STATUS_PAUSED;
  visitor.string_chunk = chunk;
  stream = lonejson_value_visitor_stream_open(runtime, &visitor, &out, NULL);
  CHECK(stream != NULL);
  CHECK(stream->push(stream, "\"\xf0", 2u, &consumed, NULL) ==
        LONEJSON_STATUS_OK);
  CHECK(consumed == 2u && out.calls == 0u);
  CHECK(stream->push(stream, "\x9f\x98", 2u, &consumed, NULL) ==
        LONEJSON_STATUS_OK);
  CHECK(consumed == 2u && out.calls == 0u);
  CHECK(stream->push(stream, "\x80\"", 2u, &consumed, NULL) ==
        LONEJSON_STATUS_PAUSED);
  CHECK(consumed == 1u && out.calls == 1u);
  CHECK(out.length == 4u && memcmp(out.text, "\xf0\x9f\x98\x80", 4u) == 0);
  CHECK(stream->push(stream, "\"", 1u, &consumed, NULL) == LONEJSON_STATUS_OK);
  CHECK(stream->finish(stream, NULL) == LONEJSON_STATUS_OK);
  stream->close(stream);
  return 1;
}

static int check_malformed_utf8(lonejson *runtime) {
  static const char *const malformed[] = {"\x80",
                                          "\xc0\xaf",
                                          "\xe0\x9f\x80",
                                          "\xf0\x8f\xbf\xbf",
                                          "\xed\xa0\x80",
                                          "\xf4\x90\x80\x80",
                                          "\xf5\x80\x80\x80",
                                          "\xe2x",
                                          "\xe2\\n",
                                          "\xef\xbf"};
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  size_t i;

  visitor.string_chunk = chunk;
  visitor.object_key_chunk = chunk;
  for (i = 0u; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
    int key;

    for (key = 0; key <= 1; ++key) {
      char input[32];
      size_t len;
      size_t split;

      strcpy(input, key ? "{\"" : "\"");
      strcat(input, malformed[i]);
      strcat(input, key ? "\":null}" : "\"");
      len = strlen(input);
      for (split = 0u; split <= len; ++split) {
        lonejson_value_visitor_stream *stream;
        lonejson_status status;
        size_t consumed;
        trace out;

        memset(&out, 0, sizeof(out));
        stream =
            lonejson_value_visitor_stream_open(runtime, &visitor, &out, NULL);
        CHECK(stream != NULL);
        status = stream->push(stream, input, split, &consumed, NULL);
        if (status == LONEJSON_STATUS_OK) {
          CHECK(consumed == split);
          status =
              stream->push(stream, input + split, len - split, &consumed, NULL);
        }
        CHECK(status == LONEJSON_STATUS_INVALID_JSON);
        CHECK(out.calls == 0u);
        stream->close(stream);
      }
    }
  }
  return 1;
}

static lonejson_status validate_nested(void *user, lonejson_error *error) {
  return lonejson_validate_cstr((lonejson *)user, "true", error);
}

static lonejson_status validate_nested_chunk(void *user, const char *data,
                                             size_t len,
                                             lonejson_error *error) {
  (void)data;
  (void)len;
  return validate_nested(user, error);
}

static lonejson_status validate_nested_bool(void *user, int value,
                                            lonejson_error *error) {
  (void)value;
  return validate_nested(user, error);
}

static lonejson_status fail_nested(void *user, lonejson_error *error) {
  lonejson_status status = lonejson_validate_cstr((lonejson *)user, "[", error);
  error->system_errno = EINVAL;
  return status;
}

static int check_callback_error_position(lonejson *runtime) {
  static const char *const inputs[] = {"123456789 x",
                                       "[123456789,\"ab\",true]\n x"};
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  lonejson_error error;
  lonejson_error nested_error;
  size_t consumed;
  size_t i;

  visitor.array_begin = validate_nested;
  visitor.number_begin = validate_nested;
  visitor.number_chunk = validate_nested_chunk;
  visitor.number_end = validate_nested;
  visitor.string_chunk = validate_nested_chunk;
  visitor.boolean_value = validate_nested_bool;
  for (i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
    size_t len = strlen(inputs[i]);

    stream =
        lonejson_value_visitor_stream_open(runtime, &visitor, runtime, NULL);
    CHECK(stream != NULL);
    CHECK(stream->push(stream, inputs[i], len, &consumed, &error) ==
          LONEJSON_STATUS_INVALID_JSON);
    CHECK(error.offset == len && error.line == i + 1u);
    CHECK(error.column == (i == 0u ? len : 2u));
    stream->close(stream);
  }

  CHECK(fail_nested(runtime, &nested_error) == LONEJSON_STATUS_INVALID_JSON);
  visitor.number_end = fail_nested;
  stream = lonejson_value_visitor_stream_open(runtime, &visitor, runtime, NULL);
  CHECK(stream != NULL);
  CHECK(stream->push(stream, inputs[0], strlen(inputs[0]), &consumed, &error) ==
        LONEJSON_STATUS_CALLBACK_FAILED);
  CHECK(consumed == 9u && error.offset == 9u && error.line == 1u &&
        error.column == 9u);
  CHECK(error.system_errno == EINVAL);
  CHECK(strcmp(error.message, nested_error.message) == 0);
  stream->close(stream);
  return 1;
}

static int check_string_error_progress(lonejson *runtime) {
  static const char *const inputs[] = {"\"abc\x01\"", "\"a\\nb\x01\""};
  static const char *const decoded[] = {"abc", "a\nb"};
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  size_t i;

  visitor.string_chunk = chunk;
  for (i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
    lonejson_value_visitor_stream *stream;
    lonejson_error error;
    size_t consumed;
    size_t len = strlen(inputs[i]);
    trace out;

    memset(&out, 0, sizeof(out));
    stream = lonejson_value_visitor_stream_open(runtime, &visitor, &out, NULL);
    CHECK(stream != NULL);
    CHECK(stream->push(stream, inputs[i], len, &consumed, &error) ==
          LONEJSON_STATUS_INVALID_JSON);
    CHECK(strcmp(out.text, decoded[i]) == 0);
    CHECK(consumed == len - 2u && error.offset == consumed);
    stream->close(stream);
  }
  return 1;
}

static int check_number_error_progress(lonejson *runtime) {
  char digits[1024];
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  size_t split;

  memset(digits, '1', sizeof(digits));
  for (split = 0u; split <= 256u; split += 128u) {
    lonejson_value_visitor_stream *stream;
    lonejson_error error;
    size_t consumed;
    trace out;

    memset(&out, 0, sizeof(out));
    visitor.number_begin = number_begin;
    stream = lonejson_value_visitor_stream_open(runtime, &visitor, &out, NULL);
    CHECK(stream != NULL);
    CHECK(stream->push(stream, digits, split, &consumed, NULL) ==
          LONEJSON_STATUS_OK);
    CHECK(consumed == split);
    CHECK(stream->push(stream, digits + split, sizeof(digits) - split,
                       &consumed, &error) == LONEJSON_STATUS_OVERFLOW);
    CHECK(consumed == 256u - split && error.offset == 256u);
    CHECK(out.calls == 0u);
    stream->close(stream);
  }
  return 1;
}

static lonejson_status count_chunk(void *user, const char *data, size_t length,
                                   lonejson_error *error) {
  size_t *count = (size_t *)user;

  (void)error;
  if (length == 0u || data[0] != 'x' || data[length - 1u] != 'x') {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  *count += length;
  return LONEJSON_STATUS_PAUSED;
}

static int check_streaming(lonejson *runtime) {
  char bytes[1024];
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  size_t count = 0u;
  size_t consumed;
  size_t i;

  visitor.string_chunk = count_chunk;
  stream = lonejson_value_visitor_stream_open(runtime, &visitor, &count, NULL);
  CHECK(stream != NULL);
  memset(bytes, 'x', sizeof(bytes));
  CHECK(stream->push(stream, "\"", 1u, &consumed, NULL) == LONEJSON_STATUS_OK);
  for (i = 0u; i < 512u; ++i) {
    CHECK(stream->push(stream, bytes, sizeof(bytes), &consumed, NULL) ==
          LONEJSON_STATUS_PAUSED);
    CHECK(consumed == sizeof(bytes) && count == (i + 1u) * sizeof(bytes));
  }
  CHECK(stream->push(stream, "\"", 1u, &consumed, NULL) == LONEJSON_STATUS_OK);
  CHECK(stream->finish(stream, NULL) == LONEJSON_STATUS_OK);
  stream->close(stream);
  return 1;
}

static int check_depth_and_number_workspace(lonejson *runtime) {
  char value[64u + 256u + 64u];
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  size_t consumed;

  stream = lonejson_value_visitor_stream_open(runtime, &visitor, NULL, NULL);
  CHECK(stream != NULL);
  memset(value, '[', 64u);
  memset(value + 64u, '1', 256u);
  memset(value + 64u + 256u, ']', 64u);
  CHECK(stream->push(stream, value, sizeof(value), &consumed, NULL) ==
        LONEJSON_STATUS_OK);
  CHECK(consumed == sizeof(value));
  CHECK(stream->finish(stream, NULL) == LONEJSON_STATUS_OK);
  stream->close(stream);

  stream = lonejson_value_visitor_stream_open(runtime, &visitor, NULL, NULL);
  CHECK(stream != NULL);
  value[64u] = '[';
  CHECK(stream->push(stream, value, 65u, &consumed, NULL) ==
        LONEJSON_STATUS_OVERFLOW);
  stream->close(stream);
  return 1;
}

static int check_limits(void) {
  lonejson_config config = lonejson_default_config();
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *number_stream;
  lonejson_value_visitor_stream *total_stream;
  lonejson *runtime;
  size_t consumed;

  config.json_value_max_number_bytes = 3u;
  config.json_value_max_total_bytes = 5u;
  runtime = lonejson_new(&config, NULL);
  CHECK(runtime != NULL);
  number_stream =
      lonejson_value_visitor_stream_open(runtime, &visitor, NULL, NULL);
  total_stream =
      lonejson_value_visitor_stream_open(runtime, &visitor, NULL, NULL);
  lonejson_free(runtime);
  CHECK(number_stream != NULL && total_stream != NULL);
  CHECK(number_stream->push(number_stream, "123", 3u, &consumed, NULL) ==
        LONEJSON_STATUS_OK);
  CHECK(number_stream->push(number_stream, "4", 1u, &consumed, NULL) ==
        LONEJSON_STATUS_OVERFLOW);
  CHECK(consumed == 0u);
  number_stream->close(number_stream);
  CHECK(total_stream->push(total_stream, "null  ", 6u, &consumed, NULL) ==
        LONEJSON_STATUS_OVERFLOW);
  CHECK(consumed == 5u);
  total_stream->close(total_stream);
  config.json_value_max_depth = (size_t)-1;
  runtime = lonejson_new(&config, NULL);
  CHECK(runtime != NULL);
  {
    lonejson_error error;
    CHECK(lonejson_value_visitor_stream_open(runtime, &visitor, NULL, &error) ==
          NULL);
    CHECK(error.code == LONEJSON_STATUS_OVERFLOW);
  }
  lonejson_free(runtime);
  return 1;
}

typedef struct allocation_state {
  size_t live;
  int fail;
} allocation_state;

static void *count_malloc(void *user, size_t size) {
  allocation_state *state = (allocation_state *)user;
  void *result = state->fail ? NULL : malloc(size);

  if (result != NULL) {
    state->live++;
  }
  return result;
}

static void *count_realloc(void *user, void *ptr, size_t size) {
  allocation_state *state = (allocation_state *)user;

  return ptr == NULL   ? count_malloc(user, size)
         : state->fail ? NULL
                       : realloc(ptr, size);
}

static void count_free(void *user, void *ptr) {
  allocation_state *state = (allocation_state *)user;

  if (ptr != NULL) {
    state->live--;
    free(ptr);
  }
}

static int check_allocator(void) {
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_config config = lonejson_default_config();
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  lonejson_error error;
  allocation_state allocations;
  trace out;
  lonejson *runtime;
  size_t consumed;

  memset(&allocations, 0, sizeof(allocations));
  memset(&out, 0, sizeof(out));
  out.response = LONEJSON_STATUS_PAUSED;
  allocator.malloc_fn = count_malloc;
  allocator.realloc_fn = count_realloc;
  allocator.free_fn = count_free;
  allocator.ctx = &allocations;
  config.allocator = &allocator;
  runtime = lonejson_new(&config, NULL);
  CHECK(runtime != NULL);
  allocations.fail = 1;
  CHECK(lonejson_value_visitor_stream_open(runtime, &visitor, &out, &error) ==
        NULL);
  CHECK(error.code == LONEJSON_STATUS_ALLOCATION_FAILED);
  allocations.fail = 0;
  visitor.number_begin = number_begin;
  visitor.number_chunk = chunk;
  stream = lonejson_value_visitor_stream_open(runtime, &visitor, &out, &error);
  CHECK(stream != NULL);
  lonejson_free(runtime);
  CHECK(stream->push(stream, "12 ", 3u, &consumed, &error) ==
        LONEJSON_STATUS_PAUSED);
  stream->close(stream);
  CHECK(allocations.live == 0u && out.calls == 1u);
  return 1;
}

int main(void) {
  lonejson *runtime = lonejson_new(NULL, NULL);
  size_t chunk_size;
  int ok;

  if (runtime == NULL) {
    return 1;
  }
  ok = check_number_at_eof(runtime) && check_empty_resume(runtime) &&
       check_failures(runtime) && check_nonadjacent_surrogates(runtime) &&
       check_split_utf8_pause(runtime) && check_malformed_utf8(runtime) &&
       check_callback_error_position(runtime) &&
       check_string_error_progress(runtime) &&
       check_number_error_progress(runtime) && check_streaming(runtime) &&
       check_depth_and_number_workspace(runtime) && check_limits() &&
       check_allocator();
  for (chunk_size = 1u; ok && chunk_size <= 64u; ++chunk_size) {
    ok = check_chunks(runtime, chunk_size, 1) &&
         check_chunks(runtime, chunk_size, 0);
  }
  lonejson_free(runtime);
  return ok ? 0 : 1;
}
