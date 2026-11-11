static lonejson_status
lonejson__runtime_lifecycle(lonejson *runtime, const lonejson_map *map,
                            void *value, lonejson_error *error, int operation);

lonejson__parse_options lonejson__default_parse_options(void) {
  lonejson__parse_options options;

  memset(&options, 0, sizeof(options));
  options.clear_destination = 1;
  options.reject_duplicate_keys = 1;
  options.max_depth = 64u;
  options.max_dynamic_string_bytes = LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES;
  options.max_alloc_bytes = LONEJSON_PARSE_MAX_ALLOC_BYTES;
  options.fixed_string_scratch = NULL;
  options.fixed_string_scratch_size = 0u;
  options.allocator = NULL;
  return options;
}

lonejson__value_limits lonejson__default_value_limits(void) {
  lonejson__value_limits limits;

  memset(&limits, 0, sizeof(limits));
  limits.max_depth = 64u;
  limits.max_string_bytes = 1024u * 1024u;
  limits.max_number_bytes = 256u;
  limits.max_key_bytes = 64u * 1024u;
  limits.max_total_bytes = 0u;
  return limits;
}

lonejson__write_options lonejson__default_write_options(void) {
  lonejson__write_options options;

  memset(&options, 0, sizeof(options));
  options.overflow_policy = LONEJSON_OVERFLOW_FAIL;
  options.pretty = 0;
  options.allocator = NULL;
  options.max_output_bytes = LONEJSON_WRITE_MAX_OUTPUT_BYTES;
  return options;
}

static size_t
lonejson__write_max_output_bytes(const lonejson__write_options *options) {
  if (options != NULL && options->max_output_bytes != 0u) {
    return options->max_output_bytes;
  }
  return LONEJSON_WRITE_MAX_OUTPUT_BYTES;
}

lonejson_owned_buffer lonejson_default_owned_buffer(void) {
  lonejson_owned_buffer buffer;

  memset(&buffer, 0, sizeof(buffer));
  buffer.data = NULL;
  buffer.len = 0u;
  buffer.alloc_size = 0u;
  buffer.allocator = lonejson_default_allocator();
  return buffer;
}

#define LONEJSON__CONFIG_COOKIE 0x4C4A4346u

lonejson_config lonejson_default_config(void) {
  lonejson_config config;
  lonejson__parse_options parse_options = lonejson__default_parse_options();
  lonejson__value_limits value_limits = lonejson__default_value_limits();
  lonejson__write_options write_options = lonejson__default_write_options();
  lonejson__spool_options spool_options = lonejson__default_spool_options();

  memset(&config, 0, sizeof(config));
  config.max_alloc_bytes = parse_options.max_alloc_bytes;
  config.max_dynamic_string_bytes = parse_options.max_dynamic_string_bytes;
  config.max_depth = parse_options.max_depth;
  config.json_value_max_total_bytes = value_limits.max_total_bytes;
  config.json_value_max_string_bytes = value_limits.max_string_bytes;
  config.json_value_max_key_bytes = value_limits.max_key_bytes;
  config.json_value_max_number_bytes = value_limits.max_number_bytes;
  config.json_value_max_depth = value_limits.max_depth;
  config.clear_destination_by_default = parse_options.clear_destination;
  config.reject_duplicate_keys_by_default = parse_options.reject_duplicate_keys;
  config.write_overflow_policy = write_options.overflow_policy;
  config.write_pretty = write_options.pretty;
  config.write_max_output_bytes = write_options.max_output_bytes;
  config.spool_default.memory_limit = spool_options.memory_limit;
  config.spool_default.max_bytes = spool_options.max_bytes;
  config.spool_default.temp_dir = spool_options.temp_dir;
  config.spool_blob = config.spool_default;
  config.spool_large_text = config.spool_default;
  config.fixed_string_scratch = NULL;
  config.fixed_string_scratch_size = 0u;
  config.allocator = parse_options.allocator;
#ifdef LONEJSON_WITH_JWT
  config.auth_provider = NULL;
#endif
#ifdef LONEJSON_WITH_OIDC
  config.http_provider = NULL;
#endif
  config._config_cookie = LONEJSON__CONFIG_COOKIE;
  return config;
}

#define LONEJSON__RUNTIME_TOKEN 1u

static void lonejson__runtime_free_owned_config(lonejson_runtime *runtime);
static void lonejson_parser_destroy(lonejson_parser *parser);

#if defined(_WIN32)
static lonejson__lock_word g_lonejson_runtime_handle_lock = 0;
#define LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE()                                \
  lonejson__lock_acquire(&g_lonejson_runtime_handle_lock)
#define LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE()                                \
  lonejson__lock_release(&g_lonejson_runtime_handle_lock)
#define LONEJSON__RUNTIME_HANDLE_WAIT_YIELD() lonejson__lock_wait_yield()
#define LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_INC(handle)                       \
  ((unsigned int)InterlockedIncrement((volatile LONG *)&(handle)->active_pins))
#define LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle)                       \
  ((unsigned int)InterlockedDecrement((volatile LONG *)&(handle)->active_pins))
#define LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_LOAD(handle)                      \
  ((unsigned int)InterlockedCompareExchange(                                   \
      (volatile LONG *)&(handle)->active_pins, 0L, 0L))
#define LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle)                          \
  ((int)InterlockedCompareExchange((volatile LONG *)&(handle)->closing, 0L, 0L))
#define LONEJSON__RUNTIME_HANDLE_CLOSING_STORE(handle, value)                  \
  do {                                                                         \
    (void)InterlockedExchange((volatile LONG *)&(handle)->closing,             \
                              (LONG)(value));                                  \
  } while (0)
#elif defined(__GNUC__) || defined(__clang__)
static lonejson__lock_word g_lonejson_runtime_handle_lock = 0;
#define LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE()                                \
  lonejson__lock_acquire(&g_lonejson_runtime_handle_lock)
#define LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE()                                \
  lonejson__lock_release(&g_lonejson_runtime_handle_lock)
#define LONEJSON__RUNTIME_HANDLE_WAIT_YIELD() lonejson__lock_wait_yield()
#define LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_INC(handle)                       \
  ((unsigned int)__atomic_add_fetch(&(handle)->active_pins, 1u,                \
                                    __ATOMIC_SEQ_CST))
#define LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle)                       \
  ((unsigned int)__atomic_sub_fetch(&(handle)->active_pins, 1u,                \
                                    __ATOMIC_SEQ_CST))
#define LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_LOAD(handle)                      \
  ((unsigned int)__atomic_load_n(&(handle)->active_pins, __ATOMIC_SEQ_CST))
#define LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle)                          \
  ((int)__atomic_load_n(&(handle)->closing, __ATOMIC_ACQUIRE))
#define LONEJSON__RUNTIME_HANDLE_CLOSING_STORE(handle, value)                  \
  __atomic_store_n(&(handle)->closing, (value), __ATOMIC_RELEASE)
#else
#error                                                                         \
    "lonejson runtime copied-handle safety requires a synchronization primitive"
#endif

static lonejson__runtime_handle *g_lonejson_runtime_handle_live_list = NULL;
static unsigned int g_lonejson_runtime_handle_generation = 1u;

#if defined(LONEJSON_TEST_RUNTIME_BORROW_HOOK)
typedef void (*lonejson__runtime_borrow_hook_fn)(void *user);
static lonejson__runtime_borrow_hook_fn g_lonejson_runtime_borrow_hook = NULL;
static void *g_lonejson_runtime_borrow_hook_user = NULL;
static void
lonejson__test_set_runtime_borrow_hook(lonejson__runtime_borrow_hook_fn hook,
                                       void *user) {
  g_lonejson_runtime_borrow_hook = hook;
  g_lonejson_runtime_borrow_hook_user = user;
}
#endif

static unsigned int lonejson__runtime_handle_next_generation(void) {
  unsigned int generation;

  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  generation = ++g_lonejson_runtime_handle_generation;
  if (generation == 0u) {
    generation = ++g_lonejson_runtime_handle_generation;
  }
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
  return generation;
}

static void lonejson__runtime_handle_register(lonejson__runtime_handle *handle,
                                              lonejson_runtime *runtime_state,
                                              lonejson *api_owner) {
  if (handle == NULL) {
    return;
  }
  memset(handle, 0, sizeof(*handle));
  handle->runtime_state = runtime_state;
  handle->api_owner = api_owner;
  handle->generation = lonejson__runtime_handle_next_generation();
  handle->active_pins = 0u;
  handle->closing = 0;
  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  handle->next_live = g_lonejson_runtime_handle_live_list;
  g_lonejson_runtime_handle_live_list = handle;
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
}

static const lonejson_runtime *
lonejson__runtime_handle_lookup_const(const lonejson *runtime) {
  const lonejson_runtime *runtime_state = NULL;
  const lonejson__runtime_handle *cursor;
  const lonejson_runtime *state_ptr;

  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return NULL;
  }
  state_ptr = (const lonejson_runtime *)runtime->state;
  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  cursor = g_lonejson_runtime_handle_live_list;
  while (cursor != NULL) {
    if (cursor->generation == runtime->_state_token &&
        cursor->runtime_state == state_ptr && !cursor->closing) {
      runtime_state = cursor->runtime_state;
      break;
    }
    cursor = cursor->next_live;
  }
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
  return runtime_state;
}

static lonejson_runtime *
lonejson__runtime_handle_lookup_mut(lonejson *runtime) {
  lonejson_runtime *runtime_state = NULL;
  lonejson__runtime_handle *cursor;
  lonejson_runtime *state_ptr;

  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return NULL;
  }
  state_ptr = (lonejson_runtime *)runtime->state;
  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  cursor = g_lonejson_runtime_handle_live_list;
  while (cursor != NULL) {
    if (cursor->generation == runtime->_state_token &&
        cursor->runtime_state == state_ptr && !cursor->closing) {
      runtime_state = cursor->runtime_state;
      break;
    }
    cursor = cursor->next_live;
  }
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
  return runtime_state;
}

static const lonejson_runtime *
lonejson__runtime_borrow_const(const lonejson *runtime,
                               lonejson__runtime_borrow *borrow) {
  const lonejson_runtime *runtime_state = NULL;
  lonejson__runtime_handle *cursor;
  const lonejson_runtime *state_ptr;

  if (borrow != NULL) {
    memset(borrow, 0, sizeof(*borrow));
  }
  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return NULL;
  }
  if (runtime->_runtime_owner_self == runtime &&
      runtime->_runtime_owner_data != NULL) {
    lonejson *owner = runtime->_runtime_owner_self;
    lonejson__runtime_handle *handle =
        &((lonejson__runtime_bundle *)owner)->live_handle;
    (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_INC(handle);
    if (handle->generation == runtime->_state_token &&
        !LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle)) {
      runtime_state = (const lonejson_runtime *)runtime->_runtime_owner_data;
      if (runtime_state != NULL && borrow != NULL) {
        borrow->runtime = runtime_state;
        borrow->handle = handle;
      } else if (runtime_state == NULL) {
        (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle);
        runtime_state = NULL;
      }
    } else {
      (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle);
    }
#if defined(LONEJSON_TEST_RUNTIME_BORROW_HOOK)
    if (borrow != NULL && borrow->handle != NULL &&
        g_lonejson_runtime_borrow_hook != NULL) {
      g_lonejson_runtime_borrow_hook(g_lonejson_runtime_borrow_hook_user);
    }
#endif
    if (runtime_state != NULL) {
      return runtime_state;
    }
  }
  state_ptr = (const lonejson_runtime *)runtime->state;
  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  cursor = g_lonejson_runtime_handle_live_list;
  while (cursor != NULL) {
    if (cursor->generation == runtime->_state_token &&
        cursor->runtime_state == state_ptr &&
        !LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(cursor)) {
      (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_INC(cursor);
      runtime_state = cursor->runtime_state;
      if (borrow != NULL) {
        borrow->runtime = runtime_state;
        borrow->handle = cursor;
      }
      break;
    }
    cursor = cursor->next_live;
  }
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
#if defined(LONEJSON_TEST_RUNTIME_BORROW_HOOK)
  if (runtime_state != NULL && borrow != NULL && borrow->handle != NULL &&
      g_lonejson_runtime_borrow_hook != NULL) {
    g_lonejson_runtime_borrow_hook(g_lonejson_runtime_borrow_hook_user);
  }
#endif
  return runtime_state;
}

static const lonejson_runtime *
lonejson__runtime_owner_pin_const_nohook(const lonejson *runtime,
                                         lonejson__runtime_borrow *borrow) {
  lonejson__runtime_handle *handle;
  const lonejson_runtime *runtime_state;

  if (borrow != NULL) {
    memset(borrow, 0, sizeof(*borrow));
  }
  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u || runtime->_runtime_owner_self != runtime ||
      runtime->_runtime_owner_data == NULL) {
    return NULL;
  }

  handle =
      &((lonejson__runtime_bundle *)runtime->_runtime_owner_self)->live_handle;
  (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_INC(handle);
  if (handle->generation != runtime->_state_token ||
      LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle)) {
    (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle);
    return NULL;
  }
  runtime_state = (const lonejson_runtime *)runtime->_runtime_owner_data;
  if (handle->generation != runtime->_state_token ||
      LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle) || runtime_state == NULL) {
    (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle);
    return NULL;
  }
#if defined(LONEJSON_TEST_RUNTIME_BORROW_HOOK)
  if (borrow != NULL && g_lonejson_runtime_borrow_hook != NULL) {
    g_lonejson_runtime_borrow_hook(g_lonejson_runtime_borrow_hook_user);
  }
#endif
  if (borrow != NULL) {
    borrow->runtime = runtime_state;
    borrow->handle = handle;
  }
  return runtime_state;
}

static void lonejson__runtime_borrow_release(lonejson__runtime_borrow *borrow) {
  if (borrow == NULL || borrow->handle == NULL) {
    return;
  }
  (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(borrow->handle);
  borrow->runtime = NULL;
  borrow->handle = NULL;
}

static const lonejson_runtime *
lonejson__require_runtime_borrow(lonejson *runtime,
                                 lonejson__runtime_borrow *borrow,
                                 lonejson_error *error) {
  const lonejson_runtime *runtime_state;

  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, borrow);
  if (LONEJSON__UNLIKELY(runtime_state == NULL)) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "lonejson runtime is required");
    return NULL;
  }
  return runtime_state;
}

static lonejson_status lonejson__runtime_snapshot_write_policy(
    lonejson *runtime, lonejson__write_options *options,
    lonejson_allocator *allocator_storage, lonejson_error *error) {
  const lonejson_runtime *runtime_state = NULL;
  const lonejson__runtime_handle *cursor;
  const lonejson_runtime *state_ptr;

  if (options == NULL || allocator_storage == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "write runtime policy snapshot is required");
  }
  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "lonejson runtime is required");
  }

  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  if (runtime->_runtime_owner_self == runtime &&
      runtime->_runtime_owner_data != NULL) {
    lonejson__runtime_handle *handle =
        &((lonejson__runtime_bundle *)runtime)->live_handle;
    if (handle->generation == runtime->_state_token &&
        !LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle)) {
      runtime_state = (const lonejson_runtime *)runtime->_runtime_owner_data;
    }
  } else {
    state_ptr = (const lonejson_runtime *)runtime->state;
    cursor = g_lonejson_runtime_handle_live_list;
    while (cursor != NULL) {
      if (cursor->generation == runtime->_state_token &&
          cursor->runtime_state == state_ptr &&
          !LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(cursor)) {
        runtime_state = cursor->runtime_state;
        break;
      }
      cursor = cursor->next_live;
    }
  }
  if (runtime_state != NULL) {
    *options = runtime_state->write_options;
    if (options->allocator != NULL) {
      *allocator_storage = runtime_state->allocator_storage;
      options->allocator = allocator_storage;
    }
  }
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
  if (runtime_state == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "lonejson runtime is required");
  }
#if defined(LONEJSON_TEST_RUNTIME_BORROW_HOOK)
  if (g_lonejson_runtime_borrow_hook != NULL) {
    g_lonejson_runtime_borrow_hook(g_lonejson_runtime_borrow_hook_user);
  }
#endif
  return LONEJSON_STATUS_OK;
}

#if defined(LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN)
static void lonejson__runtime_handle_cache_drain(void) {
  /* Live-handle registry is embedded in runtime bundles; nothing to drain. */
}
#endif

static LONEJSON__INLINE const lonejson_runtime *
lonejson__runtime_const(const lonejson *runtime) {
  return lonejson__runtime_data_const(runtime);
}

static LONEJSON__INLINE lonejson_runtime *
lonejson__runtime_mut(lonejson *runtime) {
  return lonejson__runtime_data_mut(runtime);
}

static LONEJSON__INLINE const lonejson_runtime *
lonejson__require_runtime_const(lonejson *runtime, lonejson_error *error) {
  const lonejson_runtime *runtime_state;

  runtime_state = lonejson__runtime_const(runtime);
  if (LONEJSON__UNLIKELY(runtime_state == NULL)) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "lonejson runtime is required");
    return NULL;
  }
  return runtime_state;
}

static LONEJSON__INLINE lonejson_status
lonejson__require_runtime(lonejson *runtime, lonejson_error *error) {
  return lonejson__require_runtime_const(runtime, error) != NULL
             ? LONEJSON_STATUS_OK
             : LONEJSON_STATUS_INVALID_ARGUMENT;
}

static char *lonejson__runtime_dup_string(const lonejson_allocator *allocator,
                                          const char *src) {
  size_t len;
  char *copy;

  if (src == NULL) {
    return NULL;
  }
  len = strlen(src);
  copy = (char *)lonejson__owned_malloc(allocator, len + 1u);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, src, len + 1u);
  return copy;
}

