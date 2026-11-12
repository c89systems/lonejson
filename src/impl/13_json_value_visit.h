
static lonejson_status lonejson__json_visit_event(lonejson__json_io *io,
                                                  lonejson_value_event_fn fn) {
  lonejson_status status;

  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = fn(io->visitor_user, io->error);
  return status;
}

static const lonejson_value_path *
lonejson__json_current_path(lonejson__json_io *io, lonejson_value_path *path) {
  path->segments = io->path_segments;
  path->segment_count = io->path_depth;
  return path;
}

static lonejson_status
lonejson__json_visit_path_event(lonejson__json_io *io,
                                lonejson_path_value_event_fn fn) {
  lonejson_value_path path;

  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, lonejson__json_current_path(io, &path),
            io->error);
}

static lonejson_status
lonejson__json_visit_any_event(lonejson__json_io *io,
                               lonejson_value_event_fn fn,
                               lonejson_path_value_event_fn path_fn) {
  lonejson_status status;

  if (io->path_visitor == NULL) {
    return lonejson__json_visit_event(io, fn);
  }
  status = lonejson__json_visit_event(io, fn);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__json_visit_path_event(io, path_fn);
}

static lonejson_status lonejson__json_visit_chunk(lonejson__json_io *io,
                                                  lonejson_value_chunk_fn fn,
                                                  const char *data,
                                                  size_t len) {
  if (len == 0u || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, data, len, io->error);
}

static lonejson_status
lonejson__json_visit_path_chunk(lonejson__json_io *io,
                                lonejson_path_value_chunk_fn fn,
                                const char *data, size_t len) {
  lonejson_value_path path;

  if (len == 0u || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, lonejson__json_current_path(io, &path), data, len,
            io->error);
}

