#ifdef LONEJSON_WITH_CURL
static const unsigned char lonejson__curl_state_magic[8] = {'L', 'J', 'C', 'U',
                                                            'R', 'L', '1', '0'};

static int lonejson__curl_state_is_live(const unsigned char state[8]) {
  return memcmp(state, lonejson__curl_state_magic,
                sizeof(lonejson__curl_state_magic)) == 0;
}

static void lonejson__curl_state_mark_live(unsigned char state[8]) {
  memcpy(state, lonejson__curl_state_magic, sizeof(lonejson__curl_state_magic));
}

static size_t lonejson__curl_parse_write_method(lonejson_curl_parse *ctx,
                                                char *ptr, size_t size,
                                                size_t nmemb) {
  return lonejson_curl_write_callback(ptr, size, nmemb, ctx);
}

static lonejson_status
lonejson__curl_parse_finish_method(lonejson_curl_parse *ctx) {
  return lonejson_curl_parse_finish(ctx);
}

static size_t lonejson__curl_array_write_method(lonejson_curl_array_parse *ctx,
                                                char *ptr, size_t size,
                                                size_t nmemb) {
  return lonejson_curl_array_write_callback(ptr, size, nmemb, ctx);
}

static lonejson_status
lonejson__curl_array_finish_method(lonejson_curl_array_parse *ctx) {
  return lonejson_curl_array_parse_finish(ctx);
}

static size_t
lonejson__curl_string_array_write_method(lonejson_curl_string_array_parse *ctx,
                                         char *ptr, size_t size, size_t nmemb) {
  return lonejson_curl_string_array_write_callback(ptr, size, nmemb, ctx);
}

static lonejson_status lonejson__curl_string_array_finish_method(
    lonejson_curl_string_array_parse *ctx) {
  return lonejson_curl_string_array_parse_finish(ctx);
}

static size_t
lonejson__curl_string_items_write_method(lonejson_curl_string_items_parse *ctx,
                                         char *ptr, size_t size, size_t nmemb) {
  return lonejson_curl_string_items_write_callback(ptr, size, nmemb, ctx);
}

static lonejson_status lonejson__curl_string_items_finish_method(
    lonejson_curl_string_items_parse *ctx) {
  return lonejson_curl_string_items_parse_finish(ctx);
}

static size_t lonejson__curl_upload_read_method(lonejson_curl_upload *ctx,
                                                char *ptr, size_t size,
                                                size_t nmemb) {
  return lonejson_curl_read_callback(ptr, size, nmemb, ctx);
}

static curl_off_t
lonejson__curl_upload_size_method(const lonejson_curl_upload *ctx) {
  return lonejson_curl_upload_size(ctx);
}

static int lonejson__curl_upload_seek_method(lonejson_curl_upload *ctx,
                                             curl_off_t offset, int origin) {
  return lonejson_curl_seek_callback(ctx, offset, origin);
}

#ifdef LONEJSON_WITH_OIDC
static size_t
lonejson__oidc_jwks_cache_write_method(lonejson_oidc_jwks_cache_parse *ctx,
                                       char *ptr, size_t size, size_t nmemb) {
  return lonejson_oidc_jwks_cache_write_callback(ptr, size, nmemb, ctx);
}

static lonejson_status
lonejson__oidc_jwks_cache_finish_method(lonejson_oidc_jwks_cache_parse *ctx) {
  return lonejson_oidc_jwks_cache_parse_finish(ctx);
}
#endif

static void lonejson__curl_parse_assign_methods(lonejson_curl_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->write_callback = lonejson__curl_parse_write_method;
  ctx->finish = lonejson__curl_parse_finish_method;
  ctx->cleanup = lonejson_curl_parse_cleanup;
}

static void
lonejson__curl_array_parse_assign_methods(lonejson_curl_array_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->write_callback = lonejson__curl_array_write_method;
  ctx->finish = lonejson__curl_array_finish_method;
  ctx->cleanup = lonejson_curl_array_parse_cleanup;
}

static void lonejson__curl_string_array_parse_assign_methods(
    lonejson_curl_string_array_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->write_callback = lonejson__curl_string_array_write_method;
  ctx->finish = lonejson__curl_string_array_finish_method;
  ctx->cleanup = lonejson_curl_string_array_parse_cleanup;
}

static void lonejson__curl_string_items_parse_assign_methods(
    lonejson_curl_string_items_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->write_callback = lonejson__curl_string_items_write_method;
  ctx->finish = lonejson__curl_string_items_finish_method;
  ctx->cleanup = lonejson_curl_string_items_parse_cleanup;
}

