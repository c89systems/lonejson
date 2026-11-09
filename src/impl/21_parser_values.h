static lonejson_status lonejson__assign_string(lonejson_parser *parser,
                                               const lonejson_field *field,
                                               void *ptr, const char *value,
                                               size_t len) {
  if (memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "field '%s' contains embedded NUL unsupported by C strings",
        field->json_key);
  }
  if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
    char **dst = (char **)ptr;
    char *copy;

    if (parser->options.max_dynamic_string_bytes != 0u &&
        len > parser->options.max_dynamic_string_bytes) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "string field '%s' exceeds configured max dynamic string bytes",
          field->json_key);
    }
    if (!lonejson__parser_alloc_can_grow(parser, 0u, len + 1u)) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "parse allocations exceed configured max bytes");
    }
    copy = (char *)lonejson__owned_malloc_parse(parser, len + 1u);
    if (copy == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to allocate string");
    }
    memcpy(copy, value, len);
    copy[len] = '\0';
    lonejson__owned_free_parse(parser, *dst);
    *dst = copy;
    return LONEJSON_STATUS_OK;
  }
  if (field->fixed_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "fixed string field has zero capacity");
  }
  if (len + 1u > field->fixed_capacity) {
    if (field->overflow_policy == LONEJSON_OVERFLOW_FAIL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "string field '%s' exceeds fixed capacity", field->json_key);
    }
    memcpy(ptr, value, field->fixed_capacity - 1u);
    ((char *)ptr)[field->fixed_capacity - 1u] = '\0';
    parser->error.truncated = 1;
    return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  memcpy(ptr, value, len);
  ((char *)ptr)[len] = '\0';
  return LONEJSON_STATUS_OK;
}

