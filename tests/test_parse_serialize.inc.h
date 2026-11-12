typedef struct test_numeric_record {
  lonejson_int64 signed_value;
  lonejson_uint64 unsigned_value;
  double real_value;
  bool enabled;
  unsigned char application_data[13];
} test_numeric_record;
static const lonejson_field test_numeric_fields[] = {
    LONEJSON_FIELD_I64(test_numeric_record, signed_value, "signed"),
    LONEJSON_FIELD_U64(test_numeric_record, unsigned_value, "unsigned"),
    LONEJSON_FIELD_F64(test_numeric_record, real_value, "real"),
    LONEJSON_FIELD_BOOL(test_numeric_record, enabled, "enabled")};
LONEJSON_MAP_DEFINE(test_numeric_map, test_numeric_record, test_numeric_fields);
static const lonejson_field test_numeric_gap_fields[] = {
    LONEJSON_FIELD_I64(test_numeric_record, signed_value, "signed"),
    LONEJSON_FIELD_F64(test_numeric_record, real_value, "real"),
    LONEJSON_FIELD_BOOL(test_numeric_record, enabled, "enabled")};
LONEJSON_MAP_DEFINE(test_numeric_gap_map, test_numeric_record,
                    test_numeric_gap_fields);
typedef struct test_numeric_parent {
  test_numeric_record child;
  lonejson_int64 other;
} test_numeric_parent;
static const lonejson_field test_numeric_parent_fields[] = {
    LONEJSON_FIELD_OBJECT(test_numeric_parent, child, "child",
                          &test_numeric_map),
    LONEJSON_FIELD_I64(test_numeric_parent, other, "other")};
LONEJSON_MAP_DEFINE(test_numeric_parent_map, test_numeric_parent,
                    test_numeric_parent_fields);

static void test_mapped_integer_range_boundaries(void) {
  static const char *prefixes[] = {"922337203685477580", "-922337203685477580",
                                   "1844674407370955161"};
  static const unsigned int final_digits[] = {7u, 8u, 5u};
  const lonejson_uint64 maximum = ~(lonejson_uint64)0;
  size_t kind;
  unsigned int digit;
  for (kind = 0u; kind < 3u; ++kind) {
    for (digit = 0u; digit <= 9u; ++digit) {
      char json[96];
      test_numeric_record value;
      lonejson_error error;
      lonejson_status expected = digit <= final_digits[kind]
                                     ? LONEJSON_STATUS_OK
                                     : LONEJSON_STATUS_TYPE_MISMATCH;
      sprintf(json, "{\"%s\":%s%u}", kind == 2u ? "unsigned" : "signed",
              prefixes[kind], digit);
      EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                                 &value, json, &error) == expected);
      if (expected == LONEJSON_STATUS_OK) {
        if (kind == 2u)
          EXPECT(value.unsigned_value == maximum - 5u + digit);
        else {
          lonejson_uint64 magnitude = (maximum >> 1u) - 7u + digit;
          lonejson_int64 wanted = kind == 0u
                                      ? (lonejson_int64)magnitude
                                      : -((lonejson_int64)(magnitude - 1u)) - 1;
          EXPECT(value.signed_value == wanted);
        }
      }
      /* Another digit must overflow, including after the largest valid value.
       */
      sprintf(json, "{\"%s\":%s%u0}", kind == 2u ? "unsigned" : "signed",
              prefixes[kind], digit);
      EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                                 &value, json,
                                 &error) == LONEJSON_STATUS_TYPE_MISMATCH);
    }
  }
  for (digit = 0u; digit <= 9u; ++digit) {
    char json[80];
    test_numeric_record value;
    lonejson_error error;
    sprintf(json, "{\"signed\":%u,\"unsigned\":%u}", digit, digit);
    EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                               &value, json, &error) == LONEJSON_STATUS_OK);
    EXPECT(value.signed_value == (lonejson_int64)digit);
    EXPECT(value.unsigned_value == (lonejson_uint64)digit);
    sprintf(json, "{\"signed\":-%u}", digit);
    EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                               &value, json, &error) == LONEJSON_STATUS_OK);
    EXPECT(value.signed_value == -(lonejson_int64)digit);
    sprintf(json, "{\"unsigned\":-%u}", digit);
    EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                               &value, json,
                               &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  }
  {
    static const char *invalid[] = {"00", "01",  "-00", "-01",
                                    "+1", "1.0", "1e0"};
    size_t i;
    for (i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
      char json[80];
      test_numeric_record value;
      lonejson_error error;
      sprintf(json, "{\"signed\":%s}", invalid[i]);
      EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                                 &value, json, &error) != LONEJSON_STATUS_OK);
      sprintf(json, "{\"unsigned\":%s}", invalid[i]);
      EXPECT(lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                                 &value, json, &error) != LONEJSON_STATUS_OK);
    }
  }
}

static void test_ignored_number_feed_boundaries(void) {
  static const struct {
    const char *text;
    int valid;
  } numbers[] = {{"0", 1},        {"-0", 1},    {"17", 1},
                 {"-1234567", 1}, {"1.25", 1},  {"-0.125", 1},
                 {"1e30", 1},     {"1E-30", 1}, {"18446744073709551616", 1},
                 {"1e4000", 1},   {"01", 0},    {"-01", 0},
                 {"1.", 0},       {"1e", 0},    {"1e+", 0},
                 {"-.1", 0},      {"1-2", 0},   {"1+2", 0},
                 {"--1", 0},      {"1e+2.3", 0}};
  static const char *formats[] = {"{\"ignored\":%s,\"signed\":17}",
                                  "{\"ignored\":{\"n\":%s},\"signed\":17}",
                                  "{\"ignored\":[%s],\"signed\":17}"};
  size_t example, shape, chunk;
  for (example = 0u; example < sizeof(numbers) / sizeof(numbers[0]);
       ++example) {
    for (shape = 0u; shape < sizeof(formats) / sizeof(formats[0]); ++shape) {
      char json[160];
      size_t length;
      sprintf(json, formats[shape], numbers[example].text);
      length = strlen(json);
      for (chunk = 1u; chunk <= length; ++chunk) {
        test_numeric_record value;
        lonejson_parser parser;
        unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                LONEJSON__PARSER_WORKSPACE_SLACK];
        lonejson_status status = LONEJSON_STATUS_OK;
        size_t offset = 0u;
        memset(&value, 0, sizeof(value));
        lonejson__parser_init_state(&parser, &test_numeric_map, &value, NULL,
                                    NULL, 0, 0, 0, 0u, workspace,
                                    sizeof(workspace));
        while (offset < length && status == LONEJSON_STATUS_OK) {
          size_t count = length - offset;
          if (count > chunk)
            count = chunk;
          status = lonejson_parser_feed(&parser, json + offset, count);
          offset += count;
        }
        if (status == LONEJSON_STATUS_OK)
          status = lonejson_parser_finish(&parser);
        EXPECT(status == (numbers[example].valid
                              ? LONEJSON_STATUS_OK
                              : LONEJSON_STATUS_INVALID_JSON));
        if (numbers[example].valid) {
          EXPECT(value.signed_value == 17);
          EXPECT(parser.error.offset == length);
        }
        lonejson_parser_destroy(&parser);
      }
    }
  }
  {
    size_t digits = 2u * (size_t)LONEJSON_PARSER_BUFFER_SIZE;
    char *json = (char *)malloc(digits + 64u);
    test_numeric_record value;
    lonejson_error error;
    lonejson_status status;
    EXPECT(json != NULL);
    if (json == NULL)
      return;
    strcpy(json, "{\"ignored\":");
    memset(json + strlen(json), '7', digits);
    strcpy(json + strlen("{\"ignored\":") + digits, ",\"signed\":17}");
    status = lonejson_parse_cstr(test_default_runtime(), &test_numeric_map,
                                 &value, json, &error);
    EXPECT(status == LONEJSON_STATUS_ALLOCATION_FAILED ||
           status == LONEJSON_STATUS_OVERFLOW);
    free(json);
  }
}

static void test_schema_key_comparison_boundaries(void) {
  size_t length, padding, position;
  for (length = 1u; length <= 24u; ++length) {
    for (padding = 0u; padding < 8u; ++padding) {
      char *storage = (char *)malloc(padding + length + 1u);
      char *key;
      char json[80];
      lonejson_field field = test_numeric_fields[0];
      lonejson_map map = test_numeric_map;
      test_numeric_record value;
      lonejson_error error;
      EXPECT(storage != NULL);
      if (storage == NULL)
        return;
      key = storage + padding;
      for (position = 0u; position < length; ++position)
        key[position] = (char)('a' + position % 26u);
      key[length] = '\0';
      field.json_key = key;
      field.json_key_len = length;
      field.json_key_first = (unsigned char)key[0];
      field.json_key_last = (unsigned char)key[length - 1u];
      map.fields = &field;
      map.field_count = 1u;
      map._map_identity = NULL;
      map._map_cookie = 0u;
      sprintf(json, "{\"%s\":17}", key);
      EXPECT(lonejson_parse_cstr(test_default_runtime(), &map, &value, json,
                                 &error) == LONEJSON_STATUS_OK);
      EXPECT(value.signed_value == 17);
      /* Reject every single-byte mismatch, including interior bytes with
       * matching first and last bytes. The schema allocation ends at NUL. */
      for (position = 0u; position < length; ++position) {
        json[2u + position] = 'z';
        EXPECT(lonejson_parse_cstr(test_default_runtime(), &map, &value, json,
                                   &error) == LONEJSON_STATUS_OK);
        EXPECT(value.signed_value == 0);
        json[2u + position] = key[position];
      }
      free(storage);
    }
  }
}

