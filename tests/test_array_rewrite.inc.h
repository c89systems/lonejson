typedef struct test_array_rewrite_seen {
  size_t count;
  size_t appended;
  lonejson_int64 only_parent_id;
  test_item replacement;
  test_item insert;
  test_item append;
} test_array_rewrite_seen;

typedef struct test_array_rewrite_board_ctx {
  lonejson_int64 id;
} test_array_rewrite_board_ctx;

typedef struct test_array_rewrite_repeated_parent_seen {
  size_t count;
} test_array_rewrite_repeated_parent_seen;

static lonejson_status test_array_rewrite_repeated_parent_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error);

typedef struct test_array_rewrite_owned_item {
  char *name;
} test_array_rewrite_owned_item;

typedef struct test_array_rewrite_owned_parent {
  char *name;
} test_array_rewrite_owned_parent;

typedef struct test_array_rewrite_owned_required_item {
  char *name;
  lonejson_int64 id;
} test_array_rewrite_owned_required_item;

typedef struct test_array_rewrite_owned_required_parent {
  char *name;
  lonejson_int64 id;
} test_array_rewrite_owned_required_parent;

static const lonejson_field test_array_rewrite_board_ctx_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_array_rewrite_board_ctx, id, "id")};
LONEJSON_MAP_DEFINE(test_array_rewrite_board_ctx_map,
                    test_array_rewrite_board_ctx,
                    test_array_rewrite_board_ctx_fields);

static const lonejson_field test_array_rewrite_owned_item_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_array_rewrite_owned_item, name,
                                    "name")};
LONEJSON_MAP_DEFINE(test_array_rewrite_owned_item_map,
                    test_array_rewrite_owned_item,
                    test_array_rewrite_owned_item_fields);

static const lonejson_field test_array_rewrite_owned_parent_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_array_rewrite_owned_parent, name,
                                    "name")};
LONEJSON_MAP_DEFINE(test_array_rewrite_owned_parent_map,
                    test_array_rewrite_owned_parent,
                    test_array_rewrite_owned_parent_fields);

static const lonejson_field test_array_rewrite_owned_required_item_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_array_rewrite_owned_required_item,
                                    name, "name"),
    LONEJSON_FIELD_I64_REQ(test_array_rewrite_owned_required_item, id, "id")};
LONEJSON_MAP_DEFINE(test_array_rewrite_owned_required_item_map,
                    test_array_rewrite_owned_required_item,
                    test_array_rewrite_owned_required_item_fields);

static const lonejson_field test_array_rewrite_owned_required_parent_fields[] =
    {LONEJSON_FIELD_STRING_ALLOC_REQ(test_array_rewrite_owned_required_parent,
                                     name, "name"),
     LONEJSON_FIELD_I64_REQ(test_array_rewrite_owned_required_parent, id,
                            "id")};
LONEJSON_MAP_DEFINE(test_array_rewrite_owned_required_parent_map,
                    test_array_rewrite_owned_required_parent,
                    test_array_rewrite_owned_required_parent_fields);

static lonejson_status test_array_rewrite_items(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_array_rewrite_seen *seen = (test_array_rewrite_seen *)user;
  test_item *it = (test_item *)item;
  (void)error;

  seen->count++;
  if (seen->only_parent_id != 0 && context->parent_count != 0u) {
    const test_array_rewrite_board_ctx *board =
        (const test_array_rewrite_board_ctx *)context->parents[0].dst;
    if (board->id != seen->only_parent_id) {
      return LONEJSON_STATUS_OK;
    }
  }
  if (it->id == 1) {
    result->action = LONEJSON_ARRAY_REWRITE_DROP;
  } else if (it->id == 2) {
    result->action = LONEJSON_ARRAY_REWRITE_REPLACE;
    result->replacement.map = &test_item_map;
    result->replacement.src = &seen->replacement;
  } else if (it->id == 3) {
    result->action = LONEJSON_ARRAY_REWRITE_INSERT_AFTER;
    result->insert.map = &test_item_map;
    result->insert.src = &seen->insert;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_array_rewrite_append(void *user,
                          const lonejson_array_rewrite_context *context,
                          lonejson_array_rewrite_emit_fn emit, void *emit_user,
                          lonejson_error *error) {
  test_array_rewrite_seen *seen = (test_array_rewrite_seen *)user;
  lonejson_array_rewrite_source source;

  if (seen->only_parent_id != 0 && context->parent_count != 0u) {
    const test_array_rewrite_board_ctx *board =
        (const test_array_rewrite_board_ctx *)context->parents[0].dst;
    if (board->id != seen->only_parent_id) {
      return LONEJSON_STATUS_OK;
    }
  }

  memset(&source, 0, sizeof(source));
  source.map = &test_item_map;
  source.src = &seen->append;
  seen->appended++;
  return emit(emit_user, &source, error);
}

static void test_array_rewrite_root_array_mapped_actions(void) {
  static const char json[] =
      "[{\"id\":1,\"label\":\"one\"},{\"id\":2,\"label\":\"two\"},"
      "{\"id\":3,\"label\":\"three\"}]";
  test_reader_state reader = {json, 0u, 7u};
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  memset(&item, 0, sizeof(item));
  seen.replacement.id = 20;
  strcpy(seen.replacement.label, "twenty");
  seen.insert.id = 30;
  strcpy(seen.insert.label, "thirty");
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;

  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out,
                "[{\"id\":20,\"label\":\"twenty\"},{\"id\":3,"
                "\"label\":\"three\"},{\"id\":30,\"label\":\"thirty\"},"
                "{\"id\":99,\"label\":\"tail\"}]") == 0);
  EXPECT(seen.count == 3u);
  EXPECT(seen.appended == 1u);
}

