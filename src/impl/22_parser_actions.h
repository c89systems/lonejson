static LONEJSON__INLINE lonejson_status
lonejson__complete_parent_after_value(lonejson_parser *parser) {
  lonejson_frame *parent;

  if (parser->frame_count == 0u) {
    parser->root_finished = 1;
    return LONEJSON_STATUS_OK;
  }
  parent = &parser->frames[parser->frame_count - 1u];
  if (parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->state = LONEJSON_FRAME_OBJECT_COMMA_OR_END;
    parent->pending_field = NULL;
  } else {
    parent->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__complete_streamed_string_token(lonejson_parser *parser) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;

  if (parser->lex_is_key) {
    if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT ||
        frame->state != LONEJSON_FRAME_OBJECT_KEY_OR_END) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "object key outside object context");
    }
    frame->pending_field = NULL;
    if (parser->json_stream_active &&
        !lonejson__json_value_parse_visitor_active(parser)) {
      lonejson_status status = lonejson__json_value_emit(parser, ":", 1u);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    frame->state = LONEJSON_FRAME_OBJECT_COLON;
    return LONEJSON_STATUS_OK;
  }

  if (frame == NULL) {
    if (parser->validate_only && parser->json_stream_active) {
      lonejson__parser_clear_json_stream_value(parser);
      parser->json_stream_depth = 0u;
      parser->root_started = 1;
      parser->root_finished = 1;
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "unexpected scalar at document root");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    if (frame->pending_field == NULL) {
      return lonejson__complete_parent_after_value(parser);
    }
    if (frame->pending_field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      if (!lonejson__mark_field_seen(parser, frame, frame->pending_field)) {
        return parser->error.code;
      }
      lonejson__parser_clear_json_stream_value(parser);
      parser->json_stream_depth = 0u;
      return lonejson__complete_parent_after_value(parser);
    }
  } else if (frame->kind == LONEJSON_CONTAINER_ARRAY && frame->field == NULL) {
    return lonejson__complete_parent_after_value(parser);
  }

  return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                             parser->error.offset, parser->error.line,
                             parser->error.column,
                             "invalid streamed string context");
}

static LONEJSON__INLINE int lonejson__streamed_json_value_scalar_field_active(
    const lonejson_parser *parser) {
  const lonejson_frame *frame = (parser->frame_count != 0u)
                                    ? &parser->frames[parser->frame_count - 1u]
                                    : NULL;

  return !parser->lex_is_key && frame != NULL &&
         frame->kind == LONEJSON_CONTAINER_OBJECT &&
         frame->pending_field != NULL &&
         frame->pending_field->kind == LONEJSON_FIELD_KIND_JSON_VALUE;
}

static lonejson_status lonejson__handle_scalar_for_field(
    lonejson_parser *parser, lonejson_frame *frame, const lonejson_field *field,
    const char *value, size_t len, lonejson_lex_mode mode) {
  void *base;
  void *ptr;
  lonejson_status status;

  if (field == NULL) {
    return lonejson__complete_parent_after_value(parser);
  }
  if (frame != NULL && !lonejson__mark_field_seen(parser, frame, field)) {
    return parser->error.code;
  }
  base = frame ? frame->object_ptr : parser->root_dst;
  ptr = lonejson__field_ptr(base, field);
  switch (mode) {
  case LONEJSON_LEX_STRING:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    if (field->kind == LONEJSON_FIELD_KIND_STRING_STREAM ||
        field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM) {
      return LONEJSON_STATUS_OK;
    }
    if (field->kind == LONEJSON_FIELD_KIND_STRING_SOURCE ||
        field->kind == LONEJSON_FIELD_KIND_BASE64_SOURCE) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "field '%s' is serialize-only and cannot be parsed from JSON",
          field->json_key);
    }
    if (field->kind != LONEJSON_FIELD_KIND_STRING) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected string field '%s'", field->json_key);
    }
    return lonejson__assign_string(parser, field, ptr, value, len);
  case LONEJSON_LEX_NUMBER:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    if (field->kind == LONEJSON_FIELD_KIND_I64) {
      status = lonejson__assign_i64(parser, field, ptr, value);
      if (status == LONEJSON_STATUS_OK) {
        lonejson__field_set_presence(base, field, 1);
      }
      return status;
    }
    if (field->kind == LONEJSON_FIELD_KIND_U64) {
      status = lonejson__assign_u64(parser, field, ptr, value);
      if (status == LONEJSON_STATUS_OK) {
        lonejson__field_set_presence(base, field, 1);
      }
      return status;
    }
    if (field->kind == LONEJSON_FIELD_KIND_F64) {
      status = lonejson__assign_f64(parser, field, ptr, value, len);
      if (status == LONEJSON_STATUS_OK) {
        lonejson__field_set_presence(base, field, 1);
      }
      return status;
    }
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "numeric value does not match field '%s'", field->json_key);
  case LONEJSON_LEX_TRUE:
  case LONEJSON_LEX_FALSE:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    if (field->kind != LONEJSON_FIELD_KIND_BOOL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "boolean value does not match field '%s'", field->json_key);
    }
    status =
        lonejson__assign_bool(parser, field, ptr, mode == LONEJSON_LEX_TRUE);
    if (status == LONEJSON_STATUS_OK) {
      lonejson__field_set_presence(base, field, 1);
    }
    return status;
  case LONEJSON_LEX_NULL:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    status = lonejson__assign_null(parser, field, ptr);
    if (status == LONEJSON_STATUS_OK &&
        (field->flags & LONEJSON_FIELD_ACCEPT_NULL) != 0u) {
      lonejson__field_set_presence(base, field, 0);
    }
    return status;
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "unexpected scalar mode");
  }
}