static void test_numeric_lifecycle_preserves_application_data(void) {
  const lonejson_map *maps[] = {&test_numeric_map, &test_numeric_gap_map,
                                &test_numeric_parent_map};
  size_t i;
  int reset;
  for (i = 0u; i < sizeof(maps) / sizeof(maps[0]); ++i) {
    for (reset = 0; reset < 2; ++reset) {
      test_numeric_parent value;
      unsigned char expected[sizeof(value.child.application_data)];
      lonejson_error error;
      memset(&value, 0x5a, sizeof(value));
      memset(expected, 0x5a, sizeof(expected));
      value.child.unsigned_value = 987u;
      EXPECT((reset ? lonejson_reset(test_default_runtime(), maps[i], &value,
                                     &error)
                    : lonejson_init(test_default_runtime(), maps[i], &value,
                                    &error)) == LONEJSON_STATUS_OK);
      EXPECT(value.child.signed_value == 0);
      EXPECT(value.child.unsigned_value == (i == 1u ? 987u : 0u));
      EXPECT(value.child.real_value == 0.0);
      EXPECT(value.child.enabled == 0);
      EXPECT(memcmp(value.child.application_data, expected, sizeof(expected)) ==
             0);
      if (i == 2u)
        EXPECT(value.other == 0);
    }
  }
  {
    test_numeric_record value;
    test_numeric_record before;
    lonejson_error error;
    memset(&value, 0x5a, sizeof(value));
    value.signed_value = -17;
    value.unsigned_value = 987u;
    value.real_value = 1.25;
    value.enabled = 1;
    memcpy(&before, &value, sizeof(value));
    EXPECT(lonejson_cleanup(&test_numeric_map, &value, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(memcmp(&value, &before, sizeof(value)) == 0);
  }
}

typedef struct test_empty_array_doc {
  lonejson_object_array rows;
} test_empty_array_doc;
static const lonejson_field test_empty_array_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(test_empty_array_doc, rows, "rows", test_item,
                                &test_item_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_empty_array_map, test_empty_array_doc,
                    test_empty_array_fields);

static void test_empty_array_replacement_ownership(void) {
  int fixed;
  for (fixed = 0; fixed < 2; ++fixed) {
    test_empty_array_doc value;
    test_item storage[2];
    test_allocator_state alloc;
    lonejson_config config = lonejson_default_config();
    lonejson_error error;
    lonejson *runtime;
    size_t runtime_bytes;
    test_allocator_init(&alloc);
    config.allocator = &alloc.allocator;
    config.clear_destination_by_default = 0;
    runtime = lonejson_new(&config, &error);
    EXPECT(runtime != NULL);
    if (runtime == NULL)
      return;
    runtime_bytes = alloc.stats.bytes_live;
    EXPECT(lonejson_init(runtime, &test_empty_array_map, &value, &error) ==
           LONEJSON_STATUS_OK);
    if (fixed) {
      value.rows.items = storage;
      value.rows.capacity = 2u;
      value.rows.elem_size = sizeof(storage[0]);
      value.rows.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    EXPECT(lonejson_parse_cstr(runtime, &test_empty_array_map, &value,
                               "{\"rows\":[{\"id\":1,\"label\":\"one\"}]}",
                               &error) == LONEJSON_STATUS_OK);
    EXPECT(value.rows.count == 1u);
    /* These items contain no owned fields. Retain empty backing storage to
     * check that replacement frees owned storage and preserves fixed storage.
     */
    value.rows.count = 0u;
    EXPECT(lonejson_parse_cstr(runtime, &test_empty_array_map, &value,
                               "{\"rows\":[]}", &error) == LONEJSON_STATUS_OK);
    EXPECT(value.rows.count == 0u);
    if (fixed) {
      EXPECT(value.rows.items == storage && value.rows.capacity == 2u);
      EXPECT(value.rows.elem_size == sizeof(storage[0]));
      EXPECT(value.rows.flags == LONEJSON_ARRAY_FIXED_CAPACITY);
    } else {
      EXPECT(value.rows.items == NULL && value.rows.capacity == 0u);
      EXPECT(value.rows.flags == 0u);
    }
    EXPECT(alloc.stats.bytes_live == runtime_bytes);
    EXPECT(lonejson_cleanup(&test_empty_array_map, &value, &error) ==
           LONEJSON_STATUS_OK);
    lonejson_free(runtime);
    EXPECT(alloc.stats.bytes_live == 0u);
  }
}

static void test_colliding_schema_cache_entries(void) {
  lonejson_map *maps;
  lonejson__map_flag_cache_entry *slots[LONEJSON__MAP_FLAG_CACHE_SIZE + 1u];
  const size_t count = LONEJSON__MAP_FLAG_CACHE_SIZE + 1u;
  size_t i, j;
  size_t first = 0u, second = 0u;
  if (!LONEJSON__MAP_FLAG_CACHE_ENABLED)
    return;
  maps = (lonejson_map *)calloc(count, sizeof(*maps));
  EXPECT(maps != NULL);
  if (maps == NULL)
    return;
  /* Discover a collision through the resolver, without duplicating its hash.
   * More maps than slots guarantee at least one reused slot. */
  for (i = 0u; i < count && second == 0u; ++i) {
    maps[i] = test_numeric_map;
    maps[i]._map_identity = &maps[i];
    maps[i]._map_cookie = ~(lonejson_uint64)0u - i;
    slots[i] = lonejson__map_flag_cache_slot(&maps[i]);
    for (j = 0u; j < i; ++j) {
      if (slots[j] == slots[i]) {
        first = j;
        second = i;
        break;
      }
    }
  }
  EXPECT(second != 0u);
  if (second != 0u) {
    maps[second].fields = test_person_map.fields;
    maps[second].field_count = test_person_map.field_count;
    maps[second].struct_size = test_person_map.struct_size;
    EXPECT(!lonejson__map_analysis_cached(&maps[first]).may_allocate);
    EXPECT(lonejson__map_analysis_cached(&maps[second]).may_allocate);
    /* Collisions may require reanalysis, but must not reuse another map's
     * ownership or layout results. */
    for (i = 0u; i < 100u; ++i) {
      EXPECT(!lonejson__map_analysis_cached(&maps[first]).may_allocate);
      EXPECT(lonejson__map_analysis_cached(&maps[second]).may_allocate);
    }
    /* A changed cookie must invalidate the previous analysis. */
    maps[second].fields = test_numeric_map.fields;
    maps[second].field_count = test_numeric_map.field_count;
    maps[second].struct_size = test_numeric_map.struct_size;
    --maps[second]._map_cookie;
    EXPECT(!lonejson__map_analysis_cached(&maps[second]).may_allocate);
    EXPECT(!lonejson__map_analysis_cached(&maps[first]).may_allocate);
    maps[second].struct_size = 0u;
    --maps[second]._map_cookie;
    EXPECT(!lonejson__map_analysis_cached(&maps[second]).valid);
    EXPECT(lonejson__map_analysis_cached(&maps[first]).valid);
  }
  free(maps);
}

typedef struct test_string_span_record {
  char text[160];
} test_string_span_record;
static const lonejson_field test_string_span_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_string_span_record, text,
                                "record_text_payload", LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_string_span_map, test_string_span_record,
                    test_string_span_fields);

typedef struct test_string_span_trace {
  char text[160];
  size_t len;
} test_string_span_trace;

static lonejson_status test_string_span_chunk(void *user, const char *data,
                                              size_t len,
                                              lonejson_error *error) {
  test_string_span_trace *trace = (test_string_span_trace *)user;
  (void)error;
  if (len >= sizeof(trace->text) - trace->len)
    return LONEJSON_STATUS_OVERFLOW;
  memcpy(trace->text + trace->len, data, len);
  trace->len += len;
  trace->text[trace->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static void test_json_whitespace_feed_boundaries(void) {
  static const char valid[] =
      " \t\r\n{\r\n\t\"record_text_payload\" \t:\r\n \"x y\" \r\n}\t ";
  static const char invalid[] =
      " \t\r\n{\r\n\t\"record_text_payload\" \t:\r\n \"x y\" \r\n, \n!}";
  static const size_t chunks[] = {1u, 2u, 3u, 5u, 7u, 8u, 16u, sizeof(valid)};
  size_t example, chunk_index;
  for (example = 0u; example < 2u; ++example) {
    const char *json = example == 0u ? valid : invalid;
    size_t length = strlen(json);
    for (chunk_index = 0u; chunk_index < sizeof(chunks) / sizeof(chunks[0]);
         ++chunk_index) {
      test_string_span_record record;
      lonejson_parser parser;
      unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE +
                              LONEJSON__PARSER_WORKSPACE_SLACK];
      lonejson_status status = LONEJSON_STATUS_OK;
      size_t offset = 0u;
      memset(&record, 0, sizeof(record));
      lonejson__parser_init_state(&parser, &test_string_span_map, &record, NULL,
                                  NULL, 0, 0, 0, 0u, workspace,
                                  sizeof(workspace));
      while (offset < length && status == LONEJSON_STATUS_OK) {
        size_t count = length - offset;
        if (count > chunks[chunk_index])
          count = chunks[chunk_index];
        status = lonejson_parser_feed(&parser, json + offset, count);
        offset += count;
      }
      if (status == LONEJSON_STATUS_OK)
        status = lonejson_parser_finish(&parser);
      if (example == 0u) {
        EXPECT(status == LONEJSON_STATUS_OK);
        EXPECT(strcmp(record.text, "x y") == 0);
        EXPECT(parser.error.offset == length);
      } else {
        const char *bad = strchr(json, '!');
        const char *last_line = strrchr(json, '\n');
        const char *p;
        size_t line = 1u;
        for (p = json; p < bad; ++p)
          if (*p == '\n')
            ++line;
        EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
        EXPECT(parser.error.offset == (size_t)(bad - json) + 1u);
        EXPECT(parser.error.line == line);
        EXPECT(parser.error.column == (size_t)(bad - last_line));
      }
      lonejson_parser_destroy(&parser);
    }
  }
  {
    lonejson_error error;
    EXPECT(lonejson_validate_cstr(test_default_runtime(), "[tru e]", &error) ==
           LONEJSON_STATUS_INVALID_JSON);
    EXPECT(lonejson_validate_cstr(test_default_runtime(), "[nul l]", &error) ==
           LONEJSON_STATUS_INVALID_JSON);
    EXPECT(lonejson_validate_cstr(test_default_runtime(), "[1 e2]", &error) ==
           LONEJSON_STATUS_INVALID_JSON);
  }
}

static void test_json_whitespace_runs(void) {
  static const struct {
    const char *json;
    int bad;
  } cases[] = {
      {" \r\n{ \t\"x\" \n: [ 1 , -2.5e+1 , true , false , null , {} , [] ] } ",
       0},
      {"[ \n\t{ \r\"x\" : \"a b\" }, \r\n[], \t{} ]\r\n", 0},
      {"[ \"a\\n b\", \"\", [ \t\r\n ] ]", 0},
      {"{ \t\r\n }", 0},
      {"[ \t\r\n ]", 0},
      {"[ true \t, \r\n ? ]", '?'},
      {"{ \"x\" \r\n ? 1 }", '?'},
      {"[tru e]", ' '},
      {"[fal se]", ' '},
      {"[nul l]", ' '},
      {"[1 e2]", 'e'},
      {"[\v1]", '\v'},
      {"[\f1]", '\f'}};
  size_t example, chunk;
  for (example = 0u; example < sizeof(cases) / sizeof(cases[0]); ++example) {
    const char *json = cases[example].json;
    size_t length = strlen(json);
    size_t expected_offset =
        cases[example].bad != 0
            ? (size_t)(strchr(json, cases[example].bad) - json) + 1u
            : length;
    size_t line = 1u, column = 0u, position;
    for (position = 0u; position < expected_offset; ++position) {
      if (json[position] == '\n') {
        ++line;
        column = 0u;
      } else {
        ++column;
      }
    }
    for (chunk = 1u; chunk <= length; ++chunk) {
      lonejson_parser parser;
      unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE +
                              LONEJSON__PARSER_WORKSPACE_SLACK];
      lonejson_status status = LONEJSON_STATUS_OK;
      size_t offset = 0u;
      lonejson__parser_init_state(&parser, NULL, NULL, NULL, NULL, 1, 0, 0, 0u,
                                  workspace, sizeof(workspace));
      while (offset < length && status == LONEJSON_STATUS_OK) {
        size_t count = length - offset;
        if (count > chunk)
          count = chunk;
        status = lonejson_parser_feed(&parser, json + offset, count);
        offset += count;
      }
      if (status == LONEJSON_STATUS_OK)
        status = lonejson_parser_finish(&parser);
      EXPECT(status == (cases[example].bad != 0 ? LONEJSON_STATUS_INVALID_JSON
                                                : LONEJSON_STATUS_OK));
      EXPECT(parser.error.offset == expected_offset);
      EXPECT(parser.error.line == line);
      EXPECT(parser.error.column == column);
      lonejson_parser_destroy(&parser);
    }
  }
}

static void test_string_scan_boundaries(void) {
  static const char prefix[] = "{\"record_text_payload\":\"";
  static const char suffix[] = "zzzzzzzz\"}";
  static const size_t lengths[] = {
      0u,  1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,   9u,   10u,
      11u, 12u, 13u, 14u, 15u, 16u, 31u, 32u, 33u,  51u,  52u,
      53u, 54u, 55u, 56u, 63u, 64u, 65u, 79u, 127u, 128u, 129u};
  lonejson *runtime = test_default_runtime();
  lonejson_value_visitor visitor;
  size_t padding, length, length_index;
  unsigned int byte;
  memset(&visitor, 0, sizeof(visitor));
  visitor.string_chunk = test_string_span_chunk;
  for (padding = 0u; padding < 8u; ++padding) {
    for (length_index = 0u; length_index < sizeof(lengths) / sizeof(lengths[0]);
         ++length_index) {
      length = lengths[length_index];
      for (byte = 0u; byte <= 255u; ++byte) {
        char encoded[2];
        char decoded[2];
        char expected[160];
        size_t encoded_len = 1u, decoded_len = 1u;
        size_t json_len;
        unsigned char *allocation, *json;
        lonejson_error error;
        test_string_span_record record;
        test_string_span_trace trace;
        lonejson_status wanted =
            byte < 0x20u ? LONEJSON_STATUS_INVALID_JSON : LONEJSON_STATUS_OK;
        lonejson_value_visitor_stream *stream;
        size_t consumed;
        decoded[0] = encoded[0] = (char)byte;
        if (byte == '"' || byte == '\\') {
          encoded[0] = '\\';
          encoded[1] = (char)byte;
          encoded_len = 2u;
        } else if (byte >= 0x80u) {
          decoded[0] = encoded[0] = (char)(0xC0u | (byte >> 6u));
          decoded[1] = encoded[1] = (char)(0x80u | (byte & 0x3Fu));
          encoded_len = decoded_len = 2u;
        }
        json_len =
            sizeof(prefix) - 1u + length + encoded_len + sizeof(suffix) - 1u;
        /* End the allocation exactly at the input boundary for ASan/Memcheck.
         */
        allocation = (unsigned char *)malloc(padding + json_len);
        EXPECT(allocation != NULL);
        if (allocation == NULL)
          return;
        json = allocation + padding;
        memcpy(json, prefix, sizeof(prefix) - 1u);
        memset(json + sizeof(prefix) - 1u, 'a', length);
        memcpy(json + sizeof(prefix) - 1u + length, encoded, encoded_len);
        memcpy(json + sizeof(prefix) - 1u + length + encoded_len, suffix,
               sizeof(suffix) - 1u);
        memset(expected, 'a', length);
        memcpy(expected + length, decoded, decoded_len);
        memcpy(expected + length + decoded_len, "zzzzzzzz", 9u);
        EXPECT(lonejson_validate_buffer(runtime, json, json_len, &error) ==
               wanted);
        EXPECT(lonejson_parse_buffer(runtime, &test_string_span_map, &record,
                                     json, json_len, &error) == wanted);
        if (wanted == LONEJSON_STATUS_OK)
          EXPECT(strcmp(record.text, expected) == 0);
        memset(&trace, 0, sizeof(trace));
        EXPECT(lonejson_visit_value_buffer(runtime, json, json_len, &visitor,
                                           &trace, &error) == wanted);
        if (wanted == LONEJSON_STATUS_OK)
          EXPECT(strcmp(trace.text, expected) == 0);
        memset(&trace, 0, sizeof(trace));
        stream = lonejson_value_visitor_stream_open(runtime, &visitor, &trace,
                                                    &error);
        EXPECT(stream != NULL);
        if (stream != NULL) {
          EXPECT(stream->push(stream, json, json_len, &consumed, &error) ==
                 wanted);
          if (wanted == LONEJSON_STATUS_OK) {
            EXPECT(consumed == json_len);
            EXPECT(stream->finish(stream, &error) == LONEJSON_STATUS_OK);
            EXPECT(strcmp(trace.text, expected) == 0);
          }
          stream->close(stream);
        }
        if (byte != 0u) {
          char serialized[256];
          char sink_json[256];
          test_buffer_sink sink;
          test_string_span_record roundtrip;
          size_t needed;
          memcpy(record.text, expected, strlen(expected) + 1u);
          EXPECT(lonejson_serialize_buffer(runtime, &test_string_span_map,
                                           &record, serialized,
                                           sizeof(serialized), &needed,
                                           &error) == LONEJSON_STATUS_OK);
          memset(&sink, 0, sizeof(sink));
          sink.buffer = (unsigned char *)sink_json;
          sink.capacity = sizeof(sink_json);
          EXPECT(lonejson_serialize_sink(runtime, &test_string_span_map,
                                         &record, test_buffer_sink_write, &sink,
                                         &error) == LONEJSON_STATUS_OK);
          EXPECT(strcmp(sink_json, serialized) == 0);
          EXPECT(lonejson_parse_buffer(
                     runtime, &test_string_span_map, &roundtrip, serialized,
                     strlen(serialized), &error) == LONEJSON_STATUS_OK);
          EXPECT(strcmp(roundtrip.text, expected) == 0);
        }
        free(allocation);
      }
    }
  }
}

typedef struct test_generated_json_reader_state {
  const char *prefix;
  size_t prefix_len;
  size_t payload_len;
  const char *suffix;
  size_t suffix_len;
  size_t offset;
  unsigned char fill;
  size_t chunk_limit;
} test_generated_json_reader_state;

static lonejson_read_result
test_generated_json_reader(void *user, unsigned char *buffer, size_t capacity) {
  test_generated_json_reader_state *st =
      (test_generated_json_reader_state *)user;
  lonejson_read_result rr = lonejson_default_read_result();
  size_t total_len;
  size_t remaining;
  size_t chunk;
  size_t written;

  total_len = st->prefix_len + st->payload_len + st->suffix_len;
  if (st->offset >= total_len || capacity == 0u) {
    rr.eof = (st->offset >= total_len) ? 1 : 0;
    return rr;
  }
  remaining = total_len - st->offset;
  chunk = remaining < capacity ? remaining : capacity;
  if (st->chunk_limit != 0u && chunk > st->chunk_limit) {
    chunk = st->chunk_limit;
  }
  written = 0u;
  while (written < chunk) {
    if (st->offset < st->prefix_len) {
      size_t part = st->prefix_len - st->offset;
      if (part > chunk - written) {
        part = chunk - written;
      }
      memcpy(buffer + written, st->prefix + st->offset, part);
      st->offset += part;
      written += part;
      continue;
    }
    if (st->offset < st->prefix_len + st->payload_len) {
      size_t body_off = st->offset - st->prefix_len;
      size_t part = st->payload_len - body_off;
      if (part > chunk - written) {
        part = chunk - written;
      }
      memset(buffer + written, st->fill, part);
      st->offset += part;
      written += part;
      continue;
    }
    {
      size_t suffix_off = st->offset - st->prefix_len - st->payload_len;
      size_t part = st->suffix_len - suffix_off;
      if (part > chunk - written) {
        part = chunk - written;
      }
      memcpy(buffer + written, st->suffix + suffix_off, part);
      st->offset += part;
      written += part;
    }
  }
  rr.bytes_read = written;
  rr.eof = (st->offset >= total_len) ? 1 : 0;
  return rr;
}

static void test_parse_implicit_destination_reset(void) {
  const char *json =
      "{\"name\":\"Reset\",\"address\":{\"city\":\"Oslo\",\"zip\":42},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&person, sizeof(person), 0xA5u);
  status = test_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.name != NULL && strcmp(person.name, "Reset") == 0);
  EXPECT(person.nickname[0] == '\0');
  EXPECT(person.age == 0);
  EXPECT(person.score == 0.0);
  EXPECT(person.active == false);
  EXPECT(strcmp(person.address.city, "Oslo") == 0);
  EXPECT(person.address.zip == 42);
  EXPECT(person.lucky_numbers.count == 0);
  EXPECT(person.tags.count == 0);
  EXPECT(person.items.count == 0);
  lonejson_cleanup(&test_person_map, &person, NULL);
}

static void test_dynamic_allocation_cleanup_balance(void) {
  const char *json =
      "{\"name\":\"Balance\",\"nickname\":\"bal\",\"age\":9,\"score\":9.5,"
      "\"active\":true,\"address\":{\"city\":\"Rome\",\"zip\":900},"
      "\"lucky_numbers\":[1,2,3,4,5],\"tags\":[\"a\",\"b\",\"c\"],"
      "\"items\":[{\"id\":1,\"label\":\"first\"},{\"id\":2,\"label\":"
      "\"second\"}]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  poison_bytes(&person, sizeof(person), 0x11u);
  status = test_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count > 0u);
  lonejson_cleanup(&test_person_map, &person, NULL);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_zero_alloc_validate_path(void) {
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  status = test_validate_cstr("{\"ok\":true,\"items\":[1,2,3]}", &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_nonresumable_reader_apis_reject_would_block(void) {
  static const char json[] =
      "{\"name\":\"Block\",\"address\":{\"city\":\"Oslo\",\"zip\":7},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person doc;
  test_would_block_tripwire_read_state parse_state;
  test_would_block_tripwire_read_state validate_state;
  lonejson_error error;
  lonejson_status status;
  lonejson *runtime = test_default_runtime();

  poison_bytes(&doc, sizeof(doc), 0xA5u);

  memset(&parse_state, 0, sizeof(parse_state));
  parse_state.json = json;
  parse_state.chunk_size = 32u;
  parse_state.block_after = 32u;
  status = test_parse_reader(&test_person_map, &doc,
                             test_would_block_tripwire_reader, &parse_state,
                             NULL, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(parse_state.blocked);
  EXPECT(!parse_state.reentered_after_block);
  EXPECT(parse_state.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);
  lonejson_cleanup(&test_person_map, &doc, NULL);

  poison_bytes(&doc, sizeof(doc), 0xA5u);
  memset(&parse_state, 0, sizeof(parse_state));
  parse_state.json = json;
  parse_state.chunk_size = 32u;
  parse_state.block_after = 32u;
  status = runtime->parse_reader(runtime, &test_person_map, &doc,
                                 test_would_block_tripwire_reader, &parse_state,
                                 &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(parse_state.blocked);
  EXPECT(!parse_state.reentered_after_block);
  EXPECT(parse_state.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);
  lonejson_cleanup(&test_person_map, &doc, NULL);

  poison_bytes(&doc, sizeof(doc), 0xA5u);
  memset(&parse_state, 0, sizeof(parse_state));
  parse_state.json = json;
  parse_state.chunk_size = 32u;
  parse_state.block_after = 32u;
  status = runtime->parse_reader(runtime, &test_person_map, &doc,
                                 test_would_block_tripwire_reader, &parse_state,
                                 &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(parse_state.blocked);
  EXPECT(!parse_state.reentered_after_block);
  EXPECT(parse_state.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);
  lonejson_cleanup(&test_person_map, &doc, NULL);

  memset(&validate_state, 0, sizeof(validate_state));
  validate_state.json = json;
  validate_state.chunk_size = 32u;
  validate_state.block_after = 32u;
  status = lonejson_validate_reader(runtime, test_would_block_tripwire_reader,
                                    &validate_state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(validate_state.blocked);
  EXPECT(!validate_state.reentered_after_block);
  EXPECT(validate_state.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);

  memset(&validate_state, 0, sizeof(validate_state));
  validate_state.json = json;
  validate_state.chunk_size = 32u;
  validate_state.block_after = 32u;
  status = runtime->validate_reader(runtime, test_would_block_tripwire_reader,
                                    &validate_state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(validate_state.blocked);
  EXPECT(!validate_state.reentered_after_block);
  EXPECT(validate_state.call_count == 2u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(strstr(error.message, "reader would block") != NULL);
}

static void test_parse_dynamic_string_limits_and_fixed_unaffected(void) {
  size_t text_len = LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES + 1u;
  char *text = make_repeating_text(text_len);
  const char *fmt = "{\"name\":\"%s\",\"body\":\"\",\"tags\":[]}";
  char *json;
  size_t need;
  test_alloc_parse_doc alloc_doc;
  test_fixed_log fixed_log;
  lonejson_int64 codes_storage[1];
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  EXPECT(text != NULL);
  if (text == NULL) {
    return;
  }
  need = snprintf(NULL, 0, fmt, text);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(text);
    return;
  }
  snprintf(json, need + 1u, fmt, text);

  memset(&alloc_doc, 0, sizeof(alloc_doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &alloc_doc,
                NULL);
  status = test_parse_cstr(&test_alloc_parse_doc_map, &alloc_doc, json, NULL,
                           &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max dynamic string bytes") != NULL);
  lonejson_cleanup(&test_alloc_parse_doc_map, &alloc_doc, NULL);

  options.max_dynamic_string_bytes = 0u;
  memset(&alloc_doc, 0, sizeof(alloc_doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &alloc_doc,
                NULL);
  status = test_parse_cstr(&test_alloc_parse_doc_map, &alloc_doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc_doc.name != NULL && strlen(alloc_doc.name) == text_len);
  lonejson_cleanup(&test_alloc_parse_doc_map, &alloc_doc, NULL);

  memset(&fixed_log, 0, sizeof(fixed_log));
  lonejson_init(test_default_runtime(), &test_fixed_log_map, &fixed_log, NULL);
  fixed_log.codes.items = codes_storage;
  fixed_log.codes.capacity = 1u;
  fixed_log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.max_dynamic_string_bytes = 1u;
  status =
      test_parse_cstr(&test_fixed_log_map, &fixed_log,
                      "{\"name\":\"fixed\",\"codes\":[7]}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(fixed_log.name, "fixed") == 0);
  lonejson_cleanup(&test_fixed_log_map, &fixed_log, NULL);

  free(json);
  free(text);
}

static void test_parse_dynamic_string_limit_mid_token_reader_cleanup(void) {
  test_generated_json_reader_state reader;
  test_alloc_parse_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  memset(&reader, 0, sizeof(reader));
  reader.prefix = "{\"name\":\"";
  reader.prefix_len = strlen(reader.prefix);
  reader.payload_len = 96u;
  reader.suffix = "\",\"body\":\"\",\"tags\":[]}";
  reader.suffix_len = strlen(reader.suffix);
  reader.fill = 'n';
  reader.chunk_limit = 7u;

  options.max_dynamic_string_bytes = 48u;
  reset_lonejson_alloc_stats();
  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &doc, NULL);
  status =
      test_parse_reader(&test_alloc_parse_doc_map, &doc,
                        test_generated_json_reader, &reader, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max dynamic string bytes") != NULL);
  EXPECT(doc.name == NULL);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_parse_string_array_element_dynamic_limit(void) {
  lonejson__parse_options options = lonejson__default_parse_options();
  size_t text_len = 64u;
  char *text = make_repeating_text(text_len);
  const char *fmt = "{\"name\":\"ok\",\"body\":\"\",\"tags\":[\"%s\"]}";
  char *json;
  size_t need;
  test_alloc_parse_doc doc;
  lonejson_error error;
  lonejson_status status;

  EXPECT(text != NULL);
  if (text == NULL) {
    return;
  }
  need = snprintf(NULL, 0, fmt, text);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(text);
    return;
  }
  snprintf(json, need + 1u, fmt, text);
  options.max_dynamic_string_bytes = 32u;

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &doc, NULL);
  status =
      test_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max dynamic string bytes") != NULL);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);

  free(json);
  free(text);
}

static void test_parse_alloc_cap_covers_capture_and_arrays(void) {
  size_t text_len = LONEJSON_PARSE_MAX_ALLOC_BYTES + 256u;
  char *text = make_repeating_text(text_len);
  const char *fmt = "{\"id\":\"capture\",\"selector\":\"%s\"}";
  char *json;
  size_t need;
  test_alloc_json_value_doc capture_doc;
  test_conditional_doc array_doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  EXPECT(text != NULL);
  if (text == NULL) {
    return;
  }
  need = snprintf(NULL, 0, fmt, text);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(text);
    return;
  }
  snprintf(json, need + 1u, fmt, text);

  memset(&capture_doc, 0, sizeof(capture_doc));
  lonejson_init(test_default_runtime(), &test_alloc_json_value_doc_map,
                &capture_doc, NULL);
  capture_doc.selector.parse_visitor_limits.max_string_bytes = 0u;
  status =
      lonejson_json_value_enable_parse_capture(&capture_doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = test_parse_cstr(&test_alloc_json_value_doc_map, &capture_doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "parse allocations exceed configured max bytes") != NULL);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &capture_doc, NULL);

  options.max_alloc_bytes = 0u;
  memset(&capture_doc, 0, sizeof(capture_doc));
  lonejson_init(test_default_runtime(), &test_alloc_json_value_doc_map,
                &capture_doc, NULL);
  capture_doc.selector.parse_visitor_limits.max_string_bytes = 0u;
  status =
      lonejson_json_value_enable_parse_capture(&capture_doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_parse_cstr(&test_alloc_json_value_doc_map, &capture_doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(capture_doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &capture_doc, NULL);

  memset(&array_doc, 0, sizeof(array_doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &array_doc,
                NULL);
  options = lonejson__default_parse_options();
  options.max_alloc_bytes = 24u;
  status = test_parse_cstr(&test_conditional_doc_map, &array_doc,
                           "{\"ints\":[1]}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "parse allocations exceed configured max bytes") != NULL);
  lonejson_cleanup(&test_conditional_doc_map, &array_doc, NULL);

  options.max_alloc_bytes = 64u;
  memset(&array_doc, 0, sizeof(array_doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &array_doc,
                NULL);
  status = test_parse_cstr(&test_conditional_doc_map, &array_doc,
                           "{\"ints\":[1]}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(array_doc.ints.count == 1u);
  EXPECT(array_doc.ints.items[0] == 1);
  lonejson_cleanup(&test_conditional_doc_map, &array_doc, NULL);

  free(json);
  free(text);
}

static void test_parse_alloc_cap_cumulative_tag_allocations(void) {
  test_alloc_parse_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  options.max_alloc_bytes = 64u;
  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &doc, NULL);
  status = test_parse_cstr(
      &test_alloc_parse_doc_map, &doc,
      "{\"name\":\"ok\",\"body\":\"\",\"tags\":[\"aaaa\",\"bbbb\",\"cccc\","
      "\"dddd\",\"eeee\",\"ffff\"]}",
      &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "parse allocations exceed configured max bytes") != NULL);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
}

static void test_parse_alloc_cap_counts_reused_destination_allocations(void) {
  test_conditional_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  size_t dyn_bytes;

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &doc, NULL);

  options.clear_destination = 0;
  options.max_alloc_bytes = 0u;
  status = test_parse_cstr(
      &test_conditional_doc_map, &doc,
      "{\"dyn\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.dyn != NULL);
  dyn_bytes = lonejson__owned_size(doc.dyn);
  EXPECT(dyn_bytes != 0u);

  options.max_alloc_bytes = dyn_bytes + sizeof(lonejson_int64) * 4u - 1u;
  status = test_parse_cstr(&test_conditional_doc_map, &doc, "{\"ints\":[1]}",
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "parse allocations exceed configured max bytes") != NULL);
  EXPECT(doc.dyn != NULL);
  EXPECT(strcmp(doc.dyn, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == 0);
  EXPECT(doc.ints.count == 0u);

  lonejson_cleanup(&test_conditional_doc_map, &doc, NULL);
}

static void test_parse_alloc_cap_small_budgets_allow_small_values(void) {
  test_alloc_parse_doc alloc_doc;
  test_alloc_json_value_doc capture_doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  options.clear_destination = 0;
  options.max_alloc_bytes = 8u;

  memset(&alloc_doc, 0, sizeof(alloc_doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &alloc_doc,
                NULL);
  status = test_parse_cstr(&test_alloc_parse_doc_map, &alloc_doc,
                           "{\"name\":\"a\",\"body\":\"\",\"tags\":[]}",
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc_doc.name != NULL && strcmp(alloc_doc.name, "a") == 0);
  lonejson_cleanup(&test_alloc_parse_doc_map, &alloc_doc, NULL);

  memset(&capture_doc, 0, sizeof(capture_doc));
  lonejson_init(test_default_runtime(), &test_alloc_json_value_doc_map,
                &capture_doc, NULL);
  status =
      lonejson_json_value_enable_parse_capture(&capture_doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      test_parse_cstr(&test_alloc_json_value_doc_map, &capture_doc,
                      "{\"id\":\"c\",\"selector\":\"x\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(capture_doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(capture_doc.selector.json, "\"x\"") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &capture_doc, NULL);
}

static void test_direct_string_duplicate_rejection_preserves_first_value(void) {
  test_conditional_doc fixed_doc;
  test_alloc_parse_doc dyn_doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;

  memset(&dyn_doc, 0, sizeof(dyn_doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &dyn_doc,
                NULL);
  status = test_parse_cstr(&test_alloc_parse_doc_map, &dyn_doc,
                           "{\"name\":\"keep\",\"body\":\"\",\"tags\":[]}",
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_parse_cstr(
      &test_alloc_parse_doc_map, &dyn_doc,
      "{\"name\":\"old\",\"name\":\"new\",\"body\":\"\",\"tags\":[]}", &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(dyn_doc.name != NULL);
  EXPECT(strcmp(dyn_doc.name, "old") == 0);
  lonejson_cleanup(&test_alloc_parse_doc_map, &dyn_doc, NULL);

  memset(&fixed_doc, 0, sizeof(fixed_doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &fixed_doc,
                NULL);
  status = test_parse_cstr(&test_conditional_doc_map, &fixed_doc,
                           "{\"fixed\":\"keep\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = test_parse_cstr(&test_conditional_doc_map, &fixed_doc,
                           "{\"fixed\":\"old\",\"fixed\":\"new\"}", &options,
                           &error);
  EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(strcmp(fixed_doc.fixed, "old") == 0);
  lonejson_cleanup(&test_conditional_doc_map, &fixed_doc, NULL);

  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_direct_string_failure_paths_preserve_existing_values(void) {
  test_conditional_doc fixed_doc;
  test_conditional_doc dyn_doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  size_t live_before;

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;

  memset(&dyn_doc, 0, sizeof(dyn_doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &dyn_doc,
                NULL);
  status = test_parse_cstr(&test_conditional_doc_map, &dyn_doc,
                           "{\"dyn\":\"keep\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  live_before = alloc.stats.bytes_live;
  status = test_parse_cstr(&test_conditional_doc_map, &dyn_doc,
                           "{\"dyn\":\"bad\\u0000tail\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(dyn_doc.dyn != NULL);
  EXPECT(strcmp(dyn_doc.dyn, "keep") == 0);
  EXPECT(alloc.stats.bytes_live == live_before);
  lonejson_cleanup(&test_conditional_doc_map, &dyn_doc, NULL);

  memset(&fixed_doc, 0, sizeof(fixed_doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &fixed_doc,
                NULL);
  status = test_parse_cstr(&test_conditional_doc_map, &fixed_doc,
                           "{\"fixed\":\"keep\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  live_before = alloc.stats.bytes_live;
  status = test_parse_cstr(&test_conditional_doc_map, &fixed_doc,
                           "{\"fixed\":\"toolong8\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strcmp(fixed_doc.fixed, "keep") == 0);
  EXPECT(alloc.stats.bytes_live == live_before);
  status = test_parse_cstr(&test_conditional_doc_map, &fixed_doc,
                           "{\"fixed\":\"bad\\u0000tail\"}", &options, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(strcmp(fixed_doc.fixed, "keep") == 0);
  EXPECT(alloc.stats.bytes_live == live_before);
  lonejson_cleanup(&test_conditional_doc_map, &fixed_doc, NULL);

  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_direct_string_fixed_large_capacity_stays_valid(void) {
  test_large_fixed_string_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char *payload = make_repeating_text(6000u);
  char *json;
  char *small_json;
  size_t need;
  size_t small_need;

  EXPECT(payload != NULL);
  if (payload == NULL) {
    return;
  }

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_large_fixed_string_doc_map, &doc,
                NULL);
  need = snprintf(NULL, 0, "{\"payload\":\"%s\"}", payload);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(payload);
    return;
  }
  snprintf(json, need + 1u, "{\"payload\":\"%s\"}", payload);
  small_need = snprintf(NULL, 0, "{\"payload\":\"small\"}");
  small_json = (char *)malloc(small_need + 1u);
  EXPECT(small_json != NULL);
  if (small_json == NULL) {
    free(json);
    free(payload);
    return;
  }
  snprintf(small_json, small_need + 1u, "{\"payload\":\"small\"}");

  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, payload) == 0);

  options.clear_destination = 0;
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, small_json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, "small") == 0);

  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, payload) == 0);

  free(small_json);
  free(json);
  free(payload);
}

static void test_parse_dynamic_string_limit_size_max_is_valid(void) {
  test_alloc_parse_doc doc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char *text = make_repeating_text(256u);
  char *json;
  size_t need;

  EXPECT(text != NULL);
  if (text == NULL) {
    return;
  }

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map, &doc, NULL);
  options.max_dynamic_string_bytes = SIZE_MAX;
  need = snprintf(NULL, 0, "{\"name\":\"%s\",\"body\":\"\",\"tags\":[]}", text);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(text);
    lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
    return;
  }
  snprintf(json, need + 1u, "{\"name\":\"%s\",\"body\":\"\",\"tags\":[]}",
           text);
  status =
      test_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.name != NULL);
  EXPECT(strcmp(doc.name, text) == 0);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc, NULL);
  free(json);
  free(text);
}

static void test_fixed_string_reuse_alloc_stage_obeys_budget(void) {
  test_large_fixed_string_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char *first = make_repeating_text(6000u);
  char *second = make_repeating_text(7000u);
  char *json_first;
  char *json_second;
  size_t need_first;
  size_t need_second;

  EXPECT(first != NULL);
  EXPECT(second != NULL);
  if (first == NULL || second == NULL) {
    free(first);
    free(second);
    return;
  }

  need_first = snprintf(NULL, 0, "{\"payload\":\"%s\"}", first);
  need_second = snprintf(NULL, 0, "{\"payload\":\"%s\"}", second);
  json_first = (char *)malloc(need_first + 1u);
  json_second = (char *)malloc(need_second + 1u);
  EXPECT(json_first != NULL);
  EXPECT(json_second != NULL);
  if (json_first == NULL || json_second == NULL) {
    free(json_first);
    free(json_second);
    free(first);
    free(second);
    return;
  }
  snprintf(json_first, need_first + 1u, "{\"payload\":\"%s\"}", first);
  snprintf(json_second, need_second + 1u, "{\"payload\":\"%s\"}", second);

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_large_fixed_string_doc_map, &doc,
                NULL);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_first,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, first) == 0);

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;
  options.max_alloc_bytes = sizeof(doc.payload) - 1u;
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_second,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strcmp(doc.payload, first) == 0);
  EXPECT(alloc.stats.bytes_live == 0u);

  options.max_alloc_bytes = sizeof(doc.payload);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_second,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, second) == 0);
  EXPECT(alloc.stats.bytes_live == 0u);

  free(json_first);
  free(json_second);
  free(first);
  free(second);
}

static void test_fixed_string_small_scratch_falls_back_to_budgeted_alloc(void) {
  test_large_fixed_string_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char scratch[64];
  char *first = make_repeating_text(6000u);
  char *second = make_repeating_text(7000u);
  char *json_first;
  char *json_second;
  size_t need_first;
  size_t need_second;

  EXPECT(first != NULL);
  EXPECT(second != NULL);
  if (first == NULL || second == NULL) {
    free(first);
    free(second);
    return;
  }

  need_first = snprintf(NULL, 0, "{\"payload\":\"%s\"}", first);
  need_second = snprintf(NULL, 0, "{\"payload\":\"%s\"}", second);
  json_first = (char *)malloc(need_first + 1u);
  json_second = (char *)malloc(need_second + 1u);
  EXPECT(json_first != NULL);
  EXPECT(json_second != NULL);
  if (json_first == NULL || json_second == NULL) {
    free(json_first);
    free(json_second);
    free(first);
    free(second);
    return;
  }
  snprintf(json_first, need_first + 1u, "{\"payload\":\"%s\"}", first);
  snprintf(json_second, need_second + 1u, "{\"payload\":\"%s\"}", second);

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_large_fixed_string_doc_map, &doc,
                NULL);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_first,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, first) == 0);

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;
  options.fixed_string_scratch = scratch;
  options.fixed_string_scratch_size = sizeof(scratch);
  options.max_alloc_bytes = sizeof(doc.payload) - 1u;
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_second,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strcmp(doc.payload, first) == 0);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);

  options.max_alloc_bytes = sizeof(doc.payload);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_second,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, second) == 0);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live == 0u);

  free(json_first);
  free(json_second);
  free(first);
  free(second);
}

static void test_fixed_string_reuse_scratch_avoids_alloc_budget(void) {
  test_large_fixed_string_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char scratch[8192];
  char *payload = make_repeating_text(6000u);
  char *json;
  size_t need;

  EXPECT(payload != NULL);
  if (payload == NULL) {
    return;
  }
  need = snprintf(NULL, 0, "{\"payload\":\"%s\"}", payload);
  json = (char *)malloc(need + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    free(payload);
    return;
  }
  snprintf(json, need + 1u, "{\"payload\":\"%s\"}", payload);

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_large_fixed_string_doc_map, &doc,
                NULL);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.max_alloc_bytes = 1u;
  options.fixed_string_scratch = scratch;
  options.fixed_string_scratch_size = sizeof(scratch);
  options.allocator = &alloc.allocator;
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, payload) == 0);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);

  free(json);
  free(payload);
}

