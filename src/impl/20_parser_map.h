static LONEJSON__INLINE size_t lonejson__popcount_u64(lonejson_uint64 value) {
  lonejson_uint64 all_bits = ~(lonejson_uint64)0u;
  lonejson_uint64 ones = all_bits / 3u;
  lonejson_uint64 twos = all_bits / 5u;
  lonejson_uint64 fours = all_bits / 17u;
  lonejson_uint64 bytes = all_bits / 255u;

  value = value - ((value >> 1u) & ones);
  value = (value & twos) + ((value >> 2u) & twos);
  value = (value + (value >> 4u)) & fours;
  return (size_t)((value * bytes) >> 56u);
}

static size_t lonejson__required_count_for_large_map(const lonejson_map *map) {
  size_t count;
  size_t i;

  if (map == NULL) {
    return 0u;
  }
  count = 0u;
  for (i = 0u; i < map->field_count; ++i) {
    if ((map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      count++;
    }
  }
  return count;
}

static lonejson_uint64
lonejson__required_mask_for_map(lonejson_parser *parser,
                                const lonejson_map *map) {
  size_t i;
  size_t slot;
  lonejson_uint64 mask;

  if (parser == NULL || map == NULL || map->field_count > 64u) {
    return 0u;
  }
  slot = ((size_t)((const void *)map) >> 4u) % LONEJSON__MAP_MASK_CACHE_SIZE;
  if (parser->required_mask_maps[slot] == map) {
    return parser->required_masks[slot];
  }
  mask = 0u;
  for (i = 0u; i < map->field_count; ++i) {
    if ((map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      mask |= ((lonejson_uint64)1u) << i;
    }
  }
  parser->required_mask_maps[slot] = map;
  parser->required_masks[slot] = mask;
  return mask;
}

static size_t lonejson__required_count_for_map(lonejson_parser *parser,
                                               const lonejson_map *map) {
  if (parser == NULL || map == NULL) {
    return 0u;
  }
  if (map->field_count <= 64u) {
    lonejson_uint64 mask = lonejson__required_mask_for_map(parser, map);
    return lonejson__popcount_u64(mask);
  }
  return lonejson__required_count_for_large_map(map);
}

static int lonejson__field_has_presence(const lonejson_field *field);
static size_t lonejson__field_storage_size(const lonejson_field *field);

typedef struct lonejson__map_analysis {
  int valid;
  int may_allocate;
  lonejson_uint64 adopt_mask;
  lonejson_status status;
  size_t depth;
  size_t inline_depth;
  int recursive;
} lonejson__map_analysis;

typedef struct lonejson__map_analysis_frame {
  const lonejson_map *map;
  size_t index;
  int via_array;
  lonejson__map_analysis result;
} lonejson__map_analysis_frame;

#if defined(LONEJSON_TEST_MAP_ANALYSIS_COUNTER)
static size_t g_lonejson_test_map_analysis_count = 0u;
static void lonejson__test_reset_map_analysis_count(void) {
  g_lonejson_test_map_analysis_count = 0u;
}
static size_t lonejson__test_get_map_analysis_count(void) {
  return g_lonejson_test_map_analysis_count;
}
#endif

static lonejson__map_analysis
lonejson__map_analysis_start(const lonejson_map *map) {
  lonejson__map_analysis result;
  memset(&result, 0, sizeof(result));
  result.valid = map != NULL && (map->field_count == 0u || map->fields != NULL);
  result.status =
      result.valid ? LONEJSON_STATUS_OK : LONEJSON_STATUS_INVALID_ARGUMENT;
  result.adopt_mask =
      map == NULL || map->field_count > 64u ? ~(lonejson_uint64)0u : 0u;
  result.depth = result.inline_depth = 1u;
  return result;
}

static void lonejson__map_analysis_adopt(lonejson__map_analysis *result,
                                         size_t index) {
  result->may_allocate = 1;
  if (result->adopt_mask != ~(lonejson_uint64)0u) {
    result->adopt_mask |= (lonejson_uint64)1u << index;
  }
}

static void lonejson__map_analysis_merge(lonejson__map_analysis_frame *parent,
                                         const lonejson__map_analysis *child) {
  const lonejson_field *field = &parent->map->fields[parent->index];
  lonejson__map_analysis *result = &parent->result;
  if (child->depth + 1u > result->depth)
    result->depth = child->depth + 1u;
  if (field->kind == LONEJSON_FIELD_KIND_OBJECT &&
      child->inline_depth + 1u > result->inline_depth)
    result->inline_depth = child->inline_depth + 1u;
  result->recursive |= child->recursive;
  if (field->kind == LONEJSON_FIELD_KIND_OBJECT) {
    if (child->may_allocate)
      lonejson__map_analysis_adopt(result, parent->index);
  } else {
    if (field->storage != LONEJSON_STORAGE_FIXED || child->may_allocate)
      result->may_allocate = 1;
    if (result->adopt_mask != ~(lonejson_uint64)0u)
      result->adopt_mask |= (lonejson_uint64)1u << parent->index;
  }
  ++parent->index;
}

/* Schema traversal is independent of JSON parser depth. Keep only a small
 * inline workspace; deeply nested schemas grow through the selected allocator.
 */
static lonejson__map_analysis
lonejson__analyze_map_with_allocator(const lonejson_map *map,
                                     const lonejson_allocator *allocator) {
  lonejson__map_analysis_frame local[16];
  lonejson__map_analysis_frame *frames = local;
  size_t capacity = sizeof(local) / sizeof(local[0]);
  size_t count = 1u;
  lonejson__map_analysis result = lonejson__map_analysis_start(map);
  if (!result.valid)
    return result;
  frames[0].map = map;
  frames[0].index = 0u;
  frames[0].via_array = 0;
  frames[0].result = result;
#if defined(LONEJSON_TEST_MAP_ANALYSIS_COUNTER)
  ++g_lonejson_test_map_analysis_count;
#endif
  while (count != 0u) {
    lonejson__map_analysis_frame *frame = &frames[count - 1u];
    const lonejson_field *field;
    size_t size;
    int has_presence;
    int kind_accepts_presence;
    if (frame->index == frame->map->field_count) {
      result = frame->result;
      --count;
      if (count != 0u)
        lonejson__map_analysis_merge(&frames[count - 1u], &result);
      continue;
    }
    field = &frame->map->fields[frame->index];
    size = lonejson__field_storage_size(field);
    has_presence = (field->flags & LONEJSON_FIELD_HAS_PRESENCE) != 0u;
    kind_accepts_presence = field->kind == LONEJSON_FIELD_KIND_I64 ||
                            field->kind == LONEJSON_FIELD_KIND_U64 ||
                            field->kind == LONEJSON_FIELD_KIND_F64 ||
                            field->kind == LONEJSON_FIELD_KIND_BOOL;
    if (size == 0u || field->struct_offset > frame->map->struct_size ||
        size > frame->map->struct_size - field->struct_offset ||
        (has_presence &&
         (field->presence_offset > frame->map->struct_size ||
          sizeof(int) > frame->map->struct_size - field->presence_offset ||
          !kind_accepts_presence)) ||
        ((field->flags & LONEJSON_FIELD_ACCEPT_NULL) != 0u &&
         (!has_presence || !kind_accepts_presence)) ||
        ((field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
         (field->flags &
          (LONEJSON_FIELD_OMIT_NULL | LONEJSON_FIELD_OMIT_EMPTY |
           LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL)) != 0u)) {
      result = frame->result;
      result.valid = 0;
      result.status = LONEJSON_STATUS_INVALID_ARGUMENT;
      break;
    }
    if ((field->kind == LONEJSON_FIELD_KIND_OBJECT ||
         field->kind == LONEJSON_FIELD_KIND_OBJECT_ARRAY) &&
        field->submap != NULL) {
      size_t i = count;
      int via_array = field->kind == LONEJSON_FIELD_KIND_OBJECT_ARRAY;
      int cycle = 0;
      while (i != 0u) {
        --i;
        if (frames[i].map == field->submap) {
          cycle = 1;
          break;
        }
        via_array |= frames[i].via_array;
      }
      if (cycle) {
        lonejson__map_analysis child =
            lonejson__map_analysis_start(field->submap);
        if (!via_array) {
          result = child;
          result.valid = 0;
          result.status = LONEJSON_STATUS_INVALID_ARGUMENT;
          break;
        }
        child.may_allocate = child.recursive = 1;
        child.adopt_mask = ~(lonejson_uint64)0u;
        lonejson__map_analysis_merge(frame, &child);
        continue;
      }
      if (count == capacity) {
        lonejson__map_analysis_frame *next;
        size_t bytes;
        if (capacity > SIZE_MAX / (2u * sizeof(*frames))) {
          result.status = LONEJSON_STATUS_ALLOCATION_FAILED;
          result.valid = 0;
          break;
        }
        bytes = capacity * 2u * sizeof(*frames);
        if (frames == local) {
          next = (lonejson__map_analysis_frame *)lonejson__owned_malloc(
              allocator, bytes);
          if (next != NULL)
            memcpy(next, frames, count * sizeof(*frames));
        } else {
          next = (lonejson__map_analysis_frame *)lonejson__owned_realloc(
              allocator, frames, bytes);
        }
        if (next == NULL) {
          result.status = LONEJSON_STATUS_ALLOCATION_FAILED;
          result.valid = 0;
          break;
        }
        frames = next;
        capacity *= 2u;
      }
      frames[count].map = field->submap;
      frames[count].index = 0u;
      frames[count].via_array = field->kind == LONEJSON_FIELD_KIND_OBJECT_ARRAY;
      frames[count].result = lonejson__map_analysis_start(field->submap);
      if (!frames[count].result.valid) {
        result = frames[count].result;
        break;
      }
#if defined(LONEJSON_TEST_MAP_ANALYSIS_COUNTER)
      ++g_lonejson_test_map_analysis_count;
#endif
      ++count;
      continue;
    }
    switch (field->kind) {
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage == LONEJSON_STORAGE_DYNAMIC)
        lonejson__map_analysis_adopt(&frame->result, frame->index);
      break;
    case LONEJSON_FIELD_KIND_STRING_STREAM:
    case LONEJSON_FIELD_KIND_BASE64_STREAM:
    case LONEJSON_FIELD_KIND_STRING_SOURCE:
    case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    case LONEJSON_FIELD_KIND_JSON_VALUE:
    case LONEJSON_FIELD_KIND_STRING_ARRAY:
      lonejson__map_analysis_adopt(&frame->result, frame->index);
      break;
    case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    case LONEJSON_FIELD_KIND_I64_ARRAY:
    case LONEJSON_FIELD_KIND_U64_ARRAY:
    case LONEJSON_FIELD_KIND_F64_ARRAY:
    case LONEJSON_FIELD_KIND_BOOL_ARRAY:
      if (field->storage != LONEJSON_STORAGE_FIXED)
        frame->result.may_allocate = 1;
      if (frame->result.adopt_mask != ~(lonejson_uint64)0u)
        frame->result.adopt_mask |= (lonejson_uint64)1u << frame->index;
      break;
    default:
      break;
    }
    ++frame->index;
  }
  if (frames != local)
    lonejson__owned_free(frames);
  return result;
}

static void lonejson__parser_adopt_existing_map_allocations_with_mask(
    lonejson_parser *parser, const lonejson_map *map, void *value,
    lonejson_uint64 adopt_mask);
static int lonejson__map_may_allocate(const lonejson_map *map);

static void lonejson__parser_init_state(
    lonejson_parser *parser, const lonejson_map *map, void *dst,
    const lonejson__parse_options *options, const lonejson_runtime *runtime,
    int validate_only, int root_map_analysis_known, int root_map_may_allocate,
    lonejson_uint64 root_map_adopt_mask, unsigned char *workspace,
    size_t workspace_size) {
  unsigned char *aligned_workspace = workspace;
  size_t alignment_padding = 0u;

  parser->frames = NULL;
  parser->workspace = NULL;
  parser->workspace_size = 0u;
  parser->workspace_top = 0u;
  parser->workspace_peak = 0u;
  parser->frame_count = 0u;
  parser->self_alloc_size = 0u;
  parser->validate_only = validate_only;
  parser->owns_self = 0;
  parser->root_started = 0;
  parser->root_finished = 0;
  parser->failed = 0;
  parser->lex_mode = LONEJSON_LEX_NONE;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_is_key = 0;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  parser->unicode_accum = 0u;
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  parser->direct_string_active = 0;
  parser->direct_string_ptr = NULL;
  parser->direct_string_dst = NULL;
  parser->direct_string_commit_dst = NULL;
  parser->direct_string_len = 0u;
  parser->direct_string_capacity = 0u;
  parser->direct_string_owned = 0;
  parser->direct_string_truncated = 0;
  parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
  parser->direct_string_field = NULL;
  parser->token.data = NULL;
  parser->token.len = 0u;
  parser->token.cap = 0u;
  memset(parser->required_mask_maps, 0, sizeof(parser->required_mask_maps));
  memset(parser->required_masks, 0, sizeof(parser->required_masks));
  parser->json_stream_value = NULL;
  parser->json_stream_active = 0;
  parser->json_stream_parse_visitor_active = 0;
  parser->json_stream_visit_active = 0;
  parser->json_stream_path_visit_active = 0;
  parser->json_stream_sink_active = 0;
  parser->visit_callback_error = NULL;
  parser->visit_utf8_len = 0u;
  parser->visit_utf8_expected = 0u;
  parser->visit_paused = 0;
  parser->visit_number_phase = LONEJSON__VISIT_NUMBER_NONE;
  parser->json_stream_depth = 0u;
  parser->json_stream_total_bytes = 0u;
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = 0;
  parser->json_stream_path = NULL;
  parser->parse_alloc_bytes_live = 0u;
  parser->root_map_may_allocate = 0;
  parser->root_map_adopt_mask = 0u;
  parser->root_map = map;
  parser->root_dst = dst;
  parser->runtime = runtime;
  parser->options = options ? *options : lonejson__default_parse_options();
  parser->allocator = lonejson__allocator_resolve(parser->options.allocator);
  memset(&parser->error, 0, sizeof(parser->error));
  parser->error.line = 1u;
  parser->error.column = 0u;
  parser->error.code = LONEJSON_STATUS_OK;
  if (workspace != NULL && workspace_size != 0u) {
    aligned_workspace = lonejson__align_pointer(
        workspace, LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
    alignment_padding = (size_t)(aligned_workspace - workspace);
    if (alignment_padding >= workspace_size) {
      workspace_size = 0u;
      aligned_workspace = workspace + workspace_size;
    } else {
      workspace_size -= alignment_padding;
    }
  }
  parser->workspace = aligned_workspace;
  parser->workspace_size = workspace_size;
  parser->workspace_top = workspace_size;
  parser->frames = (lonejson_frame *)aligned_workspace;
  if (workspace_size != 0u) {
    parser->token.data = (char *)lonejson__token_base(parser);
    parser->token.cap = lonejson__token_limit(parser);
    parser->token.data[0] = '\0';
  }
  if (root_map_analysis_known) {
    parser->root_map_may_allocate = root_map_may_allocate;
    parser->root_map_adopt_mask = root_map_adopt_mask;
  } else if (map != NULL) {
    lonejson__map_analysis analysis =
        lonejson__analyze_map_with_allocator(map, &parser->allocator);
    if (!analysis.valid) {
      lonejson__set_error(&parser->error, analysis.status, 0u, 0u, 0u,
                          "failed to analyze record schema");
      parser->failed = 1;
      return;
    }
    parser->root_map_may_allocate = analysis.may_allocate;
    parser->root_map_adopt_mask = analysis.adopt_mask;
  }
  if (!validate_only && map != NULL && dst != NULL &&
      !parser->options.clear_destination &&
      parser->options.max_alloc_bytes != 0u && parser->root_map_may_allocate) {
    lonejson__parser_adopt_existing_map_allocations_with_mask(
        parser, map, dst, parser->root_map_adopt_mask);
  }
  lonejson__parser_note_workspace_usage(parser);
}

static void lonejson__parser_restart_stream(lonejson_parser *parser,
                                            void *dst) {
  parser->root_dst = dst;
  lonejson__direct_string_abort(parser);
  lonejson__parser_cleanup_json_stream_path(parser);
  memset(&parser->error, 0, sizeof(parser->error));
  parser->error.line = 1u;
  parser->error.column = 0u;
  parser->error.code = LONEJSON_STATUS_OK;
  parser->workspace_top = parser->workspace_size;
  parser->frame_count = 0u;
  parser->workspace_peak = 0u;
  parser->root_started = 0;
  parser->root_finished = 0;
  parser->failed = 0;
  parser->parse_alloc_bytes_live = 0u;
  parser->lex_mode = LONEJSON_LEX_NONE;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_is_key = 0;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  parser->unicode_accum = 0u;
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  parser->json_stream_value = NULL;
  parser->json_stream_active = 0;
  parser->json_stream_parse_visitor_active = 0;
  parser->json_stream_visit_active = 0;
  parser->json_stream_path_visit_active = 0;
  parser->json_stream_sink_active = 0;
  parser->json_stream_depth = 0u;
  parser->token.len = 0u;
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  parser->token.data[0] = '\0';
  if (!parser->validate_only && parser->root_map != NULL && dst != NULL &&
      !parser->options.clear_destination &&
      parser->options.max_alloc_bytes != 0u && parser->root_map_may_allocate) {
    lonejson__parser_adopt_existing_map_allocations_with_mask(
        parser, parser->root_map, dst, parser->root_map_adopt_mask);
  }
  lonejson__parser_note_workspace_usage(parser);
}

static LONEJSON__INLINE int lonejson__utf8_append(lonejson_scratch *scratch,
                                                  lonejson_uint32 cp) {
  char *dst;
  size_t len;

  if (scratch == NULL) {
    return 0;
  }
  if (cp <= 0x7Fu) {
    len = 1u;
  } else if (cp <= 0x7FFu) {
    len = 2u;
  } else if (cp <= 0xFFFFu) {
    len = 3u;
  } else if (cp <= 0x10FFFFu) {
    len = 4u;
  } else {
    return 0;
  }
  if (!lonejson__scratch_reserve(scratch, scratch->len + len + 1u)) {
    return 0;
  }
  dst = scratch->data + scratch->len;
  if (len == 1u) {
    dst[0] = (char)cp;
  } else if (len == 2u) {
    dst[0] = (char)(0xC0u | (cp >> 6u));
    dst[1] = (char)(0x80u | (cp & 0x3Fu));
  } else if (len == 3u) {
    dst[0] = (char)(0xE0u | (cp >> 12u));
    dst[1] = (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    dst[2] = (char)(0x80u | (cp & 0x3Fu));
  } else {
    dst[0] = (char)(0xF0u | (cp >> 18u));
    dst[1] = (char)(0x80u | ((cp >> 12u) & 0x3Fu));
    dst[2] = (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    dst[3] = (char)(0x80u | (cp & 0x3Fu));
  }
  scratch->len += len;
  return 1;
}

static LONEJSON__INLINE int
lonejson__field_key_matches(const lonejson_field *field, const char *key,
                            size_t key_len) {
  if (field == NULL || field->json_key_len != key_len) {
    return 0;
  }
  if (key_len == 0u) {
    return 1;
  }
  if (field->json_key_first != (unsigned char)key[0] ||
      field->json_key_last != (unsigned char)key[key_len - 1u]) {
    return 0;
  }
  if (key_len <= 2u) {
    return 1;
  }
  return memcmp(field->json_key + 1u, key + 1u, key_len - 2u) == 0;
}

static LONEJSON__INLINE size_t
lonejson__field_index(const lonejson_map *map, const lonejson_field *field) {
  if (map == NULL || field == NULL || field < map->fields ||
      field >= map->fields + map->field_count) {
    return SIZE_MAX;
  }
  return (size_t)(field - map->fields);
}

static const lonejson_field *lonejson__find_field(const lonejson_map *map,
                                                  lonejson_frame *frame,
                                                  const char *key,
                                                  size_t key_len) {
  size_t i;

  if (map == NULL) {
    return NULL;
  }
  if (key_len != 0u && frame != NULL &&
      frame->next_field_hint < map->field_count &&
      lonejson__field_key_matches(&map->fields[frame->next_field_hint], key,
                                  key_len)) {
    const lonejson_field *field = &map->fields[frame->next_field_hint];
    frame->next_field_hint++;
    return field;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (lonejson__field_key_matches(field, key, key_len)) {
      if (frame != NULL) {
        frame->next_field_hint = i + 1u;
      }
      return field;
    }
  }
  return NULL;
}

static LONEJSON__INLINE void *lonejson__field_ptr(void *base,
                                                  const lonejson_field *field) {
  return (unsigned char *)base + field->struct_offset;
}

static LONEJSON__INLINE const void *
lonejson__field_cptr(const void *base, const lonejson_field *field) {
  return (const unsigned char *)base + field->struct_offset;
}

static LONEJSON__INLINE const void *
lonejson__field_presence_cptr(const void *base, const lonejson_field *field) {
  return (const unsigned char *)base + field->presence_offset;
}

static int lonejson__field_has_presence(const lonejson_field *field) {
  return (field->flags & LONEJSON_FIELD_HAS_PRESENCE) != 0u;
}

static int lonejson__field_presence_is_set(const void *base,
                                           const lonejson_field *field) {
  if (!lonejson__field_has_presence(field)) {
    return 1;
  }
  return *(const int *)lonejson__field_presence_cptr(base, field) != 0;
}

static void lonejson__field_set_presence(void *base,
                                         const lonejson_field *field,
                                         int present) {
  if (!lonejson__field_has_presence(field)) {
    return;
  }
  *(int *)((unsigned char *)base + field->presence_offset) = present ? 1 : 0;
}

static size_t lonejson__field_storage_size(const lonejson_field *field) {
  if (field == NULL) {
    return 0u;
  }
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    return (field->storage == LONEJSON_STORAGE_DYNAMIC) ? sizeof(char *)
                                                        : field->fixed_capacity;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return sizeof(lonejson_spooled);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return sizeof(lonejson_source);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return sizeof(lonejson_json_value);
  case LONEJSON_FIELD_KIND_I64:
    return sizeof(lonejson_int64);
  case LONEJSON_FIELD_KIND_U64:
    return sizeof(lonejson_uint64);
  case LONEJSON_FIELD_KIND_F64:
    return sizeof(double);
  case LONEJSON_FIELD_KIND_BOOL:
    return sizeof(bool);
  case LONEJSON_FIELD_KIND_OBJECT:
    return field->submap ? field->submap->struct_size : 0u;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
    return sizeof(lonejson_string_array);
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM:
    return sizeof(lonejson_string_array_stream);
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM:
    return sizeof(lonejson_mapped_array_stream);
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    return sizeof(lonejson_i64_array);
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    return sizeof(lonejson_u64_array);
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    return sizeof(lonejson_f64_array);
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    return sizeof(lonejson_bool_array);
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    return sizeof(lonejson_object_array);
  default:
    return 0u;
  }
}

static int lonejson__field_is_empty_for_omit(const lonejson_field *field,
                                             const void *ptr);

static int lonejson__map_has_serialized_fields(const lonejson_map *map,
                                               const void *src) {
  size_t i;

  if (map == NULL || src == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (!lonejson__field_is_empty_for_omit(field,
                                           lonejson__field_cptr(src, field))) {
      return 1;
    }
  }
  return 0;
}

static int lonejson__field_is_empty_for_omit(const lonejson_field *field,
                                             const void *ptr) {
  if (!lonejson__field_has_presence(field) ||
      lonejson__field_presence_is_set(
          (const unsigned char *)ptr - field->struct_offset, field)) {
    unsigned omit_flags = field->flags;

    if ((omit_flags & LONEJSON_FIELD_OMIT_EMPTY) != 0u) {
      omit_flags |= LONEJSON_FIELD_OMIT_NULL;
    }
    if ((omit_flags & (LONEJSON_FIELD_OMIT_NULL | LONEJSON_FIELD_OMIT_EMPTY)) ==
        0u) {
      return 0;
    }
    if ((omit_flags & LONEJSON_FIELD_OMIT_NULL) != 0u) {
      switch (field->kind) {
      case LONEJSON_FIELD_KIND_STRING:
        if (field->storage == LONEJSON_STORAGE_DYNAMIC &&
            *(const char *const *)ptr == NULL) {
          return 1;
        }
        break;
      case LONEJSON_FIELD_KIND_STRING_SOURCE:
      case LONEJSON_FIELD_KIND_BASE64_SOURCE:
        if (((const lonejson_source *)ptr)->kind == LONEJSON_SOURCE_NONE) {
          return 1;
        }
        break;
      case LONEJSON_FIELD_KIND_JSON_VALUE:
        if (((const lonejson_json_value *)ptr)->kind ==
            LONEJSON_JSON_VALUE_NULL) {
          return 1;
        }
        break;
      default:
        break;
      }
    }
    if ((omit_flags & LONEJSON_FIELD_OMIT_EMPTY) != 0u) {
      switch (field->kind) {
      case LONEJSON_FIELD_KIND_STRING:
        if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
          const char *value = *(const char *const *)ptr;
          return value == NULL || value[0] == '\0';
        }
        return field->fixed_capacity == 0u || *(const char *)ptr == '\0';
      case LONEJSON_FIELD_KIND_STRING_STREAM:
      case LONEJSON_FIELD_KIND_BASE64_STREAM:
        return lonejson_spooled_size((const lonejson_spooled *)ptr) == 0u;
      case LONEJSON_FIELD_KIND_OBJECT:
        return !lonejson__map_has_serialized_fields(field->submap, ptr);
      case LONEJSON_FIELD_KIND_STRING_ARRAY:
        return ((const lonejson_string_array *)ptr)->count == 0u;
      case LONEJSON_FIELD_KIND_I64_ARRAY:
        return ((const lonejson_i64_array *)ptr)->count == 0u;
      case LONEJSON_FIELD_KIND_U64_ARRAY:
        return ((const lonejson_u64_array *)ptr)->count == 0u;
      case LONEJSON_FIELD_KIND_F64_ARRAY:
        return ((const lonejson_f64_array *)ptr)->count == 0u;
      case LONEJSON_FIELD_KIND_BOOL_ARRAY:
        return ((const lonejson_bool_array *)ptr)->count == 0u;
      case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
        return ((const lonejson_object_array *)ptr)->count == 0u;
      default:
        break;
      }
    }
    return 0;
  }
  return 1;
}

static int lonejson__field_should_omit(const void *base,
                                       const lonejson_field *field) {
  if ((field->flags & LONEJSON_FIELD_REQUIRED) != 0u) {
    return 0;
  }
  return lonejson__field_is_empty_for_omit(field,
                                           lonejson__field_cptr(base, field));
}

static int lonejson__map_is_rewindable_for_serialize(const lonejson_map *map,
                                                     const void *src);

static int
lonejson__field_is_rewindable_for_serialize(const lonejson_field *field,
                                            const void *ptr) {
  size_t i;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson_source_is_rewindable((const lonejson_source *)ptr);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson_json_value_is_rewindable((const lonejson_json_value *)ptr);
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__map_is_rewindable_for_serialize(field->submap, ptr);
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    for (i = 0u; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (!lonejson__map_is_rewindable_for_serialize(field->submap, elem)) {
        return 0;
      }
    }
    return 1;
  }
  default:
    return 1;
  }
}

static int lonejson__map_is_rewindable_for_serialize(const lonejson_map *map,
                                                     const void *src) {
  size_t i;

  if (map == NULL || src == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (lonejson__field_should_omit(src, field)) {
      continue;
    }
    if (!lonejson__field_is_rewindable_for_serialize(
            field, lonejson__field_cptr(src, field))) {
      return 0;
    }
  }
  return 1;
}

static lonejson_frame *lonejson__push_frame(lonejson_parser *parser,
                                            lonejson_container_kind kind) {
  lonejson_frame *frame;

  if (parser->options.max_depth != 0 &&
      parser->frame_count >= parser->options.max_depth) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column, "maximum nesting depth exceeded");
    parser->failed = 1;
    return NULL;
  }
  if (lonejson__frame_bytes(parser->frame_count + 1u) > parser->workspace_top) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column,
                        "parser workspace exhausted by nesting depth");
    parser->failed = 1;
    return NULL;
  }
  frame = &parser->frames[parser->frame_count++];
  memset(frame, 0, sizeof(*frame));
  frame->kind = kind;
  frame->state = (kind == LONEJSON_CONTAINER_OBJECT)
                     ? LONEJSON_FRAME_OBJECT_KEY_OR_END
                     : LONEJSON_FRAME_ARRAY_VALUE_OR_END;
  return frame;
}