static int lonejson__parse_i64_token(const char *value, lonejson_int64 *out) {
  const unsigned char *p;
  lonejson_uint64 max_u64;
  lonejson_uint64 limit;
  lonejson_uint64 acc;
  int negative;

  if (value == NULL || out == NULL || *value == '\0') {
    return 0;
  }
  p = (const unsigned char *)value;
  negative = (*p == '-');
  if (negative) {
    p++;
    if (*p == '\0') {
      return 0;
    }
  }
  if (*p == '0' && p[1] != '\0') {
    return 0;
  }
  max_u64 = (lonejson_uint64) ~(lonejson_uint64)0u;
  limit = negative ? ((max_u64 >> 1u) + 1u) : (max_u64 >> 1u);
  acc = 0u;
  while (*p != '\0') {
    lonejson_uint64 digit;

    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (lonejson_uint64)(*p - '0');
    if (acc > (limit - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
    p++;
  }
  if (negative) {
    if (acc == ((max_u64 >> 1u) + 1u)) {
      *out = (lonejson_int64)(-((lonejson_int64)(acc - 1u)) - 1);
    } else {
      *out = (lonejson_int64)(-((lonejson_int64)acc));
    }
  } else {
    *out = (lonejson_int64)acc;
  }
  return 1;
}

static int lonejson__parse_u64_token(const char *value, lonejson_uint64 *out) {
  const unsigned char *p;
  lonejson_uint64 max_u64;
  lonejson_uint64 acc;

  if (value == NULL || out == NULL || *value == '\0' || *value == '-') {
    return 0;
  }
  p = (const unsigned char *)value;
  if (*p == '0' && p[1] != '\0') {
    return 0;
  }
  max_u64 = (lonejson_uint64) ~(lonejson_uint64)0u;
  acc = 0u;
  while (*p != '\0') {
    lonejson_uint64 digit;

    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (lonejson_uint64)(*p - '0');
    if (acc > (max_u64 - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
    p++;
  }
  *out = acc;
  return 1;
}

static LONEJSON__HOT int lonejson__parse_f64_fast(const char *value, size_t len,
                                                  double *out) {
  const unsigned char *p;
  const unsigned char *endp;
  double int_part;
  double frac_part;
  double scale;
  unsigned exponent;
  int exponent_negative;
  int negative;
  static const double powers10_small[] = {
      1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10,
      1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21,
      1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29, 1e30, 1e31, 1e32};
  static const double neg_powers10_small[] = {
      1e0,   1e-1,  1e-2,  1e-3,  1e-4,  1e-5,  1e-6,  1e-7,  1e-8,
      1e-9,  1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17,
      1e-18, 1e-19, 1e-20, 1e-21, 1e-22, 1e-23, 1e-24, 1e-25, 1e-26,
      1e-27, 1e-28, 1e-29, 1e-30, 1e-31, 1e-32};

  if (value == NULL || out == NULL || len == 0u || *value == '\0') {
    return 0;
  }
  p = (const unsigned char *)value;
  endp = p + len;
  negative = (*p == '-');
  if (negative) {
    p++;
    if (p == endp || *p == '\0') {
      return 0;
    }
  }
  if (p == endp) {
    return 0;
  }
  if (!lonejson__is_digit(*p)) {
    return 0;
  }
  if (*p == '0' && p + 1u < endp && p[1] != '.' && p[1] != 'e' && p[1] != 'E') {
    return 0;
  }
  int_part = 0.0;
  while (p < endp && lonejson__is_digit(*p)) {
    int_part = (int_part * 10.0) + (double)(*p - '0');
    p++;
  }
  frac_part = 0.0;
  if (p < endp && *p == '.') {
    unsigned frac_digits;

    p++;
    frac_digits = 0u;
    if (p == endp || !lonejson__is_digit(*p)) {
      return 0;
    }
    while (p < endp && lonejson__is_digit(*p)) {
      frac_part = (frac_part * 10.0) + (double)(*p - '0');
      frac_digits++;
      p++;
      if (frac_digits >= 17u && p < endp && lonejson__is_digit(*p)) {
        return 0;
      }
    }
    frac_part *= neg_powers10_small[frac_digits];
  }
  exponent = 0u;
  exponent_negative = 0;
  if (p < endp && (*p == 'e' || *p == 'E')) {
    p++;
    if (p == endp) {
      return 0;
    }
    if (*p == '+' || *p == '-') {
      exponent_negative = (*p == '-');
      p++;
      if (p == endp) {
        return 0;
      }
    }
    if (!lonejson__is_digit(*p)) {
      return 0;
    }
    while (p < endp && lonejson__is_digit(*p)) {
      if (exponent > 10000u) {
        return 0;
      }
      exponent = (exponent * 10u) + (unsigned)(*p - '0');
      p++;
    }
  }
  if (p != endp || *p != '\0') {
    return 0;
  }
  scale = int_part + frac_part;
  if (exponent != 0u) {
    if (exponent < sizeof(powers10_small) / sizeof(powers10_small[0])) {
      scale *= exponent_negative ? neg_powers10_small[exponent]
                                 : powers10_small[exponent];
    } else {
      static const double powers10[] = {1e1,  1e2,  1e4,   1e8,  1e16,
                                        1e32, 1e64, 1e128, 1e256};
      static const double neg_powers10[] = {1e-1,  1e-2,  1e-4,   1e-8,  1e-16,
                                            1e-32, 1e-64, 1e-128, 1e-256};
      const double *powers = exponent_negative ? neg_powers10 : powers10;
      size_t power_index = 0u;

      while (exponent != 0u &&
             power_index < sizeof(powers10) / sizeof(powers10[0])) {
        if ((exponent & 1u) != 0u) {
          scale *= powers[power_index];
        }
        exponent >>= 1u;
        power_index++;
      }
      if (exponent != 0u) {
        return 0;
      }
    }
    if (!lonejson__is_finite_f64(scale)) {
      return 0;
    }
  }
  *out = negative ? -scale : scale;
  return 1;
}

static lonejson_status lonejson__assign_i64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value) {
  lonejson_int64 parsed;

  (void)field;
  if (!lonejson__parse_i64_token(value, &parsed)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "expected integer for '%s'", field->json_key);
  }
  *(lonejson_int64 *)ptr = (lonejson_int64)parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_u64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value) {
  lonejson_uint64 parsed;

  if (!lonejson__parse_u64_token(value, &parsed)) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "expected unsigned integer for '%s'", field->json_key);
  }
  *(lonejson_uint64 *)ptr = (lonejson_uint64)parsed;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__HOT lonejson_status
lonejson__assign_f64(lonejson_parser *parser, const lonejson_field *field,
                     void *ptr, const char *value, size_t len) {
  char *end = NULL;
  double parsed;

  if (!lonejson__parse_f64_fast(value, len, &parsed)) {
    if (!lonejson__is_valid_json_number(value, len)) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected number for '%s'", field->json_key);
    }
    errno = 0;
    parsed = strtod(value, &end);
    if (errno != 0 || end == value || *end != '\0') {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected number for '%s'", field->json_key);
    }
  }
  *(double *)ptr = parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_bool(lonejson_parser *parser,
                                             const lonejson_field *field,
                                             void *ptr, int value) {
  (void)parser;
  (void)field;
  *(bool *)ptr = value ? true : false;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE int
lonejson__mark_field_seen(lonejson_parser *parser, lonejson_frame *frame,
                          const lonejson_field *field) {
  size_t idx;
  lonejson_uint64 bit;
  if (frame == NULL || frame->map == NULL || field == NULL) {
    return 1;
  }
  idx = lonejson__field_index(frame->map, field);
  if (idx == SIZE_MAX) {
    return 1;
  }
  if (frame->map->field_count <= 64u) {
    bit = ((lonejson_uint64)1u) << idx;
    if (parser->options.reject_duplicate_keys &&
        (frame->seen_mask & bit) != 0u) {
      lonejson__set_error(&parser->error, LONEJSON_STATUS_DUPLICATE_FIELD,
                          parser->error.offset, parser->error.line,
                          parser->error.column, "duplicate field '%s'",
                          field->json_key);
      parser->failed = 1;
      return 0;
    }
    if ((frame->seen_mask & bit) == 0u &&
        (field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
        frame->required_remaining != 0u) {
      frame->required_remaining--;
    }
    frame->seen_mask |= bit;
    return 1;
  }
  if (frame->seen_fields == NULL && frame->map->field_count != 0u) {
    frame->seen_fields =
        lonejson__workspace_alloc_top(parser, frame->map->field_count);
    if (frame->seen_fields == NULL) {
      lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                          parser->error.offset, parser->error.line,
                          parser->error.column,
                          "parser workspace exhausted by object bookkeeping");
      parser->failed = 1;
      return 0;
    }
    frame->seen_bytes = lonejson__workspace_align(frame->map->field_count);
    memset(frame->seen_fields, 0, frame->map->field_count);
  }
  if (parser->options.reject_duplicate_keys && frame->seen_fields[idx] != 0u) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_DUPLICATE_FIELD,
                        parser->error.offset, parser->error.line,
                        parser->error.column, "duplicate field '%s'",
                        field->json_key);
    parser->failed = 1;
    return 0;
  }
  if (frame->seen_fields[idx] == 0u &&
      (field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
      frame->required_remaining != 0u) {
    frame->required_remaining--;
  }
  frame->seen_fields[idx] = 1u;
  return 1;
}

static lonejson_status lonejson__assign_null(lonejson_parser *parser,
                                             const lonejson_field *field,
                                             void *ptr) {
  if ((field->flags & LONEJSON_FIELD_ACCEPT_NULL) != 0u &&
      lonejson__field_has_presence(field)) {
    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
      *(lonejson_int64 *)ptr = 0;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FIELD_KIND_U64:
      *(lonejson_uint64 *)ptr = 0u;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FIELD_KIND_F64:
      *(double *)ptr = 0.0;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FIELD_KIND_BOOL:
      *(bool *)ptr = false;
      return LONEJSON_STATUS_OK;
    default:
      break;
    }
  }
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    if (field->kind == LONEJSON_FIELD_KIND_STRING &&
        field->storage == LONEJSON_STORAGE_FIXED &&
        field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    } else {
      lonejson__cleanup_value_parse(parser, field, ptr);
    }
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson__cleanup_value_parse(parser, field, ptr);
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson__parser_alloc_note_release(parser,
                                        ((lonejson_json_value *)ptr)->json);
    lonejson__parser_alloc_note_release(parser,
                                        ((lonejson_json_value *)ptr)->path);
    lonejson_json_value_reset((lonejson_json_value *)ptr);
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_OBJECT:
    if (field->submap != NULL) {
      lonejson__cleanup_map_parse(parser, field->submap, ptr);
      memset(ptr, 0, field->submap->struct_size);
    }
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
  case LONEJSON_FIELD_KIND_I64_ARRAY:
  case LONEJSON_FIELD_KIND_U64_ARRAY:
  case LONEJSON_FIELD_KIND_F64_ARRAY:
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    lonejson__cleanup_value_parse(parser, field, ptr);
    return LONEJSON_STATUS_OK;
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "null is not allowed for '%s'", field->json_key);
  }
}