static lonejson_status lonejson__json_path_key_append(lonejson__json_io *io,
                                                      const char *data,
                                                      size_t len) {
  lonejson__json_path_frame *frame;
  size_t new_len;
  size_t new_cap;
  char *next;

  if (len == 0u || io->path_frames == NULL ||
      io->path_depth >= io->path_capacity) {
    return LONEJSON_STATUS_OK;
  }
  frame = &io->path_frames[io->path_depth];
  if (frame->key == NULL) {
    frame->key = frame->inline_key;
    frame->key_cap = sizeof(frame->inline_key);
  }
  if (len > ((size_t)-1) - frame->key_len - 1u) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
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
    next = (char *)lonejson__owned_malloc(io->allocator, new_cap);
    if (next == NULL) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                 0u, 0u, 0u,
                                 "failed to allocate JSON path key buffer");
    }
    if (frame->key_len != 0u) {
      memcpy(next, frame->key, frame->key_len);
    }
    if (frame->key_heap) {
      lonejson__owned_free(frame->key);
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

static lonejson_status lonejson__json_visit_any_chunk(
    lonejson__json_io *io, int is_key, lonejson_value_chunk_fn fn,
    lonejson_path_value_chunk_fn path_fn, const char *data, size_t len) {
  lonejson_status status;

  if (io->path_visitor == NULL) {
    (void)is_key;
    (void)path_fn;
    return lonejson__json_visit_chunk(io, fn, data, len);
  }
  if (is_key && io->path_visitor != NULL) {
    status = lonejson__json_path_key_append(io, data, len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  status = lonejson__json_visit_chunk(io, fn, data, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__json_visit_path_chunk(io, path_fn, data, len);
}

static lonejson_status lonejson__json_visit_bool(lonejson__json_io *io,
                                                 int value) {
  lonejson_value_path path;
  lonejson_status status;

  if (io->visitor == NULL || io->visitor->boolean_value == NULL) {
    status = LONEJSON_STATUS_OK;
  } else {
    status = io->visitor->boolean_value(io->visitor_user, value, io->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (io->path_visitor == NULL || io->path_visitor->boolean_value == NULL) {
    return status;
  }
  return io->path_visitor->boolean_value(io->visitor_user,
                                         lonejson__json_current_path(io, &path),
                                         value, io->error);
}

static const unsigned char *
lonejson__json_cursor_plain_span(lonejson__json_io *io, size_t *available,
                                 int *uses_read_buffer) {
  if (available != NULL) {
    *available = 0u;
  }
  if (uses_read_buffer != NULL) {
    *uses_read_buffer = 0;
  }
  if (io->has_pushback) {
    return NULL;
  }
  if (io->cursor->buffer != NULL &&
      io->cursor->buffer_off < io->cursor->buffer_len) {
    if (available != NULL) {
      *available = io->cursor->buffer_len - io->cursor->buffer_off;
    }
    return io->cursor->buffer + io->cursor->buffer_off;
  }
  if (io->cursor->read_buffer_off < io->cursor->read_buffer_len) {
    if (available != NULL) {
      *available = io->cursor->read_buffer_len - io->cursor->read_buffer_off;
    }
    if (uses_read_buffer != NULL) {
      *uses_read_buffer = 1;
    }
    return lonejson__json_cursor_read_buffer(io->cursor) +
           io->cursor->read_buffer_off;
  }
  return NULL;
}

static lonejson_status lonejson__json_visit_plain_chunk_no_path(
    lonejson__json_io *io, int is_key, lonejson_value_chunk_fn fn,
    unsigned char *plain, size_t *plain_len, size_t *decoded_bytes,
    size_t limit, const unsigned char *data, size_t len) {
  lonejson_status status;
  size_t take;
  size_t offset = 0u;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (io->limits.max_total_bytes != 0u &&
      io->total_bytes + len > io->limits.max_total_bytes) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value exceeds maximum total byte limit");
  }
  io->total_bytes += len;
  *decoded_bytes += len;
  if (limit != 0u && *decoded_bytes > limit) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               is_key ? "JSON object key exceeds maximum "
                                        "decoded byte limit"
                                      : "JSON string exceeds maximum decoded "
                                        "byte limit");
  }
  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (*plain_len == 0u) {
    return fn(io->visitor_user, (const char *)data, len, io->error);
  }
  while (offset < len) {
    take = len - offset;
    if (take > 256u) {
      take = 256u;
    }
    if (take > (256u - *plain_len)) {
      take = 256u - *plain_len;
    }
    memcpy(plain + *plain_len, data + offset, take);
    *plain_len += take;
    offset += take;
    if (*plain_len == sizeof(unsigned char[256])) {
      status = fn(io->visitor_user, (const char *)plain, *plain_len, io->error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      *plain_len = 0u;
    }
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__HOT lonejson_status
lonejson__json_visit_value_no_path(lonejson__json_io *io);

static lonejson_status
lonejson__json_visit_string_value_no_path(lonejson__json_io *io, int is_key) {
  unsigned char plain[256];
  size_t plain_len = 0u;
  size_t decoded_bytes = 0u;
  int ch;
  lonejson_status status;
  size_t limit =
      is_key ? io->limits.max_key_bytes : io->limits.max_string_bytes;
  lonejson_value_event_fn begin_fn =
      is_key ? io->visitor->object_key_begin : io->visitor->string_begin;
  lonejson_value_chunk_fn chunk_fn =
      is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk;
  lonejson_value_event_fn end_fn =
      is_key ? io->visitor->object_key_end : io->visitor->string_end;

  if (begin_fn != NULL) {
    status = begin_fn(io->visitor_user, io->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  for (;;) {
    const unsigned char *span;
    size_t available;
    size_t plain_span = 0u;
    int uses_read_buffer = 0;

    span = lonejson__json_cursor_plain_span(io, &available, &uses_read_buffer);
    while (span != NULL && available != 0u) {
      plain_span = lonejson__plain_string_span_words(span, available, 0);
      if (plain_span != 0u) {
        status = lonejson__json_visit_plain_chunk_no_path(
            io, is_key, chunk_fn, plain, &plain_len, &decoded_bytes, limit,
            span, plain_span);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        if (uses_read_buffer) {
          io->cursor->read_buffer_off += plain_span;
        } else {
          io->cursor->buffer_off += plain_span;
        }
        if (plain_span == available) {
          span = lonejson__json_cursor_plain_span(io, &available,
                                                  &uses_read_buffer);
          plain_span = 0u;
          continue;
        }
      }
      break;
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON string");
    }
    if (ch == '"') {
      if (plain_len != 0u && chunk_fn != NULL) {
        status = chunk_fn(io->visitor_user, (const char *)plain, plain_len,
                          io->error);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (end_fn == NULL) {
        return LONEJSON_STATUS_OK;
      }
      return end_fn(io->visitor_user, io->error);
    }
    if (ch < 0x20) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "control character in string");
    }
    if (ch != '\\') {
      plain[plain_len++] = (unsigned char)ch;
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u,
                                   is_key ? "JSON object key exceeds maximum "
                                            "decoded byte limit"
                                          : "JSON string exceeds maximum "
                                            "decoded byte limit");
      }
      if (plain_len == sizeof(plain)) {
        if (chunk_fn != NULL) {
          status = chunk_fn(io->visitor_user, (const char *)plain, plain_len,
                            io->error);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            return status;
          }
        }
        plain_len = 0u;
      }
      continue;
    }

    if (plain_len != 0u) {
      if (chunk_fn != NULL) {
        status = chunk_fn(io->visitor_user, (const char *)plain, plain_len,
                          io->error);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      plain_len = 0u;
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated escape sequence");
    }
    switch (ch) {
    case '"':
    case '\\':
    case '/': {
      char out = (char)ch;
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      if (chunk_fn != NULL) {
        status = chunk_fn(io->visitor_user, &out, 1u, io->error);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      break;
    }
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't': {
      char out = (ch == 'b')   ? '\b'
                 : (ch == 'f') ? '\f'
                 : (ch == 'n') ? '\n'
                 : (ch == 'r') ? '\r'
                               : '\t';
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      if (chunk_fn != NULL) {
        status = chunk_fn(io->visitor_user, &out, 1u, io->error);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      break;
    }
    case 'u': {
      int i;
      lonejson_uint32 cp = 0u;
      unsigned char utf8[6];
      lonejson_scratch scratch;
      for (i = 0; i < 4; ++i) {
        int hx = lonejson__json_cursor_getc(io);
        int hv;
        if (hx == -2) {
          return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
        }
        if (hx == EOF) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u, "invalid unicode escape");
        }
        hv = lonejson__hex_value(hx);
        if (hv < 0) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u, "invalid unicode escape");
        }
        cp = (cp << 4u) | (lonejson_uint32)hv;
      }
      if (cp >= 0xD800u && cp <= 0xDBFFu) {
        lonejson_uint32 low = 0u;
        int hx;
        if (lonejson__json_cursor_getc(io) != '\\' ||
            lonejson__json_cursor_getc(io) != 'u') {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u,
                                     "invalid unicode surrogate pair");
        }
        for (i = 0; i < 4; ++i) {
          int hv;
          hx = lonejson__json_cursor_getc(io);
          if (hx == -2) {
            return io->error ? io->error->code
                             : LONEJSON_STATUS_CALLBACK_FAILED;
          }
          if (hx == EOF) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          hv = lonejson__hex_value(hx);
          if (hv < 0) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          low = (low << 4u) | (lonejson_uint32)hv;
        }
        if (low < 0xDC00u || low > 0xDFFFu) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u,
                                     "invalid unicode surrogate pair");
        }
        cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
      } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "unexpected low surrogate");
      }
      memset(&scratch, 0, sizeof(scratch));
      scratch.data = (char *)utf8;
      scratch.cap = sizeof(utf8);
      if (!lonejson__utf8_append(&scratch, cp)) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                   0u, 0u, 0u,
                                   "failed to encode unicode escape");
      }
      decoded_bytes += scratch.len;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      if (chunk_fn != NULL) {
        status = chunk_fn(io->visitor_user, (const char *)utf8, scratch.len,
                          io->error);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      break;
    }
    default:
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid string escape");
    }
  }
}

