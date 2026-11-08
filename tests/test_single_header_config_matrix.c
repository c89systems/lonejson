#include <string.h>

#if defined(TEST_LJ_IMPLEMENTATION_ALIAS)
#define LJ_IMPLEMENTATION
#else
#define LONEJSON_IMPLEMENTATION
#endif

#if defined(TEST_LJ_CONFIG_ALIASES)
#define LJ_PARSER_BUFFER_SIZE 128
#define LJ_READER_BUFFER_SIZE 17
#define LJ_PUSH_PARSER_BUFFER_SIZE 129
#define LJ_STREAM_BUFFER_SIZE 19
#define LJ_SPOOL_MEMORY_LIMIT 23
#define LJ_WRITE_MAX_OUTPUT_BYTES 4096
#define LJ_TRACK_WORKSPACE_USAGE 1
#endif

#if defined(TEST_DISABLE_SHORT_NAMES)
#define LONEJSON_DISABLE_SHORT_NAMES 1
#endif

#if defined(TEST_OMIT_PROTOCOL_FRAMING)
#define LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL
#endif

#include "lonejson.h"

#if defined(TEST_LJ_CONFIG_ALIASES)
#if LONEJSON_PARSER_BUFFER_SIZE != 128
#error LJ_PARSER_BUFFER_SIZE must configure LONEJSON_PARSER_BUFFER_SIZE
#endif
#if LONEJSON_READER_BUFFER_SIZE != 17
#error LJ_READER_BUFFER_SIZE must configure LONEJSON_READER_BUFFER_SIZE
#endif
#if LONEJSON_PUSH_PARSER_BUFFER_SIZE != 129
#error LJ_PUSH_PARSER_BUFFER_SIZE must configure LONEJSON_PUSH_PARSER_BUFFER_SIZE
#endif
#if LONEJSON_STREAM_BUFFER_SIZE != 19
#error LJ_STREAM_BUFFER_SIZE must configure LONEJSON_STREAM_BUFFER_SIZE
#endif
#if LONEJSON_SPOOL_MEMORY_LIMIT != 23
#error LJ_SPOOL_MEMORY_LIMIT must configure LONEJSON_SPOOL_MEMORY_LIMIT
#endif
#if LONEJSON_WRITE_MAX_OUTPUT_BYTES != 4096
#error LJ_WRITE_MAX_OUTPUT_BYTES must configure LONEJSON_WRITE_MAX_OUTPUT_BYTES
#endif
#if LONEJSON_TRACK_WORKSPACE_USAGE != 1
#error LJ_TRACK_WORKSPACE_USAGE must configure LONEJSON_TRACK_WORKSPACE_USAGE
#endif
#if LJ_PARSER_BUFFER_SIZE != 128
#error caller-provided LJ_PARSER_BUFFER_SIZE must not be redefined
#endif
#if LJ_READER_BUFFER_SIZE != 17
#error caller-provided LJ_READER_BUFFER_SIZE must not be redefined
#endif
#if LJ_PUSH_PARSER_BUFFER_SIZE != 129
#error caller-provided LJ_PUSH_PARSER_BUFFER_SIZE must not be redefined
#endif
#if LJ_STREAM_BUFFER_SIZE != 19
#error caller-provided LJ_STREAM_BUFFER_SIZE must not be redefined
#endif
#if LJ_SPOOL_MEMORY_LIMIT != 23
#error caller-provided LJ_SPOOL_MEMORY_LIMIT must not be redefined
#endif
#if LJ_WRITE_MAX_OUTPUT_BYTES != 4096
#error caller-provided LJ_WRITE_MAX_OUTPUT_BYTES must not be redefined
#endif
#if LJ_TRACK_WORKSPACE_USAGE != 1
#error caller-provided LJ_TRACK_WORKSPACE_USAGE must not be redefined
#endif
#endif

#if defined(TEST_DISABLE_SHORT_NAMES)
#if defined(LJ_STATUS_OK)
#error short constant aliases must be disabled
#endif
#if defined(LJ_FIELD_I64)
#error short field aliases must be disabled
#endif
#endif

typedef struct config_item {
  char id[16];
  lonejson_int64 count;
  bool ok;
} config_item;

typedef struct config_reader_state {
  const char *json;
  size_t offset;
} config_reader_state;

static const lonejson_field config_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(config_item, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64(config_item, count, "count"),
    LONEJSON_FIELD_BOOL(config_item, ok, "ok")};
LONEJSON_MAP_DEFINE(config_item_map, config_item, config_item_fields);

static lonejson_read_result config_reader(void *user, unsigned char *buffer,
                                          size_t capacity) {
  config_reader_state *state;
  lonejson_read_result result;
  size_t remaining;
  size_t copy_len;

  state = (config_reader_state *)user;
  result = lonejson_default_read_result();
  remaining = strlen(state->json + state->offset);
  if (remaining == 0u) {
    result.eof = 1;
    return result;
  }
  copy_len = remaining < capacity ? remaining : capacity;
  memcpy(buffer, state->json + state->offset, copy_len);
  state->offset += copy_len;
  result.bytes_read = copy_len;
  return result;
}

static int config_expect_item(const config_item *item) {
  return strcmp(item->id, "cfg") == 0 && item->count == 7 && item->ok;
}

static lonejson *config_runtime(void) {
  static lonejson *runtime = NULL;

  if (runtime == NULL) {
    runtime = lonejson_new(NULL, NULL);
  }
  return runtime;
}

