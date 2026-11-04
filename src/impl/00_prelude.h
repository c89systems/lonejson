
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#define LONEJSON__HAS_MSAN 1
#endif
#endif
#ifndef LONEJSON__HAS_MSAN
#define LONEJSON__HAS_MSAN 0
#endif

#if defined(_WIN32)
#include <windows.h>
typedef volatile LONG lonejson__lock_word;
static void lonejson__lock_acquire(lonejson__lock_word *lock_word) {
  while (InterlockedCompareExchange(lock_word, 1L, 0L) != 0L) {
    Sleep(0);
  }
}
static void lonejson__lock_release(lonejson__lock_word *lock_word) {
  (void)InterlockedExchange(lock_word, 0L);
}
static void lonejson__lock_wait_yield(void) { Sleep(0); }
#elif defined(__GNUC__) || defined(__clang__)
#include <sched.h>
typedef volatile int lonejson__lock_word;
static void lonejson__lock_acquire(lonejson__lock_word *lock_word) {
  while (__sync_lock_test_and_set(lock_word, 1)) {
    while (*lock_word) {
    }
  }
}
static void lonejson__lock_release(lonejson__lock_word *lock_word) {
  __sync_lock_release(lock_word);
}
static void lonejson__lock_wait_yield(void) { (void)sched_yield(); }
#else
#error "lonejson runtime synchronization requires compiler atomic primitives"
#endif

static lonejson__lock_word g_lonejson_allocator_stats_lock = 0;

typedef enum lonejson_container_kind {
  LONEJSON_CONTAINER_OBJECT = 1,
  LONEJSON_CONTAINER_ARRAY = 2
} lonejson_container_kind;

typedef enum lonejson_frame_state {
  LONEJSON_FRAME_OBJECT_KEY_OR_END = 1,
  LONEJSON_FRAME_OBJECT_COLON = 2,
  LONEJSON_FRAME_OBJECT_VALUE = 3,
  LONEJSON_FRAME_OBJECT_COMMA_OR_END = 4,
  LONEJSON_FRAME_ARRAY_VALUE_OR_END = 5,
  LONEJSON_FRAME_ARRAY_COMMA_OR_END = 6
} lonejson_frame_state;

typedef enum lonejson_lex_mode {
  LONEJSON_LEX_NONE = 0,
  LONEJSON_LEX_STRING,
  LONEJSON_LEX_NUMBER,
  LONEJSON_LEX_TRUE,
  LONEJSON_LEX_FALSE,
  LONEJSON_LEX_NULL
} lonejson_lex_mode;

typedef enum lonejson_string_capture_mode {
  LONEJSON_STRING_CAPTURE_TOKEN = 0,
  LONEJSON_STRING_CAPTURE_DIRECT = 1,
  LONEJSON_STRING_CAPTURE_STREAM = 2,
  LONEJSON_STRING_CAPTURE_JSON_VISITOR = 3,
  LONEJSON_STRING_CAPTURE_DISCARD = 4
} lonejson_string_capture_mode;

typedef struct lonejson_parser lonejson_parser;
typedef struct lonejson_runtime lonejson_runtime;
typedef struct lonejson__runtime_handle lonejson__runtime_handle;
struct lonejson__runtime_bundle;

static const lonejson_runtime *
lonejson__runtime_data_const(const lonejson *runtime);
static lonejson_runtime *lonejson__runtime_data_mut(lonejson *runtime);

typedef struct lonejson_scratch {
  char *data;
  size_t len;
  size_t cap;
} lonejson_scratch;

typedef struct lonejson_frame {
  lonejson_container_kind kind;
  lonejson_frame_state state;
  const lonejson_map *map;
  void *object_ptr;
  const lonejson_field *field;
  const lonejson_field *pending_field;
  lonejson_uint64 seen_mask;
  unsigned char *seen_fields;
  size_t seen_bytes;
  size_t next_field_hint;
  size_t required_remaining;
  int after_comma;
} lonejson_frame;

typedef struct lonejson__json_path_frame {
  char inline_key[64];
  char index_text[32];
  char *key;
  size_t key_len;
  size_t key_cap;
  size_t next_index;
  int key_heap;
  int container_kind;
} lonejson__json_path_frame;

typedef struct lonejson__json_stream_path_state {
  lonejson_path_segment *segments;
  lonejson__json_path_frame *frames;
  size_t depth;
  size_t capacity;
} lonejson__json_stream_path_state;

static size_t lonejson__format_size_decimal(char *out, size_t out_size,
                                            size_t value) {
  char reversed[sizeof(size_t) * 3u + 1u];
  size_t len = 0u;
  size_t i;

  do {
    reversed[len] = (char)('0' + (value % 10u));
    value /= 10u;
    ++len;
  } while (value != 0u && len < sizeof(reversed));

  if (out_size <= len) {
    return len;
  }
  for (i = 0u; i < len; ++i) {
    out[i] = reversed[len - 1u - i];
  }
  out[len] = '\0';
  return len;
}

typedef union lonejson__parser_workspace_align_union {
  lonejson_frame frame;
  void *ptr;
  lonejson_uint64 u64;
  double f64;
} lonejson__parser_workspace_align_union;

typedef struct lonejson__parser_workspace_align_probe {
  char c;
  lonejson__parser_workspace_align_union value;
} lonejson__parser_workspace_align_probe;

#define LONEJSON__PARSER_WORKSPACE_ALIGNMENT                                   \
  offsetof(lonejson__parser_workspace_align_probe, value)
#define LONEJSON__PARSER_WORKSPACE_SLACK                                       \
  (LONEJSON__PARSER_WORKSPACE_ALIGNMENT - 1u)

#define LONEJSON__MAP_MASK_CACHE_SIZE 2u
#define LONEJSON__SPOOLED_MAGIC 0x4C4A5350u
#define LONEJSON__JSON_VALUE_MAGIC 0x4C4A4A56u
#define LONEJSON__STRING_ARRAY_STREAM_MAGIC 0x4C4A4153u
#define LONEJSON__MAPPED_ARRAY_STREAM_MAGIC 0x4C4A4D53u
#define LONEJSON__SPACES_64                                                    \
  "                                                                "

#if defined(__GNUC__) || defined(__clang__)
#if defined(__OPTIMIZE__)
#define LONEJSON__INLINE __inline__ __attribute__((always_inline))
#else
/* Forced inlining at -O0 duplicates each helper's stack temporaries in callers.
 * Recursive serializers must also fit the default 128 KiB musl worker stack. */
#define LONEJSON__INLINE __inline__
#endif
#define LONEJSON__NOINLINE __attribute__((noinline))
#define LONEJSON__COLD __attribute__((cold))
#define LONEJSON__HOT __attribute__((hot))
#define LONEJSON__LIKELY(expr) __builtin_expect(!!(expr), 1)
#define LONEJSON__UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define LONEJSON__TEXT_LEN(text)                                               \
  (__builtin_constant_p(text) ? (sizeof(text) - 1u) : strlen(text))
#else
#define LONEJSON__INLINE
#define LONEJSON__NOINLINE
#define LONEJSON__COLD
#define LONEJSON__HOT
#define LONEJSON__LIKELY(expr) (expr)
#define LONEJSON__UNLIKELY(expr) (expr)
#define LONEJSON__TEXT_LEN(text) strlen(text)
#endif

#if defined(LONEJSON_TEST_FORCE_NO_TLS)
#define LONEJSON__HAS_TLS 0
#define LONEJSON__TLS
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LONEJSON__HAS_TLS 1
#define LONEJSON__TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
#define LONEJSON__HAS_TLS 1
#define LONEJSON__TLS __thread
#else
#define LONEJSON__HAS_TLS 0
#define LONEJSON__TLS
#endif

void lonejson_json_value_init_with_allocator(
    lonejson_json_value *value, const lonejson_allocator *allocator);
void lonejson_spooled_init_with_allocator(
    lonejson_spooled *value, const lonejson__spool_options *options,
    const lonejson_allocator *allocator);
void lonejson_source_cleanup(lonejson_source *value);
void lonejson_source_reset(lonejson_source *value);
lonejson_status lonejson_source_set_file(lonejson_source *value, FILE *fp,
                                         lonejson_error *error);
lonejson_status lonejson_source_set_fd(lonejson_source *value, int fd,
                                       lonejson_error *error);
lonejson_status lonejson_source_set_path(lonejson_source *value,
                                         const char *path,
                                         lonejson_error *error);
lonejson_status lonejson_source_write_to_sink(const lonejson_source *value,
                                              lonejson_sink_fn sink, void *user,
                                              lonejson_error *error);