static lonejson_status
lonejson__json_visit_number_no_path(lonejson__json_io *io, int first) {
  char stack_buf[256];
  char *buffer = stack_buf;
  size_t capacity = io->limits.max_number_bytes + 1u;
  size_t len = 0u;
  int ch = first;
  lonejson_status status;

  if (capacity > sizeof(stack_buf)) {
    buffer = (char *)lonejson__owned_malloc(io->allocator, capacity);
    if (buffer == NULL) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                 0u, 0u, 0u,
                                 "failed to allocate JSON number buffer");
    }
  }
  status = lonejson__json_visit_event(io, io->visitor->number_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return status;
  }
  if (len >= io->limits.max_number_bytes) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON number exceeds maximum byte limit");
  }
  buffer[len++] = (char)first;
  ch = lonejson__json_cursor_getc_lookahead(io);
  while (ch >= 0 && (lonejson__is_digit(ch) || ch == '+' || ch == '-' ||
                     ch == '.' || ch == 'e' || ch == 'E')) {
    status = lonejson__json_enforce_total_limit(io);
    if (status != LONEJSON_STATUS_OK) {
      if (buffer != stack_buf) {
        lonejson__owned_free(buffer);
      }
      return status;
    }
    if (len >= io->limits.max_number_bytes) {
      if (buffer != stack_buf) {
        lonejson__owned_free(buffer);
      }
      return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                 0u, "JSON number exceeds maximum byte limit");
    }
    buffer[len++] = (char)ch;
    ch = lonejson__json_cursor_getc_lookahead(io);
  }
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  }
  if (ch == -2) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (!lonejson__is_valid_json_number(buffer, len)) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "invalid JSON number");
  }
  status =
      lonejson__json_visit_chunk(io, io->visitor->number_chunk, buffer, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson__json_visit_event(io, io->visitor->number_end);
  }
  if (buffer != stack_buf) {
    lonejson__owned_free(buffer);
  }
  return status;
}

static lonejson_status
lonejson__json_visit_literal_no_path(lonejson__json_io *io, int first,
                                     const char *rest, int kind) {
  size_t i;
  for (i = 0u; rest[i] != '\0'; ++i) {
    int ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch != (unsigned char)rest[i]) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid JSON literal");
    }
  }
  if (kind == 1) {
    return lonejson__json_visit_bool(io, first == 't');
  }
  return lonejson__json_visit_event(io, io->visitor->null_value);
}

