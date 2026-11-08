typedef struct lonejson__array_rewrite_segment {
  const char *key;
  size_t key_len;
  int fanout;
} lonejson__array_rewrite_segment;

typedef enum lonejson__array_rewrite_frame_kind {
  LONEJSON__ARRAY_REWRITE_FRAME_OBJECT = 1,
  LONEJSON__ARRAY_REWRITE_FRAME_ARRAY = 2
} lonejson__array_rewrite_frame_kind;

typedef struct lonejson__array_rewrite_frame {
  lonejson__array_rewrite_frame_kind kind;
  size_t count;
  size_t path_index;
  int selected_array;
  int fanout_array;
  char *key;
  size_t key_len;
  size_t key_cap;
  size_t parent_runtime;
} lonejson__array_rewrite_frame;

typedef struct lonejson__array_rewrite_parent_runtime {
  lonejson_parser *parser;
  size_t parser_alloc_size;
  size_t parent_index;
  int active;
} lonejson__array_rewrite_parent_runtime;

typedef struct lonejson__array_rewrite_state {
  const char *selector;
  lonejson_sink_fn sink;
  void *sink_user;
  lonejson_error *error;
  const lonejson_runtime *runtime;
  lonejson_array_rewrite_options options;
  lonejson__parse_options parse_options;
  lonejson_allocator allocator;
  lonejson__array_rewrite_segment segments[16];
  size_t segment_count;
  lonejson__array_rewrite_frame frames[64];
  size_t frame_count;
  lonejson__byte_buffer capture;
  int capturing;
  size_t capture_depth;
  int string_open;
  int number_open;
  int item_dst_initialized;
  int truncated;
  size_t selected_array_count;
  size_t rewritten_array_count;
  lonejson__array_stream_dup_state dup_state;
  lonejson__array_rewrite_parent_runtime parent_runtime[16];
} lonejson__array_rewrite_state;

static void
lonejson__array_rewrite_note_truncation(lonejson__array_rewrite_state *state,
                                        lonejson_status status) {
  if (status == LONEJSON_STATUS_TRUNCATED) {
    state->truncated = 1;
    if (state->error != NULL) {
      state->error->truncated = 1;
      if (state->error->code == LONEJSON_STATUS_OK) {
        state->error->code = LONEJSON_STATUS_TRUNCATED;
      }
      if (state->error->message[0] == '\0') {
        snprintf(state->error->message, sizeof(state->error->message),
                 "array rewrite truncated");
      }
    }
  }
}

