static void test_file_and_buffer_helpers(void) {
  const char *json =
      "{\"name\":\"Cara\",\"nickname\":\"cj\",\"age\":1,\"score\":1.5,"
      "\"active\":true,\"address\":{\"city\":\"Lund\",\"zip\":88},\"lucky_"
      "numbers\":[],\"tags\":[],\"items\":[]}";
  static const char expected_pretty[] = "{\n"
                                        "  \"name\": \"Cara\",\n"
                                        "  \"nickname\": \"cj\",\n"
                                        "  \"age\": 1,\n"
                                        "  \"score\": 1.5,\n"
                                        "  \"active\": true,\n"
                                        "  \"address\": {\n"
                                        "    \"city\": \"Lund\",\n"
                                        "    \"zip\": 88\n"
                                        "  },\n"
                                        "  \"lucky_numbers\": [],\n"
                                        "  \"tags\": [],\n"
                                        "  \"items\": []\n"
                                        "}";
  test_person person;
  char path[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-test-XXXXXX";
  int fd;
  FILE *fp;
  char out[64];
  char compact[512];
  char pretty[512];
  char tiny[8];
  size_t needed = 0;
  lonejson_error error;
  lonejson_config config;
  lonejson *truncate_lj = NULL;
  lonejson *pretty_lj = NULL;
  lonejson_status status;

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  EXPECT(write(fd, json, strlen(json)) == (ssize_t)strlen(json));
  close(fd);

  status = (lonejson_parse_path)(test_default_runtime(), &test_person_map,
                                 &person, path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.address.city, "Lund") == 0);

  status = (lonejson_serialize_buffer)(test_default_runtime(), &test_person_map,
                                       &person, compact, sizeof(compact),
                                       &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(compact, json) == 0);
  EXPECT(needed == strlen(json));

  status =
      (lonejson_serialize_buffer)(test_default_runtime(), &test_person_map,
                                  &person, tiny, sizeof(tiny), &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(tiny[sizeof(tiny) - 1u] == '\0');

  config = lonejson_default_config();
  config.write_overflow_policy = LONEJSON_OVERFLOW_TRUNCATE;
  truncate_lj = lonejson_new(&config, &error);
  EXPECT(truncate_lj != NULL);
  status = (lonejson_serialize_buffer)(truncate_lj, &test_person_map, &person,
                                       out, sizeof(out), &needed, &error);
  EXPECT(status == LONEJSON_STATUS_TRUNCATED || status == LONEJSON_STATUS_OK);
  EXPECT(needed > strlen(out));

  config = lonejson_default_config();
  config.write_pretty = 1;
  pretty_lj = lonejson_new(&config, &error);
  EXPECT(pretty_lj != NULL);
  status = (lonejson_serialize_buffer)(pretty_lj, &test_person_map, &person,
                                       pretty, sizeof(pretty), &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pretty, expected_pretty) == 0);

  fp = fopen(path, "wb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    status = (lonejson_serialize_filep)(test_default_runtime(),
                                        &test_person_map, &person, fp, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    fclose(fp);
  }

  lonejson_free(pretty_lj);
  lonejson_free(truncate_lj);
  unlink(path);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_jsonl_helpers(void) {
  static const char expected_jsonl[] = "{\"id\":\"evt-1\",\"ok\":true}\n"
                                       "{\"id\":\"evt-2\",\"ok\":false}\n";
  static const char expected_pretty_jsonl[] = "{\n"
                                              "  \"id\": \"evt-1\",\n"
                                              "  \"ok\": true\n"
                                              "}\n"
                                              "{\n"
                                              "  \"id\": \"evt-2\",\n"
                                              "  \"ok\": false\n"
                                              "}\n";
  test_event events[2];
  lonejson_error error;
  size_t needed = 0u;
  char buffer[128];
  char pretty[256];
  char *jsonl;
  lonejson_status status;
  char path[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-jsonl-XXXXXX";
  int fd;
  FILE *fp;
  char filebuf[128];
  size_t filelen;
  lonejson_config config;
  lonejson *pretty_lj = NULL;
  lonejson *alloc_lj = NULL;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  status =
      (lonejson_serialize_jsonl_buffer)(test_default_runtime(), &test_event_map,
                                        events, 2u, 0u, buffer, sizeof(buffer),
                                        &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, expected_jsonl) == 0);
  EXPECT(needed == strlen(buffer));

  config = lonejson_default_config();
  config.write_pretty = 1;
  pretty_lj = lonejson_new(&config, &error);
  EXPECT(pretty_lj != NULL);
  status = (lonejson_serialize_jsonl_buffer)(pretty_lj, &test_event_map, events,
                                             2u, 0u, pretty, sizeof(pretty),
                                             &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pretty, expected_pretty_jsonl) == 0);
  EXPECT(needed == strlen(pretty));

  alloc_lj = lonejson_new(NULL, &error);
  EXPECT(alloc_lj != NULL);
  jsonl = (lonejson_serialize_jsonl_alloc)(alloc_lj, &test_event_map, events,
                                           2u, 0u, NULL, &error);
  EXPECT(jsonl != NULL);
  if (jsonl != NULL) {
    EXPECT(strcmp(jsonl, buffer) == 0);
    LONEJSON_FREE(jsonl);
  }

  status =
      (lonejson_serialize_jsonl_buffer)(test_default_runtime(), &test_event_map,
                                        NULL, 0u, 0u, buffer, sizeof(buffer),
                                        &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "") == 0);
  EXPECT(needed == 0u);

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  status =
      (lonejson_serialize_jsonl_filep)(test_default_runtime(), &test_event_map,
                                       events, 2u, 0u, fp, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  fclose(fp);

  fp = fopen(path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    filelen = fread(filebuf, 1u, sizeof(filebuf) - 1u, fp);
    filebuf[filelen] = '\0';
    fclose(fp);
    EXPECT(strcmp(filebuf, expected_jsonl) == 0);
  }
  lonejson_free(pretty_lj);
  lonejson_free(alloc_lj);
  unlink(path);
}

static void test_fixed_capacity_object_array(void) {
  const char *json =
      "{\"name\":\"Dee\",\"nickname\":\"dee\",\"age\":1,\"score\":2.0,"
      "\"active\":false,\"address\":{\"city\":\"Malmo\",\"zip\":1},\"lucky_"
      "numbers\":[],\"tags\":[],\"items\":[{\"id\":1,\"label\":\"one\"},{"
      "\"id\":2,\"label\":\"two\"}]}";
  test_person person;
  test_item items_buf[2];
  lonejson_error error;
  lonejson_status status;
  lonejson_config config;
  lonejson *merge_lj;

  memset(&person, 0, sizeof(person));
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  merge_lj = lonejson_new(&config, &error);
  EXPECT(merge_lj != NULL);
  lonejson_init(merge_lj, &test_person_map, &person);
  memset(items_buf, 0, sizeof(items_buf));
  person.items.items = items_buf;
  person.items.capacity = 2;
  person.items.elem_size = sizeof(items_buf[0]);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  status =
      (lonejson_parse_cstr)(merge_lj, &test_person_map, &person, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.items.count == 2);
  EXPECT(person.items.items == items_buf);
  EXPECT(items_buf[0].id == 1);
  EXPECT(items_buf[1].id == 2);

  lonejson_cleanup(&test_person_map, &person);
  lonejson_free(merge_lj);
}

typedef struct test_recursive_node {
  char name[16];
  lonejson_object_array children;
} test_recursive_node;

static const lonejson_map test_recursive_node_map;

static const lonejson_field test_recursive_node_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_recursive_node, name, "name",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY(test_recursive_node, children, "children",
                                test_recursive_node, &test_recursive_node_map,
                                LONEJSON_OVERFLOW_FAIL),
};

LONEJSON_MAP_DEFINE(test_recursive_node_map, test_recursive_node,
                    test_recursive_node_fields);

typedef struct test_recursive_meta {
  char tag[16];
  lonejson_object_array descendants;
} test_recursive_meta;

typedef struct test_recursive_envelope {
  char name[16];
  test_recursive_meta meta;
} test_recursive_envelope;

static const lonejson_map test_recursive_meta_map;
static const lonejson_map test_recursive_envelope_map;

static const lonejson_field test_recursive_meta_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_recursive_meta, tag, "tag",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY(test_recursive_meta, descendants, "descendants",
                                test_recursive_envelope,
                                &test_recursive_envelope_map,
                                LONEJSON_OVERFLOW_FAIL),
};

static const lonejson_field test_recursive_envelope_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_recursive_envelope, name, "name",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT(test_recursive_envelope, meta, "meta",
                          &test_recursive_meta_map),
};

LONEJSON_MAP_DEFINE(test_recursive_meta_map, test_recursive_meta,
                    test_recursive_meta_fields);
LONEJSON_MAP_DEFINE(test_recursive_envelope_map, test_recursive_envelope,
                    test_recursive_envelope_fields);

static void test_recursive_schema_analysis_accepts_self_cycles(void) {
  const char *json =
      "{\"name\":\"root\",\"children\":[{\"name\":\"child\",\"children\":[{"
      "\"name\":\"leaf\",\"children\":[]}]}]}";
  test_recursive_node root;
  test_recursive_node root2;
  lonejson_error error;
  lonejson_status status;

  memset(&root, 0, sizeof(root));
  memset(&root2, 0, sizeof(root2));
  status = lonejson_parse_cstr(test_default_runtime(), &test_recursive_node_map,
                               &root, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(root.name, "root") == 0);
  EXPECT(root.children.count == 1u);
  if (root.children.count == 1u) {
    test_recursive_node *child = (test_recursive_node *)root.children.items;
    EXPECT(strcmp(child[0].name, "child") == 0);
    EXPECT(child[0].children.count == 1u);
    if (child[0].children.count == 1u) {
      test_recursive_node *leaf =
          (test_recursive_node *)child[0].children.items;
      EXPECT(strcmp(leaf[0].name, "leaf") == 0);
      EXPECT(leaf[0].children.count == 0u);
    }
  }
  status = lonejson_parse_cstr(test_default_runtime(), &test_recursive_node_map,
                               &root2, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(root2.children.count == 1u);

  lonejson_cleanup(&test_recursive_node_map, &root);
  lonejson_cleanup(&test_recursive_node_map, &root2);
}

static void test_recursive_schema_analysis_accepts_mutual_cycles(void) {
  const char *json =
      "{\"name\":\"root\",\"meta\":{\"tag\":\"meta\",\"descendants\":[{"
      "\"name\":"
      "\"child\",\"meta\":{\"tag\":\"child-meta\",\"descendants\":[]}}]}}";
  test_recursive_envelope root;
  test_recursive_envelope root2;
  lonejson_error error;
  lonejson_status status;

  memset(&root, 0, sizeof(root));
  memset(&root2, 0, sizeof(root2));
  status =
      lonejson_parse_cstr(test_default_runtime(), &test_recursive_envelope_map,
                          &root, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(root.name, "root") == 0);
  EXPECT(strcmp(root.meta.tag, "meta") == 0);
  EXPECT(root.meta.descendants.count == 1u);
  if (root.meta.descendants.count == 1u) {
    test_recursive_envelope *child =
        (test_recursive_envelope *)root.meta.descendants.items;
    EXPECT(strcmp(child[0].name, "child") == 0);
    EXPECT(strcmp(child[0].meta.tag, "child-meta") == 0);
    EXPECT(child[0].meta.descendants.count == 0u);
  }
  status =
      lonejson_parse_cstr(test_default_runtime(), &test_recursive_envelope_map,
                          &root2, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(root2.meta.descendants.count == 1u);

  lonejson_cleanup(&test_recursive_envelope_map, &root);
  lonejson_cleanup(&test_recursive_envelope_map, &root2);
}

static void test_formatting_variants_and_roundtrip(void) {
  const char *json =
      "{\n"
      "\t\"name\" : \"E\\u006dma\",\n"
      "\t\"nickname\" : \"em\",\n"
      "\t\"age\" : -12,\n"
      "\t\"score\" : 1.25e2,\n"
      "\t\"active\" : false,\n"
      "\t\"address\" : {\n"
      "\t  \"city\" : \"G\\u00f6teborg\",\n"
      "\t  \"zip\" : 90210\n"
      "\t},\n"
      "\t\"lucky_numbers\" : [ 0, -1, 200 ],\n"
      "\t\"tags\" : [ \"line\\nfeed\", \"quote\\\"ok\" ],\n"
      "\t\"items\" : [ ],\n"
      "\t\"ignored\" : { \"deep\" : [1, {\"x\": [2,3,{\"y\":4}]}, 5] }\n"
      "}\n";
  test_person person;
  test_person roundtrip;
  lonejson_error error;
  lonejson_status status;
  char *serialized = NULL;
  lonejson *alloc_lj = lonejson_new(NULL, &error);

  EXPECT(alloc_lj != NULL);

  status = (lonejson_parse_cstr)(test_default_runtime(), &test_person_map,
                                 &person, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "Emma") == 0);
  EXPECT(strcmp(person.address.city, "Göteborg") == 0);
  EXPECT(person.age == -12);
  EXPECT(person.score > 124.9 && person.score < 125.1);
  EXPECT(person.tags.count == 2);
  EXPECT(strcmp(person.tags.items[0], "line\nfeed") == 0);
  EXPECT(strcmp(person.tags.items[1], "quote\"ok") == 0);

  serialized = (lonejson_serialize_alloc)(alloc_lj, &test_person_map, &person,
                                          NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    status = (lonejson_parse_cstr)(test_default_runtime(), &test_person_map,
                                   &roundtrip, serialized, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(roundtrip.name, "Emma") == 0);
    EXPECT(strcmp(roundtrip.address.city, "Göteborg") == 0);
    LONEJSON_FREE(serialized);
  }

  lonejson_free(alloc_lj);
  lonejson_cleanup(&test_person_map, &person);
  lonejson_cleanup(&test_person_map, &roundtrip);
}

static void test_duplicate_key_policy(void) {
  const char *json =
      "{\"name\":\"first\",\"name\":\"second\",\"address\":{\"city\":\"A\","
      "\"zip\":1},\"nickname\":\"n\",\"age\":1,\"score\":1,\"active\":true,"
      "\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_config config;
  lonejson *allow_dup_lj;
  lonejson_error error;
  lonejson_status status;

  status = (lonejson_parse_cstr)(test_default_runtime(), &test_person_map,
                                 &person, json, &error);
  EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
  lonejson_cleanup(&test_person_map, &person);

  poison_bytes(&person, sizeof(person), 0xC3u);
  config = lonejson_default_config();
  config.reject_duplicate_keys_by_default = 0;
  allow_dup_lj = lonejson_new(&config, &error);
  EXPECT(allow_dup_lj != NULL);
  status = (lonejson_parse_cstr)(allow_dup_lj, &test_person_map, &person, json,
                                 &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "second") == 0);
  lonejson_cleanup(&test_person_map, &person);
  lonejson_free(allow_dup_lj);
}