static lonejson_status lonejson__json_value_emit(lonejson_parser *parser,
                                                 const void *data, size_t len) {
  lonejson_json_value *value;

  value = parser->json_stream_value;
  if (value == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value stream has no destination");
  }
  if (value->parse_visitor_limits.max_total_bytes != 0u &&
      parser->json_stream_total_bytes + len >
          value->parse_visitor_limits.max_total_bytes) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value exceeds maximum total byte limit");
  }
  parser->json_stream_total_bytes += len;
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_CAPTURE) {
    size_t required;
    size_t capacity;
    size_t next_cap;
    char *next;

    required = value->len + len + 1u;
    capacity = lonejson__owned_size(value->json);
    if (capacity < required) {
      size_t available = lonejson__parser_alloc_available(
          parser, lonejson__parser_alloc_counted_bytes(parser, value->json));
      next_cap = capacity != 0u ? capacity : required;
      if (capacity == 0u && (available == SIZE_MAX || available >= 256u) &&
          next_cap < 256u) {
        next_cap = 256u;
      }
      if (available != SIZE_MAX && available < required) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
            parser->error.line, parser->error.column,
            "parse allocations exceed configured max bytes");
      }
      if (available != SIZE_MAX && next_cap > available) {
        next_cap = available;
      }
      while (next_cap < required) {
        size_t doubled = next_cap * 2u;
        if (doubled <= next_cap) {
          next_cap = required;
          break;
        }
        if (available != SIZE_MAX && doubled > available) {
          next_cap = available;
          break;
        }
        next_cap = doubled;
      }
      if (!lonejson__parser_alloc_can_grow(
              parser, lonejson__parser_alloc_counted_bytes(parser, value->json),
              next_cap)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
            parser->error.line, parser->error.column,
            "parse allocations exceed configured max bytes");
      }
      next = (char *)lonejson__owned_realloc_parse_with_allocator(
          parser, &value->allocator, value->json, next_cap);
      if (next == NULL) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to grow captured JSON value");
      }
      value->json = next;
    }
    memcpy(value->json + value->len, data, len);
    value->len += len;
    value->json[value->len] = '\0';
    value->kind = LONEJSON_JSON_VALUE_BUFFER;
    return LONEJSON_STATUS_OK;
  }
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_SINK) {
    return value->parse_sink(value->parse_sink_user, data, len, &parser->error);
  }
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_INVALID_ARGUMENT, parser->error.offset,
      parser->error.line, parser->error.column,
      "JSON value field has no configured parse sink or capture mode");
}

typedef struct lonejson__json_parse_capture_sink_state {
  lonejson_parser *parser;
  lonejson_json_value *value;
} lonejson__json_parse_capture_sink_state;