static lonejson_status
lonejson__array_rewrite_feed_parents(lonejson__array_rewrite_state *state,
                                     const void *data, size_t len) {
  size_t i;

  for (i = 0u;
       i < sizeof(state->parent_runtime) / sizeof(state->parent_runtime[0]);
       ++i) {
    if (state->parent_runtime[i].active) {
      size_t consumed = 0u;
      lonejson_status status = lonejson__parser_feed_bytes(
          state->parent_runtime[i].parser, (const unsigned char *)data, len,
          &consumed, 0);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        if (state->error != NULL &&
            state->parent_runtime[i].parser->error.code != LONEJSON_STATUS_OK) {
          *state->error = state->parent_runtime[i].parser->error;
        }
        return status;
      }
      lonejson__array_rewrite_note_truncation(state, status);
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_emit(lonejson__array_rewrite_state *state,
                             const void *data, size_t len) {
  lonejson_status status;

  status = lonejson__array_rewrite_feed_parents(state, data, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  lonejson__array_rewrite_note_truncation(state, status);
  if (state->capturing) {
    status = lonejson__byte_append(&state->capture, data, len, SIZE_MAX - 1u,
                                   &state->allocator, state->error);
    lonejson__array_rewrite_note_truncation(state, status);
    return status;
  }
  status = lonejson__emit(state->sink, state->sink_user, state->error,
                          (const char *)data, len);
  lonejson__array_rewrite_note_truncation(state, status);
  return status;
}

static lonejson_status
lonejson__array_rewrite_emit_sink(void *user, const void *data, size_t len,
                                  lonejson_error *error) {
  (void)error;
  return lonejson__array_rewrite_emit((lonejson__array_rewrite_state *)user,
                                      data, len);
}

static size_t lonejson__array_rewrite_no_parent_runtime(void) {
  return (size_t)-1;
}

static int lonejson__array_rewrite_segment_matches_parent(
    const lonejson__array_rewrite_segment *segment,
    const lonejson_array_rewrite_parent *parent) {
  return parent->segment != NULL &&
         strlen(parent->segment) == segment->key_len &&
         memcmp(parent->segment, segment->key, segment->key_len) == 0;
}

static lonejson_status lonejson__array_rewrite_begin_parent(
    lonejson__array_rewrite_state *state,
    const lonejson__array_rewrite_segment *segment, size_t segment_index,
    size_t *runtime_index) {
  size_t occurrence;
  size_t parent_index;
  size_t slot;
  size_t parser_bytes;
  unsigned char *workspace;

  *runtime_index = lonejson__array_rewrite_no_parent_runtime();
  if (segment == NULL || state->options.parent_count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  occurrence = 0u;
  for (slot = 0u; slot < segment_index; ++slot) {
    if (state->segments[slot].fanout &&
        state->segments[slot].key_len == segment->key_len &&
        memcmp(state->segments[slot].key, segment->key, segment->key_len) ==
            0) {
      occurrence++;
    }
  }
  parent_index = state->options.parent_count;
  for (slot = 0u; slot < state->options.parent_count; ++slot) {
    if (lonejson__array_rewrite_segment_matches_parent(
            segment, &state->options.parents[slot])) {
      if (occurrence != 0u) {
        occurrence--;
        continue;
      }
      parent_index = slot;
      break;
    }
  }
  if (parent_index == state->options.parent_count) {
    return LONEJSON_STATUS_OK;
  }
  if (state->options.parents[parent_index].map == NULL ||
      state->options.parents[parent_index].dst == NULL) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u,
                               "array rewrite parent map and destination are "
                               "required");
  }
  for (slot = 0u;
       slot < sizeof(state->parent_runtime) / sizeof(state->parent_runtime[0]);
       ++slot) {
    if (!state->parent_runtime[slot].active) {
      break;
    }
  }
  if (slot ==
      sizeof(state->parent_runtime) / sizeof(state->parent_runtime[0])) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                               0u,
                               "too many active array rewrite parent contexts");
  }
  parser_bytes = sizeof(lonejson_parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                 LONEJSON__PARSER_WORKSPACE_SLACK;
  state->parent_runtime[slot].parser =
      (lonejson_parser *)lonejson__buffer_alloc(&state->allocator,
                                                parser_bytes);
  if (state->parent_runtime[slot].parser == NULL) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                               0u, 0u, 0u,
                               "failed to allocate parent rewrite parser");
  }
  workspace = ((unsigned char *)state->parent_runtime[slot].parser) +
              sizeof(lonejson_parser);
  lonejson__parser_init_state(
      state->parent_runtime[slot].parser,
      state->options.parents[parent_index].map,
      state->options.parents[parent_index].dst, &state->parse_options,
      state->runtime, 0, 0, 0, 0u, workspace,
      LONEJSON_PUSH_PARSER_BUFFER_SIZE + LONEJSON__PARSER_WORKSPACE_SLACK);
  if (!lonejson__init_map_parse(state->parent_runtime[slot].parser,
                                state->options.parents[parent_index].map,
                                state->options.parents[parent_index].dst)) {
    lonejson_status status = state->parent_runtime[slot].parser->error.code;
    if (state->error != NULL)
      *state->error = state->parent_runtime[slot].parser->error;
    lonejson_parser_destroy(state->parent_runtime[slot].parser);
    lonejson__buffer_free(&state->allocator, state->parent_runtime[slot].parser,
                          parser_bytes);
    state->parent_runtime[slot].parser = NULL;
    return status;
  }
  state->parent_runtime[slot].parser_alloc_size = parser_bytes;
  state->parent_runtime[slot].parent_index = parent_index;
  state->parent_runtime[slot].active = 1;
  *runtime_index = slot;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_end_parent(lonejson__array_rewrite_state *state,
                                   size_t runtime_index) {
  lonejson_status status;

  if (runtime_index == lonejson__array_rewrite_no_parent_runtime()) {
    return LONEJSON_STATUS_OK;
  }
  if (runtime_index >=
          sizeof(state->parent_runtime) / sizeof(state->parent_runtime[0]) ||
      !state->parent_runtime[runtime_index].active) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u, "array rewrite parent runtime mismatch");
  }
  status = lonejson_parser_finish(state->parent_runtime[runtime_index].parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (state->error != NULL) {
      *state->error = state->parent_runtime[runtime_index].parser->error;
    }
  }
  lonejson_cleanup(
      state->options.parents[state->parent_runtime[runtime_index].parent_index]
          .map,
      state->options.parents[state->parent_runtime[runtime_index].parent_index]
          .dst,
      NULL);
  lonejson__buffer_free(&state->allocator,
                        state->parent_runtime[runtime_index].parser,
                        state->parent_runtime[runtime_index].parser_alloc_size);
  memset(&state->parent_runtime[runtime_index], 0,
         sizeof(state->parent_runtime[runtime_index]));
  return status;
}

static lonejson_status
lonejson__array_rewrite_emit_cstr(lonejson__array_rewrite_state *state,
                                  const char *text) {
  return lonejson__array_rewrite_emit(state, text, strlen(text));
}

static lonejson_status
lonejson__array_rewrite_emit_escaped(lonejson__array_rewrite_state *state,
                                     const char *data, size_t len) {
  return lonejson__emit_escaped_fragment(lonejson__array_rewrite_emit_sink,
                                         state, state->error,
                                         (const unsigned char *)data, len);
}

static int lonejson__array_rewrite_source_is_valid(
    const lonejson_array_rewrite_source *source) {
  if (source == NULL) {
    return 0;
  }
  if (source->json != NULL) {
    return source->map == NULL && source->src == NULL &&
           lonejson_json_value_is_rewindable(source->json);
  }
  return source->map != NULL && source->src != NULL;
}

static lonejson_status
lonejson__array_rewrite_emit_source(void *emit_user,
                                    const lonejson_array_rewrite_source *source,
                                    lonejson_error *error) {
  lonejson__array_rewrite_state *state =
      (lonejson__array_rewrite_state *)emit_user;
  lonejson_status status;

  if (!lonejson__array_rewrite_source_is_valid(source)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "rewrite item source is invalid");
  }
  if (state->rewritten_array_count != 0u) {
    status = lonejson__emit_cstr(state->sink, state->sink_user, error, ",");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    lonejson__array_rewrite_note_truncation(state, status);
  }
  if (source->json != NULL) {
    status = lonejson_json_value_write_to_sink(source->json, state->sink,
                                               state->sink_user, error);
  } else {
    status = lonejson__serialize_map_compact(
        source->map, source->src, state->sink, state->sink_user, error);
  }
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    lonejson__array_rewrite_note_truncation(state, status);
    state->rewritten_array_count++;
  }
  return status;
}