static void test_array_rewrite_direct_and_nested_arrays(void) {
  static const char json[] =
      "{\"version\":1,\"boards\":[{\"id\":7,\"items\":[{\"id\":1,"
      "\"label\":\"one\"},{\"id\":2,\"label\":\"two\"}]},{\"id\":8,"
      "\"items\":[{\"id\":3,\"label\":\"three\"}]}],\"done\":[]}";
  test_reader_state reader = {json, 0u, 11u};
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[512];

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  memset(&item, 0, sizeof(item));
  seen.replacement.id = 20;
  strcpy(seen.replacement.label, "twenty");
  seen.insert.id = 30;
  strcpy(seen.insert.label, "thirty");
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;

  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out,
                "{\"version\":1,\"boards\":[{\"id\":7,\"items\":[{\"id\":20,"
                "\"label\":\"twenty\"},{\"id\":99,\"label\":\"tail\"}]},"
                "{\"id\":8,\"items\":[{\"id\":3,\"label\":\"three\"},"
                "{\"id\":30,\"label\":\"thirty\"},{\"id\":99,"
                "\"label\":\"tail\"}]}],\"done\":[]}") == 0);
  EXPECT(seen.count == 3u);
  EXPECT(seen.appended == 2u);
}

static void test_array_rewrite_parent_context_filters_nested_array(void) {
  static const char json[] =
      "{\"boards\":[{\"id\":7,\"items\":[{\"id\":1,\"label\":\"one\"},"
      "{\"id\":2,\"label\":\"two\"}]},"
      "{\"id\":8,\"items\":[{\"id\":1,\"label\":\"other\"}]}]}";
  static const char expected[] =
      "{\"boards\":[{\"id\":7,\"items\":[{\"id\":20,\"label\":\"twenty\"},"
      "{\"id\":99,\"label\":\"tail\"}]},{\"id\":8,\"items\":[{\"id\":1,"
      "\"label\":\"other\"}]}]}";
  char input_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-parent-in-XXXXXX";
  char output_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-parent-out-XXXXXX";
  test_reader_state reader = {json, 0u, 9u};
  test_array_rewrite_seen seen;
  test_item item;
  test_array_rewrite_board_ctx board;
  lonejson_array_rewrite_parent parents[1];
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[512];
  char file_out[512];
  FILE *fp;
  size_t n;
  int fd;

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  memset(&item, 0, sizeof(item));
  memset(&board, 0, sizeof(board));
  memset(parents, 0, sizeof(parents));
  seen.only_parent_id = 7;
  seen.replacement.id = 20;
  strcpy(seen.replacement.label, "twenty");
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;

  parents[0].segment = "boards";
  parents[0].map = &test_array_rewrite_board_ctx_map;
  parents[0].dst = &board;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.parents = parents;
  options.parent_count = 1u;
  options.item = test_array_rewrite_items;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out, expected) == 0);
  EXPECT(seen.count == 3u);
  EXPECT(seen.appended == 1u);

  fd = write_temp_text_file(input_path, json);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);
  fd = mkstemp(output_path);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
  } else {
    unlink(input_path);
    return;
  }
  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  memset(&board, 0, sizeof(board));
  seen.only_parent_id = 7;
  seen.replacement.id = 20;
  strcpy(seen.replacement.label, "twenty");
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  EXPECT(test_array_rewrite_path("boards[].items", input_path, output_path,
                                 NULL, &options, &error) == LONEJSON_STATUS_OK);
  fp = fopen(output_path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    n = fread(file_out, 1u, sizeof(file_out) - 1u, fp);
    file_out[n] = '\0';
    fclose(fp);
    EXPECT(strcmp(file_out, expected) == 0);
  }
  EXPECT(seen.count == 3u);
  EXPECT(seen.appended == 1u);
  unlink(input_path);
  unlink(output_path);
}

static void test_array_rewrite_repeated_parent_segments(void) {
  static const char json[] =
      "{\"nodes\":[{\"id\":1,\"nodes\":[{\"id\":2,\"items\":[{\"id\":3,"
      "\"label\":\"three\"}]}]}]}";
  test_reader_state reader = {json, 0u, 8u};
  test_array_rewrite_repeated_parent_seen seen;
  test_item item;
  test_array_rewrite_board_ctx outer;
  test_array_rewrite_board_ctx inner;
  lonejson_array_rewrite_parent parents[2];
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];

  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  memset(&outer, 0, sizeof(outer));
  memset(&inner, 0, sizeof(inner));
  memset(parents, 0, sizeof(parents));
  memset(&options, 0, sizeof(options));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;

  parents[0].segment = "nodes";
  parents[0].map = &test_array_rewrite_board_ctx_map;
  parents[0].dst = &outer;
  parents[1].segment = "nodes";
  parents[1].map = &test_array_rewrite_board_ctx_map;
  parents[1].dst = &inner;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.parents = parents;
  options.parent_count = 2u;
  options.item = test_array_rewrite_repeated_parent_item;
  options.user = &seen;

  EXPECT(test_array_rewrite_reader("nodes[].nodes[].items", test_state_reader,
                                   &reader, test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out, json) == 0);
  EXPECT(seen.count == 1u);
}

static lonejson_status test_array_rewrite_value_items(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  lonejson_json_value *value = (lonejson_json_value *)item;
  lonejson_json_value *replacement = (lonejson_json_value *)user;
  (void)context;
  (void)error;
  if (value->len == 4u && memcmp(value->json, "null", 4u) == 0) {
    result->action = LONEJSON_ARRAY_REWRITE_REPLACE;
    result->replacement.json = replacement;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_count_value_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  size_t *count = (size_t *)user;
  (void)context;
  (void)item;
  (void)result;
  (void)error;
  (*count)++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_repeated_parent_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_array_rewrite_repeated_parent_seen *seen =
      (test_array_rewrite_repeated_parent_seen *)user;
  const test_array_rewrite_board_ctx *outer;
  const test_array_rewrite_board_ctx *inner;
  test_item *it = (test_item *)item;
  (void)result;
  (void)error;

  EXPECT(context->parent_count == 2u);
  outer = (const test_array_rewrite_board_ctx *)context->parents[0].dst;
  inner = (const test_array_rewrite_board_ctx *)context->parents[1].dst;
  EXPECT(outer->id == 1);
  EXPECT(inner->id == 2);
  EXPECT(it->id == 3);
  seen->count++;
  return LONEJSON_STATUS_OK;
}

static void *test_fail_malloc(void *ctx, size_t size) {
  (void)ctx;
  (void)size;
  return NULL;
}

static void *test_passthrough_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  return realloc(ptr, size);
}

