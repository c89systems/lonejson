static int lonejson__hex_value(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

static LONEJSON__INLINE lonejson_status
lonejson__direct_string_reject_nul(lonejson_parser *parser) {
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
      parser->error.line, parser->error.column,
      "field '%s' contains embedded NUL unsupported by C strings",
      parser->direct_string_field != NULL
          ? parser->direct_string_field->json_key
          : "<unknown>");
}

static LONEJSON__INLINE int
lonejson__decode_unicode_quad(const unsigned char *src, lonejson_uint32 *out) {
  lonejson_uint32 cp;
  int hv;

  cp = 0u;
  hv = lonejson__hex_value(src[0]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[1]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[2]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[3]);
  if (hv < 0) {
    return 0;
  }
  *out = (cp << 4u) | (lonejson_uint32)hv;
  return 1;
}

static LONEJSON__INLINE lonejson_status
lonejson__direct_string_append_codepoint(lonejson_parser *parser,
                                         lonejson_uint32 cp) {
  unsigned char utf8[4];
  size_t utf8_len;
  size_t limit;
  size_t remaining;
  size_t copy_len;

  if (!parser->direct_string_active || parser->direct_string_ptr == NULL ||
      parser->direct_string_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "direct string field is not active");
  }
  if (cp == 0u) {
    return lonejson__direct_string_reject_nul(parser);
  }
  if (cp <= 0x7Fu) {
    utf8[0] = (unsigned char)cp;
    utf8_len = 1u;
  } else if (cp <= 0x7FFu) {
    utf8[0] = (unsigned char)(0xC0u | (cp >> 6u));
    utf8[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 2u;
  } else if (cp <= 0xFFFFu) {
    utf8[0] = (unsigned char)(0xE0u | (cp >> 12u));
    utf8[1] = (unsigned char)(0x80u | ((cp >> 6u) & 0x3Fu));
    utf8[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 3u;
  } else if (cp <= 0x10FFFFu) {
    utf8[0] = (unsigned char)(0xF0u | (cp >> 18u));
    utf8[1] = (unsigned char)(0x80u | ((cp >> 12u) & 0x3Fu));
    utf8[2] = (unsigned char)(0x80u | ((cp >> 6u) & 0x3Fu));
    utf8[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 4u;
  } else {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "invalid unicode codepoint");
  }

  limit = parser->direct_string_capacity - 1u;
  remaining = (parser->direct_string_len < limit)
                  ? (limit - parser->direct_string_len)
                  : 0u;
  copy_len = (utf8_len < remaining) ? utf8_len : remaining;
  if (copy_len != 0u) {
    memcpy(parser->direct_string_ptr + parser->direct_string_len, utf8,
           copy_len);
    parser->direct_string_len += copy_len;
  }
  if (copy_len == utf8_len) {
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

static LONEJSON__INLINE lonejson_status lonejson__append_unicode_codepoint(
    lonejson_parser *parser, lonejson_uint32 cp) {
  unsigned char utf8[4];
  size_t utf8_len;

  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
    if (!lonejson__utf8_append(&parser->token, cp)) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to append unicode escape");
    }
    return LONEJSON_STATUS_OK;
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT &&
      parser->direct_string_dst == NULL) {
    return lonejson__direct_string_append_codepoint(parser, cp);
  }
  if (cp <= 0x7Fu) {
    utf8[0] = (unsigned char)cp;
    utf8_len = 1u;
  } else if (cp <= 0x7FFu) {
    utf8[0] = (unsigned char)(0xC0u | (cp >> 6u));
    utf8[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 2u;
  } else if (cp <= 0xFFFFu) {
    utf8[0] = (unsigned char)(0xE0u | (cp >> 12u));
    utf8[1] = (unsigned char)(0x80u | ((cp >> 6u) & 0x3Fu));
    utf8[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 3u;
  } else if (cp <= 0x10FFFFu) {
    utf8[0] = (unsigned char)(0xF0u | (cp >> 18u));
    utf8[1] = (unsigned char)(0x80u | ((cp >> 12u) & 0x3Fu));
    utf8[2] = (unsigned char)(0x80u | ((cp >> 6u) & 0x3Fu));
    utf8[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 4u;
  } else {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "invalid unicode codepoint");
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
    if (cp == 0u) {
      return lonejson__direct_string_reject_nul(parser);
    }
    return lonejson__direct_string_append_bytes(parser, utf8, utf8_len);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
    return lonejson__json_value_string_chunk(parser, (const char *)utf8,
                                             utf8_len, 0);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_STREAM) {
    lonejson_status status;
    status = lonejson__stream_value_append(parser, (const unsigned char *)utf8,
                                           utf8_len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    return LONEJSON_STATUS_OK;
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DISCARD) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
      parser->error.line, parser->error.column, "invalid unicode append mode");
}

static LONEJSON__INLINE lonejson_status
lonejson__append_string_byte(lonejson_parser *parser, unsigned char ch) {
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
    return lonejson__scratch_append(&parser->token, (char)ch)
               ? LONEJSON_STATUS_OK
               : lonejson__set_error(
                     &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                     parser->error.offset, parser->error.line,
                     parser->error.column, "failed to append string");
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
    if (ch == '\0') {
      return lonejson__direct_string_reject_nul(parser);
    }
    return lonejson__direct_string_append_bytes(parser, &ch, 1u);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
    return lonejson__json_value_string_chunk(parser, (const char *)&ch, 1u, 0);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_STREAM) {
    return lonejson__stream_value_append(parser, &ch, 1u);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DISCARD) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
      parser->error.line, parser->error.column, "invalid string append mode");
}

static LONEJSON__INLINE lonejson_status lonejson__consume_simple_escape_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  unsigned char ch;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 2u || bytes[0] != '\\' ||
      parser->unicode_pending_high != 0u) {
    return LONEJSON_STATUS_OK;
  }
  switch (bytes[1]) {
  case '"':
    ch = '"';
    break;
  case '\\':
    ch = '\\';
    break;
  case '/':
    ch = '/';
    break;
  case 'b':
    ch = '\b';
    break;
  case 'f':
    ch = '\f';
    break;
  case 'n':
    ch = '\n';
    break;
  case 'r':
    ch = '\r';
    break;
  case 't':
    ch = '\t';
    break;
  default:
    return LONEJSON_STATUS_OK;
  }
  if (used != NULL) {
    *used = 2u;
  }
  parser->error.offset += 2u;
  parser->error.column += 2u;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__append_string_byte(parser, ch);
}