int lonejson_source_is_rewindable(const lonejson_source *value);
void lonejson_json_value_reset(lonejson_json_value *value);
void lonejson_json_value_cleanup(lonejson_json_value *value);
lonejson_status lonejson_json_value_set_buffer(lonejson_json_value *value,
                                               const void *data, size_t len,
                                               lonejson_error *error);
lonejson_status lonejson_json_value_set_parse_sink(lonejson_json_value *value,
                                                   lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error);
lonejson_status
lonejson_json_value_set_parse_visitor(lonejson_json_value *value,
                                      const lonejson_value_visitor *visitor,
                                      void *user, lonejson_error *error);
lonejson_status lonejson_json_value_set_parse_path_visitor(
    lonejson_json_value *value, const lonejson_path_value_visitor *visitor,
    void *user, lonejson_error *error);
lonejson_status
lonejson_json_value_enable_parse_capture(lonejson_json_value *value,
                                         lonejson_error *error);
lonejson_status lonejson_json_value_set_reader(lonejson_json_value *value,
                                               lonejson_reader_fn reader,
                                               void *user,
                                               lonejson_error *error);
lonejson_status lonejson_json_value_set_file(lonejson_json_value *value,
                                             FILE *fp, lonejson_error *error);
lonejson_status lonejson_json_value_set_fd(lonejson_json_value *value, int fd,
                                           lonejson_error *error);
lonejson_status lonejson_json_value_set_path(lonejson_json_value *value,
                                             const char *path,
                                             lonejson_error *error);
lonejson_status
lonejson_json_value_write_to_sink(const lonejson_json_value *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error);
int lonejson_json_value_is_rewindable(const lonejson_json_value *value);
void lonejson_spooled_reset(lonejson_spooled *value);
void lonejson_spooled_cleanup(lonejson_spooled *value);
size_t lonejson_spooled_size(const lonejson_spooled *value);
int lonejson_spooled_spilled(const lonejson_spooled *value);
lonejson_status lonejson_spooled_append(lonejson_spooled *value,
                                        const void *data, size_t len,
                                        lonejson_error *error);
lonejson_status lonejson_spooled_rewind(lonejson_spooled *value,
                                        lonejson_error *error);
lonejson_read_result lonejson_spooled_read(lonejson_spooled *value,
                                           unsigned char *buffer,
                                           size_t capacity);
lonejson_status lonejson_spooled_write_to_sink(const lonejson_spooled *value,
                                               lonejson_sink_fn sink,
                                               void *user,
                                               lonejson_error *error);
static void lonejson__source_assign_methods(lonejson_source *value);
static void lonejson__json_value_assign_methods(lonejson_json_value *value);
static void lonejson__spooled_assign_methods(lonejson_spooled *value);
static const lonejson_json_value_methods g_lonejson_json_value_methods;

static LONEJSON__INLINE unsigned lonejson__init_cookie(const void *ptr,
                                                       unsigned base_magic) {
  uintptr_t bits = (uintptr_t)ptr;
  return base_magic ^ (unsigned)(bits >> 4u) ^ (unsigned)(bits >> 9u);
}

struct lonejson_parser {
  lonejson_frame *frames;
  unsigned char *workspace;
  size_t workspace_size;
  size_t workspace_top;
  size_t workspace_peak;
  size_t frame_count;
  size_t self_alloc_size;
  int validate_only;
  int owns_self;
  int root_started;
  int root_finished;
  int failed;
  lonejson_lex_mode lex_mode;
  lonejson_string_capture_mode string_capture_mode;
  int lex_is_key;
  int lex_escape;
  lonejson_uint32 unicode_pending_high;
  int unicode_digits_needed;
  lonejson_uint32 unicode_accum;
  int stream_value_active;
  const lonejson_field *stream_field;
  void *stream_ptr;
  unsigned char stream_base64_quad[4];
  size_t stream_base64_quad_len;
  int stream_base64_saw_padding;
  int direct_string_active;
  char *direct_string_ptr;
  char **direct_string_dst;
  char *direct_string_commit_dst;
  size_t direct_string_len;
  size_t direct_string_capacity;
  int direct_string_owned;
  int direct_string_truncated;
  lonejson_overflow_policy direct_string_overflow_policy;
  const lonejson_field *direct_string_field;
  lonejson_scratch token;
  const lonejson_map *required_mask_maps[LONEJSON__MAP_MASK_CACHE_SIZE];
  lonejson_uint64 required_masks[LONEJSON__MAP_MASK_CACHE_SIZE];
  lonejson_json_value *json_stream_value;
  int json_stream_active;
  int json_stream_parse_visitor_active;
  int json_stream_visit_active;
  int json_stream_path_visit_active;
  int json_stream_sink_active;
  size_t json_stream_depth;
  size_t json_stream_total_bytes;
  size_t json_stream_text_bytes;
  int json_stream_text_is_key;
  lonejson__json_stream_path_state *json_stream_path;
  size_t parse_alloc_bytes_live;
  int root_map_may_allocate;
  lonejson_uint64 root_map_adopt_mask;
  const lonejson_map *root_map;
  void *root_dst;
  const lonejson_runtime *runtime;
  lonejson__parse_options options;
  lonejson_error error;
  lonejson_allocator allocator;
};

struct lonejson_runtime {
  lonejson *api_owner;
  lonejson_config config;
  lonejson_allocator allocator_storage;
  char *owned_temp_dirs[3];
  void *owned_fixed_string_scratch;
  lonejson__parse_options parse_options;
  lonejson__value_limits value_limits;
  lonejson__write_options write_options;
  lonejson__spool_options spool_options[3];
#ifdef LONEJSON_WITH_JWT
  lonejson_auth_provider auth_provider;
  int has_auth_provider;
#endif
#ifdef LONEJSON_WITH_OIDC
  lonejson_http_provider http_provider;
  int has_http_provider;
#endif
};

struct lonejson__runtime_handle {
  lonejson_runtime *runtime_state;
  lonejson *api_owner;
  unsigned int generation;
  unsigned int active_pins;
  int closing;
  lonejson__runtime_handle *next_live;
};

typedef struct lonejson__runtime_borrow {
  const lonejson_runtime *runtime;
  lonejson__runtime_handle *handle;
} lonejson__runtime_borrow;

typedef struct lonejson__runtime_bundle {
  lonejson api;
  lonejson_runtime state;
  lonejson__runtime_handle live_handle;
} lonejson__runtime_bundle;

static const lonejson_runtime *
lonejson__runtime_handle_lookup_const(const lonejson *runtime);
static lonejson_runtime *lonejson__runtime_handle_lookup_mut(lonejson *runtime);
static const lonejson_runtime *
lonejson__runtime_borrow_const(const lonejson *runtime,
                               lonejson__runtime_borrow *borrow);
static void lonejson__runtime_borrow_release(lonejson__runtime_borrow *borrow);

static LONEJSON__INLINE const lonejson_runtime *
lonejson__runtime_data_const(const lonejson *runtime) {
  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return NULL;
  }
  if (runtime->_runtime_owner_self == runtime &&
      runtime->_runtime_owner_data != NULL) {
    return (const lonejson_runtime *)runtime->_runtime_owner_data;
  }
  return lonejson__runtime_handle_lookup_const(runtime);
}

static LONEJSON__INLINE lonejson_runtime *
lonejson__runtime_data_mut(lonejson *runtime) {
  if (runtime == NULL || runtime->state == NULL ||
      runtime->_state_token == 0u) {
    return NULL;
  }
  if (runtime->_runtime_owner_self == runtime &&
      runtime->_runtime_owner_data != NULL) {
    return (lonejson_runtime *)runtime->_runtime_owner_data;
  }
  return lonejson__runtime_handle_lookup_mut(runtime);
}

typedef struct lonejson_buffer_sink {
  char *buffer;
  size_t capacity;
  size_t length;
  size_t needed;
  size_t alloc_size;
  size_t max_bytes;
  lonejson_overflow_policy policy;
  int truncated;
  const lonejson_allocator *allocator;
} lonejson_buffer_sink;

typedef enum lonejson_stream_source_kind {
  LONEJSON_STREAM_SOURCE_READER = 1,
  LONEJSON_STREAM_SOURCE_FILE = 2,
  LONEJSON_STREAM_SOURCE_FD = 3,
  LONEJSON_STREAM_SOURCE_MEMORY = 4
} lonejson_stream_source_kind;