static void
test_fixed_string_reuse_alloc_stage_failure_preserves_old_value(void) {
  test_large_fixed_string_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char *first = make_repeating_text(6000u);
  char *bad = make_repeating_text(7000u);
  char *json_first;
  char *json_bad;
  size_t need_first;
  size_t need_bad;

  EXPECT(first != NULL);
  EXPECT(bad != NULL);
  if (first == NULL || bad == NULL) {
    free(first);
    free(bad);
    return;
  }

  need_first = snprintf(NULL, 0, "{\"payload\":\"%s\"}", first);
  need_bad = snprintf(NULL, 0, "{\"payload\":\"%s\\u0000tail\"}", bad);
  json_first = (char *)malloc(need_first + 1u);
  json_bad = (char *)malloc(need_bad + 1u);
  EXPECT(json_first != NULL);
  EXPECT(json_bad != NULL);
  if (json_first == NULL || json_bad == NULL) {
    free(json_first);
    free(json_bad);
    free(first);
    free(bad);
    return;
  }
  snprintf(json_first, need_first + 1u, "{\"payload\":\"%s\"}", first);
  snprintf(json_bad, need_bad + 1u, "{\"payload\":\"%s\\u0000tail\"}", bad);

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_large_fixed_string_doc_map, &doc,
                NULL);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_first,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, first) == 0);

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;
  options.max_alloc_bytes = sizeof(doc.payload);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_bad,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(strcmp(doc.payload, first) == 0);
  EXPECT(alloc.stats.bytes_live == 0u);

  free(json_first);
  free(json_bad);
  free(first);
  free(bad);
}

