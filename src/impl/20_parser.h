lonejson_status
lonejson_json_value_write_to_sink(const lonejson_json_value *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  if (sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "sink is required");
  }
  return lonejson__json_transcode(value, sink, user, error, 0, 0u);
}

static LONEJSON__INLINE int lonejson__scratch_reserve(lonejson_scratch *scratch,
                                                      size_t need) {
  if (need + 1u <= scratch->cap) {
    return 1;
  }
  return 0;
}

static LONEJSON__INLINE int lonejson__scratch_append(lonejson_scratch *scratch,
                                                     char ch) {
  if (!lonejson__scratch_reserve(scratch, scratch->len + 2u)) {
    return 0;
  }
  scratch->data[scratch->len++] = ch;
  return 1;
}

static LONEJSON__INLINE int
lonejson__scratch_append_bytes(lonejson_scratch *scratch,
                               const unsigned char *src, size_t len) {
  if (!lonejson__scratch_reserve(scratch, scratch->len + len + 1u)) {
    return 0;
  }
  if (len == 1u)
    scratch->data[scratch->len] = (char)*src;
  else
    memcpy(scratch->data + scratch->len, src, len);
  scratch->len += len;
  return 1;
}

static int lonejson__is_valid_json_number(const char *value, size_t len);
static int lonejson__is_json_space(int ch);
static int lonejson__is_digit(int ch);
static int lonejson__is_finite_f64(double value);
static int lonejson__is_exact_fixed_capacity(unsigned flags);
static size_t lonejson__frame_bytes(size_t frame_count);
static size_t lonejson__workspace_align(size_t size);
static unsigned char *lonejson__align_pointer(unsigned char *ptr,
                                              size_t alignment);
static unsigned char *lonejson__token_base(lonejson_parser *parser);
static size_t lonejson__token_limit(lonejson_parser *parser);
static unsigned char *lonejson__workspace_alloc_top(lonejson_parser *parser,
                                                    size_t size);
static int lonejson__mark_field_seen(lonejson_parser *parser,
                                     lonejson_frame *frame,
                                     const lonejson_field *field);
static lonejson_status
lonejson__complete_parent_after_value(lonejson_parser *parser);
static lonejson_status
lonejson__complete_streamed_string_token(lonejson_parser *parser);
static LONEJSON__INLINE void *lonejson__field_ptr(void *base,
                                                  const lonejson_field *field);
static lonejson_status lonejson__stream_value_begin(lonejson_parser *parser,
                                                    const lonejson_field *field,
                                                    void *ptr);
static void lonejson__parser_init_state(
    lonejson_parser *parser, const lonejson_map *map, void *dst,
    const lonejson__parse_options *options, const lonejson_runtime *runtime,
    int validate_only, int root_map_analysis_known, int root_map_may_allocate,
    lonejson_uint64 root_map_adopt_mask, unsigned char *workspace,
    size_t workspace_size);
#define lonejson__parser_peak_workspace_used(parser_ptr)                       \
  ((parser_ptr) != NULL ? (parser_ptr)->workspace_peak : 0u)
static LONEJSON__HOT lonejson_status
lonejson__parser_feed_bytes(lonejson_parser *parser, const unsigned char *bytes,
                            size_t len, size_t *consumed, int stop_at_root);

static void lonejson__scratch_reset(lonejson_scratch *scratch) {
  scratch->len = 0;
}

static const char *lonejson__scratch_cstr(lonejson_scratch *scratch) {
  if (scratch == NULL || scratch->data == NULL) {
    return "";
  }
  scratch->data[scratch->len] = '\0';
  return scratch->data;
}