static void lonejson__runtime_free_owned_config(lonejson_runtime *runtime) {
  size_t i;

  if (runtime == NULL) {
    return;
  }
  for (i = 0; i < 3u; ++i) {
    lonejson__owned_free(runtime->owned_temp_dirs[i]);
    runtime->owned_temp_dirs[i] = NULL;
  }
  lonejson__owned_free(runtime->owned_fixed_string_scratch);
  runtime->owned_fixed_string_scratch = NULL;
}

static void lonejson__runtime_apply_config(lonejson_runtime *runtime);

static lonejson_status lonejson__runtime_snapshot_init(
    lonejson_runtime *snapshot, const lonejson_runtime *runtime,
    int include_parse_scratch, lonejson_error *error) {
  size_t i;

  if (snapshot == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "runtime snapshot is required");
  }
  snapshot->api_owner = NULL;
  snapshot->owned_temp_dirs[LONEJSON_SPOOL_CLASS_DEFAULT] = NULL;
  snapshot->owned_temp_dirs[LONEJSON_SPOOL_CLASS_BLOB] = NULL;
  snapshot->owned_temp_dirs[LONEJSON_SPOOL_CLASS_LARGE_TEXT] = NULL;
  snapshot->owned_fixed_string_scratch = NULL;
  if (runtime == NULL) {
    return LONEJSON_STATUS_OK;
  }

  snapshot->allocator_storage = runtime->allocator_storage;
#ifdef LONEJSON_WITH_JWT
  snapshot->auth_provider = runtime->auth_provider;
  snapshot->has_auth_provider = runtime->has_auth_provider;
#endif
#ifdef LONEJSON_WITH_OIDC
  snapshot->http_provider = runtime->http_provider;
  snapshot->has_http_provider = runtime->has_http_provider;
#endif
  snapshot->config.allocator = &snapshot->allocator_storage;
  snapshot->parse_options = runtime->parse_options;
  snapshot->value_limits = runtime->value_limits;
  snapshot->write_options = runtime->write_options;
  for (i = 0u; i < 3u; ++i) {
    snapshot->spool_options[i] = runtime->spool_options[i];
  }
  if ((!include_parse_scratch || runtime->config.fixed_string_scratch == NULL ||
       runtime->config.fixed_string_scratch_size == 0u) &&
      runtime->owned_temp_dirs[LONEJSON_SPOOL_CLASS_DEFAULT] == NULL &&
      runtime->owned_temp_dirs[LONEJSON_SPOOL_CLASS_BLOB] == NULL &&
      runtime->owned_temp_dirs[LONEJSON_SPOOL_CLASS_LARGE_TEXT] == NULL) {
    snapshot->config.fixed_string_scratch = NULL;
    snapshot->config.fixed_string_scratch_size = 0u;
    snapshot->config.spool_default.temp_dir = NULL;
    snapshot->config.spool_blob.temp_dir = NULL;
    snapshot->config.spool_large_text.temp_dir = NULL;
    snapshot->parse_options.allocator = snapshot->config.allocator;
    snapshot->parse_options.fixed_string_scratch = NULL;
    snapshot->parse_options.fixed_string_scratch_size = 0u;
    snapshot->write_options.allocator = snapshot->config.allocator;
    snapshot->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT].temp_dir = NULL;
    snapshot->spool_options[LONEJSON_SPOOL_CLASS_BLOB].temp_dir = NULL;
    snapshot->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT].temp_dir = NULL;
    return LONEJSON_STATUS_OK;
  }

  snapshot->config = runtime->config;
  snapshot->config.allocator = &snapshot->allocator_storage;
  if (include_parse_scratch && runtime->config.fixed_string_scratch != NULL &&
      runtime->config.fixed_string_scratch_size != 0u) {
    snapshot->owned_fixed_string_scratch =
        lonejson__owned_malloc(&snapshot->allocator_storage,
                               runtime->config.fixed_string_scratch_size);
    if (snapshot->owned_fixed_string_scratch == NULL) {
      lonejson__runtime_free_owned_config(snapshot);
      return lonejson__set_error(
          error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
          "failed to allocate runtime fixed string scratch");
    }
    memcpy(snapshot->owned_fixed_string_scratch,
           runtime->config.fixed_string_scratch,
           runtime->config.fixed_string_scratch_size);
    snapshot->config.fixed_string_scratch =
        snapshot->owned_fixed_string_scratch;
  } else {
    snapshot->config.fixed_string_scratch = NULL;
    snapshot->config.fixed_string_scratch_size = 0u;
  }
  if (runtime->owned_temp_dirs[LONEJSON_SPOOL_CLASS_DEFAULT] != NULL ||
      runtime->owned_temp_dirs[LONEJSON_SPOOL_CLASS_BLOB] != NULL ||
      runtime->owned_temp_dirs[LONEJSON_SPOOL_CLASS_LARGE_TEXT] != NULL) {
    for (i = 0u; i < 3u; ++i) {
      snapshot->owned_temp_dirs[i] = lonejson__runtime_dup_string(
          &snapshot->allocator_storage, runtime->owned_temp_dirs[i]);
      if (runtime->owned_temp_dirs[i] != NULL &&
          snapshot->owned_temp_dirs[i] == NULL) {
        lonejson__runtime_free_owned_config(snapshot);
        return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                   0u, 0u,
                                   "failed to allocate runtime spool temp_dir");
      }
    }
  }
  snapshot->config.spool_default.temp_dir =
      snapshot->owned_temp_dirs[LONEJSON_SPOOL_CLASS_DEFAULT];
  snapshot->config.spool_blob.temp_dir =
      snapshot->owned_temp_dirs[LONEJSON_SPOOL_CLASS_BLOB];
  snapshot->config.spool_large_text.temp_dir =
      snapshot->owned_temp_dirs[LONEJSON_SPOOL_CLASS_LARGE_TEXT];
  snapshot->parse_options.allocator = snapshot->config.allocator;
  snapshot->parse_options.fixed_string_scratch =
      snapshot->config.fixed_string_scratch;
  snapshot->parse_options.fixed_string_scratch_size =
      snapshot->config.fixed_string_scratch_size;
  snapshot->write_options.allocator = snapshot->config.allocator;
  snapshot->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT].temp_dir =
      snapshot->config.spool_default.temp_dir;
  snapshot->spool_options[LONEJSON_SPOOL_CLASS_BLOB].temp_dir =
      snapshot->config.spool_blob.temp_dir;
  snapshot->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT].temp_dir =
      snapshot->config.spool_large_text.temp_dir;
  return LONEJSON_STATUS_OK;
}

static void lonejson__runtime_apply_config(lonejson_runtime *runtime) {
  runtime->parse_options = lonejson__default_parse_options();
  runtime->value_limits = lonejson__default_value_limits();
  runtime->write_options = lonejson__default_write_options();
  runtime->spool_options[0] = lonejson__default_spool_options();
  runtime->spool_options[1] = runtime->spool_options[0];
  runtime->spool_options[2] = runtime->spool_options[0];

  runtime->parse_options.clear_destination =
      runtime->config.clear_destination_by_default;
  runtime->parse_options.reject_duplicate_keys =
      runtime->config.reject_duplicate_keys_by_default;
  runtime->parse_options.max_depth = runtime->config.max_depth;
  runtime->parse_options.max_dynamic_string_bytes =
      runtime->config.max_dynamic_string_bytes;
  runtime->parse_options.max_alloc_bytes = runtime->config.max_alloc_bytes;
  runtime->parse_options.fixed_string_scratch =
      runtime->config.fixed_string_scratch;
  runtime->parse_options.fixed_string_scratch_size =
      runtime->config.fixed_string_scratch_size;
  runtime->parse_options.allocator = runtime->config.allocator;

  runtime->value_limits.max_depth = runtime->config.json_value_max_depth;
  runtime->value_limits.max_string_bytes =
      runtime->config.json_value_max_string_bytes;
  runtime->value_limits.max_number_bytes =
      runtime->config.json_value_max_number_bytes;
  runtime->value_limits.max_key_bytes =
      runtime->config.json_value_max_key_bytes;
  runtime->value_limits.max_total_bytes =
      runtime->config.json_value_max_total_bytes;

  runtime->write_options.overflow_policy =
      runtime->config.write_overflow_policy;
  runtime->write_options.pretty = runtime->config.write_pretty;
  runtime->write_options.max_output_bytes =
      runtime->config.write_max_output_bytes;
  runtime->write_options.allocator = runtime->config.allocator;

  runtime->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT].memory_limit =
      runtime->config.spool_default.memory_limit;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT].max_bytes =
      runtime->config.spool_default.max_bytes;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT].temp_dir =
      runtime->config.spool_default.temp_dir;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_BLOB].memory_limit =
      runtime->config.spool_blob.memory_limit;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_BLOB].max_bytes =
      runtime->config.spool_blob.max_bytes;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_BLOB].temp_dir =
      runtime->config.spool_blob.temp_dir;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT].memory_limit =
      runtime->config.spool_large_text.memory_limit;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT].max_bytes =
      runtime->config.spool_large_text.max_bytes;
  runtime->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT].temp_dir =
      runtime->config.spool_large_text.temp_dir;
#ifdef LONEJSON_WITH_JWT
  runtime->has_auth_provider = runtime->config.auth_provider != NULL;
  if (runtime->has_auth_provider) {
    runtime->auth_provider = *runtime->config.auth_provider;
  } else {
    memset(&runtime->auth_provider, 0, sizeof(runtime->auth_provider));
  }
#endif
#ifdef LONEJSON_WITH_OIDC
  runtime->has_http_provider = runtime->config.http_provider != NULL;
  if (runtime->has_http_provider) {
    runtime->http_provider = *runtime->config.http_provider;
  } else {
    memset(&runtime->http_provider, 0, sizeof(runtime->http_provider));
  }
#endif
}

static lonejson_status
lonejson__runtime_parse_buffer(lonejson *runtime, const lonejson_map *map,
                               void *dst, const void *data, size_t len,
                               lonejson_error *error);
static lonejson_status lonejson__runtime_parse_cstr(lonejson *runtime,
                                                    const lonejson_map *map,
                                                    void *dst, const char *json,
                                                    lonejson_error *error);
static lonejson_status
lonejson__runtime_parse_reader(lonejson *runtime, const lonejson_map *map,
                               void *dst, lonejson_reader_fn reader, void *user,
                               lonejson_error *error);
static lonejson_status lonejson__runtime_parse_filep(lonejson *runtime,
                                                     const lonejson_map *map,
                                                     void *dst, FILE *fp,
                                                     lonejson_error *error);
static lonejson_status lonejson__runtime_parse_path(lonejson *runtime,
                                                    const lonejson_map *map,
                                                    void *dst, const char *path,
                                                    lonejson_error *error);
static lonejson_status lonejson__runtime_validate_buffer(lonejson *runtime,
                                                         const void *data,
                                                         size_t len,
                                                         lonejson_error *error);
static lonejson_status lonejson__runtime_validate_cstr(lonejson *runtime,
                                                       const char *json,
                                                       lonejson_error *error);
static lonejson_status
lonejson__runtime_validate_reader(lonejson *runtime, lonejson_reader_fn reader,
                                  void *user, lonejson_error *error);
static lonejson_status lonejson__runtime_validate_filep(lonejson *runtime,
                                                        FILE *fp,
                                                        lonejson_error *error);
static lonejson_status lonejson__runtime_validate_path(lonejson *runtime,
                                                       const char *path,
                                                       lonejson_error *error);
static lonejson_status lonejson__runtime_visit_value_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lonejson_value_visitor *visitor, void *user, lonejson_error *error);
static lonejson_status
lonejson__runtime_visit_value_cstr(lonejson *runtime, const char *json,
                                   const lonejson_value_visitor *visitor,
                                   void *user, lonejson_error *error);
static lonejson_status lonejson__runtime_visit_value_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_value_visitor *visitor, void *user, lonejson_error *error);
static lonejson_status
lonejson__runtime_visit_value_filep(lonejson *runtime, FILE *fp,
                                    const lonejson_value_visitor *visitor,
                                    void *user, lonejson_error *error);
static lonejson_status
lonejson__runtime_visit_value_path(lonejson *runtime, const char *path,
                                   const lonejson_value_visitor *visitor,
                                   void *user, lonejson_error *error);
static lonejson_status
lonejson__runtime_visit_value_fd(lonejson *runtime, int fd,
                                 const lonejson_value_visitor *visitor,
                                 void *user, lonejson_error *error);
static lonejson_status lonejson__runtime_visit_path_value_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error);
static lonejson_status lonejson__runtime_visit_path_value_cstr(
    lonejson *runtime, const char *json,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error);
static lonejson_status lonejson__runtime_visit_path_value_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error);
static lonejson_status lonejson__runtime_visit_path_value_filep(
    lonejson *runtime, FILE *fp, const lonejson_path_value_visitor *visitor,
    void *user, lonejson_error *error);
static lonejson_status lonejson__runtime_visit_path_value_path(
    lonejson *runtime, const char *path,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error);
static lonejson_status lonejson__runtime_visit_path_value_fd(
    lonejson *runtime, int fd, const lonejson_path_value_visitor *visitor,
    void *user, lonejson_error *error);
static lonejson_status lonejson__runtime_init_value(lonejson *runtime,
                                                    const lonejson_map *map,
                                                    void *value,
                                                    lonejson_error *error);
static lonejson_status lonejson__runtime_reset_value(lonejson *runtime,
                                                     const lonejson_map *map,
                                                     void *value,
                                                     lonejson_error *error);
static lonejson_stream *
lonejson__runtime_stream_open_reader(lonejson *runtime, const lonejson_map *map,
                                     lonejson_reader_fn reader, void *user,
                                     lonejson_error *error);
static lonejson_stream *
lonejson__runtime_stream_open_filep(lonejson *runtime, const lonejson_map *map,
                                    FILE *fp, lonejson_error *error);
static lonejson_stream *
lonejson__runtime_stream_open_path(lonejson *runtime, const lonejson_map *map,
                                   const char *path, lonejson_error *error);
static lonejson_stream *
lonejson__runtime_stream_open_fd(lonejson *runtime, const lonejson_map *map,
                                 int fd, lonejson_error *error);
static lonejson_array_stream *
lonejson__runtime_array_stream_open_reader(lonejson *runtime, const char *path,
                                           lonejson_reader_fn reader,
                                           void *user, lonejson_error *error);
static lonejson_array_stream *
lonejson__runtime_array_stream_open_filep(lonejson *runtime, const char *path,
                                          FILE *fp, lonejson_error *error);
static lonejson_array_stream *lonejson__runtime_array_stream_open_path(
    lonejson *runtime, const char *array_path, const char *path,
    lonejson_error *error);
static lonejson_array_stream *
lonejson__runtime_array_stream_open_fd(lonejson *runtime, const char *path,
                                       int fd, lonejson_error *error);
static lonejson_array_stream *
lonejson__runtime_array_stream_open_push(lonejson *runtime, const char *path,
                                         lonejson_error *error);
static void lonejson__runtime_spooled_init(lonejson *runtime,
                                           lonejson_spooled *value);
static void
lonejson__runtime_spooled_init_class(lonejson *runtime, lonejson_spooled *value,
                                     lonejson_spool_class spool_class);
static void lonejson__runtime_json_value_init(lonejson *runtime,
                                              lonejson_json_value *value);
static void lonejson__runtime_json_value_cleanup(lonejson *runtime,
                                                 lonejson_json_value *value);
