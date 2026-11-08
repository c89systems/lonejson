#ifndef LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL

static lonejson_status lonejson__sse_push_json_method(
    lonejson_sse *sse, lonejson *runtime, const lonejson_map *map, void *dst,
    const void *bytes, size_t len, const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  return lonejson_sse_push_json(runtime, sse, map, dst, bytes, len, options,
                                event_cb, user, error);
}

static lonejson_status lonejson__sse_finish_json_method(
    lonejson_sse *sse, lonejson *runtime, const lonejson_map *map, void *dst,
    const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  return lonejson_sse_finish_json(runtime, sse, map, dst, options, event_cb,
                                  user, error);
}

static lonejson_status lonejson__sse_push_json_value_method(
    lonejson_sse *sse, lonejson *runtime, lonejson_json_value *value,
    const void *bytes, size_t len, const lonejson_sse_json_options *options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error) {
  return lonejson_sse_push_json_value(runtime, sse, value, bytes, len, options,
                                      event_cb, user, error);
}

static lonejson_status lonejson__sse_finish_json_value_method(
    lonejson_sse *sse, lonejson *runtime, lonejson_json_value *value,
    const lonejson_sse_json_options *options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error) {
  return lonejson_sse_finish_json_value(runtime, sse, value, options, event_cb,
                                        user, error);
}

lonejson_sse_options lonejson_default_sse_options(void) {
  lonejson_sse_options options;

  memset(&options, 0, sizeof(options));
  options.max_line_bytes = 64u * 1024u;
  options.max_event_data_bytes = 1024u * 1024u;
  options.max_buffered_bytes = 1024u * 1024u;
  options.allocator = NULL;
  return options;
}