static lonejson_status
lonejson__json_visit_array_no_path(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  if (io->depth >= io->limits.max_depth) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value nesting exceeds maximum depth");
  }
  status = lonejson__json_visit_event(io, io->visitor->array_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON array");
    }
    if (ch == ']') {
      (void)lonejson__json_cursor_getc(io);
      return lonejson__json_visit_event(io, io->visitor->array_end);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in array");
      }
    }
    ++io->depth;
    status = lonejson__json_visit_value_no_path(io);
    --io->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static lonejson_status
lonejson__json_visit_object_no_path(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  if (io->depth >= io->limits.max_depth) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value nesting exceeds maximum depth");
  }
  status = lonejson__json_visit_event(io, io->visitor->object_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON object");
    }
    if (ch == '}') {
      (void)lonejson__json_cursor_getc(io);
      return lonejson__json_visit_event(io, io->visitor->object_end);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in object");
      }
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != '"') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected object key");
    }
    (void)lonejson__json_cursor_getc(io);
    status = lonejson__json_visit_string_value_no_path(io, 1);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != ':') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected ':' after object key");
    }
    (void)lonejson__json_cursor_getc(io);
    ++io->depth;
    status = lonejson__json_visit_value_no_path(io);
    --io->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static LONEJSON__HOT lonejson_status