static LONEJSON__INLINE int lonejson__is_json_space(int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static LONEJSON__INLINE int lonejson__is_digit(int ch) {
  return ch >= '0' && ch <= '9';
}

static int lonejson__is_finite_f64(double value) {
  return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}

static int lonejson__is_exact_fixed_capacity(unsigned flags) {
  return flags == LONEJSON_ARRAY_FIXED_CAPACITY;
}

static size_t lonejson__frame_bytes(size_t frame_count) {
  return frame_count * sizeof(lonejson_frame);
}

static LONEJSON__INLINE unsigned char *
lonejson__align_pointer(unsigned char *ptr, size_t alignment) {
  uintptr_t value = (uintptr_t)ptr;
  uintptr_t mask = (uintptr_t)(alignment - 1u);

  if (alignment == 0u) {
    return ptr;
  }
  value = (value + mask) & ~mask;
  return (unsigned char *)value;
}

static LONEJSON__INLINE unsigned char *
lonejson__token_base(lonejson_parser *parser) {
  return parser->workspace + lonejson__frame_bytes(parser->frame_count);
}

static LONEJSON__INLINE size_t lonejson__token_limit(lonejson_parser *parser) {
  size_t frame_bytes = lonejson__frame_bytes(parser->frame_count);

  if (parser->workspace_top <= frame_bytes) {
    return 0u;
  }
  return parser->workspace_top - frame_bytes - 1u;
}

static LONEJSON__INLINE void
lonejson__parser_note_workspace_usage(lonejson_parser *parser) {
#if LONEJSON_TRACK_WORKSPACE_USAGE
  size_t frame_bytes;
  size_t reserved_top_bytes;
  size_t token_bytes;
  size_t used;

  frame_bytes = lonejson__frame_bytes(parser->frame_count);
  reserved_top_bytes = parser->workspace_size - parser->workspace_top;
  token_bytes = 0u;
  if (parser->lex_mode != LONEJSON_LEX_NONE || parser->token.len != 0u) {
    token_bytes = parser->token.len + 1u;
  }
  used = frame_bytes + reserved_top_bytes + token_bytes;
  if (used > parser->workspace_peak) {
    parser->workspace_peak = used;
  }
#else
  (void)parser;
#endif
}

static LONEJSON__INLINE unsigned char *
lonejson__workspace_alloc_top(lonejson_parser *parser, size_t size) {
  size_t frame_bytes = lonejson__frame_bytes(parser->frame_count);
  size_t aligned_size = lonejson__workspace_align(size);

  if (aligned_size > parser->workspace_top ||
      parser->workspace_top - aligned_size < frame_bytes) {
    return NULL;
  }
  parser->workspace_top -= aligned_size;
  return parser->workspace + parser->workspace_top;
}

static LONEJSON__INLINE size_t lonejson__workspace_align(size_t size) {
  return (size + (sizeof(void *) - 1u)) & ~(sizeof(void *) - 1u);
}

static LONEJSON__INLINE int lonejson__prepare_token(lonejson_parser *parser) {
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  if (parser->token.cap == 0u) {
    parser->token.len = 0u;
    return 0;
  }
  lonejson__scratch_reset(&parser->token);
  return 1;
}

static int lonejson__field_is_streamed(const lonejson_field *field) {
  return field != NULL &&
         (field->kind == LONEJSON_FIELD_KIND_STRING_STREAM ||
          field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM ||
          field->kind == LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM);
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_string_lex(lonejson_parser *parser, int is_key) {
  parser->lex_mode = LONEJSON_LEX_STRING;
  parser->lex_is_key = is_key;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;

  if (LONEJSON__LIKELY(!parser->json_stream_active)) {
    if (parser->validate_only) {
      parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DISCARD;
      parser->token.data = NULL;
      parser->token.cap = 0u;
      parser->token.len = 0u;
      return LONEJSON_STATUS_OK;
    }
    return lonejson__prepare_token(parser)
               ? LONEJSON_STATUS_OK
               : lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
  }

  {
    lonejson_status status = lonejson__json_value_string_begin(parser, is_key);
    if (status != LONEJSON_STATUS_OK) {
      parser->json_stream_text_bytes = 0u;
      parser->json_stream_text_is_key = 0;
      return status;
    }
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_JSON_VISITOR;
    parser->token.data = NULL;
    parser->token.cap = 0u;
    parser->token.len = 0u;
    return LONEJSON_STATUS_OK;
  }
}

static LONEJSON__INLINE lonejson_status lonejson__begin_string_value_lex(
    lonejson_parser *parser, const lonejson_field *field, void *ptr) {
  lonejson_status status;

  if (field != NULL && field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
    status = lonejson__json_value_prepare_parse(
        parser, (lonejson_json_value *)ptr, &parser->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    lonejson__parser_set_json_stream_value(parser, (lonejson_json_value *)ptr);
    parser->json_stream_depth = 0u;
    parser->json_stream_total_bytes = 0u;
    parser->json_stream_text_bytes = 0u;
    parser->json_stream_text_is_key = 0;
  }

  parser->lex_mode = LONEJSON_LEX_STRING;
  parser->lex_is_key = 0;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  if ((field == NULL || field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) &&
      parser->json_stream_active) {
    status = lonejson__json_value_string_begin(parser, 0);
    if (status != LONEJSON_STATUS_OK) {
      if (field != NULL && field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
        lonejson__parser_clear_json_stream_value(parser);
      }
      return status;
    }
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_JSON_VISITOR;
    parser->token.data = NULL;
    parser->token.cap = 0u;
    parser->token.len = 0u;
    return LONEJSON_STATUS_OK;
  }
  if (parser->validate_only) {
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DISCARD;
    parser->token.data = NULL;
    parser->token.cap = 0u;
    parser->token.len = 0u;
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__field_is_streamed(field)) {
    status = lonejson__stream_value_begin(parser, field, ptr);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_STREAM;
    parser->token.data = NULL;
    parser->token.cap = 0u;
    parser->token.len = 0u;
    return LONEJSON_STATUS_OK;
  }
  if (field != NULL && field->kind == LONEJSON_FIELD_KIND_STRING &&
      field->storage == LONEJSON_STORAGE_FIXED && field->fixed_capacity != 0u) {
    lonejson_frame *frame = (parser->frame_count != 0u)
                                ? &parser->frames[parser->frame_count - 1u]
                                : NULL;
    char *stage = NULL;
    int owned = 0;

    if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT ||
        frame->pending_field != field) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "direct string outside object context");
    }
    if (!lonejson__mark_field_seen(parser, frame, field)) {
      return parser->error.code;
    }
    if (!parser->options.clear_destination) {
      if (ptr != NULL && ((const char *)ptr)[0] != '\0') {
        if (parser->options.fixed_string_scratch != NULL &&
            parser->options.fixed_string_scratch_size >=
                field->fixed_capacity) {
          stage = (char *)parser->options.fixed_string_scratch;
        } else if (field->fixed_capacity <= lonejson__token_limit(parser)) {
          stage = (char *)lonejson__token_base(parser);
        } else {
          if (!lonejson__parser_alloc_can_grow(parser, 0u,
                                               field->fixed_capacity)) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
                parser->error.line, parser->error.column,
                "parse allocations exceed configured max bytes");
          }
          stage = (char *)lonejson__owned_malloc_parse(parser,
                                                       field->fixed_capacity);
          if (stage == NULL) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                parser->error.offset, parser->error.line, parser->error.column,
                "failed to allocate fixed string staging buffer");
          }
          owned = 1;
        }
        parser->direct_string_commit_dst = (char *)ptr;
      } else {
        stage = (char *)ptr;
        parser->direct_string_commit_dst = NULL;
      }
      parser->direct_string_active = 1;
      parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DIRECT;
      parser->direct_string_ptr = stage;
      parser->direct_string_dst = NULL;
      parser->direct_string_len = 0u;
      parser->direct_string_capacity = field->fixed_capacity;
      parser->direct_string_owned = owned;
      parser->direct_string_truncated = 0;
      parser->direct_string_overflow_policy = field->overflow_policy;
      parser->direct_string_field = field;
      return LONEJSON_STATUS_OK;
    }
    parser->direct_string_active = 1;
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DIRECT;
    parser->direct_string_ptr = (char *)ptr;
    parser->direct_string_dst = NULL;
    parser->direct_string_commit_dst = NULL;
    parser->direct_string_len = 0u;
    parser->direct_string_capacity = field->fixed_capacity;
    parser->direct_string_owned = 0;
    parser->direct_string_truncated = 0;
    parser->direct_string_overflow_policy = field->overflow_policy;
    parser->direct_string_field = field;
    return LONEJSON_STATUS_OK;
  }
  if (field != NULL && field->kind == LONEJSON_FIELD_KIND_STRING &&
      field->storage == LONEJSON_STORAGE_DYNAMIC) {
    lonejson_frame *frame = (parser->frame_count != 0u)
                                ? &parser->frames[parser->frame_count - 1u]
                                : NULL;
    char *buffer;
    size_t initial_capacity = 64u;
    size_t available = lonejson__parser_alloc_available(parser, 0u);

    if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT ||
        frame->pending_field != field) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "direct string outside object context");
    }
    if (!lonejson__mark_field_seen(parser, frame, field)) {
      return parser->error.code;
    }
    if (parser->options.max_dynamic_string_bytes != 0u &&
        parser->options.max_dynamic_string_bytes != SIZE_MAX &&
        initial_capacity > parser->options.max_dynamic_string_bytes + 1u) {
      initial_capacity = parser->options.max_dynamic_string_bytes + 1u;
    }
    if (available != SIZE_MAX && initial_capacity > available) {
      initial_capacity = available;
    }
    if (initial_capacity == 0u) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "parse allocations exceed configured max bytes");
    }

    if (!lonejson__parser_alloc_can_grow(parser, 0u, initial_capacity)) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "parse allocations exceed configured max bytes");
    }
    buffer = (char *)lonejson__owned_malloc_parse(parser, initial_capacity);
    if (buffer == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to allocate string");
    }
    buffer[0] = '\0';
    parser->direct_string_active = 1;
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DIRECT;
    parser->direct_string_ptr = buffer;
    parser->direct_string_dst = (char **)ptr;
    parser->direct_string_commit_dst = NULL;
    parser->direct_string_len = 0u;
    parser->direct_string_capacity = initial_capacity;
    parser->direct_string_owned = 1;
    parser->direct_string_truncated = 0;
    parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
    parser->direct_string_field = field;
    return LONEJSON_STATUS_OK;
  }
  return lonejson__prepare_token(parser)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_number_lex(lonejson_parser *parser, unsigned char ch) {
  parser->lex_mode = LONEJSON_LEX_NUMBER;
  parser->lex_is_key = 0;
  if (!lonejson__prepare_token(parser)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parser workspace exhausted by token");
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append number");
}

static LONEJSON__INLINE lonejson_status lonejson__begin_literal_lex(
    lonejson_parser *parser, lonejson_lex_mode mode, unsigned char ch) {
  parser->lex_mode = mode;
  parser->lex_is_key = 0;
  if (!lonejson__prepare_token(parser)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parser workspace exhausted by token");
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append literal");
}

static lonejson_status
lonejson__stream_value_append_decoded(lonejson_parser *parser,
                                      const unsigned char *data, size_t len) {
  if (!parser->stream_value_active || parser->stream_ptr == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "streamed field is not active");
  }
  return lonejson__spooled_append_parse(parser,
                                        (lonejson_spooled *)parser->stream_ptr,
                                        data, len, &parser->error);
}

static lonejson_status
lonejson__string_array_stream_type_error(lonejson_parser *parser,
                                         const lonejson_field *field) {
  return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                             parser->error.offset, parser->error.line,
                             parser->error.column, "array '%s' expects strings",
                             field->json_key);
}