static LONEJSON__INLINE lonejson_status lonejson__consume_unicode_escape_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  lonejson_uint32 cp;
  size_t consume;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 6u || bytes[0] != '\\' ||
      bytes[1] != 'u') {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__decode_unicode_quad(bytes + 2u, &cp)) {
    return LONEJSON_STATUS_OK;
  }
  consume = 6u;
  if (parser->unicode_pending_high != 0u) {
    if (cp < 0xDC00u || cp > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u +
         (((parser->unicode_pending_high - 0xD800u) << 10u) | (cp - 0xDC00u));
    parser->unicode_pending_high = 0u;
  } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
    lonejson_uint32 low;

    if (avail < 12u || bytes[6] != '\\' || bytes[7] != 'u' ||
        !lonejson__decode_unicode_quad(bytes + 8u, &low)) {
      return LONEJSON_STATUS_OK;
    }
    if (low < 0xDC00u || low > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
    consume = 12u;
  } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected low surrogate");
  }
  if (used != NULL) {
    *used = consume;
  }
  parser->error.offset += consume;
  parser->error.column += consume;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__append_unicode_codepoint(parser, cp);
}

static LONEJSON__INLINE LONEJSON__HOT lonejson_status
lonejson__consume_string_discard_fast(lonejson_parser *parser,
                                      const unsigned char *bytes, size_t avail,
                                      size_t *used) {
  size_t pos;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  pos = 0u;
  while (pos < avail) {
    size_t start;

    start = pos;
    pos += lonejson__plain_string_span(bytes + pos, avail - pos, 0);
    if (pos != start) {
      parser->error.offset += pos - start;
      parser->error.column += pos - start;
      lonejson__parser_note_workspace_usage(parser);
      if (pos == avail) {
        break;
      }
    }

    if (bytes[pos] == '"') {
      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      parser->error.offset++;
      parser->error.column++;
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (used != NULL) {
        *used = pos + 1u;
      }
      return lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
    }

    if (bytes[pos] == '\\') {
      size_t consumed;
      lonejson_status status;

      status = lonejson__consume_simple_escape_fast(parser, bytes + pos,
                                                    avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      status = lonejson__consume_unicode_escape_fast(parser, bytes + pos,
                                                     avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      break;
    }

    if (bytes[pos] < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string literal");
    }
  }
  if (used != NULL) {
    *used = pos;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__consume_simple_escape_direct_fast(lonejson_parser *parser,
                                            const unsigned char *bytes,
                                            size_t avail, size_t *used);

static LONEJSON__INLINE lonejson_status
lonejson__consume_unicode_escape_direct_fast(lonejson_parser *parser,
                                             const unsigned char *bytes,
                                             size_t avail, size_t *used);

static LONEJSON__INLINE LONEJSON__HOT lonejson_status
lonejson__consume_string_direct_fast(lonejson_parser *parser,
                                     const unsigned char *bytes, size_t avail,
                                     size_t *used) {
  size_t pos;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  pos = 0u;
  while (pos < avail) {
    size_t start;

    start = pos;
    pos += lonejson__plain_string_span(bytes + pos, avail - pos, 0);
    if (pos != start) {
      lonejson_status status = lonejson__direct_string_append_bytes(
          parser, bytes + start, pos - start);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      parser->error.offset += pos - start;
      parser->error.column += pos - start;
      lonejson__parser_note_workspace_usage(parser);
      if (pos == avail) {
        break;
      }
    }

    if (bytes[pos] == '"') {
      lonejson_status status;

      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      parser->error.offset++;
      parser->error.column++;
      parser->lex_mode = LONEJSON_LEX_NONE;
      status = lonejson__direct_string_finish(parser);
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (used != NULL) {
        *used = pos + 1u;
      }
      return status;
    }

    if (bytes[pos] == '\\') {
      size_t consumed;
      lonejson_status status;

      status = lonejson__consume_simple_escape_direct_fast(
          parser, bytes + pos, avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      status = lonejson__consume_unicode_escape_direct_fast(
          parser, bytes + pos, avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      break;
    }

    if (bytes[pos] < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string literal");
    }
  }
  if (used != NULL) {
    *used = pos;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__consume_simple_escape_direct_fast(lonejson_parser *parser,
                                            const unsigned char *bytes,
                                            size_t avail, size_t *used) {
  unsigned char ch;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 2u || bytes[0] != '\\' ||
      parser->unicode_pending_high != 0u) {
    return LONEJSON_STATUS_OK;
  }
  switch (bytes[1]) {
  case '"':
    ch = '"';
    break;
  case '\\':
    ch = '\\';
    break;
  case '/':
    ch = '/';
    break;
  case 'b':
    ch = '\b';
    break;
  case 'f':
    ch = '\f';
    break;
  case 'n':
    ch = '\n';
    break;
  case 'r':
    ch = '\r';
    break;
  case 't':
    ch = '\t';
    break;
  default:
    return LONEJSON_STATUS_OK;
  }
  if (used != NULL) {
    *used = 2u;
  }
  parser->error.offset += 2u;
  parser->error.column += 2u;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__direct_string_append_bytes(parser, &ch, 1u);
}

static LONEJSON__INLINE lonejson_status
lonejson__consume_unicode_escape_direct_fast(lonejson_parser *parser,
                                             const unsigned char *bytes,
                                             size_t avail, size_t *used) {
  lonejson_uint32 cp;
  size_t consume;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 6u || bytes[0] != '\\' ||
      bytes[1] != 'u') {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__decode_unicode_quad(bytes + 2u, &cp)) {
    return LONEJSON_STATUS_OK;
  }
  consume = 6u;
  if (parser->unicode_pending_high != 0u) {
    if (cp < 0xDC00u || cp > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u +
         (((parser->unicode_pending_high - 0xD800u) << 10u) | (cp - 0xDC00u));
    parser->unicode_pending_high = 0u;
  } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
    lonejson_uint32 low;

    if (avail < 12u || bytes[6] != '\\' || bytes[7] != 'u' ||
        !lonejson__decode_unicode_quad(bytes + 8u, &low)) {
      return LONEJSON_STATUS_OK;
    }
    if (low < 0xDC00u || low > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
    consume = 12u;
  } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected low surrogate");
  }
  if (used != NULL) {
    *used = consume;
  }
  parser->error.offset += consume;
  parser->error.column += consume;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__direct_string_append_codepoint(parser, cp);
}

/* Hold at most one raw code point so callbacks only receive complete UTF-8. */
static lonejson_status lonejson__visit_utf8_byte(lonejson_parser *parser,
                                                 unsigned char byte,
                                                 size_t *pos) {
  unsigned int length = parser->visit_utf8_len;

  if (length == 0u) {
    if (byte >= 0xC2u && byte <= 0xDFu) {
      parser->visit_utf8_expected = 2u;
    } else if (byte >= 0xE0u && byte <= 0xEFu) {
      parser->visit_utf8_expected = 3u;
    } else if (byte >= 0xF0u && byte <= 0xF4u) {
      parser->visit_utf8_expected = 4u;
    } else {
      goto invalid;
    }
  } else {
    unsigned int min_byte = 0x80u;
    unsigned int max_byte = 0xBFu;

    if (length == 1u) {
      switch (parser->visit_utf8[0]) {
      case 0xE0u:
        min_byte = 0xA0u;
        break;
      case 0xEDu:
        max_byte = 0x9Fu;
        break;
      case 0xF0u:
        min_byte = 0x90u;
        break;
      case 0xF4u:
        max_byte = 0x8Fu;
        break;
      }
    }
    if (byte < min_byte || byte > max_byte) {
      goto invalid;
    }
  }
  parser->visit_utf8[length++] = byte;
  parser->visit_utf8_len = length;
  ++*pos;
  parser->error.offset++;
  parser->error.column++;
  if (length == parser->visit_utf8_expected) {
    parser->visit_utf8_len = 0u;
    return lonejson__json_value_string_chunk(
        parser, (const char *)parser->visit_utf8, length, 0);
  }
  return LONEJSON_STATUS_OK;

invalid:
  return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                             parser->error.offset, parser->error.line,
                             parser->error.column, "invalid UTF-8 in string");
}

static LONEJSON__INLINE LONEJSON__HOT lonejson_status
lonejson__consume_string_fast(lonejson_parser *parser,
                              const unsigned char *bytes, size_t avail,
                              size_t *used) {
  size_t pos;
  unsigned int plain_limit;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  pos = 0u;
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DISCARD) {
    return lonejson__consume_string_discard_fast(parser, bytes, avail, used);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
    return lonejson__consume_string_direct_fast(parser, bytes, avail, used);
  }
  if (parser->lex_is_key &&
      parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN &&
      parser->token.len == 0u) {
    pos += lonejson__plain_string_span(bytes + pos, avail - pos, 0);
    if (pos < avail && bytes[pos] == '"') {
      parser->error.offset += pos + 1u;
      parser->error.column += pos + 1u;
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (used != NULL) {
        *used = pos + 1u;
      }
      return lonejson__deliver_key_text(parser, (const char *)bytes, pos);
    }
    pos = 0u;
  }
  plain_limit = parser->visit_callback_error != NULL ? 0x80u : 0x100u;
  while (pos < avail && !parser->visit_paused) {
    size_t start;

    if (bytes[pos] >= plain_limit || parser->visit_utf8_len != 0u) {
      lonejson_status status =
          lonejson__visit_utf8_byte(parser, bytes[pos], &pos);

      if (used != NULL) {
        *used = pos;
      }
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      continue;
    }
    start = pos;
    if (used != NULL) {
      *used = pos;
    }
    pos += lonejson__plain_string_span(bytes + pos, avail - pos,
                                       plain_limit == 0x80u);
    if (pos != start) {
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
        if (!lonejson__scratch_append_bytes(&parser->token, bytes + start,
                                            pos - start)) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
              parser->error.offset, parser->error.line, parser->error.column,
              "failed to append string");
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_DIRECT) {
        lonejson_status status = lonejson__direct_string_append_bytes(
            parser, bytes + start, pos - start);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
        lonejson_status status = lonejson__json_value_string_chunk(
            parser, (const char *)(bytes + start), pos - start, 1);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_STREAM) {
        lonejson_status status =
            lonejson__stream_value_append(parser, bytes + start, pos - start);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
            parser->error.offset, parser->error.line, parser->error.column,
            "invalid string capture mode");
      }
      parser->error.offset += pos - start;
      parser->error.column += pos - start;
      lonejson__parser_note_workspace_usage(parser);
      if (used != NULL) {
        *used = pos;
      }
      if (pos == avail || parser->visit_paused) {
        break;
      }
      if (bytes[pos] >= plain_limit) {
        continue;
      }
    }

    if (bytes[pos] == '"') {
      lonejson_status status;

      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      parser->error.offset++;
      parser->error.column++;
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
        status = lonejson__direct_string_finish(parser);
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
        status = lonejson__json_value_string_end(parser);
        if (status == LONEJSON_STATUS_OK) {
          status =
              !lonejson__json_value_parse_visitor_active(parser) ||
                      lonejson__streamed_json_value_scalar_field_active(parser)
                  ? lonejson__complete_streamed_string_token(parser)
                  : lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_STREAM) {
        status = lonejson__stream_value_finish(parser);
        if (status == LONEJSON_STATUS_OK) {
          status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
        }
      } else {
        status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (used != NULL) {
        *used = pos + 1u;
      }
      return status;
    }

    if (bytes[pos] == '\\') {
      size_t consumed;
      lonejson_status status;

      status = lonejson__consume_simple_escape_fast(parser, bytes + pos,
                                                    avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      status = lonejson__consume_unicode_escape_fast(parser, bytes + pos,
                                                     avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      break;
    }

    if (bytes[pos] < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string literal");
    }
  }
  if (used != NULL) {
    *used = pos;
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__is_valid_json_number(const char *value, size_t len) {
  size_t i = 0u;

  if (len == 0u) {
    return 0;
  }
  if (len == 1u) {
    return lonejson__is_digit((unsigned char)value[0]);
  }
  if (value[i] == '-') {
    i++;
    if (i == len) {
      return 0;
    }
  }
  if (value[i] == '0') {
    i++;
    if (i < len && lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
  } else {
    if (!lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    ++i;
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  if (i < len && value[i] == '.') {
    i++;
    if (i == len || !lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  if (i < len && (value[i] == 'e' || value[i] == 'E')) {
    i++;
    if (i < len && (value[i] == '+' || value[i] == '-')) {
      i++;
    }
    if (i == len || !lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  return i == len;
}

static lonejson_status lonejson__parser_consume_char(lonejson_parser *parser,
                                                     unsigned char ch) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;

  parser->error.offset++;
  if (ch == '\n') {
    parser->error.line++;
    parser->error.column = 0u;
  } else {
    parser->error.column++;
  }

  if (parser->lex_mode == LONEJSON_LEX_STRING) {
    if (parser->unicode_digits_needed != 0) {
      int hv = lonejson__hex_value(ch);
      if (hv < 0) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
            parser->error.line, parser->error.column, "invalid unicode escape");
      }
      parser->unicode_accum =
          (parser->unicode_accum << 4u) | (lonejson_uint32)hv;
      parser->unicode_digits_needed--;
      if (parser->unicode_digits_needed == 0) {
        lonejson_uint32 cp = parser->unicode_accum;
        if (parser->unicode_pending_high != 0u) {
          if (cp < 0xDC00u || cp > 0xDFFFu) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_INVALID_JSON,
                parser->error.offset, parser->error.line, parser->error.column,
                "invalid unicode surrogate pair");
          }
          cp = 0x10000u + (((parser->unicode_pending_high - 0xD800u) << 10u) |
                           (cp - 0xDC00u));
          parser->unicode_pending_high = 0u;
          {
            lonejson_status status =
                lonejson__append_unicode_codepoint(parser, cp);
            if (status != LONEJSON_STATUS_OK) {
              return status;
            }
          }
        } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
          parser->unicode_pending_high = cp;
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_INVALID_JSON,
              parser->error.offset, parser->error.line, parser->error.column,
              "unexpected low surrogate");
        } else {
          lonejson_status status =
              lonejson__append_unicode_codepoint(parser, cp);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        }
      }
      return LONEJSON_STATUS_OK;
    }
    /* A high surrogate must be followed immediately by a low-surrogate
     * escape, even when the backslash or the 'u' arrives in a later feed. */
    if (parser->unicode_pending_high != 0u &&
        ch != (parser->lex_escape ? 'u' : '\\')) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected low unicode surrogate escape");
    }
    if (parser->lex_escape) {
      parser->lex_escape = 0;
      switch (ch) {
      case '"':
        return lonejson__append_string_byte(parser, '"');
      case '\\':
        return lonejson__append_string_byte(parser, '\\');
      case '/':
        return lonejson__append_string_byte(parser, '/');
      case 'b':
        return lonejson__append_string_byte(parser, '\b');
      case 'f':
        return lonejson__append_string_byte(parser, '\f');
      case 'n':
        return lonejson__append_string_byte(parser, '\n');
      case 'r':
        return lonejson__append_string_byte(parser, '\r');
      case 't':
        return lonejson__append_string_byte(parser, '\t');
      case 'u':
        parser->unicode_digits_needed = 4;
        parser->unicode_accum = 0u;
        return LONEJSON_STATUS_OK;
      default:
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "invalid escape sequence");
      }
    }
    if (ch == '\\') {
      parser->lex_escape = 1;
      return LONEJSON_STATUS_OK;
    }
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
        return lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
        return lonejson__direct_string_finish(parser);
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
        lonejson_status status = lonejson__json_value_string_end(parser);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        return !lonejson__json_value_parse_visitor_active(parser) ||
                       lonejson__streamed_json_value_scalar_field_active(parser)
                   ? lonejson__complete_streamed_string_token(parser)
                   : lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_STREAM) {
        return lonejson__stream_value_finish(parser) == LONEJSON_STATUS_OK
                   ? lonejson__deliver_token(parser, LONEJSON_LEX_STRING)
                   : parser->error.code;
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DISCARD) {
        return lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid string completion mode");
    }
    if (ch < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string");
    }
    if (parser->stream_value_active) {
      return lonejson__stream_value_append(parser, &ch, 1u);
    }
    return lonejson__scratch_append(&parser->token, (char)ch)
               ? LONEJSON_STATUS_OK
               : lonejson__set_error(
                     &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                     parser->error.offset, parser->error.line,
                     parser->error.column, "failed to append string");
  }

  if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
    if (lonejson__is_digit(ch) || ch == '+' || ch == '-' || ch == '.' ||
        ch == 'e' || ch == 'E') {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append number");
    }
    parser->lex_mode = LONEJSON_LEX_NONE;
    if (lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER) !=
        LONEJSON_STATUS_OK) {
      return parser->error.code;
    }
    /* This delimiter already advanced the source position. Process it in
     * the completed value's parent without counting the byte a second time. */
    frame = (parser->frame_count != 0u)
                ? &parser->frames[parser->frame_count - 1u]
                : NULL;
  }

  if (parser->lex_mode == LONEJSON_LEX_TRUE) {
    const char *literal = "true";
    size_t pos = parser->token.len;
    if (pos < 4u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 4u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_TRUE))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (parser->lex_mode == LONEJSON_LEX_FALSE) {
    const char *literal = "false";
    size_t pos = parser->token.len;
    if (pos < 5u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 5u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_FALSE))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (parser->lex_mode == LONEJSON_LEX_NULL) {
    const char *literal = "null";
    size_t pos = parser->token.len;
    if (pos < 4u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 4u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_NULL))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (lonejson__is_json_space(ch)) {
    return LONEJSON_STATUS_OK;
  }

  if (!parser->root_started) {
    if (ch == '{') {
      return lonejson__begin_object_value(parser);
    }
    if (ch == '[') {
      return lonejson__begin_array_value(parser);
    }
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 0;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (parser->validate_only) {
        if (parser->json_stream_active) {
          lonejson_status status = lonejson__json_value_string_begin(parser, 0);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
          parser->string_capture_mode = LONEJSON_STRING_CAPTURE_JSON_VISITOR;
          parser->token.data = NULL;
          parser->token.cap = 0u;
          parser->token.len = 0u;
          return LONEJSON_STATUS_OK;
        }
        parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DISCARD;
        parser->token.data = NULL;
        parser->token.cap = 0u;
        parser->token.len = 0u;
        return LONEJSON_STATUS_OK;
      }
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    if (ch == '-' || lonejson__is_digit(ch)) {
      parser->lex_mode = LONEJSON_LEX_NUMBER;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      if (!lonejson__scratch_append(&parser->token, (char)ch)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append number");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    if (ch == 't' || ch == 'f' || ch == 'n') {
      parser->lex_mode = (ch == 't')   ? LONEJSON_LEX_TRUE
                         : (ch == 'f') ? LONEJSON_LEX_FALSE
                                       : LONEJSON_LEX_NULL;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      if (!lonejson__scratch_append(&parser->token, (char)ch)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append literal");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    return lonejson__set_error(
        &parser->error,
        parser->validate_only ? LONEJSON_STATUS_INVALID_JSON
                              : LONEJSON_STATUS_TYPE_MISMATCH,
        parser->error.offset, parser->error.line, parser->error.column,
        parser->validate_only ? "expected JSON value"
                              : "root value must be an object");
  }

  if (frame == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected trailing data");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    switch (frame->state) {
    case LONEJSON_FRAME_OBJECT_KEY_OR_END:
      if (ch == '}') {
        if (frame->after_comma) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_INVALID_JSON,
              parser->error.offset, parser->error.line, parser->error.column,
              "trailing comma in object");
        }
        return lonejson__finalize_object(parser);
      }
      if (ch != '"') {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column, "expected object key");
      }
      frame->after_comma = 0;
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 1;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return LONEJSON_STATUS_OK;
    case LONEJSON_FRAME_OBJECT_COLON:
      if (ch != ':') {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "expected ':' after object key");
      }
      frame->state = LONEJSON_FRAME_OBJECT_VALUE;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FRAME_OBJECT_VALUE:
      if (ch == '"') {
        lonejson_status status;

        if (frame->pending_field != NULL &&
            (frame->pending_field->kind ==
                 LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM ||
             frame->pending_field->kind ==
                 LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM)) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
              parser->error.offset, parser->error.line, parser->error.column,
              "field '%s' is not an array", frame->pending_field->json_key);
        }
        parser->lex_mode = LONEJSON_LEX_STRING;
        parser->lex_is_key = 0;
        parser->lex_escape = 0;
        parser->unicode_pending_high = 0u;
        parser->unicode_digits_needed = 0;
        if (lonejson__field_is_streamed(frame->pending_field)) {
          void *ptr =
              lonejson__field_ptr(frame->object_ptr, frame->pending_field);
          status =
              lonejson__stream_value_begin(parser, frame->pending_field, ptr);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        } else if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return LONEJSON_STATUS_OK;
      }
      if (ch == '{') {
        return lonejson__begin_object_value(parser);
      }
      if (ch == '[') {
        return lonejson__begin_array_value(parser);
      }
      if (ch == '-' || lonejson__is_digit(ch)) {
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
      if (ch == 't') {
        parser->lex_mode = LONEJSON_LEX_TRUE;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 't')
                   ? ((parser->token.len == 4u)
                          ? (parser->lex_mode = LONEJSON_LEX_NONE,
                             lonejson__deliver_token(parser, LONEJSON_LEX_TRUE))
                          : LONEJSON_STATUS_OK)
                   : LONEJSON_STATUS_ALLOCATION_FAILED;
      }
      if (ch == 'f') {
        parser->lex_mode = LONEJSON_LEX_FALSE;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 'f')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append literal");
      }
      if (ch == 'n') {
        parser->lex_mode = LONEJSON_LEX_NULL;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 'n')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append literal");
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "expected JSON value");
    case LONEJSON_FRAME_OBJECT_COMMA_OR_END:
      if (ch == ',') {
        if (parser->json_stream_active &&
            !parser->json_stream_parse_visitor_active) {
          lonejson_status status = lonejson__json_value_emit(parser, ",", 1u);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        }
        frame->state = LONEJSON_FRAME_OBJECT_KEY_OR_END;
        frame->after_comma = 1;
        return LONEJSON_STATUS_OK;
      }
      if (ch == '}') {
        return lonejson__finalize_object(parser);
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected ',' or '}' in object");
    default:
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "invalid object state");
    }
  }

  switch (frame->state) {
  case LONEJSON_FRAME_ARRAY_VALUE_OR_END:
    if (ch == ']') {
      if (frame->after_comma) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "trailing comma in array");
      }
      return lonejson__finalize_array(parser);
    }
    frame->after_comma = 0;
    if (ch == '"') {
      return lonejson__begin_string_value_lex(
          parser, frame->field,
          frame->field != NULL
              ? lonejson__field_ptr(frame->object_ptr, frame->field)
              : NULL);
    }
    if (ch == '{') {
      return lonejson__begin_object_value(parser);
    }
    if (ch == '[') {
      return lonejson__begin_array_value(parser);
    }
    if (ch == '-' || lonejson__is_digit(ch)) {
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
    if (ch == 't') {
      parser->lex_mode = LONEJSON_LEX_TRUE;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 't')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    if (ch == 'f') {
      parser->lex_mode = LONEJSON_LEX_FALSE;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 'f')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    if (ch == 'n') {
      parser->lex_mode = LONEJSON_LEX_NULL;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 'n')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "expected array value");
  case LONEJSON_FRAME_ARRAY_COMMA_OR_END:
    if (ch == ',') {
      if (parser->json_stream_active &&
          !parser->json_stream_parse_visitor_active) {
        lonejson_status status = lonejson__json_value_emit(parser, ",", 1u);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      frame->state = LONEJSON_FRAME_ARRAY_VALUE_OR_END;
      frame->after_comma = 1;
      return LONEJSON_STATUS_OK;
    }
    if (ch == ']') {
      return lonejson__finalize_array(parser);
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "expected ',' or ']' in array");
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid array state");
  }
}