static int config_test_parse_and_serialize(void) {
  static const char json[] = "{\"id\":\"cfg\",\"count\":7,\"ok\":true}";
  config_item item;
  lonejson_error error;
  lonejson_status status;
  char out[128];

  lonejson_error_init(&error);
  lonejson_init(config_runtime(), &config_item_map, &item);
  status = lonejson_parse_cstr(config_runtime(), &config_item_map, &item, json,
                               &error);
  if (status != LONEJSON_STATUS_OK || !config_expect_item(&item)) {
    lonejson_cleanup(&config_item_map, &item);
    return 1;
  }
  status = lonejson_serialize_buffer(config_runtime(), &config_item_map, &item,
                                     out, sizeof(out), NULL, &error);
  lonejson_cleanup(&config_item_map, &item);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  return strcmp(out, json) == 0 ? 0 : 1;
}

static int config_test_array_stream(void) {
  config_reader_state state;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  config_item item;

  state.json =
      "{\"meta\":1,\"items\":[{\"id\":\"cfg\",\"count\":7,\"ok\":true}]}";
  state.offset = 0u;
  lonejson_error_init(&error);
  stream = lonejson_array_stream_open_reader(config_runtime(), "items",
                                             config_reader, &state, &error);
  if (stream == NULL) {
    return 1;
  }
  lonejson_init(config_runtime(), &config_item_map, &item);
  if (stream->set_record_reuse(stream, 1, &error) != LONEJSON_STATUS_OK) {
    lonejson_array_stream_close(stream);
    lonejson_cleanup(&config_item_map, &item);
    return 1;
  }
  if (stream->error_fn(stream)->code != LONEJSON_STATUS_OK) {
    stream->close(stream);
    lonejson_cleanup(&config_item_map, &item);
    return 1;
  }
  result = lonejson_array_stream_next(stream, &config_item_map, &item, &error);
  if (result != LONEJSON_ARRAY_STREAM_ITEM || !config_expect_item(&item)) {
    lonejson_cleanup(&config_item_map, &item);
    lonejson_array_stream_close(stream);
    return 1;
  }
  result = lonejson_array_stream_next(stream, &config_item_map, &item, &error);
  lonejson_cleanup(&config_item_map, &item);
  lonejson_array_stream_close(stream);
  return result == LONEJSON_ARRAY_STREAM_EOF ? 0 : 1;
}

#if !defined(TEST_OMIT_PROTOCOL_FRAMING)
static int config_test_protocol_framing(void) {
  static const char multipart_body[] = "--cfg\r\n\r\nbody\r\n--cfg--\r\n";
  lonejson_sse *sse;
  lonejson_multipart *multipart;
  lonejson_multipart_handler handler;
  lonejson_error error;
  lonejson_status status;

  lonejson_error_init(&error);
  sse = lonejson_sse_open(NULL, &error);
  if (sse == NULL) {
    return 1;
  }
  status = sse->push(sse, "data: ok\n\n", 10u, NULL, NULL, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = sse->finish(sse, NULL, NULL, &error);
  }
  sse->close(sse);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }

  memset(&handler, 0, sizeof(handler));
  multipart = lonejson_multipart_open("multipart/form-data; boundary=cfg", NULL,
                                      &error);
  if (multipart == NULL) {
    return 1;
  }
  status = multipart->push(multipart, multipart_body,
                           sizeof(multipart_body) - 1u, &handler, NULL, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = multipart->finish(multipart, &handler, NULL, &error);
  }
  multipart->close(multipart);
  return status == LONEJSON_STATUS_OK ? 0 : 1;
}
#endif

#if defined(TEST_USE_SHORT_NAMES)
static int config_test_short_aliases(void) {
  lj_error error;
  char out[128];
  config_item item;
  lj_status status;
  lonejson *runtime = lj_new(NULL, NULL);
  int ok;

  lj_error_init(&error);
  if (runtime == NULL) {
    return 1;
  }
  lj_init(runtime, &config_item_map, &item);
  strcpy(item.id, "cfg");
  item.count = 7;
  item.ok = true;
  status = lj_serialize_buffer(runtime, &config_item_map, &item, out,
                               sizeof(out), NULL, &error);
  lj_cleanup(&config_item_map, &item);
  ok = status == LJ_STATUS_OK &&
       strcmp(out, "{\"id\":\"cfg\",\"count\":7,\"ok\":true}") == 0;
  lj_free(runtime);
  return ok ? 0 : 1;
}
#endif

int main(void) {
#if defined(TEST_OMIT_PROTOCOL_FRAMING)
  if (config_runtime()->sse_push_json != NULL ||
      config_runtime()->sse_finish_json != NULL ||
      config_runtime()->sse_push_json_value != NULL ||
      config_runtime()->sse_finish_json_value != NULL) {
    return 1;
  }
#endif
  if (config_test_parse_and_serialize() != 0) {
    return 1;
  }
  if (config_test_array_stream() != 0) {
    return 1;
  }
#if !defined(TEST_OMIT_PROTOCOL_FRAMING)
  if (config_test_protocol_framing() != 0) {
    return 1;
  }
#endif
#if defined(TEST_USE_SHORT_NAMES)
  if (config_test_short_aliases() != 0) {
    return 1;
  }
#endif
  return 0;
}