typedef struct lonejson__stream_state {
  lonejson_reader_fn reader;
  void *reader_user;
  FILE *fp;
  int fd;
  const unsigned char *memory_input;
  size_t memory_input_len;
  size_t memory_input_offset;
  int owns_fp;
  int owns_fd;
  int saw_eof;
  int object_in_progress;
  size_t buffered_start;
  size_t buffered_end;
  size_t self_alloc_size;
  lonejson_stream_source_kind source_kind;
  void *current_dst;
  void *prepared_dst;
  lonejson_parser *parser;
  const lonejson_map *map;
  const lonejson_runtime *runtime;
  lonejson_runtime runtime_storage;
  lonejson__parse_options options;
  unsigned char io_buffer[LONEJSON_STREAM_BUFFER_SIZE];
  lonejson_allocator allocator;
  lonejson_stream public;
} lonejson__stream_state;

typedef struct lonejson__byte_buffer {
  char *data;
  size_t len;
  size_t cap;
} lonejson__byte_buffer;

typedef enum lonejson_array_stream_source_kind {
  LONEJSON_ARRAY_STREAM_SOURCE_READER = 1,
  LONEJSON_ARRAY_STREAM_SOURCE_FILE = 2,
  LONEJSON_ARRAY_STREAM_SOURCE_FD = 3,
  LONEJSON_ARRAY_STREAM_SOURCE_PUSH = 4
} lonejson_array_stream_source_kind;

typedef enum lonejson_array_stream_state {
  LONEJSON_ARRAY_STREAM_STATE_INIT = 0,
  LONEJSON_ARRAY_STREAM_STATE_IN_ARRAY = 1,
  LONEJSON_ARRAY_STREAM_STATE_DONE = 2,
  LONEJSON_ARRAY_STREAM_STATE_ERROR = 3
} lonejson_array_stream_state;

typedef enum lonejson_array_stream_phase {
  LONEJSON_ARRAY_STREAM_PHASE_START = 0,
  LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_KEY_OR_END = 1,
  LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COLON = 2,
  LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_VALUE = 3,
  LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COMMA_OR_END = 4,
  LONEJSON_ARRAY_STREAM_PHASE_ARRAY_VALUE_OR_END = 5,
  LONEJSON_ARRAY_STREAM_PHASE_ARRAY_COMMA_OR_END = 6,
  LONEJSON_ARRAY_STREAM_PHASE_ITEM_AFTER_VALUE = 7,
  LONEJSON_ARRAY_STREAM_PHASE_FINISH_DOCUMENT = 8
} lonejson_array_stream_phase;

typedef enum lonejson_array_stream_key_phase {
  LONEJSON_ARRAY_STREAM_KEY_IDLE = 0,
  LONEJSON_ARRAY_STREAM_KEY_BODY = 1,
  LONEJSON_ARRAY_STREAM_KEY_ESCAPE = 2,
  LONEJSON_ARRAY_STREAM_KEY_UNICODE = 3,
  LONEJSON_ARRAY_STREAM_KEY_SURROGATE_SLASH = 4,
  LONEJSON_ARRAY_STREAM_KEY_SURROGATE_U = 5,
  LONEJSON_ARRAY_STREAM_KEY_SURROGATE_LOW = 6
} lonejson_array_stream_key_phase;

typedef enum lonejson_array_stream_value_mode {
  LONEJSON_ARRAY_STREAM_VALUE_NONE = 0,
  LONEJSON_ARRAY_STREAM_VALUE_SKIP = 1,
  LONEJSON_ARRAY_STREAM_VALUE_MAPPED = 2,
  LONEJSON_ARRAY_STREAM_VALUE_RAW = 3,
  LONEJSON_ARRAY_STREAM_VALUE_STRING = 4,
  LONEJSON_ARRAY_STREAM_VALUE_SINK = 5
} lonejson_array_stream_value_mode;

typedef struct lonejson__array_stream_dup_frame {
  lonejson__byte_buffer keys;
} lonejson__array_stream_dup_frame;

typedef struct lonejson__array_stream_dup_state {
  const lonejson_allocator *allocator;
  lonejson__array_stream_dup_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  lonejson__byte_buffer key;
} lonejson__array_stream_dup_state;

typedef struct lonejson__array_stream_state {
  lonejson_reader_fn reader;
  void *reader_user;
  FILE *fp;
  int fd;
  int owns_fp;
  int owns_fd;
  lonejson_array_stream_source_kind source_kind;
  lonejson_array_stream_state state;
  lonejson_array_stream_phase phase;
  char *path;
  size_t path_len;
  size_t path_alloc_size;
  int first_item;
  int selected_seen;
  int root_object_after_comma;
  int root_array_selected;
  int would_block;
  int push_eof;
  int push_truncated;
  int has_pushback;
  int pushback;
  int current_key_matched;
  int item_pending;
  lonejson_array_stream_key_phase key_phase;
  lonejson_uint32 key_codepoint;
  lonejson_uint32 key_high_surrogate;
  int key_digits_needed;
  lonejson_array_stream_value_mode value_mode;
  lonejson_array_stream_value_mode pending_value_mode;
  int value_active;
  int value_parser_initialized;
  const lonejson_map *active_map;
  const lonejson_map *pending_map;
  void *active_dst;
  void *pending_dst;
  void *value_prepared_dst;
  lonejson_sink_fn active_sink;
  void *active_sink_user;
  size_t buffered_start;
  size_t buffered_end;
  size_t offset;
  size_t line;
  size_t column;
  size_t self_alloc_size;
  const lonejson_runtime *runtime;
  lonejson_runtime runtime_storage;
  lonejson__parse_options options;
  lonejson_allocator allocator;
  lonejson_parser value_parser;
  lonejson_json_value skip_value;
  lonejson_value_visitor skip_visitor;
  lonejson__array_stream_dup_state skip_dup_state;
  int skip_dup_active;
  lonejson_json_value string_value;
  lonejson_value_visitor string_visitor;
  const lonejson_array_stream_string_handler *string_handler;
  void *string_user;
  int string_active;
  int string_seen;
  lonejson__byte_buffer string_item;
  lonejson_array_stream_string_fn string_callback;
  void *string_callback_user;
  size_t string_item_max_bytes;
  unsigned char value_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                LONEJSON__PARSER_WORKSPACE_SLACK];
  unsigned char io_buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson__byte_buffer item;
  lonejson__byte_buffer key;
  lonejson__byte_buffer root_keys;
  lonejson_array_stream public;
} lonejson__array_stream_state;

static LONEJSON__INLINE lonejson__stream_state *
lonejson__stream_state_mut(lonejson_stream *stream) {
  return stream != NULL
             ? (lonejson__stream_state *)((char *)stream -
                                          offsetof(lonejson__stream_state,
                                                   public))
             : NULL;
}

static LONEJSON__INLINE const lonejson__stream_state *
lonejson__stream_state_const(const lonejson_stream *stream) {
  return stream != NULL
             ? (const lonejson__stream_state *)((const char *)stream -
                                                offsetof(lonejson__stream_state,
                                                         public))
             : NULL;
}

static LONEJSON__INLINE lonejson__array_stream_state *
lonejson__array_stream_state_mut(lonejson_array_stream *stream) {
  return stream != NULL
             ? (lonejson__array_stream_state
                    *)((char *)stream -
                       offsetof(lonejson__array_stream_state, public))
             : NULL;
}

static LONEJSON__INLINE const lonejson__array_stream_state *
lonejson__array_stream_state_const(const lonejson_array_stream *stream) {
  return stream != NULL
             ? (const lonejson__array_stream_state
                    *)((const char *)stream -
                       offsetof(lonejson__array_stream_state, public))
             : NULL;
}

struct lonejson_sse {
  lonejson_sse_options options;
  lonejson_allocator allocator;
  lonejson__byte_buffer line;
  lonejson__byte_buffer event;
  lonejson__byte_buffer id;
  lonejson_parser json_parser;
  unsigned char json_workspace[LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                               LONEJSON__PARSER_WORKSPACE_SLACK];
  const lonejson_map *json_map;
  void *json_dst;
  lonejson_json_value *json_value;
  const lonejson__parse_options *json_parse_options;
  const lonejson_runtime *json_runtime;
  const lonejson_runtime *json_runtime_source;
  lonejson_runtime json_runtime_storage;
  lonejson__parse_options json_parse_options_storage;
  size_t event_data_len;
  size_t json_data_len;
  unsigned long retry_ms;
  int has_retry;
  int saw_cr;
  int closed;
  int event_pending;
  int event_started;
  int data_seen;
  int json_parser_active;
  int json_data_seen;
  int json_selection_locked;
  int json_selected;
  const lonejson_map *json_retained_map;
  void *json_retained_dst;
  int json_retained_active;
  lonejson_json_value *json_retained_value;
  int json_retained_value_active;
};