static void test_fixed_string_reuse_exact_scratch_boundary_and_failures(void) {
  test_large_fixed_string_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char scratch[8192];
  char *first = make_repeating_text(6000u);
  char *second = make_repeating_text(7000u);
  char *third = make_repeating_text(6500u);
  char *json_first;
  char *json_second;
  char *json_dup;
  char *json_bad;
  size_t need_first;
  size_t need_second;
  size_t need_dup;
  size_t need_bad;

  EXPECT(first != NULL);
  EXPECT(second != NULL);
  EXPECT(third != NULL);
  if (first == NULL || second == NULL || third == NULL) {
    free(first);
    free(second);
    free(third);
    return;
  }

  need_first = snprintf(NULL, 0, "{\"payload\":\"%s\"}", first);
  need_second = snprintf(NULL, 0, "{\"payload\":\"%s\"}", second);
  need_dup = snprintf(NULL, 0, "{\"payload\":\"%s\",\"payload\":\"%s\"}",
                      second, third);
  need_bad = snprintf(NULL, 0, "{\"payload\":\"%s\\u0000tail\"}", third);
  json_first = (char *)malloc(need_first + 1u);
  json_second = (char *)malloc(need_second + 1u);
  json_dup = (char *)malloc(need_dup + 1u);
  json_bad = (char *)malloc(need_bad + 1u);
  EXPECT(json_first != NULL);
  EXPECT(json_second != NULL);
  EXPECT(json_dup != NULL);
  EXPECT(json_bad != NULL);
  if (json_first == NULL || json_second == NULL || json_dup == NULL ||
      json_bad == NULL) {
    free(json_first);
    free(json_second);
    free(json_dup);
    free(json_bad);
    free(first);
    free(second);
    free(third);
    return;
  }
  snprintf(json_first, need_first + 1u, "{\"payload\":\"%s\"}", first);
  snprintf(json_second, need_second + 1u, "{\"payload\":\"%s\"}", second);
  snprintf(json_dup, need_dup + 1u, "{\"payload\":\"%s\",\"payload\":\"%s\"}",
           second, third);
  snprintf(json_bad, need_bad + 1u, "{\"payload\":\"%s\\u0000tail\"}", third);

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_large_fixed_string_doc_map, &doc,
                NULL);
  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_first,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, first) == 0);

  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;
  options.max_alloc_bytes = 1u;
  options.fixed_string_scratch = scratch;
  options.fixed_string_scratch_size = sizeof(scratch);

  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_second,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.payload, second) == 0);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);

  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_dup,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(strcmp(doc.payload, second) == 0);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);

  status = test_parse_cstr(&test_large_fixed_string_doc_map, &doc, json_bad,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(strcmp(doc.payload, second) == 0);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);

  free(json_first);
  free(json_second);
  free(json_dup);
  free(json_bad);
  free(first);
  free(second);
  free(third);
}