static int lonejson__parser_root_complete(const lonejson_parser *parser) {
  return parser->root_finished && parser->frame_count == 0u &&
         parser->lex_mode == LONEJSON_LEX_NONE;
}

static LONEJSON__INLINE int lonejson__is_number_token_byte(unsigned char ch) {
  return lonejson__is_digit(ch) || ch == '+' || ch == '-' || ch == '.' ||
         ch == 'e' || ch == 'E';
}

/* Unmapped numbers need grammar validation but no stored value. The caller
 * already counted the first byte and excluded active JSON value capture.
 * Short input fragments use the normal lexer rather than an extra scan. */
static LONEJSON__NOINLINE lonejson_status lonejson__begin_ignored_number(
    lonejson_parser *parser, const unsigned char *bytes, size_t len,
    size_t *used) {
  lonejson_status status;
  size_t pos;

  *used = 1u;
  status = lonejson__begin_number_lex(parser, bytes[0]);
  if (status != LONEJSON_STATUS_OK)
    return status;
  pos = 1u;
  while (pos < len && lonejson__is_number_token_byte(bytes[pos]))
    ++pos;
  /* Preserve the token capacity and continuation behavior for spans that
   * cannot be completed directly from this input chunk. */
  if (pos == len || pos > parser->token.cap - 2u)
    return LONEJSON_STATUS_OK;
  *used = pos;
  parser->error.offset += pos - 1u;
  parser->error.column += pos - 1u;
  if (!lonejson__is_valid_json_number((const char *)bytes, pos))
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid JSON number");
  parser->lex_mode = LONEJSON_LEX_NONE;
  lonejson__scratch_reset(&parser->token);
  return lonejson__complete_parent_after_value(parser);
}