static void test_passthrough_free(void *ctx, void *ptr) {
  (void)ctx;
  free(ptr);
}

static void test_array_rewrite_json_value_items_and_failures(void) {
  static const char json[] = "{\"items\":[{\"keep\":true},null,[1,2]]}";
  static const char duplicate[] = "{\"items\":[{\"x\":1,\"x\":2}]}";
  static const char missing[] = "{\"other\":[]}";
  test_reader_state reader;
  lonejson_json_value item_value;
  lonejson_json_value replacement;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  lonejson_error error;
  test_buffer_sink sink;
  test_failing_sink failing_sink;
  unsigned char out[256];

  lonejson_json_value_init(test_default_runtime(), &item_value);
  lonejson_json_value_init(test_default_runtime(), &replacement);
  EXPECT(lonejson_json_value_set_buffer(&replacement, "{\"replaced\":true}",
                                        17u, &error) == LONEJSON_STATUS_OK);
  memset(&options, 0, sizeof(options));
  options.item_value = &item_value;
  options.item = test_array_rewrite_value_items;
  options.user = &replacement;
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 5u;
  EXPECT(test_array_rewrite_reader("items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out,
                "{\"items\":[{\"keep\":true},{\"replaced\":true},[1,2]]}") ==
         0);

  reader.json = duplicate;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "items", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_DUPLICATE_FIELD);

  parse_options = lonejson__default_parse_options();
  parse_options.reject_duplicate_keys = 0;
  reader.json = duplicate;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "items", test_state_reader, &reader, test_buffer_sink_write, &sink,
             &parse_options, &options, &error) == LONEJSON_STATUS_OK);

  reader.json = missing;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "items", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_TYPE_MISMATCH);

  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  failing_sink.total = 0u;
  failing_sink.fail_after = 10u;
  EXPECT(test_array_rewrite_reader("items", test_state_reader, &reader,
                                   test_failing_sink_write, &failing_sink, NULL,
                                   &options,
                                   &error) == LONEJSON_STATUS_CALLBACK_FAILED);

  lonejson_json_value_cleanup(&replacement);
  lonejson_json_value_cleanup(&item_value);
}

static lonejson_status test_array_rewrite_fail_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)user;
  (void)context;
  (void)item;
  (void)result;
  (void)error;
  return LONEJSON_STATUS_CALLBACK_FAILED;
}

static lonejson_status test_array_rewrite_bad_action_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)user;
  (void)context;
  (void)item;
  (void)error;
  result->action = (lonejson_array_rewrite_action)99;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_invalid_source_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)user;
  (void)context;
  (void)item;
  (void)error;
  result->action = LONEJSON_ARRAY_REWRITE_REPLACE;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_nonrewindable_json_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)context;
  (void)item;
  (void)error;
  result->action = LONEJSON_ARRAY_REWRITE_REPLACE;
  result->replacement.json = (lonejson_json_value *)user;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_array_rewrite_invalid_append(void *user,
                                  const lonejson_array_rewrite_context *context,
                                  lonejson_array_rewrite_emit_fn emit,
                                  void *emit_user, lonejson_error *error) {
  lonejson_array_rewrite_source source;
  (void)user;
  (void)context;
  memset(&source, 0, sizeof(source));
  return emit(emit_user, &source, error);
}

static lonejson_status test_array_rewrite_drop_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_array_rewrite_seen *seen = (test_array_rewrite_seen *)user;
  (void)context;
  (void)item;
  (void)error;
  seen->count++;
  result->action = LONEJSON_ARRAY_REWRITE_DROP;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_keep_count_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  size_t *count = (size_t *)user;
  (void)context;
  (void)item;
  (void)result;
  (void)error;
  (*count)++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_truncated_item_callback(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)user;
  (void)context;
  (void)item;
  (void)result;
  (void)error;
  return LONEJSON_STATUS_TRUNCATED;
}

static lonejson_status test_array_rewrite_count_owned_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  size_t *count = (size_t *)user;
  test_array_rewrite_owned_item *owned = (test_array_rewrite_owned_item *)item;
  (void)context;
  (void)result;
  (void)error;
  EXPECT(owned->name != NULL);
  (*count)++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_assert_owned_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_array_rewrite_owned_item *owned = (test_array_rewrite_owned_item *)item;
  size_t *count = (size_t *)user;

  (void)context;
  (void)result;
  (void)error;
  EXPECT(owned != NULL);
  EXPECT(owned->name != NULL);
  if (owned != NULL && owned->name != NULL) {
    if (*count == 0u) {
      EXPECT(strcmp(owned->name, "alpha") == 0);
    } else if (*count == 1u) {
      EXPECT(strcmp(owned->name, "beta") == 0);
    }
  }
  (*count)++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_fail_owned_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_array_rewrite_owned_item *owned = (test_array_rewrite_owned_item *)item;
  (void)user;
  (void)context;
  (void)result;
  (void)error;
  EXPECT(owned->name != NULL);
  return LONEJSON_STATUS_CALLBACK_FAILED;
}

