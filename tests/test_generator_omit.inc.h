static void test_generator_compact_streams_event_map(void) {
  test_event event;
  lonejson_generator generator;
  unsigned char chunk[7];
  char output[128];
  size_t out_len;
  size_t total;
  int eof;

  memset(&event, 0, sizeof(event));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(event.id, "evt-1");
  event.ok = true;

  EXPECT(test_generator_init(&generator, &test_event_map, &event, NULL) ==
         LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strcmp(output, "{\"id\":\"evt-1\",\"ok\":true}") == 0);
  lonejson_generator_cleanup(&generator);
}

static void test_generator_pretty_streams_source_field(void) {
  test_source_doc doc;
  lonejson_generator generator;
  lonejson__write_options options = lonejson__default_write_options();
  char path[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-generator-XXXXXX";
  unsigned char chunk[64];
  char output[512];
  size_t out_len;
  size_t total;
  int eof;
  int fd;
  FILE *fp;

  memset(&doc, 0, sizeof(doc));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(doc.id, "evt-1");
  lonejson_source_init(&doc.text);
  lonejson_source_init(&doc.bytes);
  options.pretty = 1;

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
  fputs("hello", fp);
  fclose(fp);

  EXPECT(lonejson_source_set_path(&doc.text, path, NULL) == LONEJSON_STATUS_OK);
  EXPECT(test_generator_init(&generator, &test_source_doc_map, &doc,
                             &options) == LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strstr(output, "\n") != NULL);
  EXPECT(strstr(output, "\"text\": \"hello\"") != NULL);
  lonejson_generator_cleanup(&generator);
  lonejson_cleanup(&test_source_doc_map, &doc, NULL);
  unlink(path);
}

static void test_generator_streams_json_value_fields(void) {
  test_json_value_doc doc;
  lonejson_generator generator;
  unsigned char chunk[11];
  char output[512];
  size_t out_len;
  size_t total;
  int eof;

  memset(&doc, 0, sizeof(doc));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(doc.id, "evt-1");
  lonejson_json_value_init(test_default_runtime(), &doc.selector);
  lonejson_json_value_init(test_default_runtime(), &doc.fields);
  lonejson_json_value_init(test_default_runtime(), &doc.last_error);
  EXPECT(lonejson_json_value_set_buffer(
             &doc.selector, "{\"op\":\"and\",\"clauses\":[1,true]}",
             strlen("{\"op\":\"and\",\"clauses\":[1,true]}"),
             NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_buffer(
             &doc.fields, "[\"id\",{\"metrics\":[null,3.14]}]",
             strlen("[\"id\",{\"metrics\":[null,3.14]}]"),
             NULL) == LONEJSON_STATUS_OK);

  EXPECT(test_generator_init(&generator, &test_json_value_doc_map, &doc,
                             NULL) == LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strcmp(output,
                "{\"id\":\"evt-1\",\"selector\":{\"op\":\"and\",\"clauses\":[1,"
                "true]},\"fields\":[\"id\",{\"metrics\":[null,3.14]}],"
                "\"last_error\":null}") == 0);
  lonejson_generator_cleanup(&generator);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_generator_pretty_streams_json_value_reader(void) {
  test_json_value_doc doc;
  lonejson_generator generator;
  lonejson__write_options options = lonejson__default_write_options();
  test_reader_state selector_reader = {
      "{\"filters\":[{\"op\":\"eq\",\"field\":\"kind\",\"value\":\"event\"},"
      "{\"op\":\"gt\",\"field\":\"score\",\"value\":3.5}],\"ok\":true}",
      0u, 5u};
  unsigned char chunk[17];
  char output[1024];
  size_t out_len;
  size_t total;
  int eof;

  memset(&doc, 0, sizeof(doc));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(doc.id, "evt-1");
  lonejson_json_value_init(test_default_runtime(), &doc.selector);
  lonejson_json_value_init(test_default_runtime(), &doc.fields);
  lonejson_json_value_init(test_default_runtime(), &doc.last_error);
  options.pretty = 1;

  EXPECT(lonejson_json_value_set_reader(&doc.selector, test_state_reader,
                                        &selector_reader,
                                        NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_buffer(
             &doc.fields, "[\"id\",\"kind\",{\"nested\":[1,null,false]}]",
             strlen("[\"id\",\"kind\",{\"nested\":[1,null,false]}]"),
             NULL) == LONEJSON_STATUS_OK);

  EXPECT(test_generator_init(&generator, &test_json_value_doc_map, &doc,
                             &options) == LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strstr(output, "\n") != NULL);
  EXPECT(strstr(output, "\"selector\": {\n") != NULL);
  EXPECT(strstr(output, "\"filters\": [\n") != NULL);
  EXPECT(strstr(output, "\"fields\": [\n") != NULL);
  EXPECT(strstr(output, "\"nested\": [\n") != NULL);
  lonejson_generator_cleanup(&generator);
  lonejson_cleanup(&test_json_value_doc_map, &doc, NULL);
}

static void test_serialize_omits_empty_optional_fields(void) {
  static char *tags[] = {"request"};
  test_omit_doc doc;
  test_omit_doc parsed;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  lonejson_generator generator;
  char buffer[1024];
  char sink_output[1024];
  char generated[1024];
  char *allocated;
  unsigned char chunk[3];
  size_t needed;
  size_t measured;
  size_t out_len;
  size_t total;
  int eof;
  test_reader_state metadata_reader = {"{\"one_shot\":true}", 0u, 4u};

  memset(&doc, 0, sizeof(doc));
  memset(&parsed, 0, sizeof(parsed));
  memset(&generator, 0, sizeof(generator));
  lonejson_error_init(&error);
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(test_default_runtime(), &doc.metadata);
  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  EXPECT(doc.model != NULL);
  if (doc.model == NULL) {
    return;
  }
  strcpy(doc.model, "gpt-5-nano");
  doc.max_output_tokens = 128;
  doc.seed = 42u;
  doc.temperature = 0.25;
  doc.stream = true;

  allocated =
      test_serialize_alloc(&test_omit_doc_map, &doc, NULL, NULL, &error);
  EXPECT(allocated != NULL);
  if (allocated != NULL) {
    EXPECT(strcmp(allocated, "{\"model\":\"gpt-5-nano\"}") == 0);
    free(allocated);
  }

  status = test_parse_cstr(
      &test_omit_doc_map, &parsed,
      "{\"model\":\"parsed\",\"max_output_tokens\":5,\"seed\":6,"
      "\"temperature\":1.5,\"stream\":false}",
      NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parsed.max_output_tokens == 5);
  EXPECT(parsed.has_max_output_tokens != 0);
  EXPECT(parsed.seed == 6u);
  EXPECT(parsed.has_seed != 0);
  EXPECT(parsed.temperature == 1.5);
  EXPECT(parsed.has_temperature != 0);
  EXPECT(parsed.stream == false);
  EXPECT(parsed.has_stream != 0);
  allocated =
      test_serialize_alloc(&test_omit_doc_map, &parsed, NULL, NULL, &error);
  EXPECT(allocated != NULL);
  if (allocated != NULL) {
    EXPECT(strcmp(allocated,
                  "{\"model\":\"parsed\",\"max_output_tokens\":5,\"seed\":6,"
                  "\"temperature\":1.5,\"stream\":false}") == 0);
    free(allocated);
  }
  lonejson_cleanup(&test_omit_doc_map, &parsed, NULL);

  doc.instructions = (char *)malloc(strlen("be terse") + 1u);
  doc.reasoning.effort = (char *)malloc(strlen("medium") + 1u);
  EXPECT(doc.instructions != NULL);
  EXPECT(doc.reasoning.effort != NULL);
  if (doc.instructions == NULL || doc.reasoning.effort == NULL) {
    free(doc.model);
    free(doc.instructions);
    free(doc.reasoning.effort);
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.instructions, "be terse");
  strcpy(doc.reasoning.effort, "medium");
  doc.max_output_tokens = 128;
  doc.has_max_output_tokens = 1;
  doc.seed = 42u;
  doc.has_seed = 1;
  doc.temperature = 0.25;
  doc.has_temperature = 1;
  doc.stream = true;
  doc.has_stream = 1;
  doc.tags.items = tags;
  doc.tags.count = 1u;
  doc.tags.capacity = 1u;
  doc.tags.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  doc.tools.items = doc.tool_storage;
  doc.tools.count = 1u;
  doc.tools.capacity = 1u;
  doc.tools.elem_size = sizeof(doc.tool_storage[0]);
  doc.tools.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  doc.tool_storage[0].id = 7;
  strcpy(doc.tool_storage[0].label, "tool");
  EXPECT(lonejson_json_value_set_buffer(&doc.metadata, "{\"ok\":true}",
                                        strlen("{\"ok\":true}"),
                                        &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_is_rewindable(&doc.payload) != 0);
  EXPECT(lonejson_json_value_is_rewindable(&doc.metadata) != 0);

  status = test_serialize_buffer(&test_omit_doc_map, &doc, buffer,
                                 sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer,
                "{\"model\":\"gpt-5-nano\",\"instructions\":\"be terse\","
                "\"max_output_tokens\":128,\"seed\":42,\"temperature\":0.25,"
                "\"stream\":true,\"reasoning\":{\"effort\":\"medium\"},"
                "\"tags\":[\"request\"],\"tools\":[{\"id\":7,"
                "\"label\":\"tool\"}],\"metadata\":{\"ok\":true}}") == 0);
  measured = 0u;
  status =
      test_generator_measure(&test_omit_doc_map, &doc, &measured, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(measured == strlen(buffer));

  memset(sink_output, 0, sizeof(sink_output));
  sink.buffer = (unsigned char *)sink_output;
  sink.capacity = sizeof(sink_output);
  sink.length = 0u;
  status = test_serialize_sink(&test_omit_doc_map, &doc, test_buffer_sink_write,
                               &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(sink_output, buffer) == 0);

  EXPECT(test_generator_init(&generator, &test_omit_doc_map, &doc, NULL) ==
         LONEJSON_STATUS_OK);
  memset(generated, 0, sizeof(generated));
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(generated));
    memcpy(generated + total, chunk, out_len);
    total += out_len;
  }
  generated[total] = '\0';
  EXPECT(strcmp(generated, buffer) == 0);
  lonejson_generator_cleanup(&generator);

  lonejson_json_value_cleanup(&doc.metadata);
  lonejson_json_value_init(test_default_runtime(), &doc.metadata);
  EXPECT(lonejson_json_value_set_reader(&doc.metadata, test_state_reader,
                                        &metadata_reader,
                                        &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_is_rewindable(&doc.metadata) == 0);
  status =
      test_generator_measure(&test_omit_doc_map, &doc, &measured, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "non-rewindable") != NULL);

  lonejson_json_value_cleanup(&doc.metadata);
  lonejson_source_cleanup(&doc.payload);
  free(doc.model);
  free(doc.instructions);
  free(doc.reasoning.effort);
}

static void test_nullable_presence_primitives(void) {
  test_nullable_primitives_doc doc;
  lonejson_error error;
  lonejson_status status;
  char buffer[256];
  size_t needed;

  memset(&doc, 0x7f, sizeof(doc));
  doc.has_count = 0;
  doc.has_seed = 0;
  doc.has_ratio = 0;
  doc.has_enabled = 0;
  doc.child.has_priority = 0;
  status = test_parse_cstr(&test_nullable_primitives_map, &doc,
                           "{\"required_count\":7}", NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.required_count == 7);
  EXPECT(doc.has_count == 0);
  EXPECT(doc.has_seed == 0);
  EXPECT(doc.has_ratio == 0);
  EXPECT(doc.has_enabled == 0);
  EXPECT(doc.child.has_priority == 0);

  status = test_parse_cstr(
      &test_nullable_primitives_map, &doc,
      "{\"count\":null,\"seed\":null,\"ratio\":null,\"enabled\":null,"
      "\"required_count\":8,\"child\":{\"priority\":null}}",
      NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.required_count == 8);
  EXPECT(doc.count == 0);
  EXPECT(doc.has_count == 0);
  EXPECT(doc.seed == 0u);
  EXPECT(doc.has_seed == 0);
  EXPECT(doc.ratio == 0.0);
  EXPECT(doc.has_ratio == 0);
  EXPECT(doc.enabled == false);
  EXPECT(doc.has_enabled == 0);
  EXPECT(doc.child.priority == 0);
  EXPECT(doc.child.has_priority == 0);

  status = test_parse_cstr(
      &test_nullable_primitives_map, &doc,
      "{\"count\":-12,\"seed\":42,\"ratio\":1.25,\"enabled\":false,"
      "\"required_count\":9,\"child\":{\"priority\":3}}",
      NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.count == -12);
  EXPECT(doc.has_count == 1);
  EXPECT(doc.seed == 42u);
  EXPECT(doc.has_seed == 1);
  EXPECT(doc.ratio == 1.25);
  EXPECT(doc.has_ratio == 1);
  EXPECT(doc.enabled == false);
  EXPECT(doc.has_enabled == 1);
  EXPECT(doc.child.priority == 3);
  EXPECT(doc.child.has_priority == 1);

  status = test_serialize_buffer(&test_nullable_primitives_map, &doc, buffer,
                                 sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"count\":-12,\"seed\":42,\"ratio\":1.25,"
                        "\"enabled\":false,\"required_count\":9,"
                        "\"child\":{\"priority\":3}}") == 0);

  doc.has_count = 0;
  doc.has_seed = 0;
  doc.has_ratio = 0;
  doc.has_enabled = 0;
  doc.child.has_priority = 0;
  status = test_serialize_buffer(&test_nullable_primitives_map, &doc, buffer,
                                 sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"required_count\":9,\"child\":{}}") == 0);
}

static void test_nullable_presence_primitive_failures(void) {
  static const char *wrong_type_json[] = {
      "{\"count\":\"1\",\"required_count\":1}",
      "{\"seed\":-1,\"required_count\":1}",
      "{\"ratio\":false,\"required_count\":1}",
      "{\"enabled\":0,\"required_count\":1}",
      "{\"required_count\":1,\"child\":{\"priority\":{}}}"};
  test_nullable_primitives_doc doc;
  test_omit_doc omit_doc;
  lonejson_error error;
  lonejson_status status;
  size_t i;
  char buffer[128];

  for (i = 0u; i < sizeof(wrong_type_json) / sizeof(wrong_type_json[0]); ++i) {
    memset(&doc, 0, sizeof(doc));
    status = test_parse_cstr(&test_nullable_primitives_map, &doc,
                             wrong_type_json[i], NULL, &error);
    EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  }

  memset(&doc, 0, sizeof(doc));
  status = test_parse_cstr(&test_nullable_primitives_map, &doc,
                           "{\"required_count\":null}", NULL, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(strstr(error.message, "null is not allowed") != NULL);

  memset(&omit_doc, 0, sizeof(omit_doc));
  status = test_parse_cstr(&test_omit_doc_map, &omit_doc,
                           "{\"model\":\"x\",\"max_output_tokens\":null}", NULL,
                           &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(strstr(error.message, "null is not allowed") != NULL);
  lonejson_cleanup(&test_omit_doc_map, &omit_doc, NULL);

  memset(&doc, 0, sizeof(doc));
  status = test_serialize_buffer(&test_bad_nullable_without_presence_map, &doc,
                                 buffer, sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "invalid map layout") != NULL);

  status = test_serialize_buffer(&test_bad_nullable_kind_map, &doc, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "invalid map layout") != NULL);
}

static void test_serialize_omit_layout_failures(void) {
  test_omit_doc doc;
  lonejson_error error;
  const lonejson_map *maps[4];
  char buffer[128];
  lonejson_status status;
  size_t i;

  memset(&doc, 0, sizeof(doc));
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(test_default_runtime(), &doc.metadata);
  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  EXPECT(doc.model != NULL);
  if (doc.model == NULL) {
    return;
  }
  strcpy(doc.model, "gpt-5-nano");

  maps[0] = &test_bad_required_omit_map;
  maps[1] = &test_bad_required_presence_map;
  maps[2] = &test_bad_presence_kind_map;
  maps[3] = &test_bad_presence_offset_map;
  for (i = 0u; i < sizeof(maps) / sizeof(maps[0]); ++i) {
    status = test_serialize_buffer(maps[i], &doc, buffer, sizeof(buffer), NULL,
                                   NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(strstr(error.message, "invalid map layout") != NULL);
  }

  lonejson_source_cleanup(&doc.payload);
  lonejson_json_value_cleanup(&doc.metadata);
  free(doc.model);
}

static void test_serialize_omit_pretty_and_recursive_invariants(void) {
  test_omit_doc doc;
  test_omit_deep_doc deep;
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  char buffer[1024];
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  memset(&deep, 0, sizeof(deep));
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(test_default_runtime(), &doc.metadata);
  options.pretty = 1;

  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  deep.id = (char *)malloc(strlen("deep-1") + 1u);
  EXPECT(doc.model != NULL);
  EXPECT(deep.id != NULL);
  if (doc.model == NULL || deep.id == NULL) {
    free(doc.model);
    free(deep.id);
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.model, "gpt-5-nano");
  strcpy(deep.id, "deep-1");

  status = test_serialize_buffer(&test_pretty_omit_map, &doc, buffer,
                                 sizeof(buffer), NULL, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\n  \"model\": \"gpt-5-nano\"\n}") == 0);

  doc.instructions = (char *)malloc(strlen("be terse") + 1u);
  EXPECT(doc.instructions != NULL);
  if (doc.instructions == NULL) {
    free(doc.model);
    free(deep.id);
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.instructions, "be terse");
  doc.max_output_tokens = 4096;
  doc.has_max_output_tokens = 0;
  EXPECT(lonejson_json_value_set_buffer(&doc.metadata, "{\"ok\":true}",
                                        strlen("{\"ok\":true}"),
                                        &error) == LONEJSON_STATUS_OK);
  status = test_serialize_buffer(&test_pretty_omit_map, &doc, buffer,
                                 sizeof(buffer), NULL, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\n"
                        "  \"instructions\": \"be terse\",\n"
                        "  \"model\": \"gpt-5-nano\",\n"
                        "  \"metadata\": {\n"
                        "      \"ok\": true\n"
                        "    }\n"
                        "}") == 0);

  status = test_serialize_buffer(&test_omit_deep_doc_map, &deep, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"deep-1\"}") == 0);

  deep.middle.leaf.value = (char *)malloc(strlen("set") + 1u);
  EXPECT(deep.middle.leaf.value != NULL);
  if (deep.middle.leaf.value != NULL) {
    strcpy(deep.middle.leaf.value, "set");
    status = test_serialize_buffer(&test_omit_deep_doc_map, &deep, buffer,
                                   sizeof(buffer), NULL, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(buffer, "{\"id\":\"deep-1\",\"middle\":{\"leaf\":{\"value\":"
                          "\"set\"}}}") == 0);
  }

  lonejson_source_cleanup(&doc.payload);
  lonejson_json_value_cleanup(&doc.metadata);
  free(doc.model);
  free(doc.instructions);
  free(deep.id);
  free(deep.middle.leaf.value);
}