static LONEJSON__HOT lonejson_status
lonejson__parser_feed_bytes(lonejson_parser *parser, const unsigned char *bytes,
                            size_t len, size_t *consumed, int stop_at_root) {
  size_t i;

  if (parser == NULL || (bytes == NULL && len != 0u)) {
    return lonejson__set_error(parser ? &parser->error : NULL,
                               LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                               "invalid parser feed arguments");
  }
  if (parser->failed) {
    return parser->error.code;
  }

  i = 0u;
  while (i < len && !parser->visit_paused &&
         parser->visit_number_phase == LONEJSON__VISIT_NUMBER_NONE) {
    lonejson_status status;

    if (stop_at_root && lonejson__parser_root_complete(parser)) {
      break;
    }

    if (parser->lex_mode == LONEJSON_LEX_STRING &&
        parser->unicode_digits_needed == 0 && !parser->lex_escape &&
        parser->unicode_pending_high == 0u) {
      size_t used = 0u;

      status = lonejson__consume_string_fast(parser, bytes + i, len - i, &used);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        parser->failed = 1;
        if (consumed != NULL) {
          *consumed = i + used;
        }
        return status;
      }
      i += used;
      if (parser->lex_mode == LONEJSON_LEX_NONE) {
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
      if (used != 0u) {
        continue;
      }
    } else if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
      size_t start;
      size_t scan_end = len;

      start = i;
      if (parser->visit_callback_error != NULL) {
        size_t remaining =
            parser->json_stream_value->parse_visitor_limits.max_number_bytes -
            parser->token.len;
        if (remaining < len - i) {
          scan_end = i + remaining;
        }
      }
      while (i < scan_end && lonejson__is_number_token_byte(bytes[i])) {
        i++;
      }
      if (i != start) {
        if (!lonejson__scratch_append_bytes(&parser->token, bytes + start,
                                            i - start)) {
          parser->failed = 1;
          status = lonejson__set_error(
              &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
              parser->error.offset, parser->error.line, parser->error.column,
              "failed to append number");
          if (consumed != NULL) {
            *consumed = start;
          }
          return status;
        }
        parser->error.offset += i - start;
        parser->error.column += i - start;
        lonejson__parser_note_workspace_usage(parser);
      }
      if (i == scan_end && i < len &&
          lonejson__is_number_token_byte(bytes[i])) {
        parser->failed = 1;
        if (consumed != NULL) {
          *consumed = i;
        }
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "JSON number exceeds maximum byte limit");
      }
      if (i == len) {
        break;
      }
      if (i != start || parser->visit_callback_error != NULL) {
        parser->lex_mode = LONEJSON_LEX_NONE;
        status = lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          parser->failed = 1;
          if (consumed != NULL) {
            *consumed = i;
          }
          return status;
        }
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
    }

    if (parser->lex_mode == LONEJSON_LEX_NONE && parser->frame_count != 0u) {
      lonejson_frame *active;

      active = &parser->frames[parser->frame_count - 1u];

      parser->error.offset++;
      parser->error.column++;
      if (active->kind == LONEJSON_CONTAINER_OBJECT) {
        if (active->state == LONEJSON_FRAME_OBJECT_KEY_OR_END &&
            bytes[i] == '"') {
          status = lonejson__begin_string_lex(parser, 1);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (active->state == LONEJSON_FRAME_OBJECT_COLON && bytes[i] == ':') {
          active->state = LONEJSON_FRAME_OBJECT_VALUE;
          i++;
          continue;
        }
        if (active->state == LONEJSON_FRAME_OBJECT_VALUE) {
          if (bytes[i] == '"') {
            if (active->pending_field != NULL &&
                (active->pending_field->kind ==
                     LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM ||
                 active->pending_field->kind ==
                     LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM)) {
              status = lonejson__set_error(
                  &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                  parser->error.offset, parser->error.line,
                  parser->error.column, "field '%s' is not an array",
                  active->pending_field->json_key);
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            status = lonejson__begin_string_value_lex(
                parser, active->pending_field,
                active->pending_field != NULL
                    ? lonejson__field_ptr(active->object_ptr,
                                          active->pending_field)
                    : NULL);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
          if (bytes[i] == '-' || lonejson__is_digit(bytes[i])) {
            size_t number_used = 1u;
            if (active->pending_field == NULL && !parser->json_stream_active &&
                len - i >= sizeof(lonejson_uint64))
              status = lonejson__begin_ignored_number(parser, bytes + i,
                                                      len - i, &number_used);
            else
              status = lonejson__begin_number_lex(parser, bytes[i]);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + number_used;
              }
              return status;
            }
            i += number_used;
            continue;
          }
          if (bytes[i] == 't' || bytes[i] == 'f' || bytes[i] == 'n') {
            status = lonejson__begin_literal_lex(
                parser,
                (bytes[i] == 't')   ? LONEJSON_LEX_TRUE
                : (bytes[i] == 'f') ? LONEJSON_LEX_FALSE
                                    : LONEJSON_LEX_NULL,
                bytes[i]);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
        }
        if (active->state == LONEJSON_FRAME_OBJECT_COMMA_OR_END) {
          if (bytes[i] == ',') {
            if (parser->json_stream_active &&
                !parser->json_stream_parse_visitor_active) {
              status = lonejson__json_value_emit(parser, ",", 1u);
              if (status != LONEJSON_STATUS_OK) {
                parser->failed = 1;
                if (consumed != NULL) {
                  *consumed = i + 1u;
                }
                return status;
              }
            }
            active->state = LONEJSON_FRAME_OBJECT_KEY_OR_END;
            active->after_comma = 1;
            i++;
            continue;
          }
          if (bytes[i] == '}') {
            status = lonejson__finalize_object(parser);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            lonejson__parser_note_workspace_usage(parser);
            i++;
            continue;
          }
        }
      } else if (active->kind == LONEJSON_CONTAINER_ARRAY &&
                 active->state == LONEJSON_FRAME_ARRAY_VALUE_OR_END) {
        if (bytes[i] == '"') {
          status = lonejson__begin_string_value_lex(
              parser, active->field,
              active->field != NULL
                  ? lonejson__field_ptr(active->object_ptr, active->field)
                  : NULL);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (bytes[i] == '-' || lonejson__is_digit(bytes[i])) {
          size_t number_used = 1u;
          if (active->field == NULL && !parser->json_stream_active &&
              len - i >= sizeof(lonejson_uint64))
            status = lonejson__begin_ignored_number(parser, bytes + i, len - i,
                                                    &number_used);
          else
            status = lonejson__begin_number_lex(parser, bytes[i]);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + number_used;
            }
            return status;
          }
          i += number_used;
          continue;
        }
        if (bytes[i] == 't' || bytes[i] == 'f' || bytes[i] == 'n') {
          status = lonejson__begin_literal_lex(
              parser,
              (bytes[i] == 't')   ? LONEJSON_LEX_TRUE
              : (bytes[i] == 'f') ? LONEJSON_LEX_FALSE
                                  : LONEJSON_LEX_NULL,
              bytes[i]);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
      } else if (active->kind == LONEJSON_CONTAINER_ARRAY &&
                 active->state == LONEJSON_FRAME_ARRAY_COMMA_OR_END) {
        if (bytes[i] == ',') {
          if (parser->json_stream_active &&
              !parser->json_stream_parse_visitor_active) {
            status = lonejson__json_value_emit(parser, ",", 1u);
            if (status != LONEJSON_STATUS_OK) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
          }
          active->state = LONEJSON_FRAME_ARRAY_VALUE_OR_END;
          active->after_comma = 1;
          i++;
          continue;
        }
        if (bytes[i] == ']') {
          status = lonejson__finalize_array(parser);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          lonejson__parser_note_workspace_usage(parser);
          i++;
          continue;
        }
      }
      parser->error.offset--;
      parser->error.column--;
      if (lonejson__is_json_space(bytes[i])) {
        do {
          parser->error.offset++;
          if (bytes[i] == '\n') {
            parser->error.line++;
            parser->error.column = 0u;
          } else {
            parser->error.column++;
          }
          i++;
        } while (i < len && lonejson__is_json_space(bytes[i]));
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
    }

    status = lonejson__parser_consume_char(parser, bytes[i]);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      parser->failed = 1;
      if (consumed != NULL) {
        *consumed = i + 1u;
      }
      return status;
    }
    lonejson__parser_note_workspace_usage(parser);
    i++;
  }

  if (consumed != NULL) {
    *consumed = i;
  }
  return parser->error.truncated ? LONEJSON_STATUS_TRUNCATED
                                 : LONEJSON_STATUS_OK;
}