static void lonejson__pop_frame(lonejson_parser *parser) {
  lonejson_frame *frame;

  if (parser->frame_count == 0) {
    return;
  }
  frame = &parser->frames[parser->frame_count - 1u];
  if (frame->seen_fields != NULL) {
    parser->workspace_top += frame->seen_bytes;
    frame->seen_fields = NULL;
    frame->seen_bytes = 0u;
  }
  parser->frame_count--;
}

typedef enum lonejson__array_ensure_result {
  LONEJSON__ARRAY_ENSURE_OK = 1,
  LONEJSON__ARRAY_ENSURE_TRUNCATED = 0,
  LONEJSON__ARRAY_ENSURE_ERROR = -1
} lonejson__array_ensure_result;

static lonejson__array_ensure_result
lonejson__array_ensure_bytes(lonejson_parser *parser, void **items_ptr,
                             size_t *cap_ptr, size_t elem_size, unsigned *flags,
                             size_t want, lonejson_overflow_policy policy,
                             lonejson_error *error) {
  void *next;
  size_t cap;

  if (*cap_ptr >= want) {
    return LONEJSON__ARRAY_ENSURE_OK;
  }
  if ((*flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
    if (policy == LONEJSON_OVERFLOW_FAIL) {
      lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                          error->line, error->column,
                          "array capacity exceeded");
      return LONEJSON__ARRAY_ENSURE_ERROR;
    }
    error->truncated = 1;
    return LONEJSON__ARRAY_ENSURE_TRUNCATED;
  }
  cap = (*cap_ptr != 0u) ? *cap_ptr : 4u;
  while (cap < want) {
    if (cap > (SIZE_MAX / 2u)) {
      lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                          error->line, error->column, "array too large");
      return LONEJSON__ARRAY_ENSURE_ERROR;
    }
    cap *= 2u;
  }
  if (!lonejson__parser_alloc_can_grow(
          parser, lonejson__parser_alloc_counted_bytes(parser, *items_ptr),
          cap * elem_size)) {
    lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                        error->line, error->column,
                        "parse allocations exceed configured max bytes");
    return LONEJSON__ARRAY_ENSURE_ERROR;
  }
  next = lonejson__owned_realloc_parse(parser, *items_ptr, cap * elem_size);
  if (next == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, error->offset,
                        error->line, error->column, "failed to grow array");
    return LONEJSON__ARRAY_ENSURE_ERROR;
  }
  *items_ptr = next;
  *cap_ptr = cap;
  *flags |= LONEJSON_ARRAY_OWNS_ITEMS;
  return LONEJSON__ARRAY_ENSURE_OK;
}