static lonejson_status lonejson__json_buffer_sink_parse(void *user,
                                                        const void *data,
                                                        size_t len,
                                                        lonejson_error *error) {
  lonejson__json_parse_capture_sink_state *state =
      (lonejson__json_parse_capture_sink_state *)user;
  size_t required;
  size_t capacity;
  size_t next_cap;
  char *next;

  (void)error;
  if (state == NULL || state->parser == NULL || state->value == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  required = state->value->len + len + 1u;
  capacity = lonejson__owned_size(state->value->json);
  if (capacity < required) {
    size_t available = lonejson__parser_alloc_available(
        state->parser, lonejson__parser_alloc_counted_bytes(
                           state->parser, state->value->json));
    next_cap = capacity != 0u ? capacity : required;
    if (capacity == 0u && (available == SIZE_MAX || available >= 256u) &&
        next_cap < 256u) {
      next_cap = 256u;
    }
    if (available != SIZE_MAX && available < required) {
      return lonejson__set_error(
          &state->parser->error, LONEJSON_STATUS_OVERFLOW,
          state->parser->error.offset, state->parser->error.line,
          state->parser->error.column,
          "parse allocations exceed configured max bytes");
    }
    if (available != SIZE_MAX && next_cap > available) {
      next_cap = available;
    }
    while (next_cap < required) {
      size_t doubled = next_cap * 2u;
      if (doubled <= next_cap) {
        next_cap = required;
        break;
      }
      if (available != SIZE_MAX && doubled > available) {
        next_cap = available;
        break;
      }
      next_cap = doubled;
    }
    if (!lonejson__parser_alloc_can_grow(state->parser,
                                         lonejson__parser_alloc_counted_bytes(
                                             state->parser, state->value->json),
                                         next_cap)) {
      return lonejson__set_error(
          &state->parser->error, LONEJSON_STATUS_OVERFLOW,
          state->parser->error.offset, state->parser->error.line,
          state->parser->error.column,
          "parse allocations exceed configured max bytes");
    }
    next = (char *)lonejson__owned_realloc_parse_with_allocator(
        state->parser, &state->value->allocator, state->value->json, next_cap);
    if (next == NULL) {
      return lonejson__set_error(
          &state->parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          state->parser->error.offset, state->parser->error.line,
          state->parser->error.column, "failed to grow captured JSON value");
    }
    state->value->json = next;
  }
  memcpy(state->value->json + state->value->len, data, len);
  state->value->len += len;
  state->value->json[state->value->len] = '\0';
  state->value->kind = LONEJSON_JSON_VALUE_BUFFER;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE int
lonejson__json_value_parse_visitor_active(const lonejson_parser *parser) {
  return parser != NULL && parser->json_stream_parse_visitor_active;
}

static LONEJSON__INLINE int
lonejson__json_value_parse_path_visitor_active(const lonejson_parser *parser) {
  return parser != NULL && parser->json_stream_path_visit_active;
}

static void lonejson__parser_cleanup_json_stream_path(lonejson_parser *parser) {
  lonejson__json_stream_path_state *path;
  size_t i;

  if (parser == NULL || parser->json_stream_path == NULL) {
    return;
  }
  path = parser->json_stream_path;
  if (path->frames != NULL) {
    for (i = 0u; i < path->capacity; ++i) {
      if (path->frames[i].key_heap) {
        lonejson__owned_free_parse(parser, path->frames[i].key);
      }
    }
  }
  lonejson__owned_free_parse(parser, path->segments);
  lonejson__owned_free_parse(parser, path->frames);
  lonejson__owned_free_parse(parser, path);
  parser->json_stream_path = NULL;
}

static LONEJSON__INLINE void
lonejson__parser_set_json_stream_value(lonejson_parser *parser,
                                       lonejson_json_value *value) {
  if (parser == NULL) {
    return;
  }
  if (parser->json_stream_path != NULL) {
    lonejson__parser_cleanup_json_stream_path(parser);
  }
  parser->json_stream_value = value;
  parser->json_stream_active = (value != NULL);
  parser->json_stream_visit_active =
      value != NULL && value->parse_mode == LONEJSON_JSON_VALUE_PARSE_VISITOR &&
      value->parse_visitor != NULL;
  parser->json_stream_path_visit_active =
      value != NULL &&
      value->parse_mode == LONEJSON_JSON_VALUE_PARSE_PATH_VISITOR &&
      value->parse_path_visitor != NULL;
  parser->json_stream_parse_visitor_active =
      parser->json_stream_visit_active || parser->json_stream_path_visit_active;
  parser->json_stream_sink_active =
      value != NULL && value->parse_mode == LONEJSON_JSON_VALUE_PARSE_SINK;
}

static LONEJSON__INLINE void
lonejson__parser_clear_json_stream_value(lonejson_parser *parser) {
  if (parser == NULL) {
    return;
  }
  if (parser->json_stream_path != NULL) {
    lonejson__parser_cleanup_json_stream_path(parser);
  }
  parser->json_stream_value = NULL;
  parser->json_stream_active = 0;
  parser->json_stream_parse_visitor_active = 0;
  parser->json_stream_visit_active = 0;
  parser->json_stream_path_visit_active = 0;
  parser->json_stream_sink_active = 0;
}

static lonejson_status
lonejson__json_value_path_ensure(lonejson_parser *parser) {
  lonejson_json_value *value;
  size_t capacity;

  if (!lonejson__json_value_parse_path_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->json_stream_path != NULL &&
      parser->json_stream_path->segments != NULL &&
      parser->json_stream_path->frames != NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->json_stream_path == NULL) {
    parser->json_stream_path =
        (lonejson__json_stream_path_state *)lonejson__owned_malloc_parse(
            parser, sizeof(*parser->json_stream_path));
    if (parser->json_stream_path == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to allocate JSON path visitor state");
    }
    memset(parser->json_stream_path, 0, sizeof(*parser->json_stream_path));
  }
  value = parser->json_stream_value;
  capacity = value->parse_visitor_limits.max_depth;
  if (capacity == 0u) {
    capacity = parser->options.max_depth;
  }
  if (capacity == 0u) {
    capacity = parser->workspace_size / sizeof(lonejson_frame);
  }
  if (capacity > ((size_t)-1) - 1u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON path visitor depth exceeds size limit");
  }
  ++capacity;
  parser->json_stream_path->segments =
      (lonejson_path_segment *)lonejson__owned_malloc_parse(
          parser, capacity * sizeof(*parser->json_stream_path->segments));
  parser->json_stream_path->frames =
      (lonejson__json_path_frame *)lonejson__owned_malloc_parse(
          parser, capacity * sizeof(*parser->json_stream_path->frames));
  if (parser->json_stream_path->segments == NULL ||
      parser->json_stream_path->frames == NULL) {
    lonejson__parser_cleanup_json_stream_path(parser);
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED, parser->error.offset,
        parser->error.line, parser->error.column,
        "failed to allocate JSON path visitor state");
  }
  memset(parser->json_stream_path->segments, 0,
         capacity * sizeof(*parser->json_stream_path->segments));
  memset(parser->json_stream_path->frames, 0,
         capacity * sizeof(*parser->json_stream_path->frames));
  parser->json_stream_path->capacity = capacity;
  parser->json_stream_path->depth = 0u;
  return LONEJSON_STATUS_OK;
}

static const lonejson_value_path *
lonejson__json_value_current_path(lonejson_parser *parser,
                                  lonejson_value_path *path) {
  path->segments = parser->json_stream_path->segments;
  path->segment_count = parser->json_stream_path->depth;
  return path;
}

static lonejson_status
lonejson__json_value_path_event(lonejson_parser *parser,
                                lonejson_path_value_event_fn fn) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_value_path path;

  if (!lonejson__json_value_parse_path_visitor_active(parser) || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(value->parse_path_visitor_user,
            lonejson__json_value_current_path(parser, &path), &parser->error);
}

static lonejson_status
lonejson__json_value_visit_event(lonejson_parser *parser,
                                 lonejson_value_event_fn fn) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!parser->json_stream_visit_active || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(value->parse_visitor_user, &parser->error);
}