static lonejson_status lonejson__runtime_array_rewrite_reader(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_reader_to_filep(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, FILE *output,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_reader_to_fd(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, int fd,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_filep(
    lonejson *runtime, const char *selector, FILE *fp, lonejson_sink_fn sink,
    void *sink_user, const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_filep_to_filep(
    lonejson *runtime, const char *selector, FILE *input, FILE *output,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_fd(
    lonejson *runtime, const char *selector, int fd, lonejson_sink_fn sink,
    void *sink_user, const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_fd_to_fd(
    lonejson *runtime, const char *selector, int input_fd, int output_fd,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_array_rewrite_path(
    lonejson *runtime, const char *selector, const char *input_path,
    const char *output_path,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_buffer(
    lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
    void *sink_user, const struct lonejson_value_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_filep(
    lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_fd(
    lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_path(
    lonejson *runtime, const char *input_path, const char *output_path,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_selector_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_selector_buffer(
    lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
    void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_selector_filep(
    lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_selector_fd(
    lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
static lonejson_status lonejson__runtime_value_rewrite_selector_path(
    lonejson *runtime, const char *input_path, const char *output_path,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
static lonejson_status
lonejson__runtime_generator_init(lonejson *runtime,
                                 lonejson_generator *generator,
                                 const lonejson_map *map, const void *src);
static lonejson_status
lonejson__runtime_writer_init_sink(lonejson *runtime, lonejson_writer *writer,
                                   lonejson_sink_fn sink, void *sink_user,
                                   lonejson_error *error);
static lonejson_status lonejson__runtime_cleanup_value(lonejson *runtime,
                                                       const lonejson_map *map,
                                                       void *value,
                                                       lonejson_error *error);
#ifdef LONEJSON_WITH_JWT
static lonejson_status
lonejson__runtime_set_auth_provider(lonejson *runtime,
                                    const lonejson_auth_provider *provider,
                                    lonejson_error *error);
#endif
#ifdef LONEJSON_WITH_OIDC
static lonejson_status
lonejson__runtime_set_http_provider(lonejson *runtime,
                                    const lonejson_http_provider *provider,
                                    lonejson_error *error);
#endif
#ifdef LONEJSON_WITH_CURL
static lonejson_status
lonejson__runtime_curl_parse_init(lonejson *runtime, lonejson_curl_parse *ctx,
                                  const lonejson_map *map, void *dst);
static lonejson_status lonejson__runtime_curl_array_parse_init(
    lonejson *runtime, lonejson_curl_array_parse *ctx, const char *path,
    const lonejson_map *map, void *dst, lonejson_array_stream_item_fn callback,
    void *user);
static lonejson_status lonejson__runtime_curl_string_array_parse_init(
    lonejson *runtime, lonejson_curl_string_array_parse *ctx, const char *path,
    const lonejson_array_stream_string_handler *handler, void *user);
static lonejson_status lonejson__runtime_curl_string_items_parse_init(
    lonejson *runtime, lonejson_curl_string_items_parse *ctx, const char *path,
    lonejson_array_stream_string_fn callback, void *user);
static lonejson_status
lonejson__runtime_curl_upload_init(lonejson *runtime, lonejson_curl_upload *ctx,
                                   const lonejson_map *map, const void *src);
#ifdef LONEJSON_WITH_OIDC
static lonejson_status lonejson__runtime_oidc_jwks_cache_parse_init(
    lonejson *runtime, lonejson_oidc_jwks_cache_parse *ctx,
    lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy);
#endif
#endif

lonejson *lonejson_new(const lonejson_config *config, lonejson_error *error) {
  lonejson_config resolved = lonejson_default_config();
  lonejson_allocator allocator_storage = resolved.allocator != NULL
                                             ? *resolved.allocator
                                             : lonejson_default_allocator();
  const lonejson_allocator *allocator = resolved.allocator;
  size_t alloc_size = sizeof(lonejson__runtime_bundle);
  lonejson *runtime;
  lonejson_runtime *runtime_state;
  char *spool_default_temp_dir = NULL;
  char *spool_blob_temp_dir = NULL;
  char *spool_large_text_temp_dir = NULL;

  if (config != NULL) {
    if (config->_config_cookie != LONEJSON__CONFIG_COOKIE) {
      lonejson__set_error(
          error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
          "lonejson_config must be initialized with lonejson_default_config()");
      return NULL;
    }
    resolved = *config;
    resolved._config_cookie = LONEJSON__CONFIG_COOKIE;
    allocator_storage = resolved.allocator != NULL
                            ? *resolved.allocator
                            : lonejson_default_allocator();
    allocator = resolved.allocator;
  }

  if (allocator != NULL && !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  runtime = (lonejson *)lonejson__buffer_alloc(allocator, alloc_size);
  if (runtime == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate lonejson runtime");
    return NULL;
  }
  memset(runtime, 0, alloc_size);
  runtime_state = &((lonejson__runtime_bundle *)runtime)->state;
  runtime_state->api_owner = runtime;
  runtime_state->allocator_storage = allocator_storage;
  runtime_state->config = resolved;
  runtime_state->config.allocator =
      resolved.allocator != NULL ? &runtime_state->allocator_storage : NULL;
  spool_default_temp_dir = lonejson__runtime_dup_string(
      &runtime_state->allocator_storage, resolved.spool_default.temp_dir);
  if (resolved.spool_default.temp_dir != NULL &&
      spool_default_temp_dir == NULL) {
    lonejson__buffer_free(allocator, runtime, sizeof(lonejson__runtime_bundle));
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate runtime spool temp_dir");
    return NULL;
  }
  spool_blob_temp_dir = lonejson__runtime_dup_string(
      &runtime_state->allocator_storage, resolved.spool_blob.temp_dir);
  if (resolved.spool_blob.temp_dir != NULL && spool_blob_temp_dir == NULL) {
    lonejson__owned_free(spool_default_temp_dir);
    lonejson__buffer_free(allocator, runtime, sizeof(lonejson__runtime_bundle));
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate runtime spool temp_dir");
    return NULL;
  }
  spool_large_text_temp_dir = lonejson__runtime_dup_string(
      &runtime_state->allocator_storage, resolved.spool_large_text.temp_dir);
  if (resolved.spool_large_text.temp_dir != NULL &&
      spool_large_text_temp_dir == NULL) {
    lonejson__owned_free(spool_blob_temp_dir);
    lonejson__owned_free(spool_default_temp_dir);
    lonejson__buffer_free(allocator, runtime, sizeof(lonejson__runtime_bundle));
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate runtime spool temp_dir");
    return NULL;
  }
  runtime_state->owned_temp_dirs[LONEJSON_SPOOL_CLASS_DEFAULT] =
      spool_default_temp_dir;
  runtime_state->owned_temp_dirs[LONEJSON_SPOOL_CLASS_BLOB] =
      spool_blob_temp_dir;
  runtime_state->owned_temp_dirs[LONEJSON_SPOOL_CLASS_LARGE_TEXT] =
      spool_large_text_temp_dir;
  runtime_state->config.spool_default.temp_dir =
      runtime_state->owned_temp_dirs[LONEJSON_SPOOL_CLASS_DEFAULT];
  runtime_state->config.spool_blob.temp_dir =
      runtime_state->owned_temp_dirs[LONEJSON_SPOOL_CLASS_BLOB];
  runtime_state->config.spool_large_text.temp_dir =
      runtime_state->owned_temp_dirs[LONEJSON_SPOOL_CLASS_LARGE_TEXT];
  lonejson__runtime_apply_config(runtime_state);

  lonejson__runtime_handle_register(
      &((lonejson__runtime_bundle *)runtime)->live_handle, runtime_state,
      runtime);
  runtime->_state_token =
      ((lonejson__runtime_bundle *)runtime)->live_handle.generation;
  runtime->state = runtime_state;
  runtime->_runtime_owner_self = runtime;
  runtime->_runtime_owner_data = runtime_state;
  runtime->parse_cstr = lonejson__runtime_parse_cstr;
  runtime->parse_buffer = lonejson__runtime_parse_buffer;
  runtime->parse_reader = lonejson__runtime_parse_reader;
  runtime->parse_filep = lonejson__runtime_parse_filep;
  runtime->parse_path = lonejson__runtime_parse_path;
  runtime->validate_buffer = lonejson__runtime_validate_buffer;
  runtime->validate_cstr = lonejson__runtime_validate_cstr;
  runtime->validate_reader = lonejson__runtime_validate_reader;
  runtime->validate_filep = lonejson__runtime_validate_filep;
  runtime->validate_path = lonejson__runtime_validate_path;
  runtime->visit_value_buffer = lonejson__runtime_visit_value_buffer;
  runtime->visit_value_cstr = lonejson__runtime_visit_value_cstr;
  runtime->visit_value_reader = lonejson__runtime_visit_value_reader;
  runtime->visit_value_filep = lonejson__runtime_visit_value_filep;
  runtime->visit_value_path = lonejson__runtime_visit_value_path;
  runtime->visit_value_fd = lonejson__runtime_visit_value_fd;
  runtime->value_visitor_stream_open = lonejson_value_visitor_stream_open;
  runtime->visit_path_value_buffer = lonejson__runtime_visit_path_value_buffer;
  runtime->visit_path_value_cstr = lonejson__runtime_visit_path_value_cstr;
  runtime->visit_path_value_reader = lonejson__runtime_visit_path_value_reader;
  runtime->visit_path_value_filep = lonejson__runtime_visit_path_value_filep;
  runtime->visit_path_value_path = lonejson__runtime_visit_path_value_path;
  runtime->visit_path_value_fd = lonejson__runtime_visit_path_value_fd;
  runtime->init = lonejson__runtime_init_value;
  runtime->reset = lonejson__runtime_reset_value;
  runtime->record_assign_null = lonejson_record_assign_null;
  runtime->record_assign_string = lonejson_record_assign_string;
  runtime->record_array_append_string = lonejson_record_array_append_string;
  runtime->record_array_append_i64 = lonejson_record_array_append_i64;
  runtime->record_array_append_u64 = lonejson_record_array_append_u64;
  runtime->record_array_append_f64 = lonejson_record_array_append_f64;
  runtime->record_array_append_bool = lonejson_record_array_append_bool;
  runtime->record_object_array_append_slot =
      lonejson_record_object_array_append_slot;
  runtime->cleanup = lonejson__runtime_cleanup_value;
  runtime->stream_open_reader = lonejson__runtime_stream_open_reader;
  runtime->stream_open_filep = lonejson__runtime_stream_open_filep;
  runtime->stream_open_path = lonejson__runtime_stream_open_path;
  runtime->stream_open_fd = lonejson__runtime_stream_open_fd;
  runtime->array_stream_open_reader =
      lonejson__runtime_array_stream_open_reader;
  runtime->array_stream_open_filep = lonejson__runtime_array_stream_open_filep;
  runtime->array_stream_open_path = lonejson__runtime_array_stream_open_path;
  runtime->array_stream_open_fd = lonejson__runtime_array_stream_open_fd;
  runtime->array_stream_open_push = lonejson__runtime_array_stream_open_push;
#ifndef LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL
  runtime->sse_push_json = lonejson_sse_push_json;
  runtime->sse_finish_json = lonejson_sse_finish_json;
  runtime->sse_push_json_value = lonejson_sse_push_json_value;
  runtime->sse_finish_json_value = lonejson_sse_finish_json_value;
#endif
  runtime->spooled_init = lonejson__runtime_spooled_init;
  runtime->spooled_init_class = lonejson__runtime_spooled_init_class;
  runtime->json_value_init = lonejson__runtime_json_value_init;
  runtime->json_value_cleanup = lonejson__runtime_json_value_cleanup;
  runtime->array_rewrite_reader = lonejson__runtime_array_rewrite_reader;
  runtime->array_rewrite_reader_to_filep =
      lonejson__runtime_array_rewrite_reader_to_filep;
  runtime->array_rewrite_reader_to_fd =
      lonejson__runtime_array_rewrite_reader_to_fd;
  runtime->array_rewrite_filep = lonejson__runtime_array_rewrite_filep;
  runtime->array_rewrite_filep_to_filep =
      lonejson__runtime_array_rewrite_filep_to_filep;
  runtime->array_rewrite_fd = lonejson__runtime_array_rewrite_fd;
  runtime->array_rewrite_fd_to_fd = lonejson__runtime_array_rewrite_fd_to_fd;
  runtime->array_rewrite_path = lonejson__runtime_array_rewrite_path;
  runtime->value_rewrite_reader = lonejson__runtime_value_rewrite_reader;
  runtime->value_rewrite_buffer = lonejson__runtime_value_rewrite_buffer;
  runtime->value_rewrite_filep = lonejson__runtime_value_rewrite_filep;
  runtime->value_rewrite_fd = lonejson__runtime_value_rewrite_fd;
  runtime->value_rewrite_path = lonejson__runtime_value_rewrite_path;
  runtime->value_rewrite_selector_reader =
      lonejson__runtime_value_rewrite_selector_reader;
  runtime->value_rewrite_selector_buffer =
      lonejson__runtime_value_rewrite_selector_buffer;
  runtime->value_rewrite_selector_filep =
      lonejson__runtime_value_rewrite_selector_filep;
  runtime->value_rewrite_selector_fd =
      lonejson__runtime_value_rewrite_selector_fd;
  runtime->value_rewrite_selector_path =
      lonejson__runtime_value_rewrite_selector_path;
  runtime->generator_init = lonejson__runtime_generator_init;
  runtime->generator_measure = lonejson_generator_measure;
  runtime->writer_generator_init = lonejson_writer_generator_init;
  runtime->writer_init_sink = lonejson__runtime_writer_init_sink;
  runtime->write_json_string_sink = lonejson_write_json_string_sink;
  runtime->write_json_string_buffer_sink =
      lonejson_write_json_string_buffer_sink;
  runtime->write_json_string_spooled_sink =
      lonejson_write_json_string_spooled_sink;
  runtime->serialize_sink = lonejson_serialize_sink;
  runtime->serialize_buffer = lonejson_serialize_buffer;
  runtime->serialize_alloc = lonejson_serialize_alloc;
  runtime->serialize_owned = lonejson_serialize_owned;
  runtime->serialize_filep = lonejson_serialize_filep;
  runtime->serialize_path = lonejson_serialize_path;
  runtime->serialize_jsonl_sink = lonejson_serialize_jsonl_sink;
  runtime->serialize_jsonl_buffer = lonejson_serialize_jsonl_buffer;
  runtime->serialize_jsonl_alloc = lonejson_serialize_jsonl_alloc;
  runtime->serialize_jsonl_owned = lonejson_serialize_jsonl_owned;
  runtime->serialize_jsonl_filep = lonejson_serialize_jsonl_filep;
  runtime->serialize_jsonl_path = lonejson_serialize_jsonl_path;
#ifdef LONEJSON_WITH_JWT
  runtime->jwk_parse_json = lonejson_jwk_parse_json;
  runtime->jwks_parse_json = lonejson_jwks_parse_json;
  runtime->jwt_decode_compact = lonejson_jwt_decode_compact;
  runtime->jwt_validate_signature_with_runtime =
      lonejson_jwt_validate_signature_with_runtime;
  runtime->set_auth_provider = lonejson__runtime_set_auth_provider;
#endif
#ifdef LONEJSON_WITH_OIDC
  runtime->oidc_discovery_parse_json = lonejson_oidc_discovery_parse_json;
  runtime->oauth2_introspection_response_parse_json =
      lonejson_oauth2_introspection_response_parse_json;
  runtime->oidc_userinfo_response_parse_json =
      lonejson_oidc_userinfo_response_parse_json;
  runtime->oidc_fetch_discovery = lonejson_oidc_fetch_discovery;
  runtime->oidc_jwks_cache_update_json = lonejson_oidc_jwks_cache_update_json;
  runtime->oidc_jwks_cache_refresh = lonejson_oidc_jwks_cache_refresh;
  runtime->oauth2_token_response_parse_json =
      lonejson_oauth2_token_response_parse_json;
  runtime->oauth2_client_credentials_request =
      lonejson_oauth2_client_credentials_request;
  runtime->oauth2_refresh_token_request = lonejson_oauth2_refresh_token_request;
  runtime->oauth2_token_flow_ensure = lonejson_oauth2_token_flow_ensure;
  runtime->oauth2_introspect_token_request =
      lonejson_oauth2_introspect_token_request;
  runtime->oauth2_revoke_token_request = lonejson_oauth2_revoke_token_request;
  runtime->oidc_fetch_userinfo = lonejson_oidc_fetch_userinfo;
  runtime->oidc_authorization_code_token_request =
      lonejson_oidc_authorization_code_token_request;
  runtime->oidc_validate_bearer_token = lonejson_oidc_validate_bearer_token;
  runtime->oidc_pkce_challenge_with_runtime =
      lonejson_oidc_pkce_challenge_with_runtime;
  runtime->oidc_pkce_generate_with_runtime =
      lonejson_oidc_pkce_generate_with_runtime;
  runtime->m2m_credential_generate = lonejson_m2m_credential_generate;
  runtime->m2m_verify_authorization = lonejson_m2m_verify_authorization;
  runtime->m2m_signup_generate = lonejson_m2m_signup_generate;
  runtime->m2m_signup_complete = lonejson_m2m_signup_complete;
  runtime->set_http_provider = lonejson__runtime_set_http_provider;
#endif
#ifdef LONEJSON_WITH_CURL
  runtime->curl_parse_init = lonejson__runtime_curl_parse_init;
  runtime->curl_array_parse_init = lonejson__runtime_curl_array_parse_init;
  runtime->curl_string_array_parse_init =
      lonejson__runtime_curl_string_array_parse_init;
  runtime->curl_string_items_parse_init =
      lonejson__runtime_curl_string_items_parse_init;
  runtime->curl_upload_init = lonejson__runtime_curl_upload_init;
#ifdef LONEJSON_WITH_OIDC
  runtime->oidc_jwks_cache_parse_init =
      lonejson__runtime_oidc_jwks_cache_parse_init;
#endif
#endif
  runtime->free = lonejson_free;

  lonejson__clear_error(error);
  return runtime;
}

void lonejson_free(lonejson *runtime) {
  lonejson_runtime *runtime_state;
  const lonejson_allocator *allocator = NULL;
  lonejson__runtime_handle *live_handle;
  int drained;

  if (runtime == NULL) {
    return;
  }
  if (runtime->state == NULL || runtime->_state_token == 0u) {
    return;
  }
  if (runtime->_runtime_owner_self != runtime ||
      runtime->_runtime_owner_data == NULL) {
    runtime->state = NULL;
    runtime->_state_token = 0u;
    runtime->_runtime_owner_self = NULL;
    runtime->_runtime_owner_data = NULL;
    return;
  }
  runtime_state = (lonejson_runtime *)runtime->_runtime_owner_data;
  allocator = &runtime_state->allocator_storage;
  runtime_state->api_owner = NULL;
  live_handle = &((lonejson__runtime_bundle *)runtime)->live_handle;
  LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
  LONEJSON__RUNTIME_HANDLE_CLOSING_STORE(live_handle, 1);
  {
    lonejson__runtime_handle **link = &g_lonejson_runtime_handle_live_list;
    while (*link != NULL) {
      if (*link == live_handle) {
        *link = live_handle->next_live;
        break;
      }
      link = &(*link)->next_live;
    }
  }
  live_handle->next_live = NULL;
  LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
  for (;;) {
    drained = LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_LOAD(live_handle) == 0u;
    if (drained) {
      break;
    }
    LONEJSON__RUNTIME_HANDLE_WAIT_YIELD();
  }
  runtime->state = NULL;
  runtime->_state_token = 0u;
  runtime->_runtime_owner_self = NULL;
  runtime->_runtime_owner_data = NULL;
  lonejson__runtime_free_owned_config(runtime_state);
  lonejson__buffer_free(allocator, runtime, sizeof(lonejson__runtime_bundle));
}

#ifdef LONEJSON_WITH_JWT
static lonejson_status
lonejson__runtime_set_auth_provider(lonejson *runtime,
                                    const lonejson_auth_provider *provider,
                                    lonejson_error *error) {
  return lonejson_set_auth_provider(runtime, provider, error);
}

lonejson_status
lonejson_set_auth_provider(lonejson *runtime,
                           const lonejson_auth_provider *provider,
                           lonejson_error *error) {
  lonejson_runtime *runtime_state;

  lonejson__clear_error(error);
  runtime_state = lonejson__runtime_mut(runtime);
  if (runtime_state == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "runtime is required");
  }
  if (provider != NULL && provider->verify_jws == NULL &&
      provider->random_bytes == NULL && provider->sha256 == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "auth provider must expose at least one method");
  }
  if (provider != NULL) {
    runtime_state->auth_provider = *provider;
    runtime_state->has_auth_provider = 1;
  } else {
    memset(&runtime_state->auth_provider, 0,
           sizeof(runtime_state->auth_provider));
    runtime_state->has_auth_provider = 0;
  }
  return LONEJSON_STATUS_OK;
}
#endif

#ifdef LONEJSON_WITH_OIDC
static lonejson_status
lonejson__runtime_set_http_provider(lonejson *runtime,
                                    const lonejson_http_provider *provider,
                                    lonejson_error *error) {
  return lonejson_set_http_provider(runtime, provider, error);
}

lonejson_status
lonejson_set_http_provider(lonejson *runtime,
                           const lonejson_http_provider *provider,
                           lonejson_error *error) {
  lonejson_runtime *runtime_state;

  lonejson__clear_error(error);
  runtime_state = lonejson__runtime_mut(runtime);
  if (runtime_state == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "runtime is required");
  }
  if (provider != NULL && provider->request == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP provider request method is required");
  }
  if (provider != NULL) {
    runtime_state->http_provider = *provider;
    runtime_state->has_http_provider = 1;
  } else {
    memset(&runtime_state->http_provider, 0,
           sizeof(runtime_state->http_provider));
    runtime_state->has_http_provider = 0;
  }
  return LONEJSON_STATUS_OK;
}
#endif

void lonejson_string_array_stream_init(lonejson_string_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  memset(stream, 0, sizeof(*stream));
  stream->_lonejson_magic =
      lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
  lonejson__string_array_stream_assign_methods(stream);
}

lonejson_status lonejson_string_array_stream_set_handler(
    lonejson_string_array_stream *stream,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error) {
  if (stream == NULL || handler == NULL || handler->chunk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "string array stream and chunk handler are "
                               "required");
  }
  if (!lonejson__string_array_stream_is_initialized(stream)) {
    lonejson_string_array_stream_init(stream);
  }
  stream->handler = *handler;
  stream->user = user;
  stream->active = 0;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_mapped_array_stream_init(lonejson_mapped_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  memset(stream, 0, sizeof(*stream));
  stream->_lonejson_magic =
      lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
  lonejson__mapped_array_stream_assign_methods(stream);
}

lonejson_status lonejson_mapped_array_stream_set_handler(
    lonejson_mapped_array_stream *stream,
    const lonejson_mapped_array_stream_handler *handler,
    lonejson_error *error) {
  if (stream == NULL || handler == NULL || handler->item_map == NULL ||
      handler->item_dst == NULL || handler->item == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "mapped array stream, item map, destination, "
                               "and callback are required");
  }
  if (!lonejson__map_layout_is_valid(handler->item_map)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "mapped array stream item map is invalid");
  }
  if (!lonejson__mapped_array_stream_is_initialized(stream)) {
    lonejson_mapped_array_stream_init(stream);
  }
  stream->handler = *handler;
  stream->active = 0;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_owned_buffer_init(lonejson_owned_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }
  *buffer = lonejson_default_owned_buffer();
}

const char *lonejson_status_string(lonejson_status status) {
  switch (status) {
  case LONEJSON_STATUS_OK:
    return "ok";
  case LONEJSON_STATUS_INVALID_ARGUMENT:
    return "invalid_argument";
  case LONEJSON_STATUS_INVALID_JSON:
    return "invalid_json";
  case LONEJSON_STATUS_TYPE_MISMATCH:
    return "type_mismatch";
  case LONEJSON_STATUS_MISSING_REQUIRED_FIELD:
    return "missing_required_field";
  case LONEJSON_STATUS_DUPLICATE_FIELD:
    return "duplicate_field";
  case LONEJSON_STATUS_OVERFLOW:
    return "overflow";
  case LONEJSON_STATUS_TRUNCATED:
    return "truncated";
  case LONEJSON_STATUS_ALLOCATION_FAILED:
    return "allocation_failed";
  case LONEJSON_STATUS_CALLBACK_FAILED:
    return "callback_failed";
  case LONEJSON_STATUS_IO_ERROR:
    return "io_error";
  case LONEJSON_STATUS_UNSUPPORTED:
    return "unsupported";
  case LONEJSON_STATUS_INTERNAL_ERROR:
    return "internal_error";
  case LONEJSON_STATUS_PAUSED:
    return "paused";
  default:
    return "unknown";
  }
}

#ifdef LONEJSON_WITH_CURL
static lonejson_parser *lonejson__parser_create_ex(
    const lonejson_map *map, void *dst, const lonejson__parse_options *options,
    const lonejson_runtime *runtime, lonejson_error *error, int validate_only) {
  lonejson_parser *parser;
  unsigned char *workspace;
  lonejson__map_analysis analysis;

  if ((!validate_only && (map == NULL || dst == NULL)) ||
      (validate_only && (map != NULL || dst != NULL))) {
    lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 1u, 0u,
        validate_only ? "validation parser does not accept mapping arguments"
                      : "map and destination are required");
    return NULL;
  }
  analysis.valid = 1;
  analysis.may_allocate = 0;
  analysis.adopt_mask = 0u;
  if (!validate_only) {
    analysis = lonejson__map_analysis_cached_with_allocator(
        map, options != NULL ? options->allocator : NULL);
  }
  if (!validate_only && !analysis.valid) {
    lonejson__set_error(error, analysis.status, 0u, 0u, 0u,
                        analysis.status == LONEJSON_STATUS_ALLOCATION_FAILED
                            ? "failed to allocate schema analysis workspace"
                            : "invalid field layout");
    return NULL;
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  parser = (lonejson_parser *)lonejson__buffer_alloc(
      options ? options->allocator : NULL,
      sizeof(*parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
          LONEJSON__PARSER_WORKSPACE_SLACK);
  if (parser == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 1u, 0u,
                        "failed to allocate parser");
    return NULL;
  }
  workspace = ((unsigned char *)parser) + sizeof(*parser);
  lonejson__parser_init_state(
      parser, map, dst, options, runtime, validate_only, !validate_only,
      analysis.may_allocate, analysis.adopt_mask, workspace,
      LONEJSON_PUSH_PARSER_BUFFER_SIZE + LONEJSON__PARSER_WORKSPACE_SLACK);
  parser->self_alloc_size = sizeof(*parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                            LONEJSON__PARSER_WORKSPACE_SLACK;
  parser->owns_self = 1;
  if (!validate_only && parser->options.clear_destination) {
    lonejson__init_map_parse(parser, map, dst);
    if (parser->failed) {
      if (error != NULL) {
        *error = parser->error;
      }
      lonejson_parser_destroy(parser);
      return NULL;
    }
  }
  lonejson__clear_error(error);
  return parser;
}
#endif

static lonejson_status lonejson_parser_feed(lonejson_parser *parser,
                                            const void *data, size_t len) {
  size_t consumed;

  return lonejson__parser_feed_bytes(parser, (const unsigned char *)data, len,
                                     &consumed, 0);
}

static lonejson_status lonejson_parser_finish(lonejson_parser *parser) {
  if (parser == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (parser->failed) {
    return parser->error.code;
  }
  if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
    parser->lex_mode = LONEJSON_LEX_NONE;
    if (lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER) !=
        LONEJSON_STATUS_OK) {
      return parser->error.code;
    }
  }
  if (parser->lex_mode != LONEJSON_LEX_NONE) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "incomplete JSON token at end of input");
  }
  if (!parser->root_started || !parser->root_finished ||
      parser->frame_count != 0u) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "incomplete JSON document");
  }
  return parser->error.truncated ? LONEJSON_STATUS_TRUNCATED
                                 : LONEJSON_STATUS_OK;
}

static void lonejson_parser_destroy(lonejson_parser *parser) {
  if (parser == NULL) {
    return;
  }
  lonejson__direct_string_clear(parser, 0);
  lonejson__parser_cleanup_json_stream_path(parser);
  lonejson__parser_unwind_active_mapped_array_streams(parser);
  while (parser->frame_count != 0u) {
    lonejson__pop_frame(parser);
  }
  if (parser->owns_self) {
    lonejson__buffer_free(&parser->allocator, parser, parser->self_alloc_size);
  }
}

static lonejson_status lonejson__parse_buffer_with_options(
    const lonejson_map *map, void *dst, const void *data, size_t len,
    const lonejson__parse_options *options, const lonejson_runtime *runtime,
    lonejson_error *error) {
  lonejson_parser *parser;
  lonejson_status status;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];
  lonejson__map_analysis analysis;

  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer is required");
  }
  analysis = lonejson__map_analysis_cached_with_allocator(
      map, options != NULL ? options->allocator : NULL);
  if (!analysis.valid) {
    return lonejson__set_error(
        error, analysis.status, 0u, 0u, 0u,
        analysis.status == LONEJSON_STATUS_ALLOCATION_FAILED
            ? "failed to allocate schema analysis workspace"
            : "invalid field layout");
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, map, dst, options, runtime, 0, 1,
                              analysis.may_allocate, analysis.adopt_mask,
                              parser_workspace, sizeof(parser_workspace));
  if (parser->options.clear_destination) {
    lonejson__init_map_parse(parser, map, dst);
    if (parser->failed) {
      if (error != NULL) {
        *error = parser->error;
      }
      return parser->error.code;
    }
  }
  lonejson__clear_error(error);
  status = lonejson_parser_feed(parser, data, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson_parser_finish(parser);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__parser_unwind_active_mapped_array_streams(parser);
  }
  lonejson__direct_string_abort(parser);
  lonejson__parser_cleanup_json_stream_path(parser);
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_parse_buffer(lonejson *runtime,
                                      const lonejson_map *map, void *dst,
                                      const void *data, size_t len,
                                      lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__parse_buffer_with_options(
      map, dst, data, len, &runtime_state->parse_options, runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status
lonejson__runtime_parse_buffer(lonejson *runtime, const lonejson_map *map,
                               void *dst, const void *data, size_t len,
                               lonejson_error *error) {
  return lonejson_parse_buffer(runtime, map, dst, data, len, error);
}

lonejson_status lonejson_parse_cstr(lonejson *runtime, const lonejson_map *map,
                                    void *dst, const char *json,
                                    lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_status status;

  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__parse_buffer_with_options(map, dst, json, strlen(json),
                                               &runtime_state->parse_options,
                                               runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status lonejson__runtime_parse_cstr(lonejson *runtime,
                                                    const lonejson_map *map,
                                                    void *dst, const char *json,
                                                    lonejson_error *error) {
  return lonejson_parse_cstr(runtime, map, dst, json, error);
}

static lonejson_read_result
lonejson__file_reader(void *user, unsigned char *buffer, size_t capacity) {
  FILE *fp = (FILE *)user;
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  result.bytes_read = fread(buffer, 1u, capacity, fp);
  result.eof = feof(fp) ? 1 : 0;
  result.error_code = ferror(fp) ? errno : 0;
  return result;
}

static lonejson_status lonejson__parse_reader_with_options(
    const lonejson_map *map, void *dst, lonejson_reader_fn reader, void *user,
    const lonejson__parse_options *options, const lonejson_runtime *runtime,
    lonejson_error *error) {
  lonejson_parser *parser;
  unsigned char buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson_status status = LONEJSON_STATUS_OK;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];
  lonejson__map_analysis analysis;

  if (reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader callback is required");
  }
  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  analysis = lonejson__map_analysis_cached_with_allocator(
      map, options != NULL ? options->allocator : NULL);
  if (!analysis.valid) {
    return lonejson__set_error(
        error, analysis.status, 0u, 0u, 0u,
        analysis.status == LONEJSON_STATUS_ALLOCATION_FAILED
            ? "failed to allocate schema analysis workspace"
            : "invalid field layout");
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, map, dst, options, runtime, 0, 1,
                              analysis.may_allocate, analysis.adopt_mask,
                              parser_workspace, sizeof(parser_workspace));
  if (parser->options.clear_destination) {
    lonejson__init_map_parse(parser, map, dst);
    if (parser->failed) {
      if (error != NULL) {
        *error = parser->error;
      }
      return parser->error.code;
    }
  }
  lonejson__clear_error(error);
  for (;;) {
    lonejson_read_result chunk = reader(user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      parser->error.system_errno = chunk.error_code;
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_IO_ERROR, parser->error.offset,
          parser->error.line, parser->error.column, "reader callback failed");
      break;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_parser_feed(parser, buffer, chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        break;
      }
    }
    if (chunk.would_block) {
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_CALLBACK_FAILED, parser->error.offset,
          parser->error.line, parser->error.column, "reader would block");
      break;
    }
    if (chunk.eof) {
      status = lonejson_parser_finish(parser);
      break;
    }
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__parser_unwind_active_mapped_array_streams(parser);
  }
  lonejson__direct_string_abort(parser);
  lonejson__parser_cleanup_json_stream_path(parser);
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_parse_reader(lonejson *runtime,
                                      const lonejson_map *map, void *dst,
                                      lonejson_reader_fn reader, void *user,
                                      lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__parse_reader_with_options(map, dst, reader, user,
                                               &runtime_state->parse_options,
                                               runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status
lonejson__runtime_parse_reader(lonejson *runtime, const lonejson_map *map,
                               void *dst, lonejson_reader_fn reader, void *user,
                               lonejson_error *error) {
  return lonejson_parse_reader(runtime, map, dst, reader, user, error);
}

lonejson_status lonejson_parse_filep(lonejson *runtime, const lonejson_map *map,
                                     void *dst, FILE *fp,
                                     lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_status status;

  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__parse_reader_with_options(
      map, dst, lonejson__file_reader, fp, &runtime_state->parse_options,
      runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status lonejson__runtime_parse_filep(lonejson *runtime,
                                                     const lonejson_map *map,
                                                     void *dst, FILE *fp,
                                                     lonejson_error *error) {
  return lonejson_parse_filep(runtime, map, dst, fp, error);
}

lonejson_status lonejson_parse_path(lonejson *runtime, const lonejson_map *map,
                                    void *dst, const char *path,
                                    lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_parse_filep(runtime, map, dst, fp, error);
  fclose(fp);
  return status;
}

static lonejson_status lonejson__runtime_parse_path(lonejson *runtime,
                                                    const lonejson_map *map,
                                                    void *dst, const char *path,
                                                    lonejson_error *error) {
  return lonejson_parse_path(runtime, map, dst, path, error);
}

static lonejson_status
lonejson__runtime_validate_buffer(lonejson *runtime, const void *data,
                                  size_t len, lonejson_error *error) {
  return lonejson_validate_buffer(runtime, data, len, error);
}

static lonejson_status lonejson__runtime_validate_cstr(lonejson *runtime,
                                                       const char *json,
                                                       lonejson_error *error) {
  return lonejson_validate_cstr(runtime, json, error);
}

static lonejson_status
lonejson__runtime_validate_reader(lonejson *runtime, lonejson_reader_fn reader,
                                  void *user, lonejson_error *error) {
  return lonejson_validate_reader(runtime, reader, user, error);
}

static lonejson_status lonejson__runtime_validate_filep(lonejson *runtime,
                                                        FILE *fp,
                                                        lonejson_error *error) {
  return lonejson_validate_filep(runtime, fp, error);
}

static lonejson_status lonejson__runtime_validate_path(lonejson *runtime,
                                                       const char *path,
                                                       lonejson_error *error) {
  return lonejson_validate_path(runtime, path, error);
}

static lonejson_status lonejson__runtime_visit_value_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lonejson_value_visitor *visitor, void *user, lonejson_error *error) {
  return lonejson_visit_value_buffer(runtime, data, len, visitor, user, error);
}

static lonejson_status
lonejson__runtime_visit_value_cstr(lonejson *runtime, const char *json,
                                   const lonejson_value_visitor *visitor,
                                   void *user, lonejson_error *error) {
  return lonejson_visit_value_cstr(runtime, json, visitor, user, error);
}

static lonejson_status lonejson__runtime_visit_value_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_value_visitor *visitor, void *user, lonejson_error *error) {
  return lonejson_visit_value_reader(runtime, reader, reader_user, visitor,
                                     user, error);
}

static lonejson_status
lonejson__runtime_visit_value_filep(lonejson *runtime, FILE *fp,
                                    const lonejson_value_visitor *visitor,
                                    void *user, lonejson_error *error) {
  return lonejson_visit_value_filep(runtime, fp, visitor, user, error);
}

static lonejson_status
lonejson__runtime_visit_value_path(lonejson *runtime, const char *path,
                                   const lonejson_value_visitor *visitor,
                                   void *user, lonejson_error *error) {
  return lonejson_visit_value_path(runtime, path, visitor, user, error);
}

static lonejson_status
lonejson__runtime_visit_value_fd(lonejson *runtime, int fd,
                                 const lonejson_value_visitor *visitor,
                                 void *user, lonejson_error *error) {
  return lonejson_visit_value_fd(runtime, fd, visitor, user, error);
}

static lonejson_status lonejson__runtime_visit_path_value_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error) {
  return lonejson_visit_path_value_buffer(runtime, data, len, visitor, user,
                                          error);
}

static lonejson_status lonejson__runtime_visit_path_value_cstr(
    lonejson *runtime, const char *json,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error) {
  return lonejson_visit_path_value_cstr(runtime, json, visitor, user, error);
}

static lonejson_status lonejson__runtime_visit_path_value_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error) {
  return lonejson_visit_path_value_reader(runtime, reader, reader_user, visitor,
                                          user, error);
}

static lonejson_status lonejson__runtime_visit_path_value_filep(
    lonejson *runtime, FILE *fp, const lonejson_path_value_visitor *visitor,
    void *user, lonejson_error *error) {
  return lonejson_visit_path_value_filep(runtime, fp, visitor, user, error);
}

static lonejson_status lonejson__runtime_visit_path_value_path(
    lonejson *runtime, const char *path,
    const lonejson_path_value_visitor *visitor, void *user,
    lonejson_error *error) {
  return lonejson_visit_path_value_path(runtime, path, visitor, user, error);
}

static lonejson_status lonejson__runtime_visit_path_value_fd(
    lonejson *runtime, int fd, const lonejson_path_value_visitor *visitor,
    void *user, lonejson_error *error) {
  return lonejson_visit_path_value_fd(runtime, fd, visitor, user, error);
}

static lonejson_status lonejson__runtime_init_value(lonejson *runtime,
                                                    const lonejson_map *map,
                                                    void *value,
                                                    lonejson_error *error) {
  return lonejson_init(runtime, map, value, error);
}

static lonejson_status lonejson__runtime_reset_value(lonejson *runtime,
                                                     const lonejson_map *map,
                                                     void *value,
                                                     lonejson_error *error) {
  return lonejson_reset(runtime, map, value, error);
}

static lonejson_status lonejson__runtime_cleanup_value(lonejson *runtime,
                                                       const lonejson_map *map,
                                                       void *value,
                                                       lonejson_error *error) {
  return lonejson__runtime_lifecycle(runtime, map, value, error,
                                     LONEJSON__RECORD_CLEANUP);
}

static void lonejson__stream_assign_methods(lonejson_stream *stream) {
  if (stream == NULL) {
    return;
  }
  stream->next = lonejson_stream_next;
  stream->error_fn = lonejson_stream_error;
  stream->close = lonejson_stream_close;
}

static void lonejson__stream_prepare_parser(lonejson__stream_state *stream,
                                            void *dst) {
  lonejson__parser_restart_stream(stream->parser, dst);
  if (stream->parser->options.clear_destination) {
    if (stream->prepared_dst == dst) {
      lonejson__reset_map_parse(stream->parser, stream->map, dst);
    } else {
      lonejson__init_map_parse(stream->parser, stream->map, dst);
      stream->prepared_dst = dst;
    }
  }
  stream->current_dst = dst;
  stream->object_in_progress = 1;
}

static lonejson_read_result
lonejson__stream_read(lonejson__stream_state *stream) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  switch (stream->source_kind) {
  case LONEJSON_STREAM_SOURCE_READER:
    return stream->reader(stream->reader_user, stream->io_buffer,
                          sizeof(stream->io_buffer));
  case LONEJSON_STREAM_SOURCE_FILE:
    return lonejson__file_reader(stream->fp, stream->io_buffer,
                                 sizeof(stream->io_buffer));
  case LONEJSON_STREAM_SOURCE_FD: {
    ssize_t rc = read(stream->fd, stream->io_buffer, sizeof(stream->io_buffer));
    if (rc > 0) {
      result.bytes_read = (size_t)rc;
    } else if (rc == 0) {
      result.eof = 1;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      result.would_block = 1;
    } else {
      result.error_code = errno;
    }
    return result;
  }
  default:
    result.error_code = EINVAL;
    return result;
  }
}

static lonejson__stream_state *lonejson__stream_open_common(
    const lonejson_map *map, const lonejson__parse_options *options,
    const lonejson_runtime *runtime, lonejson_error *error) {
  lonejson__stream_state *stream;
  size_t parser_bytes;
  unsigned char *workspace;

  if (map == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "map is required");
    return NULL;
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  parser_bytes = sizeof(*stream->parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                 LONEJSON__PARSER_WORKSPACE_SLACK;
  stream = (lonejson__stream_state *)lonejson__buffer_alloc(
      options ? options->allocator : NULL, sizeof(*stream) + parser_bytes);
  if (stream == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate stream");
    return NULL;
  }
  stream->map = map;
  stream->options = options ? *options : lonejson__default_parse_options();
  stream->allocator = lonejson__allocator_resolve(stream->options.allocator);
  stream->self_alloc_size = sizeof(*stream) + parser_bytes;
  memset(&stream->public, 0, sizeof(stream->public));
  lonejson__clear_error(&stream->public.error);
  lonejson__stream_assign_methods(&stream->public);
  stream->reader = NULL;
  stream->reader_user = NULL;
  stream->fp = NULL;
  stream->fd = -1;
  stream->memory_input = NULL;
  stream->memory_input_len = 0u;
  stream->memory_input_offset = 0u;
  stream->owns_fp = 0;
  stream->owns_fd = 0;
  stream->saw_eof = 0;
  stream->object_in_progress = 0;
  stream->buffered_start = 0u;
  stream->buffered_end = 0u;
  stream->source_kind = 0;
  stream->current_dst = NULL;
  stream->prepared_dst = NULL;
  if (lonejson__runtime_snapshot_init(&stream->runtime_storage, runtime, 1,
                                      error) != LONEJSON_STATUS_OK) {
    lonejson__buffer_free(&stream->allocator, stream, stream->self_alloc_size);
    return NULL;
  }
  stream->runtime = runtime != NULL ? &stream->runtime_storage
                                    : (const lonejson_runtime *)NULL;
  if (runtime != NULL &&
      stream->options.allocator == runtime->config.allocator) {
    stream->options.allocator = stream->runtime->config.allocator;
  }
  if (runtime != NULL && stream->options.fixed_string_scratch ==
                             runtime->config.fixed_string_scratch) {
    stream->options.fixed_string_scratch =
        stream->runtime->parse_options.fixed_string_scratch;
    stream->options.fixed_string_scratch_size =
        stream->runtime->parse_options.fixed_string_scratch_size;
  }
  stream->parser =
      (lonejson_parser *)(((unsigned char *)stream) + sizeof(*stream));
  workspace = ((unsigned char *)stream->parser) + sizeof(*stream->parser);
  memset(stream->parser, 0, sizeof(*stream->parser));
  workspace =
      lonejson__align_pointer(workspace, LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
  stream->parser->root_map = map;
  stream->parser->runtime = stream->runtime;
  stream->parser->options = stream->options;
  stream->parser->allocator = stream->allocator;
  stream->parser->workspace = workspace;
  stream->parser->workspace_size =
      (size_t)(((unsigned char *)stream) + sizeof(*stream) + parser_bytes -
               workspace);
  stream->parser->workspace_top = stream->parser->workspace_size;
  stream->parser->frames = (lonejson_frame *)workspace;
  stream->parser->self_alloc_size = parser_bytes;
  stream->parser->owns_self = 0;
  lonejson__clear_error(error);
  return stream;
}

static lonejson_stream *lonejson__stream_open_reader_with_options(
    const lonejson_map *map, lonejson_reader_fn reader, void *user,
    const lonejson__parse_options *options, const lonejson_runtime *runtime,
    lonejson_error *error) {
  lonejson__stream_state *stream;

  if (reader == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "reader callback is required");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, runtime, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_READER;
  stream->reader = reader;
  stream->reader_user = user;
  return &stream->public;
}

static lonejson_stream *lonejson__stream_open_filep_with_options(
    const lonejson_map *map, FILE *fp, const lonejson__parse_options *options,
    const lonejson_runtime *runtime, lonejson_error *error) {
  lonejson__stream_state *stream;

  if (fp == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "file pointer is required");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, runtime, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_FILE;
  stream->fp = fp;
  return &stream->public;
}

static lonejson_stream *lonejson__stream_open_path_with_options(
    const lonejson_map *map, const char *path,
    const lonejson__parse_options *options, const lonejson_runtime *runtime,
    lonejson_error *error) {
  lonejson_stream *stream;
  FILE *fp;

  if (path == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "path is required");
    return NULL;
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                        "failed to open '%s'", path);
    return NULL;
  }
  stream = lonejson__stream_open_filep_with_options(map, fp, options, runtime,
                                                    error);
  if (stream == NULL) {
    fclose(fp);
    return NULL;
  }
  lonejson__stream_state_mut(stream)->owns_fp = 1;
  return stream;
}

static lonejson_stream *lonejson__stream_open_fd_with_options(
    const lonejson_map *map, int fd, const lonejson__parse_options *options,
    const lonejson_runtime *runtime, lonejson_error *error) {
  lonejson__stream_state *stream;

  if (fd < 0) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "fd must be non-negative");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, runtime, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_FD;
  stream->fd = fd;
  return &stream->public;
}

lonejson_stream *lonejson_stream_open_reader(lonejson *runtime,
                                             const lonejson_map *map,
                                             lonejson_reader_fn reader,
                                             void *user,
                                             lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_stream *stream;

  runtime_state = lonejson__runtime_owner_pin_const_nohook(
      (const lonejson *)runtime, &borrow);
  if (runtime_state == NULL) {
    runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
    if (runtime_state == NULL) {
      return NULL;
    }
  }
  stream = lonejson__stream_open_reader_with_options(
      map, reader, user, &runtime_state->parse_options, runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return stream;
}

static lonejson_stream *
lonejson__runtime_stream_open_reader(lonejson *runtime, const lonejson_map *map,
                                     lonejson_reader_fn reader, void *user,
                                     lonejson_error *error) {
  return lonejson_stream_open_reader(runtime, map, reader, user, error);
}

lonejson_stream *lonejson_stream_open_filep(lonejson *runtime,
                                            const lonejson_map *map, FILE *fp,
                                            lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_stream *stream;

  runtime_state = lonejson__runtime_owner_pin_const_nohook(
      (const lonejson *)runtime, &borrow);
  if (runtime_state == NULL) {
    runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
    if (runtime_state == NULL) {
      return NULL;
    }
  }
  stream = lonejson__stream_open_filep_with_options(
      map, fp, &runtime_state->parse_options, runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return stream;
}

static lonejson_stream *
lonejson__runtime_stream_open_filep(lonejson *runtime, const lonejson_map *map,
                                    FILE *fp, lonejson_error *error) {
  return lonejson_stream_open_filep(runtime, map, fp, error);
}

lonejson_stream *lonejson_stream_open_path(lonejson *runtime,
                                           const lonejson_map *map,
                                           const char *path,
                                           lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_stream *stream;

  runtime_state = lonejson__runtime_owner_pin_const_nohook(
      (const lonejson *)runtime, &borrow);
  if (runtime_state == NULL) {
    runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
    if (runtime_state == NULL) {
      return NULL;
    }
  }
  stream = lonejson__stream_open_path_with_options(
      map, path, &runtime_state->parse_options, runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return stream;
}

static lonejson_stream *
lonejson__runtime_stream_open_path(lonejson *runtime, const lonejson_map *map,
                                   const char *path, lonejson_error *error) {
  return lonejson_stream_open_path(runtime, map, path, error);
}

lonejson_stream *lonejson_stream_open_fd(lonejson *runtime,
                                         const lonejson_map *map, int fd,
                                         lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_stream *stream;

  runtime_state = lonejson__runtime_owner_pin_const_nohook(
      (const lonejson *)runtime, &borrow);
  if (runtime_state == NULL) {
    runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
    if (runtime_state == NULL) {
      return NULL;
    }
  }
  stream = lonejson__stream_open_fd_with_options(
      map, fd, &runtime_state->parse_options, runtime_state, error);
  lonejson__runtime_borrow_release(&borrow);
  return stream;
}

static lonejson_stream *
lonejson__runtime_stream_open_fd(lonejson *runtime, const lonejson_map *map,
                                 int fd, lonejson_error *error) {
  return lonejson_stream_open_fd(runtime, map, fd, error);
}

lonejson_stream_result lonejson_stream_next(lonejson_stream *stream, void *dst,
                                            lonejson_error *error) {
  lonejson__stream_state *state;
  lonejson_status status;

  if (stream == NULL || dst == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "stream and destination are required");
    return LONEJSON_STREAM_ERROR;
  }
  state = lonejson__stream_state_mut(stream);
  if (!state->object_in_progress) {
    lonejson__stream_prepare_parser(state, dst);
    if (state->parser->failed) {
      state->object_in_progress = 0;
      state->current_dst = NULL;
      stream->error = state->parser->error;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_ERROR;
    }
  } else if (state->current_dst != dst) {
    lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                        0u, 0u, "resume the stream with the same destination");
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_STREAM_ERROR;
  }

  for (;;) {
    if (lonejson__parser_root_complete(state->parser)) {
      if (state->parser->error.code == LONEJSON_STATUS_OK &&
          !state->parser->error.truncated) {
        stream->error.code = LONEJSON_STATUS_OK;
        stream->error.line = 0u;
        stream->error.column = 0u;
        stream->error.offset = 0u;
        stream->error.system_errno = 0;
        stream->error.truncated = 0;
        stream->error.message[0] = '\0';
      } else {
        stream->error = state->parser->error;
      }
      state->object_in_progress = 0;
      state->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_OBJECT;
    }

    if (state->source_kind == LONEJSON_STREAM_SOURCE_MEMORY) {
      if (state->memory_input_offset >= state->memory_input_len) {
        state->saw_eof = 1;
        if (!state->parser->root_started) {
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_STREAM_EOF;
        }
        stream->error.code = lonejson_parser_finish(state->parser);
        stream->error = state->parser->error;
        state->object_in_progress = 0;
        state->current_dst = NULL;
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }

      if (!state->parser->root_started) {
        while (state->memory_input_offset < state->memory_input_len &&
               lonejson__is_json_space(
                   state->memory_input[state->memory_input_offset])) {
          state->memory_input_offset++;
        }
        if (state->memory_input_offset >= state->memory_input_len) {
          continue;
        }
        if (state->memory_input[state->memory_input_offset] != '{') {
          lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                              0u, 0u, "expected top-level object");
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_STREAM_ERROR;
        }
      }

      {
        size_t consumed;

        status = lonejson__parser_feed_bytes(
            state->parser, state->memory_input + state->memory_input_offset,
            state->memory_input_len - state->memory_input_offset, &consumed, 1);
        state->memory_input_offset += consumed;
      }
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        stream->error = state->parser->error;
        state->object_in_progress = 0;
        state->current_dst = NULL;
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      if (lonejson__parser_root_complete(state->parser)) {
        if (state->parser->error.code == LONEJSON_STATUS_OK &&
            !state->parser->error.truncated) {
          stream->error.code = LONEJSON_STATUS_OK;
          stream->error.line = 0u;
          stream->error.column = 0u;
          stream->error.offset = 0u;
          stream->error.system_errno = 0;
          stream->error.truncated = 0;
          stream->error.message[0] = '\0';
        } else {
          stream->error = state->parser->error;
        }
        state->object_in_progress = 0;
        state->current_dst = NULL;
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_OBJECT;
      }
      continue;
    }

    if (state->buffered_start == state->buffered_end) {
      lonejson_read_result chunk = lonejson__stream_read(state);
      if (chunk.error_code != 0) {
        stream->error.system_errno = chunk.error_code;
        lonejson__set_error(&stream->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u,
                            0u, "stream read failed");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      if (chunk.would_block) {
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_WOULD_BLOCK;
      }
      if (chunk.bytes_read == 0u && chunk.eof) {
        state->saw_eof = 1;
        if (!state->parser->root_started) {
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_STREAM_EOF;
        }
        stream->error.code = lonejson_parser_finish(state->parser);
        stream->error = state->parser->error;
        state->object_in_progress = 0;
        state->current_dst = NULL;
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      state->buffered_start = 0u;
      state->buffered_end = chunk.bytes_read;
      continue;
    }

    if (!state->parser->root_started) {
      size_t start = state->buffered_start;
      while (start < state->buffered_end &&
             lonejson__is_json_space(state->io_buffer[start])) {
        start++;
      }
      state->buffered_start = start;
      if (state->buffered_start == state->buffered_end) {
        continue;
      }
      if (state->io_buffer[state->buffered_start] != '{') {
        lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                            0u, 0u, "expected top-level object");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
    }

    {
      size_t consumed;

      status = lonejson__parser_feed_bytes(
          state->parser, state->io_buffer + state->buffered_start,
          state->buffered_end - state->buffered_start, &consumed, 1);
      state->buffered_start += consumed;
    }
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      stream->error = state->parser->error;
      state->object_in_progress = 0;
      state->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_ERROR;
    }
    if (lonejson__parser_root_complete(state->parser)) {
      if (state->parser->error.code == LONEJSON_STATUS_OK &&
          !state->parser->error.truncated) {
        stream->error.code = LONEJSON_STATUS_OK;
        stream->error.line = 0u;
        stream->error.column = 0u;
        stream->error.offset = 0u;
        stream->error.system_errno = 0;
        stream->error.truncated = 0;
        stream->error.message[0] = '\0';
      } else {
        stream->error = state->parser->error;
      }
      state->object_in_progress = 0;
      state->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_OBJECT;
    }
  }
}

const lonejson_error *lonejson_stream_error(const lonejson_stream *stream) {
  return stream ? &stream->error : NULL;
}

void lonejson_stream_close(lonejson_stream *stream) {
  lonejson__stream_state *state;

  if (stream == NULL) {
    return;
  }
  state = lonejson__stream_state_mut(stream);
  lonejson_parser_destroy(state->parser);
  if (state->owns_fp && state->fp != NULL) {
    fclose(state->fp);
  }
  if (state->owns_fd && state->fd >= 0) {
    close(state->fd);
  }
  lonejson__runtime_free_owned_config(&state->runtime_storage);
  lonejson__buffer_free(&state->allocator, state, state->self_alloc_size);
}

static lonejson_array_stream *
lonejson__runtime_array_stream_open_reader(lonejson *runtime, const char *path,
                                           lonejson_reader_fn reader,
                                           void *user, lonejson_error *error) {
  return lonejson_array_stream_open_reader(runtime, path, reader, user, error);
}

static lonejson_array_stream *
lonejson__runtime_array_stream_open_filep(lonejson *runtime, const char *path,
                                          FILE *fp, lonejson_error *error) {
  return lonejson_array_stream_open_filep(runtime, path, fp, error);
}

static lonejson_array_stream *lonejson__runtime_array_stream_open_path(
    lonejson *runtime, const char *array_path, const char *path,
    lonejson_error *error) {
  return lonejson_array_stream_open_path(runtime, array_path, path, error);
}

static lonejson_array_stream *
lonejson__runtime_array_stream_open_fd(lonejson *runtime, const char *path,
                                       int fd, lonejson_error *error) {
  return lonejson_array_stream_open_fd(runtime, path, fd, error);
}

static lonejson_array_stream *
lonejson__runtime_array_stream_open_push(lonejson *runtime, const char *path,
                                         lonejson_error *error) {
  return lonejson_array_stream_open_push(runtime, path, error);
}

static void lonejson__runtime_spooled_init(lonejson *runtime,
                                           lonejson_spooled *value) {
  lonejson_spooled_init(runtime, value);
}

static void
lonejson__runtime_spooled_init_class(lonejson *runtime, lonejson_spooled *value,
                                     lonejson_spool_class spool_class) {
  lonejson_spooled_init_class(runtime, value, spool_class);
}

static void lonejson__runtime_json_value_init(lonejson *runtime,
                                              lonejson_json_value *value) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;

  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  lonejson_json_value_init_with_allocator(
      value, runtime_state != NULL ? runtime_state->config.allocator : NULL);
  if (value != NULL && runtime_state != NULL) {
    value->parse_visitor_limits = runtime_state->value_limits;
    value->runtime_parse_visitor_limits = runtime_state->value_limits;
    value->parse_limits_follow_runtime = 1;
  }
  lonejson__runtime_borrow_release(&borrow);
}

static void lonejson__runtime_json_value_cleanup(lonejson *runtime,
                                                 lonejson_json_value *value) {
  (void)runtime;
  lonejson_json_value_cleanup(value);
}

static lonejson_status lonejson__runtime_array_rewrite_reader(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_reader(runtime, selector, reader, reader_user,
                                       sink, sink_user, options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_reader_to_filep(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, FILE *output,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_reader_to_filep(
      runtime, selector, reader, reader_user, output, options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_reader_to_fd(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, int fd,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_reader_to_fd(runtime, selector, reader,
                                             reader_user, fd, options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_filep(
    lonejson *runtime, const char *selector, FILE *fp, lonejson_sink_fn sink,
    void *sink_user, const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_filep(runtime, selector, fp, sink, sink_user,
                                      options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_filep_to_filep(
    lonejson *runtime, const char *selector, FILE *input, FILE *output,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_filep_to_filep(runtime, selector, input, output,
                                               options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_fd(
    lonejson *runtime, const char *selector, int fd, lonejson_sink_fn sink,
    void *sink_user, const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_fd(runtime, selector, fd, sink, sink_user,
                                   options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_fd_to_fd(
    lonejson *runtime, const char *selector, int input_fd, int output_fd,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_fd_to_fd(runtime, selector, input_fd, output_fd,
                                         options, error);
}

static lonejson_status lonejson__runtime_array_rewrite_path(
    lonejson *runtime, const char *selector, const char *input_path,
    const char *output_path,
    const struct lonejson_array_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_array_rewrite_path(runtime, selector, input_path, output_path,
                                     options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_reader(runtime, reader, reader_user, sink,
                                       sink_user, options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_buffer(
    lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
    void *sink_user, const struct lonejson_value_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_buffer(runtime, data, len, sink, sink_user,
                                       options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_filep(
    lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_filep(runtime, fp, sink, sink_user, options,
                                      error);
}

static lonejson_status lonejson__runtime_value_rewrite_fd(
    lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_fd(runtime, fd, sink, sink_user, options,
                                   error);
}

static lonejson_status lonejson__runtime_value_rewrite_path(
    lonejson *runtime, const char *input_path, const char *output_path,
    const struct lonejson_value_rewrite_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_path(runtime, input_path, output_path, options,
                                     error);
}

static lonejson_status lonejson__runtime_value_rewrite_selector_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_reader(
      runtime, reader, reader_user, sink, sink_user, options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_selector_buffer(
    lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
    void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_buffer(runtime, data, len, sink,
                                                sink_user, options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_selector_filep(
    lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_filep(runtime, fp, sink, sink_user,
                                               options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_selector_fd(
    lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_fd(runtime, fd, sink, sink_user,
                                            options, error);
}

static lonejson_status lonejson__runtime_value_rewrite_selector_path(
    lonejson *runtime, const char *input_path, const char *output_path,
    const struct lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_path(runtime, input_path, output_path,
                                              options, error);
}

static lonejson_status
lonejson__runtime_generator_init(lonejson *runtime,
                                 lonejson_generator *generator,
                                 const lonejson_map *map, const void *src) {
  return lonejson_generator_init(runtime, generator, map, src);
}

static lonejson_status
lonejson__runtime_writer_init_sink(lonejson *runtime, lonejson_writer *writer,
                                   lonejson_sink_fn sink, void *sink_user,
                                   lonejson_error *error) {
  return lonejson_writer_init_sink(runtime, writer, sink, sink_user, error);
}

#ifdef LONEJSON_WITH_CURL
static lonejson_status
lonejson__runtime_curl_parse_init(lonejson *runtime, lonejson_curl_parse *ctx,
                                  const lonejson_map *map, void *dst) {
  return lonejson_curl_parse_init(ctx, runtime, map, dst);
}

static lonejson_status lonejson__runtime_curl_array_parse_init(
    lonejson *runtime, lonejson_curl_array_parse *ctx, const char *path,
    const lonejson_map *map, void *dst, lonejson_array_stream_item_fn callback,
    void *user) {
  return lonejson_curl_array_parse_init(ctx, runtime, path, map, dst, callback,
                                        user);
}

static lonejson_status lonejson__runtime_curl_string_array_parse_init(
    lonejson *runtime, lonejson_curl_string_array_parse *ctx, const char *path,
    const lonejson_array_stream_string_handler *handler, void *user) {
  return lonejson_curl_string_array_parse_init(ctx, runtime, path, handler,
                                               user);
}

static lonejson_status lonejson__runtime_curl_string_items_parse_init(
    lonejson *runtime, lonejson_curl_string_items_parse *ctx, const char *path,
    lonejson_array_stream_string_fn callback, void *user) {
  return lonejson_curl_string_items_parse_init(ctx, runtime, path, callback,
                                               user);
}

static lonejson_status
lonejson__runtime_curl_upload_init(lonejson *runtime, lonejson_curl_upload *ctx,
                                   const lonejson_map *map, const void *src) {
  return lonejson_curl_upload_init(ctx, runtime, map, src);
}
#ifdef LONEJSON_WITH_OIDC
static lonejson_status lonejson__runtime_oidc_jwks_cache_parse_init(
    lonejson *runtime, lonejson_oidc_jwks_cache_parse *ctx,
    lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  return lonejson_oidc_jwks_cache_parse_init(ctx, runtime, cache, policy);
}
#endif
#endif

lonejson_status lonejson_validate_buffer(lonejson *runtime, const void *data,
                                         size_t len, lonejson_error *error) {
  lonejson_status status;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;

  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer is required");
  }
  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  if (LONEJSON__UNLIKELY(runtime_state == NULL)) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "lonejson runtime is required");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  lonejson__parser_init_state(
      &parser_storage, NULL, NULL, &runtime_state->parse_options, runtime_state,
      1, 0, 0, 0u, parser_workspace, sizeof(parser_workspace));
  status = lonejson_parser_feed(&parser_storage, data, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson_parser_finish(&parser_storage);
  }
  if (error != NULL) {
    *error = parser_storage.error;
  }
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

lonejson_status lonejson_validate_cstr(lonejson *runtime, const char *json,
                                       lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  return lonejson_validate_buffer(runtime, json, strlen(json), error);
}

lonejson_status lonejson_validate_reader(lonejson *runtime,
                                         lonejson_reader_fn reader, void *user,
                                         lonejson_error *error) {
  lonejson_parser *parser;
  unsigned char buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson_status status = LONEJSON_STATUS_OK;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;

  if (reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader callback is required");
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, NULL, NULL, &runtime_state->parse_options,
                              runtime_state, 1, 0, 0, 0u, parser_workspace,
                              sizeof(parser_workspace));
  lonejson__clear_error(error);
  for (;;) {
    lonejson_read_result chunk = reader(user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      parser->error.system_errno = chunk.error_code;
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_IO_ERROR, parser->error.offset,
          parser->error.line, parser->error.column, "reader callback failed");
      break;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_parser_feed(parser, buffer, chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        break;
      }
    }
    if (chunk.would_block) {
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_CALLBACK_FAILED, parser->error.offset,
          parser->error.line, parser->error.column, "reader would block");
      break;
    }
    if (chunk.eof) {
      status = lonejson_parser_finish(parser);
      break;
    }
  }
  if (error != NULL) {
    *error = parser->error;
  }
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

lonejson_status lonejson_validate_filep(lonejson *runtime, FILE *fp,
                                        lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_validate_reader(runtime, lonejson__file_reader, fp, error);
}

lonejson_status lonejson_validate_path(lonejson *runtime, const char *path,
                                       lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_validate_filep(runtime, fp, error);
  fclose(fp);
  return status;
}

static lonejson_read_result
lonejson__fd_reader(void *user, unsigned char *buffer, size_t capacity) {
  lonejson_read_result result;
  int fd = *(const int *)user;
  ssize_t got;

  memset(&result, 0, sizeof(result));
  got = read(fd, buffer, capacity);
  if (got < 0) {
    result.error_code = errno;
    return result;
  }
  result.bytes_read = (size_t)got;
  result.eof = (got == 0) ? 1 : 0;
  return result;
}

static lonejson_status lonejson__visit_value_buffer_with_limits(
    const void *data, size_t len, const lonejson_value_visitor *visitor,
    void *user, const lonejson__value_limits *limits,
    const lonejson_allocator *allocator, lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (data == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer and visitor are required");
  }
  memset(&cursor, 0, sizeof(cursor));
  cursor.buffer = (const unsigned char *)data;
  cursor.buffer_len = len;
  status = lonejson__json_visit_cursor(&cursor, allocator, visitor, user, NULL,
                                       limits, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__clear_error(error);
  }
  return status;
}

static lonejson_status lonejson__visit_value_cstr_with_limits(
    const char *json, const lonejson_value_visitor *visitor, void *user,
    const lonejson__value_limits *limits, const lonejson_allocator *allocator,
    lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string and visitor are required");
  }
  return lonejson__visit_value_buffer_with_limits(
      json, strlen(json), visitor, user, limits, allocator, error);
}

static lonejson_status lonejson__visit_value_reader_with_limits(
    lonejson_reader_fn reader, void *reader_user,
    const lonejson_value_visitor *visitor, void *user,
    const lonejson__value_limits *limits, const lonejson_allocator *allocator,
    lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (reader == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader and visitor are required");
  }
  memset(&cursor, 0, sizeof(cursor));
  cursor.reader = reader;
  cursor.reader_user = reader_user;
  status = lonejson__json_visit_cursor(&cursor, allocator, visitor, user, NULL,
                                       limits, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__clear_error(error);
  }
  return status;
}

static lonejson_status lonejson__visit_path_value_buffer_with_limits(
    const void *data, size_t len, const lonejson_path_value_visitor *visitor,
    void *user, const lonejson__value_limits *limits,
    const lonejson_allocator *allocator, lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (data == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer and visitor are required");
  }
  memset(&cursor, 0, sizeof(cursor));
  cursor.buffer = (const unsigned char *)data;
  cursor.buffer_len = len;
  status = lonejson__json_visit_cursor(&cursor, allocator, NULL, user, visitor,
                                       limits, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__clear_error(error);
  }
  return status;
}

static lonejson_status lonejson__visit_path_value_cstr_with_limits(
    const char *json, const lonejson_path_value_visitor *visitor, void *user,
    const lonejson__value_limits *limits, const lonejson_allocator *allocator,
    lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string and visitor are required");
  }
  return lonejson__visit_path_value_buffer_with_limits(
      json, strlen(json), visitor, user, limits, allocator, error);
}

static lonejson_status lonejson__visit_path_value_reader_with_limits(
    lonejson_reader_fn reader, void *reader_user,
    const lonejson_path_value_visitor *visitor, void *user,
    const lonejson__value_limits *limits, const lonejson_allocator *allocator,
    lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (reader == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader and visitor are required");
  }
  memset(&cursor, 0, sizeof(cursor));
  cursor.reader = reader;
  cursor.reader_user = reader_user;
  status = lonejson__json_visit_cursor(&cursor, allocator, NULL, user, visitor,
                                       limits, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__clear_error(error);
  }
  return status;
}

static lonejson_status lonejson__runtime_snapshot_visit_policy(
    lonejson *runtime, lonejson__value_limits *limits,
    lonejson_allocator *allocator_storage, const lonejson_allocator **allocator,
    lonejson_error *error) {
  const lonejson_runtime *runtime_state = NULL;
  const lonejson__runtime_handle *cursor;
  const lonejson_runtime *state_ptr;

  if (limits == NULL || allocator_storage == NULL || allocator == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "visit runtime policy snapshot is required");
  }
  *allocator = NULL;
  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "lonejson runtime is required");
  }
  if (runtime->_runtime_owner_self == runtime &&
      runtime->_runtime_owner_data != NULL) {
    lonejson__runtime_handle *handle =
        &((lonejson__runtime_bundle *)runtime)->live_handle;
    (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_INC(handle);
    if (handle->generation == runtime->_state_token &&
        !LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(handle)) {
      runtime_state = (const lonejson_runtime *)runtime->_runtime_owner_data;
      *limits = runtime_state->value_limits;
      if (runtime_state->config.allocator != NULL) {
        *allocator_storage = runtime_state->allocator_storage;
        *allocator = allocator_storage;
      }
      (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle);
      return LONEJSON_STATUS_OK;
    }
    (void)LONEJSON__RUNTIME_HANDLE_ACTIVE_PINS_DEC(handle);
  } else {
    LONEJSON__RUNTIME_HANDLE_LOCK_ACQUIRE();
    state_ptr = (const lonejson_runtime *)runtime->state;
    cursor = g_lonejson_runtime_handle_live_list;
    while (cursor != NULL) {
      if (cursor->generation == runtime->_state_token &&
          cursor->runtime_state == state_ptr &&
          !LONEJSON__RUNTIME_HANDLE_CLOSING_LOAD(cursor)) {
        runtime_state = cursor->runtime_state;
        break;
      }
      cursor = cursor->next_live;
    }
    if (runtime_state != NULL) {
      *limits = runtime_state->value_limits;
      if (runtime_state->config.allocator != NULL) {
        *allocator_storage = runtime_state->allocator_storage;
        *allocator = allocator_storage;
      }
    }
    LONEJSON__RUNTIME_HANDLE_LOCK_RELEASE();
  }
  if (runtime_state != NULL) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                             0u, "lonejson runtime is required");
}

typedef struct lonejson__value_visitor_stream {
  lonejson_value_visitor_stream api;
  lonejson_value_visitor visitor;
  lonejson_json_value value;
  lonejson_parser parser;
  lonejson_error callback_error;
  size_t allocation_size;
  size_t remaining_bytes;
  int limited;
  int eof;
} lonejson__value_visitor_stream;

lonejson_value_visitor_stream *
lonejson_value_visitor_stream_open(lonejson *runtime,
                                   const lonejson_value_visitor *visitor,
                                   void *user, lonejson_error *error) {
  lonejson__value_visitor_stream *state;
  lonejson__value_limits limits;
  lonejson__value_limits defaults;
  lonejson_allocator storage;
  const lonejson_allocator *allocator;
  lonejson__parse_options options;
  size_t token_bytes;
  size_t allocation_size;
  /* The token scratch reserves three bytes beyond the token itself. */
  const size_t overhead =
      sizeof(*state) + LONEJSON__PARSER_WORKSPACE_SLACK + 3u;

  if (visitor == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 1u, 0u,
                        "value visitor is required");
    return NULL;
  }
  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits, &storage,
                                              &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return NULL;
  }
  defaults = lonejson__default_value_limits();
  if (limits.max_depth == 0u) {
    limits.max_depth = defaults.max_depth;
  }
  if (limits.max_key_bytes == 0u) {
    limits.max_key_bytes = defaults.max_key_bytes;
  }
  if (limits.max_string_bytes == 0u) {
    limits.max_string_bytes = defaults.max_string_bytes;
  }
  if (limits.max_number_bytes == 0u) {
    limits.max_number_bytes = defaults.max_number_bytes;
  }
  token_bytes = limits.max_number_bytes;
  if (token_bytes < sizeof("false") - 1u) {
    token_bytes = sizeof("false") - 1u;
  }
  if (token_bytes > (size_t)-1 - overhead ||
      limits.max_depth >
          ((size_t)-1 - overhead - token_bytes) / sizeof(lonejson_frame)) {
    lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 1u, 0u,
                        "value visitor workspace size overflow");
    return NULL;
  }
  allocation_size =
      overhead + token_bytes + lonejson__frame_bytes(limits.max_depth);
  state = (lonejson__value_visitor_stream *)lonejson__buffer_alloc(
      allocator, allocation_size);
  if (state == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 1u, 0u,
                        "failed to allocate value visitor stream");
    return NULL;
  }
  state->api.push = lonejson_value_visitor_stream_push;
  state->api.finish = lonejson_value_visitor_stream_finish;
  state->api.close = lonejson_value_visitor_stream_close;
  state->visitor = *visitor;
  state->allocation_size = allocation_size;
  state->limited = limits.max_total_bytes != 0u;
  state->remaining_bytes = limits.max_total_bytes;
  state->eof = 0;
  lonejson_json_value_init_with_allocator(&state->value, allocator);
  (void)lonejson_json_value_set_parse_visitor(&state->value, &state->visitor,
                                              user, NULL);
  state->value.parse_visitor_limits = limits;
  options = lonejson__default_parse_options();
  options.allocator = &state->value.allocator;
  options.max_depth = limits.max_depth;
  lonejson__parser_init_state(&state->parser, NULL, NULL, &options, NULL, 1, 0,
                              0, 0u, (unsigned char *)(state + 1),
                              allocation_size - sizeof(*state));
  state->parser.visit_callback_error = &state->callback_error;
  lonejson__parser_set_json_stream_value(&state->parser, &state->value);
  lonejson__clear_error(error);
  return &state->api;
}

static lonejson_status
lonejson__value_visitor_drain_number(lonejson__value_visitor_stream *state) {
  lonejson_parser *parser = &state->parser;
  const lonejson_value_visitor *visitor = &state->visitor;
  void *user = state->value.parse_visitor_user;

  while (!parser->visit_paused &&
         parser->visit_number_phase != LONEJSON__VISIT_NUMBER_NONE) {
    lonejson_status status = LONEJSON_STATUS_OK;

    switch (parser->visit_number_phase) {
    case LONEJSON__VISIT_NUMBER_BEGIN:
      parser->visit_number_phase = LONEJSON__VISIT_NUMBER_CHUNK;
      if (visitor->number_begin != NULL) {
        status = visitor->number_begin(
            user, lonejson__json_value_visit_error_output(parser));
      }
      break;
    case LONEJSON__VISIT_NUMBER_CHUNK:
      parser->visit_number_phase = LONEJSON__VISIT_NUMBER_END;
      if (visitor->number_chunk != NULL) {
        status = visitor->number_chunk(
            user, parser->token.data, parser->token.len,
            lonejson__json_value_visit_error_output(parser));
      }
      break;
    case LONEJSON__VISIT_NUMBER_END:
      parser->visit_number_phase = LONEJSON__VISIT_NUMBER_NONE;
      if (visitor->number_end != NULL) {
        status = visitor->number_end(
            user, lonejson__json_value_visit_error_output(parser));
      }
      break;
    case LONEJSON__VISIT_NUMBER_NONE:
      break;
    }
    status = lonejson__json_value_visit_status(parser, status);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_visitor_stream_result(
    lonejson_parser *parser, lonejson_status status, lonejson_error *error) {
  if (status != LONEJSON_STATUS_OK) {
    parser->failed = 1;
    if (error != NULL) {
      *error = parser->error;
    }
    return status;
  }
  lonejson__clear_error(error);
  return parser->visit_paused ? LONEJSON_STATUS_PAUSED : LONEJSON_STATUS_OK;
}

lonejson_status
lonejson_value_visitor_stream_push(lonejson_value_visitor_stream *stream,
                                   const void *data, size_t len,
                                   size_t *consumed, lonejson_error *error) {
  lonejson__value_visitor_stream *state =
      (lonejson__value_visitor_stream *)stream;
  const unsigned char *bytes = (const unsigned char *)data;
  size_t available = len;
  lonejson_status status;
  lonejson_parser *parser;

  if (consumed != NULL) {
    *consumed = 0u;
  }
  if (state == NULL || consumed == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "stream, input and consumed are required");
  }
  parser = &state->parser;
  if (parser->failed) {
    return lonejson__value_visitor_stream_result(parser, parser->error.code,
                                                 error);
  }
  if (state->eof) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "value visitor input is already finished");
  }
  if (state->limited && available > state->remaining_bytes) {
    available = state->remaining_bytes;
  }
  parser->visit_paused = 0;
  status = lonejson__value_visitor_drain_number(state);
  while (status == LONEJSON_STATUS_OK && !parser->visit_paused &&
         *consumed < available) {
    size_t used = 0u;

    status = lonejson__parser_feed_bytes(parser, bytes + *consumed,
                                         available - *consumed, &used, 0);
    *consumed += used;
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson__value_visitor_drain_number(state);
    }
  }
  if (state->limited) {
    state->remaining_bytes -= *consumed;
    if (status == LONEJSON_STATUS_OK && !parser->visit_paused &&
        available < len) {
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "JSON value exceeds maximum total byte limit");
    }
  }
  return lonejson__value_visitor_stream_result(parser, status, error);
}

lonejson_status
lonejson_value_visitor_stream_finish(lonejson_value_visitor_stream *stream,
                                     lonejson_error *error) {
  lonejson__value_visitor_stream *state =
      (lonejson__value_visitor_stream *)stream;
  lonejson_parser *parser;
  lonejson_status status;

  if (state == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "value visitor stream is required");
  }
  parser = &state->parser;
  if (parser->failed) {
    return lonejson__value_visitor_stream_result(parser, parser->error.code,
                                                 error);
  }
  parser->visit_paused = 0;
  if (!state->eof) {
    state->eof = 1;
    status = lonejson_parser_finish(parser);
    if (status != LONEJSON_STATUS_OK) {
      return lonejson__value_visitor_stream_result(parser, status, error);
    }
  }
  status = lonejson__value_visitor_drain_number(state);
  return lonejson__value_visitor_stream_result(parser, status, error);
}

void lonejson_value_visitor_stream_close(
    lonejson_value_visitor_stream *stream) {
  lonejson__value_visitor_stream *state =
      (lonejson__value_visitor_stream *)stream;

  if (state != NULL) {
    lonejson_parser_destroy(&state->parser);
    lonejson_json_value_cleanup(&state->value);
    lonejson__buffer_free(&state->parser.allocator, state,
                          state->allocation_size);
  }
}

lonejson_status
lonejson_visit_value_buffer(lonejson *runtime, const void *data, size_t len,
                            const lonejson_value_visitor *visitor, void *user,
                            lonejson_error *error) {
  lonejson__value_limits limits;
  lonejson_allocator allocator_storage;
  const lonejson_allocator *allocator = NULL;

  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits,
                                              &allocator_storage, &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson__visit_value_buffer_with_limits(data, len, visitor, user,
                                                  &limits, allocator, error);
}

lonejson_status lonejson_visit_value_cstr(lonejson *runtime, const char *json,
                                          const lonejson_value_visitor *visitor,
                                          void *user, lonejson_error *error) {
  lonejson__value_limits limits;
  lonejson_allocator allocator_storage;
  const lonejson_allocator *allocator = NULL;

  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits,
                                              &allocator_storage, &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson__visit_value_cstr_with_limits(json, visitor, user, &limits,
                                                allocator, error);
}

lonejson_status lonejson_visit_value_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_value_visitor *visitor, void *user, lonejson_error *error) {
  lonejson__value_limits limits;
  lonejson_allocator allocator_storage;
  const lonejson_allocator *allocator = NULL;

  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits,
                                              &allocator_storage, &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson__visit_value_reader_with_limits(
      reader, reader_user, visitor, user, &limits, allocator, error);
}

lonejson_status
lonejson_visit_value_filep(lonejson *runtime, FILE *fp,
                           const lonejson_value_visitor *visitor, void *user,
                           lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer and visitor are required");
  }
  return lonejson_visit_value_reader(runtime, lonejson__file_reader, fp,
                                     visitor, user, error);
}

lonejson_status lonejson_visit_value_path(lonejson *runtime, const char *path,
                                          const lonejson_value_visitor *visitor,
                                          void *user, lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path and visitor are required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_visit_value_filep(runtime, fp, visitor, user, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_visit_value_fd(lonejson *runtime, int fd,
                                        const lonejson_value_visitor *visitor,
                                        void *user, lonejson_error *error) {
  if (fd < 0 || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "fd and visitor are required");
  }
  return lonejson_visit_value_reader(runtime, lonejson__fd_reader, &fd, visitor,
                                     user, error);
}

lonejson_status
lonejson_visit_path_value_buffer(lonejson *runtime, const void *data,
                                 size_t len,
                                 const lonejson_path_value_visitor *visitor,
                                 void *user, lonejson_error *error) {
  lonejson__value_limits limits;
  lonejson_allocator allocator_storage;
  const lonejson_allocator *allocator = NULL;

  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits,
                                              &allocator_storage, &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson__visit_path_value_buffer_with_limits(
      data, len, visitor, user, &limits, allocator, error);
}

lonejson_status
lonejson_visit_path_value_cstr(lonejson *runtime, const char *json,
                               const lonejson_path_value_visitor *visitor,
                               void *user, lonejson_error *error) {
  lonejson__value_limits limits;
  lonejson_allocator allocator_storage;
  const lonejson_allocator *allocator = NULL;

  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits,
                                              &allocator_storage, &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson__visit_path_value_cstr_with_limits(json, visitor, user,
                                                     &limits, allocator, error);
}

lonejson_status
lonejson_visit_path_value_reader(lonejson *runtime, lonejson_reader_fn reader,
                                 void *reader_user,
                                 const lonejson_path_value_visitor *visitor,
                                 void *user, lonejson_error *error) {
  lonejson__value_limits limits;
  lonejson_allocator allocator_storage;
  const lonejson_allocator *allocator = NULL;

  if (lonejson__runtime_snapshot_visit_policy(runtime, &limits,
                                              &allocator_storage, &allocator,
                                              error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson__visit_path_value_reader_with_limits(
      reader, reader_user, visitor, user, &limits, allocator, error);
}

lonejson_status
lonejson_visit_path_value_filep(lonejson *runtime, FILE *fp,
                                const lonejson_path_value_visitor *visitor,
                                void *user, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer and visitor are required");
  }
  return lonejson_visit_path_value_reader(runtime, lonejson__file_reader, fp,
                                          visitor, user, error);
}

lonejson_status
lonejson_visit_path_value_path(lonejson *runtime, const char *path,
                               const lonejson_path_value_visitor *visitor,
                               void *user, lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path and visitor are required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_visit_path_value_filep(runtime, fp, visitor, user, error);
  fclose(fp);
  return status;
}

lonejson_status
lonejson_visit_path_value_fd(lonejson *runtime, int fd,
                             const lonejson_path_value_visitor *visitor,
                             void *user, lonejson_error *error) {
  if (fd < 0 || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "fd and visitor are required");
  }
  return lonejson_visit_path_value_reader(runtime, lonejson__fd_reader, &fd,
                                          visitor, user, error);
}

static lonejson_status lonejson__serialize_sink_with_options(
    const lonejson_map *map, const void *src, lonejson_sink_fn sink, void *user,
    const lonejson__write_options *options, lonejson_error *error) {
  lonejson_status status;
  lonejson__write_state state;

  if (map == NULL || src == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map, source, and sink are required");
  }
  if (!lonejson__map_layout_is_valid(map)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid map layout");
  }
  lonejson__clear_error(error);
  if (options != NULL && options->pretty) {
    state.sink = sink;
    state.user = user;
    state.error = error;
    state.pretty = 1;
    state.depth = 0u;
    status = lonejson__serialize_map_pretty(map, src, &state);
  } else {
    status = lonejson__serialize_map_compact(map, src, sink, user, error);
  }
  return status;
}

lonejson_status lonejson_serialize_sink(lonejson *runtime,
                                        const lonejson_map *map,
                                        const void *src, lonejson_sink_fn sink,
                                        void *user, lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_sink_with_options(map, src, sink, user, &options,
                                                 error);
  return status;
}

static lonejson_status lonejson__serialize_buffer_with_options(
    const lonejson_map *map, const void *src, char *buffer, size_t capacity,
    size_t *needed, const lonejson__write_options *options,
    lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson__write_options local_options;
  lonejson_status status;

  if (buffer == NULL || capacity == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer and capacity are required");
  }
  local_options = options ? *options : lonejson__default_write_options();
  sink.buffer = buffer;
  sink.capacity = capacity;
  sink.length = 0u;
  sink.needed = 0u;
  sink.alloc_size = 0u;
  sink.max_bytes = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  buffer[0] = '\0';
  if (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL &&
      !local_options.pretty) {
    if (map == NULL || src == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "map, source, and sink are required");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      status =
          lonejson__serialize_map_compact_buffer_exact(map, src, &sink, error);
    }
  } else {
    status = lonejson__serialize_sink_with_options(
        map, src,
        (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL)
            ? lonejson__sink_buffer_exact
            : lonejson__sink_buffer,
        &sink, &local_options, error);
  }
  sink.buffer[(sink.length < sink.capacity) ? sink.length
                                            : (sink.capacity - 1u)] = '\0';
  if (needed != NULL) {
    *needed = sink.needed;
  }
  if (status == LONEJSON_STATUS_OK && sink.truncated &&
      local_options.overflow_policy == LONEJSON_OVERFLOW_TRUNCATE) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

lonejson_status lonejson_serialize_buffer(lonejson *runtime,
                                          const lonejson_map *map,
                                          const void *src, char *buffer,
                                          size_t capacity, size_t *needed,
                                          lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_buffer_with_options(map, src, buffer, capacity,
                                                   needed, &options, error);
  return status;
}

static lonejson_status lonejson__serialize_owned_with_options(
    const lonejson_map *map, const void *src, lonejson_owned_buffer *out,
    const lonejson__write_options *options, lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_status status;
  lonejson_allocator resolved;

  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer handle is required");
  }
  lonejson_owned_buffer_init(out);
  memset(&sink, 0, sizeof(sink));
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  sink.allocator = options ? options->allocator : NULL;
  sink.max_bytes = lonejson__write_max_output_bytes(options);
  if ((options == NULL || !options->pretty)) {
    if (map == NULL || src == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "map, source, and sink are required");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      status =
          lonejson__serialize_map_compact_buffer_grow(map, src, &sink, error);
    }
  } else {
    status = lonejson__serialize_sink_with_options(
        map, src, lonejson__sink_grow, &sink, options, error);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__buffer_free(sink.allocator, sink.buffer, sink.alloc_size);
    return status;
  }
  resolved = lonejson__allocator_resolve(sink.allocator);
  out->data = sink.buffer;
  out->len = sink.length;
  out->alloc_size = sink.alloc_size;
  out->allocator = resolved;
  if (out->data != NULL) {
    out->data[out->len] = '\0';
  }
  return status;
}

lonejson_status lonejson_serialize_owned(lonejson *runtime,
                                         const lonejson_map *map,
                                         const void *src,
                                         lonejson_owned_buffer *out,
                                         lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status =
      lonejson__serialize_owned_with_options(map, src, out, &options, error);
  return status;
}

void lonejson_owned_buffer_free(lonejson_owned_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }
  if (buffer->data != NULL) {
    lonejson__buffer_free(&buffer->allocator, buffer->data, buffer->alloc_size);
  }
  lonejson_owned_buffer_init(buffer);
}

lonejson_status lonejson_owned_buffer_sink(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  lonejson_owned_buffer *buffer = (lonejson_owned_buffer *)user;
  char *next;
  size_t required;
  size_t next_cap;

  if (buffer == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "owned buffer sink arguments are invalid");
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  required = buffer->len + len + 1u;
  if (required <= buffer->len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "owned buffer sink size overflow");
  }
  if (buffer->alloc_size < required) {
    next_cap = buffer->alloc_size != 0u ? buffer->alloc_size : 64u;
    while (next_cap < required) {
      size_t doubled = next_cap * 2u;
      if (doubled <= next_cap) {
        next_cap = required;
        break;
      }
      next_cap = doubled;
    }
    next = (char *)lonejson__buffer_realloc(&buffer->allocator, buffer->data,
                                            buffer->alloc_size, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to grow owned buffer sink");
    }
    buffer->data = next;
    buffer->alloc_size = next_cap;
  }
  memcpy(buffer->data + buffer->len, data, len);
  buffer->len += len;
  buffer->data[buffer->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static char *lonejson__serialize_alloc_with_options(
    const lonejson_map *map, const void *src, size_t *out_len,
    const lonejson__write_options *options, lonejson_error *error) {
  lonejson_owned_buffer buffer;
  lonejson__write_options local_options;
  lonejson_status status;

  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  if (options != NULL && options->allocator != NULL &&
      !lonejson__allocator_is_default_family(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "serialize_alloc uses the default allocator only; use "
                        "serialize_owned for custom allocators");
    return NULL;
  }
  local_options = options ? *options : lonejson__default_write_options();
  local_options.allocator = NULL;
  status = lonejson__serialize_owned_with_options(map, src, &buffer,
                                                  &local_options, error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return NULL;
  }
  if (out_len != NULL) {
    *out_len = buffer.len;
  }
  return buffer.data;
}

char *lonejson_serialize_alloc(lonejson *runtime, const lonejson_map *map,
                               const void *src, size_t *out_len,
                               lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  char *result;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return NULL;
  }
  result = lonejson__serialize_alloc_with_options(map, src, out_len, &options,
                                                  error);
  return result;
}

static lonejson_status lonejson__serialize_filep_with_options(
    const lonejson_map *map, const void *src, FILE *fp,
    const lonejson__write_options *options, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson__serialize_sink_with_options(map, src, lonejson__sink_file,
                                               fp, options, error);
}

lonejson_status lonejson_serialize_filep(lonejson *runtime,
                                         const lonejson_map *map,
                                         const void *src, FILE *fp,
                                         lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status =
      lonejson__serialize_filep_with_options(map, src, fp, &options, error);
  return status;
}

static lonejson_status lonejson__serialize_path_with_options(
    const lonejson_map *map, const void *src, const char *path,
    const lonejson__write_options *options, lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s' for writing", path);
  }
  status = lonejson__serialize_filep_with_options(map, src, fp, options, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_serialize_path(lonejson *runtime,
                                        const lonejson_map *map,
                                        const void *src, const char *path,
                                        lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status =
      lonejson__serialize_path_with_options(map, src, path, &options, error);
  return status;
}

static lonejson_status lonejson__serialize_jsonl_sink_with_options(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, const lonejson__write_options *options,
    lonejson_error *error) {
  lonejson_status status;
  lonejson__write_state state;

  if (map == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and sink are required");
  }
  if (count != 0u && items == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "items are required when count is non-zero");
  }
  if (stride == 0u) {
    stride = map->struct_size;
  }
  if (stride < map->struct_size) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "stride must be at least map->struct_size");
  }
  lonejson__clear_error(error);
  if (options != NULL && options->pretty) {
    state.sink = sink;
    state.user = user;
    state.error = error;
    state.pretty = 1;
    state.depth = 0u;
    status =
        lonejson__serialize_jsonl_records(map, items, count, stride, &state);
  } else {
    status = lonejson__serialize_jsonl_records_compact(
        map, items, count, stride, sink, user, error);
  }
  return status;
}

lonejson_status lonejson_serialize_jsonl_sink(
    lonejson *runtime, const lonejson_map *map, const void *items, size_t count,
    size_t stride, lonejson_sink_fn sink, void *user, lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_jsonl_sink_with_options(
      map, items, count, stride, sink, user, &options, error);
  return status;
}

static lonejson_status lonejson__serialize_jsonl_buffer_with_options(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lonejson__write_options *options, lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson__write_options local_options;
  lonejson_status status;

  if (buffer == NULL || capacity == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer and capacity are required");
  }
  local_options = options ? *options : lonejson__default_write_options();
  sink.buffer = buffer;
  sink.capacity = capacity;
  sink.length = 0u;
  sink.needed = 0u;
  sink.alloc_size = 0u;
  sink.max_bytes = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  buffer[0] = '\0';
  if (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL &&
      !local_options.pretty) {
    size_t i;
    const unsigned char *base = (const unsigned char *)items;

    if (map == NULL) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "map and sink are required");
    } else if (count != 0u && items == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "items are required when count is non-zero");
    } else if ((stride != 0u && stride < map->struct_size)) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "stride must be at least map->struct_size");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      if (stride == 0u) {
        stride = map->struct_size;
      }
      status = LONEJSON_STATUS_OK;
      for (i = 0u; i < count; ++i) {
        const void *record = (const void *)(base + (i * stride));
        status = lonejson__serialize_map_compact_buffer_exact(map, record,
                                                              &sink, error);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
        status = lonejson__buffer_emit_exact(&sink, error, "\n", 1u);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
      }
    }
  } else {
    status = lonejson__serialize_jsonl_sink_with_options(
        map, items, count, stride,
        (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL)
            ? lonejson__sink_buffer_exact
            : lonejson__sink_buffer,
        &sink, &local_options, error);
  }
  sink.buffer[(sink.length < sink.capacity) ? sink.length
                                            : (sink.capacity - 1u)] = '\0';
  if (needed != NULL) {
    *needed = sink.needed;
  }
  if (status == LONEJSON_STATUS_OK && sink.truncated &&
      local_options.overflow_policy == LONEJSON_OVERFLOW_TRUNCATE) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

lonejson_status lonejson_serialize_jsonl_buffer(lonejson *runtime,
                                                const lonejson_map *map,
                                                const void *items, size_t count,
                                                size_t stride, char *buffer,
                                                size_t capacity, size_t *needed,
                                                lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_jsonl_buffer_with_options(
      map, items, count, stride, buffer, capacity, needed, &options, error);
  return status;
}

static lonejson_status lonejson__serialize_jsonl_owned_with_options(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_owned_buffer *out, const lonejson__write_options *options,
    lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_status status;
  lonejson_allocator resolved;

  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer handle is required");
  }
  lonejson_owned_buffer_init(out);
  memset(&sink, 0, sizeof(sink));
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  sink.allocator = options ? options->allocator : NULL;
  sink.max_bytes = lonejson__write_max_output_bytes(options);
  if ((options == NULL || !options->pretty)) {
    size_t i;
    const unsigned char *base = (const unsigned char *)items;

    if (map == NULL) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "map and sink are required");
    } else if (count != 0u && items == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "items are required when count is non-zero");
    } else if ((stride != 0u && stride < map->struct_size)) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "stride must be at least map->struct_size");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      if (stride == 0u) {
        stride = map->struct_size;
      }
      status = LONEJSON_STATUS_OK;
      for (i = 0u; i < count; ++i) {
        const void *record = (const void *)(base + (i * stride));
        status = lonejson__serialize_map_compact_buffer_grow(map, record, &sink,
                                                             error);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
        status = lonejson__buffer_emit_grow(&sink, error, "\n", 1u);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
      }
    }
  } else {
    status = lonejson__serialize_jsonl_sink_with_options(
        map, items, count, stride, lonejson__sink_grow, &sink, options, error);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__buffer_free(sink.allocator, sink.buffer, sink.alloc_size);
    return status;
  }
  resolved = lonejson__allocator_resolve(sink.allocator);
  out->data = sink.buffer;
  out->len = sink.length;
  out->alloc_size = sink.alloc_size;
  out->allocator = resolved;
  if (out->data != NULL) {
    out->data[out->len] = '\0';
  }
  return status;
}

