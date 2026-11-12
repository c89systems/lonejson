typedef struct lonejson__write_state {
  lonejson_sink_fn sink;
  void *user;
  lonejson_error *error;
  int pretty;
  size_t depth;
} lonejson__write_state;

static LONEJSON__INLINE lonejson_status
lonejson__emit_pretty_indent(const lonejson__write_state *state, size_t depth) {
  static const char prefix[] = "\n" LONEJSON__SPACES_64;
  size_t spaces;
  lonejson_status status;

  if (!state->pretty) {
    return LONEJSON_STATUS_OK;
  }
  if (depth > (SIZE_MAX / 2u)) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                               0u, "JSON indentation depth is too large");
  }
  spaces = depth * 2u;
  if (spaces <= 64u) {
    return lonejson__emit(state->sink, state->user, state->error, prefix,
                          spaces + 1u);
  }
  status = lonejson__emit(state->sink, state->user, state->error, prefix,
                          sizeof(prefix) - 1u);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  spaces -= 64u;
  while (spaces > 64u) {
    status = lonejson__emit(state->sink, state->user, state->error,
                            LONEJSON__SPACES_64, 64u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    spaces -= 64u;
  }
  if (spaces != 0u) {
    status = lonejson__emit(state->sink, state->user, state->error,
                            LONEJSON__SPACES_64, spaces);
  }
  return status;
}

static lonejson_status
lonejson__serialize_value_pretty(const lonejson_field *field, const void *ptr,
                                 lonejson__write_state *state);

static lonejson_status
lonejson__serialize_value_compact(const lonejson_field *field, const void *ptr,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error);

static lonejson_status lonejson__serialize_map_compact(const lonejson_map *map,
                                                       const void *src,
                                                       lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error);

static lonejson_status lonejson__serialize_map_compact_buffer_exact(
    const lonejson_map *map, const void *src, lonejson_buffer_sink *sink,
    lonejson_error *error);

static lonejson_status lonejson__serialize_map_compact_buffer_grow(
    const lonejson_map *map, const void *src, lonejson_buffer_sink *sink,
    lonejson_error *error);

static lonejson_status lonejson__serialize_value_compact_buffer_exact(
    const lonejson_field *field, const void *ptr, lonejson_buffer_sink *sink,
    lonejson_error *error);

static lonejson_status lonejson__serialize_value_compact_buffer_grow(
    const lonejson_field *field, const void *ptr, lonejson_buffer_sink *sink,
    lonejson_error *error);

static lonejson_status
lonejson__serialize_parse_only_field_error(const lonejson_field *field,
                                           lonejson_error *error) {
  return lonejson__set_error(
      error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
      error ? error->line : 0u, error ? error->column : 0u,
      "field '%s' is parse-only and cannot be "
      "serialized",
      field->json_key);
}