static lonejson_status
lonejson__json_value_visit_any_event(lonejson_parser *parser,
                                     lonejson_value_event_fn fn,
                                     lonejson_path_value_event_fn path_fn) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (parser->json_stream_visit_active && fn != NULL) {
    status = fn(value->parse_visitor_user, &parser->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (!parser->json_stream_path_visit_active) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__json_value_path_event(parser, path_fn);
}

static lonejson_status
lonejson__json_value_visit_chunk(lonejson_parser *parser,
                                 lonejson_value_chunk_fn fn, const char *data,
                                 size_t len, size_t *token_bytes,
                                 size_t token_limit, const char *limit_msg) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!parser->json_stream_visit_active || len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (token_limit != 0u && token_bytes != NULL &&
      *token_bytes + len > token_limit) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "%s", limit_msg);
  }
  if (value->parse_visitor_limits.max_total_bytes != 0u &&
      parser->json_stream_total_bytes + len >
          value->parse_visitor_limits.max_total_bytes) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value exceeds maximum total byte limit");
  }
  if (token_bytes != NULL) {
    *token_bytes += len;
  }
  parser->json_stream_total_bytes += len;
  if (fn != NULL) {
    return fn(value->parse_visitor_user, data, len, &parser->error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_value_path_chunk(lonejson_parser *parser,
                                lonejson_path_value_chunk_fn fn,
                                const char *data, size_t len) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_value_path path;

  if (!lonejson__json_value_parse_path_visitor_active(parser) || len == 0u ||
      fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(value->parse_path_visitor_user,
            lonejson__json_value_current_path(parser, &path), data, len,
            &parser->error);
}

static lonejson_status
lonejson__json_value_path_key_append(lonejson_parser *parser, const char *data,
                                     size_t len) {
  lonejson__json_path_frame *frame;
  size_t new_len;
  size_t new_cap;
  char *next;

  if (!lonejson__json_value_parse_path_visitor_active(parser) || len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->json_stream_path->depth >= parser->json_stream_path->capacity) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid JSON path stack state");
  }
  frame = &parser->json_stream_path->frames[parser->json_stream_path->depth];
  if (frame->key == NULL) {
    frame->key = frame->inline_key;
    frame->key_cap = sizeof(frame->inline_key);
  }
  if (len > ((size_t)-1) - frame->key_len - 1u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON object key exceeds maximum decoded byte "
                               "limit");
  }
  new_len = frame->key_len + len;
  if (new_len + 1u > frame->key_cap) {
    new_cap = frame->key_cap == 0u ? sizeof(frame->inline_key) : frame->key_cap;
    while (new_cap < new_len + 1u) {
      if (new_cap > ((size_t)-1) / 2u) {
        new_cap = new_len + 1u;
        break;
      }
      new_cap *= 2u;
    }
    next = (char *)lonejson__owned_malloc_parse(parser, new_cap);
    if (next == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to allocate JSON path key buffer");
    }
    if (frame->key_len != 0u) {
      memcpy(next, frame->key, frame->key_len);
    }
    if (frame->key_heap) {
      lonejson__owned_free_parse(parser, frame->key);
    }
    frame->key = next;
    frame->key_cap = new_cap;
    frame->key_heap = 1;
  }
  memcpy(frame->key + frame->key_len, data, len);
  frame->key_len = new_len;
  frame->key[frame->key_len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__json_value_visit_any_chunk(
    lonejson_parser *parser, lonejson_value_chunk_fn fn,
    lonejson_path_value_chunk_fn path_fn, const char *data, size_t len,
    size_t *token_bytes, size_t token_limit, const char *limit_msg) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!lonejson__json_value_parse_visitor_active(parser) || len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (token_limit != 0u && token_bytes != NULL &&
      *token_bytes + len > token_limit) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "%s", limit_msg);
  }
  if (value->parse_visitor_limits.max_total_bytes != 0u &&
      parser->json_stream_total_bytes + len >
          value->parse_visitor_limits.max_total_bytes) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value exceeds maximum total byte limit");
  }
  if (token_bytes != NULL) {
    *token_bytes += len;
  }
  parser->json_stream_total_bytes += len;
  if (parser->json_stream_path_visit_active &&
      parser->json_stream_text_is_key) {
    status = lonejson__json_value_path_key_append(parser, data, len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (parser->json_stream_visit_active && fn != NULL) {
    status = fn(value->parse_visitor_user, data, len, &parser->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (!parser->json_stream_path_visit_active) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__json_value_path_chunk(parser, path_fn, data, len);
}

static lonejson_status
lonejson__json_value_path_begin_value(lonejson_parser *parser) {
  lonejson__json_path_frame *frame;
  size_t n;

  if (!lonejson__json_value_parse_path_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  {
    lonejson_status status = lonejson__json_value_path_ensure(parser);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  if (parser->json_stream_path->depth >= parser->json_stream_path->capacity) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid JSON path stack state");
  }
  frame = &parser->json_stream_path->frames[parser->json_stream_path->depth];
  if (frame->container_kind != LONEJSON_CONTAINER_ARRAY) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->json_stream_path->depth + 1u >=
      parser->json_stream_path->capacity) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid JSON path stack state");
  }
  n = lonejson__format_size_decimal(
      frame->index_text, sizeof(frame->index_text), frame->next_index);
  if (n >= sizeof(frame->index_text)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON array index path segment is too large");
  }
  parser->json_stream_path->segments[parser->json_stream_path->depth].data =
      frame->index_text;
  parser->json_stream_path->segments[parser->json_stream_path->depth].len = n;
  ++parser->json_stream_path->depth;
  return LONEJSON_STATUS_OK;
}

static void lonejson__json_value_path_complete_value(lonejson_parser *parser) {
  lonejson__json_path_frame *frame;

  if (!lonejson__json_value_parse_path_visitor_active(parser)) {
    return;
  }
  if (parser->json_stream_path->depth != 0u) {
    --parser->json_stream_path->depth;
  }
  if (parser->json_stream_path->depth < parser->json_stream_path->capacity) {
    frame = &parser->json_stream_path->frames[parser->json_stream_path->depth];
    if (frame->container_kind == LONEJSON_CONTAINER_ARRAY) {
      ++frame->next_index;
    }
  }
}

static lonejson_status
lonejson__json_value_path_push_object_key(lonejson_parser *parser) {
  lonejson__json_path_frame *frame;

  if (!lonejson__json_value_parse_path_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  {
    lonejson_status status = lonejson__json_value_path_ensure(parser);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  if (parser->json_stream_path->depth + 1u >=
      parser->json_stream_path->capacity) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid JSON path stack state");
  }
  frame = &parser->json_stream_path->frames[parser->json_stream_path->depth];
  parser->json_stream_path->segments[parser->json_stream_path->depth].data =
      frame->key;
  parser->json_stream_path->segments[parser->json_stream_path->depth].len =
      frame->key_len;
  ++parser->json_stream_path->depth;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_value_path_prepare_key(lonejson_parser *parser) {
  lonejson__json_path_frame *frame;

  if (!lonejson__json_value_parse_path_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  {
    lonejson_status status = lonejson__json_value_path_ensure(parser);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  if (parser->json_stream_path->depth >= parser->json_stream_path->capacity) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid JSON path stack state");
  }
  frame = &parser->json_stream_path->frames[parser->json_stream_path->depth];
  frame->key_len = 0u;
  if (frame->key == NULL) {
    frame->key = frame->inline_key;
    frame->key_cap = sizeof(frame->inline_key);
    frame->key_heap = 0;
  }
  frame->key[0] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_value_string_begin_visitor(lonejson_parser *parser, int is_key) {
  lonejson_json_value *value = parser->json_stream_value;

  if (parser->json_stream_path_visit_active) {
    if (is_key) {
      lonejson_status status = lonejson__json_value_path_prepare_key(parser);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    } else {
      lonejson_status status = lonejson__json_value_path_begin_value(parser);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
  }
  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser, value->parse_visitor
                    ? (is_key ? value->parse_visitor->object_key_begin
                              : value->parse_visitor->string_begin)
                    : NULL);
  }
  return lonejson__json_value_visit_any_event(
      parser,
      value->parse_visitor ? (is_key ? value->parse_visitor->object_key_begin
                                     : value->parse_visitor->string_begin)
                           : NULL,
      value->parse_path_visitor
          ? (is_key ? value->parse_path_visitor->object_key_begin
                    : value->parse_path_visitor->string_begin)
          : NULL);
}

static lonejson_status
lonejson__json_value_string_chunk_visitor(lonejson_parser *parser,
                                          const char *data, size_t len,
                                          size_t limit, const char *msg) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_chunk(
        parser,
        value->parse_visitor ? (parser->json_stream_text_is_key
                                    ? value->parse_visitor->object_key_chunk
                                    : value->parse_visitor->string_chunk)
                             : NULL,
        data, len, &parser->json_stream_text_bytes, limit, msg);
  }
  return lonejson__json_value_visit_any_chunk(
      parser,
      value->parse_visitor ? (parser->json_stream_text_is_key
                                  ? value->parse_visitor->object_key_chunk
                                  : value->parse_visitor->string_chunk)
                           : NULL,
      value->parse_path_visitor
          ? (parser->json_stream_text_is_key
                 ? value->parse_path_visitor->object_key_chunk
                 : value->parse_path_visitor->string_chunk)
          : NULL,
      data, len, &parser->json_stream_text_bytes, limit, msg);
}

static lonejson_status
lonejson__json_value_string_end_visitor(lonejson_parser *parser, int is_key) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser, value->parse_visitor
                    ? (is_key ? value->parse_visitor->object_key_end
                              : value->parse_visitor->string_end)
                    : NULL);
  }
  status = lonejson__json_value_visit_any_event(
      parser,
      value->parse_visitor ? (is_key ? value->parse_visitor->object_key_end
                                     : value->parse_visitor->string_end)
                           : NULL,
      value->parse_path_visitor
          ? (is_key ? value->parse_path_visitor->object_key_end
                    : value->parse_path_visitor->string_end)
          : NULL);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (parser->json_stream_path_visit_active && is_key) {
    return lonejson__json_value_path_push_object_key(parser);
  }
  if (parser->json_stream_path_visit_active) {
    lonejson__json_value_path_complete_value(parser);
  }
  return status;
}