lonejson_status lonejson_serialize_jsonl_owned(
    lonejson *runtime, const lonejson_map *map, const void *items, size_t count,
    size_t stride, lonejson_owned_buffer *out, lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_jsonl_owned_with_options(
      map, items, count, stride, out, &options, error);
  return status;
}

static char *lonejson__serialize_jsonl_alloc_with_options(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    size_t *out_len, const lonejson__write_options *options,
    lonejson_error *error) {
  lonejson_owned_buffer buffer;
  lonejson__write_options local_options;
  lonejson_status status;

  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  if (options != NULL && options->allocator != NULL &&
      !lonejson__allocator_is_default_family(options->allocator)) {
    lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "serialize_jsonl_alloc uses the default allocator only; use "
        "serialize_jsonl_owned for custom allocators");
    return NULL;
  }
  local_options = options ? *options : lonejson__default_write_options();
  local_options.allocator = NULL;
  status = lonejson__serialize_jsonl_owned_with_options(
      map, items, count, stride, &buffer, &local_options, error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return NULL;
  }
  if (out_len != NULL) {
    *out_len = buffer.len;
  }
  return buffer.data;
}

char *lonejson_serialize_jsonl_alloc(lonejson *runtime, const lonejson_map *map,
                                     const void *items, size_t count,
                                     size_t stride, size_t *out_len,
                                     lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  char *result;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return NULL;
  }
  result = lonejson__serialize_jsonl_alloc_with_options(
      map, items, count, stride, out_len, &options, error);
  return result;
}