typedef enum lonejson__multipart_phase {
  LONEJSON__MULTIPART_EXPECT_BOUNDARY = 1,
  LONEJSON__MULTIPART_HEADERS = 2,
  LONEJSON__MULTIPART_BODY_LENGTH = 3,
  LONEJSON__MULTIPART_BODY_SCAN = 4,
  LONEJSON__MULTIPART_DONE = 5
} lonejson__multipart_phase;

struct lonejson_multipart {
  lonejson_multipart_options options;
  lonejson_allocator allocator;
  lonejson__byte_buffer boundary;
  lonejson__byte_buffer boundary_line;
  lonejson__byte_buffer closing_boundary_line;
  lonejson__byte_buffer line;
  lonejson__byte_buffer body;
  lonejson_header *headers;
  size_t header_count;
  size_t header_cap;
  size_t headers_alloc_size;
  char *part_name;
  size_t part_name_alloc_size;
  char *content_type;
  size_t content_type_alloc_size;
  lonejson_int64 content_length;
  lonejson_int64 remaining;
  int saw_cr;
  int in_part;
  int closed;
  lonejson__multipart_phase phase;
};

typedef union lonejson__alloc_header_align_union {
  void *ptr;
  lonejson_uint64 u64;
  double f64;
  long double ld;
} lonejson__alloc_header_align_union;

typedef union lonejson__alloc_header {
  struct {
    lonejson_allocator allocator;
    size_t size;
    const void *budget_tag;
    size_t budget_bytes;
  } meta;
  lonejson__alloc_header_align_union _align;
} lonejson__alloc_header;

typedef struct lonejson__alloc_header_align_probe {
  char c;
  lonejson__alloc_header value;
} lonejson__alloc_header_align_probe;

#define LONEJSON__ALLOC_HEADER_ALIGNMENT                                       \
  offsetof(lonejson__alloc_header_align_probe, value)

#ifndef NDEBUG
static int lonejson__is_aligned(const void *ptr, size_t alignment) {
  if (alignment == 0u) {
    return 1;
  }
  return ((uintptr_t)ptr % (uintptr_t)alignment) == 0u;
}
#endif

static LONEJSON__INLINE int
lonejson__json_value_parse_visitor_active(const lonejson_parser *parser);
static LONEJSON__INLINE void
lonejson__parser_set_json_stream_value(lonejson_parser *parser,
                                       lonejson_json_value *value);
static LONEJSON__INLINE void
lonejson__parser_clear_json_stream_value(lonejson_parser *parser);
static void lonejson__parser_cleanup_json_stream_path(lonejson_parser *parser);
static lonejson_status
lonejson__json_value_string_begin(lonejson_parser *parser, int is_key);
static lonejson_status
lonejson__json_value_string_chunk(lonejson_parser *parser, const char *data,
                                  size_t len);
static lonejson_status lonejson__json_value_string_end(lonejson_parser *parser);

static lonejson_status lonejson__set_error(lonejson_error *error,
                                           lonejson_status code, size_t offset,
                                           size_t line, size_t column,
                                           const char *fmt, ...) {
  va_list ap;

  if (error == NULL) {
    return code;
  }
  error->code = code;
  error->offset = offset;
  error->line = line;
  error->column = column;
  error->truncated = (code == LONEJSON_STATUS_TRUNCATED) ? 1 : error->truncated;
  va_start(ap, fmt);
  vsnprintf(error->message, sizeof(error->message), fmt, ap);
  va_end(ap);
  return code;
}

static void lonejson__clear_error(lonejson_error *error) {
  if (error == NULL) {
    return;
  }
  memset(error, 0, sizeof(*error));
  error->code = LONEJSON_STATUS_OK;
}

void lonejson_error_init(lonejson_error *error) {
  lonejson__clear_error(error);
}

static void *lonejson__default_malloc(void *ctx, size_t size) {
  (void)ctx;
  return LONEJSON_MALLOC(size);
}

static void *lonejson__default_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  return LONEJSON_REALLOC(ptr, size);
}

static void lonejson__default_free(void *ctx, void *ptr) {
  (void)ctx;
  LONEJSON_FREE(ptr);
}

lonejson_allocator lonejson_default_allocator(void) {
  lonejson_allocator allocator;

  memset(&allocator, 0, sizeof(allocator));
  allocator.malloc_fn = lonejson__default_malloc;
  allocator.realloc_fn = lonejson__default_realloc;
  allocator.free_fn = lonejson__default_free;
  allocator.ctx = NULL;
  allocator.stats = NULL;
  return allocator;
}

#define LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator)                       \
  ((allocator) == NULL ? 3                                                     \
                       : (((allocator)->malloc_fn == NULL ? 1 : 0) +           \
                          ((allocator)->realloc_fn == NULL ? 1 : 0) +          \
                          ((allocator)->free_fn == NULL ? 1 : 0)))

#define LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)                         \
  (LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 0 ||                    \
   LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 3)

static LONEJSON__INLINE lonejson_allocator
lonejson__allocator_resolve(const lonejson_allocator *allocator) {
  if (LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 3) {
    return lonejson_default_allocator();
  }
  return *allocator;
}

static LONEJSON__INLINE int
lonejson__allocator_equal(const lonejson_allocator *a,
                          const lonejson_allocator *b) {
  return a != NULL && b != NULL && a->malloc_fn == b->malloc_fn &&
         a->realloc_fn == b->realloc_fn && a->free_fn == b->free_fn &&
         a->ctx == b->ctx && a->stats == b->stats;
}

static int
lonejson__allocator_is_default_family(const lonejson_allocator *allocator) {
  lonejson_allocator def;

  if (LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 3) {
    return 1;
  }
  if (!LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)) {
    return 0;
  }
  def = lonejson_default_allocator();
  return allocator->malloc_fn == def.malloc_fn &&
         allocator->realloc_fn == def.realloc_fn &&
         allocator->free_fn == def.free_fn && allocator->ctx == def.ctx;
}

static void lonejson__allocator_note_alloc(lonejson_allocator_stats *stats,
                                           size_t size) {
  if (stats == NULL) {
    return;
  }
  lonejson__lock_acquire(&g_lonejson_allocator_stats_lock);
  stats->alloc_calls++;
  stats->bytes_live += size;
  if (stats->bytes_live > stats->peak_bytes_live) {
    stats->peak_bytes_live = stats->bytes_live;
  }
  lonejson__lock_release(&g_lonejson_allocator_stats_lock);
}

static void lonejson__allocator_note_realloc(lonejson_allocator_stats *stats,
                                             size_t old_size, size_t new_size) {
  if (stats == NULL) {
    return;
  }
  lonejson__lock_acquire(&g_lonejson_allocator_stats_lock);
  stats->realloc_calls++;
  if (stats->bytes_live >= old_size) {
    stats->bytes_live -= old_size;
  } else {
    stats->bytes_live = 0u;
  }
  stats->bytes_live += new_size;
  if (stats->bytes_live > stats->peak_bytes_live) {
    stats->peak_bytes_live = stats->bytes_live;
  }
  lonejson__lock_release(&g_lonejson_allocator_stats_lock);
}

static void lonejson__allocator_note_free(lonejson_allocator_stats *stats,
                                          size_t size) {
  if (stats == NULL) {
    return;
  }
  lonejson__lock_acquire(&g_lonejson_allocator_stats_lock);
  stats->free_calls++;
  if (stats->bytes_live >= size) {
    stats->bytes_live -= size;
  } else {
    stats->bytes_live = 0u;
  }
  lonejson__lock_release(&g_lonejson_allocator_stats_lock);
}

static void *lonejson__buffer_alloc(const lonejson_allocator *allocator,
                                    size_t size) {
  lonejson_allocator resolved;
  void *ptr;

  resolved = lonejson__allocator_resolve(allocator);
  ptr = resolved.malloc_fn(resolved.ctx, size);
  if (ptr != NULL) {
    lonejson__allocator_note_alloc(resolved.stats, size);
  }
  return ptr;
}

static void *lonejson__buffer_realloc(const lonejson_allocator *allocator,
                                      void *ptr, size_t old_size,
                                      size_t new_size) {
  lonejson_allocator resolved;
  void *next;

  resolved = lonejson__allocator_resolve(allocator);
  if (ptr == NULL) {
    return lonejson__buffer_alloc(allocator, new_size);
  }
  next = resolved.realloc_fn(resolved.ctx, ptr, new_size);
  if (next != NULL) {
    lonejson__allocator_note_realloc(resolved.stats, old_size, new_size);
  }
  return next;
}