static void test_public_api_argument_and_serialization_guards(void) {
  static const char *json = "{\"name\":\"Alice\",\"age\":34}";
  static const char *json_with_nul =
      "{\"name\":\"a\\u0000b\",\"age\":34,\"active\":true,"
      "\"address\":{\"city\":\"X\",\"zip\":1},\"lucky_numbers\":[],"
      "\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;
  char *serialized;

  status = (lonejson_parse_cstr)(test_default_runtime(), &test_person_map, NULL,
                                 json, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  status = (lonejson_parse_cstr)(test_default_runtime(), &test_person_map,
                                 &person, json_with_nul, &error);
  EXPECT(status != LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_person_map, &person);

  lonejson_init(test_default_runtime(), &test_person_map, &person);
  person.name = "finite-check";
  person.score = strtod("nan", NULL);
  serialized =
      (lonejson_serialize_alloc)(test_default_runtime(), &test_person_map,
                                 &person, NULL, &error);
  EXPECT(serialized == NULL);
  person.score = strtod("inf", NULL);
  serialized =
      (lonejson_serialize_alloc)(test_default_runtime(), &test_person_map,
                                 &person, NULL, &error);
  EXPECT(serialized == NULL);
}

static void test_public_initializers_and_defaults(void) {
  static const unsigned char buffer_reader_data[] = "{}";
  lonejson_error error;
  lonejson_read_result read_result;
  lonejson_value_visitor visitor;
  lonejson_config config;
  lonejson_allocator allocator;
  lonejson__spool_options spool_options;
  lonejson__parse_options parse_options;
  lonejson__value_limits value_limits;
  lonejson__write_options write_options;
  lonejson_owned_buffer owned_buffer;
  lonejson_sse_options sse_options;
  lonejson_multipart_options multipart_options;
  lonejson_buffer_reader buffer_reader;
  lonejson_source source;
  lonejson_string_array_stream string_stream;
  lonejson_mapped_array_stream mapped_stream;
  lonejson_writer_value_stream writer_value_stream;
  test_json_value_doc doc;
  lonejson_json_value value;
  lonejson_spooled spool;

  poison_bytes(&error, sizeof(error), 0xA5u);
  lonejson_error_init(&error);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(error.line == 0u);
  EXPECT(error.column == 0u);
  EXPECT(error.offset == 0u);
  EXPECT(error.system_errno == 0);
  EXPECT(error.truncated == 0);
  EXPECT(error.message[0] == '\0');

  read_result = lonejson_default_read_result();
  if (sizeof(size_t) == 4u) {
    EXPECT(sizeof(read_result) == sizeof(size_t) + 4u * sizeof(int));
  } else if (sizeof(size_t) == 8u) {
    EXPECT(sizeof(read_result) == sizeof(size_t) + 4u * sizeof(int));
  }
  EXPECT(read_result.bytes_read == 0u);
  EXPECT(read_result.eof == 0);
  EXPECT(read_result.would_block == 0);
  EXPECT(read_result.error_code == 0);
  EXPECT(read_result.reserved == 0);

  visitor = lonejson_default_value_visitor();
  EXPECT(visitor.object_begin == NULL);
  EXPECT(visitor.object_end == NULL);
  EXPECT(visitor.object_key_begin == NULL);
  EXPECT(visitor.object_key_chunk == NULL);
  EXPECT(visitor.object_key_end == NULL);
  EXPECT(visitor.array_begin == NULL);
  EXPECT(visitor.array_end == NULL);
  EXPECT(visitor.string_begin == NULL);
  EXPECT(visitor.string_chunk == NULL);
  EXPECT(visitor.string_end == NULL);
  EXPECT(visitor.number_begin == NULL);
  EXPECT(visitor.number_chunk == NULL);
  EXPECT(visitor.number_end == NULL);
  EXPECT(visitor.boolean_value == NULL);
  EXPECT(visitor.null_value == NULL);

  allocator = lonejson_default_allocator();
  EXPECT(allocator.malloc_fn != NULL);
  EXPECT(allocator.realloc_fn != NULL);
  EXPECT(allocator.free_fn != NULL);
  EXPECT(allocator.ctx == NULL);
  EXPECT(allocator.stats == NULL);

  spool_options = lonejson__default_spool_options();
  EXPECT(spool_options.memory_limit == LONEJSON_SPOOL_MEMORY_LIMIT);
  EXPECT(spool_options.max_bytes == LONEJSON_SPOOL_MAX_BYTES);
  EXPECT(spool_options.temp_dir == NULL);

  parse_options = lonejson__default_parse_options();
  EXPECT(parse_options.clear_destination == 1);
  EXPECT(parse_options.reject_duplicate_keys == 1);
  EXPECT(parse_options.max_depth == 64u);
  EXPECT(parse_options.max_dynamic_string_bytes ==
         LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES);
  EXPECT(parse_options.max_alloc_bytes == LONEJSON_PARSE_MAX_ALLOC_BYTES);
  EXPECT(parse_options.fixed_string_scratch == NULL);
  EXPECT(parse_options.fixed_string_scratch_size == 0u);
  EXPECT(parse_options.allocator == NULL);

  value_limits = lonejson__default_value_limits();
  EXPECT(value_limits.max_depth == 64u);
  EXPECT(value_limits.max_string_bytes == 1024u * 1024u);
  EXPECT(value_limits.max_number_bytes == 256u);
  EXPECT(value_limits.max_key_bytes == 64u * 1024u);
  EXPECT(value_limits.max_total_bytes == 0u);

  write_options = lonejson__default_write_options();
  EXPECT(write_options.overflow_policy == LONEJSON_OVERFLOW_FAIL);
  EXPECT(write_options.pretty == 0);
  EXPECT(write_options.allocator == NULL);
  EXPECT(write_options.max_output_bytes == LONEJSON_WRITE_MAX_OUTPUT_BYTES);

  owned_buffer = lonejson_default_owned_buffer();
  EXPECT(owned_buffer.data == NULL);
  EXPECT(owned_buffer.len == 0u);
  EXPECT(owned_buffer.alloc_size == 0u);
  EXPECT(owned_buffer.allocator.malloc_fn != NULL);
  EXPECT(owned_buffer.allocator.realloc_fn != NULL);
  EXPECT(owned_buffer.allocator.free_fn != NULL);
  EXPECT(owned_buffer.allocator.ctx == NULL);
  EXPECT(owned_buffer.allocator.stats == NULL);

  sse_options = lonejson_default_sse_options();
  EXPECT(sse_options.max_line_bytes == 64u * 1024u);
  EXPECT(sse_options.max_event_data_bytes == 1024u * 1024u);
  EXPECT(sse_options.max_buffered_bytes == 1024u * 1024u);
  EXPECT(sse_options.allocator == NULL);

  multipart_options = lonejson_default_multipart_options();
  EXPECT(multipart_options.max_boundary_bytes == 200u);
  EXPECT(multipart_options.max_header_line_bytes == 64u * 1024u);
  EXPECT(multipart_options.max_header_count == 64u);
  EXPECT(multipart_options.max_part_buffered_bytes == 1024u * 1024u);
  EXPECT(multipart_options.allocator == NULL);

  config = lonejson_default_config();
  EXPECT(config.max_alloc_bytes == LONEJSON_PARSE_MAX_ALLOC_BYTES);
  EXPECT(config.max_dynamic_string_bytes ==
         LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES);
  EXPECT(config.max_depth == 64u);
  EXPECT(config.json_value_max_total_bytes == 0u);
  EXPECT(config.json_value_max_string_bytes == 1024u * 1024u);
  EXPECT(config.json_value_max_key_bytes == 64u * 1024u);
  EXPECT(config.json_value_max_number_bytes == 256u);
  EXPECT(config.json_value_max_depth == 64u);
  EXPECT(config.clear_destination_by_default == 1);
  EXPECT(config.reject_duplicate_keys_by_default == 1);
  EXPECT(config.write_overflow_policy == LONEJSON_OVERFLOW_FAIL);
  EXPECT(config.write_pretty == 0);
  EXPECT(config.write_max_output_bytes == LONEJSON_WRITE_MAX_OUTPUT_BYTES);
  EXPECT(config.spool_default.memory_limit == LONEJSON_SPOOL_MEMORY_LIMIT);
  EXPECT(config.spool_default.max_bytes == LONEJSON_SPOOL_MAX_BYTES);
  EXPECT(config.spool_default.temp_dir == NULL);

  poison_bytes(&buffer_reader, sizeof(buffer_reader), 0x11u);
  lonejson_buffer_reader_init(&buffer_reader, buffer_reader_data,
                              sizeof(buffer_reader_data) - 1u);
  EXPECT(buffer_reader.data == buffer_reader_data);
  EXPECT(buffer_reader.len == sizeof(buffer_reader_data) - 1u);
  EXPECT(buffer_reader.offset == 0u);

  poison_bytes(&source, sizeof(source), 0x22u);
  lonejson_source_init(&source);
  EXPECT(source.kind == LONEJSON_SOURCE_NONE);
  EXPECT(source.fp == NULL);
  EXPECT(source.fd == -1);
  EXPECT(source.path == NULL);
  EXPECT(source.cleanup == lonejson_source_cleanup);
  EXPECT(source.reset == lonejson_source_reset);
  EXPECT(source.set_file == lonejson_source_set_file);
  EXPECT(source.set_fd == lonejson_source_set_fd);
  EXPECT(source.set_path == lonejson_source_set_path);
  EXPECT(source.write_to_sink == lonejson_source_write_to_sink);
  EXPECT(source.is_rewindable == lonejson_source_is_rewindable);

  poison_bytes(&string_stream, sizeof(string_stream), 0x33u);
  lonejson_string_array_stream_init(&string_stream);
  EXPECT(string_stream.set_handler == lonejson_string_array_stream_set_handler);

  poison_bytes(&mapped_stream, sizeof(mapped_stream), 0x44u);
  lonejson_mapped_array_stream_init(&mapped_stream);
  EXPECT(mapped_stream.set_handler == lonejson_mapped_array_stream_set_handler);

  poison_bytes(&writer_value_stream, sizeof(writer_value_stream), 0x55u);
  lonejson_writer_value_stream_init(&writer_value_stream);
  EXPECT(writer_value_stream.open == lonejson_writer_value_stream_open);
  EXPECT(writer_value_stream.push == lonejson_writer_value_stream_push);
  EXPECT(writer_value_stream.close == lonejson_writer_value_stream_close);
  EXPECT(writer_value_stream.cleanup == lonejson_writer_value_stream_cleanup);

  poison_bytes(&owned_buffer, sizeof(owned_buffer), 0x66u);
  lonejson_owned_buffer_init(&owned_buffer);
  EXPECT(owned_buffer.data == NULL);
  EXPECT(owned_buffer.len == 0u);
  EXPECT(owned_buffer.alloc_size == 0u);
  EXPECT(owned_buffer.allocator.malloc_fn != NULL);
  lonejson_owned_buffer_free(&owned_buffer);

  EXPECT((int)LONEJSON_FIELD_KIND_STRING == 0);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_STREAM == 1);
  EXPECT((int)LONEJSON_FIELD_KIND_BASE64_STREAM == 2);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_SOURCE == 3);
  EXPECT((int)LONEJSON_FIELD_KIND_BASE64_SOURCE == 4);
  EXPECT((int)LONEJSON_FIELD_KIND_JSON_VALUE == 5);
  EXPECT((int)LONEJSON_FIELD_KIND_I64 == 6);
  EXPECT((int)LONEJSON_FIELD_KIND_U64 == 7);
  EXPECT((int)LONEJSON_FIELD_KIND_F64 == 8);
  EXPECT((int)LONEJSON_FIELD_KIND_BOOL == 9);
  EXPECT((int)LONEJSON_FIELD_KIND_OBJECT == 10);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_ARRAY == 11);
  EXPECT((int)LONEJSON_FIELD_KIND_I64_ARRAY == 12);
  EXPECT((int)LONEJSON_FIELD_KIND_U64_ARRAY == 13);
  EXPECT((int)LONEJSON_FIELD_KIND_F64_ARRAY == 14);
  EXPECT((int)LONEJSON_FIELD_KIND_BOOL_ARRAY == 15);
  EXPECT((int)LONEJSON_FIELD_KIND_OBJECT_ARRAY == 16);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM == 17);

  poison_bytes(&doc, sizeof(doc), 0x5Au);
  lonejson_init(test_default_runtime(), &test_json_value_doc_map, &doc);
  EXPECT(doc.id[0] == '\0');
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.selector.json == NULL);
  EXPECT(doc.selector.len == 0u);
  EXPECT(doc.selector.reader == NULL);
  EXPECT(doc.selector.reader_user == NULL);
  EXPECT(doc.selector.fp == NULL);
  EXPECT(doc.selector.fd == -1);
  EXPECT(doc.selector.path == NULL);
  EXPECT(doc.selector.parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE);
  EXPECT(doc.selector.parse_sink == NULL);
  EXPECT(doc.selector.parse_sink_user == NULL);
  EXPECT(doc.selector.parse_visitor == NULL);
  EXPECT(doc.selector.parse_visitor_user == NULL);

  poison_bytes(&value, sizeof(value), 0x33u);
  lonejson_json_value_init(NULL, &value);
  EXPECT(lonejson__json_value_is_initialized(&value));
  lonejson_json_value_cleanup(&value);

  poison_bytes(&spool, sizeof(spool), 0x44u);
  lonejson_spooled_init_class(NULL, &spool, LONEJSON_SPOOL_CLASS_BLOB);
  EXPECT(lonejson__spooled_is_initialized(&spool));
  lonejson_spooled_cleanup(&spool);
}