static void lonejson__cleanup_leaf(const lonejson_field *field, void *ptr);
static void lonejson__cleanup_leaf_parse(lonejson_parser *parser,
                                         const lonejson_field *field,
                                         void *ptr);
static void lonejson__cleanup_value_parse(lonejson_parser *parser,
                                          const lonejson_field *field,
                                          void *ptr);
static void lonejson__reset_map_parse(lonejson_parser *parser,
                                      const lonejson_map *map, void *value);
static lonejson_status lonejson__cleanup_map_checked(const lonejson_map *map,
                                                     void *value);

#define LONEJSON__MAP_FLAG_CACHE_SIZE 16u
#define LONEJSON__MAP_COOKIE LONEJSON__MAP_COOKIE_VALUE
#define LONEJSON__MAP_FLAG_CACHE_ENABLED LONEJSON__HAS_TLS
typedef struct lonejson__map_flag_cache_entry {
  const lonejson_map *map;
  lonejson_uint64 cookie;
  int known;
  lonejson__map_analysis analysis;
} lonejson__map_flag_cache_entry;

static LONEJSON__INLINE lonejson__map_flag_cache_entry *
lonejson__map_flag_cache_slot(const lonejson_map *map) {
#if LONEJSON__MAP_FLAG_CACHE_ENABLED
  static LONEJSON__TLS lonejson__map_flag_cache_entry
      cache[LONEJSON__MAP_FLAG_CACHE_SIZE];
  uintptr_t bits = (uintptr_t)map;
  size_t index = (size_t)((bits >> 4u) ^ (bits >> 9u)) &
                 (LONEJSON__MAP_FLAG_CACHE_SIZE - 1u);
  if (cache[index].map != map || cache[index].cookie != map->_map_cookie) {
    cache[index].map = map;
    cache[index].cookie = map->_map_cookie;
    cache[index].known = 0;
  }
  return &cache[index];
#else
  (void)map;
  return NULL;
#endif
}