static void lonejson__buffer_free(const lonejson_allocator *allocator,
                                  void *ptr, size_t alloc_size) {
  lonejson_allocator resolved;

  if (ptr == NULL) {
    return;
  }
  resolved = lonejson__allocator_resolve(allocator);
  lonejson__allocator_note_free(resolved.stats, alloc_size);
  resolved.free_fn(resolved.ctx, ptr);
}

static void *lonejson__owned_malloc(const lonejson_allocator *allocator,
                                    size_t size) {
  lonejson_allocator resolved;
  lonejson__alloc_header *header;
  void *raw;

  resolved = lonejson__allocator_resolve(allocator);
  raw = resolved.malloc_fn(resolved.ctx, sizeof(*header) + size);
  if (raw == NULL) {
    return NULL;
  }
#ifndef NDEBUG
  if (!lonejson__is_aligned(raw, LONEJSON__ALLOC_HEADER_ALIGNMENT)) {
    resolved.free_fn(resolved.ctx, raw);
    return NULL;
  }
#endif
  header = (lonejson__alloc_header *)raw;
  header->meta.allocator = resolved;
  header->meta.size = size;
  header->meta.budget_tag = NULL;
  header->meta.budget_bytes = 0u;
  lonejson__allocator_note_alloc(resolved.stats, size);
  return (void *)(header + 1u);
}

static void *lonejson__owned_realloc(const lonejson_allocator *allocator,
                                     void *ptr, size_t size) {
  lonejson__alloc_header *header;
  lonejson_allocator resolved;
  lonejson__alloc_header *next;

  if (ptr == NULL) {
    return lonejson__owned_malloc(allocator, size);
  }
  if (size == 0u) {
    return ptr;
  }
  header = ((lonejson__alloc_header *)ptr) - 1u;
  resolved = header->meta.allocator;
  next = (lonejson__alloc_header *)resolved.realloc_fn(resolved.ctx, header,
                                                       sizeof(*header) + size);
  if (next == NULL) {
    return NULL;
  }
  lonejson__allocator_note_realloc(resolved.stats, next->meta.size, size);
  next->meta.allocator = resolved;
  next->meta.size = size;
  return (void *)(next + 1u);
}

static size_t lonejson__owned_size(const void *ptr) {
  const lonejson__alloc_header *header;

  if (ptr == NULL) {
    return 0u;
  }
  header = ((const lonejson__alloc_header *)ptr) - 1u;
  return header->meta.size;
}

static void lonejson__owned_free(void *ptr) {
  lonejson__alloc_header *header;

  if (ptr == NULL) {
    return;
  }
  header = ((lonejson__alloc_header *)ptr) - 1u;
  lonejson__allocator_note_free(header->meta.allocator.stats,
                                header->meta.size);
  header->meta.allocator.free_fn(header->meta.allocator.ctx, header);
}

static char *lonejson__owned_strdup(const lonejson_allocator *allocator,
                                    const char *src) {
  size_t len;
  char *copy;

  if (src == NULL || src[0] == '\0') {
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

static LONEJSON__INLINE int
lonejson__parser_alloc_can_grow(const lonejson_parser *parser, size_t current,
                                size_t next) {
  size_t base;

  if (parser == NULL || parser->options.max_alloc_bytes == 0u) {
    return 1;
  }
  if (next <= current) {
    return 1;
  }
  base = (parser->parse_alloc_bytes_live >= current)
             ? (parser->parse_alloc_bytes_live - current)
             : 0u;
  return base <= parser->options.max_alloc_bytes &&
         next <= parser->options.max_alloc_bytes - base;
}

static LONEJSON__INLINE size_t lonejson__parser_alloc_available(
    const lonejson_parser *parser, size_t current) {
  size_t base;

  if (parser == NULL || parser->options.max_alloc_bytes == 0u) {
    return SIZE_MAX;
  }
  base = (parser->parse_alloc_bytes_live >= current)
             ? (parser->parse_alloc_bytes_live - current)
             : 0u;
  if (base >= parser->options.max_alloc_bytes) {
    return 0u;
  }
  return parser->options.max_alloc_bytes - base;
}

static LONEJSON__INLINE size_t lonejson__parser_alloc_counted_bytes(
    const lonejson_parser *parser, const void *ptr) {
  const lonejson__alloc_header *header;

  if (parser == NULL || ptr == NULL) {
    return 0u;
  }
  header = ((const lonejson__alloc_header *)ptr) - 1u;
  return header->meta.budget_tag == parser ? header->meta.budget_bytes : 0u;
}

static LONEJSON__INLINE void
lonejson__parser_alloc_note_resize(lonejson_parser *parser, void *ptr,
                                   size_t next);

static LONEJSON__INLINE int
lonejson__parser_alloc_try_adopt(lonejson_parser *parser, void *ptr) {
  size_t current;
  size_t next;

  if (parser == NULL || ptr == NULL) {
    return 1;
  }
  next = lonejson__owned_size(ptr);
  current = lonejson__parser_alloc_counted_bytes(parser, ptr);
  if (!lonejson__parser_alloc_can_grow(parser, current, next)) {
    return 0;
  }
  lonejson__parser_alloc_note_resize(parser, ptr, next);
  return 1;
}

static LONEJSON__INLINE void
lonejson__parser_alloc_note_resize(lonejson_parser *parser, void *ptr,
                                   size_t next) {
  lonejson__alloc_header *header;
  size_t current;

  if (parser == NULL || ptr == NULL) {
    return;
  }
  header = ((lonejson__alloc_header *)ptr) - 1u;
  current = header->meta.budget_tag == parser ? header->meta.budget_bytes : 0u;
  if (parser->parse_alloc_bytes_live >= current) {
    parser->parse_alloc_bytes_live -= current;
  } else {
    parser->parse_alloc_bytes_live = 0u;
  }
  header->meta.budget_tag = parser;
  header->meta.budget_bytes = next;
  parser->parse_alloc_bytes_live += next;
}

static LONEJSON__INLINE void
lonejson__parser_alloc_note_release(lonejson_parser *parser, void *ptr) {
  lonejson__alloc_header *header;

  if (parser == NULL || ptr == NULL) {
    return;
  }
  header = ((lonejson__alloc_header *)ptr) - 1u;
  if (header->meta.budget_tag == parser) {
    if (parser->parse_alloc_bytes_live >= header->meta.budget_bytes) {
      parser->parse_alloc_bytes_live -= header->meta.budget_bytes;
    } else {
      parser->parse_alloc_bytes_live = 0u;
    }
    header->meta.budget_tag = NULL;
    header->meta.budget_bytes = 0u;
  }
}

static void *lonejson__owned_malloc_parse(lonejson_parser *parser,
                                          size_t size) {
  void *ptr;

  if (!lonejson__parser_alloc_can_grow(parser, 0u, size)) {
    return NULL;
  }
  ptr = lonejson__owned_malloc(&parser->allocator, size);
  lonejson__parser_alloc_note_resize(parser, ptr, size);
  return ptr;
}

static void *lonejson__owned_realloc_parse(lonejson_parser *parser, void *ptr,
                                           size_t size) {
  size_t current;
  void *next;

  current = lonejson__parser_alloc_counted_bytes(parser, ptr);
  if (!lonejson__parser_alloc_can_grow(parser, current, size)) {
    return NULL;
  }
  next = lonejson__owned_realloc(&parser->allocator, ptr, size);
  lonejson__parser_alloc_note_resize(parser, next, size);
  return next;
}

static void *lonejson__owned_realloc_parse_with_allocator(
    lonejson_parser *parser, const lonejson_allocator *allocator, void *ptr,
    size_t size) {
  size_t current;
  void *next;

  current = lonejson__parser_alloc_counted_bytes(parser, ptr);
  if (!lonejson__parser_alloc_can_grow(parser, current, size)) {
    return NULL;
  }
  next = lonejson__owned_realloc(allocator, ptr, size);
  lonejson__parser_alloc_note_resize(parser, next, size);
  return next;
}

static void lonejson__owned_free_parse(lonejson_parser *parser, void *ptr) {
  lonejson__parser_alloc_note_release(parser, ptr);
  lonejson__owned_free(ptr);
}

lonejson__spool_options lonejson__default_spool_options(void) {
  lonejson__spool_options options;

  memset(&options, 0, sizeof(options));
  options.memory_limit = LONEJSON_SPOOL_MEMORY_LIMIT;
  options.max_bytes = LONEJSON_SPOOL_MAX_BYTES;
  options.temp_dir = NULL;
  return options;
}

lonejson_read_result lonejson_default_read_result(void) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  return result;
}

void lonejson_buffer_reader_init(lonejson_buffer_reader *reader,
                                 const void *data, size_t len) {
  if (reader == NULL) {
    return;
  }
  reader->data = (const unsigned char *)data;
  reader->len = len;
  reader->offset = 0u;
}

lonejson_read_result lonejson_buffer_reader_read(void *user,
                                                 unsigned char *buffer,
                                                 size_t capacity) {
  lonejson_buffer_reader *reader = (lonejson_buffer_reader *)user;
  lonejson_read_result result;
  size_t remaining;
  size_t n;

  memset(&result, 0, sizeof(result));
  if (reader == NULL || buffer == NULL ||
      (reader->data == NULL && reader->len != 0u)) {
    result.error_code = EINVAL;
    return result;
  }
  if (reader->offset >= reader->len) {
    result.eof = 1;
    return result;
  }
  if (capacity == 0u) {
    return result;
  }
  remaining = reader->len - reader->offset;
  n = remaining < capacity ? remaining : capacity;
  memcpy(buffer, reader->data + reader->offset, n);
  reader->offset += n;
  result.bytes_read = n;
  result.eof = reader->offset >= reader->len ? 1 : 0;
  return result;
}

lonejson_value_visitor lonejson_default_value_visitor(void) {
  lonejson_value_visitor visitor;

  memset(&visitor, 0, sizeof(visitor));
  return visitor;
}

lonejson_path_value_visitor lonejson_default_path_value_visitor(void) {
  lonejson_path_value_visitor visitor;

  memset(&visitor, 0, sizeof(visitor));
  return visitor;
}

static void
lonejson__spooled_apply_options(lonejson_spooled *value,
                                const lonejson__spool_options *options) {
  lonejson__spool_options local;

  if (value == NULL) {
    return;
  }
  local = options ? *options : lonejson__default_spool_options();
  value->memory_limit = local.memory_limit;
  value->max_bytes = local.max_bytes;
  value->temp_dir = local.temp_dir;
}

static void
lonejson__spooled_apply_allocator(lonejson_spooled *value,
                                  const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  value->allocator = LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)
                         ? lonejson__allocator_resolve(allocator)
                         : lonejson_default_allocator();
}