lonejson__json_visit_value_no_path(lonejson__json_io *io) {
  int ch = lonejson__json_peek_nonspace(io);

  if (ch == -2) {
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (ch == EOF) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
  ch = lonejson__json_cursor_getc(io);
  switch (ch) {
  case '"':
    return lonejson__json_visit_string_value_no_path(io, 0);
  case '{':
    return lonejson__json_visit_object_no_path(io);
  case '[':
    return lonejson__json_visit_array_no_path(io);
  case 't':
    return lonejson__json_visit_literal_no_path(io, ch, "rue", 1);
  case 'f':
    return lonejson__json_visit_literal_no_path(io, ch, "alse", 1);
  case 'n':
    return lonejson__json_visit_literal_no_path(io, ch, "ull", 0);
  default:
    if (ch == '-' || lonejson__is_digit(ch)) {
      return lonejson__json_visit_number_no_path(io, ch);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
}

static lonejson_status
lonejson__json_visit_plain_chunk(lonejson__json_io *io, int is_key,
                                 unsigned char *plain, size_t *plain_len,
                                 size_t *decoded_bytes, size_t limit,
                                 const unsigned char *data, size_t len) {
  lonejson_status status;
  size_t take;
  size_t offset = 0u;
  lonejson_value_chunk_fn fn = io->visitor == NULL
                                   ? NULL
                                   : (is_key ? io->visitor->object_key_chunk
                                             : io->visitor->string_chunk);
  lonejson_path_value_chunk_fn path_fn =
      io->path_visitor == NULL ? NULL
                               : (is_key ? io->path_visitor->object_key_chunk
                                         : io->path_visitor->string_chunk);

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (io->limits.max_total_bytes != 0u &&
      io->total_bytes + len > io->limits.max_total_bytes) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value exceeds maximum total byte limit");
  }
  io->total_bytes += len;
  *decoded_bytes += len;
  if (limit != 0u && *decoded_bytes > limit) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               is_key ? "JSON object key exceeds maximum "
                                        "decoded byte limit"
                                      : "JSON string exceeds maximum decoded "
                                        "byte limit");
  }
  if (*plain_len == 0u) {
    return lonejson__json_visit_any_chunk(io, is_key, fn, path_fn,
                                          (const char *)data, len);
  }
  while (offset < len) {
    take = len - offset;
    if (take > 256u) {
      take = 256u;
    }
    if (take > (256u - *plain_len)) {
      take = 256u - *plain_len;
    }
    memcpy(plain + *plain_len, data + offset, take);
    *plain_len += take;
    offset += take;
    if (*plain_len == sizeof(unsigned char[256])) {
      status = lonejson__json_visit_any_chunk(io, is_key, fn, path_fn,
                                              (const char *)plain, *plain_len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      *plain_len = 0u;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__json_visit_string_value(lonejson__json_io *io,
                                                         int is_key) {
  unsigned char plain[256];
  size_t plain_len = 0u;
  size_t decoded_bytes = 0u;
  int ch;
  lonejson_status status;
  size_t limit =
      is_key ? io->limits.max_key_bytes : io->limits.max_string_bytes;
  lonejson_value_event_fn begin_fn =
      io->visitor == NULL ? NULL
                          : (is_key ? io->visitor->object_key_begin
                                    : io->visitor->string_begin);
  lonejson_value_event_fn end_fn =
      io->visitor == NULL
          ? NULL
          : (is_key ? io->visitor->object_key_end : io->visitor->string_end);
  lonejson_value_chunk_fn chunk_fn =
      io->visitor == NULL ? NULL
                          : (is_key ? io->visitor->object_key_chunk
                                    : io->visitor->string_chunk);
  lonejson_path_value_event_fn path_begin_fn =
      io->path_visitor == NULL ? NULL
                               : (is_key ? io->path_visitor->object_key_begin
                                         : io->path_visitor->string_begin);
  lonejson_path_value_event_fn path_end_fn =
      io->path_visitor == NULL ? NULL
                               : (is_key ? io->path_visitor->object_key_end
                                         : io->path_visitor->string_end);
  lonejson_path_value_chunk_fn path_chunk_fn =
      io->path_visitor == NULL ? NULL
                               : (is_key ? io->path_visitor->object_key_chunk
                                         : io->path_visitor->string_chunk);

  if (is_key && io->path_visitor != NULL && io->path_frames != NULL &&
      io->path_depth < io->path_capacity) {
    lonejson__json_path_frame *frame = &io->path_frames[io->path_depth];
    frame->key_len = 0u;
    if (frame->key == NULL) {
      frame->key = frame->inline_key;
      frame->key_cap = sizeof(frame->inline_key);
      frame->key_heap = 0;
    }
    frame->key[0] = '\0';
  }
  status = lonejson__json_visit_any_event(io, begin_fn, path_begin_fn);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    const unsigned char *span;
    size_t available;
    size_t plain_span = 0u;
    int uses_read_buffer = 0;

    span = lonejson__json_cursor_plain_span(io, &available, &uses_read_buffer);
    while (span != NULL && available != 0u) {
      plain_span = lonejson__plain_string_span_words(span, available, 0);
      if (plain_span != 0u) {
        status = lonejson__json_visit_plain_chunk(io, is_key, plain, &plain_len,
                                                  &decoded_bytes, limit, span,
                                                  plain_span);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        if (uses_read_buffer) {
          io->cursor->read_buffer_off += plain_span;
        } else {
          io->cursor->buffer_off += plain_span;
        }
        if (plain_span == available) {
          span = lonejson__json_cursor_plain_span(io, &available,
                                                  &uses_read_buffer);
          plain_span = 0u;
          continue;
        }
      }
      break;
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON string");
    }
    if (ch == '"') {
      if (plain_len != 0u) {
        status =
            lonejson__json_visit_any_chunk(io, is_key, chunk_fn, path_chunk_fn,
                                           (const char *)plain, plain_len);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      return lonejson__json_visit_any_event(io, end_fn, path_end_fn);
    }
    if (ch < 0x20) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "control character in string");
    }
    if (ch != '\\') {
      plain[plain_len++] = (unsigned char)ch;
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u,
                                   is_key ? "JSON object key exceeds maximum "
                                            "decoded byte limit"
                                          : "JSON string exceeds maximum "
                                            "decoded byte limit");
      }
      if (plain_len == sizeof(plain)) {
        status =
            lonejson__json_visit_any_chunk(io, is_key, chunk_fn, path_chunk_fn,
                                           (const char *)plain, plain_len);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        plain_len = 0u;
      }
      continue;
    }

    if (plain_len != 0u) {
      status = lonejson__json_visit_any_chunk(
          io, is_key, chunk_fn, path_chunk_fn, (const char *)plain, plain_len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      plain_len = 0u;
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated escape sequence");
    }
    switch (ch) {
    case '"':
    case '\\':
    case '/': {
      char out = (char)ch;
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_any_chunk(io, is_key, chunk_fn,
                                              path_chunk_fn, &out, 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      break;
    }
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't': {
      char out = (ch == 'b')   ? '\b'
                 : (ch == 'f') ? '\f'
                 : (ch == 'n') ? '\n'
                 : (ch == 'r') ? '\r'
                               : '\t';
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_any_chunk(io, is_key, chunk_fn,
                                              path_chunk_fn, &out, 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      break;
    }
    case 'u': {
      int i;
      lonejson_uint32 cp = 0u;
      /* lonejson__scratch_reserve keeps one byte of slack beyond the explicit
       * payload length, so a 4-byte UTF-8 sequence needs 6 bytes of storage
       * here rather than 5.
       */
      unsigned char utf8[6];
      lonejson_scratch scratch;
      for (i = 0; i < 4; ++i) {
        int hx = lonejson__json_cursor_getc(io);
        int hv;
        if (hx == -2) {
          return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
        }
        if (hx == EOF) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u, "invalid unicode escape");
        }
        hv = lonejson__hex_value(hx);
        if (hv < 0) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u, "invalid unicode escape");
        }
        cp = (cp << 4u) | (lonejson_uint32)hv;
      }
      if (cp >= 0xD800u && cp <= 0xDBFFu) {
        lonejson_uint32 low = 0u;
        int hx;
        if (lonejson__json_cursor_getc(io) != '\\' ||
            lonejson__json_cursor_getc(io) != 'u') {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u,
                                     "invalid unicode surrogate pair");
        }
        for (i = 0; i < 4; ++i) {
          int hv;
          hx = lonejson__json_cursor_getc(io);
          if (hx == -2) {
            return io->error ? io->error->code
                             : LONEJSON_STATUS_CALLBACK_FAILED;
          }
          if (hx == EOF) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          hv = lonejson__hex_value(hx);
          if (hv < 0) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          low = (low << 4u) | (lonejson_uint32)hv;
        }
        if (low < 0xDC00u || low > 0xDFFFu) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u,
                                     "invalid unicode surrogate pair");
        }
        cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
      } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "unexpected low surrogate");
      }
      memset(&scratch, 0, sizeof(scratch));
      scratch.data = (char *)utf8;
      scratch.cap = sizeof(utf8);
      if (!lonejson__utf8_append(&scratch, cp)) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                   0u, 0u, 0u,
                                   "failed to encode unicode escape");
      }
      decoded_bytes += scratch.len;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_any_chunk(
          io, is_key, chunk_fn, path_chunk_fn, (const char *)utf8, scratch.len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      break;
    }
    default:
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid string escape");
    }
  }
}