static lonejson_status lonejson__serialize_jsonl_filep_with_options(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    FILE *fp, const lonejson__write_options *options, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson__serialize_jsonl_sink_with_options(
      map, items, count, stride, lonejson__sink_file, fp, options, error);
}

lonejson_status lonejson_serialize_jsonl_filep(lonejson *runtime,
                                               const lonejson_map *map,
                                               const void *items, size_t count,
                                               size_t stride, FILE *fp,
                                               lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_jsonl_filep_with_options(
      map, items, count, stride, fp, &options, error);
  return status;
}

static lonejson_status lonejson__serialize_jsonl_path_with_options(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    const char *path, const lonejson__write_options *options,
    lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s' for writing", path);
  }
  status = lonejson__serialize_jsonl_filep_with_options(
      map, items, count, stride, fp, options, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_serialize_jsonl_path(lonejson *runtime,
                                              const lonejson_map *map,
                                              const void *items, size_t count,
                                              size_t stride, const char *path,
                                              lonejson_error *error) {
  lonejson__write_options options;
  lonejson_allocator allocator_storage;
  lonejson_status status;

  if (lonejson__runtime_snapshot_write_policy(
          runtime, &options, &allocator_storage, error) != LONEJSON_STATUS_OK) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  status = lonejson__serialize_jsonl_path_with_options(
      map, items, count, stride, path, &options, error);
  return status;
}

static lonejson_status lonejson__record_report(lonejson_status status,
                                               lonejson_error *error) {
  lonejson__clear_error(error);
  if (status == LONEJSON_STATUS_OK)
    return status;
  return lonejson__set_error(
      error, status, 0u, 0u, 0u,
      status == LONEJSON_STATUS_ALLOCATION_FAILED
          ? "failed to allocate record traversal workspace"
          : "invalid map, record, or cyclic record storage");
}

lonejson_status lonejson_cleanup(const lonejson_map *map, void *value,
                                 lonejson_error *error) {
  return lonejson__record_report(lonejson__cleanup_map_checked(map, value),
                                 error);
}

static lonejson_status
lonejson__runtime_lifecycle(lonejson *runtime, const lonejson_map *map,
                            void *value, lonejson_error *error, int operation) {
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_status status;
  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  status = lonejson__record_run(
      map, value, operation, NULL,
      runtime_state != NULL ? runtime_state->config.allocator : NULL,
      runtime_state);
  if (runtime_state != NULL)
    lonejson__runtime_borrow_release(&borrow);
  return lonejson__record_report(status, error);
}

lonejson_status lonejson_init(lonejson *runtime, const lonejson_map *map,
                              void *value, lonejson_error *error) {
  return lonejson__runtime_lifecycle(runtime, map, value, error,
                                     LONEJSON__RECORD_INIT);
}
lonejson_status lonejson_reset(lonejson *runtime, const lonejson_map *map,
                               void *value, lonejson_error *error) {
  return lonejson__runtime_lifecycle(runtime, map, value, error,
                                     LONEJSON__RECORD_RESET);
}

int lonejson_field_has_presence(const lonejson_field *field) {
  return lonejson__field_has_presence(field);
}

void lonejson_field_set_presence(void *record, const lonejson_field *field,
                                 int present) {
  lonejson__field_set_presence(record, field, present ? 1 : 0);
}

static void lonejson__record_assign_parser_init(
    const lonejson_runtime *runtime_state, const lonejson_map *map,
    void *record, lonejson_parser *parser, unsigned char *workspace,
    size_t workspace_size) {
  lonejson__parse_options options = runtime_state != NULL
                                        ? runtime_state->parse_options
                                        : lonejson__default_parse_options();
  options.clear_destination = 0;
  lonejson__parser_init_state(parser, map, record, &options, runtime_state, 0,
                              0, 0, 0u, workspace, workspace_size);
}

static lonejson_status lonejson__record_assign_finish(lonejson_parser *parser,
                                                      lonejson_status status,
                                                      lonejson_error *error) {
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_record_assign_null(lonejson *runtime,
                                            const lonejson_map *map,
                                            void *record,
                                            const lonejson_field *field,
                                            lonejson_error *error) {
  lonejson_parser parser;
  unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  void *ptr;
  lonejson_status status;

  if (record == NULL || field == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "record and field are required");
  }
  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  ptr = (unsigned char *)record + field->struct_offset;
  lonejson__record_assign_parser_init(runtime_state, map, record, &parser,
                                      workspace, sizeof(workspace));
  status = lonejson__assign_null(&parser, field, ptr);
  if (status == LONEJSON_STATUS_OK &&
      (field->flags & LONEJSON_FIELD_ACCEPT_NULL) != 0u) {
    lonejson__field_set_presence(record, field, 0);
  }
  lonejson__runtime_borrow_release(&borrow);
  return lonejson__record_assign_finish(&parser, status, error);
}

lonejson_status lonejson_record_assign_string(lonejson *runtime,
                                              const lonejson_map *map,
                                              void *record,
                                              const lonejson_field *field,
                                              const char *data, size_t len,
                                              lonejson_error *error) {
  lonejson_parser parser;
  unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  void *ptr;
  lonejson_status status;

  if (record == NULL || field == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "record, field, and string data are "
                               "required");
  }
  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  ptr = (unsigned char *)record + field->struct_offset;
  lonejson__record_assign_parser_init(runtime_state, map, record, &parser,
                                      workspace, sizeof(workspace));
  status = lonejson__assign_string(&parser, field, ptr, data, len);
  lonejson__runtime_borrow_release(&borrow);
  return lonejson__record_assign_finish(&parser, status, error);
}