static LONEJSON__INLINE int lonejson__map_cacheable(const lonejson_map *map) {
  return map != NULL && map->_map_identity == map;
}

static lonejson__map_analysis lonejson__map_analysis_cached_with_allocator(
    const lonejson_map *map, const lonejson_allocator *allocator) {
  lonejson__map_flag_cache_entry *entry;
  lonejson__map_analysis analysis;
  if (!LONEJSON__MAP_FLAG_CACHE_ENABLED || !lonejson__map_cacheable(map))
    return lonejson__analyze_map_with_allocator(map, allocator);
  entry = lonejson__map_flag_cache_slot(map);
  if (entry->known)
    return entry->analysis;
  analysis = lonejson__analyze_map_with_allocator(map, allocator);
  if (analysis.status != LONEJSON_STATUS_ALLOCATION_FAILED) {
    entry->analysis = analysis;
    entry->known = 1;
  }
  return analysis;
}

static lonejson__map_analysis
lonejson__map_analysis_cached(const lonejson_map *map) {
  return lonejson__map_analysis_cached_with_allocator(map, NULL);
}
static int lonejson__map_layout_is_valid(const lonejson_map *map) {
  return lonejson__map_analysis_cached(map).valid;
}
static int lonejson__map_may_allocate(const lonejson_map *map) {
  lonejson__map_analysis analysis = lonejson__map_analysis_cached(map);
  return analysis.valid ? analysis.may_allocate : 1;
}