static void test_serialize_omit_empty_implies_null(void) {
  test_omit_doc doc;
  lonejson_error error;
  char buffer[256];
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(test_default_runtime(), &doc.metadata);
  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  EXPECT(doc.model != NULL);
  if (doc.model == NULL) {
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.model, "gpt-5-nano");

  status = test_serialize_buffer(&test_omit_empty_null_map, &doc, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"model\":\"gpt-5-nano\"}") == 0);

  lonejson_source_cleanup(&doc.payload);
  lonejson_json_value_cleanup(&doc.metadata);
  free(doc.model);
}

static void test_serialize_declarative_conditional_helpers(void) {
  test_conditional_doc doc;
  lonejson_error error;
  char buffer[512];
  lonejson_int64 ints[] = {-1, 2};
  lonejson_uint64 uints[] = {3u};
  double nums[] = {1.5};
  bool bools[] = {true, false};
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  lonejson_spooled_init_with_allocator(&doc.text, NULL, NULL);
  lonejson_spooled_init_with_allocator(&doc.bytes, NULL, NULL);

  status = test_serialize_buffer(&test_conditional_doc_map, &doc, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{}") == 0);

  doc.dyn = (char *)malloc(1u);
  EXPECT(doc.dyn != NULL);
  if (doc.dyn == NULL) {
    lonejson_spooled_cleanup(&doc.text);
    lonejson_spooled_cleanup(&doc.bytes);
    return;
  }
  doc.dyn[0] = '\0';
  status = test_serialize_buffer(&test_conditional_doc_map, &doc, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{}") == 0);

  strcpy(doc.fixed, "fx");
  EXPECT(lonejson_spooled_append(&doc.text, "hi", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&doc.bytes, "A", 1u, &error) ==
         LONEJSON_STATUS_OK);
  doc.ints.items = ints;
  doc.ints.count = 2u;
  doc.uints.items = uints;
  doc.uints.count = 1u;
  doc.nums.items = nums;
  doc.nums.count = 1u;
  doc.bools.items = bools;
  doc.bools.count = 2u;

  status = test_serialize_buffer(&test_conditional_doc_map, &doc, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"fixed\":\"fx\",\"text\":\"hi\",\"bytes\":\"QQ==\","
                        "\"ints\":[-1,2],\"uints\":[3],\"nums\":[1.5],"
                        "\"bools\":[true,false]}") == 0);

  free(doc.dyn);
  lonejson_spooled_cleanup(&doc.text);
  lonejson_spooled_cleanup(&doc.bytes);
}

static void test_rewindability_edges_and_measure_preserves_sources(void) {
  test_source_doc source_doc;
  test_json_value_doc json_doc;
  lonejson_error error;
  lonejson_status status;
  char text_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-rewind-source-text-XXXXXX";
  char bytes_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-rewind-source-bytes-XXXXXX";
  char json_path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-rewind-json-XXXXXX";
  int text_fd = -1;
  int bytes_fd = -1;
  int json_fd = -1;
  int pipe_fds[2];
  FILE *text_fp = NULL;
  FILE *json_fp = NULL;
  size_t measured;
  char buffer[512];

  text_fd = write_temp_text_file(text_path, "seekable text");
  bytes_fd = write_temp_text_file(bytes_path, "abc");
  json_fd = write_temp_text_file(json_path, "{\"path\":true}");
  EXPECT(text_fd >= 0);
  EXPECT(bytes_fd >= 0);
  EXPECT(json_fd >= 0);
  if (text_fd < 0 || bytes_fd < 0 || json_fd < 0) {
    if (text_fd >= 0) {
      close(text_fd);
      unlink(text_path);
    }
    if (bytes_fd >= 0) {
      close(bytes_fd);
      unlink(bytes_path);
    }
    if (json_fd >= 0) {
      close(json_fd);
      unlink(json_path);
    }
    return;
  }
  close(text_fd);
  close(bytes_fd);
  close(json_fd);

  lonejson_init(test_default_runtime(), &test_source_doc_map, &source_doc,
                NULL);
  strcpy(source_doc.id, "rewind-1");
  text_fp = fopen(text_path, "rb");
  bytes_fd = open(bytes_path, O_RDONLY);
  EXPECT(text_fp != NULL);
  EXPECT(bytes_fd >= 0);
  if (text_fp == NULL || bytes_fd < 0) {
    if (text_fp != NULL) {
      fclose(text_fp);
    }
    if (bytes_fd >= 0) {
      close(bytes_fd);
    }
    lonejson_cleanup(&test_source_doc_map, &source_doc, NULL);
    unlink(text_path);
    unlink(bytes_path);
    unlink(json_path);
    return;
  }
  EXPECT(lonejson_source_set_file(&source_doc.text, text_fp, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_set_fd(&source_doc.bytes, bytes_fd, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_is_rewindable(&source_doc.text) != 0);
  EXPECT(lonejson_source_is_rewindable(&source_doc.bytes) != 0);
  status = test_generator_measure(&test_source_doc_map, &source_doc, &measured,
                                  NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_serialize_buffer(&test_source_doc_map, &source_doc, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"rewind-1\",\"text\":\"seekable text\","
                        "\"bytes\":\"YWJj\"}") == 0);
  EXPECT(measured == strlen(buffer));
  EXPECT(fseek(text_fp, 0L, SEEK_SET) == 0);
  EXPECT(lseek(bytes_fd, 0, SEEK_SET) == 0);
  lonejson_cleanup(&test_source_doc_map, &source_doc, NULL);
  fclose(text_fp);
  close(bytes_fd);

  lonejson_source_init(&source_doc.text);
  EXPECT(lonejson_source_set_path(&source_doc.text, text_path, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_is_rewindable(&source_doc.text) != 0);
  lonejson_source_cleanup(&source_doc.text);

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "pipe", 4u) == 4);
    close(pipe_fds[1]);
    lonejson_init(test_default_runtime(), &test_source_doc_map, &source_doc,
                  NULL);
    strcpy(source_doc.id, "pipe-1");
    EXPECT(lonejson_source_set_fd(&source_doc.text, pipe_fds[0], &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_source_is_rewindable(&source_doc.text) == 0);
    status = test_generator_measure(&test_source_doc_map, &source_doc,
                                    &measured, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_cleanup(&test_source_doc_map, &source_doc, NULL);
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }

  poison_bytes(&json_doc, sizeof(json_doc), 0xC3u);
  test_init_map(&test_json_value_doc_map, &json_doc);
  strcpy(json_doc.id, "json-1");
  EXPECT(lonejson_json_value_set_path(&json_doc.selector, json_path, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_is_rewindable(&json_doc.selector) != 0);
  lonejson_cleanup(&test_json_value_doc_map, &json_doc, NULL);

  poison_bytes(&json_doc, sizeof(json_doc), 0xC4u);
  test_init_map(&test_json_value_doc_map, &json_doc);
  strcpy(json_doc.id, "json-2");
  json_fp = fopen(json_path, "rb");
  json_fd = open(json_path, O_RDONLY);
  EXPECT(json_fp != NULL);
  EXPECT(json_fd >= 0);
  if (json_fp != NULL && json_fd >= 0) {
    EXPECT(lonejson_json_value_set_file(&json_doc.selector, json_fp, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_json_value_set_fd(&json_doc.fields, json_fd, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_json_value_is_rewindable(&json_doc.selector) != 0);
    EXPECT(lonejson_json_value_is_rewindable(&json_doc.fields) != 0);
    status = test_generator_measure(&test_json_value_doc_map, &json_doc,
                                    &measured, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  lonejson_cleanup(&test_json_value_doc_map, &json_doc, NULL);
  if (json_fp != NULL) {
    fclose(json_fp);
  }
  if (json_fd >= 0) {
    close(json_fd);
  }

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "{\"pipe\":true}", strlen("{\"pipe\":true}")) ==
           (ssize_t)strlen("{\"pipe\":true}"));
    close(pipe_fds[1]);
    poison_bytes(&json_doc, sizeof(json_doc), 0xC5u);
    test_init_map(&test_json_value_doc_map, &json_doc);
    strcpy(json_doc.id, "json-3");
    EXPECT(lonejson_json_value_set_fd(&json_doc.selector, pipe_fds[0],
                                      &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_json_value_is_rewindable(&json_doc.selector) == 0);
    status = test_generator_measure(&test_json_value_doc_map, &json_doc,
                                    &measured, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_cleanup(&test_json_value_doc_map, &json_doc, NULL);
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }

  unlink(text_path);
  unlink(bytes_path);
  unlink(json_path);
}