static lonejson_status
lonejson__json_value_object_begin(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser,
        value->parse_visitor ? value->parse_visitor->object_begin : NULL);
  }
  status = lonejson__json_value_path_begin_value(parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->object_begin : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->object_begin
                                : NULL);
  if ((status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
      lonejson__json_value_parse_path_visitor_active(parser) &&
      parser->json_stream_path->depth < parser->json_stream_path->capacity) {
    parser->json_stream_path->frames[parser->json_stream_path->depth]
        .container_kind = LONEJSON_CONTAINER_OBJECT;
  }
  return status;
}

static lonejson_status
lonejson__json_value_object_end(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser, value->parse_visitor ? value->parse_visitor->object_end : NULL);
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->object_end : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->object_end : NULL);
  if ((status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
      lonejson__json_value_parse_path_visitor_active(parser) &&
      parser->json_stream_path->depth < parser->json_stream_path->capacity) {
    parser->json_stream_path->frames[parser->json_stream_path->depth]
        .container_kind = 0;
    lonejson__json_value_path_complete_value(parser);
  }
  return status;
}

static lonejson_status
lonejson__json_value_array_begin(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser,
        value->parse_visitor ? value->parse_visitor->array_begin : NULL);
  }
  status = lonejson__json_value_path_begin_value(parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->array_begin : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->array_begin
                                : NULL);
  if ((status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
      lonejson__json_value_parse_path_visitor_active(parser) &&
      parser->json_stream_path->depth < parser->json_stream_path->capacity) {
    parser->json_stream_path->frames[parser->json_stream_path->depth]
        .container_kind = LONEJSON_CONTAINER_ARRAY;
    parser->json_stream_path->frames[parser->json_stream_path->depth]
        .next_index = 0u;
  }
  return status;
}

static lonejson_status lonejson__json_value_array_end(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser, value->parse_visitor ? value->parse_visitor->array_end : NULL);
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->array_end : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->array_end : NULL);
  if ((status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
      lonejson__json_value_parse_path_visitor_active(parser) &&
      parser->json_stream_path->depth < parser->json_stream_path->capacity) {
    parser->json_stream_path->frames[parser->json_stream_path->depth]
        .container_kind = 0;
    lonejson__json_value_path_complete_value(parser);
  }
  return status;
}