static void test_fixed_string_stage_competes_with_existing_alloc_budget(void) {
  test_alloc_fixed_combo_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;
  char *payload = make_repeating_text(6000u);
  char *json;
  char *payload_only_json;
  char scratch[8192];
  size_t need;
  size_t payload_only_need;
  size_t dyn_bytes;

  EXPECT(payload != NULL);
  if (payload == NULL) {
    return;
  }
  need = snprintf(NULL, 0, "{\"dyn\":\"hold\",\"payload\":\"%s\"}", payload);
  payload_only_need = snprintf(NULL, 0, "{\"payload\":\"%s\"}", payload);
  json = (char *)malloc(need + 1u);
  payload_only_json = (char *)malloc(payload_only_need + 1u);
  EXPECT(json != NULL);
  EXPECT(payload_only_json != NULL);
  if (json == NULL || payload_only_json == NULL) {
    free(payload);
    free(json);
    free(payload_only_json);
    return;
  }
  snprintf(json, need + 1u, "{\"dyn\":\"hold\",\"payload\":\"%s\"}", payload);
  snprintf(payload_only_json, payload_only_need + 1u, "{\"payload\":\"%s\"}",
           payload);

  test_allocator_init(&alloc);
  options.allocator = &alloc.allocator;
  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_alloc_fixed_combo_doc_map, &doc,
                NULL);
  status = test_parse_cstr(&test_alloc_fixed_combo_doc_map, &doc, json,
                           &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.dyn != NULL);
  dyn_bytes = lonejson__owned_size(doc.dyn);
  EXPECT(dyn_bytes != 0u);

  options.clear_destination = 0;
  options.max_alloc_bytes = dyn_bytes + sizeof(doc.payload) - 1u;
  status = test_parse_cstr(&test_alloc_fixed_combo_doc_map, &doc,
                           payload_only_json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strcmp(doc.dyn, "hold") == 0);
  EXPECT(strcmp(doc.payload, payload) == 0);
  EXPECT(alloc.stats.bytes_live == dyn_bytes);

  options.fixed_string_scratch = scratch;
  options.fixed_string_scratch_size = sizeof(scratch);
  options.max_alloc_bytes = dyn_bytes;
  status = test_parse_cstr(&test_alloc_fixed_combo_doc_map, &doc,
                           payload_only_json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.dyn, "hold") == 0);
  EXPECT(strcmp(doc.payload, payload) == 0);
  EXPECT(alloc.stats.bytes_live == dyn_bytes);

  lonejson_cleanup(&test_alloc_fixed_combo_doc_map, &doc, NULL);
  free(json);
  free(payload_only_json);
  free(payload);
}