static void test_public_dynamic_record_helpers(void) {
  lonejson *runtime;
  lonejson *limited_runtime;
  lonejson_config config;
  lonejson_error error;
  test_person person;
  test_nullable_primitives_doc nullable_doc;
  test_two_alloc_string_doc doc;
  test_conditional_doc *arrays;
  test_item *item;
  char buffer[512];
  size_t needed = 0u;
  size_t first_alloc_size;
  lonejson_status status;

  runtime = lonejson_new(NULL, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  lonejson_init(runtime, &test_person_map, &person);
  EXPECT(!lonejson_field_has_presence(&test_person_map.fields[0]));

  status = runtime->record_assign_string(runtime, &test_person_map, &person,
                                         &test_person_map.fields[0], "Dynamic",
                                         7u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = runtime->record_assign_string(runtime, &test_person_map, &person,
                                         &test_person_map.fields[1], "dyn", 3u,
                                         &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  person.age = 12;
  person.score = 1.5;
  person.active = true;
  status = runtime->record_assign_string(
      runtime, &test_address_map, &person.address, &test_address_map.fields[0],
      "Visby", 5u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  person.address.zip = 62157;

  status = runtime->record_array_append_i64(runtime, &test_person_map, &person,
                                            &test_person_map.fields[6],
                                            &person.lucky_numbers, 7, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = runtime->record_array_append_string(
      runtime, &test_person_map, &person, &test_person_map.fields[7],
      &person.tags, "blue", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  item = (test_item *)runtime->record_object_array_append_slot(
      runtime, &test_person_map, &person, &test_person_map.fields[8],
      &person.items, &error);
  EXPECT(item != NULL);
  if (item != NULL) {
    item->id = 9;
    status = runtime->record_assign_string(runtime, &test_item_map, item,
                                           &test_item_map.fields[1], "nine", 4u,
                                           &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }

  status = lonejson_serialize_buffer(runtime, &test_person_map, &person, buffer,
                                     sizeof(buffer), &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(buffer, "\"name\":\"Dynamic\"") != NULL);
  EXPECT(strstr(buffer, "\"lucky_numbers\":[7]") != NULL);
  EXPECT(strstr(buffer, "\"tags\":[\"blue\"]") != NULL);
  EXPECT(strstr(buffer, "\"items\":[{\"id\":9,\"label\":\"nine\"}]") != NULL);

  lonejson_cleanup(&test_person_map, &person);

  lonejson_init(runtime, &test_nullable_primitives_map, &nullable_doc);
  nullable_doc.count = -12;
  nullable_doc.has_count = 1;
  nullable_doc.seed = 42u;
  nullable_doc.has_seed = 1;
  nullable_doc.ratio = 1.25;
  nullable_doc.has_ratio = 1;
  nullable_doc.enabled = true;
  nullable_doc.has_enabled = 1;
  nullable_doc.required_count = 9;
  nullable_doc.child.priority = 3;
  nullable_doc.child.has_priority = 1;

  status = runtime->record_assign_null(
      runtime, &test_nullable_primitives_map, &nullable_doc,
      &test_nullable_primitives_map.fields[0], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = runtime->record_assign_null(
      runtime, &test_nullable_primitives_map, &nullable_doc,
      &test_nullable_primitives_map.fields[1], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = runtime->record_assign_null(
      runtime, &test_nullable_primitives_map, &nullable_doc,
      &test_nullable_primitives_map.fields[2], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = runtime->record_assign_null(
      runtime, &test_nullable_primitives_map, &nullable_doc,
      &test_nullable_primitives_map.fields[3], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = runtime->record_assign_null(
      runtime, &test_nullable_child_map, &nullable_doc.child,
      &test_nullable_child_map.fields[0], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(nullable_doc.count == 0);
  EXPECT(nullable_doc.has_count == 0);
  EXPECT(nullable_doc.seed == 0u);
  EXPECT(nullable_doc.has_seed == 0);
  EXPECT(nullable_doc.ratio == 0.0);
  EXPECT(nullable_doc.has_ratio == 0);
  EXPECT(nullable_doc.enabled == false);
  EXPECT(nullable_doc.has_enabled == 0);
  EXPECT(nullable_doc.child.priority == 0);
  EXPECT(nullable_doc.child.has_priority == 0);

  status = lonejson_serialize_buffer(runtime, &test_nullable_primitives_map,
                                     &nullable_doc, buffer, sizeof(buffer),
                                     &needed, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"required_count\":9,\"child\":{}}") == 0);
  lonejson_cleanup(&test_nullable_primitives_map, &nullable_doc);

  arrays = (test_conditional_doc *)malloc(sizeof(*arrays));
  EXPECT(arrays != NULL);
  if (arrays != NULL) {
    runtime->init(runtime, &test_conditional_doc_map, arrays);
    EXPECT(runtime->record_array_append_u64(
               runtime, &test_conditional_doc_map, arrays,
               &test_conditional_doc_map.fields[5], &arrays->uints, 42u,
               &error) == LONEJSON_STATUS_OK);
    EXPECT(runtime->record_array_append_f64(
               runtime, &test_conditional_doc_map, arrays,
               &test_conditional_doc_map.fields[6], &arrays->nums, 1.5,
               &error) == LONEJSON_STATUS_OK);
    EXPECT(runtime->record_array_append_bool(
               runtime, &test_conditional_doc_map, arrays,
               &test_conditional_doc_map.fields[7], &arrays->bools, 1,
               &error) == LONEJSON_STATUS_OK);
    EXPECT(arrays->uints.count == 1u && arrays->uints.items[0] == 42u);
    EXPECT(arrays->nums.count == 1u && arrays->nums.items[0] == 1.5);
    EXPECT(arrays->bools.count == 1u && arrays->bools.items[0]);
    EXPECT(runtime->serialize_buffer(runtime, &test_conditional_doc_map, arrays,
                                     buffer, sizeof(buffer), &needed,
                                     &error) == LONEJSON_STATUS_OK);
    EXPECT(strstr(buffer, "\"uints\":[42]") != NULL);
    EXPECT(strstr(buffer, "\"nums\":[1.5]") != NULL);
    EXPECT(strstr(buffer, "\"bools\":[true]") != NULL);
    runtime->cleanup(runtime, &test_conditional_doc_map, arrays);
    free(arrays);
  }

  lonejson_free(runtime);

  runtime = lonejson_new(NULL, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  lonejson_init(runtime, &test_two_alloc_string_doc_map, &doc);
  status = runtime->record_assign_string(
      runtime, &test_two_alloc_string_doc_map, &doc,
      &test_two_alloc_string_doc_map.fields[0], "aaaaaaaa", 8u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.first != NULL);
  first_alloc_size = lonejson__owned_size(doc.first);
  EXPECT(first_alloc_size != 0u);
  lonejson_cleanup(&test_two_alloc_string_doc_map, &doc);
  lonejson_free(runtime);

  config = lonejson_default_config();
  config.max_alloc_bytes = first_alloc_size * 2u - 1u;
  limited_runtime = lonejson_new(&config, &error);
  EXPECT(limited_runtime != NULL);
  if (limited_runtime == NULL) {
    return;
  }
  lonejson_init(limited_runtime, &test_two_alloc_string_doc_map, &doc);
  status = limited_runtime->record_assign_string(
      limited_runtime, &test_two_alloc_string_doc_map, &doc,
      &test_two_alloc_string_doc_map.fields[0], "aaaaaaaa", 8u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.first != NULL);
  EXPECT(lonejson__owned_size(doc.first) == first_alloc_size);
  status = limited_runtime->record_assign_string(
      limited_runtime, &test_two_alloc_string_doc_map, &doc,
      &test_two_alloc_string_doc_map.fields[1], "bbbbbbbb", 8u, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "parse allocations exceed configured max bytes") != NULL);
  EXPECT(doc.first != NULL);
  EXPECT(strcmp(doc.first, "aaaaaaaa") == 0);
  EXPECT(doc.second == NULL);
  lonejson_cleanup(&test_two_alloc_string_doc_map, &doc);
  lonejson_free(limited_runtime);
}

static lonejson_status test_runtime_array_rewrite_keep_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)user;
  (void)context;
  (void)item;
  (void)result;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static void test_runtime_defaults_and_method_dispatch(void) {
  lonejson_config config;
  lonejson *lj;
  lonejson_error error;
  test_person person;
  lonejson_json_value value;
  lonejson_spooled spool;
  test_visit_chunk_state visit_state;
  lonejson_value_visitor visitor;
  test_buffer_sink sink;
  unsigned char sink_buffer[128];
  lonejson_value_rewrite_options value_rewrite_options;
  lonejson_array_rewrite_options array_rewrite_options;
  lonejson_buffer_reader array_reader;
  const char *json =
      "{\"name\":\"Ada\",\"nickname\":\"A\",\"age\":37,\"score\":1.25,"
      "\"active\":true,\"address\":{\"city\":\"Lund\",\"zip\":22222},"
      "\"lucky_numbers\":[7],\"tags\":[\"x\"],\"items\":[]}";
  const char *value_json = "{\"ok\":true}";
  const char *array_json = "[{\"id\":1,\"label\":\"one\"}]";
  test_item item;

  config = lonejson_default_config();
  EXPECT(config.max_alloc_bytes == LONEJSON_PARSE_MAX_ALLOC_BYTES);
  EXPECT(config.max_dynamic_string_bytes ==
         LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES);
  EXPECT(config.max_depth == 64u);
  EXPECT(config.json_value_max_total_bytes == 0u);
  EXPECT(config.json_value_max_string_bytes == 1024u * 1024u);
  EXPECT(config.json_value_max_key_bytes == 64u * 1024u);
  EXPECT(config.json_value_max_number_bytes == 256u);
  EXPECT(config.json_value_max_depth == 64u);
  EXPECT(config.clear_destination_by_default == 1);
  EXPECT(config.reject_duplicate_keys_by_default == 1);
  EXPECT(config.write_overflow_policy == LONEJSON_OVERFLOW_FAIL);
  EXPECT(config.write_pretty == 0);
  EXPECT(config.write_max_output_bytes == LONEJSON_WRITE_MAX_OUTPUT_BYTES);
  EXPECT(config.spool_default.memory_limit == LONEJSON_SPOOL_MEMORY_LIMIT);
  EXPECT(config.spool_default.max_bytes == LONEJSON_SPOOL_MAX_BYTES);
  EXPECT(config.spool_blob.memory_limit == config.spool_default.memory_limit);
  EXPECT(config.spool_blob.max_bytes == config.spool_default.max_bytes);
  EXPECT(config.spool_large_text.memory_limit ==
         config.spool_default.memory_limit);
  EXPECT(config.spool_large_text.max_bytes == config.spool_default.max_bytes);
  EXPECT(config.allocator == NULL);

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  EXPECT(lj->parse_cstr != NULL);
  EXPECT(lj->parse_buffer != NULL);
  EXPECT(lj->parse_reader != NULL);
  EXPECT(lj->parse_filep != NULL);
  EXPECT(lj->parse_path != NULL);
  EXPECT(lj->validate_buffer != NULL);
  EXPECT(lj->validate_cstr != NULL);
  EXPECT(lj->validate_reader != NULL);
  EXPECT(lj->validate_filep != NULL);
  EXPECT(lj->validate_path != NULL);
  EXPECT(lj->visit_value_buffer != NULL);
  EXPECT(lj->visit_value_cstr != NULL);
  EXPECT(lj->visit_value_reader != NULL);
  EXPECT(lj->visit_value_filep != NULL);
  EXPECT(lj->visit_value_path != NULL);
  EXPECT(lj->visit_value_fd != NULL);
  EXPECT(lj->visit_path_value_buffer != NULL);
  EXPECT(lj->visit_path_value_cstr != NULL);
  EXPECT(lj->visit_path_value_reader != NULL);
  EXPECT(lj->visit_path_value_filep != NULL);
  EXPECT(lj->visit_path_value_path != NULL);
  EXPECT(lj->visit_path_value_fd != NULL);
  EXPECT(lj->init != NULL);
  EXPECT(lj->reset != NULL);
  EXPECT(lj->stream_open_reader != NULL);
  EXPECT(lj->stream_open_filep != NULL);
  EXPECT(lj->stream_open_path != NULL);
  EXPECT(lj->stream_open_fd != NULL);
  EXPECT(lj->array_stream_open_reader != NULL);
  EXPECT(lj->array_stream_open_filep != NULL);
  EXPECT(lj->array_stream_open_path != NULL);
  EXPECT(lj->array_stream_open_fd != NULL);
  EXPECT(lj->array_stream_open_push != NULL);
  EXPECT(lj->spooled_init != NULL);
  EXPECT(lj->spooled_init_class != NULL);
  EXPECT(lj->json_value_init != NULL);
  EXPECT(lj->json_value_cleanup != NULL);
  EXPECT(lj->array_rewrite_reader != NULL);
  EXPECT(lj->array_rewrite_reader_to_filep != NULL);
  EXPECT(lj->array_rewrite_reader_to_fd != NULL);
  EXPECT(lj->array_rewrite_filep != NULL);
  EXPECT(lj->array_rewrite_filep_to_filep != NULL);
  EXPECT(lj->array_rewrite_fd != NULL);
  EXPECT(lj->array_rewrite_fd_to_fd != NULL);
  EXPECT(lj->array_rewrite_path != NULL);
  EXPECT(lj->value_rewrite_reader != NULL);
  EXPECT(lj->value_rewrite_buffer != NULL);
  EXPECT(lj->value_rewrite_filep != NULL);
  EXPECT(lj->value_rewrite_fd != NULL);
  EXPECT(lj->value_rewrite_path != NULL);
  EXPECT(lj->value_rewrite_selector_reader != NULL);
  EXPECT(lj->value_rewrite_selector_buffer != NULL);
  EXPECT(lj->value_rewrite_selector_filep != NULL);
  EXPECT(lj->value_rewrite_selector_fd != NULL);
  EXPECT(lj->value_rewrite_selector_path != NULL);
  EXPECT(lj->generator_init != NULL);
  EXPECT(lj->writer_init_sink != NULL);
  EXPECT(lj->free != NULL);

  poison_bytes(&person, sizeof(person), 0xCDu);
  lj->init(lj, &test_person_map, &person);
  EXPECT(person.name == NULL);
  EXPECT(person.lucky_numbers.items == NULL);
  EXPECT(person.tags.items == NULL);

  memset(&person, 0, sizeof(person));
  EXPECT(lj->parse_cstr(lj, &test_person_map, &person, json, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "Ada") == 0);
  EXPECT(strcmp(person.address.city, "Lund") == 0);
  EXPECT(person.age == 37);
  EXPECT(person.active == true);
  lj->reset(lj, &test_person_map, &person);
  EXPECT(person.name == NULL);
  EXPECT(person.tags.count == 0u);

  EXPECT(lj->validate_cstr(lj, value_json, &error) == LONEJSON_STATUS_OK);

  memset(&visit_state, 0, sizeof(visit_state));
  visitor = test_counting_value_visitor();
  EXPECT(lj->visit_value_cstr(lj, value_json, &visitor, &visit_state, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(visit_state.object_count == 1u);
  EXPECT(visit_state.bool_count == 1u);

  poison_bytes(&spool, sizeof(spool), 0xCDu);
  lj->spooled_init(lj, &spool);
  EXPECT(lonejson__spooled_is_initialized(&spool));
  lonejson_spooled_cleanup(&spool);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  memset(&value_rewrite_options, 0, sizeof(value_rewrite_options));
  EXPECT(lj->value_rewrite_buffer(
             lj, value_json, strlen(value_json), test_buffer_sink_write, &sink,
             &value_rewrite_options, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, value_json) == 0);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  memset(&array_rewrite_options, 0, sizeof(array_rewrite_options));
  memset(&item, 0, sizeof(item));
  array_rewrite_options.item_map = &test_item_map;
  array_rewrite_options.item_dst = &item;
  array_rewrite_options.item = test_runtime_array_rewrite_keep_item;
  lonejson_buffer_reader_init(&array_reader, (const unsigned char *)array_json,
                              strlen(array_json));
  EXPECT(lj->array_rewrite_reader(lj, NULL, lonejson_buffer_reader_read,
                                  &array_reader, test_buffer_sink_write, &sink,
                                  &array_rewrite_options,
                                  &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, array_json) == 0);

  poison_bytes(&value, sizeof(value), 0xCDu);
  lj->json_value_init(lj, &value);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  lj->json_value_cleanup(lj, &value);

  lonejson_free(lj);
}

static void test_runtime_method_json_value_init_null_does_not_leak_pin(void) {
  lonejson *lj;
  lonejson_error error;
  lonejson__runtime_bundle *bundle;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  bundle = (lonejson__runtime_bundle *)lj;
  lj->json_value_init(lj, NULL);
  EXPECT(bundle->live_handle.active_pins == 0u);
  lonejson_free(lj);
}

static void test_receiver_methods_dispatch_on_public_handles(void) {
  lonejson *lj;
  lonejson_error error;
  lonejson_spooled spool;
  lonejson_json_value value;
  lonejson_source source;
  lonejson_generator generator;
  lonejson_writer writer;
  lonejson_writer_value_stream value_stream;
  lonejson_string_array_stream helper_string_stream =
      LONEJSON_STRING_ARRAY_STREAM_INIT;
  lonejson_mapped_array_stream helper_mapped_stream =
      LONEJSON_MAPPED_ARRAY_STREAM_INIT;
  lonejson_stream *stream;
  lonejson_array_stream *array_stream;
  test_buffer_sink sink;
  unsigned char sink_buffer[512];
  unsigned char read_buffer[32];
  lonejson_read_result read_result;
  test_event event;
  char path[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-methods-XXXXXX";
  int fd;
  const char *framed =
      "{\"id\":\"evt-1\",\"ok\":true}{\"id\":\"evt-2\",\"ok\":false}!";
  const char *array_json = "[{\"id\":\"evt-1\",\"ok\":true}]";
  lonejson_stream_result stream_result;
  lonejson_array_stream_result array_result;
  lonejson_buffer_reader framed_reader;
  lonejson_buffer_reader array_reader;
  size_t out_len;
  int out_eof;
  lonejson_status status;
  lonejson_owned_buffer owned;
  char *allocated;
  test_event events[2];
  size_t needed;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  lonejson_owned_buffer_init(&owned);
  memset(events, 0, sizeof(events));

  EXPECT(lj->cleanup != NULL);
  EXPECT(lj->write_json_string_sink != NULL);
  EXPECT(lj->write_json_string_buffer_sink != NULL);
  EXPECT(lj->write_json_string_spooled_sink != NULL);
  EXPECT(lj->serialize_sink != NULL);
  EXPECT(lj->serialize_buffer != NULL);
  EXPECT(lj->serialize_alloc != NULL);
  EXPECT(lj->serialize_owned != NULL);
  EXPECT(lj->serialize_filep != NULL);
  EXPECT(lj->serialize_path != NULL);
  EXPECT(lj->serialize_jsonl_sink != NULL);
  EXPECT(lj->serialize_jsonl_buffer != NULL);
  EXPECT(lj->serialize_jsonl_alloc != NULL);
  EXPECT(lj->serialize_jsonl_owned != NULL);
  EXPECT(lj->serialize_jsonl_filep != NULL);
  EXPECT(lj->serialize_jsonl_path != NULL);

  memset(&spool, 0, sizeof(spool));
  lj->spooled_init(lj, &spool);
  EXPECT(spool.reset != NULL);
  EXPECT(spool.cleanup != NULL);
  EXPECT(spool.size_fn != NULL);
  EXPECT(spool.spilled_fn != NULL);
  EXPECT(spool.append != NULL);
  EXPECT(spool.rewind != NULL);
  EXPECT(spool.read != NULL);
  EXPECT(spool.write_to_sink != NULL);
  EXPECT(spool.append(&spool, "hello", 5u, &error) == LONEJSON_STATUS_OK);
  EXPECT(spool.size_fn(&spool) == 5u);
  EXPECT(spool.spilled_fn(&spool) == 0);
  EXPECT(spool.rewind(&spool, &error) == LONEJSON_STATUS_OK);
  read_result = spool.read(&spool, read_buffer, sizeof(read_buffer));
  EXPECT(read_result.bytes_read == 5u);
  EXPECT(memcmp(read_buffer, "hello", 5u) == 0);
  EXPECT(spool.write_to_sink(&spool, test_buffer_sink_write, &sink, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "hello") == 0);
  spool.reset(&spool);
  EXPECT(spool.size_fn(&spool) == 0u);
  spool.cleanup(&spool);

  EXPECT(helper_string_stream.set_handler ==
         lonejson_string_array_stream_set_handler);
  EXPECT(helper_mapped_stream.set_handler ==
         lonejson_mapped_array_stream_set_handler);

  memset(&value, 0, sizeof(value));
  lj->json_value_init(lj, &value);
  EXPECT(value.methods != NULL);
  EXPECT(value.methods->reset != NULL);
  EXPECT(value.methods->cleanup != NULL);
  EXPECT(value.methods->set_buffer != NULL);
  EXPECT(value.methods->set_parse_sink != NULL);
  EXPECT(value.methods->set_parse_visitor != NULL);
  EXPECT(value.methods->set_parse_path_visitor != NULL);
  EXPECT(value.methods->enable_parse_capture != NULL);
  EXPECT(value.methods->set_reader != NULL);
  EXPECT(value.methods->set_file != NULL);
  EXPECT(value.methods->set_fd != NULL);
  EXPECT(value.methods->set_path != NULL);
  EXPECT(value.methods->write_to_sink != NULL);
  EXPECT(value.methods->is_rewindable != NULL);
  EXPECT(value.methods->set_buffer(&value, "true", 4u, &error) ==
         LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(value.methods->is_rewindable(&value) != 0);
  EXPECT(value.methods->write_to_sink(&value, test_buffer_sink_write, &sink,
                                      &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "true") == 0);
  value.methods->reset(&value);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  value.methods->cleanup(&value);

  memset(&source, 0, sizeof(source));
  lonejson_source_init(&source);
  EXPECT(source.cleanup != NULL);
  EXPECT(source.reset != NULL);
  EXPECT(source.set_file != NULL);
  EXPECT(source.set_fd != NULL);
  EXPECT(source.set_path != NULL);
  EXPECT(source.write_to_sink != NULL);
  EXPECT(source.is_rewindable != NULL);
  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    EXPECT(write(fd, "world", 5u) == 5);
    close(fd);
    EXPECT(source.set_path(&source, path, &error) == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    memset(sink_buffer, 0, sizeof(sink_buffer));
    sink.buffer = sink_buffer;
    sink.capacity = sizeof(sink_buffer);
    EXPECT(source.is_rewindable(&source) != 0);
    EXPECT(source.write_to_sink(&source, test_buffer_sink_write, &sink,
                                &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp((const char *)sink.buffer, "world") == 0);
    unlink(path);
  }
  source.cleanup(&source);

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  lonejson_buffer_reader_init(&framed_reader, (const unsigned char *)"gamma",
                              5u);
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->write_json_string_sink(lj, lonejson_buffer_reader_read,
                                    &framed_reader, test_buffer_sink_write,
                                    &sink, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "\"gamma\"") == 0);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->write_json_string_buffer_sink(lj, "alpha", 5u,
                                           test_buffer_sink_write, &sink,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "\"alpha\"") == 0);

  memset(&spool, 0, sizeof(spool));
  lj->spooled_init(lj, &spool);
  EXPECT(spool.append(&spool, "beta", 4u, &error) == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->write_json_string_spooled_sink(lj, &spool, test_buffer_sink_write,
                                            &sink,
                                            &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "\"beta\"") == 0);
  spool.cleanup(&spool);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->serialize_sink(lj, &test_event_map, &event, test_buffer_sink_write,
                            &sink, &error) == LONEJSON_STATUS_OK);
  EXPECT(strstr((const char *)sink.buffer, "\"id\":\"evt-1\"") != NULL);

  memset(sink_buffer, 0, sizeof(sink_buffer));
  needed = 0u;
  EXPECT(lj->serialize_buffer(lj, &test_event_map, &event, (char *)sink_buffer,
                              sizeof(sink_buffer), &needed,
                              &error) == LONEJSON_STATUS_OK);
  EXPECT(strstr((const char *)sink_buffer, "\"id\":\"evt-1\"") != NULL);

  allocated = lj->serialize_alloc(lj, &test_event_map, &event, NULL, &error);
  EXPECT(allocated != NULL);
  if (allocated != NULL) {
    EXPECT(strstr(allocated, "\"ok\":true") != NULL);
    free(allocated);
  }

  EXPECT(lj->serialize_owned(lj, &test_event_map, &event, &owned, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(owned.data != NULL);
  if (owned.data != NULL) {
    EXPECT(strstr((const char *)owned.data, "\"evt-1\"") != NULL);
  }
  lonejson_owned_buffer_free(&owned);
  lonejson_owned_buffer_init(&owned);

  memset(sink_buffer, 0, sizeof(sink_buffer));
  needed = 0u;
  EXPECT(lj->serialize_jsonl_buffer(lj, &test_event_map, events, 2u, 0u,
                                    (char *)sink_buffer, sizeof(sink_buffer),
                                    &needed, &error) == LONEJSON_STATUS_OK);
  EXPECT(strstr((const char *)sink_buffer, "\n") != NULL);

  allocated = lj->serialize_jsonl_alloc(lj, &test_event_map, events, 2u, 0u,
                                        NULL, &error);
  EXPECT(allocated != NULL);
  if (allocated != NULL) {
    EXPECT(strstr(allocated, "\"evt-2\"") != NULL);
    free(allocated);
  }

  EXPECT(lj->serialize_jsonl_owned(lj, &test_event_map, events, 2u, 0u, &owned,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(owned.data != NULL);
  if (owned.data != NULL) {
    EXPECT(strstr((const char *)owned.data, "\"evt-1\"") != NULL);
  }
  lonejson_owned_buffer_free(&owned);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->serialize_jsonl_sink(lj, &test_event_map, events, 2u, 0u,
                                  test_buffer_sink_write, &sink,
                                  &error) == LONEJSON_STATUS_OK);
  EXPECT(strstr((const char *)sink.buffer, "\"evt-2\"") != NULL);

  memset(&generator, 0, sizeof(generator));
  EXPECT(lj->generator_init(lj, &generator, &test_event_map, &event) ==
         LONEJSON_STATUS_OK);
  EXPECT(generator.read != NULL);
  EXPECT(generator.cleanup != NULL);
  out_len = 0u;
  EXPECT(lj->generator_measure(lj, &test_event_map, &event, &out_len, &error) ==
         LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  for (;;) {
    status = generator.read(&generator, read_buffer, sizeof(read_buffer),
                            &out_len, &out_eof);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(test_buffer_sink_write(&sink, read_buffer, out_len, &error) ==
           LONEJSON_STATUS_OK);
    if (out_eof) {
      break;
    }
  }
  EXPECT(strcmp((const char *)sink.buffer, "{\"id\":\"evt-1\",\"ok\":true}") ==
         0);
  generator.cleanup(&generator);

  memset(&writer, 0, sizeof(writer));
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->writer_init_sink(lj, &writer, test_buffer_sink_write, &sink,
                              &error) == LONEJSON_STATUS_OK);
  EXPECT(writer.cleanup != NULL);
  EXPECT(writer.begin_object != NULL);
  EXPECT(writer.end_object != NULL);
  EXPECT(writer.begin_array != NULL);
  EXPECT(writer.end_array != NULL);
  EXPECT(writer.key != NULL);
  EXPECT(writer.string != NULL);
  EXPECT(writer.string_begin != NULL);
  EXPECT(writer.string_chunk != NULL);
  EXPECT(writer.string_end != NULL);
  EXPECT(writer.string_reader != NULL);
  EXPECT(writer.string_spooled != NULL);
  EXPECT(writer.source_text != NULL);
  EXPECT(writer.spooled_base64 != NULL);
  EXPECT(writer.source_base64 != NULL);
  EXPECT(writer.number_text != NULL);
  EXPECT(writer.i64 != NULL);
  EXPECT(writer.u64 != NULL);
  EXPECT(writer.f64 != NULL);
  EXPECT(writer.bool_fn != NULL);
  EXPECT(writer.null_fn != NULL);
  EXPECT(writer.json_value != NULL);
  EXPECT(writer.json_value_reader != NULL);
  EXPECT(writer.json_value_buffer != NULL);
  EXPECT(writer.json_value_file != NULL);
  EXPECT(writer.json_value_fd != NULL);
  EXPECT(writer.json_value_path != NULL);
  EXPECT(writer.json_value_spooled != NULL);
  EXPECT(writer.array_items_reader != NULL);
  EXPECT(writer.array_items_buffer != NULL);
  EXPECT(writer.array_items_filep != NULL);
  EXPECT(writer.array_items_fd != NULL);
  EXPECT(writer.array_items_path != NULL);
  EXPECT(writer.array_items_spooled != NULL);
  EXPECT(writer.mapped != NULL);
  EXPECT(writer.finish != NULL);
  EXPECT(writer.begin_object(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(writer.key(&writer, "ok", 2u, &error) == LONEJSON_STATUS_OK);
  EXPECT(writer.bool_fn(&writer, 1, &error) == LONEJSON_STATUS_OK);
  EXPECT(writer.end_object(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(writer.finish(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "{\"ok\":true}") == 0);
  writer.cleanup(&writer);

  memset(&writer, 0, sizeof(writer));
  lonejson_writer_value_stream_init(&value_stream);
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lj->writer_init_sink(lj, &writer, test_buffer_sink_write, &sink,
                              &error) == LONEJSON_STATUS_OK);
  EXPECT(value_stream.open != NULL);
  EXPECT(value_stream.push != NULL);
  EXPECT(value_stream.close != NULL);
  EXPECT(value_stream.cleanup != NULL);
  EXPECT(value_stream.open(&value_stream, &writer, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(value_stream.push(&value_stream, "true", 4u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(value_stream.close(&value_stream, &error) == LONEJSON_STATUS_OK);
  EXPECT(writer.finish(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "true") == 0);
  value_stream.cleanup(&value_stream);
  writer.cleanup(&writer);

  lonejson_buffer_reader_init(&framed_reader, (const unsigned char *)framed,
                              strlen(framed));
  stream = lj->stream_open_reader(
      lj, &test_event_map, lonejson_buffer_reader_read, &framed_reader, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    EXPECT(stream->next != NULL);
    EXPECT(stream->close != NULL);
    memset(&event, 0, sizeof(event));
    stream_result = stream->next(stream, &event, &error);
    EXPECT(stream_result == LONEJSON_STREAM_OBJECT);
    EXPECT(strcmp(event.id, "evt-1") == 0);
    EXPECT(event.ok == true);
    EXPECT(stream->error_fn(stream)->code == LONEJSON_STATUS_OK);
    EXPECT(stream->next(stream, &event, &error) == LONEJSON_STREAM_OBJECT);
    EXPECT(stream->next(stream, &event, &error) == LONEJSON_STREAM_ERROR);
    EXPECT(stream->error_fn(stream)->code == LONEJSON_STATUS_INVALID_JSON);
    stream->close(stream);
  }

  lonejson_buffer_reader_init(&array_reader, (const unsigned char *)array_json,
                              strlen(array_json));
  array_stream = lj->array_stream_open_reader(
      lj, NULL, lonejson_buffer_reader_read, &array_reader, &error);
  EXPECT(array_stream != NULL);
  if (array_stream != NULL) {
    EXPECT(array_stream->next != NULL);
    EXPECT(array_stream->next_value != NULL);
    EXPECT(array_stream->push != NULL);
    EXPECT(array_stream->finish != NULL);
    EXPECT(array_stream->push_string != NULL);
    EXPECT(array_stream->finish_string != NULL);
    EXPECT(array_stream->push_string_items != NULL);
    EXPECT(array_stream->finish_string_items != NULL);
    EXPECT(array_stream->close != NULL);
    EXPECT(array_stream->set_record_reuse(array_stream, 1, &error) ==
           LONEJSON_STATUS_OK);
    memset(&event, 0, sizeof(event));
    array_result =
        array_stream->next(array_stream, &test_event_map, &event, &error);
    EXPECT(array_result == LONEJSON_ARRAY_STREAM_ITEM);
    EXPECT(array_stream->error_fn(array_stream)->code == LONEJSON_STATUS_OK);
    EXPECT(array_stream->set_record_reuse(array_stream, 0, &error) ==
           LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(strcmp(event.id, "evt-1") == 0);
    EXPECT(event.ok == true);
    array_stream->close(array_stream);
  }

#ifdef LONEJSON_WITH_CURL
  {
    static const char curl_json[] = "{\"id\":\"evt-3\",\"ok\":true}";
    lonejson_curl_parse parse = {0};
    lonejson_curl_upload upload = {0};
    size_t wrote;

    memset(&parse, 0, sizeof(parse));
    memset(&event, 0, sizeof(event));
    EXPECT(lj->curl_parse_init != NULL);
    EXPECT(lj->curl_upload_init != NULL);
    EXPECT(lj->curl_parse_init(lj, &parse, &test_event_map, &event) ==
           LONEJSON_STATUS_OK);
    EXPECT(parse.write_callback != NULL);
    EXPECT(parse.finish != NULL);
    EXPECT(parse.cleanup != NULL);
    wrote =
        parse.write_callback(&parse, (char *)curl_json, 1u, strlen(curl_json));
    EXPECT(wrote == strlen(curl_json));
    EXPECT(parse.finish(&parse) == LONEJSON_STATUS_OK);
    EXPECT(strcmp(event.id, "evt-3") == 0);
    parse.cleanup(&parse);

    memset(&upload, 0, sizeof(upload));
    strcpy(event.id, "evt-up");
    event.ok = true;
    EXPECT(lj->curl_upload_init(lj, &upload, &test_event_map, &event) ==
           LONEJSON_STATUS_OK);
    EXPECT(upload.read_callback != NULL);
    EXPECT(upload.size_fn != NULL);
    EXPECT(upload.cleanup != NULL);
    wrote = upload.read_callback(&upload, (char *)read_buffer, 1u,
                                 sizeof(read_buffer));
    EXPECT(wrote != 0u);
    EXPECT(wrote != CURL_READFUNC_ABORT);
    if (wrote != CURL_READFUNC_ABORT && wrote <= sizeof(read_buffer)) {
    }
    EXPECT(upload.size_fn(&upload) == (curl_off_t)-1);
    upload.cleanup(&upload);
  }
#endif

  lonejson_free(lj);
}

static void test_remaining_public_api_entrypoints_are_directly_covered(void) {
  static const char event_json[] = "{\"id\":\"evt-1\",\"ok\":true}";
  static const char event_stream_json[] =
      "{\"id\":\"evt-1\",\"ok\":true}{\"id\":\"evt-2\",\"ok\":false}";
  static const char array_json[] =
      "{\"items\":[{\"id\":\"evt-1\",\"ok\":true}],\"done\":true}";
  static const char rewrite_json[] = "{\"items\":[1,2],\"ok\":true}";
  lonejson *lj;
  lonejson_error error;
  test_event event;
  test_event events[2];
  lonejson_stream *stream;
  lonejson_stream_result stream_result;
  lonejson_array_stream *array_stream;
  lonejson_array_stream_result array_result;
  lonejson_spooled spool;
  lonejson_json_value rewrite_value;
  lonejson_writer writer;
  lonejson_owned_buffer owned;
  lonejson_array_rewrite_options rewrite_options;
  lonejson_value_rewrite_options value_options;
  test_buffer_sink sink;
  unsigned char sink_buffer[512];
  FILE *fp;
  FILE *out_fp;
  char fd_in_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-public-api-fd-in-XXXXXX";
  char fd_out_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-public-api-fd-out-XXXXXX";
  char reader_fd_out_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-public-api-rfd-out-XXXXXX";
  char value_fd_in_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-public-api-value-in-XXXXXX";
  char selector_fd_in_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-public-api-sel-in-XXXXXX";
  char selector_path_in[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-public-api-sel-path-in-XXXXXX";
  char selector_path_out[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-public-api-sel-path-out-XXXXXX";
  int in_fd;
  int out_fd;
  ssize_t wrote;
  size_t rewrite_count;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  EXPECT(lonejson_status_string(LONEJSON_STATUS_OK) != NULL);
  EXPECT(lonejson_status_string((lonejson_status)9999) != NULL);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  lonejson_spooled_init(lj, &spool);
  EXPECT(lonejson_spooled_append(&spool, "spooled", 7u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_write_to_sink(&spool, test_buffer_sink_write, &sink,
                                        &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "spooled") == 0);
  lonejson_spooled_reset(&spool);
  EXPECT(lonejson_spooled_size(&spool) == 0u);
  lonejson_spooled_cleanup(&spool);

  fp = tmpfile();
  EXPECT(fp != NULL);
  if (fp != NULL) {
    EXPECT(fwrite(event_stream_json, 1u, strlen(event_stream_json), fp) ==
           strlen(event_stream_json));
    rewind(fp);
    stream = lonejson_stream_open_filep(lj, &test_event_map, fp, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      memset(&event, 0, sizeof(event));
      stream_result = lonejson_stream_next(stream, &event, &error);
      EXPECT(stream_result == LONEJSON_STREAM_OBJECT);
      EXPECT(strcmp(event.id, "evt-1") == 0);
      EXPECT(lonejson_stream_error(stream) != NULL);
      lonejson_stream_close(stream);
    }
    fclose(fp);
  }

  fp = tmpfile();
  EXPECT(fp != NULL);
  if (fp != NULL) {
    EXPECT(fwrite(array_json, 1u, strlen(array_json), fp) ==
           strlen(array_json));
    rewind(fp);
    array_stream = lonejson_array_stream_open_filep(lj, "items", fp, &error);
    EXPECT(array_stream != NULL);
    if (array_stream != NULL) {
      memset(&event, 0, sizeof(event));
      array_result = lonejson_array_stream_next(array_stream, &test_event_map,
                                                &event, &error);
      EXPECT(array_result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(strcmp(event.id, "evt-1") == 0);
      EXPECT(lonejson_array_stream_error(array_stream) != NULL);
      lonejson_array_stream_close(array_stream);
    }
    fclose(fp);
  }

  fp = tmpfile();
  EXPECT(fp != NULL);
  if (fp != NULL) {
    EXPECT(fwrite(event_json, 1u, strlen(event_json), fp) ==
           strlen(event_json));
    rewind(fp);
    EXPECT(lonejson_validate_filep(lj, fp, &error) == LONEJSON_STATUS_OK);
    fclose(fp);
  }

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lonejson_write_json_string_buffer_sink(lj, "chunk", 5u,
                                                test_buffer_sink_write, &sink,
                                                &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "\"chunk\"") == 0);

  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  EXPECT(lonejson_writer_init_sink(lj, &writer, test_buffer_sink_write, &sink,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_string_begin(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_string_chunk(&writer, "ab", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_string_chunk(&writer, "cd", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_string_end(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_finish(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)sink.buffer, "\"abcd\"") == 0);
  lonejson_writer_cleanup(&writer);

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;
  lonejson_owned_buffer_init(&owned);
  EXPECT(lonejson_serialize_jsonl_owned(lj, &test_event_map, events, 2u, 0u,
                                        &owned, &error) == LONEJSON_STATUS_OK);
  EXPECT(owned.data != NULL);
  if (owned.data != NULL) {
    EXPECT(strstr(owned.data, "\"evt-2\"") != NULL);
  }
  lonejson_owned_buffer_free(&owned);

  memset(&rewrite_options, 0, sizeof(rewrite_options));
  lonejson_json_value_init(lj, &rewrite_value);
  rewrite_options.item_value = &rewrite_value;
  rewrite_options.item = test_array_rewrite_count_value_item;
  rewrite_options.user = &rewrite_count;
  rewrite_count = 0u;

  in_fd = mkstemp(fd_in_path);
  out_fd = mkstemp(fd_out_path);
  EXPECT(in_fd >= 0);
  EXPECT(out_fd >= 0);
  if (in_fd >= 0 && out_fd >= 0) {
    wrote = write(in_fd, rewrite_json, strlen(rewrite_json));
    EXPECT(wrote == (ssize_t)strlen(rewrite_json));
    EXPECT(lseek(in_fd, 0, SEEK_SET) == 0);
    EXPECT(lonejson_array_rewrite_fd_to_fd(lj, "items", in_fd, out_fd,
                                           &rewrite_options,
                                           &error) == LONEJSON_STATUS_OK);
    EXPECT(lseek(in_fd, 0, SEEK_SET) == 0);
    memset(&sink, 0, sizeof(sink));
    memset(sink_buffer, 0, sizeof(sink_buffer));
    sink.buffer = sink_buffer;
    sink.capacity = sizeof(sink_buffer);
    EXPECT(lonejson_array_rewrite_fd(lj, "items", in_fd, test_buffer_sink_write,
                                     &sink, &rewrite_options,
                                     &error) == LONEJSON_STATUS_OK);
    EXPECT(strstr((const char *)sink.buffer, "\"items\":[1,2]") != NULL);
  }
  if (in_fd >= 0) {
    close(in_fd);
  }
  if (out_fd >= 0) {
    close(out_fd);
  }
  unlink(fd_in_path);
  unlink(fd_out_path);

  fp = tmpfile();
  out_fp = tmpfile();
  EXPECT(fp != NULL);
  EXPECT(out_fp != NULL);
  if (fp != NULL && out_fp != NULL) {
    EXPECT(fwrite(rewrite_json, 1u, strlen(rewrite_json), fp) ==
           strlen(rewrite_json));
    rewind(fp);
    memset(&sink, 0, sizeof(sink));
    memset(sink_buffer, 0, sizeof(sink_buffer));
    sink.buffer = sink_buffer;
    sink.capacity = sizeof(sink_buffer);
    EXPECT(lonejson_array_rewrite_filep(lj, "items", fp, test_buffer_sink_write,
                                        &sink, &rewrite_options,
                                        &error) == LONEJSON_STATUS_OK);
    EXPECT(strstr((const char *)sink.buffer, "\"items\":[1,2]") != NULL);
    rewind(fp);
    EXPECT(lonejson_array_rewrite_filep_to_filep(lj, "items", fp, out_fp,
                                                 &rewrite_options,
                                                 &error) == LONEJSON_STATUS_OK);
  }
  if (fp != NULL) {
    fclose(fp);
  }
  if (out_fp != NULL) {
    fclose(out_fp);
  }

  {
    test_reader_state reader = {rewrite_json, 0u, 4u};
    out_fp = tmpfile();
    EXPECT(out_fp != NULL);
    if (out_fp != NULL) {
      EXPECT(lonejson_array_rewrite_reader_to_filep(
                 lj, "items", test_state_reader, &reader, out_fp,
                 &rewrite_options, &error) == LONEJSON_STATUS_OK);
      fclose(out_fp);
    }
  }
  {
    test_reader_state reader = {rewrite_json, 0u, 4u};
    out_fd = mkstemp(reader_fd_out_path);
    EXPECT(out_fd >= 0);
    if (out_fd >= 0) {
      EXPECT(lonejson_array_rewrite_reader_to_fd(
                 lj, "items", test_state_reader, &reader, out_fd,
                 &rewrite_options, &error) == LONEJSON_STATUS_OK);
      close(out_fd);
    }
    unlink(reader_fd_out_path);
  }
  EXPECT(rewrite_count != 0u);
  lonejson_json_value_cleanup(&rewrite_value);

  memset(&value_options, 0, sizeof(value_options));
  value_options.action = LONEJSON_VALUE_REWRITE_KEEP;

  fp = tmpfile();
  EXPECT(fp != NULL);
  if (fp != NULL) {
    EXPECT(fwrite(event_json, 1u, strlen(event_json), fp) ==
           strlen(event_json));
    rewind(fp);
    memset(&sink, 0, sizeof(sink));
    memset(sink_buffer, 0, sizeof(sink_buffer));
    sink.buffer = sink_buffer;
    sink.capacity = sizeof(sink_buffer);
    EXPECT(lonejson_value_rewrite_filep(lj, fp, test_buffer_sink_write, &sink,
                                        &value_options,
                                        &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp((const char *)sink.buffer, event_json) == 0);
    fclose(fp);
  }

  in_fd = mkstemp(value_fd_in_path);
  EXPECT(in_fd >= 0);
  if (in_fd >= 0) {
    wrote = write(in_fd, event_json, strlen(event_json));
    EXPECT(wrote == (ssize_t)strlen(event_json));
    EXPECT(lseek(in_fd, 0, SEEK_SET) == 0);
    memset(&sink, 0, sizeof(sink));
    memset(sink_buffer, 0, sizeof(sink_buffer));
    sink.buffer = sink_buffer;
    sink.capacity = sizeof(sink_buffer);
    EXPECT(lonejson_value_rewrite_fd(lj, in_fd, test_buffer_sink_write, &sink,
                                     &value_options,
                                     &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp((const char *)sink.buffer, event_json) == 0);
    close(in_fd);
  }
  unlink(value_fd_in_path);

  {
    lonejson_value_rewrite_selector_options selector_options;
    memset(&selector_options, 0, sizeof(selector_options));
    selector_options.selector = "id";
    selector_options.action = LONEJSON_VALUE_REWRITE_KEEP;
    fp = tmpfile();
    EXPECT(fp != NULL);
    if (fp != NULL) {
      EXPECT(fwrite(event_json, 1u, strlen(event_json), fp) ==
             strlen(event_json));
      rewind(fp);
      memset(&sink, 0, sizeof(sink));
      memset(sink_buffer, 0, sizeof(sink_buffer));
      sink.buffer = sink_buffer;
      sink.capacity = sizeof(sink_buffer);
      EXPECT(lonejson_value_rewrite_selector_filep(
                 lj, fp, test_buffer_sink_write, &sink, &selector_options,
                 &error) == LONEJSON_STATUS_OK);
      EXPECT(strcmp((const char *)sink.buffer, event_json) == 0);
      fclose(fp);
    }
    in_fd = mkstemp(selector_fd_in_path);
    EXPECT(in_fd >= 0);
    if (in_fd >= 0) {
      wrote = write(in_fd, event_json, strlen(event_json));
      EXPECT(wrote == (ssize_t)strlen(event_json));
      EXPECT(lseek(in_fd, 0, SEEK_SET) == 0);
      memset(&sink, 0, sizeof(sink));
      memset(sink_buffer, 0, sizeof(sink_buffer));
      sink.buffer = sink_buffer;
      sink.capacity = sizeof(sink_buffer);
      EXPECT(lonejson_value_rewrite_selector_fd(
                 lj, in_fd, test_buffer_sink_write, &sink, &selector_options,
                 &error) == LONEJSON_STATUS_OK);
      EXPECT(strcmp((const char *)sink.buffer, event_json) == 0);
      close(in_fd);
    }
    unlink(selector_fd_in_path);
    in_fd = mkstemp(selector_path_in);
    out_fd = mkstemp(selector_path_out);
    EXPECT(in_fd >= 0);
    EXPECT(out_fd >= 0);
    if (in_fd >= 0 && out_fd >= 0) {
      wrote = write(in_fd, event_json, strlen(event_json));
      EXPECT(wrote == (ssize_t)strlen(event_json));
      close(in_fd);
      close(out_fd);
      EXPECT(lonejson_value_rewrite_selector_path(
                 lj, selector_path_in, selector_path_out, &selector_options,
                 &error) == LONEJSON_STATUS_OK);
    } else {
      if (in_fd >= 0) {
        close(in_fd);
      }
      if (out_fd >= 0) {
        close(out_fd);
      }
    }
    unlink(selector_path_in);
    unlink(selector_path_out);
  }

#ifdef LONEJSON_WITH_CURL
  {
    lonejson_curl_upload upload = {0};
    strcpy(event.id, "evt-up");
    event.ok = true;
    EXPECT(lonejson_curl_upload_init(&upload, lj, &test_event_map, &event) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_curl_upload_size(&upload) == (curl_off_t)-1);
    lonejson_curl_upload_cleanup(&upload);
  }
#endif

  lonejson_free(lj);
}

static void test_runtime_copy_free_leaves_owner_live(void) {
  typedef struct test_runtime_wrapper {
    lonejson runtime;
  } test_runtime_wrapper;

  lonejson *lj;
  lonejson copy;
  lonejson second_copy;
  test_runtime_wrapper wrapper;
  lonejson_error error;
  test_event event;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson_config config;

  test_runtime_handle_cache_drain();
  reset_lonejson_alloc_stats();
  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  copy = *lj;
  second_copy = *lj;
  wrapper.runtime = *lj;
  EXPECT(copy.state == lj->state);
  EXPECT(wrapper.runtime.state == lj->state);
  EXPECT(second_copy.state == lj->state);

  copy.free(&copy);
  EXPECT(copy.state == NULL);
  EXPECT(copy._state_token == 0u);
  EXPECT(g_lonejson_alloc_calls == 1u);
  EXPECT(g_lonejson_free_calls == 0u);

  second_copy.free(&second_copy);
  EXPECT(second_copy.state == NULL);
  EXPECT(second_copy._state_token == 0u);
  EXPECT(g_lonejson_free_calls == 0u);

  wrapper.runtime.free(&wrapper.runtime);
  EXPECT(wrapper.runtime.state == NULL);
  EXPECT(wrapper.runtime._state_token == 0u);
  EXPECT(g_lonejson_free_calls == 0u);

  memset(&event, 0, sizeof(event));
  EXPECT(lonejson_parse_cstr(lj, &test_event_map, &event, json, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(event.id, "ev1") == 0);
  EXPECT(event.ok == true);

  lonejson_free(lj);
  EXPECT(g_lonejson_free_calls == 1u);
  test_runtime_handle_cache_drain();
}

static void test_runtime_bundle_uses_custom_allocator(void) {
  lonejson *lj;
  lonejson_error error;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson_config config;

  test_runtime_handle_cache_drain();
  reset_lonejson_alloc_stats();
  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  EXPECT(lj->state != NULL);
  EXPECT(lj->state == lj->_runtime_owner_data);
  EXPECT(g_lonejson_alloc_calls == 1u);
  EXPECT(g_lonejson_free_calls == 0u);

  lonejson_free(lj);
  EXPECT(g_lonejson_free_calls == 1u);
  test_runtime_handle_cache_drain();
  EXPECT(g_lonejson_free_calls == 1u);
}

static void test_runtime_copy_free_does_not_retire_owner(void) {
  lonejson *lj;
  lonejson copy;
  lonejson_error error;
  test_event event;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson_config config;

  test_runtime_handle_cache_drain();
  reset_lonejson_alloc_stats();
  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  copy = *lj;
  copy.free(&copy);
  EXPECT(copy.state == NULL);
  EXPECT(copy._state_token == 0u);
  EXPECT(g_lonejson_free_calls == 0u);

  memset(&event, 0, sizeof(event));
  EXPECT(lonejson_parse_cstr(lj, &test_event_map, &event, json, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(event.id, "ev1") == 0);
  EXPECT(event.ok == true);

  lonejson_free(lj);
  EXPECT(g_lonejson_free_calls == 1u);
  test_runtime_handle_cache_drain();
}

static void test_runtime_custom_allocator_handle_not_cached(void) {
  lonejson *custom;
  lonejson *def;
  lonejson_error error;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson_config config;

  test_runtime_handle_cache_drain();
  reset_lonejson_alloc_stats();
  config = lonejson_default_config();
  config.allocator = &allocator;
  custom = lonejson_new(&config, &error);
  EXPECT(custom != NULL);
  if (custom == NULL) {
    return;
  }

  lonejson_free(custom);
  EXPECT(g_lonejson_alloc_calls == 1u);
  EXPECT(g_lonejson_free_calls == 1u);

  reset_lonejson_alloc_stats();
  def = lonejson_new(NULL, &error);
  EXPECT(def != NULL);
  if (def == NULL) {
    return;
  }
  EXPECT(g_lonejson_alloc_calls == 1u);
  lonejson_free(def);
}

static void test_runtime_copy_rejects_calls_after_owner_free(void) {
  lonejson *lj;
  lonejson copy;
  lonejson_error error;
  test_event event;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  copy = *lj;
  lonejson_free(lj);

  memset(&event, 0, sizeof(event));
  EXPECT(lonejson_parse_cstr(&copy, &test_event_map, &event, json, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  copy.free(&copy);
  EXPECT(copy.state == NULL);
  EXPECT(copy._state_token == 0u);
}

static const lonejson_map *test_runtime_default_spool_map(void);

static void test_runtime_stale_copy_stays_dead_after_handle_reuse(void) {
  lonejson *first;
  lonejson *second;
  lonejson copy;
  lonejson_error error;
  test_event event;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";

  test_runtime_handle_cache_drain();
  first = lonejson_new(NULL, &error);
  EXPECT(first != NULL);
  if (first == NULL) {
    return;
  }
  copy = *first;
  lonejson_free(first);

  second = lonejson_new(NULL, &error);
  EXPECT(second != NULL);
  if (second == NULL) {
    return;
  }

  memset(&event, 0, sizeof(event));
  EXPECT(lonejson_parse_cstr(&copy, &test_event_map, &event, json, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&event, 0, sizeof(event));
  EXPECT(lonejson_parse_cstr(second, &test_event_map, &event, json, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(event.id, "ev1") == 0);
  EXPECT(event.ok == true);

  lonejson_free(second);
}

static void
test_runtime_custom_allocator_copy_rejects_calls_after_owner_free(void) {
  lonejson *lj;
  lonejson copy;
  lonejson_error error;
  test_event event;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson_config config;

  test_runtime_handle_cache_drain();
  reset_lonejson_alloc_stats();
  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  copy = *lj;
  lonejson_free(lj);
  EXPECT(g_lonejson_alloc_calls == 1u);
  EXPECT(g_lonejson_free_calls == 1u);

  memset(&event, 0, sizeof(event));
  EXPECT(lonejson_parse_cstr(&copy, &test_event_map, &event, json, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  copy.free(&copy);
  EXPECT(copy.state == NULL);
  EXPECT(copy._state_token == 0u);
  EXPECT(g_lonejson_free_calls == 1u);
  test_runtime_handle_cache_drain();
}

static void test_runtime_stale_copy_json_value_init_is_safe_default(void) {
  lonejson *lj;
  lonejson copy;
  lonejson_error error;
  lonejson_json_value value;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  copy = *lj;
  lonejson_free(lj);

  memset(&value, 0xa5, sizeof(value));
  copy.json_value_init(&copy, &value);
  EXPECT(lonejson__json_value_is_initialized(&value));
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(value.parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE);
  lonejson_json_value_cleanup(&value);

  copy.free(&copy);
}

static void
test_runtime_stale_copy_init_safely_initializes_stateful_fields(void) {
  const lonejson_map *spool_map = test_runtime_default_spool_map();
  lonejson *lj;
  lonejson copy;
  lonejson_error error;
  test_json_value_doc json_doc;
  test_spool_limits_doc spool_doc;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  copy = *lj;
  lonejson_free(lj);

  memset(&json_doc, 0xa5, sizeof(json_doc));
  lonejson_init(&copy, &test_json_value_doc_map, &json_doc);
  EXPECT(lonejson__json_value_is_initialized(&json_doc.selector));
  EXPECT(lonejson__json_value_is_initialized(&json_doc.fields));
  EXPECT(lonejson__json_value_is_initialized(&json_doc.last_error));
  lonejson_cleanup(&test_json_value_doc_map, &json_doc);

  memset(&spool_doc, 0xa5, sizeof(spool_doc));
  lonejson_init(&copy, spool_map, &spool_doc);
  EXPECT(lonejson__spooled_is_initialized(&spool_doc.text));
  EXPECT(spool_doc.text.temp_dir == NULL);
  lonejson_cleanup(spool_map, &spool_doc);

  copy.free(&copy);
}

static void test_runtime_stale_copy_reset_safely_reseeds_stateful_fields(void) {
  const lonejson_map *spool_map = test_runtime_default_spool_map();
  lonejson *lj;
  lonejson copy;
  lonejson_error error;
  test_json_value_doc json_doc;
  test_spool_limits_doc spool_doc;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  lonejson_init(lj, &test_json_value_doc_map, &json_doc);
  lonejson_init(lj, spool_map, &spool_doc);
  copy = *lj;
  lonejson_free(lj);

  memset(&json_doc.selector, 0, sizeof(json_doc.selector));
  memset(&json_doc.fields, 0, sizeof(json_doc.fields));
  memset(&json_doc.last_error, 0, sizeof(json_doc.last_error));
  lonejson_reset(&copy, &test_json_value_doc_map, &json_doc);
  EXPECT(lonejson__json_value_is_initialized(&json_doc.selector));
  EXPECT(lonejson__json_value_is_initialized(&json_doc.fields));
  EXPECT(lonejson__json_value_is_initialized(&json_doc.last_error));
  lonejson_cleanup(&test_json_value_doc_map, &json_doc);

  memset(&spool_doc.text, 0, sizeof(spool_doc.text));
  lonejson_reset(&copy, spool_map, &spool_doc);
  EXPECT(lonejson__spooled_is_initialized(&spool_doc.text));
  EXPECT(spool_doc.text.temp_dir == NULL);
  lonejson_cleanup(spool_map, &spool_doc);

  copy.free(&copy);
}

#if LONEJSON_TEST_HAS_PTHREAD && defined(LONEJSON_TEST_RUNTIME_BORROW_HOOK)
typedef struct test_runtime_borrow_barrier {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int entered;
  int release;
} test_runtime_borrow_barrier;

typedef struct test_runtime_copy_parse_thread_args {
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;
  test_event event;
  const char *json;
} test_runtime_copy_parse_thread_args;

typedef struct test_runtime_free_thread_args {
  lonejson *runtime;
  int done;
  pthread_mutex_t *done_mutex;
  pthread_cond_t *done_cond;
} test_runtime_free_thread_args;

static int
test_runtime_borrow_wait_entered(test_runtime_borrow_barrier *barrier,
                                 long timeout_ms) {
  struct timespec deadline;
  int rc;

  if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
    return 0;
  }
  deadline.tv_sec += timeout_ms / 1000L;
  deadline.tv_nsec += (timeout_ms % 1000L) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_sec += 1L;
    deadline.tv_nsec -= 1000000000L;
  }
  rc = 0;
  pthread_mutex_lock(&barrier->mutex);
  while (!barrier->entered && rc == 0) {
    rc = pthread_cond_timedwait(&barrier->cond, &barrier->mutex, &deadline);
  }
  pthread_mutex_unlock(&barrier->mutex);
  return rc == 0 && barrier->entered;
}

static void test_runtime_borrow_hook_block(void *user) {
  test_runtime_borrow_barrier *barrier = (test_runtime_borrow_barrier *)user;

  pthread_mutex_lock(&barrier->mutex);
  barrier->entered = 1;
  pthread_cond_broadcast(&barrier->cond);
  while (!barrier->release) {
    pthread_cond_wait(&barrier->cond, &barrier->mutex);
  }
  pthread_mutex_unlock(&barrier->mutex);
}

static void *test_runtime_copy_parse_thread_main(void *user) {
  test_runtime_copy_parse_thread_args *args =
      (test_runtime_copy_parse_thread_args *)user;

  memset(&args->event, 0, sizeof(args->event));
  args->status = lonejson_parse_cstr(args->runtime, &test_event_map,
                                     &args->event, args->json, &args->error);
  return NULL;
}

static void *test_runtime_owner_free_thread_main(void *user) {
  test_runtime_free_thread_args *args = (test_runtime_free_thread_args *)user;

  lonejson_free(args->runtime);
  if (args->done_mutex != NULL) {
    pthread_mutex_lock(args->done_mutex);
  }
  args->done = 1;
  if (args->done_cond != NULL) {
    pthread_cond_broadcast(args->done_cond);
  }
  if (args->done_mutex != NULL) {
    pthread_mutex_unlock(args->done_mutex);
  }
  return NULL;
}

static int test_runtime_wait_free_done(test_runtime_free_thread_args *args,
                                       long timeout_ms) {
  struct timespec deadline;
  int rc;

  if (args->done_mutex == NULL) {
    return args->done != 0;
  }
  if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
    return 0;
  }
  deadline.tv_sec += timeout_ms / 1000L;
  deadline.tv_nsec += (timeout_ms % 1000L) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_sec += 1L;
    deadline.tv_nsec -= 1000000000L;
  }
  rc = 0;
  pthread_mutex_lock(args->done_mutex);
  while (!args->done && rc == 0) {
    rc = pthread_cond_timedwait(args->done_cond, args->done_mutex, &deadline);
  }
  pthread_mutex_unlock(args->done_mutex);
  return rc == 0 && args->done;
}

static void test_runtime_copy_use_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson_error error;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";
  pthread_t parse_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_copy_parse_thread_args args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&args, 0, sizeof(args));
  args.runtime = (lonejson *)malloc(sizeof(lonejson));
  EXPECT(args.runtime != NULL);
  if (args.runtime == NULL) {
    pthread_cond_destroy(&barrier.cond);
    pthread_mutex_destroy(&barrier.mutex);
    lonejson_free(lj);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    return;
  }
  *args.runtime = *lj;
  args.json = json;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&parse_thread, NULL,
                        test_runtime_copy_parse_thread_main, &args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(parse_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_free(lj);
    free(args.runtime);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(parse_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(args.status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(args.event.id, "ev1") == 0);
  EXPECT(args.event.ok == true);
  EXPECT(lonejson_parse_cstr(args.runtime, &test_event_map, &args.event, json,
                             &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  args.runtime->free(args.runtime);
  EXPECT(args.runtime->state == NULL);
  EXPECT(args.runtime->_state_token == 0u);
  free(args.runtime);
}

static void test_runtime_owner_use_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson_error error;
  const char *json = "{\"id\":\"ev1\",\"ok\":true}";
  pthread_t parse_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_copy_parse_thread_args args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&args, 0, sizeof(args));
  args.runtime = lj;
  args.json = json;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&parse_thread, NULL,
                        test_runtime_copy_parse_thread_main, &args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(parse_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(parse_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(args.status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(args.event.id, "ev1") == 0);
  EXPECT(args.event.ok == true);
}

typedef struct test_runtime_array_stream_open_thread_args {
  lonejson *runtime;
  lonejson_array_stream *stream;
  lonejson_error error;
} test_runtime_array_stream_open_thread_args;

typedef struct test_runtime_stream_open_thread_args {
  lonejson *runtime;
  lonejson_stream *stream;
  lonejson_error error;
} test_runtime_stream_open_thread_args;

typedef struct test_runtime_sse_push_thread_args {
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;
  lonejson_sse *sse;
  test_event event;
} test_runtime_sse_push_thread_args;

typedef struct test_runtime_json_value_init_thread_args {
  lonejson *runtime;
  lonejson_json_value value;
} test_runtime_json_value_init_thread_args;

typedef struct test_runtime_spooled_init_thread_args {
  lonejson *runtime;
  lonejson_spooled spool;
} test_runtime_spooled_init_thread_args;

typedef struct test_runtime_serialize_thread_args {
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;
  test_item item;
  char buffer[64];
  size_t needed;
} test_runtime_serialize_thread_args;

static void *test_runtime_array_stream_open_thread_main(void *user) {
  test_runtime_array_stream_open_thread_args *args =
      (test_runtime_array_stream_open_thread_args *)user;
  test_reader_state reader;

  memset(&reader, 0, sizeof(reader));
  reader.json = "[123]";
  reader.chunk_size = 2u;
  args->stream = lonejson_array_stream_open_reader(
      args->runtime, "", test_state_reader, &reader, &args->error);
  return NULL;
}

static void *test_runtime_stream_open_thread_main(void *user) {
  test_runtime_stream_open_thread_args *args =
      (test_runtime_stream_open_thread_args *)user;
  test_reader_state reader;

  memset(&reader, 0, sizeof(reader));
  reader.json = "{\"id\":\"ev1\",\"ok\":true}";
  reader.chunk_size = 3u;
  args->stream = lonejson_stream_open_reader(
      args->runtime, &test_event_map, test_state_reader, &reader, &args->error);
  return NULL;
}

static void *test_runtime_sse_push_thread_main(void *user) {
  test_runtime_sse_push_thread_args *args =
      (test_runtime_sse_push_thread_args *)user;
  const char *framed = "event: item\ndata: {\"id\":\"ev1\",\"ok\":true}\n\n";
  lonejson_sse_json_options options;

  memset(&args->event, 0, sizeof(args->event));
  memset(&options, 0, sizeof(options));
  args->status = lonejson_sse_push_json(
      args->runtime, args->sse, &test_event_map, &args->event, framed,
      strlen(framed), &options, NULL, NULL, &args->error);
  return NULL;
}

static void *test_runtime_json_value_init_thread_main(void *user) {
  test_runtime_json_value_init_thread_args *args =
      (test_runtime_json_value_init_thread_args *)user;

  lonejson_json_value_init(args->runtime, &args->value);
  return NULL;
}

static void *test_runtime_spooled_init_thread_main(void *user) {
  test_runtime_spooled_init_thread_args *args =
      (test_runtime_spooled_init_thread_args *)user;

  lonejson_spooled_init_class(args->runtime, &args->spool,
                              LONEJSON_SPOOL_CLASS_BLOB);
  return NULL;
}

static void *test_runtime_serialize_thread_main(void *user) {
  test_runtime_serialize_thread_args *args =
      (test_runtime_serialize_thread_args *)user;

  args->status = lonejson_serialize_buffer(
      args->runtime, &test_item_map, &args->item, args->buffer,
      sizeof(args->buffer), &args->needed, &args->error);
  return NULL;
}

static void test_runtime_copy_array_stream_open_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson *copy;
  lonejson_error error;
  pthread_t open_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_array_stream_open_thread_args open_args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }
  copy = (lonejson *)malloc(sizeof(lonejson));
  EXPECT(copy != NULL);
  if (copy == NULL) {
    lonejson_free(lj);
    return;
  }
  *copy = *lj;

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&open_args, 0, sizeof(open_args));
  open_args.runtime = copy;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&open_thread, NULL,
                        test_runtime_array_stream_open_thread_main,
                        &open_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(open_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    free(copy);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(open_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(open_args.stream != NULL);
  if (open_args.stream != NULL) {
    lonejson_array_stream_close(open_args.stream);
  }
  EXPECT(lonejson_array_stream_open_push(copy, "", &error) == NULL);
  copy->free(copy);
  free(copy);
}

static void test_runtime_owner_stream_open_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson_error error;
  pthread_t open_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_stream_open_thread_args open_args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&open_args, 0, sizeof(open_args));
  open_args.runtime = lj;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&open_thread, NULL,
                        test_runtime_stream_open_thread_main, &open_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(open_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(open_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(open_args.stream != NULL);
  if (open_args.stream != NULL) {
    lonejson_stream_close(open_args.stream);
  }
}

static void test_runtime_owner_array_stream_open_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson_error error;
  pthread_t open_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_array_stream_open_thread_args open_args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&open_args, 0, sizeof(open_args));
  open_args.runtime = lj;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&open_thread, NULL,
                        test_runtime_array_stream_open_thread_main,
                        &open_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(open_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(open_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(open_args.stream != NULL);
  if (open_args.stream != NULL) {
    lonejson_array_stream_close(open_args.stream);
  }
}

static void test_runtime_copy_serialize_uses_snapshot_after_owner_free(void) {
  lonejson *lj;
  lonejson *copy;
  lonejson_error error;
  pthread_t serialize_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_serialize_thread_args args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }
  copy = (lonejson *)malloc(sizeof(lonejson));
  EXPECT(copy != NULL);
  if (copy == NULL) {
    lonejson_free(lj);
    return;
  }
  *copy = *lj;

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&args, 0, sizeof(args));
  args.runtime = copy;
  args.item.id = 42;
  strcpy(args.item.label, "runtime");
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&serialize_thread, NULL,
                        test_runtime_serialize_thread_main, &args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(serialize_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    free(copy);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(test_runtime_wait_free_done(&free_args, 1000L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(serialize_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(args.status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(args.buffer, "{\"id\":42,\"label\":\"runtime\"}") == 0);
  EXPECT(lonejson_serialize_buffer(
             copy, &test_item_map, &args.item, args.buffer, sizeof(args.buffer),
             &args.needed, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  copy->free(copy);
  free(copy);
}

static void test_runtime_copy_sse_push_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson *copy;
  lonejson_error error;
  lonejson_sse *sse;
  pthread_t push_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_sse_push_thread_args push_args;
  test_runtime_free_thread_args free_args;
  lonejson_sse_json_options options;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }
  copy = (lonejson *)malloc(sizeof(lonejson));
  EXPECT(copy != NULL);
  if (copy == NULL) {
    lonejson_free(lj);
    return;
  }
  *copy = *lj;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    free(copy);
    lonejson_free(lj);
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&push_args, 0, sizeof(push_args));
  push_args.runtime = copy;
  push_args.sse = sse;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;
  memset(&options, 0, sizeof(options));

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&push_thread, NULL, test_runtime_sse_push_thread_main,
                        &push_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(push_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_sse_close(sse);
    free(copy);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(push_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(push_args.status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(push_args.event.id, "ev1") == 0);
  EXPECT(push_args.event.ok == true);
  EXPECT(lonejson_sse_finish_json(copy, sse, &test_event_map, &push_args.event,
                                  &options, NULL, NULL,
                                  &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_sse_close(sse);
  copy->free(copy);
  free(copy);
}

static void test_runtime_owner_json_value_init_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson_error error;
  pthread_t init_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_json_value_init_thread_args init_args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&init_args, 0, sizeof(init_args));
  init_args.runtime = lj;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&init_thread, NULL,
                        test_runtime_json_value_init_thread_main,
                        &init_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(init_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(init_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(lonejson__json_value_is_initialized(&init_args.value));
  lonejson_json_value_cleanup(&init_args.value);
}

static void test_runtime_copy_json_value_init_waits_for_owner_free(void) {
  lonejson *lj;
  lonejson *copy;
  lonejson_error error;
  pthread_t init_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_json_value_init_thread_args init_args;
  test_runtime_free_thread_args free_args;

  lj = lonejson_new(NULL, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }
  copy = (lonejson *)malloc(sizeof(lonejson));
  EXPECT(copy != NULL);
  if (copy == NULL) {
    lonejson_free(lj);
    return;
  }
  *copy = *lj;

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&init_args, 0, sizeof(init_args));
  init_args.runtime = copy;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&init_thread, NULL,
                        test_runtime_json_value_init_thread_main,
                        &init_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(init_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    free(copy);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(init_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(lonejson__json_value_is_initialized(&init_args.value));
  lonejson_json_value_cleanup(&init_args.value);
  copy->free(copy);
  free(copy);
}

static void test_runtime_owner_spooled_init_waits_for_owner_free(void) {
  lonejson_config config;
  lonejson *lj;
  lonejson_error error;
  pthread_t init_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_spooled_init_thread_args init_args;
  test_runtime_free_thread_args free_args;

  config = lonejson_default_config();
  config.spool_blob.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&init_args, 0, sizeof(init_args));
  init_args.runtime = lj;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&init_thread, NULL,
                        test_runtime_spooled_init_thread_main,
                        &init_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(init_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(init_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(lonejson__spooled_is_initialized(&init_args.spool));
  lonejson_spooled_cleanup(&init_args.spool);
}

static void test_runtime_copy_spooled_init_waits_for_owner_free(void) {
  lonejson_config config;
  lonejson *lj;
  lonejson *copy;
  lonejson_error error;
  pthread_t init_thread;
  pthread_t free_thread;
  test_runtime_borrow_barrier barrier;
  test_runtime_spooled_init_thread_args init_args;
  test_runtime_free_thread_args free_args;

  config = lonejson_default_config();
  config.spool_blob.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }
  copy = (lonejson *)malloc(sizeof(lonejson));
  EXPECT(copy != NULL);
  if (copy == NULL) {
    lonejson_free(lj);
    return;
  }
  *copy = *lj;

  memset(&barrier, 0, sizeof(barrier));
  EXPECT(pthread_mutex_init(&barrier.mutex, NULL) == 0);
  EXPECT(pthread_cond_init(&barrier.cond, NULL) == 0);
  memset(&init_args, 0, sizeof(init_args));
  init_args.runtime = copy;
  memset(&free_args, 0, sizeof(free_args));
  free_args.runtime = lj;
  free_args.done_mutex = &barrier.mutex;
  free_args.done_cond = &barrier.cond;

  lonejson__test_set_runtime_borrow_hook(test_runtime_borrow_hook_block,
                                         &barrier);
  EXPECT(pthread_create(&init_thread, NULL,
                        test_runtime_spooled_init_thread_main,
                        &init_args) == 0);
  EXPECT(test_runtime_borrow_wait_entered(&barrier, 1000L));
  if (!barrier.entered) {
    EXPECT(pthread_join(init_thread, NULL) == 0);
    lonejson__test_set_runtime_borrow_hook(NULL, NULL);
    EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
    EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);
    free(copy);
    lonejson_free(lj);
    return;
  }

  EXPECT(pthread_create(&free_thread, NULL, test_runtime_owner_free_thread_main,
                        &free_args) == 0);
  EXPECT(!test_runtime_wait_free_done(&free_args, 10L));

  pthread_mutex_lock(&barrier.mutex);
  barrier.release = 1;
  pthread_cond_broadcast(&barrier.cond);
  pthread_mutex_unlock(&barrier.mutex);

  EXPECT(pthread_join(init_thread, NULL) == 0);
  EXPECT(pthread_join(free_thread, NULL) == 0);
  lonejson__test_set_runtime_borrow_hook(NULL, NULL);
  EXPECT(pthread_cond_destroy(&barrier.cond) == 0);
  EXPECT(pthread_mutex_destroy(&barrier.mutex) == 0);

  EXPECT(lonejson__spooled_is_initialized(&init_args.spool));
  lonejson_spooled_cleanup(&init_args.spool);
  copy->free(copy);
  free(copy);
}
#endif

static lonejson *test_new_runtime_from_stack_config(
    const lonejson_allocator **allocator_addr_out,
    const char **default_temp_dir_addr_out, const char **blob_temp_dir_addr_out,
    const char **large_text_temp_dir_addr_out, lonejson_error *error) {
  lonejson_config config;
  lonejson_allocator allocator;
  char default_temp_dir[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lj-stack-default";
  char blob_temp_dir[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lj-stack-blob";
  char large_text_temp_dir[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lj-stack-large";

  allocator = lonejson_default_allocator();
  config = lonejson_default_config();
  config.allocator = &allocator;
  config.spool_default.temp_dir = default_temp_dir;
  config.spool_blob.temp_dir = blob_temp_dir;
  config.spool_large_text.temp_dir = large_text_temp_dir;

  if (allocator_addr_out != NULL) {
    *allocator_addr_out = config.allocator;
  }
  if (default_temp_dir_addr_out != NULL) {
    *default_temp_dir_addr_out = config.spool_default.temp_dir;
  }
  if (blob_temp_dir_addr_out != NULL) {
    *blob_temp_dir_addr_out = config.spool_blob.temp_dir;
  }
  if (large_text_temp_dir_addr_out != NULL) {
    *large_text_temp_dir_addr_out = config.spool_large_text.temp_dir;
  }
  return lonejson_new(&config, error);
}

static void test_runtime_rejects_raw_partial_configs(void) {
  lonejson_config raw_config;
  lonejson_error error;
  lonejson *lj;

  memset(&raw_config, 0, sizeof(raw_config));
  raw_config.clear_destination_by_default = 0;
  lj = lonejson_new(&raw_config, &error);
  EXPECT(lj == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "lonejson_default_config()") != NULL);
}

static void test_runtime_owns_config_backing_storage(void) {
  const lonejson_allocator *borrowed_allocator = NULL;
  const char *borrowed_default_temp_dir = NULL;
  const char *borrowed_blob_temp_dir = NULL;
  const char *borrowed_large_text_temp_dir = NULL;
  lonejson_error error;
  lonejson *lj;
  const lonejson_runtime *runtime_state;

  lj = test_new_runtime_from_stack_config(
      &borrowed_allocator, &borrowed_default_temp_dir, &borrowed_blob_temp_dir,
      &borrowed_large_text_temp_dir, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  runtime_state = lonejson__runtime_data_const(lj);
  EXPECT(runtime_state != NULL);
  EXPECT(runtime_state->config.allocator != NULL);
  EXPECT(runtime_state->config.allocator != borrowed_allocator);
  EXPECT(runtime_state->config.allocator->malloc_fn ==
         lonejson_default_allocator().malloc_fn);
  EXPECT(runtime_state->config.allocator->realloc_fn ==
         lonejson_default_allocator().realloc_fn);
  EXPECT(runtime_state->config.allocator->free_fn ==
         lonejson_default_allocator().free_fn);

  EXPECT(runtime_state->config.spool_default.temp_dir !=
         borrowed_default_temp_dir);
  EXPECT(runtime_state->config.spool_blob.temp_dir != borrowed_blob_temp_dir);
  EXPECT(runtime_state->config.spool_large_text.temp_dir !=
         borrowed_large_text_temp_dir);
  EXPECT(strcmp(runtime_state->config.spool_default.temp_dir,
                LONEJSON_SOURCE_DIR "/build/test-tmp/lj-stack-default") == 0);
  EXPECT(strcmp(runtime_state->config.spool_blob.temp_dir,
                LONEJSON_SOURCE_DIR "/build/test-tmp/lj-stack-blob") == 0);
  EXPECT(strcmp(runtime_state->config.spool_large_text.temp_dir,
                LONEJSON_SOURCE_DIR "/build/test-tmp/lj-stack-large") == 0);
  EXPECT(runtime_state->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT].temp_dir ==
         runtime_state->config.spool_default.temp_dir);
  EXPECT(runtime_state->spool_options[LONEJSON_SPOOL_CLASS_BLOB].temp_dir ==
         runtime_state->config.spool_blob.temp_dir);
  EXPECT(
      runtime_state->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT].temp_dir ==
      runtime_state->config.spool_large_text.temp_dir);

  lonejson_free(lj);
}

static void test_runtime_preserves_caller_owned_fixed_string_scratch(void) {
  lonejson_config config;
  lonejson_error error;
  lonejson *lj;
  const lonejson_runtime *runtime_state;
  unsigned char fixed_string_scratch[128];

  memset(fixed_string_scratch, 0xA5, sizeof(fixed_string_scratch));
  config = lonejson_default_config();
  config.fixed_string_scratch = fixed_string_scratch;
  config.fixed_string_scratch_size = sizeof(fixed_string_scratch);
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  runtime_state = lonejson__runtime_data_const(lj);
  EXPECT(runtime_state != NULL);
  EXPECT(runtime_state->config.fixed_string_scratch == fixed_string_scratch);
  EXPECT(runtime_state->config.fixed_string_scratch_size ==
         sizeof(fixed_string_scratch));
  EXPECT(runtime_state->parse_options.fixed_string_scratch ==
         fixed_string_scratch);
  EXPECT(runtime_state->parse_options.fixed_string_scratch_size ==
         sizeof(fixed_string_scratch));

  lonejson_free(lj);
}

static void test_runtime_instance_limits_are_isolated(void) {
  lonejson_config config;
  lonejson *strict_lj;
  lonejson *default_lj;
  lonejson_error error;
  test_alloc_parse_doc doc;
  const char *json = "{\"name\":\"abcdef\"}";

  config = lonejson_default_config();
  config.max_dynamic_string_bytes = 4u;
  strict_lj = lonejson_new(&config, &error);
  EXPECT(strict_lj != NULL);
  default_lj = lonejson_new(NULL, &error);
  EXPECT(default_lj != NULL);

  memset(&doc, 0, sizeof(doc));
  EXPECT(lonejson_parse_cstr(default_lj, &test_alloc_parse_doc_map, &doc, json,
                             &error) == LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc);

  memset(&doc, 0, sizeof(doc));
  EXPECT(strict_lj->parse_cstr(strict_lj, &test_alloc_parse_doc_map, &doc, json,
                               &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc);

  memset(&doc, 0, sizeof(doc));
  EXPECT(default_lj->parse_cstr(default_lj, &test_alloc_parse_doc_map, &doc,
                                json, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.name, "abcdef") == 0);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc);

  lonejson_free(strict_lj);
  lonejson_free(default_lj);
}

static void test_mutated_map_analysis_recomputes(void) {
  static lonejson_field fields[1];
  static lonejson_map map;

  memset(&map, 0, sizeof(map));
  map.name = "test_mutated_map_analysis";

  fields[0] = test_large_fixed_string_doc_fields[0];
  map.struct_size = sizeof(test_large_fixed_string_doc);
  map.fields = fields;
  map.field_count = 1u;
  EXPECT(lonejson__map_layout_is_valid(&map));
  EXPECT(!lonejson__map_may_allocate(&map));
  EXPECT(lonejson__map_adopt_mask(&map) == 0u);

  fields[0] = test_alloc_parse_doc_fields[0];
  map.struct_size = sizeof(test_alloc_parse_doc);
  EXPECT(lonejson__map_layout_is_valid(&map));
  EXPECT(lonejson__map_may_allocate(&map));
  EXPECT(lonejson__map_adopt_mask(&map) == 1u);

  fields[0].struct_offset = sizeof(test_alloc_parse_doc);
  EXPECT(!lonejson__map_layout_is_valid(&map));
}

static void test_heap_map_cookie_change_invalidates_cache(void) {
  static lonejson_field fields[1];
  static lonejson_map map;

  memset(&map, 0, sizeof(map));
  map.name = "test_heap_map_cookie_change";
  map.fields = fields;
  map.field_count = 1u;
  map._map_identity = &map;
  map._map_cookie = LONEJSON__MAP_COOKIE_VALUE;

  fields[0] = test_large_fixed_string_doc_fields[0];
  map.struct_size = sizeof(test_large_fixed_string_doc);
  EXPECT(lonejson__map_layout_is_valid(&map));
  EXPECT(!lonejson__map_may_allocate(&map));
  EXPECT(lonejson__map_adopt_mask(&map) == 0u);

  fields[0] = test_alloc_parse_doc_fields[0];
  map.struct_size = sizeof(test_alloc_parse_doc);
  map._map_cookie =
      (LONEJSON__MAP_COOKIE_VALUE & ~((lonejson_uint64)0xffffffffu)) | 0x1u;
  EXPECT(lonejson__map_layout_is_valid(&map));
  EXPECT(lonejson__map_may_allocate(&map));
  EXPECT(lonejson__map_adopt_mask(&map) == 1u);
}

static void test_macro_map_cookies_are_unique_and_non_sentinel(void) {
  EXPECT(test_alloc_parse_doc_map._map_cookie != LONEJSON__MAP_COOKIE_VALUE);
  EXPECT(test_large_fixed_string_doc_map._map_cookie !=
         LONEJSON__MAP_COOKIE_VALUE);
  EXPECT(test_alloc_parse_doc_map._map_cookie !=
         test_large_fixed_string_doc_map._map_cookie);
}

static void test_copied_macro_map_parse_revalidates_layout(void) {
  lonejson_field fields[1];
  lonejson_map map;
  test_alloc_parse_doc doc;
  lonejson_error error;
  lonejson_status status;

  map = test_alloc_parse_doc_map;
  fields[0] = test_alloc_parse_doc_fields[0];
  map.fields = fields;
  map.field_count = 1u;

  EXPECT(lonejson__map_cacheable(&test_alloc_parse_doc_map));
  EXPECT(!lonejson__map_cacheable(&map));

  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(test_default_runtime(), &map, &doc,
                               "{\"name\":\"alpha\"}", &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.name, "alpha") == 0);
  lonejson_cleanup(&map, &doc);

  fields[0].struct_offset = sizeof(test_alloc_parse_doc);
  EXPECT(!lonejson__map_layout_is_valid(&map));
  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(test_default_runtime(), &map, &doc,
                               "{\"name\":\"beta\"}", &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "invalid field layout") != NULL);
}

static void test_macro_map_copy_is_not_cacheable(void) {
  lonejson_map map;

  EXPECT(lonejson__map_cacheable(&test_alloc_parse_doc_map));
  map = test_alloc_parse_doc_map;
  EXPECT(!lonejson__map_cacheable(&map));
}

static void test_mutated_map_parse_revalidates_layout(void) {
  static lonejson_field fields[1];
  static lonejson_map map;
  test_alloc_parse_doc doc;
  lonejson_error error;
  lonejson_status status;

  memset(&map, 0, sizeof(map));
  map.name = "test_mutated_map_parse";
  map.struct_size = sizeof(test_alloc_parse_doc);
  map.fields = fields;
  map.field_count = 1u;
  fields[0] = test_alloc_parse_doc_fields[0];

  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(test_default_runtime(), &map, &doc,
                               "{\"name\":\"alpha\"}", &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.name, "alpha") == 0);
  lonejson_cleanup(&map, &doc);

  fields[0].struct_offset = sizeof(test_alloc_parse_doc);
  EXPECT(!lonejson__map_layout_is_valid(&map));
  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(test_default_runtime(), &map, &doc,
                               "{\"name\":\"beta\"}", &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "invalid field layout") != NULL);
}

static void test_runtime_init_seeds_mapped_json_value_limits(void) {
  lonejson_config config;
  lonejson *limited_lj;
  lonejson *default_lj;
  lonejson_error error;
  lonejson_value_visitor visitor;
  test_nested_json_value_doc doc;
  test_buffer_sink sink;
  unsigned char sink_bytes[64];
  const char *json =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",\"payload\":"
      "{\"value\":\"ab\"}}}";
  lonejson_status status;

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  config.json_value_max_string_bytes = 1u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  default_lj = lonejson_new(&config, &error);
  EXPECT(default_lj != NULL);
  if (limited_lj == NULL || default_lj == NULL) {
    lonejson_free(limited_lj);
    lonejson_free(default_lj);
    return;
  }

  visitor = lonejson_default_value_visitor();
  sink.buffer = sink_bytes;
  sink.capacity = sizeof(sink_bytes);

  poison_bytes(&doc, sizeof(doc), 0xA5u);
  lonejson_init(limited_lj, &test_nested_json_value_doc_map, &doc);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);
  EXPECT(lonejson_json_value_set_parse_visitor(&doc.response.payload, &visitor,
                                               NULL,
                                               &error) == LONEJSON_STATUS_OK);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);
  status = lonejson_parse_cstr(limited_lj, &test_nested_json_value_doc_map,
                               &doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc);

  poison_bytes(&doc, sizeof(doc), 0xB5u);
  lonejson_init(limited_lj, &test_nested_json_value_doc_map, &doc);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);
  EXPECT(lonejson_json_value_enable_parse_capture(
             &doc.response.payload, &error) == LONEJSON_STATUS_OK);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc);

  poison_bytes(&doc, sizeof(doc), 0xC5u);
  lonejson_init(limited_lj, &test_nested_json_value_doc_map, &doc);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);
  sink.length = 0u;
  EXPECT(lonejson_json_value_set_parse_sink(&doc.response.payload,
                                            test_buffer_sink_write, &sink,
                                            &error) == LONEJSON_STATUS_OK);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc);

  poison_bytes(&doc, sizeof(doc), 0x5Au);
  lonejson_init(default_lj, &test_nested_json_value_doc_map, &doc);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes ==
         1024u * 1024u);
  EXPECT(lonejson_json_value_set_parse_visitor(&doc.response.payload, &visitor,
                                               NULL,
                                               &error) == LONEJSON_STATUS_OK);
  status = lonejson_parse_cstr(default_lj, &test_nested_json_value_doc_map,
                               &doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc);

  lonejson_free(limited_lj);
  lonejson_free(default_lj);
}

static void test_runtime_json_value_set_buffer_preserves_limits(void) {
  lonejson_config config;
  lonejson *limited_lj;
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;

  config = lonejson_default_config();
  config.json_value_max_string_bytes = 1u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  if (limited_lj == NULL) {
    return;
  }

  lonejson_json_value_init(limited_lj, &value);
  status = lonejson_json_value_set_buffer(&value, "\"ab\"", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.parse_visitor_limits.max_string_bytes == 1u);
  EXPECT(value.runtime_parse_visitor_limits.max_string_bytes == 1u);
  lonejson_json_value_cleanup(&value);
  lonejson_free(limited_lj);
}

static void test_runtime_json_value_reset_preserves_limits(void) {
  lonejson_config config;
  lonejson *limited_lj;
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;

  config = lonejson_default_config();
  config.json_value_max_string_bytes = 1u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  if (limited_lj == NULL) {
    return;
  }

  lonejson_json_value_init(limited_lj, &value);
  EXPECT(value.parse_visitor_limits.max_string_bytes == 1u);
  status = lonejson_json_value_enable_parse_capture(&value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.parse_visitor_limits.max_string_bytes == 1u);
  status = lonejson_json_value_set_buffer(&value, "\"o\"", 3u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_json_value_reset(&value);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(value.parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE);
  EXPECT(value.parse_visitor_limits.max_string_bytes == 1u);

  status = lonejson_json_value_enable_parse_capture(&value, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.parse_visitor_limits.max_string_bytes == 1u);
  status = lonejson_json_value_set_buffer(&value, "\"o\"", 3u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_json_value_cleanup(&value);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(value.parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE);
  EXPECT(value.parse_visitor_limits.max_string_bytes == 1u);

  lonejson_free(limited_lj);
}

static void test_runtime_reuse_reseeds_json_value_limits(void) {
  lonejson_config strict_config;
  lonejson_config relaxed_config;
  lonejson *strict_lj;
  lonejson *relaxed_lj;
  lonejson_error error;
  lonejson_value_visitor visitor;
  test_nested_json_value_doc doc;
  const char *json =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",\"payload\":"
      "{\"value\":\"ab\"}}}";
  lonejson_status status;

  strict_config = lonejson_default_config();
  strict_config.clear_destination_by_default = 0;
  strict_config.json_value_max_string_bytes = 1u;
  strict_lj = lonejson_new(&strict_config, &error);
  EXPECT(strict_lj != NULL);

  relaxed_config = lonejson_default_config();
  relaxed_config.clear_destination_by_default = 0;
  relaxed_lj = lonejson_new(&relaxed_config, &error);
  EXPECT(relaxed_lj != NULL);
  if (strict_lj == NULL || relaxed_lj == NULL) {
    lonejson_free(strict_lj);
    lonejson_free(relaxed_lj);
    return;
  }

  visitor = lonejson_default_value_visitor();
  poison_bytes(&doc, sizeof(doc), 0x6Au);
  lonejson_init(relaxed_lj, &test_nested_json_value_doc_map, &doc);
  EXPECT(lonejson_json_value_set_parse_visitor(&doc.response.payload, &visitor,
                                               NULL,
                                               &error) == LONEJSON_STATUS_OK);
  status = lonejson_parse_cstr(relaxed_lj, &test_nested_json_value_doc_map,
                               &doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes ==
         1024u * 1024u);

  status = lonejson_parse_cstr(strict_lj, &test_nested_json_value_doc_map, &doc,
                               json, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(doc.response.payload.parse_visitor_limits.max_string_bytes == 1u);

  lonejson_cleanup(&test_nested_json_value_doc_map, &doc);
  lonejson_free(strict_lj);
  lonejson_free(relaxed_lj);
}

static void test_spooled_invalid_class_uses_default_runtime_policy(void) {
  lonejson_config config;
  lonejson *limited_lj;
  lonejson_spooled value;
  lonejson_error error;
  lonejson_status status;

  config = lonejson_default_config();
  config.spool_default.max_bytes = 4u;
  config.spool_blob.max_bytes = 64u;
  config.spool_large_text.max_bytes = 128u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  if (limited_lj == NULL) {
    return;
  }

  lonejson_spooled_init_class(limited_lj, &value, (lonejson_spool_class)99);
  status = lonejson_spooled_append(&value, "abcd", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_spooled_append(&value, "e", 1u, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  lonejson_spooled_cleanup(&value);

  lonejson_free(limited_lj);
}

static void test_stream_field_invalid_class_uses_default_runtime_policy(void) {
  static lonejson_field fields[1];
  static lonejson_map invalid_spool_map;
  static const char *json = "{\"text\":\"abcde\"}";
  lonejson_config config;
  lonejson *limited_lj;
  test_spool_limits_doc doc;
  lonejson_error error;
  lonejson_status status;

  fields[0] = test_spool_limits_doc_fields[0];
  fields[0].reserved_policy = NULL;
  fields[0].spool_class = (lonejson_spool_class)99;
  invalid_spool_map.name = "test_invalid_spool_class_doc";
  invalid_spool_map.struct_size = sizeof(test_spool_limits_doc);
  invalid_spool_map.fields = fields;
  invalid_spool_map.field_count = 1u;

  config = lonejson_default_config();
  config.spool_default.max_bytes = 4u;
  config.spool_blob.max_bytes = 64u;
  config.spool_large_text.max_bytes = 128u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  if (limited_lj == NULL) {
    return;
  }

  memset(&doc, 0, sizeof(doc));
  status =
      lonejson_parse_cstr(limited_lj, &invalid_spool_map, &doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  lonejson_cleanup(&invalid_spool_map, &doc);
  lonejson_free(limited_lj);
}

static void test_runtime_reuse_reseeds_spool_policy(void) {
  static lonejson_field fields[1];
  static lonejson_map runtime_spool_map;
  static const char *json = "{\"text\":\"abcde\"}";
  lonejson_config strict_config;
  lonejson_config relaxed_config;
  lonejson *strict_lj;
  lonejson *relaxed_lj;
  test_spool_limits_doc doc;
  lonejson_error error;
  lonejson_status status;

  fields[0] = test_spool_limits_doc_fields[0];
  fields[0].reserved_policy = NULL;
  fields[0].spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
  runtime_spool_map.name = "test_runtime_spool_reseed_doc";
  runtime_spool_map.struct_size = sizeof(test_spool_limits_doc);
  runtime_spool_map.fields = fields;
  runtime_spool_map.field_count = 1u;

  strict_config = lonejson_default_config();
  strict_config.clear_destination_by_default = 0;
  strict_config.spool_default.max_bytes = 4u;
  strict_lj = lonejson_new(&strict_config, &error);
  EXPECT(strict_lj != NULL);

  relaxed_config = lonejson_default_config();
  relaxed_config.clear_destination_by_default = 0;
  relaxed_config.spool_default.max_bytes = 64u;
  relaxed_lj = lonejson_new(&relaxed_config, &error);
  EXPECT(relaxed_lj != NULL);
  if (strict_lj == NULL || relaxed_lj == NULL) {
    lonejson_free(strict_lj);
    lonejson_free(relaxed_lj);
    return;
  }

  memset(&doc, 0, sizeof(doc));
  status =
      lonejson_parse_cstr(relaxed_lj, &runtime_spool_map, &doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&doc.text) == 5u);

  lonejson_reset(strict_lj, &runtime_spool_map, &doc);
  EXPECT(doc.text.max_bytes == 4u);
  status =
      lonejson_parse_cstr(strict_lj, &runtime_spool_map, &doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  lonejson_cleanup(&runtime_spool_map, &doc);
  lonejson_free(strict_lj);
  lonejson_free(relaxed_lj);
}

static void test_spooled_handle_owns_temp_dir_after_runtime_free(void) {
  lonejson_config config;
  lonejson *lj;
  const lonejson_runtime *runtime_state;
  lonejson_spooled value;
  lonejson_error error;
  lonejson_status status;
  struct stat st;

  config = lonejson_default_config();
  config.spool_default.memory_limit = 1u;
  config.spool_default.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  runtime_state = lonejson__runtime_data_const(lj);
  lonejson_spooled_init(lj, &value);
  EXPECT(value.temp_dir != NULL);
  EXPECT(value.temp_dir != runtime_state->config.spool_default.temp_dir);
  EXPECT(strcmp(value.temp_dir, LONEJSON_SOURCE_DIR "/build/test-tmp") == 0);

  lonejson_free(lj);
  status = lonejson_spooled_append(&value, "xx", 2u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.spilled == 1);
  EXPECT(value.temp_path[0] != '\0');
  EXPECT(stat(value.temp_path, &st) == 0);

  lonejson_spooled_cleanup(&value);
}

static void test_spooled_reset_preserves_long_temp_dir(void) {
  lonejson__spool_options options;
  lonejson_spooled value;
  size_t prefix_len = strlen(LONEJSON_SOURCE_DIR "/build/test-tmp/");
  size_t long_len = LONEJSON_SPOOL_TEMP_PATH_CAPACITY + 32u;
  char *long_temp_dir = (char *)malloc(long_len + 1u);

  EXPECT(long_temp_dir != NULL);
  if (long_temp_dir == NULL) {
    return;
  }

  memcpy(long_temp_dir, LONEJSON_SOURCE_DIR "/build/test-tmp/", prefix_len);
  memset(long_temp_dir + prefix_len, 'x', long_len - prefix_len);
  long_temp_dir[long_len] = '\0';

  options.memory_limit = 1u;
  options.max_bytes = 8u;
  options.temp_dir = long_temp_dir;
  lonejson_spooled_init_with_allocator(&value, &options, NULL);
  EXPECT(value.temp_dir != NULL);
  EXPECT(value.temp_dir != long_temp_dir);
  EXPECT(strcmp(value.temp_dir, long_temp_dir) == 0);

  lonejson_spooled_reset(&value);
  EXPECT(value.temp_dir != NULL);
  EXPECT(strcmp(value.temp_dir, long_temp_dir) == 0);

  lonejson_spooled_cleanup(&value);
  free(long_temp_dir);
}

static void
test_runtime_init_spooled_field_owns_temp_dir_after_runtime_free(void) {
  static lonejson_field fields[1];
  static lonejson_map runtime_spool_map;
  lonejson_config config;
  lonejson *lj;
  const lonejson_runtime *runtime_state;
  test_spool_limits_doc doc;
  lonejson_error error;
  lonejson_status status;
  struct stat st;

  config = lonejson_default_config();
  config.spool_default.memory_limit = 1u;
  config.spool_default.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  fields[0] = test_spool_limits_doc_fields[0];
  fields[0].reserved_policy = NULL;
  fields[0].spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
  runtime_spool_map.name = "test_runtime_init_spooled_field_temp_dir_doc";
  runtime_spool_map.struct_size = sizeof(test_spool_limits_doc);
  runtime_spool_map.fields = fields;
  runtime_spool_map.field_count = 1u;

  memset(&doc, 0, sizeof(doc));
  runtime_state = lonejson__runtime_data_const(lj);
  lonejson_init(lj, &runtime_spool_map, &doc);
  EXPECT(doc.text.temp_dir != NULL);
  EXPECT(doc.text.temp_dir != runtime_state->config.spool_default.temp_dir);
  EXPECT(strcmp(doc.text.temp_dir, LONEJSON_SOURCE_DIR "/build/test-tmp") == 0);

  lonejson_free(lj);
  status = lonejson_spooled_append(&doc.text, "xx", 2u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.text.spilled == 1);
  EXPECT(doc.text.temp_path[0] != '\0');
  EXPECT(stat(doc.text.temp_path, &st) == 0);

  lonejson_cleanup(&runtime_spool_map, &doc);
}

static const lonejson_map *test_runtime_default_spool_map(void) {
  static lonejson_field fields[1];
  static lonejson_map runtime_spool_map;

  fields[0] = test_spool_limits_doc_fields[0];
  fields[0].reserved_policy = NULL;
  fields[0].spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
  runtime_spool_map.name = "test_runtime_default_spool_doc";
  runtime_spool_map.struct_size = sizeof(test_spool_limits_doc);
  runtime_spool_map.fields = fields;
  runtime_spool_map.field_count = 1u;
  return &runtime_spool_map;
}

static void
test_runtime_parse_counts_stream_temp_dir_against_alloc_budget(void) {
  const lonejson_map *map = test_runtime_default_spool_map();
  lonejson_config config;
  lonejson *lj;
  lonejson_error error;
  test_spool_limits_doc doc;
  lonejson_status status;

  config = lonejson_default_config();
  config.max_alloc_bytes = 1u;
  config.spool_default.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(lj, map, &doc, "{\"text\":\"\"}", &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(doc.text.temp_dir == NULL);
  lonejson_cleanup(map, &doc);
  lonejson_free(lj);
}

static void
test_runtime_stream_counts_stream_temp_dir_against_alloc_budget(void) {
  const lonejson_map *map = test_runtime_default_spool_map();
  lonejson_config config;
  lonejson *lj;
  lonejson_stream *stream;
  lonejson_error error;
  test_spool_limits_doc doc;
  test_reader_state state;
  lonejson_stream_result result;

  config = lonejson_default_config();
  config.max_alloc_bytes = 1u;
  config.spool_default.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&state, 0, sizeof(state));
  state.json = "{\"text\":\"\"}";
  state.chunk_size = 5u;
  stream =
      lonejson_stream_open_reader(lj, map, test_state_reader, &state, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_free(lj);
    return;
  }

  memset(&doc, 0, sizeof(doc));
  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_ERROR);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(doc.text.temp_dir == NULL);

  lonejson_stream_close(stream);
  lonejson_cleanup(map, &doc);
  lonejson_free(lj);
}

static void
test_runtime_array_stream_counts_stream_temp_dir_against_alloc_budget(void) {
  const lonejson_map *map = test_runtime_default_spool_map();
  lonejson_config config;
  lonejson *lj;
  lonejson_array_stream *stream;
  lonejson_error error;
  test_spool_limits_doc doc;
  test_reader_state state;
  lonejson_array_stream_result result;

  config = lonejson_default_config();
  config.max_alloc_bytes = 1u;
  config.spool_default.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&state, 0, sizeof(state));
  state.json = "[{\"text\":\"\"}]";
  state.chunk_size = 6u;
  stream = lonejson_array_stream_open_reader(lj, "", test_state_reader, &state,
                                             &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_free(lj);
    return;
  }

  memset(&doc, 0, sizeof(doc));
  result = lonejson_array_stream_next(stream, map, &doc, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(doc.text.temp_dir == NULL);

  lonejson_array_stream_close(stream);
  lonejson_cleanup(map, &doc);
  lonejson_free(lj);
}

static void test_spooled_temp_dir_oom_falls_back_to_anonymous_spill(void) {
  lonejson__spool_options options = lonejson__default_spool_options();
  test_fail_after_allocator_state alloc_state;
  lonejson_spooled value;
  lonejson_error error;
  lonejson_status status;

  options.memory_limit = 0u;
  options.temp_dir = LONEJSON_SOURCE_DIR "/build/test-tmp";
  test_fail_after_allocator_init(&alloc_state, 0u);
  lonejson_spooled_init_with_allocator(&value, &options,
                                       &alloc_state.allocator);
  EXPECT(value.temp_dir == NULL);

  status = lonejson_spooled_append(&value, "xx", 2u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.spilled == 1);
  EXPECT(value.spill_fp != NULL);
  EXPECT(value.temp_path[0] == '\0');

  lonejson_spooled_cleanup(&value);
}

static void test_runtime_reset_preserves_field_allocators(void) {
  static lonejson_field fields[1];
  static lonejson_map runtime_spool_map;
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *lj;
  lonejson_error error;
  test_nested_json_value_doc json_doc;
  test_spool_limits_doc spool_doc;
  lonejson_status status;

  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  fields[0] = test_spool_limits_doc_fields[0];
  fields[0].reserved_policy = NULL;
  fields[0].spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
  runtime_spool_map.name = "test_runtime_reset_allocator_doc";
  runtime_spool_map.struct_size = sizeof(test_spool_limits_doc);
  runtime_spool_map.fields = fields;
  runtime_spool_map.field_count = 1u;

  memset(&json_doc, 0, sizeof(json_doc));
  lonejson_init(lj, &test_nested_json_value_doc_map, &json_doc);
  reset_lonejson_alloc_stats();
  EXPECT(lonejson_json_value_enable_parse_capture(
             &json_doc.response.payload, &error) == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_buffer(&json_doc.response.payload,
                                          "{\"k\":1}", 7u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson__json_value_is_initialized(&json_doc.response.payload));
  EXPECT(g_lonejson_alloc_calls > 0u);
  reset_lonejson_alloc_stats();
  lonejson_reset(lj, &test_nested_json_value_doc_map, &json_doc);
  EXPECT(g_lonejson_free_calls > 0u);
  EXPECT(lonejson__json_value_is_initialized(&json_doc.response.payload));
  reset_lonejson_alloc_stats();
  EXPECT(lonejson_json_value_enable_parse_capture(
             &json_doc.response.payload, &error) == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_buffer(&json_doc.response.payload,
                                          "{\"k\":1}", 7u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson__json_value_is_initialized(&json_doc.response.payload));
  EXPECT(g_lonejson_alloc_calls > 0u);
  lonejson_cleanup(&test_nested_json_value_doc_map, &json_doc);

  memset(&spool_doc, 0, sizeof(spool_doc));
  lonejson_init(lj, &runtime_spool_map, &spool_doc);
  reset_lonejson_alloc_stats();
  status = lonejson_spooled_append(&spool_doc.text, "abc", 3u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls > 0u);
  lonejson_reset(lj, &runtime_spool_map, &spool_doc);
  reset_lonejson_alloc_stats();
  status = lonejson_spooled_append(&spool_doc.text, "abc", 3u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls > 0u);
  lonejson_cleanup(&runtime_spool_map, &spool_doc);

  lonejson_free(lj);
}

static void test_runtime_streams_survive_runtime_free(void) {
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *lj;
  lonejson_error error;
  lonejson_stream *stream;
  lonejson_stream_result stream_result;
  lonejson_array_stream *array_stream;
  lonejson_array_stream_result array_result;
  test_item item;
  char object_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-runtime-stream-XXXXXX";
  char array_path[] = LONEJSON_SOURCE_DIR
      "/build/test-tmp/lonejson-runtime-array-stream-XXXXXX";
  int fd;

  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  fd = write_temp_text_file(object_path, "{\"id\":1,\"label\":\"one\"}\n"
                                         "{\"id\":2,\"label\":\"two\"}\n");
  EXPECT(fd >= 0);
  if (fd < 0) {
    lonejson_free(lj);
    return;
  }
  close(fd);
  stream = lonejson_stream_open_path(lj, &test_item_map, object_path, &error);
  EXPECT(stream != NULL);

  fd = write_temp_text_file(array_path,
                            "{\"items\":[{\"id\":3,\"label\":\"three\"}]}");
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
  }
  array_stream =
      lonejson_array_stream_open_path(lj, "items", array_path, &error);
  EXPECT(array_stream != NULL);

  lonejson_free(lj);

  if (stream != NULL) {
    reset_lonejson_alloc_stats();
    poison_bytes(&item, sizeof(item), 0xA5u);
    stream_result = lonejson_stream_next(stream, &item, &error);
    EXPECT(stream_result == LONEJSON_STREAM_OBJECT);
    EXPECT(item.id == 1);
    EXPECT(strcmp(item.label, "one") == 0);
    lonejson_cleanup(&test_item_map, &item);
    poison_bytes(&item, sizeof(item), 0xB5u);
    stream_result = lonejson_stream_next(stream, &item, &error);
    EXPECT(stream_result == LONEJSON_STREAM_OBJECT);
    EXPECT(item.id == 2);
    EXPECT(strcmp(item.label, "two") == 0);
    lonejson_cleanup(&test_item_map, &item);
    stream_result = lonejson_stream_next(stream, &item, &error);
    EXPECT(stream_result == LONEJSON_STREAM_EOF);
    lonejson_stream_close(stream);
    EXPECT(g_lonejson_free_calls > 0u);
  }

  if (array_stream != NULL) {
    reset_lonejson_alloc_stats();
    poison_bytes(&item, sizeof(item), 0xC5u);
    array_result =
        lonejson_array_stream_next(array_stream, &test_item_map, &item, &error);
    EXPECT(array_result == LONEJSON_ARRAY_STREAM_ITEM);
    EXPECT(item.id == 3);
    EXPECT(strcmp(item.label, "three") == 0);
    lonejson_cleanup(&test_item_map, &item);
    array_result =
        lonejson_array_stream_next(array_stream, &test_item_map, &item, &error);
    EXPECT(array_result == LONEJSON_ARRAY_STREAM_EOF);
    lonejson_array_stream_close(array_stream);
    EXPECT(g_lonejson_free_calls > 0u);
  }

  unlink(object_path);
  unlink(array_path);
}

static void test_runtime_handle_counters_use_native_width(void) {
  EXPECT(sizeof(((lonejson__runtime_handle *)0)->generation) ==
         sizeof(unsigned int));
  EXPECT(sizeof(((lonejson__runtime_handle *)0)->active_pins) ==
         sizeof(unsigned int));
}

static void test_runtime_instances_operate_independently_interleaved(void) {
  lonejson_config strict_config;
  lonejson_config relaxed_config;
  lonejson *strict_lj;
  lonejson *relaxed_lj;
  lonejson_error error;
  lonejson_status status;
  lonejson_value_visitor visitor;
  lonejson_json_value strict_value;
  lonejson_json_value relaxed_value;
  lonejson_spooled strict_spool;
  lonejson_spooled relaxed_spool;
  test_nested_json_value_doc strict_doc;
  test_nested_json_value_doc relaxed_doc;
  const char *json =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",\"payload\":"
      "{\"value\":\"ab\"}}}";
  lonejson_field runtime_spool_fields[1];
  lonejson_map runtime_spool_map;
  test_spool_limits_doc strict_spool_doc;
  test_spool_limits_doc relaxed_spool_doc;
  char stream_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-runtime-interleaved-XXXXXX";
  int fd;
  lonejson_stream *strict_stream;
  lonejson_stream *relaxed_stream;
  lonejson_stream_result stream_result;
  test_alloc_parse_doc strict_alloc_doc;
  test_alloc_parse_doc relaxed_alloc_doc;
  const char *stream_json =
      "{\"name\":\"alphabet\",\"body\":\"x\",\"tags\":[]}\n";

  strict_config = lonejson_default_config();
  strict_config.clear_destination_by_default = 0;
  strict_config.max_dynamic_string_bytes = 4u;
  strict_config.json_value_max_string_bytes = 1u;
  strict_config.spool_default.max_bytes = 4u;
  strict_lj = lonejson_new(&strict_config, &error);
  EXPECT(strict_lj != NULL);

  relaxed_config = lonejson_default_config();
  relaxed_config.clear_destination_by_default = 0;
  relaxed_config.spool_default.max_bytes = 64u;
  relaxed_lj = lonejson_new(&relaxed_config, &error);
  EXPECT(relaxed_lj != NULL);
  if (strict_lj == NULL || relaxed_lj == NULL) {
    lonejson_free(strict_lj);
    lonejson_free(relaxed_lj);
    return;
  }

  lonejson_json_value_init(strict_lj, &strict_value);
  lonejson_json_value_init(relaxed_lj, &relaxed_value);
  status = lonejson_json_value_set_buffer(&strict_value, "\"ab\"", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_buffer(&relaxed_value, "\"ab\"", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_json_value_cleanup(&strict_value);
  lonejson_json_value_cleanup(&relaxed_value);

  lonejson_spooled_init_class(strict_lj, &strict_spool,
                              LONEJSON_SPOOL_CLASS_DEFAULT);
  lonejson_spooled_init_class(relaxed_lj, &relaxed_spool,
                              LONEJSON_SPOOL_CLASS_DEFAULT);
  status = lonejson_spooled_append(&strict_spool, "abcd", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_spooled_append(&relaxed_spool, "abcd", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_spooled_append(&strict_spool, "e", 1u, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  status = lonejson_spooled_append(&relaxed_spool, "e", 1u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_spooled_cleanup(&strict_spool);
  lonejson_spooled_cleanup(&relaxed_spool);

  visitor = lonejson_default_value_visitor();
  poison_bytes(&strict_doc, sizeof(strict_doc), 0x41u);
  poison_bytes(&relaxed_doc, sizeof(relaxed_doc), 0x42u);
  lonejson_init(strict_lj, &test_nested_json_value_doc_map, &strict_doc);
  lonejson_init(relaxed_lj, &test_nested_json_value_doc_map, &relaxed_doc);
  EXPECT(lonejson_json_value_set_parse_visitor(&strict_doc.response.payload,
                                               &visitor, NULL,
                                               &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_parse_visitor(&relaxed_doc.response.payload,
                                               &visitor, NULL,
                                               &error) == LONEJSON_STATUS_OK);
  status = lonejson_parse_cstr(strict_lj, &test_nested_json_value_doc_map,
                               &strict_doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  status = lonejson_parse_cstr(relaxed_lj, &test_nested_json_value_doc_map,
                               &relaxed_doc, json, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_nested_json_value_doc_map, &strict_doc);
  lonejson_cleanup(&test_nested_json_value_doc_map, &relaxed_doc);

  memset(runtime_spool_fields, 0, sizeof(runtime_spool_fields));
  memset(&runtime_spool_map, 0, sizeof(runtime_spool_map));
  runtime_spool_fields[0] = test_spool_limits_doc_fields[0];
  runtime_spool_fields[0].reserved_policy = NULL;
  runtime_spool_fields[0].spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
  runtime_spool_map.name = "test_runtime_interleaved_spool_doc";
  runtime_spool_map.struct_size = sizeof(test_spool_limits_doc);
  runtime_spool_map.fields = runtime_spool_fields;
  runtime_spool_map.field_count = 1u;

  memset(&strict_spool_doc, 0, sizeof(strict_spool_doc));
  memset(&relaxed_spool_doc, 0, sizeof(relaxed_spool_doc));
  lonejson_init(strict_lj, &runtime_spool_map, &strict_spool_doc);
  lonejson_init(relaxed_lj, &runtime_spool_map, &relaxed_spool_doc);
  status = lonejson_spooled_append(&strict_spool_doc.text, "abcd", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_spooled_append(&relaxed_spool_doc.text, "abcd", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_spooled_append(&strict_spool_doc.text, "e", 1u, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  status = lonejson_spooled_append(&relaxed_spool_doc.text, "e", 1u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_cleanup(&runtime_spool_map, &strict_spool_doc);
  lonejson_cleanup(&runtime_spool_map, &relaxed_spool_doc);

  fd = write_temp_text_file(stream_path, stream_json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    strict_stream = lonejson_stream_open_path(
        strict_lj, &test_alloc_parse_doc_map, stream_path, &error);
    relaxed_stream = lonejson_stream_open_path(
        relaxed_lj, &test_alloc_parse_doc_map, stream_path, &error);
    EXPECT(strict_stream != NULL);
    EXPECT(relaxed_stream != NULL);
    if (strict_stream != NULL && relaxed_stream != NULL) {
      memset(&strict_alloc_doc, 0, sizeof(strict_alloc_doc));
      memset(&relaxed_alloc_doc, 0, sizeof(relaxed_alloc_doc));
      stream_result =
          lonejson_stream_next(strict_stream, &strict_alloc_doc, &error);
      EXPECT(stream_result == LONEJSON_STREAM_ERROR);
      stream_result =
          lonejson_stream_next(relaxed_stream, &relaxed_alloc_doc, &error);
      EXPECT(stream_result == LONEJSON_STREAM_OBJECT);
      if (stream_result == LONEJSON_STREAM_OBJECT) {
        EXPECT(strcmp(relaxed_alloc_doc.name, "alphabet") == 0);
        lonejson_cleanup(&test_alloc_parse_doc_map, &relaxed_alloc_doc);
      }
      lonejson_stream_close(strict_stream);
      lonejson_stream_close(relaxed_stream);
    } else {
      if (strict_stream != NULL) {
        lonejson_stream_close(strict_stream);
      }
      if (relaxed_stream != NULL) {
        lonejson_stream_close(relaxed_stream);
      }
    }
    unlink(stream_path);
  }

  lonejson_free(strict_lj);
  lonejson_free(relaxed_lj);
}

#if LONEJSON_TEST_HAS_PTHREAD
typedef struct test_runtime_thread_ctx {
  lonejson *runtime;
  const char *json;
  lonejson_status expected_parse_status;
  lonejson_status expected_spool_status;
  int failed;
  char message[128];
} test_runtime_thread_ctx;

static void *test_runtime_instance_thread_main(void *arg) {
  test_runtime_thread_ctx *ctx = (test_runtime_thread_ctx *)arg;
  lonejson_error error;
  lonejson_value_visitor visitor;
  lonejson_status status;
  lonejson_field runtime_spool_fields[1];
  lonejson_map runtime_spool_map;
  size_t i;

  memset(runtime_spool_fields, 0, sizeof(runtime_spool_fields));
  memset(&runtime_spool_map, 0, sizeof(runtime_spool_map));
  runtime_spool_fields[0] = test_spool_limits_doc_fields[0];
  runtime_spool_fields[0].reserved_policy = NULL;
  runtime_spool_fields[0].spool_class = LONEJSON_SPOOL_CLASS_DEFAULT;
  runtime_spool_map.name = "test_runtime_thread_spool_doc";
  runtime_spool_map.struct_size = sizeof(test_spool_limits_doc);
  runtime_spool_map.fields = runtime_spool_fields;
  runtime_spool_map.field_count = 1u;
  visitor = lonejson_default_value_visitor();

  for (i = 0u; i < 64u; ++i) {
    test_nested_json_value_doc doc;
    test_spool_limits_doc spool_doc;

    poison_bytes(&doc, sizeof(doc), (unsigned char)(0x40u + (i & 0x3Fu)));
    lonejson_init(ctx->runtime, &test_nested_json_value_doc_map, &doc);
    status = lonejson_json_value_set_parse_visitor(&doc.response.payload,
                                                   &visitor, NULL, &error);
    if (status != LONEJSON_STATUS_OK) {
      ctx->failed = 1;
      snprintf(ctx->message, sizeof(ctx->message),
               "set_parse_visitor failed: %s", error.message);
      lonejson_cleanup(&test_nested_json_value_doc_map, &doc);
      return NULL;
    }
    status = lonejson_parse_cstr(ctx->runtime, &test_nested_json_value_doc_map,
                                 &doc, ctx->json, &error);
    if (status != ctx->expected_parse_status) {
      ctx->failed = 1;
      snprintf(ctx->message, sizeof(ctx->message), "parse status %d != %d",
               (int)status, (int)ctx->expected_parse_status);
      lonejson_cleanup(&test_nested_json_value_doc_map, &doc);
      return NULL;
    }
    lonejson_cleanup(&test_nested_json_value_doc_map, &doc);

    memset(&spool_doc, 0, sizeof(spool_doc));
    lonejson_init(ctx->runtime, &runtime_spool_map, &spool_doc);
    status = lonejson_spooled_append(&spool_doc.text, "abcd", 4u, &error);
    if (status != LONEJSON_STATUS_OK) {
      ctx->failed = 1;
      snprintf(ctx->message, sizeof(ctx->message),
               "first spool append failed: %d", (int)status);
      lonejson_cleanup(&runtime_spool_map, &spool_doc);
      return NULL;
    }
    status = lonejson_spooled_append(&spool_doc.text, "e", 1u, &error);
    if (status != ctx->expected_spool_status) {
      ctx->failed = 1;
      snprintf(ctx->message, sizeof(ctx->message), "spool status %d != %d",
               (int)status, (int)ctx->expected_spool_status);
      lonejson_cleanup(&runtime_spool_map, &spool_doc);
      return NULL;
    }
    lonejson_cleanup(&runtime_spool_map, &spool_doc);
  }
  return NULL;
}
#endif

static void test_runtime_instances_operate_independently_in_threads(void) {
#if LONEJSON_TEST_HAS_PTHREAD
  lonejson_config strict_config;
  lonejson_config relaxed_config;
  lonejson *strict_lj;
  lonejson *relaxed_lj;
  lonejson_error error;
  pthread_t strict_thread;
  pthread_t relaxed_thread;
  test_runtime_thread_ctx strict_ctx;
  test_runtime_thread_ctx relaxed_ctx;
  const char *json =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",\"payload\":"
      "{\"value\":\"ab\"}}}";

  strict_config = lonejson_default_config();
  strict_config.clear_destination_by_default = 0;
  strict_config.json_value_max_string_bytes = 1u;
  strict_config.spool_default.max_bytes = 4u;
  strict_lj = lonejson_new(&strict_config, &error);
  EXPECT(strict_lj != NULL);

  relaxed_config = lonejson_default_config();
  relaxed_config.clear_destination_by_default = 0;
  relaxed_config.spool_default.max_bytes = 64u;
  relaxed_lj = lonejson_new(&relaxed_config, &error);
  EXPECT(relaxed_lj != NULL);
  if (strict_lj == NULL || relaxed_lj == NULL) {
    lonejson_free(strict_lj);
    lonejson_free(relaxed_lj);
    return;
  }

  memset(&strict_ctx, 0, sizeof(strict_ctx));
  strict_ctx.runtime = strict_lj;
  strict_ctx.json = json;
  strict_ctx.expected_parse_status = LONEJSON_STATUS_OVERFLOW;
  strict_ctx.expected_spool_status = LONEJSON_STATUS_OVERFLOW;

  memset(&relaxed_ctx, 0, sizeof(relaxed_ctx));
  relaxed_ctx.runtime = relaxed_lj;
  relaxed_ctx.json = json;
  relaxed_ctx.expected_parse_status = LONEJSON_STATUS_OK;
  relaxed_ctx.expected_spool_status = LONEJSON_STATUS_OK;

  EXPECT(pthread_create(&strict_thread, NULL, test_runtime_instance_thread_main,
                        &strict_ctx) == 0);
  EXPECT(pthread_create(&relaxed_thread, NULL,
                        test_runtime_instance_thread_main, &relaxed_ctx) == 0);
  EXPECT(pthread_join(strict_thread, NULL) == 0);
  EXPECT(pthread_join(relaxed_thread, NULL) == 0);
  EXPECT(!strict_ctx.failed);
  EXPECT(!relaxed_ctx.failed);

  lonejson_free(strict_lj);
  lonejson_free(relaxed_lj);
#endif
}

static void
test_runtime_same_instance_allows_concurrent_independent_calls(void) {
#if LONEJSON_TEST_HAS_PTHREAD
  lonejson_config config;
  lonejson *lj;
  lonejson_error error;
  pthread_t first_thread;
  pthread_t second_thread;
  test_runtime_thread_ctx first_ctx;
  test_runtime_thread_ctx second_ctx;
  const char *json =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",\"payload\":"
      "{\"value\":\"ab\"}}}";

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  config.spool_default.max_bytes = 64u;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  memset(&first_ctx, 0, sizeof(first_ctx));
  first_ctx.runtime = lj;
  first_ctx.json = json;
  first_ctx.expected_parse_status = LONEJSON_STATUS_OK;
  first_ctx.expected_spool_status = LONEJSON_STATUS_OK;

  memset(&second_ctx, 0, sizeof(second_ctx));
  second_ctx.runtime = lj;
  second_ctx.json = json;
  second_ctx.expected_parse_status = LONEJSON_STATUS_OK;
  second_ctx.expected_spool_status = LONEJSON_STATUS_OK;

  EXPECT(pthread_create(&first_thread, NULL, test_runtime_instance_thread_main,
                        &first_ctx) == 0);
  EXPECT(pthread_create(&second_thread, NULL, test_runtime_instance_thread_main,
                        &second_ctx) == 0);
  EXPECT(pthread_join(first_thread, NULL) == 0);
  EXPECT(pthread_join(second_thread, NULL) == 0);
  EXPECT(!first_ctx.failed);
  EXPECT(!second_ctx.failed);

  lonejson_free(lj);
#endif
}

static void
test_runtime_same_instance_allows_concurrent_calls_with_thread_safe_allocator(
    void) {
#if LONEJSON_TEST_HAS_PTHREAD
  lonejson_config config;
  lonejson *lj;
  lonejson_error error;
  pthread_t first_thread;
  pthread_t second_thread;
  test_runtime_thread_ctx first_ctx;
  test_runtime_thread_ctx second_ctx;
  test_serial_allocator_state allocator_state;
  const char *json =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",\"payload\":"
      "{\"value\":\"ab\"}}}";

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  config.spool_default.max_bytes = 64u;
  test_serial_allocator_init(&allocator_state);
  config.allocator = &allocator_state.allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    test_serial_allocator_destroy(&allocator_state);
    return;
  }

  memset(&first_ctx, 0, sizeof(first_ctx));
  first_ctx.runtime = lj;
  first_ctx.json = json;
  first_ctx.expected_parse_status = LONEJSON_STATUS_OK;
  first_ctx.expected_spool_status = LONEJSON_STATUS_OK;

  memset(&second_ctx, 0, sizeof(second_ctx));
  second_ctx.runtime = lj;
  second_ctx.json = json;
  second_ctx.expected_parse_status = LONEJSON_STATUS_OK;
  second_ctx.expected_spool_status = LONEJSON_STATUS_OK;

  EXPECT(pthread_create(&first_thread, NULL, test_runtime_instance_thread_main,
                        &first_ctx) == 0);
  EXPECT(pthread_create(&second_thread, NULL, test_runtime_instance_thread_main,
                        &second_ctx) == 0);
  EXPECT(pthread_join(first_thread, NULL) == 0);
  EXPECT(pthread_join(second_thread, NULL) == 0);
  EXPECT(!first_ctx.failed);
  EXPECT(!second_ctx.failed);
  EXPECT(allocator_state.overlap_detected == 0);
  EXPECT(allocator_state.stats.alloc_calls > 0u);
  EXPECT(allocator_state.stats.free_calls > 0u);

  lonejson_free(lj);
  EXPECT(allocator_state.stats.bytes_live == 0u);
  test_serial_allocator_destroy(&allocator_state);
#endif
}

static void test_runtime_writer_survives_runtime_free(void) {
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *lj;
  lonejson_writer writer;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  unsigned char buffer[128];

  config = lonejson_default_config();
  config.allocator = &allocator;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    return;
  }

  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  sink.length = 0u;
  status = lonejson_writer_init_sink(lj, &writer, test_buffer_sink_write, &sink,
                                     &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  lonejson_free(lj);
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson_writer_json_value_buffer(&writer, "{\"k\":1}", 7u, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_writer_finish(&writer, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp((const char *)buffer, "{\"k\":1}") == 0);
    lonejson_writer_cleanup(&writer);
  }
}

static void
test_runtime_nonparse_snapshots_skip_fixed_string_scratch_copy(void) {
  lonejson_config config;
  test_allocator_state allocator_state;
  lonejson *lj;
  lonejson_generator generator;
  lonejson_error error;
  lonejson_status status;
  unsigned char *scratch;
  test_event event;
  size_t runtime_bytes_live;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;
  scratch = (unsigned char *)malloc(1u << 20);
  EXPECT(scratch != NULL);
  if (scratch == NULL) {
    return;
  }
  memset(scratch, 0xA5, 1u << 20);
  test_allocator_init(&allocator_state);
  config = lonejson_default_config();
  config.allocator = &allocator_state.allocator;
  config.fixed_string_scratch = scratch;
  config.fixed_string_scratch_size = 1u << 20;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    free(scratch);
    return;
  }
  free(scratch);
  runtime_bytes_live = allocator_state.stats.bytes_live;

  allocator_state.stats.peak_bytes_live = allocator_state.stats.bytes_live;
  status = lonejson_generator_init(lj, &generator, &test_event_map, &event);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(allocator_state.stats.peak_bytes_live - runtime_bytes_live < 131072u);
  lonejson_generator_cleanup(&generator);
  EXPECT(allocator_state.stats.bytes_live == runtime_bytes_live);

  lonejson_free(lj);
  EXPECT(allocator_state.stats.bytes_live == 0u);
}

static void test_runtime_writer_snapshot_preserves_fixed_string_scratch(void) {
  lonejson_config config;
  test_allocator_state allocator_state;
  lonejson *lj;
  lonejson_writer writer;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  unsigned char buffer[64];
  unsigned char *scratch;
  size_t runtime_bytes_live;
  lonejson__writer_state *writer_state;

  scratch = (unsigned char *)malloc(8192u);
  EXPECT(scratch != NULL);
  if (scratch == NULL) {
    return;
  }
  memset(scratch, 0x5Au, 8192u);
  test_allocator_init(&allocator_state);
  config = lonejson_default_config();
  config.allocator = &allocator_state.allocator;
  config.fixed_string_scratch = scratch;
  config.fixed_string_scratch_size = 8192u;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    free(scratch);
    return;
  }
  runtime_bytes_live = allocator_state.stats.bytes_live;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  allocator_state.stats.peak_bytes_live = allocator_state.stats.bytes_live;
  status = lonejson_writer_init_sink(lj, &writer, test_buffer_sink_write, &sink,
                                     &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  if (status == LONEJSON_STATUS_OK) {
    writer_state = (lonejson__writer_state *)writer.state;
    EXPECT(writer_state != NULL);
    EXPECT(writer_state->runtime != NULL);
    if (writer_state != NULL && writer_state->runtime != NULL) {
      EXPECT(writer_state->runtime->parse_options.fixed_string_scratch != NULL);
      EXPECT(writer_state->runtime->parse_options.fixed_string_scratch_size ==
             8192u);
      EXPECT(writer_state->runtime->parse_options.fixed_string_scratch !=
             scratch);
      EXPECT(memcmp(writer_state->runtime->parse_options.fixed_string_scratch,
                    scratch, 8192u) == 0);
      EXPECT(allocator_state.stats.peak_bytes_live >=
             runtime_bytes_live + 8192u);
    }
    lonejson_writer_cleanup(&writer);
  }

  lonejson_free(lj);
  free(scratch);
  EXPECT(allocator_state.stats.bytes_live == 0u);
}

static void test_runtime_snapshot_disables_borrowed_fixed_string_scratch(void) {
  test_reader_state state;
  test_large_fixed_string_doc doc;
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *lj;
  lonejson_stream *stream;
  lonejson_stream_result result;
  lonejson_error error;
  char *scratch;
  char *first;
  char *second;
  char *json;
  size_t need;

  scratch = (char *)malloc(8192u);
  first = make_repeating_text(6000u);
  second = make_repeating_text(7000u);
  EXPECT(scratch != NULL);
  EXPECT(first != NULL);
  EXPECT(second != NULL);
  if (scratch == NULL || first == NULL || second == NULL) {
    free(scratch);
    free(first);
    free(second);
    return;
  }

  need = snprintf(NULL, 0, "{\"payload\":\"%s\"}{\"payload\":\"%s\"}", first,
                  second);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(scratch);
    free(first);
    free(second);
    return;
  }
  snprintf(json, need + 1u, "{\"payload\":\"%s\"}{\"payload\":\"%s\"}", first,
           second);

  memset(&state, 0, sizeof(state));
  state.json = json;
  state.chunk_size = 17u;
  memset(&doc, 0, sizeof(doc));

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  config.max_alloc_bytes = 16384u;
  config.allocator = &allocator;
  config.fixed_string_scratch = scratch;
  config.fixed_string_scratch_size = 8192u;
  lj = lonejson_new(&config, &error);
  EXPECT(lj != NULL);
  if (lj == NULL) {
    free(json);
    free(scratch);
    free(first);
    free(second);
    return;
  }

  lonejson_init(lj, &test_large_fixed_string_doc_map, &doc);
  stream = lonejson_stream_open_reader(lj, &test_large_fixed_string_doc_map,
                                       test_framed_reader, &state, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_large_fixed_string_doc_map, &doc);
    lonejson_free(lj);
    free(json);
    free(scratch);
    free(first);
    free(second);
    return;
  }

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.payload, first) == 0);

  lonejson_free(lj);
  free(scratch);

  reset_lonejson_alloc_stats();
  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.payload, second) == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_large_fixed_string_doc_map, &doc);
  free(json);
  free(first);
  free(second);
}

static void test_invalid_json_vectors(void) {
  static const char *invalid_cases[] = {
      "spec/invalid_extra_trailing_garbage.json",
      "spec/invalid_leading_zero.json",
      "spec/invalid_fraction_without_digit.json",
      "spec/invalid_exponent_without_digit.json",
      "spec/invalid_plus_sign.json",
      "spec/invalid_double_comma.json",
      "spec/invalid_array_trailing_comma.json",
      "spec/invalid_object_trailing_comma.json",
      "spec/invalid_missing_colon.json",
      "spec/invalid_missing_value.json",
      "spec/invalid_bad_literal.json",
      "spec/invalid_raw_control_char_in_string.json",
      "spec/invalid_bad_escape.json",
      "spec/invalid_bad_unicode.json",
      "spec/invalid_unterminated_array.json",
      "spec/invalid_unterminated_object.json",
      "spec/invalid_unterminated_string.json"};
  size_t i;

  for (i = 0; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    char *path = fixture_path(invalid_cases[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    lonejson_init(test_default_runtime(), &test_person_map, &person);
    status = test_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status != LONEJSON_STATUS_OK);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void assert_fixture_person(const test_person *person) {
  EXPECT(person->name != NULL);
  if (person->name == NULL) {
    return;
  }
  EXPECT(strncmp(person->name, "Fixture Person Example", 22) == 0);
  EXPECT(strcmp(person->nickname, "fixture") == 0);
  EXPECT(person->age == 57);
  EXPECT(person->score > 4321.0 && person->score < 4321.2);
  EXPECT(person->active == true);
  EXPECT(strcmp(person->address.city, "Stockholm") == 0);
  EXPECT(person->address.zip == 11223344);
  EXPECT(person->lucky_numbers.count == 96);
  if (person->lucky_numbers.count >= 96 &&
      person->lucky_numbers.items != NULL) {
    EXPECT(person->lucky_numbers.items[0] == 1);
    EXPECT(person->lucky_numbers.items[11] == 233);
    EXPECT(person->lucky_numbers.items[95] == 233);
  }
  EXPECT(person->tags.count == 10);
  if (person->tags.count >= 10 && person->tags.items != NULL) {
    EXPECT(strcmp(person->tags.items[0], "alpha") == 0);
    EXPECT(strcmp(person->tags.items[9], "kappa") == 0);
  }
  EXPECT(person->items.count == 5);
  if (person->items.count >= 5 && person->items.items != NULL) {
    EXPECT(((test_item *)person->items.items)[0].id == 101);
    EXPECT(((test_item *)person->items.items)[4].id == 505);
    EXPECT(strcmp(((test_item *)person->items.items)[4].label, "fifth-entry") ==
           0);
  }
}

static void test_fixture_documents(void) {
  static const char *fixture_names[] = {
      "profile_compact.json", "profile_pretty.json", "profile_custom.json"};
  size_t i;

  for (i = 0; i < sizeof(fixture_names) / sizeof(fixture_names[0]); ++i) {
    char *path = generated_fixture_path(fixture_names[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = test_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
    assert_fixture_person(&person);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void test_large_generated_fixture_sizes(void) {
  static const struct {
    const char *name;
    long min_bytes;
  } cases[] = {
      {"large_object_compact.json", 22L * 1024L * 1024L},
      {"large_object_pretty.json", 16L * 1024L * 1024L},
      {"large_array_objects.json", 18L * 1024L * 1024L},
      {"large_base64_payloads.json", 16L * 1024L * 1024L},
      {"large_stream.jsonl", 18L * 1024L * 1024L},
  };
  size_t i;
  long total_bytes;

  total_bytes = 0L;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    char *path = generated_fixture_path(cases[i].name);
    struct stat st;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    EXPECT(stat(path, &st) == 0);
    if (stat(path, &st) == 0) {
      EXPECT((long)st.st_size >= cases[i].min_bytes);
      total_bytes += (long)st.st_size;
    }
    free(path);
  }
  EXPECT(total_bytes >= 95L * 1024L * 1024L);
  EXPECT(total_bytes <= 110L * 1024L * 1024L);
}

static void test_large_generated_fixture_parsing(void) {
  static const char *object_fixture_names[] = {
      "large_object_compact.json", "large_object_pretty.json",
      "large_array_objects.json", "large_base64_payloads.json"};
  size_t i;

  for (i = 0;
       i < sizeof(object_fixture_names) / sizeof(object_fixture_names[0]);
       ++i) {
    char *path = generated_fixture_path(object_fixture_names[i]);
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = test_validate_path(path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    free(path);
  }

  {
    char *path = generated_fixture_path("large_stream.jsonl");
    lonejson_stream *stream;
    lonejson_stream_result result;
    lonejson_error error;
    size_t seen;
    test_large_stream_record record;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    stream = test_stream_open_path(&test_large_stream_record_map, path, NULL,
                                   &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      free(path);
      return;
    }
    seen = 0u;
    for (;;) {
      result = lonejson_stream_next(stream, &record, &error);
      if (result == LONEJSON_STREAM_EOF) {
        break;
      }
      EXPECT(result == LONEJSON_STREAM_OBJECT);
      if (result != LONEJSON_STREAM_OBJECT) {
        break;
      }
      EXPECT(record.id > 0);
      EXPECT(strncmp(record.name, "stream-", 7) == 0 ||
             strcmp(record.name, "tail") == 0);
      EXPECT(strncmp(record.kind, "jsonl", 5) == 0 || record.id == 999999999);
      EXPECT(record.meta.shard >= 0 || record.id == 999999999);
      ++seen;
    }
    lonejson_stream_close(stream);
    free(path);
    EXPECT(seen > 1000u);
  }
}

static void test_small_valid_spec_fixtures(void) {
  static const char *fixture_names[] = {
      "spec/valid_empty_arrays.json",   "spec/valid_escaped_strings.json",
      "spec/valid_nested_ignored.json", "spec/valid_object_basic.json",
      "spec/valid_object_pretty.json",  "spec/valid_unicode_escapes.json"};
  size_t i;

  for (i = 0; i < sizeof(fixture_names) / sizeof(fixture_names[0]); ++i) {
    char *path = fixture_path(fixture_names[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = test_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void test_json_test_suite_parsing(void) {
  const char *dir_path =
      LONEJSON_SOURCE_DIR "/tests/fixtures/vendor/json_test_suite/test_parsing";
  DIR *dir;
  struct dirent *entry;
  size_t y_count = 0u;
  size_t n_count = 0u;
  size_t i_count = 0u;
  size_t i_accept = 0u;
  size_t i_reject = 0u;

  dir = opendir(dir_path);
  EXPECT(dir != NULL);
  if (dir == NULL) {
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    char *path;
    lonejson_error error;
    lonejson_status status;

    if (entry->d_name[0] == '.') {
      continue;
    }
    path = vendor_fixture_path(entry->d_name);
    EXPECT(path != NULL);
    if (path == NULL) {
      closedir(dir);
      return;
    }

    status = test_validate_path(path, &error);
    if (strncmp(entry->d_name, "y_", 2u) == 0) {
      ++y_count;
      EXPECT(status == LONEJSON_STATUS_OK);
    } else if (strncmp(entry->d_name, "n_", 2u) == 0) {
      ++n_count;
      EXPECT(status != LONEJSON_STATUS_OK);
    } else if (strncmp(entry->d_name, "i_", 2u) == 0) {
      ++i_count;
      if (status == LONEJSON_STATUS_OK) {
        ++i_accept;
      } else {
        ++i_reject;
      }
    } else {
      EXPECT(0);
    }
    free(path);
  }

  closedir(dir);
  EXPECT(y_count == 95u);
  EXPECT(n_count == 188u);
  EXPECT(i_count == 35u);
  EXPECT(i_accept + i_reject == i_count);
}