static LONEJSON__INLINE int lonejson__bytes_are_initialized(const void *ptr,
                                                            size_t size) {
#if LONEJSON__HAS_MSAN
  return ptr == NULL || __msan_test_shadow(ptr, size) < 0;
#else
  (void)ptr;
  (void)size;
  return 1;
#endif
}

static LONEJSON__INLINE int
lonejson__spooled_is_initialized(const lonejson_spooled *value) {
  return value != NULL &&
         lonejson__bytes_are_initialized(&value->_lonejson_magic,
                                         sizeof(value->_lonejson_magic)) &&
         value->_lonejson_magic ==
             lonejson__init_cookie(value, LONEJSON__SPOOLED_MAGIC);
}

static void
lonejson__json_value_apply_allocator(lonejson_json_value *value,
                                     const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  value->allocator = LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)
                         ? lonejson__allocator_resolve(allocator)
                         : lonejson_default_allocator();
}

static LONEJSON__INLINE int
lonejson__json_value_is_initialized(const lonejson_json_value *value) {
  return value != NULL &&
         lonejson__bytes_are_initialized(&value->methods,
                                         sizeof(value->methods)) &&
         lonejson__bytes_are_initialized(&value->_lonejson_magic,
                                         sizeof(value->_lonejson_magic)) &&
         value->methods == &g_lonejson_json_value_methods &&
         value->_lonejson_magic ==
             lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
}

static void lonejson__string_array_stream_assign_methods(
    lonejson_string_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  stream->set_handler = lonejson_string_array_stream_set_handler;
}

static void lonejson__mapped_array_stream_assign_methods(
    lonejson_mapped_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  stream->set_handler = lonejson_mapped_array_stream_set_handler;
}

static LONEJSON__INLINE int lonejson__string_array_stream_is_initialized(
    const lonejson_string_array_stream *stream) {
  return stream != NULL &&
         lonejson__bytes_are_initialized(&stream->_lonejson_magic,
                                         sizeof(stream->_lonejson_magic)) &&
         stream->_lonejson_magic ==
             lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
}

static LONEJSON__INLINE int lonejson__mapped_array_stream_is_initialized(
    const lonejson_mapped_array_stream *stream) {
  return stream != NULL &&
         lonejson__bytes_are_initialized(&stream->_lonejson_magic,
                                         sizeof(stream->_lonejson_magic)) &&
         stream->_lonejson_magic ==
             lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
}

static void lonejson__source_assign_methods(lonejson_source *value) {
  static const lonejson_source template_methods = {
      0,
      NULL,
      -1,
      NULL,
      lonejson_source_cleanup,
      lonejson_source_reset,
      lonejson_source_set_file,
      lonejson_source_set_fd,
      lonejson_source_set_path,
      lonejson_source_write_to_sink,
      lonejson_source_is_rewindable};
  if (value == NULL) {
    return;
  }
  memcpy(&value->cleanup, &template_methods.cleanup,
         sizeof(*value) - offsetof(lonejson_source, cleanup));
}

static LONEJSON__INLINE int
lonejson__source_is_initialized(const lonejson_source *value) {
  return value != NULL &&
         lonejson__bytes_are_initialized(
             &value->cleanup,
             sizeof(*value) - offsetof(lonejson_source, cleanup)) &&
         value->cleanup == lonejson_source_cleanup &&
         value->reset == lonejson_source_reset &&
         value->set_file == lonejson_source_set_file &&
         value->set_fd == lonejson_source_set_fd &&
         value->set_path == lonejson_source_set_path &&
         value->write_to_sink == lonejson_source_write_to_sink &&
         value->is_rewindable == lonejson_source_is_rewindable;
}

void lonejson_source_init(lonejson_source *value) {
  if (value == NULL) {
    return;
  }
  memset(value, 0, offsetof(lonejson_source, cleanup));
  value->kind = LONEJSON_SOURCE_NONE;
  value->fd = -1;
  lonejson__source_assign_methods(value);
}

void lonejson_source_cleanup(lonejson_source *value) {
  if (value == NULL) {
    return;
  }
  lonejson__owned_free(value->path);
  value->path = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->kind = LONEJSON_SOURCE_NONE;
}

void lonejson_source_reset(lonejson_source *value) {
  lonejson_source_cleanup(value);
}

void lonejson_json_value_init(lonejson *runtime, lonejson_json_value *value) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state =
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

static const lonejson_json_value_methods g_lonejson_json_value_methods = {
    lonejson_json_value_reset,
    lonejson_json_value_cleanup,
    lonejson_json_value_set_buffer,
    lonejson_json_value_set_parse_sink,
    lonejson_json_value_set_parse_visitor,
    lonejson_json_value_set_parse_path_visitor,
    lonejson_json_value_enable_parse_capture,
    lonejson_json_value_set_reader,
    lonejson_json_value_set_file,
    lonejson_json_value_set_fd,
    lonejson_json_value_set_path,
    lonejson_json_value_write_to_sink,
    lonejson_json_value_is_rewindable};

static void lonejson__json_value_assign_methods(lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  value->methods = &g_lonejson_json_value_methods;
}

void lonejson_json_value_init_with_allocator(
    lonejson_json_value *value, const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  value->kind = LONEJSON_JSON_VALUE_NULL;
  value->json = NULL;
  value->len = 0u;
  value->reader = NULL;
  value->reader_user = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->path = NULL;
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_NONE;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_path_visitor = NULL;
  value->parse_path_visitor_user = NULL;
  value->parse_visitor_limits = lonejson__default_value_limits();
  value->runtime_parse_visitor_limits = value->parse_visitor_limits;
  value->parse_limits_follow_runtime = 1;
  lonejson__json_value_apply_allocator(value, allocator);
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
  lonejson__json_value_assign_methods(value);
}