static lonejson_status
lonejson__handle_array_scalar(lonejson_parser *parser,
                              lonejson_frame *array_frame, const char *value,
                              size_t len, lonejson_lex_mode mode) {
  void *ptr = lonejson__field_ptr(array_frame->object_ptr, array_frame->field);

  switch (array_frame->field->kind) {
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
    if (mode == LONEJSON_LEX_STRING) {
      return LONEJSON_STATUS_OK;
    }
    return lonejson__string_array_stream_type_error(parser, array_frame->field);
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "array '%s' expects object items", array_frame->field->json_key);
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
    if (mode == LONEJSON_LEX_NULL) {
      return lonejson__array_append_string(
          parser, array_frame->field, (lonejson_string_array *)ptr, "", 0u);
    }
    if (mode != LONEJSON_LEX_STRING) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects strings", array_frame->field->json_key);
    }
    return lonejson__array_append_string(
        parser, array_frame->field, (lonejson_string_array *)ptr, value, len);
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects integers", array_frame->field->json_key);
    } else {
      lonejson_int64 parsed = 0;
      lonejson_status status =
          lonejson__assign_i64(parser, array_frame->field, &parsed, value);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_i64(parser, array_frame->field,
                                        (lonejson_i64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects unsigned integers", array_frame->field->json_key);
    } else {
      lonejson_uint64 parsed = 0;
      lonejson_status status =
          lonejson__assign_u64(parser, array_frame->field, &parsed, value);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_u64(parser, array_frame->field,
                                        (lonejson_u64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects numbers", array_frame->field->json_key);
    } else {
      double parsed = 0.0;
      lonejson_status status =
          lonejson__assign_f64(parser, array_frame->field, &parsed, value, len);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_f64(parser, array_frame->field,
                                        (lonejson_f64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    if (mode != LONEJSON_LEX_TRUE && mode != LONEJSON_LEX_FALSE) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects booleans", array_frame->field->json_key);
    }
    return lonejson__array_append_bool(parser, array_frame->field,
                                       (lonejson_bool_array *)ptr,
                                       mode == LONEJSON_LEX_TRUE);
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "field '%s' does not accept scalar array values",
                               array_frame->field->json_key);
  }
}

static lonejson_status lonejson__mapped_array_stream_require_handler(
    lonejson_parser *parser, const lonejson_field *field,
    lonejson_mapped_array_stream *stream) {
  if (stream == NULL ||
      stream->_lonejson_magic !=
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC) ||
      stream->handler.item_map == NULL || stream->handler.item_dst == NULL ||
      stream->handler.item == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_ARGUMENT, parser->error.offset,
        parser->error.line, parser->error.column,
        "mapped array stream field '%s' has no item handler", field->json_key);
  }
  if (!lonejson__map_layout_is_valid(stream->handler.item_map)) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_ARGUMENT, parser->error.offset,
        parser->error.line, parser->error.column,
        "mapped array stream field '%s' has an invalid item map",
        field->json_key);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__mapped_array_stream_finish_item(lonejson_parser *parser,
                                          lonejson_frame *frame) {
  lonejson_frame *parent;
  lonejson_mapped_array_stream *stream;
  lonejson_status status;

  if (frame == NULL || frame->field == NULL ||
      frame->field->kind != LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->frame_count < 2u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "mapped array stream item has no parent array");
  }
  parent = &parser->frames[parser->frame_count - 2u];
  if (parent->kind != LONEJSON_CONTAINER_ARRAY ||
      parent->field != frame->field || parent->object_ptr == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "mapped array stream item parent mismatch");
  }
  stream = (lonejson_mapped_array_stream *)lonejson__field_ptr(
      parent->object_ptr, parent->field);
  status = stream->handler.item(stream->handler.user, stream->handler.item_dst,
                                &parser->error);
  lonejson__reset_map_parse(parser, stream->handler.item_map,
                            stream->handler.item_dst);
  if (status != LONEJSON_STATUS_OK) {
    if (parser->error.code == LONEJSON_STATUS_OK) {
      return lonejson__set_error(
          &parser->error, status, parser->error.offset, parser->error.line,
          parser->error.column,
          "mapped array stream field '%s' item callback failed",
          frame->field->json_key);
    }
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static void
lonejson__mapped_array_stream_reset_item(lonejson_parser *parser,
                                         lonejson_mapped_array_stream *stream) {
  if (parser == NULL || stream == NULL ||
      stream->_lonejson_magic !=
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC) ||
      stream->handler.item_map == NULL || stream->handler.item_dst == NULL) {
    return;
  }
  lonejson__reset_map_parse(parser, stream->handler.item_map,
                            stream->handler.item_dst);
}