static lonejson_status lonejson__json_value_visit_bool(lonejson_parser *parser,
                                                       int boolean_value) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  if (!parser->json_stream_path_visit_active) {
    if (value->parse_visitor->boolean_value == NULL) {
      return LONEJSON_STATUS_OK;
    }
    return value->parse_visitor->boolean_value(value->parse_visitor_user,
                                               boolean_value, &parser->error);
  }
  status = lonejson__json_value_path_begin_value(parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = LONEJSON_STATUS_OK;
  if (parser->json_stream_visit_active &&
      value->parse_visitor->boolean_value != NULL) {
    status = value->parse_visitor->boolean_value(value->parse_visitor_user,
                                                 boolean_value, &parser->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (parser->json_stream_path_visit_active &&
      value->parse_path_visitor->boolean_value != NULL) {
    lonejson_value_path path;
    status = value->parse_path_visitor->boolean_value(
        value->parse_path_visitor_user,
        lonejson__json_value_current_path(parser, &path), boolean_value,
        &parser->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  lonejson__json_value_path_complete_value(parser);
  return status;
}

static lonejson_status
lonejson__json_value_string_begin(lonejson_parser *parser, int is_key) {
  lonejson_json_value *value = parser->json_stream_value;

  if (value == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value stream has no destination");
  }
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = is_key;
  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return lonejson__json_value_emit(parser, "\"", 1u);
  }
  (void)value;
  return lonejson__json_value_string_begin_visitor(parser, is_key);
}

static lonejson_status
lonejson__json_value_string_chunk(lonejson_parser *parser, const char *data,
                                  size_t len) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson__json_parse_capture_sink_state capture_state;
  size_t limit;
  const char *msg;

  limit = parser->json_stream_text_is_key
              ? value->parse_visitor_limits.max_key_bytes
              : value->parse_visitor_limits.max_string_bytes;
  msg = parser->json_stream_text_is_key
            ? "JSON object key exceeds maximum decoded byte limit"
            : "JSON string exceeds maximum decoded byte limit";

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    if (limit != 0u && parser->json_stream_text_bytes + len > limit) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "%s", msg);
    }
    parser->json_stream_text_bytes += len;
    capture_state.parser = parser;
    capture_state.value = value;
    return lonejson__emit_escaped_fragment(
        parser->json_stream_sink_active ? value->parse_sink
                                        : lonejson__json_buffer_sink_parse,
        parser->json_stream_sink_active ? value->parse_sink_user
                                        : &capture_state,
        &parser->error, (const unsigned char *)data, len);
  }
  return lonejson__json_value_string_chunk_visitor(parser, data, len, limit,
                                                   msg);
}

static lonejson_status lonejson__json_value_emit_string(lonejson_parser *parser,
                                                        const char *value,
                                                        size_t len) {
  lonejson_status status;
  lonejson__json_parse_capture_sink_state capture_state;
  lonejson_json_value *json_value = parser->json_stream_value;

  if (json_value != NULL &&
      json_value->parse_visitor_limits.max_string_bytes != 0u &&
      len > json_value->parse_visitor_limits.max_string_bytes) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
        parser->error.line, parser->error.column,
        "JSON string exceeds maximum decoded byte limit");
  }

  status = lonejson__json_value_emit(parser, "\"", 1u);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  capture_state.parser = parser;
  capture_state.value = parser->json_stream_value;
  status = lonejson__emit_escaped_fragment(
      parser->json_stream_sink_active ? parser->json_stream_value->parse_sink
                                      : lonejson__json_buffer_sink_parse,
      parser->json_stream_sink_active
          ? parser->json_stream_value->parse_sink_user
          : &capture_state,
      &parser->error, (const unsigned char *)value, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__json_value_emit(parser, "\"", 1u);
}

static lonejson_status
lonejson__json_value_string_end(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  int is_key = parser->json_stream_text_is_key;

  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = 0;
  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return lonejson__json_value_emit(parser, "\"", 1u);
  }
  (void)value;
  return lonejson__json_value_string_end_visitor(parser, is_key);
}

static lonejson_status lonejson__json_value_number(lonejson_parser *parser,
                                                   const char *text,
                                                   size_t len) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;
  size_t count = 0u;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  if (!parser->json_stream_path_visit_active) {
    status = lonejson__json_value_visit_event(
        parser,
        value->parse_visitor ? value->parse_visitor->number_begin : NULL);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__json_value_visit_chunk(
        parser,
        value->parse_visitor ? value->parse_visitor->number_chunk : NULL, text,
        len, &count, value->parse_visitor_limits.max_number_bytes,
        "JSON number exceeds maximum byte limit");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    return lonejson__json_value_visit_event(
        parser, value->parse_visitor ? value->parse_visitor->number_end : NULL);
  }
  status = lonejson__json_value_path_begin_value(parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->number_begin : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->number_begin
                                : NULL);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_any_chunk(
      parser, value->parse_visitor ? value->parse_visitor->number_chunk : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->number_chunk
                                : NULL,
      text, len, &count, value->parse_visitor_limits.max_number_bytes,
      "JSON number exceeds maximum byte limit");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->number_end : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->number_end : NULL);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    lonejson__json_value_path_complete_value(parser);
  }
  return status;
}

static lonejson_status lonejson__json_value_null(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;

  if (!parser->json_stream_path_visit_active) {
    return lonejson__json_value_visit_event(
        parser, value->parse_visitor ? value->parse_visitor->null_value : NULL);
  }
  status = lonejson__json_value_path_begin_value(parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_any_event(
      parser, value->parse_visitor ? value->parse_visitor->null_value : NULL,
      value->parse_path_visitor ? value->parse_path_visitor->null_value : NULL);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    lonejson__json_value_path_complete_value(parser);
  }
  return status;
}