static lonejson_status lonejson__json_visit_number(lonejson__json_io *io,
                                                   int first) {
  char stack_buf[256];
  char *buffer = stack_buf;
  size_t capacity = io->limits.max_number_bytes + 1u;
  size_t len = 0u;
  int ch = first;
  lonejson_status status;
  lonejson_value_event_fn begin_fn =
      io->visitor == NULL ? NULL : io->visitor->number_begin;
  lonejson_value_chunk_fn chunk_fn =
      io->visitor == NULL ? NULL : io->visitor->number_chunk;
  lonejson_value_event_fn end_fn =
      io->visitor == NULL ? NULL : io->visitor->number_end;
  lonejson_path_value_event_fn path_begin_fn =
      io->path_visitor == NULL ? NULL : io->path_visitor->number_begin;
  lonejson_path_value_chunk_fn path_chunk_fn =
      io->path_visitor == NULL ? NULL : io->path_visitor->number_chunk;
  lonejson_path_value_event_fn path_end_fn =
      io->path_visitor == NULL ? NULL : io->path_visitor->number_end;

  if (capacity > sizeof(stack_buf)) {
    buffer = (char *)lonejson__owned_malloc(io->allocator, capacity);
    if (buffer == NULL) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                 0u, 0u, 0u,
                                 "failed to allocate JSON number buffer");
    }
  }
  status = lonejson__json_visit_any_event(io, begin_fn, path_begin_fn);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return status;
  }
  if (len >= io->limits.max_number_bytes) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON number exceeds maximum byte limit");
  }
  buffer[len++] = (char)first;
  ch = lonejson__json_cursor_getc_lookahead(io);
  while (ch >= 0 && (lonejson__is_digit(ch) || ch == '+' || ch == '-' ||
                     ch == '.' || ch == 'e' || ch == 'E')) {
    status = lonejson__json_enforce_total_limit(io);
    if (status != LONEJSON_STATUS_OK) {
      if (buffer != stack_buf) {
        lonejson__owned_free(buffer);
      }
      return status;
    }
    if (len >= io->limits.max_number_bytes) {
      if (buffer != stack_buf) {
        lonejson__owned_free(buffer);
      }
      return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                 0u, "JSON number exceeds maximum byte limit");
    }
    buffer[len++] = (char)ch;
    ch = lonejson__json_cursor_getc_lookahead(io);
  }
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  }
  if (ch == -2) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (!lonejson__is_valid_json_number(buffer, len)) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "invalid JSON number");
  }
  status = lonejson__json_visit_any_chunk(io, 0, chunk_fn, path_chunk_fn,
                                          buffer, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson__json_visit_any_event(io, end_fn, path_end_fn);
  }
  if (buffer != stack_buf) {
    lonejson__owned_free(buffer);
  }
  return status;
}

static lonejson_status lonejson__json_visit_literal(lonejson__json_io *io,
                                                    int first, const char *rest,
                                                    int kind) {
  size_t i;
  for (i = 0u; rest[i] != '\0'; ++i) {
    int ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch != (unsigned char)rest[i]) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid JSON literal");
    }
  }
  if (kind == 1) {
    return lonejson__json_visit_bool(io, first == 't');
  }
  return lonejson__json_visit_any_event(
      io, io->visitor == NULL ? NULL : io->visitor->null_value,
      io->path_visitor == NULL ? NULL : io->path_visitor->null_value);
}

static LONEJSON__HOT lonejson_status
lonejson__json_visit_value(lonejson__json_io *io);