static void
lonejson__parser_unwind_active_mapped_array_streams(lonejson_parser *parser) {
  size_t i;

  if (parser == NULL) {
    return;
  }
  i = parser->frame_count;
  while (i != 0u) {
    lonejson_frame *frame;
    lonejson_frame *parent;
    lonejson_mapped_array_stream *stream;

    --i;
    frame = &parser->frames[i];
    if (frame->kind != LONEJSON_CONTAINER_OBJECT || frame->field == NULL ||
        frame->field->kind != LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM ||
        i == 0u) {
      continue;
    }
    parent = &parser->frames[i - 1u];
    if (parent->kind != LONEJSON_CONTAINER_ARRAY ||
        parent->field != frame->field || parent->object_ptr == NULL) {
      continue;
    }
    stream = (lonejson_mapped_array_stream *)lonejson__field_ptr(
        parent->object_ptr, parent->field);
    lonejson__mapped_array_stream_reset_item(parser, stream);
  }
  for (i = 0u; i < parser->frame_count; ++i) {
    lonejson_frame *frame = &parser->frames[i];
    if (frame->kind == LONEJSON_CONTAINER_ARRAY && frame->field != NULL &&
        frame->field->kind == LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM &&
        frame->object_ptr != NULL) {
      lonejson_mapped_array_stream *stream =
          (lonejson_mapped_array_stream *)lonejson__field_ptr(frame->object_ptr,
                                                              frame->field);
      if (stream->_lonejson_magic ==
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC)) {
        stream->active = 0;
      }
    }
  }
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_object_value(lonejson_parser *parser) {
  lonejson_frame *parent = (parser->frame_count != 0u)
                               ? &parser->frames[parser->frame_count - 1u]
                               : NULL;
  const lonejson_field *field = NULL;
  void *object_ptr = NULL;
  const lonejson_map *map = NULL;
  lonejson_frame *frame;
  lonejson_mapped_array_stream *mapped_stream = NULL;
  lonejson_status status;
  int started_capture = 0;
  int started_root_visitor = 0;
  int started_root_value = 0;

  if (!parser->root_started) {
    parser->root_started = 1;
    map = parser->validate_only ? NULL : parser->root_map;
    object_ptr = parser->validate_only ? NULL : parser->root_dst;
    started_root_visitor = parser->validate_only &&
                           lonejson__json_value_parse_visitor_active(parser);
    started_root_value = parser->validate_only && parser->json_stream_active;
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->after_comma = 0;
    field = parent->pending_field;
    if (field == NULL) {
      map = NULL;
      object_ptr = NULL;
    } else {
      if (!lonejson__mark_field_seen(parser, parent, field)) {
        return parser->error.code;
      }
      if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
        lonejson_json_value *value = (lonejson_json_value *)lonejson__field_ptr(
            parent->object_ptr, field);
        if (lonejson__json_value_prepare_parse(parser, value, &parser->error) !=
            LONEJSON_STATUS_OK) {
          return parser->error.code;
        }
        lonejson__parser_set_json_stream_value(parser, value);
        parser->json_stream_depth = 1u;
        parser->json_stream_total_bytes = 0u;
        parser->json_stream_text_bytes = 0u;
        parser->json_stream_text_is_key = 0;
        if (lonejson__json_value_parse_visitor_active(parser)) {
          if (value->parse_visitor_limits.max_depth != 0u &&
              parser->json_stream_depth >
                  value->parse_visitor_limits.max_depth) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
                parser->error.line, parser->error.column,
                "JSON value nesting exceeds maximum depth");
          }
          status = lonejson__json_value_object_begin(parser);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return status;
          }
        } else if (lonejson__json_value_emit(parser, "{", 1u) !=
                   LONEJSON_STATUS_OK) {
          lonejson__parser_clear_json_stream_value(parser);
          parser->json_stream_depth = 0u;
          return parser->error.code;
        }
        map = NULL;
        object_ptr = NULL;
        started_capture = 1;
      } else {
        if (field->kind != LONEJSON_FIELD_KIND_OBJECT) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
              parser->error.offset, parser->error.line, parser->error.column,
              "field '%s' expects non-object value", field->json_key);
        }
        map = field->submap;
        object_ptr = lonejson__field_ptr(parent->object_ptr, field);
        lonejson__reset_map_parse(parser, map, object_ptr);
      }
    }
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_ARRAY) {
    parent->after_comma = 0;
    if (parent->field == NULL) {
      map = NULL;
      object_ptr = NULL;
    } else if (parent->field->kind == LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM) {
      status = lonejson__mapped_array_stream_require_handler(
          parser, parent->field,
          (lonejson_mapped_array_stream *)lonejson__field_ptr(
              parent->object_ptr, parent->field));
      mapped_stream = (lonejson_mapped_array_stream *)lonejson__field_ptr(
          parent->object_ptr, parent->field);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      lonejson__reset_map_parse(parser, mapped_stream->handler.item_map,
                                mapped_stream->handler.item_dst);
      field = parent->field;
      map = mapped_stream->handler.item_map;
      object_ptr = mapped_stream->handler.item_dst;
    } else {
      lonejson_object_array *arr;
      if (parent->field->kind != LONEJSON_FIELD_KIND_OBJECT_ARRAY) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
            parser->error.line, parser->error.column,
            "array '%s' expects scalar elements", parent->field->json_key);
      }
      arr = (lonejson_object_array *)lonejson__field_ptr(parent->object_ptr,
                                                         parent->field);
      object_ptr =
          lonejson__object_array_append_slot(parser, parent->field, arr);
      if (object_ptr == NULL) {
        if (parser->error.code != LONEJSON_STATUS_OK) {
          return parser->error.code;
        }
        parser->error.truncated = 1;
        map = NULL;
      } else {
        map = parent->field->submap;
      }
    }
  }

  frame = lonejson__push_frame(parser, LONEJSON_CONTAINER_OBJECT);
  if (frame == NULL) {
    if (mapped_stream != NULL) {
      lonejson__mapped_array_stream_reset_item(parser, mapped_stream);
    }
    return parser->error.code;
  }
  frame->map = map;
  frame->object_ptr = object_ptr;
  frame->field = field;
  frame->required_remaining = lonejson__required_count_for_map(parser, map);
  if (started_root_visitor) {
    parser->json_stream_depth = 1u;
    return lonejson__json_value_object_begin(parser);
  }
  if (started_root_value) {
    parser->json_stream_depth = 1u;
    return lonejson__json_value_emit(parser, "{", 1u);
  }
  if (!started_capture && parser->json_stream_active && map == NULL &&
      field == NULL && parent != NULL) {
    parser->json_stream_depth++;
    if (lonejson__json_value_parse_visitor_active(parser)) {
      if (parser->json_stream_value->parse_visitor_limits.max_depth != 0u &&
          parser->json_stream_depth >
              parser->json_stream_value->parse_visitor_limits.max_depth) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "JSON value nesting exceeds maximum depth");
      }
      return lonejson__json_value_object_begin(parser);
    }
    return lonejson__json_value_emit(parser, "{", 1u);
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_array_value(lonejson_parser *parser) {
  lonejson_frame *parent = (parser->frame_count != 0u)
                               ? &parser->frames[parser->frame_count - 1u]
                               : NULL;
  const lonejson_field *field = NULL;
  void *object_ptr = NULL;
  lonejson_frame *frame;
  lonejson_status status;
  int started_capture = 0;
  int started_root_visitor = 0;
  int started_root_value = 0;

  if (!parser->root_started) {
    if (!parser->validate_only) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "root value must be an object");
    }
    parser->root_started = 1;
    started_root_visitor = lonejson__json_value_parse_visitor_active(parser);
    started_root_value = parser->json_stream_active;
  }
  if (parent != NULL && parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->after_comma = 0;
    field = parent->pending_field;
    object_ptr = parent->object_ptr;
    if (field != NULL) {
      if (!lonejson__mark_field_seen(parser, parent, field)) {
        return parser->error.code;
      }
      if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
        lonejson_json_value *value = (lonejson_json_value *)lonejson__field_ptr(
            parent->object_ptr, field);
        if (lonejson__json_value_prepare_parse(parser, value, &parser->error) !=
            LONEJSON_STATUS_OK) {
          return parser->error.code;
        }
        lonejson__parser_set_json_stream_value(parser, value);
        parser->json_stream_depth = 1u;
        parser->json_stream_total_bytes = 0u;
        parser->json_stream_text_bytes = 0u;
        parser->json_stream_text_is_key = 0;
        if (lonejson__json_value_parse_visitor_active(parser)) {
          if (value->parse_visitor_limits.max_depth != 0u &&
              parser->json_stream_depth >
                  value->parse_visitor_limits.max_depth) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
                parser->error.line, parser->error.column,
                "JSON value nesting exceeds maximum depth");
          }
          status = lonejson__json_value_array_begin(parser);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return status;
          }
        } else if (lonejson__json_value_emit(parser, "[", 1u) !=
                   LONEJSON_STATUS_OK) {
          lonejson__parser_clear_json_stream_value(parser);
          parser->json_stream_depth = 0u;
          return parser->error.code;
        }
        field = NULL;
        object_ptr = NULL;
        started_capture = 1;
      } else {
        switch (field->kind) {
        case LONEJSON_FIELD_KIND_STRING_ARRAY:
        case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
        case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
        case LONEJSON_FIELD_KIND_I64_ARRAY:
        case LONEJSON_FIELD_KIND_U64_ARRAY:
        case LONEJSON_FIELD_KIND_F64_ARRAY:
        case LONEJSON_FIELD_KIND_BOOL_ARRAY:
        case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
          lonejson__reset_present_array_field(
              parser, field, lonejson__field_ptr(object_ptr, field));
          break;
        default:
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
              parser->error.offset, parser->error.line, parser->error.column,
              "field '%s' is not an array", field->json_key);
        }
        if (field->kind == LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM) {
          lonejson_mapped_array_stream *stream =
              (lonejson_mapped_array_stream *)lonejson__field_ptr(object_ptr,
                                                                  field);
          status = lonejson__mapped_array_stream_require_handler(parser, field,
                                                                 stream);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
          if (stream->active) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                parser->error.offset, parser->error.line, parser->error.column,
                "mapped array stream field '%s' is already active",
                field->json_key);
          }
          stream->active = 1;
        }
      }
    }
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_ARRAY) {
    parent->after_comma = 0;
    if (parent->field != NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "field '%s' does not accept nested arrays", parent->field->json_key);
    }
    field = NULL;
    object_ptr = NULL;
  }

  frame = lonejson__push_frame(parser, LONEJSON_CONTAINER_ARRAY);
  if (frame == NULL) {
    if (field != NULL &&
        field->kind == LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM &&
        object_ptr != NULL) {
      lonejson_mapped_array_stream *stream =
          (lonejson_mapped_array_stream *)lonejson__field_ptr(object_ptr,
                                                              field);
      if (stream->_lonejson_magic ==
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC)) {
        stream->active = 0;
      }
    }
    return parser->error.code;
  }
  frame->map = NULL;
  frame->object_ptr = object_ptr;
  frame->field = field;
  if (started_root_visitor) {
    parser->json_stream_depth = 1u;
    return lonejson__json_value_array_begin(parser);
  }
  if (started_root_value) {
    parser->json_stream_depth = 1u;
    return lonejson__json_value_emit(parser, "[", 1u);
  }
  if (!started_capture && parser->json_stream_active && field == NULL &&
      parent != NULL) {
    parser->json_stream_depth++;
    if (lonejson__json_value_parse_visitor_active(parser)) {
      if (parser->json_stream_value->parse_visitor_limits.max_depth != 0u &&
          parser->json_stream_depth >
              parser->json_stream_value->parse_visitor_limits.max_depth) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "JSON value nesting exceeds maximum depth");
      }
      return lonejson__json_value_array_begin(parser);
    }
    return lonejson__json_value_emit(parser, "[", 1u);
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__finalize_object(lonejson_parser *parser) {
  lonejson_frame *frame = &parser->frames[parser->frame_count - 1u];
  lonejson_status status;
  size_t i;
  lonejson_uint64 bit;
  lonejson_uint64 required_mask;

  if (frame->map != NULL && frame->required_remaining == 0u &&
      (frame->field == NULL ||
       frame->field->kind != LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM)) {
    lonejson__pop_frame(parser);
    return lonejson__complete_parent_after_value(parser);
  }

  if (frame->map != NULL && frame->map->field_count <= 64u) {
    required_mask = lonejson__required_mask_for_map(parser, frame->map);
    if ((frame->seen_mask & required_mask) != required_mask) {
      for (i = 0u; i < frame->map->field_count; ++i) {
        bit = ((lonejson_uint64)1u) << i;
        if ((required_mask & bit) != 0u && (frame->seen_mask & bit) == 0u) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
              parser->error.offset, parser->error.line, parser->error.column,
              "missing required field '%s'", frame->map->fields[i].json_key);
        }
      }
    }
  } else if (frame->map != NULL && frame->seen_fields != NULL) {
    for (i = 0u; i < frame->map->field_count; ++i) {
      if ((frame->map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u &&
          frame->seen_fields[i] == 0u) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
            parser->error.offset, parser->error.line, parser->error.column,
            "missing required field '%s'", frame->map->fields[i].json_key);
      }
    }
  } else if (frame->map != NULL) {
    for (i = 0u; i < frame->map->field_count; ++i) {
      if ((frame->map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
            parser->error.offset, parser->error.line, parser->error.column,
            "missing required field '%s'", frame->map->fields[i].json_key);
      }
    }
  }

  if (parser->json_stream_active) {
    status = lonejson__json_value_parse_visitor_active(parser)
                 ? lonejson__json_value_object_end(parser)
                 : lonejson__json_value_emit(parser, "}", 1u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  status = lonejson__mapped_array_stream_finish_item(parser, frame);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__pop_frame(parser);
  if (parser->json_stream_active && parser->json_stream_depth != 0u) {
    parser->json_stream_depth--;
    if (parser->json_stream_depth == 0u) {
      lonejson__parser_clear_json_stream_value(parser);
    }
  }
  return lonejson__complete_parent_after_value(parser);
}

static LONEJSON__INLINE lonejson_status
lonejson__finalize_array(lonejson_parser *parser) {
  lonejson_frame *frame = &parser->frames[parser->frame_count - 1u];
  if (parser->json_stream_active) {
    lonejson_status status = lonejson__json_value_parse_visitor_active(parser)
                                 ? lonejson__json_value_array_end(parser)
                                 : lonejson__json_value_emit(parser, "]", 1u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (frame->field != NULL &&
      frame->field->kind == LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM &&
      frame->object_ptr != NULL) {
    lonejson_mapped_array_stream *stream =
        (lonejson_mapped_array_stream *)lonejson__field_ptr(frame->object_ptr,
                                                            frame->field);
    stream->active = 0;
  }
  lonejson__pop_frame(parser);
  if (parser->json_stream_active && parser->json_stream_depth != 0u) {
    parser->json_stream_depth--;
    if (parser->json_stream_depth == 0u) {
      lonejson__parser_clear_json_stream_value(parser);
    }
  }
  return lonejson__complete_parent_after_value(parser);
}

static int lonejson__mapped_numeric_target(const lonejson_frame *frame) {
  const lonejson_field *field;

  if (frame == NULL) {
    return 0;
  }
  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    field = frame->pending_field;
    return field != NULL && (field->kind == LONEJSON_FIELD_KIND_I64 ||
                             field->kind == LONEJSON_FIELD_KIND_U64 ||
                             field->kind == LONEJSON_FIELD_KIND_F64);
  }
  if (frame->kind == LONEJSON_CONTAINER_ARRAY) {
    field = frame->field;
    return field != NULL && (field->kind == LONEJSON_FIELD_KIND_I64_ARRAY ||
                             field->kind == LONEJSON_FIELD_KIND_U64_ARRAY ||
                             field->kind == LONEJSON_FIELD_KIND_F64_ARRAY);
  }
  return 0;
}

static LONEJSON__INLINE lonejson_status lonejson__deliver_key_text(
    lonejson_parser *parser, const char *key, size_t key_len) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;

  if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT ||
      frame->state != LONEJSON_FRAME_OBJECT_KEY_OR_END) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "object key outside object context");
  }
  if (lonejson__json_value_parse_visitor_active(parser)) {
    frame->pending_field = NULL;
    frame->state = LONEJSON_FRAME_OBJECT_COLON;
    return LONEJSON_STATUS_OK;
  }
  frame->pending_field = lonejson__find_field(frame->map, frame, key, key_len);
  if (parser->json_stream_active) {
    lonejson_status status =
        lonejson__json_value_emit_string(parser, key, key_len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__json_value_emit(parser, ":", 1u);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  frame->state = LONEJSON_FRAME_OBJECT_COLON;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__deliver_token(lonejson_parser *parser, lonejson_lex_mode mode) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;
  const char *token_text;

  token_text = lonejson__scratch_cstr(&parser->token);

  if (parser->lex_is_key) {
    return lonejson__deliver_key_text(parser, token_text, parser->token.len);
  }

  if (frame == NULL) {
    if (!parser->root_started && parser->validate_only) {
      lonejson_status status = LONEJSON_STATUS_OK;
      int allow_truncated_status = 0;

      if (mode == LONEJSON_LEX_NUMBER &&
          !lonejson__is_valid_json_number(token_text, parser->token.len)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column, "invalid JSON number");
      }
      if (parser->json_stream_active) {
        if (lonejson__json_value_parse_visitor_active(parser)) {
          allow_truncated_status = 1;
          if (mode == LONEJSON_LEX_NUMBER) {
            status = lonejson__json_value_number(parser, token_text,
                                                 parser->token.len);
          } else if (mode == LONEJSON_LEX_TRUE) {
            status = lonejson__json_value_visit_bool(parser, 1);
          } else if (mode == LONEJSON_LEX_FALSE) {
            status = lonejson__json_value_visit_bool(parser, 0);
          } else if (mode == LONEJSON_LEX_NULL) {
            status = lonejson__json_value_null(parser);
          }
        } else if (mode == LONEJSON_LEX_NUMBER) {
          status =
              lonejson__json_value_emit(parser, token_text, parser->token.len);
        } else if (mode == LONEJSON_LEX_TRUE) {
          status = lonejson__json_value_emit(parser, "true", 4u);
        } else if (mode == LONEJSON_LEX_FALSE) {
          status = lonejson__json_value_emit(parser, "false", 5u);
        } else if (mode == LONEJSON_LEX_NULL) {
          status = lonejson__json_value_emit(parser, "null", 4u);
        }
        if (status != LONEJSON_STATUS_OK &&
            (!allow_truncated_status || status != LONEJSON_STATUS_TRUNCATED)) {
          return status;
        }
        lonejson__parser_clear_json_stream_value(parser);
        parser->json_stream_depth = 0u;
      }
      parser->root_started = 1;
      parser->root_finished = 1;
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "unexpected scalar at document root");
  }

  if (mode == LONEJSON_LEX_NUMBER &&
      (parser->validate_only || !lonejson__mapped_numeric_target(frame)) &&
      !lonejson__is_valid_json_number(token_text, parser->token.len)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid JSON number");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    lonejson_status status;

    if (lonejson__json_value_parse_visitor_active(parser) &&
        frame->pending_field == NULL) {
      if (mode == LONEJSON_LEX_NUMBER) {
        status =
            lonejson__json_value_number(parser, token_text, parser->token.len);
      } else if (mode == LONEJSON_LEX_TRUE) {
        status = lonejson__json_value_visit_bool(parser, 1);
      } else if (mode == LONEJSON_LEX_FALSE) {
        status = lonejson__json_value_visit_bool(parser, 0);
      } else if (mode == LONEJSON_LEX_NULL) {
        status = lonejson__json_value_null(parser);
      } else {
        status = LONEJSON_STATUS_OK;
      }
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__complete_parent_after_value(parser);
    }

    if (parser->json_stream_active && frame->pending_field == NULL) {
      if (mode == LONEJSON_LEX_STRING) {
        status = lonejson__json_value_emit_string(parser, token_text,
                                                  parser->token.len);
      } else {
        static const char true_text[] = "true";
        static const char false_text[] = "false";
        static const char null_text[] = "null";
        const char *text = token_text;
        size_t len = parser->token.len;
        if (mode == LONEJSON_LEX_TRUE) {
          text = true_text;
          len = 4u;
        } else if (mode == LONEJSON_LEX_FALSE) {
          text = false_text;
          len = 5u;
        } else if (mode == LONEJSON_LEX_NULL) {
          text = null_text;
          len = 4u;
        }
        status = lonejson__json_value_emit(parser, text, len);
      }
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__complete_parent_after_value(parser);
    }
    status =
        lonejson__handle_scalar_for_field(parser, frame, frame->pending_field,
                                          token_text, parser->token.len, mode);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      return lonejson__complete_parent_after_value(parser);
    }
    return status;
  }

  if (frame->kind == LONEJSON_CONTAINER_ARRAY) {
    lonejson_status status;
    if (frame->field == NULL) {
      if (lonejson__json_value_parse_visitor_active(parser)) {
        if (mode == LONEJSON_LEX_NUMBER) {
          status = lonejson__json_value_number(parser, token_text,
                                               parser->token.len);
        } else if (mode == LONEJSON_LEX_TRUE) {
          status = lonejson__json_value_visit_bool(parser, 1);
        } else if (mode == LONEJSON_LEX_FALSE) {
          status = lonejson__json_value_visit_bool(parser, 0);
        } else if (mode == LONEJSON_LEX_NULL) {
          status = lonejson__json_value_null(parser);
        } else {
          status = LONEJSON_STATUS_OK;
        }
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (parser->json_stream_active) {
        if (mode == LONEJSON_LEX_STRING) {
          status = lonejson__json_value_emit_string(parser, token_text,
                                                    parser->token.len);
        } else {
          static const char true_text[] = "true";
          static const char false_text[] = "false";
          static const char null_text[] = "null";
          const char *text = token_text;
          size_t len = parser->token.len;
          if (mode == LONEJSON_LEX_TRUE) {
            text = true_text;
            len = 4u;
          } else if (mode == LONEJSON_LEX_FALSE) {
            text = false_text;
            len = 5u;
          } else if (mode == LONEJSON_LEX_NULL) {
            text = null_text;
            len = 4u;
          }
          status = lonejson__json_value_emit(parser, text, len);
        }
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      frame->after_comma = 0;
      frame->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
      return LONEJSON_STATUS_OK;
    }
    status = lonejson__handle_array_scalar(parser, frame, token_text,
                                           parser->token.len, mode);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      frame->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
    }
    return status;
  }

  return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                             parser->error.offset, parser->error.line,
                             parser->error.column, "invalid parser state");
}