static int lonejson__parser_adopt_owned_pointer(lonejson_parser *parser,
                                                void *ptr) {
  if (ptr == NULL || parser == NULL || parser->options.max_alloc_bytes == 0u) {
    return 1;
  }
  if (lonejson__parser_alloc_try_adopt(parser, ptr)) {
    return 1;
  }
  lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                      parser->error.offset, parser->error.line,
                      parser->error.column,
                      "parse allocations exceed configured max bytes");
  parser->failed = 1;
  return 0;
}

static int lonejson__parser_adopt_leaf(lonejson_parser *parser,
                                       const lonejson_field *field, void *ptr) {
  size_t i;

  if (parser == NULL || field == NULL || ptr == NULL || parser->failed ||
      parser->options.max_alloc_bytes == 0u) {
    return parser == NULL || !parser->failed;
  }

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      return lonejson__parser_adopt_owned_pointer(parser, *(void **)ptr);
    }
    return 1;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__parser_adopt_owned_pointer(
        parser, ((lonejson_spooled *)ptr)->memory);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__parser_adopt_owned_pointer(parser,
                                                ((lonejson_source *)ptr)->path);
  case LONEJSON_FIELD_KIND_JSON_VALUE: {
    lonejson_json_value *value = (lonejson_json_value *)ptr;
    if (!lonejson__parser_adopt_owned_pointer(parser, value->json)) {
      return 0;
    }
    return lonejson__parser_adopt_owned_pointer(parser, value->path);
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u &&
        !lonejson__parser_adopt_owned_pointer(parser, arr->items)) {
      return 0;
    }
    for (i = 0u; i < arr->count; ++i) {
      if (!lonejson__parser_adopt_owned_pointer(parser, arr->items[i])) {
        return 0;
      }
    }
    return 1;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    return ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) == 0u) ||
           lonejson__parser_adopt_owned_pointer(parser, arr->items);
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    return ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) == 0u) ||
           lonejson__parser_adopt_owned_pointer(parser, arr->items);
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    return ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) == 0u) ||
           lonejson__parser_adopt_owned_pointer(parser, arr->items);
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    return ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) == 0u) ||
           lonejson__parser_adopt_owned_pointer(parser, arr->items);
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u &&
        !lonejson__parser_adopt_owned_pointer(parser, arr->items)) {
      return 0;
    }
    return 1;
  }
  default:
    return 1;
  }
}