lonejson_status lonejson_record_array_append_string(
    lonejson *runtime, const lonejson_map *map, void *record,
    const lonejson_field *field, lonejson_string_array *array, const char *data,
    size_t len, lonejson_error *error) {
  lonejson_parser parser;
  unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  lonejson_status status;

  if (record == NULL || field == NULL || array == NULL ||
      (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "record, field, array, and string data are "
                               "required");
  }
  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  lonejson__record_assign_parser_init(runtime_state, map, record, &parser,
                                      workspace, sizeof(workspace));
  status = lonejson__array_append_string(&parser, field, array, data, len);
  lonejson__runtime_borrow_release(&borrow);
  return lonejson__record_assign_finish(&parser, status, error);
}

#define LONEJSON__RECORD_ARRAY_APPEND_IMPL(api_name, internal_name,            \
                                           array_type, value_type)             \
  lonejson_status api_name(lonejson *runtime, const lonejson_map *map,         \
                           void *record, const lonejson_field *field,          \
                           array_type *array, value_type value,                \
                           lonejson_error *error) {                            \
    lonejson_parser parser;                                                    \
    unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];                      \
    const lonejson_runtime *runtime_state;                                     \
    lonejson__runtime_borrow borrow;                                           \
    lonejson_status status;                                                    \
                                                                               \
    if (record == NULL || field == NULL || array == NULL) {                    \
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,  \
                                 0u, 0u,                                       \
                                 "record, field, and array are required");     \
    }                                                                          \
    runtime_state =                                                            \
        lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);    \
    lonejson__record_assign_parser_init(runtime_state, map, record, &parser,   \
                                        workspace, sizeof(workspace));         \
    status = internal_name(&parser, field, array, value);                      \
    lonejson__runtime_borrow_release(&borrow);                                 \
    return lonejson__record_assign_finish(&parser, status, error);             \
  }