static void test_parse_alloc_cap_array_truncate_modes_still_fail_hard(void) {
  static const char ints_json[] = "{\"ints\":[1,2,3,4,5]}";
  static const char tags_json[] = "{\"tags\":[\"a\",\"b\",\"c\",\"d\",\"e\"]}";
  static const char items_json[] =
      "{\"items\":[{\"id\":1},{\"id\":2},{\"id\":3},{\"id\":4},{\"id\":5}]}";
  const lonejson_map *maps[] = {&test_trunc_array_doc_map,
                                &test_silent_array_doc_map};
  size_t i;

  for (i = 0u; i < sizeof(maps) / sizeof(maps[0]); ++i) {
    test_trunc_array_doc doc;
    lonejson__parse_options options = lonejson__default_parse_options();
    lonejson_error error;
    lonejson_status status;

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), maps[i], &doc, NULL);
    options.max_alloc_bytes = sizeof(lonejson_int64) * 4u - 1u;
    status = test_parse_cstr(maps[i], &doc, ints_json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(strstr(error.message,
                  "parse allocations exceed configured max bytes") != NULL);
    EXPECT(doc.ints.count == 0u);
    lonejson_cleanup(maps[i], &doc, NULL);

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), maps[i], &doc, NULL);
    options.max_alloc_bytes = sizeof(lonejson_int64) * 4u;
    status = test_parse_cstr(maps[i], &doc, ints_json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(strstr(error.message,
                  "parse allocations exceed configured max bytes") != NULL);
    EXPECT(doc.ints.count == 4u);
    lonejson_cleanup(maps[i], &doc, NULL);

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), maps[i], &doc, NULL);
    options.max_alloc_bytes = sizeof(char *) * 4u - 1u;
    status = test_parse_cstr(maps[i], &doc, tags_json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(strstr(error.message,
                  "parse allocations exceed configured max bytes") != NULL);
    EXPECT(doc.tags.count == 0u);
    lonejson_cleanup(maps[i], &doc, NULL);

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), maps[i], &doc, NULL);
    options.max_alloc_bytes = sizeof(char *) * 4u + 8u;
    status = test_parse_cstr(maps[i], &doc, tags_json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(strstr(error.message,
                  "parse allocations exceed configured max bytes") != NULL);
    EXPECT(doc.tags.count == 4u);
    lonejson_cleanup(maps[i], &doc, NULL);

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), maps[i], &doc, NULL);
    options.max_alloc_bytes = sizeof(test_item) * 4u - 1u;
    status = test_parse_cstr(maps[i], &doc, items_json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(strstr(error.message,
                  "parse allocations exceed configured max bytes") != NULL);
    EXPECT(doc.items.count == 0u);
    lonejson_cleanup(maps[i], &doc, NULL);

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), maps[i], &doc, NULL);
    options.max_alloc_bytes = sizeof(test_item) * 4u;
    status = test_parse_cstr(maps[i], &doc, items_json, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(strstr(error.message,
                  "parse allocations exceed configured max bytes") != NULL);
    EXPECT(doc.items.count == 4u);
    lonejson_cleanup(maps[i], &doc, NULL);
  }
}

static void test_parse_spool_default_and_unbounded_limits(void) {
  static const char default_prefix[] = "{\"text\":\"";
  static const char default_suffix[] = "\"}";
  static const char explicit_prefix[] = "{\"name\":\"ok\",\"body\":\"";
  static const char explicit_suffix[] = "\",\"tags\":[]}";
  test_generated_json_reader_state reader;
  test_conditional_doc default_doc;
  test_alloc_parse_doc explicit_doc;
  lonejson_error error;
  lonejson_status status;
  size_t payload_len = LONEJSON_SPOOL_MAX_BYTES + 1u;

  memset(&default_doc, 0, sizeof(default_doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &default_doc,
                NULL);
  memset(&reader, 0, sizeof(reader));
  reader.prefix = default_prefix;
  reader.prefix_len = sizeof(default_prefix) - 1u;
  reader.payload_len = payload_len;
  reader.suffix = default_suffix;
  reader.suffix_len = sizeof(default_suffix) - 1u;
  reader.fill = 'x';
  reader.chunk_limit = 4096u;
  status = test_parse_reader(&test_conditional_doc_map, &default_doc,
                             test_generated_json_reader, &reader, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "spooled field exceeds configured max bytes") !=
         NULL);
  lonejson_cleanup(&test_conditional_doc_map, &default_doc, NULL);

  memset(&explicit_doc, 0, sizeof(explicit_doc));
  lonejson_init(test_default_runtime(), &test_alloc_parse_doc_map,
                &explicit_doc, NULL);
  memset(&reader, 0, sizeof(reader));
  reader.prefix = explicit_prefix;
  reader.prefix_len = sizeof(explicit_prefix) - 1u;
  reader.payload_len = payload_len;
  reader.suffix = explicit_suffix;
  reader.suffix_len = sizeof(explicit_suffix) - 1u;
  reader.fill = 'y';
  reader.chunk_limit = 4096u;
  status = test_parse_reader(&test_alloc_parse_doc_map, &explicit_doc,
                             test_generated_json_reader, &reader, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&explicit_doc.body) == payload_len);
  EXPECT(lonejson_spooled_spilled(&explicit_doc.body) != 0);
  lonejson_cleanup(&test_alloc_parse_doc_map, &explicit_doc, NULL);
}

static void test_parse_base64_stream_default_limit(void) {
  static const char prefix[] = "{\"id\":\"b64\",\"text\":\"\",\"bytes\":\"";
  static const char suffix[] = "\"}";
  test_generated_json_reader_state reader;
  test_conditional_doc doc;
  lonejson_error error;
  lonejson_status status;
  size_t decoded_len = LONEJSON_SPOOL_MAX_BYTES + 1u;
  size_t encoded_len = ((decoded_len + 2u) / 3u) * 4u;

  memset(&reader, 0, sizeof(reader));
  reader.prefix = prefix;
  reader.prefix_len = sizeof(prefix) - 1u;
  reader.payload_len = encoded_len;
  reader.suffix = suffix;
  reader.suffix_len = sizeof(suffix) - 1u;
  reader.fill = 'A';
  reader.chunk_limit = 4096u;

  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_conditional_doc_map, &doc, NULL);
  status = test_parse_reader(&test_conditional_doc_map, &doc,
                             test_generated_json_reader, &reader, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "spooled field exceeds configured max bytes") !=
         NULL);
  lonejson_cleanup(&test_conditional_doc_map, &doc, NULL);
}

static void test_f64_boundary_exponents_stay_on_fast_path(void) {
  double parsed;

  parsed = 0.0;
  EXPECT(lonejson__parse_f64_fast("0.1e309", strlen("0.1e309"), &parsed));
  EXPECT(parsed > 9.0e307);
  EXPECT(parsed < DBL_MAX);

  parsed = 0.0;
  EXPECT(lonejson__parse_f64_fast("1.5e-309", strlen("1.5e-309"), &parsed));
  EXPECT(parsed > 0.0);
  EXPECT(parsed < 1.0e-308);
}

static void test_f64_serializer_fast_format_matches_snprintf(void) {
  static const double values[] = {
      0.0625, 0.1, 0.5, 1.5, 99.5, -99.5, 999999999999.9375, 1.0e12, -0.0};
  size_t i;

  for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
    char expected[64];
    char actual[64];
    int expected_len;
    int actual_len;

    expected_len = snprintf(expected, sizeof(expected), "%.17g", values[i]);
    actual_len = lonejson__format_f64_text(actual, sizeof(actual), values[i]);
    EXPECT(actual_len == expected_len);
    if (actual_len == expected_len && actual_len >= 0) {
      EXPECT(memcmp(actual, expected, (size_t)actual_len + 1u) == 0);
    }
  }
}

static void test_f64_boundary_exponents_are_locale_independent(void) {
  static const char *const locales[] = {"sv_SE.UTF-8", "sv_SE.utf8",
                                        "de_DE.UTF-8", "de_DE.utf8",
                                        "fr_FR.UTF-8", "fr_FR.utf8"};
  char saved_locale[128];
  const char *current_locale;
  const char *selected_locale = NULL;
  size_t i;
  test_person person;
  lonejson_error error;
  lonejson_status status;
  const char *json =
      "{\"name\":\"Locale\",\"score\":0.1e309,"
      "\"address\":{\"city\":\"Malmo\",\"zip\":211},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";

  current_locale = setlocale(LC_NUMERIC, NULL);
  snprintf(saved_locale, sizeof(saved_locale), "%s",
           current_locale != NULL ? current_locale : "C");
  for (i = 0u; i < sizeof(locales) / sizeof(locales[0]); ++i) {
    if (setlocale(LC_NUMERIC, locales[i]) != NULL) {
      selected_locale = locales[i];
      break;
    }
  }
  if (selected_locale == NULL) {
    (void)setlocale(LC_NUMERIC, saved_locale);
    return;
  }

  poison_bytes(&person, sizeof(person), 0x3Cu);
  status = test_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  if (status == LONEJSON_STATUS_OK) {
    EXPECT(person.score > 9.0e307);
    EXPECT(person.score < DBL_MAX);
    lonejson_cleanup(&test_person_map, &person, NULL);
  }
  (void)setlocale(LC_NUMERIC, saved_locale);
}

static void test_parser_workspace_accounting(void) {
  const char *json = "{\"name\":\"workspace-check\",\"codes\":[5,8,13,21]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];
  lonejson_error error;
  lonejson_status status;

  memset(&log, 0, sizeof(log));
  lonejson_init(test_default_runtime(), &test_fixed_log_map, &log, NULL);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              NULL, 0, 0, 0, 0u, parser_workspace,
                              sizeof(parser_workspace));
  status = lonejson__parser_feed_bytes(
      &parser_storage, (const unsigned char *)json, strlen(json), NULL, 0);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  status = lonejson_parser_finish(&parser_storage);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  error = parser_storage.error;
  EXPECT(error.code == LONEJSON_STATUS_OK || error.code == status);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(lonejson__parser_peak_workspace_used(&parser_storage) > 0u);
  EXPECT(lonejson__parser_peak_workspace_used(&parser_storage) <=
         (size_t)LONEJSON_PARSER_BUFFER_SIZE);
  lonejson_cleanup(&test_fixed_log_map, &log, NULL);
}

static void test_parser_workspace_alignment_regression(void) {
  const char *json = "{\"name\":\"aligned\",\"codes\":[1,2]}";
  lonejson_parser parser_storage;
  unsigned char raw_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                              LONEJSON__PARSER_WORKSPACE_ALIGNMENT];
  unsigned char *misaligned_workspace = raw_workspace + 1u;
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_status status;
  size_t alignment_offset;

  memset(&log, 0, sizeof(log));
  lonejson_init(test_default_runtime(), &test_fixed_log_map, &log, NULL);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              NULL, 0, 0, 0, 0u, misaligned_workspace,
                              LONEJSON_PARSER_BUFFER_SIZE);
  EXPECT(parser_storage.workspace != misaligned_workspace);
  EXPECT(parser_storage.workspace_size <= LONEJSON_PARSER_BUFFER_SIZE);
  alignment_offset = (size_t)(parser_storage.workspace - misaligned_workspace);
  EXPECT(alignment_offset < LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
  EXPECT(parser_storage.frames == (lonejson_frame *)parser_storage.workspace);
  EXPECT(parser_storage.workspace ==
         lonejson__align_pointer(misaligned_workspace,
                                 LONEJSON__PARSER_WORKSPACE_ALIGNMENT));

  status = lonejson__parser_feed_bytes(
      &parser_storage, (const unsigned char *)json, strlen(json), NULL, 0);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  status = lonejson_parser_finish(&parser_storage);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strcmp(log.name, "aligned") == 0);
  EXPECT(log.codes.count == 2u);
  EXPECT(log.codes.items[1] == 2);

  lonejson_cleanup(&test_fixed_log_map, &log, NULL);
}

static void test_init_does_not_preserve_garbage_array_state(void) {
  test_person person;

  poison_bytes(&person, sizeof(person), 0x7bu);
  person.items.items = (test_item *)0x10f;
  person.items.count = 99u;
  person.items.capacity = 99u;
  person.items.elem_size = sizeof(test_item);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  lonejson_init(test_default_runtime(), &test_person_map, &person, NULL);

  EXPECT(person.name == NULL);
  EXPECT(person.nickname[0] == '\0');
  EXPECT(person.items.items == NULL);
  EXPECT(person.items.count == 0u);
  EXPECT(person.items.capacity == 0u);
  EXPECT(person.items.flags == 0u);
  EXPECT(person.items.elem_size == sizeof(test_item));

  lonejson_cleanup(&test_person_map, &person, NULL);
}