static void lonejson__cleanup_leaf_parse(lonejson_parser *parser,
                                         const lonejson_field *field,
                                         void *ptr) {
  size_t i;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      char **p = (char **)ptr;
      lonejson__parser_alloc_note_release(parser, *p);
      lonejson__owned_free(*p);
      *p = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson__spooled_reset_parse(parser, (lonejson_spooled *)ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson__source_reset_parse(parser, (lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson__json_value_clear_runtime_parse(parser,
                                             (lonejson_json_value *)ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    char **items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    for (i = 0; i < arr->count; ++i) {
      lonejson__parser_alloc_note_release(parser, arr->items[i]);
      lonejson__owned_free(arr->items[i]);
    }
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__parser_alloc_note_release(parser, arr->items);
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM: {
    lonejson_string_array_stream *stream = (lonejson_string_array_stream *)ptr;
    if (!lonejson__string_array_stream_is_initialized(stream)) {
      memset(stream, 0, sizeof(*stream));
      stream->_lonejson_magic =
          lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
    }
    lonejson__string_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM: {
    lonejson_mapped_array_stream *stream = (lonejson_mapped_array_stream *)ptr;
    if (!lonejson__mapped_array_stream_is_initialized(stream)) {
      memset(stream, 0, sizeof(*stream));
      stream->_lonejson_magic =
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
    }
    lonejson__mapped_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_int64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__parser_alloc_note_release(parser, arr->items);
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_uint64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__parser_alloc_note_release(parser, arr->items);
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    double *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__parser_alloc_note_release(parser, arr->items);
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    bool *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__parser_alloc_note_release(parser, arr->items);
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    void *items = arr->items;
    size_t capacity = arr->capacity;
    size_t elem_size = arr->elem_size;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__parser_alloc_note_release(parser, arr->items);
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->elem_size = elem_size;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  default:
    break;
  }
}

static void lonejson__reset_scalar_field(const lonejson_field *field,
                                         void *ptr) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_I64:
    *(lonejson_int64 *)ptr = 0;
    break;
  case LONEJSON_FIELD_KIND_U64:
    *(lonejson_uint64 *)ptr = 0u;
    break;
  case LONEJSON_FIELD_KIND_F64:
    *(double *)ptr = 0.0;
    break;
  case LONEJSON_FIELD_KIND_BOOL:
    *(bool *)ptr = false;
    break;
  default:
    break;
  }
}

static void lonejson__reset_present_array_field(lonejson_parser *parser,
                                                const lonejson_field *field,
                                                void *ptr) {
  if (field == NULL || ptr == NULL) {
    return;
  }
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson__cleanup_value_parse(parser, field, ptr);
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    if (lonejson__is_exact_fixed_capacity(((lonejson_i64_array *)ptr)->flags)) {
      ((lonejson_i64_array *)ptr)->count = 0u;
      break;
    }
    lonejson__cleanup_value_parse(parser, field, ptr);
    break;
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    if (lonejson__is_exact_fixed_capacity(((lonejson_u64_array *)ptr)->flags)) {
      ((lonejson_u64_array *)ptr)->count = 0u;
      break;
    }
    lonejson__cleanup_value_parse(parser, field, ptr);
    break;
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    if (lonejson__is_exact_fixed_capacity(((lonejson_f64_array *)ptr)->flags)) {
      ((lonejson_f64_array *)ptr)->count = 0u;
      break;
    }
    lonejson__cleanup_value_parse(parser, field, ptr);
    break;
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    if (lonejson__is_exact_fixed_capacity(
            ((lonejson_bool_array *)ptr)->flags)) {
      ((lonejson_bool_array *)ptr)->count = 0u;
      break;
    }
    lonejson__cleanup_value_parse(parser, field, ptr);
    break;
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    if (lonejson__is_exact_fixed_capacity(
            ((lonejson_object_array *)ptr)->flags) &&
        !lonejson__map_may_allocate(field->submap)) {
      ((lonejson_object_array *)ptr)->count = 0u;
      break;
    }
    lonejson__cleanup_value_parse(parser, field, ptr);
    break;
  default:
    break;
  }
}

static void lonejson__init_json_value_field(lonejson_json_value *value,
                                            const lonejson_allocator *allocator,
                                            const lonejson_runtime *runtime,
                                            unsigned flags) {
  lonejson_json_value_init_with_allocator(value, allocator);
  if (value != NULL && runtime != NULL) {
    value->parse_visitor_limits = runtime->value_limits;
    value->runtime_parse_visitor_limits = runtime->value_limits;
    value->parse_limits_follow_runtime = 1;
  }
  if ((flags & LONEJSON__FIELD_JSON_VALUE_DEFAULT_CAPTURE) != 0u) {
    lonejson_json_value_enable_parse_capture(value, NULL);
  }
}

static void lonejson__reseed_json_value_field(
    lonejson_json_value *value, const lonejson_allocator *allocator,
    const lonejson_runtime *runtime, unsigned flags) {
  lonejson_allocator resolved_allocator;

  if (!lonejson__json_value_is_initialized(value)) {
    lonejson__init_json_value_field(value, allocator, runtime, flags);
    return;
  }
  lonejson__json_value_clear_runtime(value);
  resolved_allocator = LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)
                           ? lonejson__allocator_resolve(allocator)
                           : lonejson_default_allocator();
  if (!lonejson__allocator_equal(&value->allocator, &resolved_allocator)) {
    value->allocator = resolved_allocator;
  }
  if (runtime != NULL && value->parse_limits_follow_runtime) {
    value->parse_visitor_limits = runtime->value_limits;
  }
  if (runtime != NULL) {
    value->runtime_parse_visitor_limits = runtime->value_limits;
  }
  if ((flags & LONEJSON__FIELD_JSON_VALUE_DEFAULT_CAPTURE) != 0u &&
      value->parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE) {
    lonejson_json_value_enable_parse_capture(value, NULL);
  }
}

static void lonejson__reseed_json_value_field_parse(lonejson_parser *parser,
                                                    lonejson_json_value *value,
                                                    unsigned flags) {
  lonejson_allocator resolved_allocator;

  if (!lonejson__json_value_is_initialized(value)) {
    lonejson__init_json_value_field(value, &parser->allocator, parser->runtime,
                                    flags);
    return;
  }
  lonejson__json_value_clear_runtime_parse(parser, value);
  resolved_allocator = lonejson__allocator_resolve(&parser->allocator);
  if (!lonejson__allocator_equal(&value->allocator, &resolved_allocator)) {
    value->allocator = resolved_allocator;
  }
  if (parser->runtime != NULL && value->parse_limits_follow_runtime) {
    value->parse_visitor_limits = parser->runtime->value_limits;
  }
  if (parser->runtime != NULL) {
    value->runtime_parse_visitor_limits = parser->runtime->value_limits;
  }
  if ((flags & LONEJSON__FIELD_JSON_VALUE_DEFAULT_CAPTURE) != 0u &&
      value->parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE) {
    lonejson_json_value_enable_parse_capture(value, NULL);
  }
}

static void lonejson__init_spooled_field(lonejson_spooled *value,
                                         const lonejson_field *field,
                                         const lonejson_allocator *allocator,
                                         const lonejson_runtime *runtime) {
  const lonejson__spool_options *options =
      field != NULL && field->reserved_policy != NULL
          ? (const lonejson__spool_options *)field->reserved_policy
          : lonejson__runtime_spool_options_for_class(runtime,
                                                      field->spool_class);
  lonejson_spooled_init_with_allocator(value, options, allocator);
}

static void lonejson__reseed_spooled_field(lonejson_spooled *value,
                                           const lonejson_field *field,
                                           const lonejson_allocator *allocator,
                                           const lonejson_runtime *runtime) {
  const lonejson__spool_options *options =
      field != NULL && field->reserved_policy != NULL
          ? (const lonejson__spool_options *)field->reserved_policy
          : lonejson__runtime_spool_options_for_class(runtime,
                                                      field->spool_class);
  if (lonejson__spooled_is_initialized(value)) {
    lonejson_spooled_cleanup(value);
  }
  lonejson_spooled_init_with_allocator(value, options, allocator);
}

static void lonejson__reseed_spooled_field_parse(lonejson_parser *parser,
                                                 const lonejson_field *field,
                                                 lonejson_spooled *value) {
  const lonejson__spool_options *options =
      field != NULL && field->reserved_policy != NULL
          ? (const lonejson__spool_options *)field->reserved_policy
          : lonejson__runtime_spool_options_for_class(parser->runtime,
                                                      field->spool_class);
  if (lonejson__spooled_is_initialized(value)) {
    lonejson__parser_alloc_note_release(parser, value->memory);
    lonejson__parser_alloc_note_release(parser, value->owned_temp_dir);
    lonejson_spooled_cleanup(value);
  }
  lonejson_spooled_init_with_allocator(value, options, &parser->allocator);
  if (value->owned_temp_dir != NULL &&
      !lonejson__parser_adopt_owned_pointer(parser, value->owned_temp_dir)) {
    lonejson_spooled_cleanup(value);
  }
}

static void lonejson__reset_field(const lonejson_field *field, void *value,
                                  const lonejson_runtime *runtime) {
  void *ptr = lonejson__field_ptr(value, field);
  const lonejson_allocator *allocator =
      runtime != NULL ? runtime->config.allocator : NULL;
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson__reseed_json_value_field((lonejson_json_value *)ptr, allocator,
                                      runtime, field->flags);
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson__reseed_spooled_field((lonejson_spooled *)ptr, field, allocator,
                                   runtime);
    break;
  case LONEJSON_FIELD_KIND_I64:
  case LONEJSON_FIELD_KIND_U64:
  case LONEJSON_FIELD_KIND_F64:
  case LONEJSON_FIELD_KIND_BOOL:
    lonejson__reset_scalar_field(field, ptr);
    lonejson__field_set_presence(value, field, 0);
    break;
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_FIXED &&
        field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
      break;
    }
    lonejson__cleanup_leaf(field, ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
  case LONEJSON_FIELD_KIND_I64_ARRAY:
  case LONEJSON_FIELD_KIND_U64_ARRAY:
  case LONEJSON_FIELD_KIND_F64_ARRAY:
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    lonejson__cleanup_leaf(field, ptr);
    break;
  default:
    lonejson__cleanup_leaf(field, ptr);
    break;
  }
}