static lonejson_status
lonejson__json_path_push_object_key(lonejson__json_io *io) {
  lonejson__json_path_frame *frame;

  if (io->path_visitor == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (io->path_depth >= io->path_capacity) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u, "invalid JSON path stack state");
  }
  frame = &io->path_frames[io->path_depth];
  io->path_segments[io->path_depth].data = frame->key;
  io->path_segments[io->path_depth].len = frame->key_len;
  ++io->path_depth;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_path_push_array_index(lonejson__json_io *io) {
  lonejson__json_path_frame *frame;
  size_t n;

  if (io->path_visitor == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (io->path_depth >= io->path_capacity) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u, "invalid JSON path stack state");
  }
  frame = &io->path_frames[io->path_depth];
  n = lonejson__format_size_decimal(
      frame->index_text, sizeof(frame->index_text), frame->next_index);
  if (n >= sizeof(frame->index_text)) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON array index path segment is too large");
  }
  io->path_segments[io->path_depth].data = frame->index_text;
  io->path_segments[io->path_depth].len = n;
  ++io->path_depth;
  return LONEJSON_STATUS_OK;
}

static void lonejson__json_path_pop(lonejson__json_io *io) {
  if (io->path_visitor != NULL && io->path_depth != 0u) {
    --io->path_depth;
  }
}

static lonejson_status lonejson__json_visit_array(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  if (io->depth >= io->limits.max_depth) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value nesting exceeds maximum depth");
  }
  if (io->path_visitor != NULL && io->path_depth < io->path_capacity) {
    io->path_frames[io->path_depth].next_index = 0u;
  }
  status = lonejson__json_visit_any_event(
      io, io->visitor == NULL ? NULL : io->visitor->array_begin,
      io->path_visitor == NULL ? NULL : io->path_visitor->array_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON array");
    }
    if (ch == ']') {
      (void)lonejson__json_cursor_getc(io);
      return lonejson__json_visit_any_event(
          io, io->visitor == NULL ? NULL : io->visitor->array_end,
          io->path_visitor == NULL ? NULL : io->path_visitor->array_end);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in array");
      }
    }
    status = lonejson__json_path_push_array_index(io);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ++io->depth;
    status = lonejson__json_visit_value(io);
    --io->depth;
    lonejson__json_path_pop(io);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (io->path_visitor != NULL && io->path_depth < io->path_capacity) {
      ++io->path_frames[io->path_depth].next_index;
    }
    first = 0;
  }
}

static lonejson_status lonejson__json_visit_object(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  if (io->depth >= io->limits.max_depth) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value nesting exceeds maximum depth");
  }
  status = lonejson__json_visit_any_event(
      io, io->visitor == NULL ? NULL : io->visitor->object_begin,
      io->path_visitor == NULL ? NULL : io->path_visitor->object_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON object");
    }
    if (ch == '}') {
      (void)lonejson__json_cursor_getc(io);
      return lonejson__json_visit_any_event(
          io, io->visitor == NULL ? NULL : io->visitor->object_end,
          io->path_visitor == NULL ? NULL : io->path_visitor->object_end);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in object");
      }
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != '"') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected object key");
    }
    (void)lonejson__json_cursor_getc(io);
    status = lonejson__json_visit_string_value(io, 1);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != ':') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected ':' after object key");
    }
    (void)lonejson__json_cursor_getc(io);
    status = lonejson__json_path_push_object_key(io);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ++io->depth;
    status = lonejson__json_visit_value(io);
    --io->depth;
    lonejson__json_path_pop(io);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static LONEJSON__HOT lonejson_status