static void test_zero_alloc_fixed_storage_parse(void) {
  const char *json = "{\"name\":\"long-event-name\",\"codes\":[5,8,13]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_error error;
  lonejson_status status;
  lonejson__parse_options options = lonejson__default_parse_options();

  memset(&log, 0, sizeof(log));
  lonejson_init(test_default_runtime(), &test_fixed_log_map, &log, NULL);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;

  reset_lonejson_alloc_stats();
  status = test_parse_cstr(&test_fixed_log_map, &log, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strcmp(log.name, "long-ev") == 0);
  EXPECT(log.codes.count == 3u);
  EXPECT(log.codes.items[2] == 13);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  lonejson_cleanup(&test_fixed_log_map, &log, NULL);
}

static void test_zero_alloc_fixed_storage_serialize(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  test_counting_sink sink;
  char buffer[128];
  char path[] = LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-serialize-XXXXXX";
  lonejson_error error;
  lonejson_status status;
  int fd;
  FILE *fp;

  memset(&log, 0, sizeof(log));
  memset(codes_storage, 0, sizeof(codes_storage));
  memset(&sink, 0, sizeof(sink));
  strcpy(log.name, "evt");
  codes_storage[0] = 5;
  codes_storage[1] = 8;
  codes_storage[2] = 13;
  log.codes.items = codes_storage;
  log.codes.count = 3u;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  status = test_serialize_sink(&test_fixed_log_map, &log,
                               test_counting_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total != 0u);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  reset_lonejson_alloc_stats();
  status = test_serialize_buffer(&test_fixed_log_map, &log, buffer,
                                 sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"name\":\"evt\",\"codes\":[5,8,13]}") == 0);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

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
  reset_lonejson_alloc_stats();
  status = test_serialize_filep(&test_fixed_log_map, &log, fp, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  fclose(fp);

  reset_lonejson_alloc_stats();
  status = test_serialize_path(&test_fixed_log_map, &log, path, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  unlink(path);
}

static void test_zero_alloc_jsonl_serialize(void) {
  test_event events[2];
  test_counting_sink sink;
  char buffer[128];
  char path[] =
      LONEJSON_SOURCE_DIR "/build/test-tmp/lonejson-jsonl-serialize-XXXXXX";
  lonejson_error error;
  lonejson_status status;
  int fd;
  FILE *fp;

  memset(events, 0, sizeof(events));
  memset(&sink, 0, sizeof(sink));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  status =
      test_serialize_jsonl_sink(&test_event_map, events, 2u, 0u,
                                test_counting_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total != 0u);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  reset_lonejson_alloc_stats();
  status = test_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u, buffer,
                                       sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"evt-1\",\"ok\":true}\n"
                        "{\"id\":\"evt-2\",\"ok\":false}\n") == 0);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

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
  reset_lonejson_alloc_stats();
  status = test_serialize_jsonl_filep(&test_event_map, events, 2u, 0u, fp, NULL,
                                      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  fclose(fp);

  reset_lonejson_alloc_stats();
  status = test_serialize_jsonl_path(&test_event_map, events, 2u, 0u, path,
                                     NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  unlink(path);
}

static void test_serialize_alloc_balance(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[2];
  lonejson_error error;
  char *json;

  memset(&log, 0, sizeof(log));
  strcpy(log.name, "evt");
  codes_storage[0] = 1;
  codes_storage[1] = 2;
  log.codes.items = codes_storage;
  log.codes.count = 2u;
  log.codes.capacity = 2u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  json = test_serialize_alloc(&test_fixed_log_map, &log, NULL, NULL, &error);
  EXPECT(json != NULL);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count == 1u);
  if (json != NULL) {
    LONEJSON_FREE(json);
  }
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_serialize_owned_output_cap_is_enforced(void) {
  test_allocator_state alloc;
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_owned_buffer owned;
  test_event event;
  char buffer[128];
  size_t needed = 0u;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-cap");
  event.ok = true;

  options.max_output_bytes = 16u;
  status = test_serialize_buffer(&test_event_map, &event, buffer,
                                 sizeof(buffer), &needed, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"evt-cap\",\"ok\":true}") == 0);
  EXPECT(needed == strlen(buffer));

  test_allocator_init(&alloc);
  options.allocator = &alloc.allocator;
  lonejson_owned_buffer_init(&owned);
  status =
      test_serialize_owned(&test_event_map, &event, &owned, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max_output_bytes") != NULL);
  EXPECT(owned.data == NULL);
  EXPECT(owned.len == 0u);
  EXPECT(alloc.stats.peak_bytes_live <= options.max_output_bytes + 1u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_serialize_owned_output_cap_allows_exact_payload(void) {
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_owned_buffer owned;
  test_event event;
  const char *expected;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "exact");
  event.ok = true;
  expected = "{\"id\":\"exact\",\"ok\":true}";
  options.max_output_bytes = strlen(expected);
  lonejson_owned_buffer_init(&owned);

  status =
      test_serialize_owned(&test_event_map, &event, &owned, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(owned.data != NULL);
  EXPECT(owned.len == strlen(expected));
  if (owned.data != NULL) {
    EXPECT(strcmp(owned.data, expected) == 0);
    EXPECT(owned.data[owned.len] == '\0');
  }
  lonejson_owned_buffer_free(&owned);

  options.max_output_bytes = strlen(expected) - 1u;
  status =
      test_serialize_owned(&test_event_map, &event, &owned, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(owned.data == NULL);
}

static void test_serialize_alloc_zero_cap_uses_safe_default(void) {
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "zcap");
  event.ok = true;
  options.max_output_bytes = 0u;
  serialized =
      test_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"zcap\",\"ok\":true}") == 0);
    LONEJSON_FREE(serialized);
  }
}

static int test_child_serialize_alloc_free_compatibility(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[2];
  lonejson_error error;
  char *json;

  memset(&log, 0, sizeof(log));
  strcpy(log.name, "evt");
  codes_storage[0] = 1;
  codes_storage[1] = 2;
  log.codes.items = codes_storage;
  log.codes.count = 2u;
  log.codes.capacity = 2u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  json = test_serialize_alloc(&test_fixed_log_map, &log, NULL, NULL, &error);
  if (json == NULL) {
    return 2;
  }
  LONEJSON_FREE(json);
  return (g_alloc_record_count == 0u) ? 0 : 3;
}

static void test_serialize_alloc_default_free_compatibility(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_serialize_alloc_free_compatibility());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_serialize_jsonl_alloc_balance(void) {
  test_event events[2];
  lonejson_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  jsonl = test_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                     NULL, &error);
  EXPECT(jsonl != NULL);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count == 1u);
  if (jsonl != NULL) {
    LONEJSON_FREE(jsonl);
  }
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_serialize_jsonl_alloc_output_cap_is_enforced(void) {
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  test_event events[2];
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "cap-1");
  events[0].ok = true;
  strcpy(events[1].id, "cap-2");
  events[1].ok = false;

  options.max_output_bytes = 32u;
  jsonl = test_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                     &options, &error);
  EXPECT(jsonl == NULL);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max_output_bytes") != NULL);
}

static void test_serialize_alloc_pretty_output_cap_is_enforced(void) {
  lonejson__write_options options = lonejson__default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "pcap");
  event.ok = true;

  options.pretty = 1;
  options.max_output_bytes = 24u;
  serialized =
      test_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max_output_bytes") != NULL);
}

static int test_child_serialize_jsonl_alloc_free_compatibility(void) {
  test_event events[2];
  lonejson_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  jsonl = test_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                     NULL, &error);
  if (jsonl == NULL) {
    return 2;
  }
  LONEJSON_FREE(jsonl);
  return (g_alloc_record_count == 0u) ? 0 : 3;
}

static void test_serialize_jsonl_alloc_default_free_compatibility(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_serialize_jsonl_alloc_free_compatibility());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_dynamic_object_array_alignment(void) {
  static const char json[] =
      "{\"items\":[{\"name\":\"a\"},{\"name\":\"b\"},{\"name\":\"c\"}]}";
  test_aligned_doc doc;
  lonejson_error error;
  lonejson_status status;
  struct align_probe {
    char c;
    test_aligned_item value;
  };
  size_t alignment;
  uintptr_t bits;

  memset(&doc, 0, sizeof(doc));
  status = test_parse_cstr(&test_aligned_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.items.count == 3u);
  EXPECT(doc.items.items != NULL);
  if (doc.items.items != NULL) {
    alignment = offsetof(struct align_probe, value);
    bits = (uintptr_t)doc.items.items;
    EXPECT((bits % alignment) == 0u);
    EXPECT(strcmp(((test_aligned_item *)doc.items.items)[0].name, "a") == 0);
    ((test_aligned_item *)doc.items.items)[2].weight = (long double)3.75;
    EXPECT(((test_aligned_item *)doc.items.items)[2].weight > 3.7);
  }
  lonejson_cleanup(&test_aligned_doc_map, &doc, NULL);
}