static void lonejson__reset_field_parse(lonejson_parser *parser,
                                        const lonejson_field *field,
                                        void *value) {
  void *ptr = lonejson__field_ptr(value, field);
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson__reseed_json_value_field_parse(parser, (lonejson_json_value *)ptr,
                                            field->flags);
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson__reseed_spooled_field_parse(parser, field,
                                         (lonejson_spooled *)ptr);
    break;
  case LONEJSON_FIELD_KIND_I64:
  case LONEJSON_FIELD_KIND_U64:
  case LONEJSON_FIELD_KIND_F64:
  case LONEJSON_FIELD_KIND_BOOL:
    lonejson__reset_scalar_field(field, ptr);
    lonejson__field_set_presence(value, field, 0);
    break;
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_FIXED &&
        field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
      break;
    }
    lonejson__cleanup_leaf_parse(parser, field, ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
  case LONEJSON_FIELD_KIND_I64_ARRAY:
  case LONEJSON_FIELD_KIND_U64_ARRAY:
  case LONEJSON_FIELD_KIND_F64_ARRAY:
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    lonejson__cleanup_leaf_parse(parser, field, ptr);
    break;
  default:
    lonejson__cleanup_leaf_parse(parser, field, ptr);
    break;
  }
}

static void lonejson__init_value(const lonejson_field *field, void *ptr,
                                 const lonejson_allocator *allocator,
                                 const lonejson_runtime *runtime) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      *(char **)ptr = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson__init_spooled_field((lonejson_spooled *)ptr, field, allocator,
                                 runtime);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson__init_json_value_field((lonejson_json_value *)ptr, allocator,
                                    runtime, field->flags);
    break;
  case LONEJSON_FIELD_KIND_I64:
  case LONEJSON_FIELD_KIND_U64:
  case LONEJSON_FIELD_KIND_F64:
  case LONEJSON_FIELD_KIND_BOOL:
    lonejson__reset_scalar_field(field, ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM: {
    lonejson_string_array_stream *stream = (lonejson_string_array_stream *)ptr;
    memset(stream, 0, sizeof(*stream));
    stream->_lonejson_magic =
        lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
    lonejson__string_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM: {
    lonejson_mapped_array_stream *stream = (lonejson_mapped_array_stream *)ptr;
    memset(stream, 0, sizeof(*stream));
    stream->_lonejson_magic =
        lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
    lonejson__mapped_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    arr->elem_size = field->elem_size;
    break;
  }
  default:
    break;
  }
}

static int lonejson__init_map_parse(lonejson_parser *parser,
                                    const lonejson_map *map, void *value);