static void lonejson__curl_upload_assign_methods(lonejson_curl_upload *ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->read_callback = lonejson__curl_upload_read_method;
  ctx->size_fn = lonejson__curl_upload_size_method;
  ctx->is_rewindable = lonejson_curl_upload_is_rewindable;
  ctx->rewind = lonejson_curl_upload_rewind;
  ctx->seek_callback = lonejson__curl_upload_seek_method;
  ctx->cleanup = lonejson_curl_upload_cleanup;
}

#ifdef LONEJSON_WITH_OIDC
static void
lonejson__oidc_jwks_cache_assign_methods(lonejson_oidc_jwks_cache_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->write_callback = lonejson__oidc_jwks_cache_write_method;
  ctx->finish = lonejson__oidc_jwks_cache_finish_method;
  ctx->cleanup = lonejson_oidc_jwks_cache_parse_cleanup;
}
#endif

lonejson_status lonejson_curl_parse_init(lonejson_curl_parse *ctx,
                                         lonejson *runtime,
                                         const lonejson_map *map, void *dst) {
  lonejson__runtime_borrow borrow;
  lonejson_runtime *runtime_snapshot;
  const lonejson_runtime *runtime_state;
  lonejson_allocator allocator;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_curl_parse_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_parse_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  runtime_state =
      lonejson__require_runtime_borrow(runtime, &borrow, &ctx->error);
  if (runtime_state == NULL) {
    return ctx->error.code;
  }
  allocator = runtime_state->allocator_storage;
  runtime_snapshot = (lonejson_runtime *)allocator.malloc_fn(
      allocator.ctx, sizeof(*runtime_snapshot));
  if (runtime_snapshot == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                               0u, 0u, 0u,
                               "failed to allocate curl parse runtime");
  }
  memset(runtime_snapshot, 0, sizeof(*runtime_snapshot));
  if (lonejson__runtime_snapshot_init(runtime_snapshot, runtime_state, 1,
                                      &ctx->error) != LONEJSON_STATUS_OK) {
    allocator.free_fn(allocator.ctx, runtime_snapshot);
    lonejson__runtime_borrow_release(&borrow);
    return ctx->error.code;
  }
  ctx->parser =
      lonejson__parser_create_ex(map, dst, &runtime_snapshot->parse_options,
                                 runtime_snapshot, &ctx->error, 0);
  if (ctx->parser == NULL) {
    lonejson__runtime_free_owned_config(runtime_snapshot);
    allocator.free_fn(allocator.ctx, runtime_snapshot);
    lonejson__runtime_borrow_release(&borrow);
    return ctx->error.code;
  }
  ctx->runtime_snapshot = runtime_snapshot;
  lonejson__runtime_borrow_release(&borrow);
  return LONEJSON_STATUS_OK;
}

size_t lonejson_curl_write_callback(char *ptr, size_t size, size_t nmemb,
                                    void *userdata) {
  lonejson_curl_parse *ctx = (lonejson_curl_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;
  if (ctx == NULL || ctx->parser == NULL) {
    return 0u;
  }
  status = lonejson_parser_feed(ctx->parser, ptr, bytes);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    return bytes;
  }
  ctx->error = ((lonejson_parser *)ctx->parser)->error;
  return 0u;
}