static lonejson_status test_array_rewrite_bad_action_owned_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_array_rewrite_owned_item *owned = (test_array_rewrite_owned_item *)item;
  (void)user;
  (void)context;
  (void)error;
  EXPECT(owned->name != NULL);
  result->action = (lonejson_array_rewrite_action)99;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_owned_parent_append(
    void *user, const lonejson_array_rewrite_context *context,
    lonejson_array_rewrite_emit_fn emit, void *emit_user,
    lonejson_error *error) {
  size_t *count = (size_t *)user;
  (void)emit;
  (void)emit_user;
  (void)error;
  if (context->parent_count != 0u) {
    const test_array_rewrite_owned_parent *parent =
        (const test_array_rewrite_owned_parent *)context->parents[0].dst;
    EXPECT(parent->name != NULL);
    (*count)++;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_rewrite_spool_replace_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  test_item *it = (test_item *)item;
  test_spool_doc *replacement = (test_spool_doc *)user;
  (void)context;
  (void)error;
  if (it->id == 1) {
    result->action = LONEJSON_ARRAY_REWRITE_REPLACE;
    result->replacement.map = &test_spool_doc_map;
    result->replacement.src = replacement;
  }
  return LONEJSON_STATUS_OK;
}

typedef struct test_array_rewrite_count_sink {
  size_t bytes;
} test_array_rewrite_count_sink;

static lonejson_status
test_array_rewrite_count_sink_write(void *user, const void *data, size_t len,
                                    lonejson_error *error) {
  test_array_rewrite_count_sink *sink = (test_array_rewrite_count_sink *)user;
  (void)data;
  (void)error;
  sink->bytes += len;
  return LONEJSON_STATUS_OK;
}

static void test_array_rewrite_callback_and_action_failures(void) {
  static const char json[] = "[{\"id\":1,\"label\":\"one\"}]";
  test_reader_state reader;
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[128];

  memset(&options, 0, sizeof(options));
  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_fail_item;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_CALLBACK_FAILED);

  options.item = test_array_rewrite_bad_action_item;
  reader.offset = 0u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_array_rewrite_argument_and_selector_failures(void) {
  static const char json[] = "[]";
  const char *bad_selectors[] = {"items.", "[].items", "items[]"};
  test_reader_state reader = {json, 0u, 64u};
  test_item item;
  lonejson_array_rewrite_parent parents[1];
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  lonejson_error error;
  test_buffer_sink sink;
  test_allocator_state alloc_state;
  unsigned char out[32];
  size_t i;

  memset(&options, 0, sizeof(options));
  memset(&item, 0, sizeof(item));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  EXPECT(test_array_rewrite_reader("", NULL, &reader, test_buffer_sink_write,
                                   &sink, NULL, &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader, NULL, &sink,
                                   NULL, &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL, NULL,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  options.item = test_array_rewrite_items;
  options.item_map = &test_item_map;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  options.item_dst = &item;
  options.item_value = (lonejson_json_value *)&item;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  options.parent_count = 1u;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(parents, 0, sizeof(parents));
  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  options.parents = parents;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  alloc_state.allocator.realloc_fn = NULL;
  alloc_state.allocator.free_fn = NULL;
  parse_options.allocator = &alloc_state.allocator;
  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink,
                                   &parse_options, &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  for (i = 0u; i < sizeof(bad_selectors) / sizeof(bad_selectors[0]); ++i) {
    reader.offset = 0u;
    EXPECT(test_array_rewrite_reader(bad_selectors[i], test_state_reader,
                                     &reader, test_buffer_sink_write, &sink,
                                     NULL, &options, &error) ==
           LONEJSON_STATUS_INVALID_ARGUMENT);
  }
}

static void test_array_rewrite_parse_type_and_reader_failures(void) {
  static const char malformed[] = "{\"items\":[{\"id\":1,\"label\":\"one\"}";
  static const char non_array[] = "{\"items\":{\"id\":1}}";
  static const char reader_prefix[] =
      "{\"items\":[{\"id\":1,\"label\":\"one\"}]}";
  test_reader_state reader;
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];
  lonejson_status status;

  memset(&options, 0, sizeof(options));
  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  options.user = &seen;

  reader.json = malformed;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader(
             "items", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_JSON);

  reader.json = non_array;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "items", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_TYPE_MISMATCH);

  reader.json = reader_prefix;
  reader.offset = 0u;
  reader.chunk_size = 12u;
  sink.length = 0u;
  status = test_array_rewrite_reader("items", test_error_after_prefix_reader,
                                     &reader, test_buffer_sink_write, &sink,
                                     NULL, &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.system_errno == EIO);
}

static void test_array_rewrite_string_capture_allocation_failures(void) {
  static const char json[] =
      "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]";
  char input_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-string-in-XXXXXX";
  char output_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-string-out-XXXXXX";
  test_realloc_limit_state limit = {64u, 0u};
  test_reader_state reader = {json, 0u, 64u};
  lonejson_json_value item_value;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  lonejson_allocator allocator;
  lonejson_error error;
  test_buffer_sink sink;
  size_t count;
  unsigned char out[128];
  int fd;

  lonejson_json_value_init(test_default_runtime(), &item_value);
  allocator = lonejson_default_allocator();
  allocator.ctx = &limit;
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &allocator;
  memset(&options, 0, sizeof(options));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  count = 0u;
  options.item_value = &item_value;
  options.item = test_array_rewrite_count_value_item;
  options.user = &count;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink,
                                   &parse_options, &options, &error) ==
         LONEJSON_STATUS_ALLOCATION_FAILED);
  EXPECT(count == 0u);

  fd = write_temp_text_file(input_path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    fd = mkstemp(output_path);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      close(fd);
      count = 0u;
      EXPECT(test_array_rewrite_path("", input_path, output_path,
                                     &parse_options, &options, &error) ==
             LONEJSON_STATUS_ALLOCATION_FAILED);
      EXPECT(count == 0u);
    }
    unlink(input_path);
    unlink(output_path);
  }
  lonejson_json_value_cleanup(&item_value);
}

static void test_array_rewrite_reader_uses_configured_allocator(void) {
  static const char json[] = "[1]";
  test_reader_state reader = {json, 0u, 1u};
  test_allocator_state alloc_state;
  lonejson_json_value item_value;
  lonejson__parse_options parse_options;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  size_t count;
  unsigned char out[32];

  test_allocator_init(&alloc_state);
  lonejson_json_value_init(test_default_runtime(), &item_value);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;
  memset(&options, 0, sizeof(options));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  count = 0u;
  options.item_value = &item_value;
  options.item = test_array_rewrite_count_value_item;
  options.user = &count;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             &parse_options, &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out, "[1]") == 0);
  EXPECT(count == 1u);
  EXPECT(alloc_state.stats.alloc_calls != 0u);
  EXPECT(alloc_state.stats.bytes_live == 0u);
  lonejson_json_value_cleanup(&item_value);
}