static lonejson_status
lonejson__stream_value_append_base64_char(lonejson_parser *parser,
                                          unsigned char ch) {
  int value;

  value =
      ch == (unsigned char)'='
          ? -2
          : lonejson__base64_value_for_variant(ch, LONEJSON_BASE64_STANDARD);
  if (value < -1) {
    value = -2;
  }
  if (value == -1) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid base64 character in streamed field");
  }
  if (parser->stream_base64_saw_padding && ch != '=') {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "non-padding base64 data after padding");
  }
  parser->stream_base64_quad[parser->stream_base64_quad_len++] = ch;
  if (ch == '=') {
    parser->stream_base64_saw_padding = 1;
  }
  if (parser->stream_base64_quad_len == 4u) {
    unsigned char out[3];
    size_t out_len;
    lonejson_status status;
    lonejson_error decode_error;

    lonejson_error_init(&decode_error);
    status = lonejson_base64_decode((const char *)parser->stream_base64_quad,
                                    4u, LONEJSON_BASE64_STANDARD, out,
                                    sizeof(out), &out_len, &decode_error);
    if (status != LONEJSON_STATUS_OK) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid base64 quartet in streamed field");
    }
    status = lonejson__stream_value_append_decoded(parser, out, out_len);
    parser->stream_base64_quad_len = 0u;
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_append(lonejson_parser *parser,
                                                     const unsigned char *data,
                                                     size_t len) {
  if (parser->stream_field->kind == LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM) {
    lonejson_string_array_stream *stream =
        (lonejson_string_array_stream *)parser->stream_ptr;
    if (stream->handler.chunk == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_INVALID_ARGUMENT,
          parser->error.offset, parser->error.line, parser->error.column,
          "string array stream field '%s' has no chunk handler",
          parser->stream_field->json_key);
    }
    return stream->handler.chunk(stream->user, (const char *)data, len,
                                 &parser->error);
  }
  if (parser->stream_field->kind == LONEJSON_FIELD_KIND_STRING_STREAM) {
    return lonejson__stream_value_append_decoded(parser, data, len);
  }
  {
    size_t i;
    lonejson_status status;

    for (i = 0u; i < len; ++i) {
      status = lonejson__stream_value_append_base64_char(parser, data[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_begin(lonejson_parser *parser,
                                                    const lonejson_field *field,
                                                    void *ptr) {
  if (field->kind == LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM) {
    lonejson_string_array_stream *stream = (lonejson_string_array_stream *)ptr;
    if (stream->_lonejson_magic !=
            lonejson__init_cookie(stream,
                                  LONEJSON__STRING_ARRAY_STREAM_MAGIC) ||
        stream->handler.chunk == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_INVALID_ARGUMENT,
          parser->error.offset, parser->error.line, parser->error.column,
          "string array stream field '%s' has no chunk handler",
          field->json_key);
    }
    if (stream->active) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
          parser->error.line, parser->error.column,
          "string array stream field '%s' is already active", field->json_key);
    }
    if (stream->handler.begin != NULL) {
      lonejson_status status =
          stream->handler.begin(stream->user, &parser->error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    lonejson__string_array_stream_assign_methods(stream);
    stream->active = 1;
    parser->stream_value_active = 1;
    parser->stream_field = field;
    parser->stream_ptr = ptr;
    parser->stream_base64_quad_len = 0u;
    parser->stream_base64_saw_padding = 0;
    return LONEJSON_STATUS_OK;
  }
  lonejson__spooled_apply_allocator((lonejson_spooled *)ptr,
                                    &parser->allocator);
  lonejson__spooled_reset_parse(parser, (lonejson_spooled *)ptr);
  parser->stream_value_active = 1;
  parser->stream_field = field;
  parser->stream_ptr = ptr;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_finish(lonejson_parser *parser) {
  lonejson_status status;

  status = LONEJSON_STATUS_OK;
  if (parser->stream_value_active) {
    if (parser->stream_field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM &&
        parser->stream_base64_quad_len != 0u) {
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
          parser->error.line, parser->error.column,
          "incomplete base64 quartet in streamed field");
    } else if (parser->stream_field->kind ==
               LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM) {
      lonejson_string_array_stream *stream =
          (lonejson_string_array_stream *)parser->stream_ptr;
      if (stream->handler.end != NULL) {
        status = stream->handler.end(stream->user, &parser->error);
      }
      stream->active = 0;
    }
  }
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  return status;
}

static LONEJSON__INLINE lonejson_status lonejson__direct_string_append_bytes(
    lonejson_parser *parser, const unsigned char *data, size_t len) {
  size_t limit;
  size_t remaining;
  size_t copy_len;
  size_t required;
  size_t next_capacity;
  size_t grown;
  size_t string_limit;
  char *next;

  if (parser->direct_string_dst != NULL) {
    required = parser->direct_string_len + len + 1u;
    string_limit = parser->options.max_dynamic_string_bytes;
    if (!parser->direct_string_active || parser->direct_string_ptr == NULL ||
        parser->direct_string_dst == NULL ||
        parser->direct_string_capacity == 0u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "direct string field is not active");
    }
    if (string_limit != 0u && parser->direct_string_len + len > string_limit) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "string field '%s' exceeds configured max dynamic string bytes",
          parser->direct_string_field != NULL
              ? parser->direct_string_field->json_key
              : "<unknown>");
    }
    if (required > parser->direct_string_capacity) {
      next_capacity = parser->direct_string_capacity;
      while (next_capacity < required) {
        grown = next_capacity * 2u;
        if (grown <= next_capacity) {
          next_capacity = required;
          break;
        }
        next_capacity = grown;
      }
      if (string_limit != 0u && string_limit != SIZE_MAX &&
          next_capacity > string_limit + 1u) {
        next_capacity = string_limit + 1u;
      }
      if (!lonejson__parser_alloc_can_grow(
              parser,
              lonejson__parser_alloc_counted_bytes(parser,
                                                   parser->direct_string_ptr),
              next_capacity)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
            parser->error.line, parser->error.column,
            "parse allocations exceed configured max bytes");
      }
      next = (char *)lonejson__owned_realloc_parse(
          parser, parser->direct_string_ptr, next_capacity);
      if (next == NULL) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to allocate string");
      }
      parser->direct_string_ptr = next;
      parser->direct_string_capacity = next_capacity;
    }
    memcpy(parser->direct_string_ptr + parser->direct_string_len, data, len);
    parser->direct_string_len += len;
    return LONEJSON_STATUS_OK;
  }
  if (!parser->direct_string_active || parser->direct_string_ptr == NULL ||
      parser->direct_string_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "direct string field is not active");
  }
  limit = parser->direct_string_capacity - 1u;
  remaining = (parser->direct_string_len < limit)
                  ? (limit - parser->direct_string_len)
                  : 0u;
  copy_len = len < remaining ? len : remaining;
  if (copy_len != 0u) {
    memcpy(parser->direct_string_ptr + parser->direct_string_len, data,
           copy_len);
    parser->direct_string_len += copy_len;
  }
  if (copy_len == len) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->direct_string_overflow_policy == LONEJSON_OVERFLOW_FAIL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "string field '%s' exceeds fixed capacity",
                               parser->direct_string_field != NULL
                                   ? parser->direct_string_field->json_key
                                   : "<unknown>");
  }
  parser->direct_string_truncated = 1;
  parser->error.truncated = 1;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE void