static lonejson_status lonejson__array_rewrite_emit_original_item(
    lonejson__array_rewrite_state *state) {
  lonejson_array_rewrite_source source;
  lonejson_json_value value;
  lonejson_status status;

  lonejson_json_value_init_with_allocator(&value, &state->allocator);
  status = lonejson_json_value_set_buffer(&value, state->capture.data,
                                          state->capture.len, state->error);
  lonejson__array_rewrite_note_truncation(state, status);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    memset(&source, 0, sizeof(source));
    source.json = &value;
    status = lonejson__array_rewrite_emit_source(state, &source, state->error);
  }
  lonejson_json_value_cleanup(&value);
  return status;
}

static lonejson_status
lonejson__array_rewrite_finish_item(lonejson__array_rewrite_state *state) {
  lonejson_array_rewrite_context context;
  lonejson_array_rewrite_result result;
  lonejson_status status;
  void *item_arg;
  int cleanup_item_dst;

  memset(&context, 0, sizeof(context));
  context.selector = state->selector;
  context.parents = state->options.parents;
  context.parent_count = state->options.parent_count;
  memset(&result, 0, sizeof(result));
  result.action = LONEJSON_ARRAY_REWRITE_KEEP;
  item_arg = NULL;
  cleanup_item_dst = 0;
  if (state->options.item_map != NULL) {
    status = lonejson__parse_buffer_with_options(
        state->options.item_map, state->options.item_dst, state->capture.data,
        state->capture.len, &state->parse_options, state->runtime,
        state->error);
    state->item_dst_initialized = 1;
    cleanup_item_dst = 1;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      lonejson_cleanup(state->options.item_map, state->options.item_dst, NULL);
      state->item_dst_initialized = 0;
      return status;
    }
    lonejson__array_rewrite_note_truncation(state, status);
    item_arg = state->options.item_dst;
  } else if (state->options.item_value != NULL) {
    lonejson_json_value_reset(state->options.item_value);
    status = lonejson_json_value_set_buffer(state->options.item_value,
                                            state->capture.data,
                                            state->capture.len, state->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    lonejson__array_rewrite_note_truncation(state, status);
    item_arg = state->options.item_value;
  }
  if (state->options.item != NULL) {
    status = state->options.item(state->options.user, &context, item_arg,
                                 &result, state->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      if (cleanup_item_dst) {
        lonejson_cleanup(state->options.item_map, state->options.item_dst,
                         NULL);
        state->item_dst_initialized = 0;
      }
      if (state->error != NULL && state->error->code == LONEJSON_STATUS_OK) {
        return lonejson__set_error(state->error, status, 0u, 0u, 0u,
                                   "array rewrite item callback failed");
      }
      return status;
    }
    lonejson__array_rewrite_note_truncation(state, status);
  }
  switch (result.action) {
  case LONEJSON_ARRAY_REWRITE_KEEP:
    status = lonejson__array_rewrite_emit_original_item(state);
    break;
  case LONEJSON_ARRAY_REWRITE_DROP:
    status = LONEJSON_STATUS_OK;
    break;
  case LONEJSON_ARRAY_REWRITE_REPLACE:
    status = lonejson__array_rewrite_emit_source(state, &result.replacement,
                                                 state->error);
    break;
  case LONEJSON_ARRAY_REWRITE_INSERT_BEFORE:
    status = lonejson__array_rewrite_emit_source(state, &result.insert,
                                                 state->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      break;
    }
    status = lonejson__array_rewrite_emit_original_item(state);
    break;
  case LONEJSON_ARRAY_REWRITE_INSERT_AFTER:
    status = lonejson__array_rewrite_emit_original_item(state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      break;
    }
    status = lonejson__array_rewrite_emit_source(state, &result.insert,
                                                 state->error);
    break;
  case LONEJSON_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER:
    status = lonejson__array_rewrite_emit_source(state, &result.replacement,
                                                 state->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      break;
    }
    status = lonejson__array_rewrite_emit_source(state, &result.insert,
                                                 state->error);
    break;
  default:
    status = lonejson__set_error(state->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                                 0u, 0u, 0u, "invalid array rewrite action");
    break;
  }
  if (cleanup_item_dst) {
    lonejson_cleanup(state->options.item_map, state->options.item_dst, NULL);
    state->item_dst_initialized = 0;
  }
  if (status == LONEJSON_STATUS_OK && state->truncated) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

static int lonejson__array_rewrite_key_matches(
    const lonejson__array_rewrite_frame *frame,
    const lonejson__array_rewrite_segment *segment) {
  return frame->key_len == segment->key_len &&
         (frame->key_len == 0u ||
          memcmp(frame->key, segment->key, frame->key_len) == 0);
}

static lonejson__array_rewrite_frame *
lonejson__array_rewrite_top(lonejson__array_rewrite_state *state) {
  return state->frame_count == 0u ? NULL
                                  : &state->frames[state->frame_count - 1u];
}

static lonejson_status
lonejson__array_rewrite_pop_frame(lonejson__array_rewrite_state *state) {
  lonejson__array_rewrite_frame *frame;

  if (state->frame_count == 0u) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u, "array rewrite frame underflow");
  }
  frame = &state->frames[state->frame_count - 1u];
  lonejson__buffer_free(&state->allocator, frame->key, frame->key_cap);
  memset(frame, 0, sizeof(*frame));
  state->frame_count--;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_before_value(lonejson__array_rewrite_state *state,
                                     int *select_array, int *fanout_array,
                                     size_t *object_path_index) {
  lonejson__array_rewrite_frame *parent;
  lonejson_status status;

  *select_array = 0;
  *fanout_array = 0;
  *object_path_index = lonejson__array_rewrite_no_parent_runtime();
  parent = lonejson__array_rewrite_top(state);
  if (state->capturing && (parent == NULL || parent->selected_array)) {
    return LONEJSON_STATUS_OK;
  }
  if (parent == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (parent->selected_array) {
    if (parent->count != 0u) {
      status = lonejson__array_rewrite_feed_parents(state, ",", 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    parent->count++;
    state->capturing = 1;
    state->capture_depth = 0u;
    lonejson__byte_reset(&state->capture);
    return LONEJSON_STATUS_OK;
  }
  if (parent->kind == LONEJSON__ARRAY_REWRITE_FRAME_ARRAY) {
    if (parent->count != 0u) {
      status = lonejson__array_rewrite_emit_cstr(state, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    parent->count++;
    return LONEJSON_STATUS_OK;
  }
  if (parent->count != 0u) {
    status = lonejson__array_rewrite_emit_cstr(state, ",");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  parent->count++;
  status = lonejson__array_rewrite_emit_cstr(state, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status =
      lonejson__array_rewrite_emit_escaped(state, parent->key, parent->key_len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__array_rewrite_emit_cstr(state, "\":");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (!state->capturing && parent->path_index < state->segment_count &&
      lonejson__array_rewrite_key_matches(
          parent, &state->segments[parent->path_index])) {
    if (parent->path_index + 1u == state->segment_count &&
        !state->segments[parent->path_index].fanout) {
      *select_array = 1;
    } else if (state->segments[parent->path_index].fanout) {
      *fanout_array = 1;
    } else {
      *object_path_index = parent->path_index + 1u;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_push_frame(lonejson__array_rewrite_state *state,
                                   lonejson__array_rewrite_frame_kind kind,
                                   int selected_array, int fanout_array) {
  lonejson__array_rewrite_frame *parent;
  lonejson__array_rewrite_frame *frame;

  if (state->frame_count >= sizeof(state->frames) / sizeof(state->frames[0])) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                               0u, "array rewrite nesting exceeds limit");
  }
  parent = lonejson__array_rewrite_top(state);
  frame = &state->frames[state->frame_count++];
  memset(frame, 0, sizeof(*frame));
  frame->kind = kind;
  frame->selected_array = selected_array;
  frame->fanout_array = fanout_array;
  frame->parent_runtime = lonejson__array_rewrite_no_parent_runtime();
  frame->path_index = parent != NULL ? parent->path_index : 0u;
  if (parent != NULL && parent->fanout_array &&
      kind == LONEJSON__ARRAY_REWRITE_FRAME_OBJECT) {
    frame->path_index = parent->path_index + 1u;
  }
  if (fanout_array) {
    frame->path_index = parent != NULL ? parent->path_index : 0u;
  }
  if (selected_array) {
    state->selected_array_count++;
    state->rewritten_array_count = 0u;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_append_selected(lonejson__array_rewrite_state *state) {
  lonejson_array_rewrite_context context;

  if (state->options.append == NULL) {
    return LONEJSON_STATUS_OK;
  }
  memset(&context, 0, sizeof(context));
  context.selector = state->selector;
  context.parents = state->options.parents;
  context.parent_count = state->options.parent_count;
  {
    lonejson_status status = state->options.append(
        state->options.user, &context, lonejson__array_rewrite_emit_source,
        state, state->error);
    lonejson__array_rewrite_note_truncation(state, status);
    return status;
  }
}

static lonejson_status
lonejson__array_rewrite_object_begin(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  int select_array;
  int fanout_array;
  size_t object_path_index;
  lonejson_status status;
  lonejson__array_rewrite_frame *parent;
  size_t parent_runtime;

  (void)error;
  parent_runtime = lonejson__array_rewrite_no_parent_runtime();
  object_path_index = lonejson__array_rewrite_no_parent_runtime();
  parent = lonejson__array_rewrite_top(state);
  status = lonejson__array_rewrite_before_value(
      state, &select_array, &fanout_array, &object_path_index);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (select_array || fanout_array) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                               0u, 0u,
                               "selected array path matched a non-array value");
  }
  if (!state->capturing && parent != NULL && parent->fanout_array &&
      parent->path_index < state->segment_count) {
    status = lonejson__array_rewrite_begin_parent(
        state, &state->segments[parent->path_index], parent->path_index,
        &parent_runtime);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (state->parse_options.reject_duplicate_keys) {
    status =
        lonejson__array_stream_dup_push_object(&state->dup_state, state->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__array_rewrite_emit_cstr(state, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (state->capturing) {
    state->capture_depth++;
    status = lonejson__array_rewrite_push_frame(
        state, LONEJSON__ARRAY_REWRITE_FRAME_OBJECT, 0, 0);
    if (status == LONEJSON_STATUS_OK &&
        object_path_index != lonejson__array_rewrite_no_parent_runtime()) {
      state->frames[state->frame_count - 1u].path_index = object_path_index;
    }
    if (status == LONEJSON_STATUS_OK &&
        parent_runtime != lonejson__array_rewrite_no_parent_runtime()) {
      state->frames[state->frame_count - 1u].parent_runtime = parent_runtime;
    }
    return status;
  }
  status = lonejson__array_rewrite_push_frame(
      state, LONEJSON__ARRAY_REWRITE_FRAME_OBJECT, 0, 0);
  if (status == LONEJSON_STATUS_OK &&
      object_path_index != lonejson__array_rewrite_no_parent_runtime()) {
    state->frames[state->frame_count - 1u].path_index = object_path_index;
  }
  if (status == LONEJSON_STATUS_OK &&
      parent_runtime != lonejson__array_rewrite_no_parent_runtime()) {
    state->frames[state->frame_count - 1u].parent_runtime = parent_runtime;
  }
  return status;
}

static lonejson_status
lonejson__array_rewrite_object_end(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  lonejson_status status;
  size_t parent_runtime;

  (void)error;
  parent_runtime = lonejson__array_rewrite_no_parent_runtime();
  if (state->capturing && state->frame_count != 0u) {
    parent_runtime = state->frames[state->frame_count - 1u].parent_runtime;
  } else if (!state->capturing && state->frame_count != 0u) {
    parent_runtime = state->frames[state->frame_count - 1u].parent_runtime;
  }
  if (state->parse_options.reject_duplicate_keys) {
    status =
        lonejson__array_stream_dup_pop_object(&state->dup_state, state->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__array_rewrite_emit_cstr(state, "}");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (state->capturing) {
    state->capture_depth--;
    if (state->frame_count == 0u) {
      return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 0u, 0u, 0u,
                                 "array rewrite capture frame underflow");
    }
    status = lonejson__array_rewrite_pop_frame(state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__array_rewrite_end_parent(state, parent_runtime);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (state->capture_depth == 0u) {
      state->capturing = 0;
      return lonejson__array_rewrite_finish_item(state);
    }
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__array_rewrite_pop_frame(state);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__array_rewrite_end_parent(state, parent_runtime);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_array_begin(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  int select_array;
  int fanout_array;
  size_t object_path_index;
  lonejson_status status;

  (void)error;
  select_array = 0;
  fanout_array = 0;
  object_path_index = lonejson__array_rewrite_no_parent_runtime();
  if (state->frame_count == 0u && state->segment_count == 0u) {
    select_array = 1;
  } else {
    status = lonejson__array_rewrite_before_value(
        state, &select_array, &fanout_array, &object_path_index);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  status = lonejson__array_rewrite_emit_cstr(state, "[");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (state->capturing) {
    state->capture_depth++;
    return lonejson__array_rewrite_push_frame(
        state, LONEJSON__ARRAY_REWRITE_FRAME_ARRAY, 0, 0);
  }
  return lonejson__array_rewrite_push_frame(
      state, LONEJSON__ARRAY_REWRITE_FRAME_ARRAY, select_array, fanout_array);
}

static lonejson_status
lonejson__array_rewrite_array_end(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  lonejson__array_rewrite_frame *frame;
  lonejson_status status;
  int selected;

  (void)error;
  if (state->capturing) {
    status = lonejson__array_rewrite_emit_cstr(state, "]");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    state->capture_depth--;
    if (state->frame_count == 0u) {
      return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 0u, 0u, 0u,
                                 "array rewrite capture frame underflow");
    }
    status = lonejson__array_rewrite_pop_frame(state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (state->capture_depth == 0u) {
      state->capturing = 0;
      return lonejson__array_rewrite_finish_item(state);
    }
    return LONEJSON_STATUS_OK;
  }
  frame = lonejson__array_rewrite_top(state);
  selected = frame != NULL && frame->selected_array;
  if (selected) {
    status = lonejson__array_rewrite_append_selected(state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  status = lonejson__array_rewrite_emit_cstr(state, "]");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__array_rewrite_pop_frame(state);
}

static lonejson_status
lonejson__array_rewrite_key_begin(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  lonejson__array_rewrite_frame *frame = lonejson__array_rewrite_top(state);

  (void)error;
  if (frame != NULL) {
    frame->key_len = 0u;
  }
  if (state->parse_options.reject_duplicate_keys) {
    return lonejson__array_stream_dup_key_begin(&state->dup_state,
                                                state->error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_key_chunk(void *user, const char *data, size_t len,
                                  lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  lonejson__array_rewrite_frame *frame = lonejson__array_rewrite_top(state);
  char *next;
  size_t next_cap;
  lonejson_status status;

  if (state->parse_options.reject_duplicate_keys) {
    status = lonejson__array_stream_dup_key_chunk(&state->dup_state, data, len,
                                                  state->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  if (frame == NULL || len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (frame->key_len + len + 1u > frame->key_cap) {
    next_cap = frame->key_cap == 0u ? 32u : frame->key_cap;
    while (next_cap < frame->key_len + len + 1u) {
      next_cap *= 2u;
    }
    next = (char *)lonejson__buffer_realloc(&state->allocator, frame->key,
                                            frame->key_cap, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate array rewrite key");
    }
    frame->key = next;
    frame->key_cap = next_cap;
  }
  memcpy(frame->key + frame->key_len, data, len);
  frame->key_len += len;
  frame->key[frame->key_len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__array_rewrite_key_end(void *user,
                                                       lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  (void)error;
  if (state->parse_options.reject_duplicate_keys) {
    return lonejson__array_stream_dup_key_end(&state->dup_state, state->error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_string_begin(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  int select_array;
  int fanout_array;
  size_t object_path_index;
  lonejson_status status;

  (void)error;
  status = lonejson__array_rewrite_before_value(
      state, &select_array, &fanout_array, &object_path_index);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (select_array || fanout_array) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                               0u, 0u,
                               "selected array path matched a non-array value");
  }
  status = lonejson__array_rewrite_emit_cstr(state, "\"");
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    state->string_open = 1;
  }
  return status;
}

static lonejson_status
lonejson__array_rewrite_string_chunk(void *user, const char *data, size_t len,
                                     lonejson_error *error) {
  (void)error;
  return lonejson__array_rewrite_emit_escaped(
      (lonejson__array_rewrite_state *)user, data, len);
}

static lonejson_status
lonejson__array_rewrite_string_end(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__array_rewrite_emit_cstr(state, "\"");
  state->string_open = 0;
  if (state->capturing && state->capture_depth == 0u) {
    state->capturing = 0;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    return lonejson__array_rewrite_finish_item(state);
  }
  return status;
}

static lonejson_status
lonejson__array_rewrite_number_begin(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  int select_array;
  int fanout_array;
  size_t object_path_index;
  lonejson_status status;
  (void)error;
  state->number_open = 1;
  status = lonejson__array_rewrite_before_value(
      state, &select_array, &fanout_array, &object_path_index);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (select_array || fanout_array) {
    state->number_open = 0;
    return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                               0u, 0u,
                               "selected array path matched a non-array value");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_rewrite_number_chunk(void *user, const char *data, size_t len,
                                     lonejson_error *error) {
  (void)error;
  return lonejson__array_rewrite_emit((lonejson__array_rewrite_state *)user,
                                      data, len);
}

static lonejson_status
lonejson__array_rewrite_number_end(void *user, lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  (void)error;
  state->number_open = 0;
  if (state->capturing && state->capture_depth == 0u) {
    state->capturing = 0;
    return lonejson__array_rewrite_finish_item(state);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__array_rewrite_boolean(void *user, int value,
                                                       lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  int select_array;
  int fanout_array;
  size_t object_path_index;
  lonejson_status status;
  (void)error;
  status = lonejson__array_rewrite_before_value(
      state, &select_array, &fanout_array, &object_path_index);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (select_array || fanout_array) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                               0u, 0u,
                               "selected array path matched a non-array value");
  }
  status = lonejson__array_rewrite_emit_cstr(state, value ? "true" : "false");
  if (state->capturing && state->capture_depth == 0u) {
    state->capturing = 0;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    return lonejson__array_rewrite_finish_item(state);
  }
  return status;
}

static lonejson_status lonejson__array_rewrite_null(void *user,
                                                    lonejson_error *error) {
  lonejson__array_rewrite_state *state = (lonejson__array_rewrite_state *)user;
  int select_array;
  int fanout_array;
  size_t object_path_index;
  lonejson_status status;
  (void)error;
  status = lonejson__array_rewrite_before_value(
      state, &select_array, &fanout_array, &object_path_index);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (select_array || fanout_array) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                               0u, 0u,
                               "selected array path matched a non-array value");
  }
  status = lonejson__array_rewrite_emit_cstr(state, "null");
  if (state->capturing && state->capture_depth == 0u) {
    state->capturing = 0;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    return lonejson__array_rewrite_finish_item(state);
  }
  return status;
}

static void
lonejson__array_rewrite_cleanup(lonejson__array_rewrite_state *state) {
  size_t i;
  for (i = 0u; i < state->frame_count; ++i) {
    lonejson__buffer_free(&state->allocator, state->frames[i].key,
                          state->frames[i].key_cap);
  }
  for (i = 0u;
       i < sizeof(state->parent_runtime) / sizeof(state->parent_runtime[0]);
       ++i) {
    if (state->parent_runtime[i].parser != NULL) {
      lonejson_cleanup(
          state->options.parents[state->parent_runtime[i].parent_index].map,
          state->options.parents[state->parent_runtime[i].parent_index].dst,
          NULL);
      lonejson__buffer_free(&state->allocator, state->parent_runtime[i].parser,
                            state->parent_runtime[i].parser_alloc_size);
    }
  }
  if (state->item_dst_initialized) {
    lonejson_cleanup(state->options.item_map, state->options.item_dst, NULL);
  }
  lonejson__byte_free(&state->capture, &state->allocator);
  lonejson__array_stream_dup_state_cleanup(&state->dup_state);
}

static lonejson_status
lonejson__array_rewrite_parse_selector(lonejson__array_rewrite_state *state,
                                       const char *selector) {
  const char *p;
  const char *start;
  size_t len;

  if (selector == NULL || selector[0] == '\0') {
    state->segment_count = 0u;
    return LONEJSON_STATUS_OK;
  }
  p = selector;
  while (*p != '\0') {
    if (state->segment_count >=
        sizeof(state->segments) / sizeof(state->segments[0])) {
      return lonejson__set_error(state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                 0u, "array rewrite selector is too deep");
    }
    start = p;
    while (*p != '\0' && *p != '.') {
      p++;
    }
    len = (size_t)(p - start);
    if (len == 0u) {
      return lonejson__set_error(
          state->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
          "array rewrite selector contains empty segment");
    }
    state->segments[state->segment_count].fanout = 0;
    if (len >= 2u && start[len - 2u] == '[' && start[len - 1u] == ']') {
      state->segments[state->segment_count].fanout = 1;
      len -= 2u;
      if (len == 0u) {
        return lonejson__set_error(
            state->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
            "array rewrite selector contains empty fanout segment");
      }
    }
    state->segments[state->segment_count].key = start;
    state->segments[state->segment_count].key_len = len;
    state->segment_count++;
    if (*p == '.') {
      p++;
      if (*p == '\0') {
        return lonejson__set_error(
            state->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
            "array rewrite selector contains empty segment");
      }
    }
  }
  if (state->segment_count != 0u &&
      state->segments[state->segment_count - 1u].fanout) {
    return lonejson__set_error(
        state->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "array rewrite selector must end at an array field, not a fanout");
  }
  return LONEJSON_STATUS_OK;
}

static void
lonejson__array_rewrite_resolve_limits(const lonejson__parse_options *options,
                                       const lonejson_runtime *runtime,
                                       lonejson__value_limits *limits) {
  *limits = runtime != NULL ? runtime->value_limits
                            : lonejson__default_value_limits();
  if (options != NULL && options->max_depth != 0u) {
    limits->max_depth = options->max_depth;
  }
}

static lonejson_status lonejson__array_rewrite_reader_common(
    const char *selector, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const lonejson__parse_options *parse_options,
    const lonejson_runtime *runtime,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  struct lonejson__rewrite_context {
    lonejson__array_rewrite_state state;
    lonejson_json_value input;
  } *context;
  lonejson__array_rewrite_state *state;
  lonejson_json_value *input;
  lonejson_allocator allocator;
  lonejson_value_visitor visitor;
  lonejson__value_limits limits;
  lonejson_status status;

  if (reader == NULL || sink == NULL || options == NULL ||
      (options->item == NULL && options->append == NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "selector, reader, sink, options, and at least "
                               "one rewrite callback are required");
  }
  if ((options->item_map == NULL) != (options->item_dst == NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "item_map and item_dst must be supplied "
                               "together");
  }
  if ((options->parents == NULL) != (options->parent_count == 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "parents and parent_count must be supplied "
                               "together");
  }
  if (options->item != NULL) {
    if ((options->item_map != NULL && options->item_value != NULL) ||
        (options->item_map == NULL && options->item_value == NULL)) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                 0u, 0u,
                                 "configure exactly one rewrite item delivery "
                                 "mode");
    }
  } else if (options->item_map != NULL || options->item_dst != NULL ||
             options->item_value != NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "item delivery requires an item callback");
  }
  if (parse_options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(parse_options->allocator)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "allocator must provide either all callbacks "
                               "or none");
  }
  allocator = lonejson__allocator_resolve(
      parse_options != NULL ? parse_options->allocator : NULL);
  context = (struct lonejson__rewrite_context *)lonejson__owned_malloc(
      &allocator, sizeof(*context));
  if (context == NULL)
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate array rewrite context");
  state = &context->state;
  input = &context->input;
  memset(state, 0, sizeof(*state));
  state->selector = selector != NULL ? selector : "";
  state->sink = sink;
  state->sink_user = sink_user;
  state->error = error;
  state->runtime = runtime;
  state->options = *options;
  state->parse_options = parse_options != NULL
                             ? *parse_options
                             : lonejson__default_parse_options();
  state->allocator =
      lonejson__allocator_resolve(state->parse_options.allocator);
  state->dup_state.allocator = &state->allocator;
  lonejson_json_value_init_with_allocator(input, &state->allocator);
  lonejson__clear_error(error);
  status = lonejson__array_rewrite_parse_selector(state, state->selector);
  if (status == LONEJSON_STATUS_OK) {
    visitor = lonejson_default_value_visitor();
    visitor.object_begin = lonejson__array_rewrite_object_begin;
    visitor.object_end = lonejson__array_rewrite_object_end;
    visitor.object_key_begin = lonejson__array_rewrite_key_begin;
    visitor.object_key_chunk = lonejson__array_rewrite_key_chunk;
    visitor.object_key_end = lonejson__array_rewrite_key_end;
    visitor.array_begin = lonejson__array_rewrite_array_begin;
    visitor.array_end = lonejson__array_rewrite_array_end;
    visitor.string_begin = lonejson__array_rewrite_string_begin;
    visitor.string_chunk = lonejson__array_rewrite_string_chunk;
    visitor.string_end = lonejson__array_rewrite_string_end;
    visitor.number_begin = lonejson__array_rewrite_number_begin;
    visitor.number_chunk = lonejson__array_rewrite_number_chunk;
    visitor.number_end = lonejson__array_rewrite_number_end;
    visitor.boolean_value = lonejson__array_rewrite_boolean;
    visitor.null_value = lonejson__array_rewrite_null;
    lonejson__array_rewrite_resolve_limits(&state->parse_options, runtime,
                                           &limits);
    status = lonejson_json_value_set_reader(input, reader, reader_user, error);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      status = lonejson__json_visit(input, &visitor, state, &limits, error);
    }
  }
  if ((status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
      state->selected_array_count == 0u) {
    status = lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u, "selected array was not found");
  }
  if (status == LONEJSON_STATUS_OK && state->truncated) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  if (status == LONEJSON_STATUS_TRUNCATED && error != NULL) {
    error->truncated = 1;
    if (error->code == LONEJSON_STATUS_OK) {
      error->code = LONEJSON_STATUS_TRUNCATED;
    }
    if (error->message[0] == '\0') {
      snprintf(error->message, sizeof(error->message),
               "array rewrite truncated");
    }
  }
  lonejson_json_value_cleanup(input);
  lonejson__array_rewrite_cleanup(state);
  lonejson__owned_free(context);
  return status;
}

static lonejson_status lonejson__array_rewrite_reader_with_options(
    const char *selector, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const lonejson__parse_options *parse_options,
    const lonejson_runtime *runtime,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  return lonejson__array_rewrite_reader_common(selector, reader, reader_user,
                                               sink, sink_user, parse_options,
                                               runtime, options, error);
}

lonejson_status lonejson_array_rewrite_reader(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, lonejson_sink_fn sink, void *sink_user,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__array_rewrite_reader_with_options(
      selector, reader, reader_user, sink, sink_user,
      &runtime_state->parse_options, runtime_state, options, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status lonejson__array_rewrite_fd_sink(void *user,
                                                       const void *data,
                                                       size_t len,
                                                       lonejson_error *error) {
  int fd = *(int *)user;
  const unsigned char *p = (const unsigned char *)data;
  while (len != 0u) {
    ssize_t written = write(fd, p, len);
    if (written < 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to write rewrite output fd");
    }
    if (written == 0) {
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "rewrite output fd wrote zero bytes");
    }
    p += (size_t)written;
    len -= (size_t)written;
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_array_rewrite_reader_to_filep(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, FILE *output,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (output == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output file is required");
  }
  return lonejson_array_rewrite_reader(runtime, selector, reader, reader_user,
                                       lonejson__sink_file, output, options,
                                       error);
}

lonejson_status lonejson_array_rewrite_reader_to_fd(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, int fd, const lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output fd must be non-negative");
  }
  return lonejson_array_rewrite_reader(runtime, selector, reader, reader_user,
                                       lonejson__array_rewrite_fd_sink, &fd,
                                       options, error);
}

lonejson_status
lonejson_array_rewrite_filep(lonejson *runtime, const char *selector, FILE *fp,
                             lonejson_sink_fn sink, void *sink_user,
                             const lonejson_array_rewrite_options *options,
                             lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input file is required");
  }
  return lonejson_array_rewrite_reader(runtime, selector, lonejson__file_reader,
                                       fp, sink, sink_user, options, error);
}

lonejson_status lonejson_array_rewrite_filep_to_filep(
    lonejson *runtime, const char *selector, FILE *input, FILE *output,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (output == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output file is required");
  }
  return lonejson_array_rewrite_filep(
      runtime, selector, input, lonejson__sink_file, output, options, error);
}

lonejson_status
lonejson_array_rewrite_fd(lonejson *runtime, const char *selector, int fd,
                          lonejson_sink_fn sink, void *sink_user,
                          const lonejson_array_rewrite_options *options,
                          lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input fd must be non-negative");
  }
  return lonejson_array_rewrite_reader(runtime, selector, lonejson__fd_reader,
                                       &fd, sink, sink_user, options, error);
}

lonejson_status lonejson_array_rewrite_fd_to_fd(
    lonejson *runtime, const char *selector, int input_fd, int output_fd,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (output_fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output fd must be non-negative");
  }
  return lonejson_array_rewrite_fd(runtime, selector, input_fd,
                                   lonejson__array_rewrite_fd_sink, &output_fd,
                                   options, error);
}

lonejson_status
lonejson_array_rewrite_path(lonejson *runtime, const char *selector,
                            const char *input_path, const char *output_path,
                            const lonejson_array_rewrite_options *options,
                            lonejson_error *error) {
  struct stat input_stat;
  struct stat output_stat;
  FILE *in;
  FILE *out;
  lonejson_status status;

  if (input_path == NULL || output_path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input and output paths are required");
  }
  {
    lonejson__runtime_borrow borrow;
    if (lonejson__require_runtime_borrow(runtime, &borrow, error) == NULL) {
      return LONEJSON_STATUS_INVALID_ARGUMENT;
    }
    lonejson__runtime_borrow_release(&borrow);
  }
  if (stat(input_path, &input_stat) == 0 &&
      stat(output_path, &output_stat) == 0 &&
      input_stat.st_dev == output_stat.st_dev &&
      input_stat.st_ino == output_stat.st_ino) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "input and output paths must refer to distinct "
                               "files");
  }
  in = fopen(input_path, "rb");
  if (in == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open rewrite input path");
  }
  out = fopen(output_path, "wb");
  if (out == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    fclose(in);
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open rewrite output path");
  }
  status = lonejson_array_rewrite_filep(
      runtime, selector, in, lonejson__sink_file, out, options, error);
  if (fclose(out) != 0 && status == LONEJSON_STATUS_OK) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    status = lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to close rewrite output path");
  }
  fclose(in);
  return status;
}