static void test_dynamic_allocation_reset_reparse_balance(void) {
  const char *json_a =
      "{\"name\":\"First\",\"nickname\":\"aa\",\"age\":1,\"score\":1.0,"
      "\"active\":false,\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[1],\"tags\":[\"x\"],"
      "\"items\":[{\"id\":10,\"label\":\"one\"}]}";
  const char *json_b =
      "{\"name\":\"Second\",\"nickname\":\"bb\",\"age\":2,\"score\":2.0,"
      "\"active\":true,\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"lucky_numbers\":[7,8],\"tags\":[\"y\",\"z\"],"
      "\"items\":[{\"id\":20,\"label\":\"two\"},{\"id\":30,\"label\":\"three\"}"
      "]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  poison_bytes(&person, sizeof(person), 0x22u);
  status = test_parse_cstr(&test_person_map, &person, json_a, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  lonejson_reset(test_default_runtime(), &test_person_map, &person, NULL);
  EXPECT(g_alloc_record_count == 0u);
  status = test_parse_cstr(&test_person_map, &person, json_b, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Second") == 0);
  EXPECT(person.tags.count == 2);
  lonejson_cleanup(&test_person_map, &person, NULL);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_clear_destination_false_present_arrays_replace(void) {
  const char *json_a =
      "{\"name\":\"First\",\"nickname\":\"aa\",\"age\":1,\"score\":1.0,"
      "\"active\":false,\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[1,2,3],\"tags\":[\"x\",\"y\"],"
      "\"items\":[{\"id\":10,\"label\":\"one\"},{\"id\":11,\"label\":\"two\"}]"
      "}";
  const char *json_b =
      "{\"name\":\"First\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[7,8],\"tags\":[\"z\"],"
      "\"items\":[{\"id\":20,\"label\":\"three\"}]}";
  test_person person;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&person, sizeof(person), 0x23u);
  status = test_parse_cstr(&test_person_map, &person, json_a, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);

  options.clear_destination = 0;
  status = test_parse_cstr(&test_person_map, &person, json_b, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "First") == 0);
  EXPECT(person.lucky_numbers.count == 2u);
  EXPECT(person.lucky_numbers.items[0] == 7);
  EXPECT(person.lucky_numbers.items[1] == 8);
  EXPECT(person.tags.count == 1u);
  EXPECT(strcmp(person.tags.items[0], "z") == 0);
  EXPECT(person.items.count == 1u);
  EXPECT(((test_item *)person.items.items)[0].id == 20);

  lonejson_cleanup(&test_person_map, &person, NULL);
}

static void
test_clear_destination_false_fixed_capacity_string_array_reuse_frees_replaced_entries(
    void) {
  const char *json_a =
      "{\"name\":\"First\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"tags\":[\"old-a\",\"old-b\",\"old-c\"]}";
  const char *json_b =
      "{\"name\":\"Second\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"tags\":[\"fresh\"]}";
  test_person person;
  char *tag_storage[4];
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_error error;
  lonejson_status status;

  memset(&person, 0, sizeof(person));
  memset(tag_storage, 0, sizeof(tag_storage));
  test_allocator_init(&alloc);
  lonejson_init(test_default_runtime(), &test_person_map, &person, NULL);
  person.tags.items = tag_storage;
  person.tags.capacity = 4u;
  person.tags.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;

  status = test_parse_cstr(&test_person_map, &person, json_a, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.tags.count == 3u);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);

  status = test_parse_cstr(&test_person_map, &person, json_b, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.tags.count == 1u);
  EXPECT(strcmp(person.tags.items[0], "fresh") == 0);

  lonejson_cleanup(&test_person_map, &person, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_parse_buffer_basic(void) {
  const char *json = "{"
                     "\"name\":\"Alice\","
                     "\"nickname\":\"Wonderland\","
                     "\"age\":34,"
                     "\"score\":99.5,"
                     "\"active\":true,"
                     "\"address\":{\"city\":\"Stockholm\",\"zip\":12345},"
                     "\"lucky_numbers\":[7,11,42],"
                     "\"tags\":[\"admin\",\"ops\"],"
                     "\"items\":[{\"id\":1,\"label\":\"alpha\"},{\"id\":2,"
                     "\"label\":\"bravo-longer-than-fit\"}],"
                     "\"ignored\":{\"nested\":true}"
                     "}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  status = test_parse_cstr(&test_person_map, &person, json, NULL, &error);

  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Alice") == 0);
  EXPECT(strcmp(person.nickname, "Wonderl") == 0);
  EXPECT(person.age == 34);
  EXPECT(person.score > 99.4 && person.score < 99.6);
  EXPECT(person.active == true);
  EXPECT(strcmp(person.address.city, "Stockholm") == 0);
  EXPECT(person.address.zip == 12345);
  EXPECT(person.lucky_numbers.count == 3);
  EXPECT(person.lucky_numbers.items[0] == 7);
  EXPECT(person.tags.count == 2);
  EXPECT(strcmp(person.tags.items[1], "ops") == 0);
  EXPECT(person.items.count == 2);
  EXPECT(((test_item *)person.items.items)[0].id == 1);
  EXPECT(strcmp(((test_item *)person.items.items)[1].label, "bravo-longe") ==
         0);

  lonejson_cleanup(&test_person_map, &person, NULL);
}

static void test_parse_fixed_strings_with_escapes_and_truncation(void) {
  const char *json = "{"
                     "\"name\":\"Alice\","
                     "\"nickname\":\"AB\\nCDEFG\","
                     "\"age\":1,"
                     "\"score\":1.0,"
                     "\"active\":false,"
                     "\"address\":{\"city\":\"M\\u00f6lle\",\"zip\":7},"
                     "\"lucky_numbers\":[],"
                     "\"tags\":[],"
                     "\"items\":[]"
                     "}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  status = test_parse_cstr(&test_person_map, &person, json, NULL, &error);

  EXPECT(status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Alice") == 0);
  EXPECT(strcmp(person.nickname, "AB\nCDEF") == 0);
  EXPECT(strcmp(person.address.city, "M\303\266lle") == 0);
  EXPECT(person.address.zip == 7);

  lonejson_cleanup(&test_person_map, &person, NULL);
}

static void test_chunked_parser_and_missing_required(void) {
  test_reader_state state = {
      "{\"name\":\"Bob\",\"address\":{\"city\":\"Uppsala\",\"zip\":77},"
      "\"active\":false,\"lucky_numbers\":[1,2],\"tags\":[],\"items\":[]}",
      0u, 9u};
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_status status;
  lonejson_stream_result result;

  poison_bytes(&person, sizeof(person), 0x5Au);
  stream = test_stream_open_reader(&test_person_map, test_state_reader, &state,
                                   NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.name != NULL && strcmp(person.name, "Bob") == 0);
  EXPECT(strcmp(person.address.city, "Uppsala") == 0);
  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &person, NULL);

  poison_bytes(&person, sizeof(person), 0x5Au);
  status =
      test_parse_cstr(&test_person_map, &person, "{\"age\":1}", NULL, &error);
  EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  lonejson_cleanup(&test_person_map, &person, NULL);
}

static void test_incremental_parse_large_dynamic_strings(void) {
  static const size_t feed_chunks[] = {1u, 7u, 97u};
  const size_t text_len = 10000u;
  const char *prefix = "{\"type\":\"doc\",\"text\":\"";
  const char *suffix = "\"}";
  size_t json_len;
  char *json;
  size_t i;

  json_len = strlen(prefix) + text_len + strlen(suffix);
  json = (char *)malloc(json_len + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    return;
  }
  memcpy(json, prefix, strlen(prefix));
  memset(json + strlen(prefix), 'x', text_len);
  memcpy(json + strlen(prefix) + text_len, suffix, strlen(suffix) + 1u);

  for (i = 0u; i < sizeof(feed_chunks) / sizeof(feed_chunks[0]); ++i) {
    test_sse_large_event_doc doc;
    lonejson_parser parser;
    unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE +
                            LONEJSON__PARSER_WORKSPACE_SLACK];
    lonejson_status status;
    size_t offset;

    memset(&doc, 0, sizeof(doc));
    lonejson_init(test_default_runtime(), &test_sse_large_event_doc_map, &doc,
                  NULL);
    lonejson__parser_init_state(&parser, &test_sse_large_event_doc_map, &doc,
                                NULL, NULL, 0, 0, 0, 0u, workspace,
                                sizeof(workspace));
    for (offset = 0u; offset < json_len; offset += feed_chunks[i]) {
      size_t chunk = json_len - offset;
      if (chunk > feed_chunks[i]) {
        chunk = feed_chunks[i];
      }
      status = lonejson_parser_feed(&parser, json + offset, chunk);
      EXPECT(status == LONEJSON_STATUS_OK);
    }
    status = lonejson_parser_finish(&parser);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(doc.type != NULL && strcmp(doc.type, "doc") == 0);
    EXPECT(doc.text != NULL && strlen(doc.text) == text_len);
    lonejson_parser_destroy(&parser);
    lonejson_cleanup(&test_sse_large_event_doc_map, &doc, NULL);
  }

  {
    test_reader_state state;
    test_sse_large_event_doc doc;
    lonejson_error error;
    lonejson_status status;

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 13u;
    memset(&doc, 0, sizeof(doc));
    status = test_parse_reader(&test_sse_large_event_doc_map, &doc,
                               test_state_reader, &state, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(doc.type != NULL && strcmp(doc.type, "doc") == 0);
    EXPECT(doc.text != NULL && strlen(doc.text) == text_len);
    lonejson_cleanup(&test_sse_large_event_doc_map, &doc, NULL);
  }

  free(json);
}

static void test_object_framed_stream_jsonl(void) {
  test_reader_state state = {
      " \n{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}\n"
      "{\n  \"name\":\"Three\",\n  \"address\":{\"city\":\"C\",\"zip\":3},\n"
      "  \"active\":true,\n  \"lucky_numbers\":[],\n  \"tags\":[],\n"
      "  \"items\":[]\n}\n",
      0u, 7u};
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  stream = test_stream_open_reader(&test_person_map, test_framed_reader, &state,
                                   NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "One") == 0);
  lonejson_cleanup(&test_person_map, &person, NULL);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "Two") == 0);
  lonejson_cleanup(&test_person_map, &person, NULL);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "Three") == 0);
  lonejson_cleanup(&test_person_map, &person, NULL);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);
  lonejson_stream_close(stream);
}

static void test_stream_reuses_and_changes_destination(void) {
  test_reader_state state = {
      "{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Three\",\"address\":{\"city\":\"C\",\"zip\":3},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}",
      0u, 11u};
  test_person first;
  test_person second;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  poison_bytes(&first, sizeof(first), 0x5Au);
  poison_bytes(&second, sizeof(second), 0xA5u);
  stream = test_stream_open_reader(&test_person_map, test_framed_reader, &state,
                                   NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &first, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(first.name, "One") == 0);

  result = lonejson_stream_next(stream, &first, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(first.name, "Two") == 0);

  result = lonejson_stream_next(stream, &second, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(second.name, "Three") == 0);

  result = lonejson_stream_next(stream, &second, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &first, NULL);
  lonejson_cleanup(&test_person_map, &second, NULL);
}

static void test_object_stream_nonblocking_fd_would_block(void) {
  static const char json[] =
      "{\"name\":\"Fd\",\"address\":{\"city\":\"Pipe\",\"zip\":9},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;
  int pipe_fds[2];
  int flags;

  if (pipe(pipe_fds) != 0) {
    EXPECT(0);
    return;
  }
  flags = fcntl(pipe_fds[0], F_GETFL, 0);
  EXPECT(flags >= 0);
  if (flags >= 0) {
    EXPECT(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
  }

  stream = test_stream_open_fd(&test_person_map, pipe_fds[0], NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    poison_bytes(&person, sizeof(person), 0x5Au);
    result = lonejson_stream_next(stream, &person, &error);
    EXPECT(result == LONEJSON_STREAM_WOULD_BLOCK);
    EXPECT(error.code == LONEJSON_STATUS_OK);

    EXPECT(write(pipe_fds[1], json, strlen(json)) == (ssize_t)strlen(json));
    close(pipe_fds[1]);
    pipe_fds[1] = -1;

    result = lonejson_stream_next(stream, &person, &error);
    EXPECT(result == LONEJSON_STREAM_OBJECT);
    if (result == LONEJSON_STREAM_OBJECT) {
      EXPECT(strcmp(person.name, "Fd") == 0);
      EXPECT(strcmp(person.address.city, "Pipe") == 0);
      lonejson_cleanup(&test_person_map, &person, NULL);
    }
    result = lonejson_stream_next(stream, &person, &error);
    EXPECT(result == LONEJSON_STREAM_EOF);
    lonejson_stream_close(stream);
  }
  if (pipe_fds[1] >= 0) {
    close(pipe_fds[1]);
  }
  close(pipe_fds[0]);
}

static void
test_stream_reuse_fixed_capacity_string_array_frees_replaced_entries(void) {
  test_reader_state state = {
      "{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"tags\":[\"old-a\",\"old-b\"]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"tags\":[\"fresh\"]}",
      0u, 9u};
  test_person person;
  char *tag_storage[4];
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  memset(&person, 0, sizeof(person));
  memset(tag_storage, 0, sizeof(tag_storage));
  test_allocator_init(&alloc);
  lonejson_init(test_default_runtime(), &test_person_map, &person, NULL);
  person.tags.items = tag_storage;
  person.tags.capacity = 4u;
  person.tags.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;

  stream = test_stream_open_reader(&test_person_map, test_framed_reader, &state,
                                   &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_person_map, &person, NULL);
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.tags.count == 2u);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.tags.count == 1u);
  EXPECT(strcmp(person.tags.items[0], "fresh") == 0);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &person, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_stream_reuse_releases_spooled_budget_bytes(void) {
  test_reader_state state = {
      "{\"id\":\"one\",\"text\":\"abc\"}{\"id\":\"two\",\"text\":\"def\"}", 0u,
      5u};
  test_spool_doc doc;
  test_allocator_state alloc;
  lonejson__parse_options options = lonejson__default_parse_options();
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;
  unsigned char text_buf[8];
  size_t n;
  char fixed_scratch[16];

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  options.clear_destination = 0;
  options.max_alloc_bytes = 5u;
  options.allocator = &alloc.allocator;
  options.fixed_string_scratch = fixed_scratch;
  options.fixed_string_scratch_size = sizeof(fixed_scratch);

  lonejson_init(test_default_runtime(), &test_spool_doc_map, &doc, NULL);
  stream = test_stream_open_reader(&test_spool_doc_map, test_framed_reader,
                                   &state, &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_spool_doc_map, &doc, NULL);
    return;
  }

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.id, "one") == 0);
  n = read_spooled_all(&doc.text, text_buf, sizeof(text_buf));
  EXPECT(n == 3u);
  EXPECT(memcmp(text_buf, "abc", 3u) == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(doc.id, "two") == 0);
  n = read_spooled_all(&doc.text, text_buf, sizeof(text_buf));
  EXPECT(n == 3u);
  EXPECT(memcmp(text_buf, "def", 3u) == 0);

  result = lonejson_stream_next(stream, &doc, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_spool_doc_map, &doc, NULL);
  EXPECT(alloc.stats.bytes_live == 0u);
}