lonejson__direct_string_clear(lonejson_parser *parser,
                              int restore_destination) {
  if (parser == NULL || !parser->direct_string_active) {
    return;
  }
  (void)restore_destination;
  if (parser->direct_string_owned && parser->direct_string_ptr != NULL) {
    lonejson__owned_free_parse(parser, parser->direct_string_ptr);
  }
  parser->direct_string_active = 0;
  parser->direct_string_ptr = NULL;
  parser->direct_string_dst = NULL;
  parser->direct_string_commit_dst = NULL;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->direct_string_len = 0u;
  parser->direct_string_capacity = 0u;
  parser->direct_string_owned = 0;
  parser->direct_string_truncated = 0;
  parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
  parser->direct_string_field = NULL;
}

static LONEJSON__INLINE void
lonejson__direct_string_abort(lonejson_parser *parser) {
  lonejson__direct_string_clear(parser, 1);
}

static LONEJSON__INLINE lonejson_status
lonejson__direct_string_finish(lonejson_parser *parser) {
  lonejson_status status;
  char *old;

  if (!parser->direct_string_active || parser->direct_string_ptr == NULL ||
      parser->direct_string_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "direct string field is not active");
  }
  parser->direct_string_ptr[parser->direct_string_len] = '\0';
  status = parser->direct_string_truncated &&
                   parser->direct_string_overflow_policy ==
                       LONEJSON_OVERFLOW_TRUNCATE
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  if (parser->direct_string_owned) {
    if (parser->direct_string_dst != NULL) {
      old = *parser->direct_string_dst;
      *parser->direct_string_dst = parser->direct_string_ptr;
      lonejson__owned_free_parse(parser, old);
    } else if (parser->direct_string_commit_dst != NULL) {
      memcpy(parser->direct_string_commit_dst, parser->direct_string_ptr,
             parser->direct_string_len + 1u);
      lonejson__owned_free_parse(parser, parser->direct_string_ptr);
    }
  } else if (parser->direct_string_commit_dst != NULL &&
             parser->direct_string_ptr != parser->direct_string_commit_dst) {
    memcpy(parser->direct_string_commit_dst, parser->direct_string_ptr,
           parser->direct_string_len + 1u);
  }
  parser->direct_string_active = 0;
  parser->direct_string_ptr = NULL;
  parser->direct_string_dst = NULL;
  parser->direct_string_commit_dst = NULL;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->direct_string_len = 0u;
  parser->direct_string_capacity = 0u;
  parser->direct_string_owned = 0;
  parser->direct_string_truncated = 0;
  parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
  parser->direct_string_field = NULL;
  {
    lonejson_status complete = lonejson__complete_parent_after_value(parser);
    if (complete != LONEJSON_STATUS_OK &&
        complete != LONEJSON_STATUS_TRUNCATED) {
      return complete;
    }
  }
  return status;
}