lonejson__json_visit_value(lonejson__json_io *io) {
  int ch = lonejson__json_peek_nonspace(io);

  if (ch == -2) {
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (ch == EOF) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
  ch = lonejson__json_cursor_getc(io);
  switch (ch) {
  case '"':
    return lonejson__json_visit_string_value(io, 0);
  case '{':
    return lonejson__json_visit_object(io);
  case '[':
    return lonejson__json_visit_array(io);
  case 't':
    return lonejson__json_visit_literal(io, ch, "rue", 1);
  case 'f':
    return lonejson__json_visit_literal(io, ch, "alse", 1);
  case 'n':
    return lonejson__json_visit_literal(io, ch, "ull", 0);
  default:
    if (ch == '-' || lonejson__is_digit(ch)) {
      return lonejson__json_visit_number(io, ch);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
}

static lonejson_status lonejson__json_visit_cursor(
    lonejson__json_cursor *cursor, const lonejson_allocator *allocator,
    const lonejson_value_visitor *visitor, void *user,
    const lonejson_path_value_visitor *path_visitor,
    const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson__json_io io;
  lonejson__value_limits defaults;
  lonejson_status status;
  int ch;
  size_t i;

  if (cursor == NULL || (visitor == NULL && path_visitor == NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value source and visitor are required");
  }
  memset(&io, 0, sizeof(io));
  io.cursor = cursor;
  io.visitor = visitor;
  io.path_visitor = path_visitor;
  io.visitor_user = user;
  io.error = error;
  io.allocator = allocator;
  if (limits != NULL && limits->max_depth != 0u &&
      limits->max_string_bytes != 0u && limits->max_number_bytes != 0u &&
      limits->max_key_bytes != 0u) {
    io.limits = *limits;
  } else {
    defaults = lonejson__default_value_limits();
    io.limits = limits ? *limits : defaults;
    if (io.limits.max_depth == 0u) {
      io.limits.max_depth = defaults.max_depth;
    }
    if (io.limits.max_string_bytes == 0u) {
      io.limits.max_string_bytes = defaults.max_string_bytes;
    }
    if (io.limits.max_number_bytes == 0u) {
      io.limits.max_number_bytes = defaults.max_number_bytes;
    }
    if (io.limits.max_key_bytes == 0u) {
      io.limits.max_key_bytes = defaults.max_key_bytes;
    }
  }
  if (cursor->has_pushback) {
    io.has_pushback = 1;
    io.pushback_counted = cursor->count_pushback;
    io.pushback = cursor->pushback;
    cursor->has_pushback = 0;
    cursor->count_pushback = 0;
  }
  if (path_visitor != NULL) {
    io.path_capacity = io.limits.max_depth + 1u;
    io.path_segments = (lonejson_path_segment *)lonejson__owned_malloc(
        allocator, io.path_capacity * sizeof(*io.path_segments));
    io.path_frames = (lonejson__json_path_frame *)lonejson__owned_malloc(
        allocator, io.path_capacity * sizeof(*io.path_frames));
    if (io.path_segments == NULL || io.path_frames == NULL) {
      lonejson__owned_free(io.path_segments);
      lonejson__owned_free(io.path_frames);
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate JSON path visitor state");
    }
    memset(io.path_segments, 0, io.path_capacity * sizeof(*io.path_segments));
    memset(io.path_frames, 0, io.path_capacity * sizeof(*io.path_frames));
  }
  status = io.path_visitor == NULL ? lonejson__json_visit_value_no_path(&io)
                                   : lonejson__json_visit_value(&io);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    ch = lonejson__json_peek_nonspace(&io);
    if (ch != EOF) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "visited JSON must contain exactly one value");
    }
  }
  if (io.has_pushback) {
    cursor->has_pushback = 1;
    cursor->count_pushback = io.pushback_counted;
    cursor->pushback = io.pushback;
    cursor->pushback_offset = lonejson__json_cursor_last_offset(cursor);
  }
  if (io.path_frames != NULL) {
    for (i = 0u; i < io.path_capacity; ++i) {
      if (io.path_frames[i].key_heap) {
        lonejson__owned_free(io.path_frames[i].key);
      }
    }
  }
  lonejson__owned_free(io.path_segments);
  lonejson__owned_free(io.path_frames);
  return status;
}

static lonejson_status lonejson__json_visit(
    const lonejson_json_value *value, const lonejson_value_visitor *visitor,
    void *user, const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value source and visitor are required");
  }
  status = lonejson__json_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__json_visit_cursor(&cursor, &value->allocator, visitor,
                                       user, NULL, limits, error);
  lonejson__json_cursor_close(&cursor);
  return status;
}

static lonejson_status lonejson__json_transcode_cursor(
    lonejson__json_cursor *cursor, const lonejson_allocator *allocator,
    lonejson_sink_fn sink, void *user, lonejson_error *error, int pretty,
    size_t base_depth) {
  lonejson__json_io io;
  lonejson_status status;
  int ch;

  if (cursor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  memset(&io, 0, sizeof(io));
  io.cursor = cursor;
  io.sink = sink;
  io.sink_user = user;
  io.error = error;
  io.pretty = pretty;
  io.base_depth = base_depth;
  io.allocator = allocator;
  status = lonejson__json_parse_value(&io);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    ch = lonejson__json_peek_nonspace(&io);
    if (ch != EOF) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "embedded JSON must contain exactly one value");
    }
  }
  return status;
}

static lonejson_status
lonejson__json_transcode(const lonejson_json_value *value,
                         lonejson_sink_fn sink, void *user,
                         lonejson_error *error, int pretty, size_t base_depth) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (value->kind == LONEJSON_JSON_VALUE_NULL) {
    return sink(user, "null", 4u, error);
  }
  if (!pretty && value->kind == LONEJSON_JSON_VALUE_BUFFER) {
    return sink(user, value->json, value->len, error);
  }
  status = lonejson__json_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__json_transcode_cursor(&cursor, &value->allocator, sink,
                                           user, error, pretty, base_depth);
  lonejson__json_cursor_close(&cursor);
  return status;
}
