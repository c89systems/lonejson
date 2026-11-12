static LONEJSON__INLINE lonejson_status lonejson__sink_buffer(
    void *user, const void *data, size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  size_t available =
      (sink->capacity > sink->length) ? (sink->capacity - sink->length) : 0u;
  size_t writable = (available > 0u) ? (available - 1u) : 0u;

  sink->needed += len;
  if (available > 0u) {
    size_t copy_len = (len < writable) ? len : writable;
    if (copy_len > 0u) {
      memcpy(sink->buffer + sink->length, data, copy_len);
      sink->length += copy_len;
    }
  }
  if (len > writable) {
    sink->truncated = 1;
    if (sink->policy == LONEJSON_OVERFLOW_FAIL) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "output buffer too small");
    }
    return (sink->policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__sink_buffer_exact(
    void *user, const void *data, size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  size_t available =
      (sink->capacity > sink->length) ? (sink->capacity - sink->length) : 0u;
  size_t writable = (available > 0u) ? (available - 1u) : 0u;

  sink->needed += len;
  if (len > writable) {
    sink->truncated = 1;
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too small");
  }
  if (len != 0u) {
    if (len == 1u)
      sink->buffer[sink->length] = *(const char *)data;
    else
      memcpy(sink->buffer + sink->length, data, len);
    sink->length += len;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__sink_grow(
    void *user, const void *data, size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  char *next;
  size_t next_cap;
  size_t alloc_required;
  size_t payload_required;
  size_t max_alloc;

  (void)error;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (len > (SIZE_MAX - sink->length)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too large");
  }
  payload_required = sink->length + len;
  if (payload_required == SIZE_MAX) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too large");
  }
  if (sink->max_bytes != 0u && payload_required > sink->max_bytes) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "serializer-owned output exceeds max_output_bytes");
  }
  alloc_required = payload_required + 1u;
  if (alloc_required > sink->capacity) {
    next_cap = (sink->capacity == 0u) ? 2048u : sink->capacity;
    while (next_cap < alloc_required) {
      if (next_cap > (SIZE_MAX / 2u)) {
        next_cap = alloc_required;
        break;
      }
      next_cap *= 2u;
    }
    max_alloc = sink->max_bytes != 0u ? sink->max_bytes + 1u : 0u;
    if (sink->max_bytes != 0u && max_alloc == 0u) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "output buffer too large");
    }
    if (max_alloc != 0u && next_cap > max_alloc) {
      next_cap = max_alloc;
    }
    next = (char *)lonejson__buffer_realloc(sink->allocator, sink->buffer,
                                            sink->alloc_size, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to grow output buffer");
    }
    sink->buffer = next;
    sink->capacity = next_cap;
    sink->alloc_size = next_cap;
  }
  if (len == 1u)
    sink->buffer[sink->length] = *(const char *)data;
  else
    memcpy(sink->buffer + sink->length, data, len);
  sink->length += len;
  sink->needed = sink->length;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sink_file(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  FILE *fp = (FILE *)user;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (fwrite(data, 1u, len, fp) != len) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(
        error, LONEJSON_STATUS_IO_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to write JSON output");
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__emit(lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error,
                                                       const char *data,
                                                       size_t len) {
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  return sink(user, data, len, error);
}

static LONEJSON__INLINE lonejson_status
lonejson__emit_cstr(lonejson_sink_fn sink, void *user, lonejson_error *error,
                    const char *text) {
  return lonejson__emit(sink, user, error, text, LONEJSON__TEXT_LEN(text));
}

static lonejson_status
lonejson__emit_escaped_fragment(lonejson_sink_fn sink, void *user,
                                lonejson_error *error,
                                const unsigned char *text, size_t len) {
  static const char hex[] = "0123456789abcdef";
  size_t i;
  size_t start;
  lonejson_status status;

  start = 0u;
  for (i = 0u; i < len; ++i) {
    char escape_buf[7];
    if (text[i] >= 0x20u && text[i] != '"' && text[i] != '\\') {
      continue;
    }
    if (start != i) {
      status = lonejson__emit(sink, user, error, (const char *)(text + start),
                              i - start);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    switch (text[i]) {
    case '"':
      status = lonejson__emit_cstr(sink, user, error, "\\\"");
      break;
    case '\\':
      status = lonejson__emit_cstr(sink, user, error, "\\\\");
      break;
    case '\b':
      status = lonejson__emit_cstr(sink, user, error, "\\b");
      break;
    case '\f':
      status = lonejson__emit_cstr(sink, user, error, "\\f");
      break;
    case '\n':
      status = lonejson__emit_cstr(sink, user, error, "\\n");
      break;
    case '\r':
      status = lonejson__emit_cstr(sink, user, error, "\\r");
      break;
    case '\t':
      status = lonejson__emit_cstr(sink, user, error, "\\t");
      break;
    default:
      if (text[i] < 0x20u) {
        escape_buf[0] = '\\';
        escape_buf[1] = 'u';
        escape_buf[2] = '0';
        escape_buf[3] = '0';
        escape_buf[4] = hex[(text[i] >> 4u) & 0x0Fu];
        escape_buf[5] = hex[text[i] & 0x0Fu];
        escape_buf[6] = '\0';
        status = lonejson__emit(sink, user, error, escape_buf, 6u);
      } else {
        status = LONEJSON_STATUS_OK;
      }
      break;
    }
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    start = i + 1u;
  }
  if (start != len) {
    status = lonejson__emit(sink, user, error, (const char *)(text + start),
                            len - start);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__emit_escaped_bytes(lonejson_sink_fn sink, void *user,
                             lonejson_error *error, const unsigned char *text,
                             size_t len, const char *closing,
                             size_t closing_len) {
  lonejson_status status;
  size_t i;

  i = lonejson__plain_string_span(text, len, 0);
  if (i == len) {
    status = lonejson__emit_cstr(sink, user, error, "\"");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit(sink, user, error, (const char *)text, len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    return lonejson__emit(sink, user, error, closing, closing_len);
  }

  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__emit_escaped_fragment(sink, user, error, text, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__emit(sink, user, error, closing, closing_len);
}

static lonejson_status lonejson__emit_escaped_string(lonejson_sink_fn sink,
                                                     void *user,
                                                     lonejson_error *error,
                                                     const char *text) {
  return lonejson__emit_escaped_bytes(sink, user, error,
                                      (const unsigned char *)text,
                                      LONEJSON__TEXT_LEN(text), "\"", 1u);
}

static LONEJSON__INLINE lonejson_status lonejson__emit_escaped_len(
    lonejson_sink_fn sink, void *user, lonejson_error *error, const char *text,
    size_t len) {
  return lonejson__emit_escaped_bytes(
      sink, user, error, (const unsigned char *)text, len, "\"", 1u);
}

/* A key's closing quote and separator are one literal sink chunk. */
static LONEJSON__INLINE lonejson_status lonejson__emit_escaped_key(
    lonejson_sink_fn sink, void *user, lonejson_error *error, const char *text,
    size_t len, int pretty) {
  return lonejson__emit_escaped_bytes(
      sink, user, error, (const unsigned char *)text, len,
      pretty ? "\": " : "\":", pretty ? 3u : 2u);
}

static LONEJSON__INLINE lonejson_status
lonejson__buffer_emit_exact(lonejson_buffer_sink *sink, lonejson_error *error,
                            const void *data, size_t len) {
  size_t available =
      (sink->capacity > sink->length) ? (sink->capacity - sink->length) : 0u;
  size_t writable = (available > 0u) ? (available - 1u) : 0u;

  sink->needed += len;
  if (len > writable) {
    sink->truncated = 1;
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too small");
  }
  if (len != 0u) {
    memcpy(sink->buffer + sink->length, data, len);
    sink->length += len;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__buffer_emit_cstr_exact(
    lonejson_buffer_sink *sink, lonejson_error *error, const char *text) {
  return lonejson__buffer_emit_exact(sink, error, text,
                                     LONEJSON__TEXT_LEN(text));
}

static LONEJSON__INLINE lonejson_status lonejson__buffer_reserve_grow(
    lonejson_buffer_sink *sink, lonejson_error *error, size_t len) {
  char *next;
  size_t next_cap;
  size_t alloc_required;
  size_t payload_required;
  size_t max_alloc;

  if (len > (SIZE_MAX - sink->length)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too large");
  }
  payload_required = sink->length + len;
  if (payload_required == SIZE_MAX) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too large");
  }
  if (sink->max_bytes != 0u && payload_required > sink->max_bytes) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "serializer-owned output exceeds max_output_bytes");
  }
  alloc_required = payload_required + 1u;
  if (alloc_required <= sink->capacity) {
    return LONEJSON_STATUS_OK;
  }
  next_cap = (sink->capacity == 0u) ? 2048u : sink->capacity;
  while (next_cap < alloc_required) {
    if (next_cap > (SIZE_MAX / 2u)) {
      next_cap = alloc_required;
      break;
    }
    next_cap *= 2u;
  }
  max_alloc = sink->max_bytes != 0u ? sink->max_bytes + 1u : 0u;
  if (sink->max_bytes != 0u && max_alloc == 0u) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too large");
  }
  if (max_alloc != 0u && next_cap > max_alloc) {
    next_cap = max_alloc;
  }
  next = (char *)lonejson__buffer_realloc(sink->allocator, sink->buffer,
                                          sink->alloc_size, next_cap);
  if (next == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to grow output buffer");
  }
  sink->buffer = next;
  sink->capacity = next_cap;
  sink->alloc_size = next_cap;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__buffer_emit_grow(lonejson_buffer_sink *sink, lonejson_error *error,
                           const void *data, size_t len) {
  lonejson_status status;

  sink->needed += len;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__buffer_reserve_grow(sink, error, len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  memcpy(sink->buffer + sink->length, data, len);
  sink->length += len;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__buffer_emit_cstr_grow(
    lonejson_buffer_sink *sink, lonejson_error *error, const char *text) {
  return lonejson__buffer_emit_grow(sink, error, text,
                                    LONEJSON__TEXT_LEN(text));
}

static lonejson_status lonejson__buffer_emit_escaped_fragment_exact(
    lonejson_buffer_sink *sink, lonejson_error *error,
    const unsigned char *text, size_t len) {
  static const char hex[] = "0123456789abcdef";
  size_t i;
  size_t start;
  lonejson_status status;

  start = 0u;
  i = 0u;
  while (i < len) {
    char escape_buf[7];
    i += lonejson__plain_string_span_bulk(text + i, len - i, 0);
    if (i == len)
      break;
    if (start != i) {
      status =
          lonejson__buffer_emit_exact(sink, error, text + start, i - start);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    switch (text[i]) {
    case '"':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\\"");
      break;
    case '\\':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\\\");
      break;
    case '\b':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\b");
      break;
    case '\f':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\f");
      break;
    case '\n':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\n");
      break;
    case '\r':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\r");
      break;
    case '\t':
      status = lonejson__buffer_emit_cstr_exact(sink, error, "\\t");
      break;
    default:
      if (text[i] < 0x20u) {
        escape_buf[0] = '\\';
        escape_buf[1] = 'u';
        escape_buf[2] = '0';
        escape_buf[3] = '0';
        escape_buf[4] = hex[(text[i] >> 4u) & 0x0Fu];
        escape_buf[5] = hex[text[i] & 0x0Fu];
        escape_buf[6] = '\0';
        status = lonejson__buffer_emit_exact(sink, error, escape_buf, 6u);
      } else {
        status = LONEJSON_STATUS_OK;
      }
      break;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    start = i + 1u;
    ++i;
  }
  if (start != len) {
    status =
        lonejson__buffer_emit_exact(sink, error, text + start, len - start);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__buffer_emit_escaped_bytes_exact(
    lonejson_buffer_sink *sink, lonejson_error *error,
    const unsigned char *text, size_t len) {
  lonejson_status status;

  status = lonejson__buffer_emit_cstr_exact(sink, error, "\"");
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__buffer_emit_escaped_fragment_exact(sink, error, text, len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__buffer_emit_cstr_exact(sink, error, "\"");
}

static LONEJSON__INLINE lonejson_status
lonejson__buffer_emit_escaped_string_exact(lonejson_buffer_sink *sink,
                                           lonejson_error *error,
                                           const char *text) {
  return lonejson__buffer_emit_escaped_bytes_exact(
      sink, error, (const unsigned char *)text, LONEJSON__TEXT_LEN(text));
}

static LONEJSON__INLINE lonejson_status lonejson__buffer_emit_escaped_len_exact(
    lonejson_buffer_sink *sink, lonejson_error *error, const char *text,
    size_t len) {
  return lonejson__buffer_emit_escaped_bytes_exact(
      sink, error, (const unsigned char *)text, len);
}