static lonejson_status lonejson__assign_json_scalar(lonejson_parser *parser,
                                                    lonejson_json_value *value,
                                                    const char *text,
                                                    size_t len,
                                                    lonejson_lex_mode mode) {
  lonejson_status status;

  status = lonejson__json_value_prepare_parse(parser, value, &parser->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__parser_set_json_stream_value(parser, value);
  parser->json_stream_depth = 0u;
  parser->json_stream_total_bytes = 0u;
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = 0;
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_VISITOR ||
      value->parse_mode == LONEJSON_JSON_VALUE_PARSE_PATH_VISITOR) {
    status = lonejson__json_value_path_ensure(parser);
    if (status != LONEJSON_STATUS_OK) {
      lonejson__parser_clear_json_stream_value(parser);
      parser->json_stream_depth = 0u;
      return status;
    }
    switch (mode) {
    case LONEJSON_LEX_STRING:
      status = lonejson__json_value_string_begin(parser, 0);
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__json_value_string_chunk(parser, text, len);
      }
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__json_value_string_end(parser);
      }
      break;
    case LONEJSON_LEX_NUMBER:
      status = lonejson__json_value_number(parser, text, len);
      break;
    case LONEJSON_LEX_TRUE:
      status = lonejson__json_value_visit_bool(parser, 1);
      break;
    case LONEJSON_LEX_FALSE:
      status = lonejson__json_value_visit_bool(parser, 0);
      break;
    case LONEJSON_LEX_NULL:
      status = lonejson__json_value_null(parser);
      break;
    default:
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
          parser->error.line, parser->error.column,
          "unexpected JSON scalar mode");
      break;
    }
    lonejson__parser_clear_json_stream_value(parser);
    parser->json_stream_depth = 0u;
    return status;
  }
  switch (mode) {
  case LONEJSON_LEX_STRING:
    status = lonejson__json_value_emit_string(parser, text, len);
    break;
  case LONEJSON_LEX_NUMBER:
  case LONEJSON_LEX_TRUE:
  case LONEJSON_LEX_FALSE:
    status = lonejson__json_value_emit(parser, text, len);
    break;
  case LONEJSON_LEX_NULL:
    status = lonejson__json_value_emit(parser, "null", 4u);
    break;
  default:
    status = lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "unexpected JSON scalar mode");
    break;
  }
  lonejson__parser_clear_json_stream_value(parser);
  return status;
}

static lonejson_status lonejson__array_append_string(
    lonejson_parser *parser, const lonejson_field *field,
    lonejson_string_array *arr, const char *value, size_t len) {
  char *copy;

  if (memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "array '%s' contains embedded NUL unsupported by C strings",
        field->json_key);
  }
  {
    lonejson__array_ensure_result ensure = lonejson__array_ensure_bytes(
        parser, (void **)&arr->items, &arr->capacity, sizeof(char *),
        &arr->flags, arr->count + 1u, field->overflow_policy, &parser->error);
    if (ensure == LONEJSON__ARRAY_ENSURE_ERROR) {
      return parser->error.code;
    }
    if (ensure == LONEJSON__ARRAY_ENSURE_TRUNCATED) {
      return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
                 ? LONEJSON_STATUS_TRUNCATED
                 : LONEJSON_STATUS_OK;
    }
  }
  if (arr->count >= arr->capacity) {
    parser->error.truncated = 1;
    return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  if (parser->options.max_dynamic_string_bytes != 0u &&
      len > parser->options.max_dynamic_string_bytes) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "array '%s' contains a string exceeding "
                               "configured max dynamic string bytes",
                               field->json_key);
  }
  if (!lonejson__parser_alloc_can_grow(parser, 0u, len + 1u)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parse allocations exceed configured max bytes");
  }
  copy = (char *)lonejson__owned_malloc_parse(parser, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED, parser->error.offset,
        parser->error.line, parser->error.column,
        "failed to allocate array string");
  }
  memcpy(copy, value, len);
  copy[len] = '\0';
  arr->items[arr->count++] = copy;
  return LONEJSON_STATUS_OK;
}

#define LONEJSON__DEFINE_ARRAY_APPEND(name, array_type, elem_type)             \
  static lonejson_status name(lonejson_parser *parser,                         \
                              const lonejson_field *field, array_type *arr,    \
                              elem_type value) {                               \
    {                                                                          \
      lonejson__array_ensure_result ensure = lonejson__array_ensure_bytes(     \
          parser, (void **)&arr->items, &arr->capacity, sizeof(elem_type),     \
          &arr->flags, arr->count + 1u, field->overflow_policy,                \
          &parser->error);                                                     \
      if (ensure == LONEJSON__ARRAY_ENSURE_ERROR) {                            \
        return parser->error.code;                                             \
      }                                                                        \
      if (ensure == LONEJSON__ARRAY_ENSURE_TRUNCATED) {                        \
        return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)          \
                   ? LONEJSON_STATUS_TRUNCATED                                 \
                   : LONEJSON_STATUS_OK;                                       \
      }                                                                        \
    }                                                                          \
    if (arr->count >= arr->capacity) {                                         \
      parser->error.truncated = 1;                                             \
      return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)            \
                 ? LONEJSON_STATUS_TRUNCATED                                   \
                 : LONEJSON_STATUS_OK;                                         \
    }                                                                          \
    arr->items[arr->count++] = value;                                          \
    return LONEJSON_STATUS_OK;                                                 \
  }

LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_i64, lonejson_i64_array,
                              lonejson_int64)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_u64, lonejson_u64_array,
                              lonejson_uint64)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_f64, lonejson_f64_array,
                              double)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_bool, lonejson_bool_array,
                              bool)

static void *lonejson__object_array_append_slot(lonejson_parser *parser,
                                                const lonejson_field *field,
                                                lonejson_object_array *arr) {
  void *slot;

  if (arr->elem_size == 0u) {
    arr->elem_size = field->elem_size;
  }
  {
    lonejson__array_ensure_result ensure = lonejson__array_ensure_bytes(
        parser, &arr->items, &arr->capacity, arr->elem_size, &arr->flags,
        arr->count + 1u, field->overflow_policy, &parser->error);
    if (ensure == LONEJSON__ARRAY_ENSURE_ERROR) {
      return NULL;
    }
    if (ensure == LONEJSON__ARRAY_ENSURE_TRUNCATED) {
      parser->error.truncated = 1;
      return NULL;
    }
  }
  if (arr->count >= arr->capacity) {
    parser->error.truncated = 1;
    return NULL;
  }
  slot = (unsigned char *)arr->items + (arr->count * arr->elem_size);
  memset(slot, 0, arr->elem_size);
  if (field->submap != NULL) {
    if (!lonejson__init_map_parse(parser, field->submap, slot))
      return NULL;
  }
  arr->count++;
  return slot;
}