static lonejson_status
lonejson__serialize_map_pretty(const lonejson_map *map, const void *src,
                               lonejson__write_state *state) {
  size_t i;
  size_t emitted;
  lonejson_status status;

  emitted = 0u;
  status = lonejson__emit_cstr(state->sink, state->user, state->error, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0; i < map->field_count; ++i) {
    if (lonejson__field_should_omit(src, &map->fields[i])) {
      continue;
    }
    if (emitted != 0u) {
      status = lonejson__emit_cstr(state->sink, state->user, state->error, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth + 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    status = lonejson__emit_escaped_key(
        state->sink, state->user, state->error, map->fields[i].json_key,
        map->fields[i].json_key_len, state->pretty);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ++state->depth;
    status = lonejson__serialize_value_pretty(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), state);
    --state->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    emitted++;
  }
  if (state->pretty && emitted != 0u) {
    status = lonejson__emit_pretty_indent(state, state->depth);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(state->sink, state->user, state->error, "}");
}

static lonejson_status
lonejson__serialize_jsonl_records(const lonejson_map *map, const void *items,
                                  size_t count, size_t stride,
                                  lonejson__write_state *state) {
  const unsigned char *base = (const unsigned char *)items;
  size_t i;
  lonejson_status status;

  for (i = 0; i < count; ++i) {
    const void *record = (const void *)(base + (i * stride));
    status = lonejson__serialize_map_pretty(map, record, state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = state->sink(state->user, "\n", 1u, state->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__serialize_jsonl_records_compact(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, lonejson_error *error) {
  const unsigned char *base = (const unsigned char *)items;
  size_t i;
  lonejson_status status;

  for (i = 0; i < count; ++i) {
    const void *record = (const void *)(base + (i * stride));
    status = lonejson__serialize_map_compact(map, record, sink, user, error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = sink(user, "\n", 1u, error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

/* Exact small binary fractions need no decimal rounding.  Keep the general
 * formatter for every value outside this narrow, proven representation. */
static int lonejson__format_f64_text(char *buf, size_t capacity, double value) {
  double magnitude = value < 0.0 ? -value : value;

  if (magnitude >= 0.0625 && magnitude < 1.0e12 && capacity >= 32u) {
    double scaled = magnitude * 16.0;
    lonejson_uint64 units = (lonejson_uint64)scaled;
    if ((double)units == scaled) {
      char digits[24];
      size_t pos = sizeof(digits);
      size_t len = 0u;
      lonejson_uint64 whole = units / 16u;
      unsigned fraction = (unsigned)(units % 16u) * 625u;
      unsigned divisor;

      do {
        digits[--pos] = (char)('0' + (int)(whole % 10u));
        whole /= 10u;
      } while (whole != 0u);
      if (value < 0.0) {
        buf[len++] = '-';
      }
      memcpy(buf + len, digits + pos, sizeof(digits) - pos);
      len += sizeof(digits) - pos;
      if (fraction != 0u) {
        buf[len++] = '.';
        for (divisor = 1000u; fraction != 0u; divisor /= 10u) {
          buf[len++] = (char)('0' + fraction / divisor);
          fraction %= divisor;
        }
      }
      buf[len] = '\0';
      return (int)len;
    }
  }
  return snprintf(buf, capacity, "%.17g", value);
}

static lonejson_status lonejson__emit_f64_value(lonejson_sink_fn sink,
                                                void *user,
                                                lonejson_error *error,
                                                double value) {
  char buf[64];
  int written;

  written = lonejson__format_f64_text(buf, sizeof(buf), value);
  if (written < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to format number");
  }
  return lonejson__emit(sink, user, error, buf, (size_t)written);
}

static LONEJSON__INLINE lonejson_status
lonejson__emit_u64_value(lonejson_sink_fn sink, void *user,
                         lonejson_error *error, lonejson_uint64 value) {
  char buf[32];
  size_t idx;

  idx = sizeof(buf);
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    --idx;
    buf[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  return lonejson__emit(sink, user, error, buf + idx, sizeof(buf) - idx);
}

static LONEJSON__INLINE lonejson_status
lonejson__emit_i64_value(lonejson_sink_fn sink, void *user,
                         lonejson_error *error, lonejson_int64 value) {
  lonejson_status status;
  lonejson_uint64 magnitude;

  if (value < 0) {
    status = lonejson__emit_cstr(sink, user, error, "-");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    magnitude = (lonejson_uint64)(-(value + 1)) + 1u;
  } else {
    magnitude = (lonejson_uint64)value;
  }
  return lonejson__emit_u64_value(sink, user, error, magnitude);
}

static lonejson_status
lonejson__buffer_emit_f64_value_exact(lonejson_buffer_sink *sink,
                                      lonejson_error *error, double value) {
  char buf[64];
  int written;

  written = lonejson__format_f64_text(buf, sizeof(buf), value);
  if (written < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to format number");
  }
  return lonejson__buffer_emit_exact(sink, error, buf, (size_t)written);
}

static LONEJSON__INLINE lonejson_status lonejson__buffer_emit_u64_value_exact(
    lonejson_buffer_sink *sink, lonejson_error *error, lonejson_uint64 value) {
  char buf[32];
  size_t idx;

  idx = sizeof(buf);
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    --idx;
    buf[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  return lonejson__buffer_emit_exact(sink, error, buf + idx, sizeof(buf) - idx);
}

static LONEJSON__INLINE lonejson_status lonejson__buffer_emit_i64_value_exact(
    lonejson_buffer_sink *sink, lonejson_error *error, lonejson_int64 value) {
  lonejson_status status;
  lonejson_uint64 magnitude;

  if (value < 0) {
    status = lonejson__buffer_emit_cstr_exact(sink, error, "-");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    magnitude = (lonejson_uint64)(-(value + 1)) + 1u;
  } else {
    magnitude = (lonejson_uint64)value;
  }
  return lonejson__buffer_emit_u64_value_exact(sink, error, magnitude);
}

static lonejson_status lonejson__emit_base64_bytes(lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error,
                                                   const unsigned char *data,
                                                   size_t len) {
  return lonejson_base64_encode_sink(data, len, LONEJSON_BASE64_STANDARD, sink,
                                     user, error);
}

static lonejson_status
lonejson__serialize_spooled_text(const lonejson_spooled *value,
                                 lonejson_sink_fn sink, void *user,
                                 lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson_spooled cursor;
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read spooled text");
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson__emit_escaped_fragment(sink, user, error, buffer,
                                               chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_spooled_base64(const lonejson_spooled *value,
                                   lonejson_sink_fn sink, void *user,
                                   lonejson_error *error) {
  unsigned char buffer[4096];
  unsigned char carry[3];
  size_t carry_len;
  lonejson_spooled cursor;
  lonejson_status status;

  carry_len = 0u;
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(&cursor, buffer, sizeof(buffer));
    size_t offset;

    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read spooled bytes");
    }
    offset = 0u;
    if (carry_len != 0u) {
      while (carry_len < 3u && offset < chunk.bytes_read) {
        carry[carry_len++] = buffer[offset++];
      }
      if (carry_len == 3u) {
        status = lonejson__emit_base64_bytes(sink, user, error, carry, 3u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        carry_len = 0u;
      }
    }
    while (offset + 3u <= chunk.bytes_read) {
      status =
          lonejson__emit_base64_bytes(sink, user, error, buffer + offset, 3u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      offset += 3u;
    }
    while (offset < chunk.bytes_read) {
      carry[carry_len++] = buffer[offset++];
    }
    if (chunk.eof) {
      break;
    }
  }
  if (carry_len != 0u) {
    status = lonejson__emit_base64_bytes(sink, user, error, carry, carry_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_source_text(const lonejson_source *value,
                                lonejson_sink_fn sink, void *user,
                                lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson__source_cursor cursor;
  lonejson_read_result chunk;
  lonejson_status status;

  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__emit_cstr(sink, user, error, "null");
  }
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__source_cursor_close(&cursor);
    return status;
  }
  for (;;) {
    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source text");
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson__emit_escaped_fragment(sink, user, error, buffer,
                                               chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_source_base64(const lonejson_source *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  unsigned char buffer[4096];
  unsigned char carry[3];
  size_t carry_len;
  lonejson__source_cursor cursor;
  lonejson_read_result chunk;
  lonejson_status status;

  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__emit_cstr(sink, user, error, "null");
  }
  carry_len = 0u;
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__source_cursor_close(&cursor);
    return status;
  }
  for (;;) {
    size_t offset;

    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source bytes");
    }
    offset = 0u;
    if (carry_len != 0u) {
      while (carry_len < 3u && offset < chunk.bytes_read) {
        carry[carry_len++] = buffer[offset++];
      }
      if (carry_len == 3u) {
        status = lonejson__emit_base64_bytes(sink, user, error, carry, 3u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          lonejson__source_cursor_close(&cursor);
          return status;
        }
        carry_len = 0u;
      }
    }
    while (offset + 3u <= chunk.bytes_read) {
      status =
          lonejson__emit_base64_bytes(sink, user, error, buffer + offset, 3u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
      offset += 3u;
    }
    while (offset < chunk.bytes_read) {
      carry[carry_len++] = buffer[offset++];
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  if (carry_len != 0u) {
    status = lonejson__emit_base64_bytes(sink, user, error, carry, carry_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_value_pretty(const lonejson_field *field, const void *ptr,
                                 lonejson__write_state *state) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__emit_cstr(state->sink, state->user, state->error,
                                   "null");
      }
      return lonejson__emit_escaped_string(state->sink, state->user,
                                           state->error, text);
    }
    return lonejson__emit_escaped_string(state->sink, state->user, state->error,
                                         (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text(
        (const lonejson_spooled *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64(
        (const lonejson_spooled *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text(
        (const lonejson_source *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64(
        (const lonejson_source *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson__json_transcode((const lonejson_json_value *)ptr,
                                    state->sink, state->user, state->error, 1,
                                    state->depth + 1u);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(state->sink, state->user, state->error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(state->sink, state->user, state->error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 state->error ? state->error->offset : 0u,
                                 state->error ? state->error->line : 0u,
                                 state->error ? state->error->column : 0u,
                                 "non-finite double cannot be serialized");
    }
    return lonejson__emit_f64_value(state->sink, state->user, state->error,
                                    *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__emit_cstr(state->sink, state->user, state->error,
                               *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_pretty(field->submap, ptr, state);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status =
          lonejson__emit_escaped_string(state->sink, state->user, state->error,
                                        arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
    return lonejson__serialize_parse_only_field_error(field, state->error);
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            state->error, LONEJSON_STATUS_TYPE_MISMATCH,
            state->error ? state->error->offset : 0u,
            state->error ? state->error->line : 0u,
            state->error ? state->error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status = lonejson__emit_f64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_cstr(state->sink, state->user, state->error,
                                   arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      ++state->depth;
      status = lonejson__serialize_map_pretty(field->submap, elem, state);
      --state->depth;
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  default:
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               state->error ? state->error->offset : 0u,
                               state->error ? state->error->line : 0u,
                               state->error ? state->error->column : 0u,
                               "unsupported field kind");
  }
}

static lonejson_status lonejson__serialize_map_compact(const lonejson_map *map,
                                                       const void *src,
                                                       lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error) {
  size_t i;
  size_t emitted;
  lonejson_status status;

  emitted = 0u;
  status = lonejson__emit_cstr(sink, user, error, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0; i < map->field_count; ++i) {
    if (lonejson__field_should_omit(src, &map->fields[i])) {
      continue;
    }
    if (emitted != 0u) {
      status = lonejson__emit_cstr(sink, user, error, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    status =
        lonejson__emit_escaped_key(sink, user, error, map->fields[i].json_key,
                                   map->fields[i].json_key_len, 0);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__serialize_value_compact(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), sink, user,
        error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    emitted++;
  }
  return lonejson__emit_cstr(sink, user, error, "}");
}

static lonejson_status
lonejson__serialize_value_compact(const lonejson_field *field, const void *ptr,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__emit_cstr(sink, user, error, "null");
      }
      return lonejson__emit_escaped_string(sink, user, error, text);
    }
    return lonejson__emit_escaped_string(sink, user, error, (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text((const lonejson_spooled *)ptr, sink,
                                            user, error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64((const lonejson_spooled *)ptr,
                                              sink, user, error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text((const lonejson_source *)ptr, sink,
                                           user, error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64((const lonejson_source *)ptr, sink,
                                             user, error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson__json_transcode((const lonejson_json_value *)ptr, sink,
                                    user, error, 0, 0u);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(sink, user, error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(sink, user, error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "non-finite double cannot be serialized");
    }
    return lonejson__emit_f64_value(sink, user, error, *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__emit_cstr(sink, user, error,
                               *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_compact(field->submap, ptr, sink, user,
                                           error);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_escaped_string(
          sink, user, error, arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
    return lonejson__serialize_parse_only_field_error(field, error);
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
            error ? error->line : 0u, error ? error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status = lonejson__emit_f64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_cstr(sink, user, error,
                                   arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__serialize_map_compact(field->submap, elem, sink, user,
                                               error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  default:
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "unsupported field kind");
  }
}

static lonejson_status lonejson__serialize_map_compact_buffer_exact(
    const lonejson_map *map, const void *src, lonejson_buffer_sink *sink,
    lonejson_error *error) {
  size_t i;
  size_t emitted;
  lonejson_status status;

  emitted = 0u;
  status = lonejson__buffer_emit_cstr_exact(sink, error, "{");
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (i = 0; i < map->field_count; ++i) {
    if (lonejson__field_should_omit(src, &map->fields[i])) {
      continue;
    }
    if (emitted != 0u) {
      status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    status = lonejson__buffer_emit_escaped_len_exact(
        sink, error, map->fields[i].json_key, map->fields[i].json_key_len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__buffer_emit_cstr_exact(sink, error, ":");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__serialize_value_compact_buffer_exact(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), sink,
        error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    emitted++;
  }
  return lonejson__buffer_emit_cstr_exact(sink, error, "}");
}

static lonejson_status lonejson__serialize_value_compact_buffer_exact(
    const lonejson_field *field, const void *ptr, lonejson_buffer_sink *sink,
    lonejson_error *error) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__buffer_emit_cstr_exact(sink, error, "null");
      }
      return lonejson__buffer_emit_escaped_string_exact(sink, error, text);
    }
    return lonejson__buffer_emit_escaped_string_exact(sink, error,
                                                      (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text((const lonejson_spooled *)ptr,
                                            lonejson__sink_buffer_exact, sink,
                                            error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64((const lonejson_spooled *)ptr,
                                              lonejson__sink_buffer_exact, sink,
                                              error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text(
        (const lonejson_source *)ptr, lonejson__sink_buffer_exact, sink, error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64(
        (const lonejson_source *)ptr, lonejson__sink_buffer_exact, sink, error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson__json_transcode((const lonejson_json_value *)ptr,
                                    lonejson__sink_buffer_exact, sink, error, 0,
                                    0u);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__buffer_emit_i64_value_exact(sink, error,
                                                 *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__buffer_emit_u64_value_exact(sink, error,
                                                 *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "non-finite double cannot be serialized");
    }
    return lonejson__buffer_emit_f64_value_exact(sink, error,
                                                 *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__buffer_emit_cstr_exact(
        sink, error, *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_compact_buffer_exact(field->submap, ptr,
                                                        sink, error);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__buffer_emit_cstr_exact(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__buffer_emit_escaped_string_exact(
          sink, error, arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_exact(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
    return lonejson__serialize_parse_only_field_error(field, error);
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__buffer_emit_cstr_exact(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status =
          lonejson__buffer_emit_i64_value_exact(sink, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_exact(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__buffer_emit_cstr_exact(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status =
          lonejson__buffer_emit_u64_value_exact(sink, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_exact(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__buffer_emit_cstr_exact(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
            error ? error->line : 0u, error ? error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status =
          lonejson__buffer_emit_f64_value_exact(sink, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_exact(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__buffer_emit_cstr_exact(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__buffer_emit_cstr_exact(
          sink, error, arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_exact(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__buffer_emit_cstr_exact(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_exact(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__serialize_map_compact_buffer_exact(field->submap, elem,
                                                            sink, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_exact(sink, error, "]");
  }
  default:
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "unsupported field kind");
  }
}

static lonejson_status lonejson__serialize_map_compact_buffer_grow(
    const lonejson_map *map, const void *src, lonejson_buffer_sink *sink,
    lonejson_error *error) {
  size_t i;
  size_t emitted;
  lonejson_status status;

  emitted = 0u;
  status = lonejson__buffer_emit_cstr_grow(sink, error, "{");
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (i = 0; i < map->field_count; ++i) {
    if (lonejson__field_should_omit(src, &map->fields[i])) {
      continue;
    }
    if (emitted != 0u) {
      status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    status = lonejson__emit_escaped_len(lonejson__sink_grow, sink, error,
                                        map->fields[i].json_key,
                                        map->fields[i].json_key_len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__buffer_emit_cstr_grow(sink, error, ":");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__serialize_value_compact_buffer_grow(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), sink,
        error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    emitted++;
  }
  return lonejson__buffer_emit_cstr_grow(sink, error, "}");
}

static lonejson_status lonejson__serialize_value_compact_buffer_grow(
    const lonejson_field *field, const void *ptr, lonejson_buffer_sink *sink,
    lonejson_error *error) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__buffer_emit_cstr_grow(sink, error, "null");
      }
      return lonejson__emit_escaped_string(lonejson__sink_grow, sink, error,
                                           text);
    }
    return lonejson__emit_escaped_string(lonejson__sink_grow, sink, error,
                                         (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text((const lonejson_spooled *)ptr,
                                            lonejson__sink_grow, sink, error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64((const lonejson_spooled *)ptr,
                                              lonejson__sink_grow, sink, error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text((const lonejson_source *)ptr,
                                           lonejson__sink_grow, sink, error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64((const lonejson_source *)ptr,
                                             lonejson__sink_grow, sink, error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson__json_transcode((const lonejson_json_value *)ptr,
                                    lonejson__sink_grow, sink, error, 0, 0u);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(lonejson__sink_grow, sink, error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(lonejson__sink_grow, sink, error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "non-finite double cannot be serialized");
    }
    return lonejson__emit_f64_value(lonejson__sink_grow, sink, error,
                                    *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__buffer_emit_cstr_grow(
        sink, error, *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_compact_buffer_grow(field->submap, ptr, sink,
                                                       error);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__buffer_emit_cstr_grow(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__emit_escaped_string(
          lonejson__sink_grow, sink, error, arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_grow(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
    return lonejson__serialize_parse_only_field_error(field, error);
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__buffer_emit_cstr_grow(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(lonejson__sink_grow, sink, error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_grow(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__buffer_emit_cstr_grow(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(lonejson__sink_grow, sink, error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_grow(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__buffer_emit_cstr_grow(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
            error ? error->line : 0u, error ? error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status = lonejson__emit_f64_value(lonejson__sink_grow, sink, error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_grow(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__buffer_emit_cstr_grow(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__buffer_emit_cstr_grow(
          sink, error, arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_grow(sink, error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__buffer_emit_cstr_grow(sink, error, "[");
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status = lonejson__buffer_emit_cstr_grow(sink, error, ",");
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      status = lonejson__serialize_map_compact_buffer_grow(field->submap, elem,
                                                           sink, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    return lonejson__buffer_emit_cstr_grow(sink, error, "]");
  }
  default:
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "unsupported field kind");
  }
}