lonejson_status lonejson_curl_parse_finish(lonejson_curl_parse *ctx) {
  if (ctx == NULL || ctx->parser == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  ctx->error.code = lonejson_parser_finish(ctx->parser);
  ctx->error = ((lonejson_parser *)ctx->parser)->error;
  return ctx->error.code;
}

void lonejson_curl_parse_cleanup(lonejson_curl_parse *ctx) {
  lonejson_runtime *runtime_snapshot;

  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson_parser_destroy(ctx->parser);
  ctx->parser = NULL;
  runtime_snapshot = (lonejson_runtime *)ctx->runtime_snapshot;
  if (runtime_snapshot != NULL) {
    lonejson__runtime_free_owned_config(runtime_snapshot);
    runtime_snapshot->allocator_storage.free_fn(
        runtime_snapshot->allocator_storage.ctx, runtime_snapshot);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_parse_assign_methods(ctx);
}

lonejson_status lonejson_curl_array_parse_init(
    lonejson_curl_array_parse *ctx, lonejson *runtime, const char *path,
    const lonejson_map *map, void *dst, lonejson_array_stream_item_fn callback,
    void *user) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_curl_array_parse_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_array_parse_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  runtime_state =
      lonejson__require_runtime_borrow(runtime, &borrow, &ctx->error);
  if (runtime_state == NULL) {
    return ctx->error.code;
  }
  if (map == NULL || dst == NULL || callback == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u,
                               "map, destination, and callback are required");
  }
  ctx->map = map;
  ctx->dst = dst;
  ctx->callback = callback;
  ctx->user = user;
  ctx->stream = lonejson__array_stream_open_push_with_options(
      path, &runtime_state->parse_options, runtime_state, &ctx->error);
  lonejson__runtime_borrow_release(&borrow);
  return ctx->stream ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_array_write_callback(char *ptr, size_t size, size_t nmemb,
                                          void *userdata) {
  lonejson_curl_array_parse *ctx = (lonejson_curl_array_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;

  if (ctx == NULL || ctx->stream == NULL) {
    return 0u;
  }
  if (bytes != 0u && ptr == NULL) {
    return 0u;
  }
  status =
      lonejson_array_stream_push(ctx->stream, ctx->map, ctx->dst, ptr, bytes,
                                 ctx->callback, ctx->user, &ctx->error);
  return (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED)
             ? bytes
             : 0u;
}

lonejson_status
lonejson_curl_array_parse_finish(lonejson_curl_array_parse *ctx) {
  if (ctx == NULL || ctx->stream == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson_array_stream_finish(ctx->stream, ctx->map, ctx->dst,
                                      ctx->callback, ctx->user, &ctx->error);
}

void lonejson_curl_array_parse_cleanup(lonejson_curl_array_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson_array_stream_close(ctx->stream);
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_array_parse_assign_methods(ctx);
}

lonejson_status lonejson_curl_string_array_parse_init(
    lonejson_curl_string_array_parse *ctx, lonejson *runtime, const char *path,
    const lonejson_array_stream_string_handler *handler, void *user) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_curl_string_array_parse_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_string_array_parse_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  runtime_state =
      lonejson__require_runtime_borrow(runtime, &borrow, &ctx->error);
  if (runtime_state == NULL) {
    return ctx->error.code;
  }
  if (handler == NULL || handler->chunk == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u, "string chunk handler is required");
  }
  ctx->handler = *handler;
  ctx->user = user;
  ctx->stream = lonejson__array_stream_open_push_with_options(
      path, &runtime_state->parse_options, runtime_state, &ctx->error);
  lonejson__runtime_borrow_release(&borrow);
  return ctx->stream ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_string_array_write_callback(char *ptr, size_t size,
                                                 size_t nmemb, void *userdata) {
  lonejson_curl_string_array_parse *ctx =
      (lonejson_curl_string_array_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;

  if (ctx == NULL || ctx->stream == NULL) {
    return 0u;
  }
  if (bytes != 0u && ptr == NULL) {
    return 0u;
  }
  status = lonejson_array_stream_push_string(
      ctx->stream, ptr, bytes, &ctx->handler, ctx->user, &ctx->error);
  return status == LONEJSON_STATUS_OK ? bytes : 0u;
}

lonejson_status
lonejson_curl_string_array_parse_finish(lonejson_curl_string_array_parse *ctx) {
  if (ctx == NULL || ctx->stream == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson_array_stream_finish_string(ctx->stream, &ctx->handler,
                                             ctx->user, &ctx->error);
}

void lonejson_curl_string_array_parse_cleanup(
    lonejson_curl_string_array_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson_array_stream_close(ctx->stream);
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_string_array_parse_assign_methods(ctx);
}

lonejson_status lonejson_curl_string_items_parse_init(
    lonejson_curl_string_items_parse *ctx, lonejson *runtime, const char *path,
    lonejson_array_stream_string_fn callback, void *user) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_curl_string_items_parse_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_string_items_parse_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  runtime_state =
      lonejson__require_runtime_borrow(runtime, &borrow, &ctx->error);
  if (runtime_state == NULL) {
    return ctx->error.code;
  }
  if (callback == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u, "string item callback is required");
  }
  ctx->callback = callback;
  ctx->user = user;
  ctx->stream = lonejson__array_stream_open_push_with_options(
      path, &runtime_state->parse_options, runtime_state, &ctx->error);
  lonejson__runtime_borrow_release(&borrow);
  return ctx->stream ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_string_items_write_callback(char *ptr, size_t size,
                                                 size_t nmemb, void *userdata) {
  lonejson_curl_string_items_parse *ctx =
      (lonejson_curl_string_items_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;

  if (ctx == NULL || ctx->stream == NULL) {
    return 0u;
  }
  if (bytes != 0u && ptr == NULL) {
    return 0u;
  }
  status = lonejson_array_stream_push_string_items(
      ctx->stream, ptr, bytes, ctx->callback, ctx->user, &ctx->error);
  return status == LONEJSON_STATUS_OK ? bytes : 0u;
}

lonejson_status
lonejson_curl_string_items_parse_finish(lonejson_curl_string_items_parse *ctx) {
  if (ctx == NULL || ctx->stream == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson_array_stream_finish_string_items(ctx->stream, ctx->callback,
                                                   ctx->user, &ctx->error);
}

void lonejson_curl_string_items_parse_cleanup(
    lonejson_curl_string_items_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson_array_stream_close(ctx->stream);
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_string_items_parse_assign_methods(ctx);
}

lonejson_status lonejson_curl_upload_init(lonejson_curl_upload *ctx,
                                          lonejson *runtime,
                                          const lonejson_map *map,
                                          const void *src) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  const lonejson__write_options *options;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_curl_upload_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_upload_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  runtime_state =
      lonejson__require_runtime_borrow(runtime, &borrow, &ctx->generator.error);
  if (runtime_state == NULL) {
    return ctx->generator.error.code;
  }
  options = &runtime_state->write_options;
  if (lonejson__generator_init_with_options(&ctx->generator, map, src,
                                            options) != LONEJSON_STATUS_OK) {
    lonejson__runtime_borrow_release(&borrow);
    return ctx->generator.error.code;
  }
  lonejson__runtime_borrow_release(&borrow);
  return LONEJSON_STATUS_OK;
}

size_t lonejson_curl_read_callback(char *ptr, size_t size, size_t nmemb,
                                   void *userdata) {
  lonejson_curl_upload *ctx = (lonejson_curl_upload *)userdata;
  lonejson_status status;
  size_t out_len;
  int eof;
  size_t capacity = size * nmemb;

  if (ctx == NULL || ptr == NULL) {
    return CURL_READFUNC_ABORT;
  }
  if (ctx->generator.state == NULL) {
    return CURL_READFUNC_ABORT;
  }
  if (capacity == 0u) {
    return 0u;
  }
  status = lonejson_generator_read(&ctx->generator, (unsigned char *)ptr,
                                   capacity, &out_len, &eof);
  if (status != LONEJSON_STATUS_OK) {
    return CURL_READFUNC_ABORT;
  }
  return out_len;
}

int lonejson_curl_upload_is_rewindable(const lonejson_curl_upload *ctx) {
  const lonejson__generator_state *state;

  if (ctx == NULL || !lonejson__curl_state_is_live(ctx->_reserved_state) ||
      ctx->generator.state == NULL) {
    return 0;
  }
  state = (const lonejson__generator_state *)ctx->generator.state;
  return state->magic == LONEJSON__GENERATOR_MAGIC &&
         lonejson__map_is_rewindable_for_serialize(state->map, state->src);
}

lonejson_status lonejson_curl_upload_rewind(lonejson_curl_upload *ctx) {
  lonejson__generator_state *state;
  lonejson_generator replacement;
  lonejson_status status;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (!lonejson_curl_upload_is_rewindable(ctx)) {
    return lonejson__set_error(&ctx->generator.error,
                               LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                               "curl upload is inactive or non-rewindable");
  }
  state = (lonejson__generator_state *)ctx->generator.state;
  status = lonejson__generator_init_with_options(&replacement, state->map,
                                                 state->src, &state->options);
  if (status != LONEJSON_STATUS_OK) {
    ctx->generator.error = replacement.error;
    return status;
  }
  lonejson_generator_cleanup(&ctx->generator);
  ctx->generator = replacement;
  return LONEJSON_STATUS_OK;
}

int lonejson_curl_seek_callback(void *userdata, curl_off_t offset, int origin) {
  lonejson_curl_upload *ctx = (lonejson_curl_upload *)userdata;

  if (ctx == NULL || !lonejson__curl_state_is_live(ctx->_reserved_state) ||
      ctx->generator.state == NULL) {
    return CURL_SEEKFUNC_FAIL;
  }
  if (origin != SEEK_SET || offset != 0 ||
      !lonejson_curl_upload_is_rewindable(ctx)) {
    return CURL_SEEKFUNC_CANTSEEK;
  }
  return lonejson_curl_upload_rewind(ctx) == LONEJSON_STATUS_OK
             ? CURL_SEEKFUNC_OK
             : CURL_SEEKFUNC_FAIL;
}

curl_off_t lonejson_curl_upload_size(const lonejson_curl_upload *ctx) {
  (void)ctx;
  return (curl_off_t)-1;
}

void lonejson_curl_upload_cleanup(lonejson_curl_upload *ctx) {
  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson_generator_cleanup(&ctx->generator);
  memset(ctx, 0, sizeof(*ctx));
  lonejson__curl_upload_assign_methods(ctx);
}

#endif