lonejson_sse *lonejson_sse_open(const lonejson_sse_options *options,
                                lonejson_error *error) {
  lonejson__sse_state *sse;
  lonejson_sse_options resolved;

  resolved = options ? *options : lonejson_default_sse_options();
  if (resolved.max_line_bytes == 0u) {
    resolved.max_line_bytes = 64u * 1024u;
  }
  if (resolved.max_event_data_bytes == 0u) {
    resolved.max_event_data_bytes = 1024u * 1024u;
  }
  if (resolved.max_buffered_bytes == 0u) {
    resolved.max_buffered_bytes = 1024u * 1024u;
  }
  if (!LONEJSON__ALLOCATOR_IS_VALID_CONFIG(resolved.allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  sse = (lonejson__sse_state *)lonejson__buffer_alloc(resolved.allocator,
                                                      sizeof(*sse));
  if (sse == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate SSE parser");
    return NULL;
  }
  memset(sse, 0, sizeof(*sse));
  sse->public.push = lonejson_sse_push;
  sse->public.finish = lonejson_sse_finish;
  sse->public.push_json = lonejson__sse_push_json_method;
  sse->public.finish_json = lonejson__sse_finish_json_method;
  sse->public.push_json_value = lonejson__sse_push_json_value_method;
  sse->public.finish_json_value = lonejson__sse_finish_json_value_method;
  sse->public.close = lonejson_sse_close;
  sse->options = resolved;
  sse->allocator = lonejson__allocator_resolve(resolved.allocator);
  lonejson__clear_error(error);
  return &sse->public;
}

static lonejson_sse_event
lonejson__sse_event_view(const lonejson__sse_state *sse) {
  lonejson_sse_event event;
  memset(&event, 0, sizeof(event));
  event.event = sse->event.data ? sse->event.data : "";
  event.id = sse->id.data ? sse->id.data : NULL;
  event.data_len = sse->event_data_len;
  event.retry_ms = sse->retry_ms;
  event.has_retry = sse->has_retry;
  return event;
}

static void lonejson__sse_reset_event(lonejson__sse_state *sse) {
  lonejson__byte_reset(&sse->event);
  lonejson__byte_reset(&sse->id);
  sse->event_data_len = 0u;
  sse->retry_ms = 0u;
  sse->has_retry = 0;
  sse->event_pending = 0;
  sse->event_started = 0;
  sse->data_seen = 0;
}

static lonejson_status lonejson__sse_callback_error(lonejson_error *error,
                                                    const char *message) {
  if (error != NULL && error->code != LONEJSON_STATUS_OK) {
    return error->code;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             message);
}

static lonejson_status
lonejson__sse_begin_if_needed(lonejson__sse_state *sse,
                              const lonejson_sse_handler *handler, void *user,
                              lonejson_error *error) {
  lonejson_sse_event event;
  lonejson_status status;

  if (sse->event_started) {
    return LONEJSON_STATUS_OK;
  }
  sse->event_started = 1;
  if (handler != NULL && handler->begin_event != NULL) {
    event = lonejson__sse_event_view(sse);
    status = handler->begin_event(user, &event, error);
    if (status != LONEJSON_STATUS_OK) {
      return lonejson__sse_callback_error(error,
                                          "SSE begin_event callback failed");
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__sse_emit_data(lonejson__sse_state *sse,
                        const lonejson_sse_handler *handler, void *user,
                        const char *data, size_t len, lonejson_error *error) {
  lonejson_status status;
  size_t add_len;

  add_len = len + (sse->data_seen ? 1u : 0u);
  if (add_len > sse->options.max_event_data_bytes ||
      sse->event_data_len > sse->options.max_event_data_bytes - add_len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "SSE event data exceeds configured limit");
  }
  sse->event_pending = 1;
  status = lonejson__sse_begin_if_needed(sse, handler, user, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (sse->data_seen) {
    if (handler != NULL && handler->data_chunk != NULL) {
      status = handler->data_chunk(user, "\n", 1u, error);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__sse_callback_error(error,
                                            "SSE data_chunk callback failed");
      }
    }
    ++sse->event_data_len;
  }
  if (len != 0u && handler != NULL && handler->data_chunk != NULL) {
    status = handler->data_chunk(user, data, len, error);
    if (status != LONEJSON_STATUS_OK) {
      return lonejson__sse_callback_error(error,
                                          "SSE data_chunk callback failed");
    }
  }
  sse->event_data_len += len;
  sse->data_seen = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sse_emit(lonejson__sse_state *sse,
                                          const lonejson_sse_handler *handler,
                                          void *user, lonejson_error *error) {
  lonejson_sse_event event;
  lonejson_status status;

  if (!sse->event_pending) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__sse_begin_if_needed(sse, handler, user, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (handler != NULL && handler->end_event != NULL) {
    event = lonejson__sse_event_view(sse);
    status = handler->end_event(user, &event, error);
    if (status != LONEJSON_STATUS_OK) {
      return lonejson__sse_callback_error(error,
                                          "SSE end_event callback failed");
    }
  }
  lonejson__sse_reset_event(sse);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__sse_process_line(lonejson__sse_state *sse,
                           const lonejson_sse_handler *handler, void *user,
                           lonejson_error *error) {
  const char *line;
  const char *colon;
  const char *value;
  const char *line_end;
  size_t name_len;
  size_t value_len;
  unsigned long retry;

  line = sse->line.data ? sse->line.data : "";
  line_end = line + sse->line.len;
  if (sse->line.len == 0u) {
    return lonejson__sse_emit(sse, handler, user, error);
  }
  if (line[0] == ':') {
    return LONEJSON_STATUS_OK;
  }
  colon = (const char *)memchr(line, ':', sse->line.len);
  if (colon == NULL) {
    name_len = sse->line.len;
    value = line_end;
  } else {
    name_len = (size_t)(colon - line);
    value = colon + 1;
    if (value < line_end && *value == ' ') {
      ++value;
    }
  }
  value_len = (size_t)(line_end - value);
  if (name_len == 4u && memcmp(line, "data", 4u) == 0) {
    return lonejson__sse_emit_data(sse, handler, user, value, value_len, error);
  }
  if (name_len == 5u && memcmp(line, "event", 5u) == 0) {
    lonejson__byte_reset(&sse->event);
    sse->event_pending = 1;
    return lonejson__byte_append(&sse->event, value, value_len,
                                 sse->options.max_buffered_bytes,
                                 &sse->allocator, error);
  }
  if (name_len == 2u && memcmp(line, "id", 2u) == 0) {
    if (memchr(value, '\0', value_len) == NULL) {
      lonejson__byte_reset(&sse->id);
      sse->event_pending = 1;
      return lonejson__byte_append(&sse->id, value, value_len,
                                   sse->options.max_buffered_bytes,
                                   &sse->allocator, error);
    }
  }
  if (name_len == 5u && memcmp(line, "retry", 5u) == 0) {
    if (lonejson__parse_sse_retry(value, value_len, &retry)) {
      sse->retry_ms = retry;
      sse->has_retry = 1;
      sse->event_pending = 1;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__sse_feed_char(lonejson__sse_state *sse, char ch,
                        const lonejson_sse_handler *handler, void *user,
                        lonejson_error *error) {
  lonejson_status status;

  if (sse->saw_cr) {
    sse->saw_cr = 0;
    status = lonejson__sse_process_line(sse, handler, user, error);
    lonejson__byte_reset(&sse->line);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    if (ch == '\n') {
      return LONEJSON_STATUS_OK;
    }
  }
  if (ch == '\r') {
    sse->saw_cr = 1;
    return LONEJSON_STATUS_OK;
  }
  if (ch == '\n') {
    status = lonejson__sse_process_line(sse, handler, user, error);
    lonejson__byte_reset(&sse->line);
    return status;
  }
  return lonejson__byte_append_char(&sse->line, ch, sse->options.max_line_bytes,
                                    &sse->allocator, error);
}

lonejson_status lonejson_sse_push(lonejson_sse *receiver, const void *bytes,
                                  size_t len,
                                  const lonejson_sse_handler *handler,
                                  void *user, lonejson_error *error) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  const char *p;
  size_t i;
  lonejson_status status;

  if (sse == NULL || (bytes == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser and bytes are required");
  }
  if (sse->closed) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser is already finished");
  }
  lonejson__clear_error(error);
  p = (const char *)bytes;
  for (i = 0u; i < len; ++i) {
    status = lonejson__sse_feed_char(sse, p[i], handler, user, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_sse_finish(lonejson_sse *receiver,
                                    const lonejson_sse_handler *handler,
                                    void *user, lonejson_error *error) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  lonejson_status status;

  if (sse == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser is required");
  }
  if (sse->closed) {
    return LONEJSON_STATUS_OK;
  }
  if (sse->saw_cr || sse->line.len != 0u) {
    sse->saw_cr = 0;
    status = lonejson__sse_process_line(sse, handler, user, error);
    lonejson__byte_reset(&sse->line);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__sse_emit(sse, handler, user, error);
  if (status == LONEJSON_STATUS_OK) {
    sse->closed = 1;
  }
  return status;
}

typedef struct lonejson__sse_json_ctx {
  const lonejson_map *map;
  void *dst;
  lonejson_json_value *value;
  const lonejson_sse_json_options *options;
  const lonejson__parse_options *parse_options;
  const lonejson_runtime *runtime;
  lonejson_sse_json_event_fn cb;
  lonejson_sse_json_value_event_fn value_cb;
  void *user;
} lonejson__sse_json_ctx;

static void lonejson__sse_json_release_runtime(lonejson__sse_state *sse) {
  if (sse == NULL) {
    return;
  }
  lonejson__runtime_free_owned_config(&sse->json_runtime_storage);
  sse->json_runtime = NULL;
  sse->json_runtime_source = NULL;
}

static lonejson_status
lonejson__sse_json_snapshot_runtime(lonejson__sse_state *sse,
                                    const lonejson__sse_json_ctx *ctx,
                                    lonejson_error *error) {
  if (sse == NULL || ctx == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE JSON parser context is required");
  }
  if (lonejson__runtime_snapshot_init(&sse->json_runtime_storage, ctx->runtime,
                                      1, error) != LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  sse->json_runtime = ctx->runtime != NULL ? &sse->json_runtime_storage
                                           : (const lonejson_runtime *)NULL;
  sse->json_runtime_source = ctx->runtime;
  sse->json_parse_options_storage = ctx->parse_options != NULL
                                        ? *ctx->parse_options
                                        : lonejson__default_parse_options();
  if (ctx->runtime != NULL && sse->json_parse_options_storage.allocator ==
                                  ctx->runtime->config.allocator) {
    sse->json_parse_options_storage.allocator =
        sse->json_runtime->config.allocator;
  }
  if (ctx->runtime != NULL &&
      sse->json_parse_options_storage.fixed_string_scratch ==
          ctx->runtime->config.fixed_string_scratch) {
    sse->json_parse_options_storage.fixed_string_scratch =
        sse->json_runtime->parse_options.fixed_string_scratch;
    sse->json_parse_options_storage.fixed_string_scratch_size =
        sse->json_runtime->parse_options.fixed_string_scratch_size;
  }
  sse->json_parse_options = &sse->json_parse_options_storage;
  return LONEJSON_STATUS_OK;
}

static int
lonejson__sse_json_event_selected(const lonejson_sse_json_options *options,
                                  const char *event_name) {
  size_t i;
  if (options == NULL || options->event_names == NULL ||
      options->event_name_count == 0u) {
    return 1;
  }
  for (i = 0u; i < options->event_name_count; ++i) {
    const char *candidate = options->event_names[i];
    if (candidate != NULL && strcmp(candidate, event_name) == 0) {
      return 1;
    }
  }
  return 0;
}

static int
lonejson__sse_json_has_filter(const lonejson_sse_json_options *options) {
  return options != NULL && options->event_names != NULL &&
         options->event_name_count != 0u;
}

static void
lonejson__sse_json_cleanup_retained_map_destination(lonejson__sse_state *sse) {
  if (sse == NULL) {
    return;
  }
  if (sse->json_retained_active && sse->json_retained_map != NULL &&
      sse->json_retained_dst != NULL) {
    lonejson__cleanup_map_checked(sse->json_retained_map,
                                  sse->json_retained_dst);
  }
  sse->json_retained_map = NULL;
  sse->json_retained_dst = NULL;
  sse->json_retained_active = 0;
}

static void lonejson__sse_json_cleanup_retained_value_destination(
    lonejson__sse_state *sse) {
  if (sse == NULL) {
    return;
  }
  if (sse->json_retained_value_active && sse->json_retained_value != NULL) {
    lonejson__json_value_clear_runtime(sse->json_retained_value);
  }
  sse->json_retained_value = NULL;
  sse->json_retained_value_active = 0;
}

static void
lonejson__sse_json_cleanup_current_destination(lonejson__sse_state *sse) {
  if (sse == NULL) {
    return;
  }
  if (sse->json_value != NULL) {
    lonejson__sse_json_cleanup_retained_value_destination(sse);
    return;
  }
  if (sse->json_parser.options.clear_destination) {
    lonejson__sse_json_cleanup_retained_map_destination(sse);
  }
}

static void lonejson__sse_json_reset_event(lonejson__sse_state *sse) {
  if (sse->json_parser_active) {
    lonejson_parser_destroy(&sse->json_parser);
  }
  sse->json_map = NULL;
  sse->json_dst = NULL;
  sse->json_value = NULL;
  sse->json_parse_options = NULL;
  lonejson__sse_json_release_runtime(sse);
  sse->json_data_len = 0u;
  sse->json_parser_active = 0;
  sse->json_data_seen = 0;
  sse->json_selection_locked = 0;
  sse->json_selected = 0;
  lonejson__byte_reset(&sse->event);
  lonejson__byte_reset(&sse->id);
  sse->retry_ms = 0u;
  sse->has_retry = 0;
}

static lonejson_status
lonejson__sse_json_begin_parser(lonejson__sse_state *sse,
                                const lonejson__sse_json_ctx *ctx,
                                lonejson_error *error) {
  if (sse->json_parser_active) {
    if (sse->json_runtime_source != ctx->runtime) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
          "cannot change SSE JSON runtime while an event is active");
    }
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__sse_json_snapshot_runtime(sse, ctx, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  lonejson__parser_init_state(&sse->json_parser, ctx->map, ctx->dst,
                              sse->json_parse_options, sse->json_runtime,
                              ctx->value != NULL, 0, 0, 0u, sse->json_workspace,
                              sizeof(sse->json_workspace));
  if (ctx->value != NULL) {
    lonejson_status status = lonejson__json_value_prepare_parse(
        &sse->json_parser, ctx->value, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_parser_destroy(&sse->json_parser);
      lonejson__sse_json_release_runtime(sse);
      return status;
    }
    lonejson__parser_set_json_stream_value(&sse->json_parser, ctx->value);
    sse->json_retained_value = ctx->value;
    sse->json_retained_value_active = 1;
  } else if (sse->json_parser.options.clear_destination) {
    lonejson__sse_json_cleanup_retained_map_destination(sse);
    lonejson__init_map_with_allocator(ctx->map, ctx->dst,
                                      &sse->json_parser.allocator,
                                      sse->json_parser.runtime);
    sse->json_retained_map = ctx->map;
    sse->json_retained_dst = ctx->dst;
    sse->json_retained_active = 1;
  }
  sse->json_map = ctx->map;
  sse->json_dst = ctx->dst;
  sse->json_value = ctx->value;
  sse->json_parser_active = 1;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sse_json_feed_data(
    lonejson__sse_state *sse, const lonejson__sse_json_ctx *ctx,
    const char *data, size_t len, lonejson_error *error) {
  const char newline = '\n';
  lonejson_status status;
  size_t add_len;

  if (!sse->json_selection_locked) {
    sse->json_selected = lonejson__sse_json_event_selected(
        ctx->options, sse->event.data ? sse->event.data : "");
    sse->json_selection_locked = 1;
  }
  if (!sse->json_selected) {
    return LONEJSON_STATUS_OK;
  }
  add_len = len + (sse->json_data_seen ? 1u : 0u);
  if (add_len > sse->options.max_event_data_bytes ||
      sse->json_data_len > sse->options.max_event_data_bytes - add_len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "SSE JSON event data exceeds configured limit");
  }
  if (sse->json_parser_active &&
      (sse->json_map != ctx->map || sse->json_dst != ctx->dst ||
       sse->json_value != ctx->value ||
       sse->json_runtime_source != ctx->runtime)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "SSE JSON parser arguments changed during an event");
  }
  status = lonejson__sse_json_begin_parser(sse, ctx, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (sse->json_data_seen) {
    status = lonejson_parser_feed(&sse->json_parser, &newline, 1u);
    if (status != LONEJSON_STATUS_OK) {
      if (error != NULL) {
        *error = sse->json_parser.error;
      }
      lonejson__sse_json_cleanup_current_destination(sse);
      lonejson__sse_json_reset_event(sse);
      return status;
    }
    ++sse->json_data_len;
  }
  status = lonejson_parser_feed(&sse->json_parser, data, len);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      *error = sse->json_parser.error;
    }
    lonejson__sse_json_cleanup_current_destination(sse);
    lonejson__sse_json_reset_event(sse);
    return status;
  }
  sse->json_data_len += len;
  sse->json_data_seen = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__sse_json_emit(lonejson__sse_state *sse,
                        const lonejson__sse_json_ctx *ctx,
                        lonejson_error *error) {
  lonejson_sse_event event;
  lonejson_status status;

  if (!sse->json_data_seen && sse->event.len == 0u && sse->id.len == 0u &&
      !sse->has_retry) {
    return LONEJSON_STATUS_OK;
  }
  if (!sse->json_selection_locked) {
    sse->json_selected = lonejson__sse_json_event_selected(
        ctx->options, sse->event.data ? sse->event.data : "");
    sse->json_selection_locked = 1;
  }
  if (!sse->json_selected) {
    lonejson__sse_json_reset_event(sse);
    return LONEJSON_STATUS_OK;
  }
  if (!sse->json_data_seen) {
    lonejson__sse_json_reset_event(sse);
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__sse_json_begin_parser(sse, ctx, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_parser_finish(&sse->json_parser);
  }
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      *error = sse->json_parser.error;
    }
    lonejson__sse_json_cleanup_current_destination(sse);
    lonejson__sse_json_reset_event(sse);
    return status;
  }
  if (ctx->cb != NULL) {
    memset(&event, 0, sizeof(event));
    event.event = sse->event.data ? sse->event.data : "";
    event.id = sse->id.data ? sse->id.data : NULL;
    event.data_len = sse->json_data_len;
    event.retry_ms = sse->retry_ms;
    event.has_retry = sse->has_retry;
    status = ctx->cb(ctx->user, &event, ctx->dst, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson__sse_json_cleanup_current_destination(sse);
      lonejson__sse_json_reset_event(sse);
      if (error != NULL && error->code != LONEJSON_STATUS_OK) {
        return error->code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                                 0u, "SSE JSON callback failed");
    }
  } else if (ctx->value_cb != NULL) {
    memset(&event, 0, sizeof(event));
    event.event = sse->event.data ? sse->event.data : "";
    event.id = sse->id.data ? sse->id.data : NULL;
    event.data_len = sse->json_data_len;
    event.retry_ms = sse->retry_ms;
    event.has_retry = sse->has_retry;
    status = ctx->value_cb(ctx->user, &event, ctx->value, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson__sse_json_cleanup_current_destination(sse);
      lonejson__sse_json_reset_event(sse);
      if (error != NULL && error->code != LONEJSON_STATUS_OK) {
        return error->code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                                 0u, "SSE JSON callback failed");
    }
  }
  lonejson__sse_json_reset_event(sse);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__sse_json_process_line(lonejson__sse_state *sse,
                                const lonejson__sse_json_ctx *ctx,
                                lonejson_error *error) {
  const char *line;
  const char *line_end;
  const char *colon;
  const char *value;
  size_t name_len;
  size_t value_len;
  unsigned long retry;

  line = sse->line.data ? sse->line.data : "";
  line_end = line + sse->line.len;
  if (sse->line.len == 0u) {
    return lonejson__sse_json_emit(sse, ctx, error);
  }
  if (line[0] == ':') {
    return LONEJSON_STATUS_OK;
  }
  colon = (const char *)memchr(line, ':', sse->line.len);
  if (colon == NULL) {
    name_len = sse->line.len;
    value = line_end;
  } else {
    name_len = (size_t)(colon - line);
    value = colon + 1;
    if (value < line_end && *value == ' ') {
      ++value;
    }
  }
  value_len = (size_t)(line_end - value);
  if (name_len == 4u && memcmp(line, "data", 4u) == 0) {
    return lonejson__sse_json_feed_data(sse, ctx, value, value_len, error);
  }
  if (name_len == 5u && memcmp(line, "event", 5u) == 0) {
    if (sse->json_data_seen && lonejson__sse_json_has_filter(ctx->options)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
          "SSE event name must precede data for streaming JSON filters");
    }
    lonejson__byte_reset(&sse->event);
    return lonejson__byte_append(&sse->event, value, value_len,
                                 sse->options.max_buffered_bytes,
                                 &sse->allocator, error);
  }
  if (name_len == 2u && memcmp(line, "id", 2u) == 0) {
    if (memchr(value, '\0', value_len) == NULL) {
      lonejson__byte_reset(&sse->id);
      return lonejson__byte_append(&sse->id, value, value_len,
                                   sse->options.max_buffered_bytes,
                                   &sse->allocator, error);
    }
  }
  if (name_len == 5u && memcmp(line, "retry", 5u) == 0) {
    if (lonejson__parse_sse_retry(value, value_len, &retry)) {
      sse->retry_ms = retry;
      sse->has_retry = 1;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__sse_json_feed_char(lonejson__sse_state *sse, char ch,
                             const lonejson__sse_json_ctx *ctx,
                             lonejson_error *error) {
  lonejson_status status;

  if (sse->saw_cr) {
    sse->saw_cr = 0;
    status = lonejson__sse_json_process_line(sse, ctx, error);
    lonejson__byte_reset(&sse->line);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    if (ch == '\n') {
      return LONEJSON_STATUS_OK;
    }
  }
  if (ch == '\r') {
    sse->saw_cr = 1;
    return LONEJSON_STATUS_OK;
  }
  if (ch == '\n') {
    status = lonejson__sse_json_process_line(sse, ctx, error);
    lonejson__byte_reset(&sse->line);
    return status;
  }
  return lonejson__byte_append_char(&sse->line, ch, sse->options.max_line_bytes,
                                    &sse->allocator, error);
}

static lonejson_status lonejson__sse_push_json_impl(
    lonejson__sse_state *sse, const lonejson__sse_json_ctx *ctx,
    const void *bytes, size_t len, lonejson_error *error) {
  const char *p;
  size_t i;
  lonejson_status status;

  if (sse == NULL || (bytes == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser and bytes are required");
  }
  if (sse->closed) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser is already finished");
  }
  lonejson__clear_error(error);
  p = (const char *)bytes;
  for (i = 0u; i < len; ++i) {
    status = lonejson__sse_json_feed_char(sse, p[i], ctx, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sse_push_json_with_options(
    lonejson_sse *receiver, const lonejson_map *map, void *dst,
    const void *bytes, size_t len, const lonejson__parse_options *parse_options,
    const lonejson_runtime *runtime, const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  lonejson__sse_json_ctx ctx;
  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  if (parse_options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(parse_options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  ctx.map = map;
  ctx.dst = dst;
  ctx.value = NULL;
  ctx.options = options;
  ctx.parse_options = parse_options;
  ctx.runtime = runtime;
  ctx.cb = event_cb;
  ctx.value_cb = NULL;
  ctx.user = user;
  return lonejson__sse_push_json_impl(sse, &ctx, bytes, len, error);
}

LONEJSON__NOINLINE
LONEJSON__COLD lonejson_status lonejson_sse_push_json(
    lonejson *runtime, lonejson_sse *sse, const lonejson_map *map, void *dst,
    const void *bytes, size_t len, const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__sse_push_json_with_options(
      sse, map, dst, bytes, len, &runtime_state->parse_options, runtime_state,
      options, event_cb, user, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status lonejson__sse_finish_json_with_options(
    lonejson_sse *receiver, const lonejson_map *map, void *dst,
    const lonejson__parse_options *parse_options,
    const lonejson_runtime *runtime, const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  lonejson__sse_json_ctx ctx;
  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  if (parse_options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(parse_options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  ctx.map = map;
  ctx.dst = dst;
  ctx.value = NULL;
  ctx.options = options;
  ctx.parse_options = parse_options;
  ctx.runtime = runtime;
  ctx.cb = event_cb;
  ctx.value_cb = NULL;
  ctx.user = user;
  if (sse == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser is required");
  }
  if (sse->closed) {
    return LONEJSON_STATUS_OK;
  }
  if (sse->saw_cr || sse->line.len != 0u) {
    sse->saw_cr = 0;
    {
      lonejson_status status =
          lonejson__sse_json_process_line(sse, &ctx, error);
      lonejson__byte_reset(&sse->line);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  {
    lonejson_status status = lonejson__sse_json_emit(sse, &ctx, error);
    if (status == LONEJSON_STATUS_OK) {
      sse->closed = 1;
    }
    return status;
  }
}

lonejson_status lonejson_sse_finish_json(
    lonejson *runtime, lonejson_sse *sse, const lonejson_map *map, void *dst,
    const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__sse_finish_json_with_options(
      sse, map, dst, &runtime_state->parse_options, runtime_state, options,
      event_cb, user, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status lonejson__sse_push_json_value_with_options(
    lonejson_sse *receiver, lonejson_json_value *value, const void *bytes,
    size_t len, const lonejson__parse_options *parse_options,
    const lonejson_runtime *runtime, const lonejson_sse_json_options *options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  lonejson__sse_json_ctx ctx;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (parse_options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(parse_options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  ctx.map = NULL;
  ctx.dst = NULL;
  ctx.value = value;
  ctx.options = options;
  ctx.parse_options = parse_options;
  ctx.runtime = runtime;
  ctx.cb = NULL;
  ctx.value_cb = event_cb;
  ctx.user = user;
  return lonejson__sse_push_json_impl(sse, &ctx, bytes, len, error);
}

LONEJSON__NOINLINE
LONEJSON__COLD lonejson_status lonejson_sse_push_json_value(
    lonejson *runtime, lonejson_sse *sse, lonejson_json_value *value,
    const void *bytes, size_t len, const lonejson_sse_json_options *options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__sse_push_json_value_with_options(
      sse, value, bytes, len, &runtime_state->parse_options, runtime_state,
      options, event_cb, user, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status lonejson__sse_finish_json_value_with_options(
    lonejson_sse *receiver, lonejson_json_value *value,
    const lonejson__parse_options *parse_options,
    const lonejson_runtime *runtime, const lonejson_sse_json_options *options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  lonejson__sse_json_ctx ctx;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (parse_options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(parse_options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  ctx.map = NULL;
  ctx.dst = NULL;
  ctx.value = value;
  ctx.options = options;
  ctx.parse_options = parse_options;
  ctx.runtime = runtime;
  ctx.cb = NULL;
  ctx.value_cb = event_cb;
  ctx.user = user;
  if (sse == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SSE parser is required");
  }
  if (sse->closed) {
    return LONEJSON_STATUS_OK;
  }
  if (sse->saw_cr || sse->line.len != 0u) {
    sse->saw_cr = 0;
    {
      lonejson_status status =
          lonejson__sse_json_process_line(sse, &ctx, error);
      lonejson__byte_reset(&sse->line);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  {
    lonejson_status status = lonejson__sse_json_emit(sse, &ctx, error);
    if (status == LONEJSON_STATUS_OK) {
      sse->closed = 1;
    }
    return status;
  }
}

lonejson_status
lonejson_sse_finish_json_value(lonejson *runtime, lonejson_sse *sse,
                               lonejson_json_value *value,
                               const lonejson_sse_json_options *options,
                               lonejson_sse_json_value_event_fn event_cb,
                               void *user, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__sse_finish_json_value_with_options(
      sse, value, &runtime_state->parse_options, runtime_state, options,
      event_cb, user, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

void lonejson_sse_close(lonejson_sse *receiver) {
  lonejson__sse_state *sse = lonejson__sse_state_mut(receiver);
  if (sse == NULL) {
    return;
  }
  lonejson__byte_free(&sse->line, &sse->allocator);
  lonejson__byte_free(&sse->event, &sse->allocator);
  lonejson__byte_free(&sse->id, &sse->allocator);
  if (sse->json_parser_active) {
    lonejson_parser_destroy(&sse->json_parser);
  }
  lonejson__sse_json_release_runtime(sse);
  lonejson__buffer_free(&sse->allocator, sse, sizeof(*sse));
}

lonejson_multipart_options lonejson_default_multipart_options(void) {
  lonejson_multipart_options options;

  memset(&options, 0, sizeof(options));
  options.max_boundary_bytes = 200u;
  options.max_header_line_bytes = 64u * 1024u;
  options.max_header_count = 64u;
  options.max_part_buffered_bytes = 1024u * 1024u;
  options.allocator = NULL;
  return options;
}

static lonejson_status lonejson__multipart_parse_boundary(
    const char *content_type, lonejson__byte_buffer *out,
    const lonejson_multipart_options *options,
    const lonejson_allocator *allocator, lonejson_error *error) {
  const char *p;
  const char *name;
  const char *value;
  const char *end;
  size_t name_len;
  size_t len;

  p = content_type;
  while (*p != '\0') {
    while (*p == ';' || *p == ' ' || *p == '\t') {
      ++p;
    }
    name = p;
    while (*p != '\0' && *p != '=' && *p != ';') {
      ++p;
    }
    end = p;
    while (end > name && (end[-1] == ' ' || end[-1] == '\t')) {
      --end;
    }
    name_len = (size_t)(end - name);
    if (*p != '=') {
      while (*p != '\0' && *p != ';') {
        ++p;
      }
      continue;
    }
    ++p;
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    value = p;
    if (*value == '"') {
      ++value;
      end = value;
      while (*end != '\0' && *end != '"') {
        ++end;
      }
      if (*end != '"') {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u,
                                   "multipart boundary quote is unterminated");
      }
      p = end + 1;
    } else {
      end = value;
      while (*end != '\0' && *end != ';' && *end != ' ' && *end != '\t') {
        ++end;
      }
      p = end;
    }
    if (lonejson__ascii_ieq(name, name_len, "boundary")) {
      len = (size_t)(end - value);
      if (len == 0u) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "multipart boundary is invalid");
      }
      if (len > options->max_boundary_bytes) {
        return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                                   "multipart boundary length limit exceeded");
      }
      return lonejson__byte_append(out, value, len, options->max_boundary_bytes,
                                   allocator, error);
    }
  }
  return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                             0u, "multipart Content-Type missing boundary");
}

lonejson_multipart *
lonejson_multipart_open(const char *content_type,
                        const lonejson_multipart_options *options,
                        lonejson_error *error) {
  lonejson__multipart_state *mp;
  lonejson_multipart_options resolved;
  lonejson_status status;

  if (content_type == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "multipart Content-Type is required");
    return NULL;
  }
  resolved = options ? *options : lonejson_default_multipart_options();
  if (resolved.max_boundary_bytes == 0u) {
    resolved.max_boundary_bytes = 200u;
  }
  if (resolved.max_header_line_bytes == 0u) {
    resolved.max_header_line_bytes = 64u * 1024u;
  }
  if (resolved.max_header_count == 0u) {
    resolved.max_header_count = 64u;
  }
  if (resolved.max_part_buffered_bytes == 0u) {
    resolved.max_part_buffered_bytes = 1024u * 1024u;
  }
  if (!LONEJSON__ALLOCATOR_IS_VALID_CONFIG(resolved.allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  mp = (lonejson__multipart_state *)lonejson__buffer_alloc(resolved.allocator,
                                                           sizeof(*mp));
  if (mp == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate multipart parser");
    return NULL;
  }
  memset(mp, 0, sizeof(*mp));
  mp->public.push = lonejson_multipart_push;
  mp->public.finish = lonejson_multipart_finish;
  mp->public.close = lonejson_multipart_close;
  mp->options = resolved;
  mp->allocator = lonejson__allocator_resolve(resolved.allocator);
  mp->content_length = -1;
  mp->phase = LONEJSON__MULTIPART_EXPECT_BOUNDARY;
  status = lonejson__multipart_parse_boundary(
      content_type, &mp->boundary, &mp->options, &mp->allocator, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_multipart_close(&mp->public);
    return NULL;
  }
  status = lonejson__byte_append(&mp->boundary_line, "--", 2u,
                                 mp->options.max_boundary_bytes + 4u,
                                 &mp->allocator, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__byte_append(
        &mp->boundary_line, mp->boundary.data, mp->boundary.len,
        mp->options.max_boundary_bytes + 4u, &mp->allocator, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__byte_append(
        &mp->closing_boundary_line, mp->boundary_line.data,
        mp->boundary_line.len, mp->options.max_boundary_bytes + 6u,
        &mp->allocator, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_multipart_close(&mp->public);
    return NULL;
  }
  status = lonejson__byte_append(&mp->closing_boundary_line, "--", 2u,
                                 mp->options.max_boundary_bytes + 6u,
                                 &mp->allocator, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_multipart_close(&mp->public);
    return NULL;
  }
  lonejson__clear_error(error);
  return &mp->public;
}

static void lonejson__multipart_reset_part(lonejson__multipart_state *mp) {
  size_t i;
  for (i = 0u; i < mp->header_count; ++i) {
    lonejson__buffer_free(&mp->allocator, mp->headers[i].name,
                          strlen(mp->headers[i].name) + 1u);
    lonejson__buffer_free(&mp->allocator, mp->headers[i].value,
                          strlen(mp->headers[i].value) + 1u);
  }
  mp->header_count = 0u;
  lonejson__buffer_free(&mp->allocator, mp->part_name,
                        mp->part_name_alloc_size);
  mp->part_name = NULL;
  mp->part_name_alloc_size = 0u;
  lonejson__buffer_free(&mp->allocator, mp->content_type,
                        mp->content_type_alloc_size);
  mp->content_type = NULL;
  mp->content_type_alloc_size = 0u;
  mp->content_length = -1;
  mp->remaining = 0;
  lonejson__byte_reset(&mp->body);
  mp->in_part = 0;
}

static lonejson_multipart_part
lonejson__multipart_part_view(const lonejson__multipart_state *mp) {
  lonejson_multipart_part part;
  memset(&part, 0, sizeof(part));
  part.name = mp->part_name;
  part.content_type = mp->content_type;
  part.content_length = mp->content_length;
  part.headers = mp->headers;
  part.header_count = mp->header_count;
  return part;
}

static lonejson_status
lonejson__multipart_call_begin(lonejson__multipart_state *mp,
                               const lonejson_multipart_handler *handler,
                               void *user, lonejson_error *error) {
  lonejson_multipart_part part;
  lonejson_status status;
  if (handler == NULL || handler->begin_part == NULL) {
    return LONEJSON_STATUS_OK;
  }
  part = lonejson__multipart_part_view(mp);
  status = handler->begin_part(user, &part, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL && error->code != LONEJSON_STATUS_OK) {
      return error->code;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "multipart begin_part callback failed");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__multipart_call_data(const lonejson_multipart_handler *handler,
                              void *user, const void *bytes, size_t len,
                              lonejson_error *error) {
  lonejson_status status;
  if (len == 0u || handler == NULL || handler->part_data == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = handler->part_data(user, bytes, len, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL && error->code != LONEJSON_STATUS_OK) {
      return error->code;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "multipart part_data callback failed");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__multipart_call_end(lonejson__multipart_state *mp,
                             const lonejson_multipart_handler *handler,
                             void *user, lonejson_error *error) {
  lonejson_multipart_part part;
  lonejson_status status;
  if (handler == NULL || handler->end_part == NULL) {
    return LONEJSON_STATUS_OK;
  }
  part = lonejson__multipart_part_view(mp);
  status = handler->end_part(user, &part, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL && error->code != LONEJSON_STATUS_OK) {
      return error->code;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "multipart end_part callback failed");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__multipart_add_header(lonejson__multipart_state *mp, const char *name,
                               size_t name_len, const char *value,
                               size_t value_len, lonejson_error *error) {
  lonejson_header *next;
  size_t next_cap;
  char *owned_name;
  char *owned_value;
  size_t alloc_size;

  if (mp->header_count >= mp->options.max_header_count) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "multipart header count limit exceeded");
  }
  if (mp->header_count == mp->header_cap) {
    next_cap = mp->header_cap == 0u ? 8u : mp->header_cap * 2u;
    if (next_cap > mp->options.max_header_count) {
      next_cap = mp->options.max_header_count;
    }
    next = (lonejson_header *)lonejson__buffer_realloc(
        &mp->allocator, mp->headers, mp->headers_alloc_size,
        next_cap * sizeof(*next));
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate headers");
    }
    mp->headers = next;
    mp->headers_alloc_size = next_cap * sizeof(*next);
    mp->header_cap = next_cap;
  }
  owned_name =
      lonejson__dup_range(name, name_len, &mp->allocator, &alloc_size, error);
  if (owned_name == NULL) {
    return error ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  owned_value =
      lonejson__dup_range(value, value_len, &mp->allocator, NULL, error);
  if (owned_value == NULL) {
    lonejson__buffer_free(&mp->allocator, owned_name, alloc_size);
    return error ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  mp->headers[mp->header_count].name = owned_name;
  mp->headers[mp->header_count].value = owned_value;
  mp->header_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__multipart_parse_content_disposition(
    lonejson__multipart_state *mp, const char *value, lonejson_error *error) {
  const char *p;
  const char *name;
  const char *name_end;
  const char *begin;
  const char *end;
  lonejson__byte_buffer unescaped;
  lonejson_status status;

  p = value;
  while (*p != '\0' && *p != ';') {
    ++p;
  }
  while (*p == ';') {
    ++p;
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    name = p;
    while (*p != '\0' && *p != '=' && *p != ';') {
      ++p;
    }
    name_end = p;
    while (name_end > name && (name_end[-1] == ' ' || name_end[-1] == '\t')) {
      --name_end;
    }
    if (*p != '=') {
      while (*p != '\0' && *p != ';') {
        ++p;
      }
      continue;
    }
    ++p;
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    memset(&unescaped, 0, sizeof(unescaped));
    if (*p == '"') {
      ++p;
      while (*p != '\0' && *p != '"') {
        if (*p == '\\' && p[1] != '\0') {
          ++p;
        }
        status = lonejson__byte_append_char(&unescaped, *p,
                                            mp->options.max_header_line_bytes,
                                            &mp->allocator, error);
        if (status != LONEJSON_STATUS_OK) {
          lonejson__byte_free(&unescaped, &mp->allocator);
          return status;
        }
        ++p;
      }
      if (*p != '"') {
        lonejson__byte_free(&unescaped, &mp->allocator);
        return lonejson__set_error(
            error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
            "multipart Content-Disposition name invalid");
      }
      ++p;
      begin = unescaped.data ? unescaped.data : "";
      end = begin + unescaped.len;
    } else {
      begin = p;
      while (*p != '\0' && *p != ';' && *p != ' ' && *p != '\t') {
        ++p;
      }
      end = p;
    }
    if (lonejson__ascii_ieq(name, (size_t)(name_end - name), "name")) {
      lonejson__buffer_free(&mp->allocator, mp->part_name,
                            mp->part_name_alloc_size);
      mp->part_name =
          lonejson__dup_range(begin, (size_t)(end - begin), &mp->allocator,
                              &mp->part_name_alloc_size, error);
      lonejson__byte_free(&unescaped, &mp->allocator);
      return mp->part_name
                 ? LONEJSON_STATUS_OK
                 : (error ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED);
    }
    lonejson__byte_free(&unescaped, &mp->allocator);
    while (*p != '\0' && *p != ';') {
      ++p;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__multipart_process_header_line(
    lonejson__multipart_state *mp, const char *line, lonejson_error *error) {
  const char *colon;
  const char *value;
  const char *value_end;
  const char *num;
  size_t name_len;
  lonejson_int64 parsed;
  lonejson_uint64 acc;
  lonejson_uint64 digit;
  lonejson_uint64 max_i64;
  lonejson_status status;

  colon = strchr(line, ':');
  if (colon == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "invalid multipart header line");
  }
  name_len = (size_t)(colon - line);
  value = colon + 1;
  while (*value == ' ' || *value == '\t') {
    ++value;
  }
  value_end = value + strlen(value);
  while (value_end > value && (value_end[-1] == ' ' || value_end[-1] == '\t')) {
    --value_end;
  }
  status = lonejson__multipart_add_header(mp, line, name_len, value,
                                          (size_t)(value_end - value), error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (lonejson__ascii_ieq(line, name_len, "Content-Disposition")) {
    return lonejson__multipart_parse_content_disposition(mp, value, error);
  }
  if (lonejson__ascii_ieq(line, name_len, "Content-Type")) {
    lonejson__buffer_free(&mp->allocator, mp->content_type,
                          mp->content_type_alloc_size);
    mp->content_type =
        lonejson__dup_range(value, (size_t)(value_end - value), &mp->allocator,
                            &mp->content_type_alloc_size, error);
    return mp->content_type
               ? LONEJSON_STATUS_OK
               : (error ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED);
  }
  if (lonejson__ascii_ieq(line, name_len, "Content-Length")) {
    max_i64 = ((lonejson_uint64) ~(lonejson_uint64)0u) >> 1u;
    acc = 0u;
    if (value == value_end) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                                 0u, "multipart Content-Length is invalid");
    }
    for (num = value; num < value_end; ++num) {
      if (*num < '0' || *num > '9') {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                                   0u, "multipart Content-Length is invalid");
      }
      digit = (lonejson_uint64)(*num - '0');
      if (acc > (max_i64 - digit) / 10u) {
        return lonejson__set_error(
            error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
            "multipart Content-Length exceeds supported range");
      }
      acc = (acc * 10u) + digit;
    }
    parsed = (lonejson_int64)acc;
    if (parsed < 0) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                                 0u, "multipart Content-Length is invalid");
    }
    mp->content_length = parsed;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__multipart_process_line(lonejson__multipart_state *mp,
                                 const lonejson_multipart_handler *handler,
                                 void *user, lonejson_error *error) {
  const char *line;
  lonejson_status status;

  line = mp->line.data ? mp->line.data : "";
  if (mp->phase == LONEJSON__MULTIPART_EXPECT_BOUNDARY) {
    if (mp->line.len == 0u) {
      return LONEJSON_STATUS_OK;
    }
    if (strcmp(line, mp->boundary_line.data) == 0) {
      lonejson__multipart_reset_part(mp);
      mp->phase = LONEJSON__MULTIPART_HEADERS;
      return LONEJSON_STATUS_OK;
    }
    if (strcmp(line, mp->closing_boundary_line.data) == 0) {
      mp->phase = LONEJSON__MULTIPART_DONE;
      mp->closed = 1;
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "unexpected multipart boundary line");
  }
  if (mp->phase == LONEJSON__MULTIPART_HEADERS) {
    if (mp->line.len == 0u) {
      status = lonejson__multipart_call_begin(mp, handler, user, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      mp->in_part = 1;
      if (mp->content_length >= 0) {
        mp->remaining = mp->content_length;
        if (mp->remaining == 0) {
          status = lonejson__multipart_call_end(mp, handler, user, error);
          lonejson__multipart_reset_part(mp);
          mp->phase = LONEJSON__MULTIPART_EXPECT_BOUNDARY;
          return status;
        }
        mp->phase = LONEJSON__MULTIPART_BODY_LENGTH;
      } else {
        mp->phase = LONEJSON__MULTIPART_BODY_SCAN;
      }
      return LONEJSON_STATUS_OK;
    }
    return lonejson__multipart_process_header_line(mp, line, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__multipart_feed_line_char(lonejson__multipart_state *mp, char ch,
                                   const lonejson_multipart_handler *handler,
                                   void *user, lonejson_error *error) {
  lonejson_status status;
  if (mp->saw_cr) {
    mp->saw_cr = 0;
    status = lonejson__multipart_process_line(mp, handler, user, error);
    lonejson__byte_reset(&mp->line);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    if (ch == '\n') {
      return LONEJSON_STATUS_OK;
    }
  }
  if (ch == '\r') {
    mp->saw_cr = 1;
    return LONEJSON_STATUS_OK;
  }
  if (ch == '\n') {
    status = lonejson__multipart_process_line(mp, handler, user, error);
    lonejson__byte_reset(&mp->line);
    return status;
  }
  return lonejson__byte_append_char(
      &mp->line, ch, mp->options.max_header_line_bytes, &mp->allocator, error);
}

static int lonejson__multipart_boundary_line_complete(
    const lonejson__byte_buffer *body, size_t pos, const char *line,
    size_t line_len, size_t *consume_len) {
  size_t after;

  if (body->len - pos < line_len ||
      memcmp(body->data + pos, line, line_len) != 0) {
    return 0;
  }
  after = pos + line_len;
  if (after == body->len) {
    return 0;
  }
  if (body->data[after] == '\n') {
    *consume_len = after + 1u;
    return 1;
  }
  if (body->data[after] == '\r') {
    if (after + 1u == body->len) {
      return 0;
    }
    if (body->data[after + 1u] == '\n') {
      *consume_len = after + 2u;
      return 1;
    }
  }
  return 0;
}

static int
lonejson__multipart_boundary_line_prefix(const lonejson__byte_buffer *body,
                                         size_t pos, const char *line,
                                         size_t line_len) {
  size_t available;

  if (pos >= body->len) {
    return 0;
  }
  available = body->len - pos;
  if (available >= line_len) {
    if (memcmp(body->data + pos, line, line_len) != 0) {
      return 0;
    }
    if (available == line_len) {
      return 1;
    }
    return body->data[pos + line_len] == '\r' ||
           body->data[pos + line_len] == '\n';
  }
  return memcmp(body->data + pos, line, available) == 0;
}

static size_t
lonejson__multipart_emit_len_before_boundary(const lonejson__byte_buffer *body,
                                             size_t boundary_pos) {
  size_t emit_len;

  emit_len = boundary_pos;
  if (emit_len > 0u && body->data[emit_len - 1u] == '\n') {
    --emit_len;
    if (emit_len > 0u && body->data[emit_len - 1u] == '\r') {
      --emit_len;
    }
  }
  return emit_len;
}

static lonejson_status lonejson__multipart_finish_boundary(
    lonejson__multipart_state *mp, const lonejson_multipart_handler *handler,
    void *user, int closing, lonejson_error *error) {
  lonejson_status status;

  status = lonejson__multipart_call_end(mp, handler, user, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__multipart_reset_part(mp);
  if (closing) {
    mp->phase = LONEJSON__MULTIPART_DONE;
    mp->closed = 1;
  } else {
    mp->phase = LONEJSON__MULTIPART_HEADERS;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__multipart_process_body_buffer(
    lonejson__multipart_state *mp, const lonejson_multipart_handler *handler,
    void *user, lonejson_error *error) {
  size_t pos;
  size_t consume_len;
  size_t emit_len;
  size_t keep_from;
  lonejson_status status;

  pos = 0u;
  while (pos < mp->body.len) {
    if (pos == 0u || mp->body.data[pos - 1u] == '\n') {
      if (lonejson__multipart_boundary_line_complete(
              &mp->body, pos, mp->closing_boundary_line.data,
              mp->closing_boundary_line.len, &consume_len)) {
        emit_len = lonejson__multipart_emit_len_before_boundary(&mp->body, pos);
        status = lonejson__multipart_call_data(handler, user, mp->body.data,
                                               emit_len, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        lonejson__byte_consume(&mp->body, consume_len);
        return lonejson__multipart_finish_boundary(mp, handler, user, 1, error);
      }
      if (lonejson__multipart_boundary_line_complete(
              &mp->body, pos, mp->boundary_line.data, mp->boundary_line.len,
              &consume_len)) {
        emit_len = lonejson__multipart_emit_len_before_boundary(&mp->body, pos);
        status = lonejson__multipart_call_data(handler, user, mp->body.data,
                                               emit_len, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        lonejson__byte_consume(&mp->body, consume_len);
        return lonejson__multipart_finish_boundary(mp, handler, user, 0, error);
      }
      if (lonejson__multipart_boundary_line_prefix(
              &mp->body, pos, mp->closing_boundary_line.data,
              mp->closing_boundary_line.len) ||
          lonejson__multipart_boundary_line_prefix(
              &mp->body, pos, mp->boundary_line.data, mp->boundary_line.len)) {
        keep_from =
            lonejson__multipart_emit_len_before_boundary(&mp->body, pos);
        if (keep_from > 0u) {
          status = lonejson__multipart_call_data(handler, user, mp->body.data,
                                                 keep_from, error);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
          lonejson__byte_consume(&mp->body, keep_from);
        }
        return LONEJSON_STATUS_OK;
      }
    }
    ++pos;
  }

  keep_from = mp->body.len;
  if (keep_from > 0u && mp->body.data[keep_from - 1u] == '\n') {
    --keep_from;
    if (keep_from > 0u && mp->body.data[keep_from - 1u] == '\r') {
      --keep_from;
    }
  } else if (keep_from > 0u && mp->body.data[keep_from - 1u] == '\r') {
    --keep_from;
  }
  if (keep_from > 0u) {
    status = lonejson__multipart_call_data(handler, user, mp->body.data,
                                           keep_from, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    lonejson__byte_consume(&mp->body, keep_from);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__multipart_feed_body_scan_char(
    lonejson__multipart_state *mp, char ch,
    const lonejson_multipart_handler *handler, void *user,
    lonejson_error *error) {
  lonejson_status status;

  status = lonejson__byte_append_char(&mp->body, ch,
                                      mp->options.max_part_buffered_bytes,
                                      &mp->allocator, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__multipart_process_body_buffer(mp, handler, user, error);
}

lonejson_status
lonejson_multipart_push(lonejson_multipart *receiver, const void *bytes,
                        size_t len, const lonejson_multipart_handler *handler,
                        void *user, lonejson_error *error) {
  lonejson__multipart_state *mp = lonejson__multipart_state_mut(receiver);
  const char *p;
  size_t i;
  size_t chunk;
  lonejson_status status;

  if (mp == NULL || (bytes == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "multipart parser and bytes are required");
  }
  if (mp->closed) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "multipart parser is already finished");
  }
  lonejson__clear_error(error);
  p = (const char *)bytes;
  i = 0u;
  while (i < len) {
    if (mp->phase == LONEJSON__MULTIPART_BODY_LENGTH) {
      chunk = len - i;
      if ((lonejson_uint64)chunk > (lonejson_uint64)mp->remaining) {
        chunk = (size_t)mp->remaining;
      }
      status =
          lonejson__multipart_call_data(handler, user, p + i, chunk, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      i += chunk;
      mp->remaining -= (lonejson_int64)chunk;
      if (mp->remaining == 0) {
        status = lonejson__multipart_call_end(mp, handler, user, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        lonejson__multipart_reset_part(mp);
        mp->phase = LONEJSON__MULTIPART_EXPECT_BOUNDARY;
      }
      continue;
    }
    if (mp->phase == LONEJSON__MULTIPART_BODY_SCAN) {
      status = lonejson__multipart_feed_body_scan_char(mp, p[i++], handler,
                                                       user, error);
    } else {
      status =
          lonejson__multipart_feed_line_char(mp, p[i++], handler, user, error);
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status
lonejson_multipart_finish(lonejson_multipart *receiver,
                          const lonejson_multipart_handler *handler, void *user,
                          lonejson_error *error) {
  lonejson__multipart_state *mp = lonejson__multipart_state_mut(receiver);
  if (mp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "multipart parser is required");
  }
  if (mp->closed || mp->phase == LONEJSON__MULTIPART_DONE) {
    mp->closed = 1;
    return LONEJSON_STATUS_OK;
  }
  (void)handler;
  (void)user;
  return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                             "multipart stream ended before closing boundary");
}

void lonejson_multipart_close(lonejson_multipart *receiver) {
  lonejson__multipart_state *mp = lonejson__multipart_state_mut(receiver);
  if (mp == NULL) {
    return;
  }
  lonejson__multipart_reset_part(mp);
  lonejson__byte_free(&mp->boundary, &mp->allocator);
  lonejson__byte_free(&mp->boundary_line, &mp->allocator);
  lonejson__byte_free(&mp->closing_boundary_line, &mp->allocator);
  lonejson__byte_free(&mp->line, &mp->allocator);
  lonejson__byte_free(&mp->body, &mp->allocator);
  lonejson__buffer_free(&mp->allocator, mp->headers, mp->headers_alloc_size);
  lonejson__buffer_free(&mp->allocator, mp, sizeof(*mp));
}
#endif