static void test_array_rewrite_reader_allocator_failure(void) {
  static const char json[] = "[1]";
  test_reader_state reader = {json, 0u, 1u};
  lonejson_allocator allocator;
  lonejson_json_value item_value;
  lonejson__parse_options parse_options;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  size_t count;
  unsigned char out[32];

  lonejson_json_value_init(test_default_runtime(), &item_value);
  allocator = lonejson_default_allocator();
  allocator.malloc_fn = test_fail_malloc;
  allocator.realloc_fn = test_passthrough_realloc;
  allocator.free_fn = test_passthrough_free;
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &allocator;
  memset(&options, 0, sizeof(options));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  count = 0u;
  options.item_value = &item_value;
  options.item = test_array_rewrite_count_value_item;
  options.user = &count;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink,
                                   &parse_options, &options, &error) ==
         LONEJSON_STATUS_ALLOCATION_FAILED);
  EXPECT(count == 0u);
  lonejson_json_value_cleanup(&item_value);
}

static void test_array_rewrite_item_parse_and_limit_failures(void) {
  static const char missing_required[] = "[{\"label\":\"one\"}]";
  static const char too_deep[] = "[[[[]]]]";
  test_reader_state reader;
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[128];

  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  options.user = &seen;

  reader.json = missing_required;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);

  parse_options = lonejson__default_parse_options();
  parse_options.max_depth = 2u;
  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  options.user = &seen;
  reader.json = too_deep;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             &parse_options, &options, &error) == LONEJSON_STATUS_OVERFLOW);
}

static void test_array_rewrite_runtime_json_value_limits(void) {
  static const char json[] = "[\"ab\"]";
  lonejson_config config;
  lonejson *limited_lj;
  lonejson *default_lj;
  test_reader_state reader;
  test_array_rewrite_seen seen;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[128];
  lonejson_status status;

  config = lonejson_default_config();
  config.json_value_max_string_bytes = 1u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  default_lj = lonejson_new(NULL, &error);
  EXPECT(default_lj != NULL);
  if (limited_lj == NULL || default_lj == NULL) {
    lonejson_free(limited_lj);
    lonejson_free(default_lj);
    return;
  }

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  memset(out, 0, sizeof(out));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  seen.append.id = 9;
  strcpy(seen.append.label, "tail");
  options.append = test_array_rewrite_append;
  options.user = &seen;

  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = lonejson_array_rewrite_reader(limited_lj, "", test_state_reader,
                                         &reader, test_buffer_sink_write, &sink,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  reader.offset = 0u;
  sink.length = 0u;
  status = lonejson_array_rewrite_reader(default_lj, "", test_state_reader,
                                         &reader, test_buffer_sink_write, &sink,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "[\"ab\",{\"id\":9,\"label\":\"tail\"}]") ==
         0);

  lonejson_free(limited_lj);
  lonejson_free(default_lj);
}

static void test_array_rewrite_source_and_parent_failures(void) {
  static const char json[] = "[{\"id\":1,\"label\":\"one\"}]";
  static const char nested_missing_parent[] =
      "{\"boards\":[{\"items\":[{\"id\":1,\"label\":\"one\"}]}]}";
  static const char replacement_json[] = "{\"id\":2,\"label\":\"two\"}";
  test_reader_state reader;
  test_reader_state replacement_reader;
  test_array_rewrite_seen seen;
  test_item item;
  test_array_rewrite_board_ctx board;
  lonejson_array_rewrite_parent parents[1];
  lonejson_array_rewrite_options options;
  lonejson_json_value nonrewindable;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];

  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  memset(&options, 0, sizeof(options));
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_invalid_source_item;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  lonejson_json_value_init(test_default_runtime(), &nonrewindable);
  replacement_reader.json = replacement_json;
  replacement_reader.offset = 0u;
  replacement_reader.chunk_size = 64u;
  EXPECT(lonejson_json_value_set_reader(&nonrewindable, test_state_reader,
                                        &replacement_reader,
                                        &error) == LONEJSON_STATUS_OK);
  reader.offset = 0u;
  sink.length = 0u;
  options.item = test_array_rewrite_nonrewindable_json_item;
  options.user = &nonrewindable;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_json_value_cleanup(&nonrewindable);

  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_invalid_append;
  reader.offset = 0u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(parents, 0, sizeof(parents));
  memset(&options, 0, sizeof(options));
  reader.json = "{\"boards\":[{\"id\":7,\"items\":[]}]}";
  reader.offset = 0u;
  reader.chunk_size = 64u;
  parents[0].segment = "boards";
  options.parents = parents;
  options.parent_count = 1u;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&board, 0, sizeof(board));
  parents[0].map = &test_array_rewrite_board_ctx_map;
  parents[0].dst = &board;
  reader.json = nested_missing_parent;
  reader.offset = 0u;
  sink.length = 0u;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
}

static void test_array_rewrite_append_only_and_object_path(void) {
  static const char json[] =
      "{\"meta\":{\"items\":[{\"id\":4,\"label\":\"keep\"}]}}";
  test_reader_state reader = {json, 0u, 8u};
  test_array_rewrite_seen seen;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_reader("meta.items", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out,
                "{\"meta\":{\"items\":[{\"id\":4,\"label\":\"keep\"},"
                "{\"id\":99,\"label\":\"tail\"}]}}") == 0);
  EXPECT(seen.count == 0u);
  EXPECT(seen.appended == 1u);
}