void lonejson_json_value_cleanup(lonejson_json_value *value) {
  lonejson_allocator allocator;
  lonejson__value_limits limits;
  lonejson__value_limits runtime_limits;
  int follow_runtime;

  if (value == NULL) {
    return;
  }
  allocator = value->allocator;
  limits = value->parse_visitor_limits;
  runtime_limits = value->runtime_parse_visitor_limits;
  follow_runtime = value->parse_limits_follow_runtime;
  lonejson__owned_free(value->json);
  if (value->kind == LONEJSON_JSON_VALUE_PATH) {
    lonejson__owned_free(value->path);
  }
  value->json = NULL;
  value->len = 0u;
  value->reader = NULL;
  value->reader_user = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->path = NULL;
  value->kind = LONEJSON_JSON_VALUE_NULL;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_path_visitor = NULL;
  value->parse_path_visitor_user = NULL;
  value->parse_visitor_limits = limits;
  value->runtime_parse_visitor_limits = runtime_limits;
  value->parse_limits_follow_runtime = follow_runtime;
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_NONE;
  value->allocator = allocator;
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
}

void lonejson_json_value_reset(lonejson_json_value *value) {
  lonejson_json_value_cleanup(value);
}

static void lonejson__json_value_clear_runtime(lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  if (value->kind == LONEJSON_JSON_VALUE_NULL && value->json == NULL &&
      value->len == 0u && value->reader == NULL && value->reader_user == NULL &&
      value->fp == NULL && value->fd == -1 && value->path == NULL) {
    return;
  }
  lonejson__owned_free(value->json);
  if (value->kind == LONEJSON_JSON_VALUE_PATH) {
    lonejson__owned_free(value->path);
  }
  value->json = NULL;
  value->len = 0u;
  value->reader = NULL;
  value->reader_user = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->path = NULL;
  value->kind = LONEJSON_JSON_VALUE_NULL;
}

static void
lonejson__json_value_clear_runtime_parse(lonejson_parser *parser,
                                         lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  if (value->kind == LONEJSON_JSON_VALUE_NULL && value->json == NULL &&
      value->len == 0u && value->reader == NULL && value->reader_user == NULL &&
      value->fp == NULL && value->fd == -1 && value->path == NULL) {
    return;
  }
  lonejson__parser_alloc_note_release(parser, value->json);
  lonejson__parser_alloc_note_release(parser, value->path);
  lonejson__json_value_clear_runtime(value);
}

static lonejson_status
lonejson__json_value_prepare_parse(lonejson_parser *parser,
                                   lonejson_json_value *value,
                                   lonejson_error *error) {
  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value parse requires a configured parse "
                               "sink, parse visitor, or parse capture");
  }
  lonejson__json_value_clear_runtime_parse(parser, value);
  return LONEJSON_STATUS_OK;
}

static void lonejson__spooled_reset_parse(lonejson_parser *parser,
                                          lonejson_spooled *value) {
  if (value == NULL) {
    return;
  }
  lonejson__parser_alloc_note_release(parser, value->memory);
  lonejson__parser_alloc_note_release(parser, value->owned_temp_dir);
  lonejson_spooled_reset(value);
  if (value->owned_temp_dir != NULL &&
      !lonejson__parser_alloc_try_adopt(parser, value->owned_temp_dir)) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column,
                        "parse allocations exceed configured max bytes");
    parser->failed = 1;
    lonejson_spooled_cleanup(value);
  }
}

static void lonejson__source_reset_parse(lonejson_parser *parser,
                                         lonejson_source *value) {
  if (value == NULL) {
    return;
  }
  lonejson__parser_alloc_note_release(parser, value->path);
  lonejson_source_reset(value);
}

lonejson_status lonejson_source_set_file(lonejson_source *value, FILE *fp,
                                         lonejson_error *error) {
  if (value == NULL || fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "source handle and FILE* are required");
  }
  lonejson_source_cleanup(value);
  value->kind = LONEJSON_SOURCE_FILE;
  value->fp = fp;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_source_set_fd(lonejson_source *value, int fd,
                                       lonejson_error *error) {
  if (value == NULL || fd < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "source handle and non-negative fd are required");
  }
  lonejson_source_cleanup(value);
  value->kind = LONEJSON_SOURCE_FD;
  value->fd = fd;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_source_set_path(lonejson_source *value,
                                         const char *path,
                                         lonejson_error *error) {
  size_t len;
  char *copy;

  if (value == NULL || path == NULL || path[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "source handle and path are required");
  }
  len = strlen(path);
  copy = (char *)lonejson__owned_malloc(NULL, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to duplicate source path");
  }
  memcpy(copy, path, len + 1u);
  lonejson_source_cleanup(value);
  value->kind = LONEJSON_SOURCE_PATH;
  value->path = copy;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_spooled_init(lonejson *runtime, lonejson_spooled *value) {
  lonejson_spooled_init_class(runtime, value, LONEJSON_SPOOL_CLASS_DEFAULT);
}

static const lonejson__spool_options *
lonejson__runtime_spool_options_for_class(const lonejson_runtime *runtime,
                                          lonejson_spool_class spool_class) {
  if (runtime == NULL) {
    return NULL;
  }
  switch (spool_class) {
  case LONEJSON_SPOOL_CLASS_DEFAULT:
    return &runtime->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT];
  case LONEJSON_SPOOL_CLASS_BLOB:
    return &runtime->spool_options[LONEJSON_SPOOL_CLASS_BLOB];
  case LONEJSON_SPOOL_CLASS_LARGE_TEXT:
    return &runtime->spool_options[LONEJSON_SPOOL_CLASS_LARGE_TEXT];
  default:
    return &runtime->spool_options[LONEJSON_SPOOL_CLASS_DEFAULT];
  }
}

void lonejson_spooled_init_class(lonejson *runtime, lonejson_spooled *value,
                                 lonejson_spool_class spool_class) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_borrow_const((const lonejson *)runtime, &borrow);
  const lonejson__spool_options *options =
      lonejson__runtime_spool_options_for_class(runtime_state, spool_class);

  lonejson_spooled_init_with_allocator(
      value, options,
      runtime_state != NULL ? runtime_state->config.allocator : NULL);
  lonejson__runtime_borrow_release(&borrow);
}

static void lonejson__spooled_assign_methods(lonejson_spooled *value) {
  static const lonejson_spooled template_methods = {
      NULL,
      0u,
      0u,
      0u,
      0u,
      0u,
      NULL,
      NULL,
      0,
      "",
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
      NULL,
#else
      NULL,
#endif
      {NULL, NULL, NULL, NULL, NULL},
      0u,
      lonejson_spooled_reset,
      lonejson_spooled_cleanup,
      lonejson_spooled_size,
      lonejson_spooled_spilled,
      lonejson_spooled_append,
      lonejson_spooled_rewind,
      lonejson_spooled_read,
      lonejson_spooled_write_to_sink};
  if (value == NULL) {
    return;
  }
  memcpy(&value->reset, &template_methods.reset,
         sizeof(*value) - offsetof(lonejson_spooled, reset));
}

void lonejson_spooled_init_with_allocator(
    lonejson_spooled *value, const lonejson__spool_options *options,
    const lonejson_allocator *allocator) {
  const char *temp_dir = NULL;

  if (value == NULL) {
    return;
  }
  if (options != NULL) {
    temp_dir = options->temp_dir;
  }
  memset(value, 0, offsetof(lonejson_spooled, reset));
  lonejson__spooled_apply_allocator(value, allocator);
  value->owned_temp_dir = lonejson__owned_strdup(&value->allocator, temp_dir);
  lonejson__spooled_apply_options(value, options);
  value->temp_dir = value->owned_temp_dir;
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__SPOOLED_MAGIC);
  lonejson__spooled_assign_methods(value);
}

static void lonejson__spooled_close_temp(lonejson_spooled *value) {
  if (value == NULL) {
    return;
  }
  if (value->spill_fp != NULL) {
    fclose(value->spill_fp);
    value->spill_fp = NULL;
  }
  if (value->temp_path[0] != '\0') {
    unlink(value->temp_path);
    value->temp_path[0] = '\0';
  }
}

void lonejson_spooled_cleanup(lonejson_spooled *value) {
  if (value == NULL) {
    return;
  }
  lonejson__owned_free(value->memory);
  value->memory = NULL;
  value->memory_len = 0u;
  value->size = 0u;
  value->read_offset = 0u;
  value->spilled = 0;
  lonejson__spooled_close_temp(value);
  lonejson__owned_free(value->owned_temp_dir);
  value->owned_temp_dir = NULL;
  value->temp_dir = NULL;
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__SPOOLED_MAGIC);
}