static void lonejson__init_value_parse(lonejson_parser *parser,
                                       const lonejson_field *field, void *ptr) {
  if (parser == NULL || field == NULL || ptr == NULL || parser->failed) {
    return;
  }
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      *(char **)ptr = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson__reseed_spooled_field_parse(parser, field,
                                         (lonejson_spooled *)ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson__reseed_json_value_field_parse(parser, (lonejson_json_value *)ptr,
                                            field->flags);
    break;
  case LONEJSON_FIELD_KIND_I64:
  case LONEJSON_FIELD_KIND_U64:
  case LONEJSON_FIELD_KIND_F64:
  case LONEJSON_FIELD_KIND_BOOL:
    lonejson__reset_scalar_field(field, ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM: {
    lonejson_string_array_stream *stream = (lonejson_string_array_stream *)ptr;
    if (!lonejson__string_array_stream_is_initialized(stream)) {
      memset(stream, 0, sizeof(*stream));
      stream->_lonejson_magic =
          lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
    }
    lonejson__string_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM: {
    lonejson_mapped_array_stream *stream = (lonejson_mapped_array_stream *)ptr;
    if (!lonejson__mapped_array_stream_is_initialized(stream)) {
      memset(stream, 0, sizeof(*stream));
      stream->_lonejson_magic =
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
    }
    lonejson__mapped_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    arr->elem_size = field->elem_size;
    break;
  }
  default:
    break;
  }
}

static void lonejson__cleanup_leaf(const lonejson_field *field, void *ptr) {
  size_t i;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      char **p = (char **)ptr;
      lonejson__owned_free(*p);
      *p = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson_spooled_cleanup((lonejson_spooled *)ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_cleanup((lonejson_source *)ptr);
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson_json_value_cleanup((lonejson_json_value *)ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    char **items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    for (i = 0; i < arr->count; ++i) {
      lonejson__owned_free(arr->items[i]);
    }
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM: {
    lonejson_string_array_stream *stream = (lonejson_string_array_stream *)ptr;
    if (!lonejson__string_array_stream_is_initialized(stream)) {
      memset(stream, 0, sizeof(*stream));
      stream->_lonejson_magic =
          lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
    }
    lonejson__string_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM: {
    lonejson_mapped_array_stream *stream = (lonejson_mapped_array_stream *)ptr;
    if (!lonejson__mapped_array_stream_is_initialized(stream)) {
      memset(stream, 0, sizeof(*stream));
      stream->_lonejson_magic =
          lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
    }
    lonejson__mapped_array_stream_assign_methods(stream);
    stream->active = 0;
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_int64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_uint64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    double *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    bool *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    void *items = arr->items;
    size_t capacity = arr->capacity;
    size_t elem_size = arr->elem_size;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->elem_size = elem_size;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  default:
    break;
  }
}

/* Map lifecycle traversal has no relationship to the JSON parser's depth
 * limit. Keep its explicit stack small for ordinary maps and grow on the heap
 * before touching the record. Cleanup/reset of recursive arrays first measure
 * the actual record so allocation failure leaves it available for retry. */
enum lonejson__record_operation {
  LONEJSON__RECORD_INIT,
  LONEJSON__RECORD_RESET,
  LONEJSON__RECORD_CLEANUP,
  LONEJSON__RECORD_ADOPT
};
typedef struct lonejson__record_frame {
  const lonejson_map *map;
  void *value;
  size_t index;
  size_t array_index;
  int operation;
  int zero_on_exit;
} lonejson__record_frame;

typedef struct lonejson__record_workspace {
  lonejson__record_frame *frames;
  size_t capacity;
  lonejson__record_frame local[16];
} lonejson__record_workspace;

static int lonejson__record_reserve(lonejson__record_workspace *workspace,
                                    size_t capacity,
                                    const lonejson_allocator *allocator) {
  lonejson__record_frame *next;
  if (capacity <= workspace->capacity)
    return 1;
  if (capacity > SIZE_MAX / sizeof(*next))
    return 0;
  if (workspace->frames == workspace->local) {
    next = (lonejson__record_frame *)lonejson__owned_malloc(
        allocator, capacity * sizeof(*next));
    if (next != NULL)
      memcpy(next, workspace->local, sizeof(workspace->local));
  } else {
    next = (lonejson__record_frame *)lonejson__owned_realloc(
        allocator, workspace->frames, capacity * sizeof(*next));
  }
  if (next == NULL)
    return 0;
  workspace->frames = next;
  workspace->capacity = capacity;
  return 1;
}

static void lonejson__record_apply_field(int operation, lonejson_parser *parser,
                                         const lonejson_field *field,
                                         void *value,
                                         const lonejson_allocator *allocator,
                                         const lonejson_runtime *runtime) {
  void *ptr = lonejson__field_ptr(value, field);
  switch (operation) {
  case LONEJSON__RECORD_INIT:
    if (parser != NULL)
      lonejson__init_value_parse(parser, field, ptr);
    else
      lonejson__init_value(field, ptr, allocator, runtime);
    break;
  case LONEJSON__RECORD_RESET:
    if (parser != NULL)
      lonejson__reset_field_parse(parser, field, value);
    else
      lonejson__reset_field(field, value, runtime);
    break;
  case LONEJSON__RECORD_CLEANUP:
    if (parser != NULL)
      lonejson__cleanup_leaf_parse(parser, field, ptr);
    else
      lonejson__cleanup_leaf(field, ptr);
    break;
  case LONEJSON__RECORD_ADOPT:
    (void)lonejson__parser_adopt_leaf(parser, field, ptr);
    break;
  }
  if (operation == LONEJSON__RECORD_INIT ||
      operation == LONEJSON__RECORD_CLEANUP)
    lonejson__field_set_presence(value, field, 0);
}

static lonejson_status lonejson__record_walk(
    lonejson__record_workspace *workspace, const lonejson_map *map, void *value,
    int operation, lonejson_parser *parser, const lonejson_allocator *allocator,
    const lonejson_runtime *runtime, int measure) {
  size_t count = 1u;
  lonejson__record_frame root;
  memset(&root, 0, sizeof(root));
  root.map = map;
  root.value = value;
  root.operation = operation;
  workspace->frames[0] = root;
  while (count != 0u) {
    lonejson__record_frame *frame = &workspace->frames[count - 1u];
    const lonejson_field *field;
    void *ptr;
    void *child_value = NULL;
    int child_operation = frame->operation;
    int zero_on_exit = 0;
    if (frame->index == frame->map->field_count) {
      if (!measure && frame->zero_on_exit)
        memset(frame->value, 0, frame->map->struct_size);
      --count;
      continue;
    }
    field = &frame->map->fields[frame->index];
    ptr = lonejson__field_ptr(frame->value, field);
    if (field->kind == LONEJSON_FIELD_KIND_OBJECT && field->submap != NULL) {
      child_value = ptr;
      zero_on_exit = frame->operation == LONEJSON__RECORD_CLEANUP;
      ++frame->index;
    } else if (field->kind == LONEJSON_FIELD_KIND_OBJECT_ARRAY &&
               field->submap != NULL &&
               frame->operation != LONEJSON__RECORD_INIT) {
      lonejson_object_array *array = (lonejson_object_array *)ptr;
      if (frame->operation == LONEJSON__RECORD_RESET &&
          lonejson__is_exact_fixed_capacity(array->flags) &&
          !lonejson__map_may_allocate(field->submap))
        frame->array_index = array->count;
      if ((array->count != 0u && array->items == NULL) ||
          (array->count != 0u && field->elem_size > SIZE_MAX / array->count))
        return LONEJSON_STATUS_INVALID_ARGUMENT;
      if (frame->array_index < array->count) {
        child_value = (unsigned char *)array->items +
                      frame->array_index * field->elem_size;
        ++frame->array_index;
        if (frame->operation == LONEJSON__RECORD_RESET)
          child_operation = LONEJSON__RECORD_CLEANUP;
      }
    }
    if (child_value != NULL) {
      size_t i;
      if (measure) {
        for (i = 0u; i < count; ++i) {
          if (workspace->frames[i].map == field->submap &&
              workspace->frames[i].value == child_value)
            return LONEJSON_STATUS_INVALID_ARGUMENT;
        }
        if (count == workspace->capacity &&
            (workspace->capacity > SIZE_MAX / 2u ||
             !lonejson__record_reserve(workspace, workspace->capacity * 2u,
                                       allocator)))
          return LONEJSON_STATUS_ALLOCATION_FAILED;
      }
      /* A measured record cannot grow between the two passes: callers own it.
       */
      if (count == workspace->capacity)
        return LONEJSON_STATUS_ALLOCATION_FAILED;
      root.map = field->submap;
      root.value = child_value;
      root.index = root.array_index = 0u;
      root.operation = child_operation;
      root.zero_on_exit = zero_on_exit;
      workspace->frames[count++] = root;
      continue;
    }
    if (!measure) {
      lonejson__record_apply_field(frame->operation, parser, field,
                                   frame->value, allocator, runtime);
      if (parser != NULL && parser->failed)
        return parser->error.code;
    }
    ++frame->index;
    frame->array_index = 0u;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__record_run(const lonejson_map *map,
                                            void *value, int operation,
                                            lonejson_parser *parser,
                                            const lonejson_allocator *allocator,
                                            const lonejson_runtime *runtime) {
  lonejson__record_workspace workspace;
  lonejson__map_analysis analysis;
  lonejson_status status;
  size_t depth;
  if (map == NULL || value == NULL)
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  analysis = lonejson__map_analysis_cached_with_allocator(map, allocator);
  if (!analysis.valid)
    return analysis.status;
  workspace.frames = workspace.local;
  workspace.capacity = sizeof(workspace.local) / sizeof(workspace.local[0]);
  depth = operation == LONEJSON__RECORD_INIT ? analysis.inline_depth
                                             : analysis.depth;
  status = lonejson__record_reserve(&workspace, depth, allocator)
               ? LONEJSON_STATUS_OK
               : LONEJSON_STATUS_ALLOCATION_FAILED;
  if (status == LONEJSON_STATUS_OK && analysis.recursive &&
      operation != LONEJSON__RECORD_INIT)
    status = lonejson__record_walk(&workspace, map, value, operation, parser,
                                   allocator, runtime, 1);
  if (status == LONEJSON_STATUS_OK)
    status = lonejson__record_walk(&workspace, map, value, operation, parser,
                                   allocator, runtime, 0);
  if (workspace.frames != workspace.local)
    lonejson__owned_free(workspace.frames);
  return status;
}

static void lonejson__record_parse_status(lonejson_parser *parser,
                                          lonejson_status status) {
  if (status != LONEJSON_STATUS_OK && !parser->failed) {
    lonejson__set_error(&parser->error, status, parser->error.offset,
                        parser->error.line, parser->error.column,
                        status == LONEJSON_STATUS_ALLOCATION_FAILED
                            ? "failed to allocate record traversal workspace"
                            : "invalid record layout or cyclic record storage");
    parser->failed = 1;
  }
}

static lonejson_status lonejson__cleanup_map_checked(const lonejson_map *map,
                                                     void *value) {
  return lonejson__record_run(map, value, LONEJSON__RECORD_CLEANUP, NULL, NULL,
                              NULL);
}
static int lonejson__init_map_parse(lonejson_parser *parser,
                                    const lonejson_map *map, void *value) {
  lonejson__record_parse_status(
      parser, lonejson__record_run(map, value, LONEJSON__RECORD_INIT, parser,
                                   &parser->allocator, parser->runtime));
  return !parser->failed;
}
static void lonejson__reset_map_parse(lonejson_parser *parser,
                                      const lonejson_map *map, void *value) {
  lonejson__record_parse_status(
      parser, lonejson__record_run(map, value, LONEJSON__RECORD_RESET, parser,
                                   &parser->allocator, parser->runtime));
}
static void lonejson__parser_adopt_existing_map_allocations_with_mask(
    lonejson_parser *parser, const lonejson_map *map, void *value,
    lonejson_uint64 mask) {
  if (parser == NULL || parser->failed ||
      parser->options.max_alloc_bytes == 0u || mask == 0u)
    return;
  lonejson__record_parse_status(
      parser, lonejson__record_run(map, value, LONEJSON__RECORD_ADOPT, parser,
                                   &parser->allocator, parser->runtime));
}

/* Direct field cleanup (parser field replacement) shares exactly the same
 * traversal. A synthetic one-field map adjusts the record-relative offset. */
static lonejson_status lonejson__cleanup_field_run(lonejson_parser *parser,
                                                   const lonejson_field *field,
                                                   void *ptr) {
  lonejson_map map;
  lonejson_field adjusted = *field;
  memset(&map, 0, sizeof(map));
  adjusted.struct_offset = 0u;
  adjusted.flags &= ~(LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL);
  map.struct_size = lonejson__field_storage_size(field);
  map.fields = &adjusted;
  map.field_count = 1u;
  return lonejson__record_run(&map, ptr, LONEJSON__RECORD_CLEANUP, parser,
                              parser != NULL ? &parser->allocator : NULL,
                              parser != NULL ? parser->runtime : NULL);
}
static void lonejson__cleanup_value_parse(lonejson_parser *parser,
                                          const lonejson_field *field,
                                          void *ptr) {
  lonejson__record_parse_status(
      parser, lonejson__cleanup_field_run(parser, field, ptr));
}
static int lonejson__map_is_flat_zeroable(const lonejson_map *map) {
  size_t i;

  if (map == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];

    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
    case LONEJSON_FIELD_KIND_U64:
    case LONEJSON_FIELD_KIND_F64:
    case LONEJSON_FIELD_KIND_BOOL:
      break;
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage != LONEJSON_STORAGE_FIXED) {
        return 0;
      }
      break;
    default:
      return 0;
    }
  }
  return 1;
}

static void lonejson__cleanup_map_parse(lonejson_parser *parser,
                                        const lonejson_map *map, void *value) {
  lonejson__record_parse_status(
      parser, lonejson__record_run(map, value, LONEJSON__RECORD_CLEANUP, parser,
                                   &parser->allocator, parser->runtime));
}