static void test_array_rewrite_empty_all_drop_and_all_append(void) {
  static const char empty[] = "[]";
  static const char all_drop[] =
      "[{\"id\":1,\"label\":\"one\"},{\"id\":2,\"label\":\"two\"}]";
  test_reader_state reader;
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  options.append = test_array_rewrite_append;
  options.user = &seen;
  reader.json = empty;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out, "[{\"id\":99,\"label\":\"tail\"}]") == 0);
  EXPECT(seen.appended == 1u);

  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  sink.length = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_drop_item;
  options.user = &seen;
  reader.json = all_drop;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out, "[]") == 0);
  EXPECT(seen.count == 2u);
}

static void test_array_rewrite_append_only_root_bounded_workspace(void) {
  static const char json[] = "[]";
  test_reader_state reader = {json, 0u, 64u};
  test_array_rewrite_seen seen;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  test_allocator_state alloc_state;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[128];

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             &parse_options, &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)out, "[{\"id\":99,\"label\":\"tail\"}]") == 0);
  EXPECT(alloc_state.stats.peak_bytes_live > 0u);
  EXPECT(alloc_state.stats.peak_bytes_live < 16u * 1024u);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_rewrite_spooled_replacement_item(void) {
  static const char json[] = "[{\"id\":1,\"label\":\"one\"}]";
  static const char large_text[] =
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
  test_reader_state reader = {json, 0u, 64u};
  test_spool_doc replacement;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];

  memset(&replacement, 0, sizeof(replacement));
  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  strcpy(replacement.id, "spooled");
  lonejson_spooled_init_with_allocator(&replacement.text,
                                       &test_spool_small_options, NULL);
  lonejson_spooled_init_with_allocator(&replacement.bytes,
                                       &test_spool_small_options, NULL);
  EXPECT(lonejson_spooled_append(&replacement.text, large_text,
                                 strlen(large_text),
                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_spilled(&replacement.text) != 0);

  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_spool_replace_item;
  options.user = &replacement;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_buffer_sink_write, &sink, NULL,
                                   &options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strstr((char *)out, "\"id\":\"spooled\"") != NULL);
  EXPECT(strstr((char *)out, large_text) != NULL);

  lonejson_spooled_cleanup(&replacement.bytes);
  lonejson_spooled_cleanup(&replacement.text);
}

static void test_array_rewrite_large_array_memory_bound(void) {
  static char json[65536];
  char *p;
  size_t remaining;
  size_t i;
  size_t n;
  test_reader_state reader;
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  test_allocator_state alloc_state;
  lonejson_error error;
  test_array_rewrite_count_sink sink;

  p = json;
  remaining = sizeof(json);
  n = (size_t)snprintf(p, remaining, "{\"items\":[");
  p += n;
  remaining -= n;
  for (i = 0u; i < 1000u; ++i) {
    n = (size_t)snprintf(p, remaining, "%s{\"id\":%lu,\"label\":\"x\"}",
                         i == 0u ? "" : ",", (unsigned long)(i + 10u));
    p += n;
    remaining -= n;
  }
  n = (size_t)snprintf(p, remaining, "]}");
  p += n;
  remaining -= n;
  EXPECT(remaining != 0u);

  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  options.user = &seen;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 13u;
  EXPECT(test_array_rewrite_reader("items", test_state_reader, &reader,
                                   test_array_rewrite_count_sink_write, &sink,
                                   &parse_options, &options,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(seen.count == 1000u);
  EXPECT(sink.bytes > 10000u);
  EXPECT(alloc_state.stats.peak_bytes_live < 65536u);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_rewrite_owned_item_cleanup_between_items(void) {
  static char json[32768];
  char *p;
  size_t remaining;
  size_t i;
  size_t n;
  size_t count;
  test_reader_state reader;
  test_array_rewrite_owned_item item;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  test_allocator_state alloc_state;
  lonejson_error error;
  test_array_rewrite_count_sink sink;

  p = json;
  remaining = sizeof(json);
  n = (size_t)snprintf(p, remaining, "[");
  p += n;
  remaining -= n;
  for (i = 0u; i < 200u; ++i) {
    n = (size_t)snprintf(p, remaining, "%s{\"name\":\"owned-item-%lu\"}",
                         i == 0u ? "" : ",", (unsigned long)i);
    p += n;
    remaining -= n;
  }
  n = (size_t)snprintf(p, remaining, "]");
  p += n;
  remaining -= n;
  EXPECT(remaining != 0u);

  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  count = 0u;
  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;
  options.item_map = &test_array_rewrite_owned_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_count_owned_item;
  options.user = &count;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 17u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_array_rewrite_count_sink_write, &sink,
                                   &parse_options, &options,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(count == 200u);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_rewrite_initializes_owned_item_destination(void) {
  static const char json[] = "[{\"name\":\"alpha\"},{\"name\":\"beta\"}]";
  test_reader_state reader;
  test_array_rewrite_owned_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_array_rewrite_count_sink sink;
  size_t count;

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  count = 0u;
  item.name = NULL;
  options.item_map = &test_array_rewrite_owned_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_assert_owned_item;
  options.user = &count;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 5u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_array_rewrite_count_sink_write, &sink,
                                   NULL, &options,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(count == 2u);
  EXPECT(sink.bytes == strlen(json));
}

static void test_array_rewrite_truncation_status_is_preserved(void) {
  static const char json[] =
      "[{\"id\":4,\"label\":\"this-label-is-too-long\"}]";
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char out[256];
  test_reader_state reader;
  size_t count;
  lonejson_json_value value;

  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  count = 0u;
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_keep_count_item;
  options.user = &count;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 17u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_TRUNCATED);
  EXPECT(error.truncated);
  EXPECT(error.code == LONEJSON_STATUS_TRUNCATED);
  EXPECT(count == 1u);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "[{\"id\":4,\"label\":\"this-label-is-too-long\"}]") == 0);

  lonejson_json_value_init(test_default_runtime(), &value);
  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.item_value = &value;
  options.item = test_array_rewrite_truncated_item_callback;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader, test_buffer_sink_write, &sink,
             NULL, &options, &error) == LONEJSON_STATUS_TRUNCATED);
  lonejson_json_value_cleanup(&value);
}