LONEJSON__RECORD_ARRAY_APPEND_IMPL(lonejson_record_array_append_i64,
                                   lonejson__array_append_i64,
                                   lonejson_i64_array, lonejson_int64)
LONEJSON__RECORD_ARRAY_APPEND_IMPL(lonejson_record_array_append_u64,
                                   lonejson__array_append_u64,
                                   lonejson_u64_array, lonejson_uint64)
LONEJSON__RECORD_ARRAY_APPEND_IMPL(lonejson_record_array_append_f64,
                                   lonejson__array_append_f64,
                                   lonejson_f64_array, double)
LONEJSON__RECORD_ARRAY_APPEND_IMPL(lonejson_record_array_append_bool,
                                   lonejson__array_append_bool,
                                   lonejson_bool_array, int)

#undef LONEJSON__RECORD_ARRAY_APPEND_IMPL

void *lonejson_record_object_array_append_slot(lonejson *runtime,
                                               const lonejson_map *map,
                                               void *record,
                                               const lonejson_field *field,
                                               lonejson_object_array *array,
                                               lonejson_error *error) {
  lonejson_parser parser;
  unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];
  const lonejson_runtime *runtime_state;
  lonejson__runtime_borrow borrow;
  void *slot;

  if (record == NULL || field == NULL || array == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "record, field, and array are required");
    return NULL;
  }
  runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  lonejson__record_assign_parser_init(runtime_state, map, record, &parser,
                                      workspace, sizeof(workspace));
  slot = lonejson__object_array_append_slot(&parser, field, array);
  lonejson__runtime_borrow_release(&borrow);
  if (error != NULL) {
    *error = parser.error;
  }
  return slot;
}