void lonejson_spooled_reset(lonejson_spooled *value) {
  lonejson__spool_options options;
  lonejson_allocator allocator;
  char *saved_temp_dir = NULL;

  if (value == NULL) {
    return;
  }
  options.memory_limit = value->memory_limit;
  options.max_bytes = value->max_bytes;
  saved_temp_dir = value->owned_temp_dir;
  value->owned_temp_dir = NULL;
  options.temp_dir = saved_temp_dir;
  allocator = value->allocator;
  lonejson_spooled_cleanup(value);
  lonejson__spooled_apply_options(value, &options);
  value->allocator = allocator;
  value->owned_temp_dir = saved_temp_dir;
  value->temp_dir = saved_temp_dir;
}

size_t lonejson_spooled_size(const lonejson_spooled *value) {
  return value ? value->size : 0u;
}

int lonejson_spooled_spilled(const lonejson_spooled *value) {
  return value ? value->spilled : 0;
}

static lonejson_status lonejson__spooled_open_temp(lonejson_spooled *value,
                                                   lonejson_error *error) {
  if (value->spill_fp != NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (value->temp_dir != NULL && value->temp_dir[0] != '\0') {
    int fd;
    int written;

    written = snprintf(value->temp_path, sizeof(value->temp_path),
                       "%s/lonejson-spool-XXXXXX", value->temp_dir);
    if (written < 0 || (size_t)written >= sizeof(value->temp_path)) {
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "temporary spool path is too long");
    }
    fd = mkstemp(value->temp_path);
    if (fd < 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to create spool file");
    }
    value->spill_fp = fdopen(fd, "w+b");
    if (value->spill_fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      close(fd);
      unlink(value->temp_path);
      value->temp_path[0] = '\0';
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to open spool file");
    }
  } else {
    value->spill_fp = tmpfile();
    if (value->spill_fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to create anonymous spool file");
    }
  }
  value->spilled = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__spooled_reserve_memory(lonejson_spooled *value,
                                                        size_t need,
                                                        lonejson_error *error) {
  unsigned char *next;

  if (need <= value->memory_len) {
    return LONEJSON_STATUS_OK;
  }
  next = (unsigned char *)lonejson__owned_realloc(&value->allocator,
                                                  value->memory, need);
  if (next == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to expand spooled in-memory prefix");
  }
  value->memory = next;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__spooled_reserve_memory_parse(lonejson_parser *parser,
                                       lonejson_spooled *value, size_t need,
                                       lonejson_error *error) {
  unsigned char *next;

  if (need <= value->memory_len) {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__parser_alloc_can_grow(
          parser, lonejson__parser_alloc_counted_bytes(parser, value->memory),
          need)) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "parse allocations exceed configured max bytes");
  }
  next = (unsigned char *)lonejson__owned_realloc_parse(parser, value->memory,
                                                        need);
  if (next == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to expand spooled in-memory prefix");
  }
  value->memory = next;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__spooled_append(lonejson_spooled *value,
                                                const unsigned char *data,
                                                size_t len,
                                                lonejson_error *error) {
  size_t memory_room;
  size_t memory_copy;
  lonejson_status status;

  if (value == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid spooled append arguments");
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (value->max_bytes != 0u && value->size + len > value->max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "spooled field exceeds configured max bytes");
  }

  memory_room = (value->memory_limit > value->memory_len)
                    ? (value->memory_limit - value->memory_len)
                    : 0u;
  memory_copy = (len < memory_room) ? len : memory_room;
  if (memory_copy != 0u) {
    status = lonejson__spooled_reserve_memory(
        value, value->memory_len + memory_copy, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    memcpy(value->memory + value->memory_len, data, memory_copy);
    value->memory_len += memory_copy;
  }
  if (memory_copy < len) {
    size_t spill_len = len - memory_copy;
    long spill_read_pos;

    status = lonejson__spooled_open_temp(value, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    spill_read_pos = (value->read_offset > value->memory_len)
                         ? (long)(value->read_offset - value->memory_len)
                         : 0L;
    if (fseek(value->spill_fp, 0L, SEEK_END) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to seek spool file for append");
    }
    if (fwrite(data + memory_copy, 1u, spill_len, value->spill_fp) !=
        spill_len) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to write spool file");
    }
    if (fseek(value->spill_fp, spill_read_pos, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to restore spool file read cursor");
    }
  }
  value->size += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__spooled_append_parse(lonejson_parser *parser,
                                                      lonejson_spooled *value,
                                                      const unsigned char *data,
                                                      size_t len,
                                                      lonejson_error *error) {
  size_t memory_room;
  size_t memory_copy;
  lonejson_status status;

  if (value == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid spooled append arguments");
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (value->max_bytes != 0u && value->size + len > value->max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "spooled field exceeds configured max bytes");
  }

  memory_room = (value->memory_limit > value->memory_len)
                    ? (value->memory_limit - value->memory_len)
                    : 0u;
  memory_copy = (len < memory_room) ? len : memory_room;
  if (memory_copy != 0u) {
    status = lonejson__spooled_reserve_memory_parse(
        parser, value, value->memory_len + memory_copy, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    memcpy(value->memory + value->memory_len, data, memory_copy);
    value->memory_len += memory_copy;
  }
  if (memory_copy < len) {
    size_t spill_len = len - memory_copy;
    long spill_read_pos;

    status = lonejson__spooled_open_temp(value, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    spill_read_pos = (value->read_offset > value->memory_len)
                         ? (long)(value->read_offset - value->memory_len)
                         : 0L;
    if (fseek(value->spill_fp, 0L, SEEK_END) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to seek spool file for append");
    }
    if (fwrite(data + memory_copy, 1u, spill_len, value->spill_fp) !=
        spill_len) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to write spool file");
    }
    if (fseek(value->spill_fp, spill_read_pos, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to restore spool file read cursor");
    }
  }
  value->size += len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_spooled_append(lonejson_spooled *value,
                                        const void *data, size_t len,
                                        lonejson_error *error) {
  return lonejson__spooled_append(value, (const unsigned char *)data, len,
                                  error);
}

lonejson_status lonejson_spooled_rewind(lonejson_spooled *value,
                                        lonejson_error *error) {
  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "spooled value is required");
  }
  value->read_offset = 0u;
  if (value->spill_fp != NULL) {
    if (fseek(value->spill_fp, 0L, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to rewind spool file");
    }
  }
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_read_result lonejson_spooled_read(lonejson_spooled *value,
                                           unsigned char *buffer,
                                           size_t capacity) {
  lonejson_read_result result;
  size_t memory_remaining;
  size_t copied;

  memset(&result, 0, sizeof(result));
  if (value == NULL || buffer == NULL || capacity == 0u) {
    result.eof = 1;
    return result;
  }
  memory_remaining = (value->read_offset < value->memory_len)
                         ? (value->memory_len - value->read_offset)
                         : 0u;
  copied = (memory_remaining < capacity) ? memory_remaining : capacity;
  if (copied != 0u) {
    memcpy(buffer, value->memory + value->read_offset, copied);
    value->read_offset += copied;
    result.bytes_read = copied;
    result.eof = (value->read_offset == value->size) ? 1 : 0;
    return result;
  }
  if (value->spill_fp != NULL) {
    size_t spill_offset = value->read_offset - value->memory_len;
    off_t spill_pos;
    ssize_t got;

    spill_pos = (off_t)spill_offset;
    if (spill_pos >= (off_t)0 && (size_t)spill_pos == spill_offset) {
      got = pread(fileno(value->spill_fp), buffer, capacity, spill_pos);
      if (got < 0) {
        result.error_code = errno != 0 ? errno : EIO;
        return result;
      }
      value->read_offset += (size_t)got;
      result.bytes_read = (size_t)got;
      result.eof = (value->read_offset >= value->size) ? 1 : 0;
      return result;
    }

    got = (ssize_t)fread(buffer, 1u, capacity, value->spill_fp);
    value->read_offset += (size_t)got;
    result.bytes_read = (size_t)got;
    result.eof = (value->read_offset >= value->size) ? 1 : 0;
    result.error_code = ferror(value->spill_fp) ? errno : 0;
    return result;
  }
  result.eof = 1;
  return result;
}

lonejson_status lonejson_spooled_write_to_sink(const lonejson_spooled *value,
                                               lonejson_sink_fn sink,
                                               void *user,
                                               lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson_spooled cursor;
  lonejson_status status;

  if (value == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "spooled value and sink are required");
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
                                 "failed to read spooled value");
    }
    if (chunk.bytes_read != 0u) {
      status = sink(user, buffer, chunk.bytes_read, error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  return LONEJSON_STATUS_OK;
}