static void test_array_rewrite_owned_parent_cleanup_between_parents(void) {
  static char json[32768];
  char *p;
  size_t remaining;
  size_t i;
  size_t n;
  size_t count;
  test_reader_state reader;
  test_array_rewrite_owned_parent parent;
  lonejson_array_rewrite_parent parents[1];
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  test_allocator_state alloc_state;
  lonejson_error error;
  test_array_rewrite_count_sink sink;

  p = json;
  remaining = sizeof(json);
  n = (size_t)snprintf(p, remaining, "{\"boards\":[");
  p += n;
  remaining -= n;
  for (i = 0u; i < 200u; ++i) {
    n = (size_t)snprintf(p, remaining,
                         "%s{\"name\":\"owned-parent-%lu\",\"items\":[]}",
                         i == 0u ? "" : ",", (unsigned long)i);
    p += n;
    remaining -= n;
  }
  n = (size_t)snprintf(p, remaining, "]}");
  p += n;
  remaining -= n;
  EXPECT(remaining != 0u);

  memset(&parent, 0, sizeof(parent));
  memset(parents, 0, sizeof(parents));
  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  count = 0u;
  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;
  parents[0].segment = "boards";
  parents[0].map = &test_array_rewrite_owned_parent_map;
  parents[0].dst = &parent;
  options.parents = parents;
  options.parent_count = 1u;
  options.append = test_array_rewrite_owned_parent_append;
  options.user = &count;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 19u;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_array_rewrite_count_sink_write, &sink,
                                   &parse_options, &options,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(count == 200u);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_rewrite_owned_item_cleanup_on_failures(void) {
  static const char partial[] = "[{\"name\":\"partial\"}]";
  static const char json[] = "[{\"name\":\"owned\"}]";
  test_reader_state reader;
  test_array_rewrite_owned_required_item required_item;
  test_array_rewrite_owned_item item;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  test_allocator_state alloc_state;
  lonejson_error error;
  test_failing_sink failing_sink;
  test_array_rewrite_count_sink count_sink;

  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;

  memset(&required_item, 0, sizeof(required_item));
  memset(&options, 0, sizeof(options));
  memset(&count_sink, 0, sizeof(count_sink));
  options.item_map = &test_array_rewrite_owned_required_item_map;
  options.item_dst = &required_item;
  options.item = test_array_rewrite_fail_owned_item;
  reader.json = partial;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader(
             "", test_state_reader, &reader,
             test_array_rewrite_count_sink_write, &count_sink, &parse_options,
             &options, &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(alloc_state.stats.bytes_live == 0u);

  memset(&item, 0, sizeof(item));
  memset(&options, 0, sizeof(options));
  memset(&count_sink, 0, sizeof(count_sink));
  options.item_map = &test_array_rewrite_owned_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_fail_owned_item;
  reader.json = json;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_array_rewrite_count_sink_write,
                                   &count_sink, &parse_options, &options,
                                   &error) == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(alloc_state.stats.bytes_live == 0u);

  memset(&item, 0, sizeof(item));
  options.item = test_array_rewrite_bad_action_owned_item;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_array_rewrite_count_sink_write,
                                   &count_sink, &parse_options, &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(alloc_state.stats.bytes_live == 0u);

  memset(&item, 0, sizeof(item));
  options.item = test_array_rewrite_count_owned_item;
  options.user = &count_sink.bytes;
  failing_sink.total = 0u;
  failing_sink.fail_after = 2u;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("", test_state_reader, &reader,
                                   test_failing_sink_write, &failing_sink,
                                   &parse_options, &options,
                                   &error) == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_rewrite_owned_parent_cleanup_on_failures(void) {
  static const char missing_required[] =
      "{\"boards\":[{\"name\":\"partial\",\"items\":[]}]}";
  static const char sink_failure[] =
      "{\"boards\":[{\"name\":\"active\",\"items\":[]}]}";
  test_reader_state reader;
  test_array_rewrite_owned_required_parent required_parent;
  test_array_rewrite_owned_parent parent;
  lonejson_array_rewrite_parent parents[1];
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  test_allocator_state alloc_state;
  lonejson_error error;
  test_array_rewrite_count_sink count_sink;
  test_failing_sink failing_sink;

  test_allocator_init(&alloc_state);
  parse_options = lonejson__default_parse_options();
  parse_options.allocator = &alloc_state.allocator;

  memset(&required_parent, 0, sizeof(required_parent));
  memset(parents, 0, sizeof(parents));
  memset(&options, 0, sizeof(options));
  memset(&count_sink, 0, sizeof(count_sink));
  parents[0].segment = "boards";
  parents[0].map = &test_array_rewrite_owned_required_parent_map;
  parents[0].dst = &required_parent;
  options.parents = parents;
  options.parent_count = 1u;
  options.append = test_array_rewrite_owned_parent_append;
  options.user = &count_sink.bytes;
  reader.json = missing_required;
  reader.offset = 0u;
  reader.chunk_size = 64u;
  EXPECT(test_array_rewrite_reader(
             "boards[].items", test_state_reader, &reader,
             test_array_rewrite_count_sink_write, &count_sink, &parse_options,
             &options, &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(alloc_state.stats.bytes_live == 0u);

  memset(&parent, 0, sizeof(parent));
  memset(parents, 0, sizeof(parents));
  memset(&options, 0, sizeof(options));
  parents[0].segment = "boards";
  parents[0].map = &test_array_rewrite_owned_parent_map;
  parents[0].dst = &parent;
  options.parents = parents;
  options.parent_count = 1u;
  options.append = test_array_rewrite_owned_parent_append;
  options.user = &count_sink.bytes;
  failing_sink.total = 0u;
  failing_sink.fail_after = 2u;
  reader.json = sink_failure;
  reader.offset = 0u;
  EXPECT(test_array_rewrite_reader("boards[].items", test_state_reader, &reader,
                                   test_failing_sink_write, &failing_sink,
                                   &parse_options, &options,
                                   &error) == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_rewrite_path_helper(void) {
  char input_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-array-rewrite-in-XXXXXX";
  char output_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-array-rewrite-out-XXXXXX";
  char same_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-array-rewrite-same-XXXXXX";
  const char *json = "[{\"id\":1,\"label\":\"one\"}]";
  test_array_rewrite_seen seen;
  test_item item;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  FILE *fp;
  char buffer[128];
  size_t n;
  int fd;

  fd = write_temp_text_file(input_path, json);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);
  fd = mkstemp(output_path);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
  }

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  options.item_map = &test_item_map;
  options.item_dst = &item;
  options.item = test_array_rewrite_items;
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_path("", input_path, output_path, NULL, &options,
                                 &error) == LONEJSON_STATUS_OK);
  fp = fopen(output_path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    n = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    buffer[n] = '\0';
    fclose(fp);
    EXPECT(strcmp(buffer, "[{\"id\":99,\"label\":\"tail\"}]") == 0);
  }

  fd = write_temp_text_file(same_path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    EXPECT(test_array_rewrite_path("", same_path, same_path, NULL, &options,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
    fp = fopen(same_path, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      n = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
      buffer[n] = '\0';
      fclose(fp);
      EXPECT(strcmp(buffer, json) == 0);
    }
    unlink(same_path);
  }
  unlink(input_path);
  unlink(output_path);
}

static void test_array_rewrite_helper_failures(void) {
  static const char json[] = "[]";
  char output_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-helper-out-XXXXXX";
  test_reader_state reader = {json, 0u, 64u};
  test_array_rewrite_seen seen;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  FILE *fp;
  int fd;

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  options.append = test_array_rewrite_append;
  options.user = &seen;

  EXPECT(test_array_rewrite_reader_to_filep("", test_state_reader, &reader,
                                            NULL, NULL, &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_reader_to_fd("", test_state_reader, &reader, -1,
                                         NULL, &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_filep("", NULL, test_array_rewrite_count_sink_write,
                                  &seen, NULL, &options,
                                  &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_filep_to_filep("", NULL, NULL, NULL, &options,
                                           &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_fd("", -1, test_array_rewrite_count_sink_write,
                               &seen, NULL, &options,
                               &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_fd_to_fd("", -1, -1, NULL, &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(test_array_rewrite_path(
             "",
             LONEJSON_SOURCE_DIR
             "/build/test-tmp/does-not-exist-lonejson-array-rewrite",
             output_path, NULL, &options, &error) == LONEJSON_STATUS_IO_ERROR);
  EXPECT(test_array_rewrite_path("", NULL, output_path, NULL, &options,
                                 &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  fd = mkstemp(output_path);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    reader.offset = 0u;
    EXPECT(test_array_rewrite_reader_to_fd("", test_state_reader, &reader, fd,
                                           NULL, &options,
                                           &error) == LONEJSON_STATUS_IO_ERROR);
    unlink(output_path);
  }

  fp = tmpfile();
  EXPECT(fp != NULL);
  if (fp != NULL) {
    reader.offset = 0u;
    EXPECT(test_array_rewrite_reader_to_filep("", test_state_reader, &reader,
                                              fp, NULL, &options,
                                              &error) == LONEJSON_STATUS_OK);
    fclose(fp);
  }
}

static void
test_array_rewrite_path_rejects_invalid_runtime_without_truncating(void) {
  char input_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-invalid-in-XXXXXX";
  char output_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-invalid-out-XXXXXX";
  const char *input_json = "[1,2,3]";
  const char *sentinel = "keep me";
  test_array_rewrite_seen seen;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  FILE *fp;
  char buffer[64];
  size_t n;
  int fd;

  fd = write_temp_text_file(input_path, input_json);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);

  fd = write_temp_text_file(output_path, sentinel);
  EXPECT(fd >= 0);
  if (fd < 0) {
    unlink(input_path);
    return;
  }
  close(fd);

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(lonejson_array_rewrite_path(NULL, "", input_path, output_path,
                                     &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  fp = fopen(output_path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    n = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    buffer[n] = '\0';
    fclose(fp);
    EXPECT(strcmp(buffer, sentinel) == 0);
  }

  unlink(input_path);
  unlink(output_path);
}

static void test_array_rewrite_fd_to_fd_helper(void) {
  char input_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-array-rewrite-fd-in-XXXXXX";
  char output_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-array-rewrite-fd-out-XXXXXX";
  const char *json = "[{\"id\":1,\"label\":\"one\"}]";
  test_array_rewrite_seen seen;
  lonejson_array_rewrite_options options;
  lonejson_error error;
  FILE *fp;
  char buffer[128];
  size_t n;
  int input_fd;
  int output_fd;

  input_fd = write_temp_text_file(input_path, json);
  EXPECT(input_fd >= 0);
  output_fd = mkstemp(output_path);
  EXPECT(output_fd >= 0);
  if (input_fd < 0 || output_fd < 0) {
    if (input_fd >= 0) {
      close(input_fd);
      unlink(input_path);
    }
    if (output_fd >= 0) {
      close(output_fd);
      unlink(output_path);
    }
    return;
  }
  EXPECT(lseek(input_fd, 0, SEEK_SET) == 0);

  memset(&seen, 0, sizeof(seen));
  memset(&options, 0, sizeof(options));
  seen.append.id = 99;
  strcpy(seen.append.label, "tail");
  options.append = test_array_rewrite_append;
  options.user = &seen;
  EXPECT(test_array_rewrite_fd_to_fd("", input_fd, output_fd, NULL, &options,
                                     &error) == LONEJSON_STATUS_OK);
  close(input_fd);
  close(output_fd);

  fp = fopen(output_path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    n = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    buffer[n] = '\0';
    fclose(fp);
    EXPECT(strcmp(buffer, "[{\"id\":1,\"label\":\"one\"},{\"id\":99,"
                          "\"label\":\"tail\"}]") == 0);
  }
  unlink(input_path);
  unlink(output_path);
}
