#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <locale.h>
#if !defined(_WIN32)
#include <pthread.h>
#define LONEJSON_TEST_HAS_PTHREAD 1
#else
#define LONEJSON_TEST_HAS_PTHREAD 0
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(LONEJSON_WITH_JWT) && defined(LONEJSON_WITH_OPENSSL)
#define LONEJSON_TEST_ENABLE_FAIL_ALLOC 1
#else
#define LONEJSON_TEST_ENABLE_FAIL_ALLOC 0
#endif

typedef struct alloc_record {
  void *ptr;
} alloc_record;

static alloc_record g_alloc_records[8192];
static size_t g_alloc_record_count = 0u;
static size_t g_lonejson_alloc_calls = 0u;
static size_t g_lonejson_free_calls = 0u;
#if LONEJSON_TEST_ENABLE_FAIL_ALLOC
static int g_lonejson_fail_alloc_enabled = 0;
static size_t g_lonejson_fail_alloc_calls = 0u;
static size_t g_lonejson_fail_alloc_after = 0u;
#endif

#if LONEJSON_TEST_HAS_PTHREAD
static pthread_mutex_t g_alloc_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
#define TEST_ALLOC_STATS_LOCK() pthread_mutex_lock(&g_alloc_stats_mutex)
#define TEST_ALLOC_STATS_UNLOCK() pthread_mutex_unlock(&g_alloc_stats_mutex)
#else
#define TEST_ALLOC_STATS_LOCK() ((void)0)
#define TEST_ALLOC_STATS_UNLOCK() ((void)0)
#endif

static void track_alloc_ptr_locked(void *ptr) {
  if (ptr == NULL) {
    return;
  }
  if (g_alloc_record_count <
      sizeof(g_alloc_records) / sizeof(g_alloc_records[0])) {
    g_alloc_records[g_alloc_record_count].ptr = ptr;
    ++g_alloc_record_count;
  }
}

static void track_free_ptr_locked(void *ptr) {
  size_t i;

  if (ptr == NULL) {
    return;
  }
  for (i = 0; i < g_alloc_record_count; ++i) {
    if (g_alloc_records[i].ptr == ptr) {
      g_alloc_records[i] = g_alloc_records[g_alloc_record_count - 1u];
      --g_alloc_record_count;
      return;
    }
  }
}

static void *test_lonejson_malloc(size_t size) {
  void *ptr;

#if LONEJSON_TEST_ENABLE_FAIL_ALLOC
  if (g_lonejson_fail_alloc_enabled) {
    if (g_lonejson_fail_alloc_calls >= g_lonejson_fail_alloc_after) {
      return NULL;
    }
    ++g_lonejson_fail_alloc_calls;
  }
#endif
  ptr = malloc(size);
  if (ptr != NULL) {
    TEST_ALLOC_STATS_LOCK();
    ++g_lonejson_alloc_calls;
    track_alloc_ptr_locked(ptr);
    TEST_ALLOC_STATS_UNLOCK();
  }
  return ptr;
}

static void *test_lonejson_realloc(void *ptr, size_t size) {
  void *next;

#if LONEJSON_TEST_ENABLE_FAIL_ALLOC
  if (g_lonejson_fail_alloc_enabled) {
    if (g_lonejson_fail_alloc_calls >= g_lonejson_fail_alloc_after) {
      return NULL;
    }
    ++g_lonejson_fail_alloc_calls;
  }
#endif
  if (ptr == NULL) {
    next = realloc(NULL, size);
    if (next != NULL) {
      TEST_ALLOC_STATS_LOCK();
      ++g_lonejson_alloc_calls;
      track_alloc_ptr_locked(next);
      TEST_ALLOC_STATS_UNLOCK();
    }
    return next;
  }

  TEST_ALLOC_STATS_LOCK();
  track_free_ptr_locked(ptr);
  next = realloc(ptr, size);
  if (next != NULL) {
    track_alloc_ptr_locked(next);
  } else {
    track_alloc_ptr_locked(ptr);
  }
  TEST_ALLOC_STATS_UNLOCK();
  return next;
}

static void test_lonejson_free(void *ptr) {
  if (ptr != NULL) {
    TEST_ALLOC_STATS_LOCK();
    ++g_lonejson_free_calls;
    track_free_ptr_locked(ptr);
    TEST_ALLOC_STATS_UNLOCK();
  }
  free(ptr);
}

static void reset_lonejson_alloc_stats(void) {
  TEST_ALLOC_STATS_LOCK();
  g_alloc_record_count = 0u;
  g_lonejson_alloc_calls = 0u;
  g_lonejson_free_calls = 0u;
  TEST_ALLOC_STATS_UNLOCK();
}

#if LONEJSON_TEST_ENABLE_FAIL_ALLOC
static void test_lonejson_fail_alloc_after(size_t successful_calls) {
  g_lonejson_fail_alloc_calls = 0u;
  g_lonejson_fail_alloc_after = successful_calls;
  g_lonejson_fail_alloc_enabled = 1;
}

static void test_lonejson_fail_alloc_clear(void) {
  g_lonejson_fail_alloc_enabled = 0;
  g_lonejson_fail_alloc_calls = 0u;
  g_lonejson_fail_alloc_after = 0u;
}
#endif

#if defined(LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN)
static void lonejson__runtime_handle_cache_drain(void);
#endif

static void test_runtime_handle_cache_drain(void) {
#if defined(LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN)
  lonejson__runtime_handle_cache_drain();
#endif
}

#define LONEJSON_MALLOC test_lonejson_malloc
#define LONEJSON_REALLOC test_lonejson_realloc
#define LONEJSON_FREE test_lonejson_free
#define LONEJSON_TRACK_WORKSPACE_USAGE 1
#include "../src/lonejson_internal.h"

typedef struct test_allocator_state {
  lonejson_allocator allocator;
  lonejson_allocator_stats stats;
} test_allocator_state;

#if LONEJSON_TEST_HAS_PTHREAD
typedef struct test_serial_allocator_state {
  lonejson_allocator allocator;
  lonejson_allocator_stats stats;
  pthread_mutex_t mutex;
  int active_calls;
  int overlap_detected;
} test_serial_allocator_state;
#endif

static void *test_allocator_malloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void *test_allocator_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  return realloc(ptr, size);
}

static void test_allocator_free(void *ctx, void *ptr) {
  (void)ctx;
  free(ptr);
}

static void *test_runtime_allocator_malloc(void *ctx, size_t size) {
  (void)ctx;
  return test_lonejson_malloc(size);
}

static void *test_runtime_allocator_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  return test_lonejson_realloc(ptr, size);
}

static void test_runtime_allocator_free(void *ctx, void *ptr) {
  (void)ctx;
  test_lonejson_free(ptr);
}

static lonejson *test_default_runtime(void) {
  static lonejson *runtime = NULL;
  static lonejson_allocator runtime_allocator = {
      test_runtime_allocator_malloc, test_runtime_allocator_realloc,
      test_runtime_allocator_free, NULL, NULL};

  if (runtime == NULL) {
    lonejson_config config = lonejson_default_config();
    config.allocator = &runtime_allocator;
#if defined(LONEJSON_WITH_JWT) && defined(LONEJSON_WITH_OPENSSL)
    {
      static lonejson_auth_provider auth_provider;
      if (lonejson_auth_provider_init_openssl(&auth_provider, NULL, NULL) ==
          LONEJSON_STATUS_OK) {
        config.auth_provider = &auth_provider;
      }
    }
#endif
    runtime = lonejson_new(&config, NULL);
    if (runtime == NULL) {
      fprintf(stderr, "failed to allocate default lonejson test runtime\n");
      abort();
    }
  }
  return runtime;
}

static void test_init_map(const lonejson_map *map, void *value) {
  lonejson_init(test_default_runtime(), map, value, NULL);
}

static lonejson_status test_parse_cstr(const lonejson_map *map, void *dst,
                                       const char *json,
                                       const lonejson__parse_options *options,
                                       lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_parse_cstr(test_default_runtime(), map, dst, json, error);
  }
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  return lonejson__parse_buffer_with_options(map, dst, json, strlen(json),
                                             options, runtime_state, error);
}

static lonejson_status test_parse_reader(const lonejson_map *map, void *dst,
                                         lonejson_reader_fn reader, void *user,
                                         const lonejson__parse_options *options,
                                         lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_parse_reader(test_default_runtime(), map, dst, reader, user,
                                 error);
  }
  return lonejson__parse_reader_with_options(map, dst, reader, user, options,
                                             runtime_state, error);
}

static lonejson_status test_parse_filep(const lonejson_map *map, void *dst,
                                        FILE *fp,
                                        const lonejson__parse_options *options,
                                        lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_parse_filep(test_default_runtime(), map, dst, fp, error);
  }
  return lonejson__parse_reader_with_options(map, dst, lonejson__file_reader,
                                             fp, options, runtime_state, error);
}

static lonejson_status test_parse_path(const lonejson_map *map, void *dst,
                                       const char *path,
                                       const lonejson__parse_options *options,
                                       lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (options == NULL) {
    return lonejson_parse_path(test_default_runtime(), map, dst, path, error);
  }
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
  status = test_parse_filep(map, dst, fp, options, error);
  fclose(fp);
  return status;
}

static lonejson_status test_validate_buffer(const void *data, size_t len,
                                            lonejson_error *error) {
  return lonejson_validate_buffer(test_default_runtime(), data, len, error);
}

static lonejson_status test_validate_cstr(const char *json,
                                          lonejson_error *error) {
  return lonejson_validate_cstr(test_default_runtime(), json, error);
}

static lonejson_status test_validate_path(const char *path,
                                          lonejson_error *error) {
  return lonejson_validate_path(test_default_runtime(), path, error);
}

static lonejson_status
test_visit_value_cstr(const char *json, const lonejson_value_visitor *visitor,
                      void *user, const lonejson__value_limits *limits,
                      lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_visit_value_cstr(test_default_runtime(), json, visitor,
                                     user, error);
  }
  return lonejson__visit_value_cstr_with_limits(json, visitor, user, limits,
                                                NULL, error);
}

static lonejson_status
test_visit_value_reader(lonejson_reader_fn reader, void *reader_user,
                        const lonejson_value_visitor *visitor, void *user,
                        const lonejson__value_limits *limits,
                        lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_visit_value_reader(test_default_runtime(), reader,
                                       reader_user, visitor, user, error);
  }
  return lonejson__visit_value_reader_with_limits(reader, reader_user, visitor,
                                                  user, limits, NULL, error);
}

static lonejson_status
test_visit_value_filep(FILE *fp, const lonejson_value_visitor *visitor,
                       void *user, const lonejson__value_limits *limits,
                       lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_visit_value_filep(test_default_runtime(), fp, visitor, user,
                                      error);
  }
  return test_visit_value_reader(lonejson__file_reader, fp, visitor, user,
                                 limits, error);
}

static lonejson_status
test_visit_value_path(const char *path, const lonejson_value_visitor *visitor,
                      void *user, const lonejson__value_limits *limits,
                      lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (limits == NULL) {
    return lonejson_visit_value_path(test_default_runtime(), path, visitor,
                                     user, error);
  }
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
  status = test_visit_value_filep(fp, visitor, user, limits, error);
  fclose(fp);
  return status;
}

static lonejson_status
test_visit_value_fd(int fd, const lonejson_value_visitor *visitor, void *user,
                    const lonejson__value_limits *limits,
                    lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_visit_value_fd(test_default_runtime(), fd, visitor, user,
                                   error);
  }
  return test_visit_value_reader(lonejson__fd_reader, &fd, visitor, user,
                                 limits, error);
}

static lonejson_status test_sse_push_json(
    lonejson_sse *sse, const lonejson_map *map, void *dst, const void *bytes,
    size_t len, const lonejson_sse_json_options *options,
    const lonejson__parse_options *parse_options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (parse_options == NULL) {
    return test_default_runtime()->sse_push_json(test_default_runtime(), sse,
                                                 map, dst, bytes, len, options,
                                                 event_cb, user, error);
  }
  return lonejson__sse_push_json_with_options(sse, map, dst, bytes, len,
                                              parse_options, runtime_state,
                                              options, event_cb, user, error);
}

static lonejson_status
test_sse_finish_json(lonejson_sse *sse, const lonejson_map *map, void *dst,
                     const lonejson_sse_json_options *options,
                     const lonejson__parse_options *parse_options,
                     lonejson_sse_json_event_fn event_cb, void *user,
                     lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (parse_options == NULL) {
    return test_default_runtime()->sse_finish_json(
        test_default_runtime(), sse, map, dst, options, event_cb, user, error);
  }
  return lonejson__sse_finish_json_with_options(sse, map, dst, parse_options,
                                                runtime_state, options,
                                                event_cb, user, error);
}

static lonejson_status
test_sse_push_json_value(lonejson_sse *sse, lonejson_json_value *value,
                         const void *bytes, size_t len,
                         const lonejson_sse_json_options *options,
                         const lonejson__parse_options *parse_options,
                         lonejson_sse_json_value_event_fn event_cb, void *user,
                         lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (parse_options == NULL) {
    return test_default_runtime()->sse_push_json_value(
        test_default_runtime(), sse, value, bytes, len, options, event_cb, user,
        error);
  }
  return lonejson__sse_push_json_value_with_options(
      sse, value, bytes, len, parse_options, runtime_state, options, event_cb,
      user, error);
}

static lonejson_status
test_sse_finish_json_value(lonejson_sse *sse, lonejson_json_value *value,
                           const lonejson_sse_json_options *options,
                           const lonejson__parse_options *parse_options,
                           lonejson_sse_json_value_event_fn event_cb,
                           void *user, lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (parse_options == NULL) {
    return test_default_runtime()->sse_finish_json_value(
        test_default_runtime(), sse, value, options, event_cb, user, error);
  }
  return lonejson__sse_finish_json_value_with_options(
      sse, value, parse_options, runtime_state, options, event_cb, user, error);
}

static lonejson_status
test_value_rewrite_reader(lonejson_reader_fn reader, void *reader_user,
                          lonejson_sink_fn sink, void *sink_user,
                          const lonejson_value_rewrite_options *options,
                          lonejson_error *error) {
  return lonejson_value_rewrite_reader(test_default_runtime(), reader,
                                       reader_user, sink, sink_user, options,
                                       error);
}

static lonejson_status test_value_rewrite_buffer(
    const void *data, size_t len, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_options *options, lonejson_error *error) {
  return lonejson_value_rewrite_buffer(test_default_runtime(), data, len, sink,
                                       sink_user, options, error);
}

static lonejson_status test_value_rewrite_selector_reader(
    lonejson_reader_fn reader, void *reader_user, lonejson_sink_fn sink,
    void *sink_user, const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_reader(test_default_runtime(), reader,
                                                reader_user, sink, sink_user,
                                                options, error);
}

static lonejson_status test_value_rewrite_selector_buffer(
    const void *data, size_t len, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  return lonejson_value_rewrite_selector_buffer(
      test_default_runtime(), data, len, sink, sink_user, options, error);
}

static lonejson_status test_array_rewrite_reader(
    const char *selector, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (parse_options == NULL) {
    return lonejson_array_rewrite_reader(test_default_runtime(), selector,
                                         reader, reader_user, sink, sink_user,
                                         options, error);
  }
  return lonejson__array_rewrite_reader_with_options(
      selector, reader, reader_user, sink, sink_user, parse_options,
      runtime_state, options, error);
}

static lonejson_status test_array_rewrite_reader_to_filep(
    const char *selector, lonejson_reader_fn reader, void *reader_user,
    FILE *output, const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (output == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output file is required");
  }
  return test_array_rewrite_reader(selector, reader, reader_user,
                                   lonejson__sink_file, output, parse_options,
                                   options, error);
}

static lonejson_status test_array_rewrite_reader_to_fd(
    const char *selector, lonejson_reader_fn reader, void *reader_user, int fd,
    const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output fd must be non-negative");
  }
  return test_array_rewrite_reader(selector, reader, reader_user,
                                   lonejson__array_rewrite_fd_sink, &fd,
                                   parse_options, options, error);
}

static lonejson_status test_array_rewrite_filep(
    const char *selector, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input file is required");
  }
  return test_array_rewrite_reader(selector, lonejson__file_reader, fp, sink,
                                   sink_user, parse_options, options, error);
}

static lonejson_status test_array_rewrite_filep_to_filep(
    const char *selector, FILE *input, FILE *output,
    const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  return test_array_rewrite_filep(selector, input, lonejson__sink_file, output,
                                  parse_options, options, error);
}

static lonejson_status test_array_rewrite_fd(
    const char *selector, int fd, lonejson_sink_fn sink, void *sink_user,
    const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input fd must be non-negative");
  }
  return test_array_rewrite_reader(selector, lonejson__fd_reader, &fd, sink,
                                   sink_user, parse_options, options, error);
}

static lonejson_status
test_array_rewrite_fd_to_fd(const char *selector, int input_fd, int output_fd,
                            const lonejson__parse_options *parse_options,
                            const lonejson_array_rewrite_options *options,
                            lonejson_error *error) {
  if (output_fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output fd must be non-negative");
  }
  return test_array_rewrite_fd(selector, input_fd,
                               lonejson__array_rewrite_fd_sink, &output_fd,
                               parse_options, options, error);
}

static lonejson_status test_array_rewrite_path(
    const char *selector, const char *input_path, const char *output_path,
    const lonejson__parse_options *parse_options,
    const lonejson_array_rewrite_options *options, lonejson_error *error) {
  struct stat input_stat;
  struct stat output_stat;
  FILE *in;
  FILE *out;
  lonejson_status status;

  if (input_path == NULL || output_path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input and output paths are required");
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
  status = test_array_rewrite_filep(selector, in, lonejson__sink_file, out,
                                    parse_options, options, error);
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

static lonejson_array_stream *test_array_stream_open_reader(
    const char *path, lonejson_reader_fn reader, void *user,
    const lonejson__parse_options *options, lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_array_stream_open_reader(test_default_runtime(), path,
                                             reader, user, error);
  }
  return lonejson__array_stream_open_reader_with_options(
      path, reader, user, options, runtime_state, error);
}

static lonejson_array_stream *
test_array_stream_open_path(const char *array_path, const char *path,
                            const lonejson__parse_options *options,
                            lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_array_stream_open_path(test_default_runtime(), array_path,
                                           path, error);
  }
  return lonejson__array_stream_open_path_with_options(
      array_path, path, options, runtime_state, error);
}

static lonejson_array_stream *
test_array_stream_open_fd(const char *path, int fd,
                          const lonejson__parse_options *options,
                          lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_array_stream_open_fd(test_default_runtime(), path, fd,
                                         error);
  }
  return lonejson__array_stream_open_fd_with_options(path, fd, options,
                                                     runtime_state, error);
}

static lonejson_array_stream *
test_array_stream_open_push(const char *path,
                            const lonejson__parse_options *options,
                            lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_array_stream_open_push(test_default_runtime(), path, error);
  }
  return lonejson__array_stream_open_push_with_options(path, options,
                                                       runtime_state, error);
}

static lonejson_stream *
test_stream_open_reader(const lonejson_map *map, lonejson_reader_fn reader,
                        void *user, const lonejson__parse_options *options,
                        lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_stream_open_reader(test_default_runtime(), map, reader,
                                       user, error);
  }
  return lonejson__stream_open_reader_with_options(map, reader, user, options,
                                                   runtime_state, error);
}

static lonejson_stream *
test_stream_open_path(const lonejson_map *map, const char *path,
                      const lonejson__parse_options *options,
                      lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_stream_open_path(test_default_runtime(), map, path, error);
  }
  return lonejson__stream_open_path_with_options(map, path, options,
                                                 runtime_state, error);
}

static lonejson_stream *
test_stream_open_fd(const lonejson_map *map, int fd,
                    const lonejson__parse_options *options,
                    lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_stream_open_fd(test_default_runtime(), map, fd, error);
  }
  return lonejson__stream_open_fd_with_options(map, fd, options, runtime_state,
                                               error);
}

static lonejson_status test_serialize_sink(
    const lonejson_map *map, const void *src, lonejson_sink_fn sink, void *user,
    const lonejson__write_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_sink(test_default_runtime(), map, src, sink, user,
                                   error);
  }
  return lonejson__serialize_sink_with_options(map, src, sink, user, options,
                                               error);
}

static lonejson_status
test_serialize_buffer(const lonejson_map *map, const void *src, char *buffer,
                      size_t capacity, size_t *needed,
                      const lonejson__write_options *options,
                      lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_buffer(test_default_runtime(), map, src, buffer,
                                     capacity, needed, error);
  }
  return lonejson__serialize_buffer_with_options(map, src, buffer, capacity,
                                                 needed, options, error);
}

static char *test_serialize_alloc(const lonejson_map *map, const void *src,
                                  size_t *out_len,
                                  const lonejson__write_options *options,
                                  lonejson_error *error) {
  return lonejson__serialize_alloc_with_options(map, src, out_len, options,
                                                error);
}

static lonejson_status test_serialize_owned(
    const lonejson_map *map, const void *src, lonejson_owned_buffer *out,
    const lonejson__write_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_owned(test_default_runtime(), map, src, out,
                                    error);
  }
  return lonejson__serialize_owned_with_options(map, src, out, options, error);
}

static lonejson_status
test_serialize_filep(const lonejson_map *map, const void *src, FILE *fp,
                     const lonejson__write_options *options,
                     lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_filep(test_default_runtime(), map, src, fp,
                                    error);
  }
  return lonejson__serialize_filep_with_options(map, src, fp, options, error);
}

static lonejson_status
test_serialize_path(const lonejson_map *map, const void *src, const char *path,
                    const lonejson__write_options *options,
                    lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_path(test_default_runtime(), map, src, path,
                                   error);
  }
  return lonejson__serialize_path_with_options(map, src, path, options, error);
}

static lonejson_status
test_serialize_jsonl_sink(const lonejson_map *map, const void *items,
                          size_t count, size_t stride, lonejson_sink_fn sink,
                          void *user, const lonejson__write_options *options,
                          lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_jsonl_sink(test_default_runtime(), map, items,
                                         count, stride, sink, user, error);
  }
  return lonejson__serialize_jsonl_sink_with_options(
      map, items, count, stride, sink, user, options, error);
}

static lonejson_status test_serialize_jsonl_buffer(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lonejson__write_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_jsonl_buffer(test_default_runtime(), map, items,
                                           count, stride, buffer, capacity,
                                           needed, error);
  }
  return lonejson__serialize_jsonl_buffer_with_options(
      map, items, count, stride, buffer, capacity, needed, options, error);
}

static char *test_serialize_jsonl_alloc(const lonejson_map *map,
                                        const void *items, size_t count,
                                        size_t stride, size_t *out_len,
                                        const lonejson__write_options *options,
                                        lonejson_error *error) {
  return lonejson__serialize_jsonl_alloc_with_options(map, items, count, stride,
                                                      out_len, options, error);
}

static lonejson_status test_serialize_jsonl_filep(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    FILE *fp, const lonejson__write_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_jsonl_filep(test_default_runtime(), map, items,
                                          count, stride, fp, error);
  }
  return lonejson__serialize_jsonl_filep_with_options(map, items, count, stride,
                                                      fp, options, error);
}

static lonejson_status
test_serialize_jsonl_path(const lonejson_map *map, const void *items,
                          size_t count, size_t stride, const char *path,
                          const lonejson__write_options *options,
                          lonejson_error *error) {
  if (options == NULL) {
    return lonejson_serialize_jsonl_path(test_default_runtime(), map, items,
                                         count, stride, path, error);
  }
  return lonejson__serialize_jsonl_path_with_options(map, items, count, stride,
                                                     path, options, error);
}

static lonejson_status
test_generator_init(lonejson_generator *generator, const lonejson_map *map,
                    const void *src, const lonejson__write_options *options) {
  if (options == NULL) {
    return lonejson_generator_init(test_default_runtime(), generator, map, src);
  }
  return lonejson__generator_init_with_options(generator, map, src, options);
}

static lonejson_status
test_generator_measure(const lonejson_map *map, const void *src,
                       size_t *out_len, const lonejson__write_options *options,
                       lonejson_error *error) {
  if (options == NULL) {
    return test_default_runtime()->generator_measure(test_default_runtime(),
                                                     map, src, out_len, error);
  }
  return lonejson__generator_measure_with_options(map, src, out_len, options,
                                                  error);
}

static lonejson_status
test_writer_init_sink(lonejson_writer *writer, lonejson_sink_fn sink,
                      void *sink_user, const lonejson__write_options *options,
                      lonejson_error *error) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return lonejson_writer_init_sink(test_default_runtime(), writer, sink,
                                     sink_user, error);
  }
  return lonejson__writer_init_sink_with_options(writer, sink, sink_user,
                                                 options, runtime_state, error);
}

static lonejson_status test_writer_generator_init(
    lonejson_generator *generator, lonejson_writer_producer_fn producer,
    void *producer_user, const lonejson__write_options *options) {
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  if (options == NULL) {
    return test_default_runtime()->writer_generator_init(
        test_default_runtime(), generator, producer, producer_user);
  }
  return lonejson__writer_generator_init_with_options(
      generator, producer, producer_user, options, runtime_state);
}

static lonejson_status
test_write_json_string_sink(lonejson_reader_fn reader, void *reader_user,
                            lonejson_sink_fn sink, void *sink_user,
                            const lonejson__write_options *options,
                            lonejson_error *error) {
  if (options == NULL) {
    return lonejson_write_json_string_sink(test_default_runtime(), reader,
                                           reader_user, sink, sink_user, error);
  }
  {
    lonejson_writer writer;
    lonejson_status status = lonejson__writer_init_sink_with_options(
        &writer, sink, sink_user, options,
        lonejson__runtime_const(test_default_runtime()), error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson_writer_string_reader(&writer, reader, reader_user, error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson_writer_finish(&writer, error);
    }
    lonejson_writer_cleanup(&writer);
    return status;
  }
}

static lonejson_status test_writer_value_stream_open(
    lonejson_writer_value_stream *stream, lonejson_writer *writer,
    const lonejson__value_limits *limits, lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_value_stream_open(stream, writer, error);
  }
  return lonejson__writer_value_stream_open_with_limits(stream, writer, limits,
                                                        error);
}

static lonejson_status test_writer_json_value_reader(
    lonejson_writer *writer, lonejson_reader_fn reader, void *reader_user,
    const lonejson__value_limits *limits, lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_json_value_reader(writer, reader, reader_user,
                                             error);
  }
  return lonejson__writer_json_value_reader_with_limits(
      writer, reader, reader_user, limits, error);
}

static lonejson_status
test_writer_json_value_buffer(lonejson_writer *writer, const void *data,
                              size_t len, const lonejson__value_limits *limits,
                              lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_json_value_buffer(writer, data, len, error);
  }
  return lonejson__writer_json_value_buffer_with_limits(writer, data, len,
                                                        limits, error);
}

static lonejson_status
test_writer_json_value_file(lonejson_writer *writer, FILE *fp,
                            const lonejson__value_limits *limits,
                            lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_json_value_file(writer, fp, error);
  }
  return lonejson__writer_json_value_file_with_limits(writer, fp, limits,
                                                      error);
}

static lonejson_status
test_writer_json_value_fd(lonejson_writer *writer, int fd,
                          const lonejson__value_limits *limits,
                          lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_json_value_fd(writer, fd, error);
  }
  return lonejson__writer_json_value_fd_with_limits(writer, fd, limits, error);
}

static lonejson_status
test_writer_json_value_path(lonejson_writer *writer, const char *path,
                            const lonejson__value_limits *limits,
                            lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_json_value_path(writer, path, error);
  }
  return lonejson__writer_json_value_path_with_limits(writer, path, limits,
                                                      error);
}

static lonejson_status test_writer_json_value_spooled(
    lonejson_writer *writer, const lonejson_spooled *value,
    const lonejson__value_limits *limits, lonejson_error *error) {
  if (limits == NULL) {
    return lonejson_writer_json_value_spooled(writer, value, error);
  }
  return lonejson__writer_json_value_spooled_with_limits(writer, value, limits,
                                                         error);
}

static lonejson_status
test_writer_array_items_reader(lonejson_writer *writer, const char *selector,
                               lonejson_reader_fn reader, void *reader_user,
                               const lonejson__parse_options *options,
                               lonejson_error *error) {
  if (options == NULL) {
    return lonejson_writer_array_items_reader(writer, selector, reader,
                                              reader_user, error);
  }
  return lonejson__writer_array_items_reader_with_options(
      writer, selector, reader, reader_user, options, error);
}

static lonejson_status test_writer_array_items_buffer(
    lonejson_writer *writer, const char *selector, const void *data, size_t len,
    const lonejson__parse_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_writer_array_items_buffer(writer, selector, data, len,
                                              error);
  }
  return lonejson__writer_array_items_buffer_with_options(
      writer, selector, data, len, options, error);
}

static lonejson_status
test_writer_array_items_filep(lonejson_writer *writer, const char *selector,
                              FILE *fp, const lonejson__parse_options *options,
                              lonejson_error *error) {
  if (options == NULL) {
    return lonejson_writer_array_items_filep(writer, selector, fp, error);
  }
  return lonejson__writer_array_items_filep_with_options(writer, selector, fp,
                                                         options, error);
}

static lonejson_status
test_writer_array_items_fd(lonejson_writer *writer, const char *selector,
                           int fd, const lonejson__parse_options *options,
                           lonejson_error *error) {
  if (options == NULL) {
    return lonejson_writer_array_items_fd(writer, selector, fd, error);
  }
  return lonejson__writer_array_items_fd_with_options(writer, selector, fd,
                                                      options, error);
}

static lonejson_status test_writer_array_items_path(
    lonejson_writer *writer, const char *selector, const char *path,
    const lonejson__parse_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_writer_array_items_path(writer, selector, path, error);
  }
  return lonejson__writer_array_items_path_with_options(writer, selector, path,
                                                        options, error);
}

static lonejson_status
test_writer_array_items_spooled(lonejson_writer *writer, const char *selector,
                                const lonejson_spooled *value,
                                const lonejson__parse_options *options,
                                lonejson_error *error) {
  if (options == NULL) {
    return lonejson_writer_array_items_spooled(writer, selector, value, error);
  }
  return lonejson__writer_array_items_spooled_with_options(
      writer, selector, value, options, error);
}

static lonejson_status test_write_json_string_spooled_sink(
    const lonejson_spooled *value, lonejson_sink_fn sink, void *sink_user,
    const lonejson__write_options *options, lonejson_error *error) {
  if (options == NULL) {
    return lonejson_write_json_string_spooled_sink(
        test_default_runtime(), value, sink, sink_user, error);
  }
  {
    lonejson_writer writer;
    lonejson_status status = lonejson__writer_init_sink_with_options(
        &writer, sink, sink_user, options,
        lonejson__runtime_const(test_default_runtime()), error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson_writer_string_spooled(&writer, value, error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson_writer_finish(&writer, error);
    }
    lonejson_writer_cleanup(&writer);
    return status;
  }
}

static void *test_no_realloc_malloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void *test_no_realloc_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)ptr;
  (void)size;
  return NULL;
}

static void test_no_realloc_free(void *ctx, void *ptr) {
  (void)ctx;
  free(ptr);
}

typedef struct test_realloc_limit_state {
  size_t max_realloc_size;
  size_t observed_max_realloc_size;
} test_realloc_limit_state;

static void *test_realloc_limit_malloc(void *ctx, size_t size) {
  test_realloc_limit_state *state = (test_realloc_limit_state *)ctx;

  if (state != NULL && size > state->observed_max_realloc_size) {
    state->observed_max_realloc_size = size;
  }
  return malloc(size);
}

static void *test_realloc_limit_realloc(void *ctx, void *ptr, size_t size) {
  test_realloc_limit_state *state = (test_realloc_limit_state *)ctx;

  if (state != NULL && size > state->observed_max_realloc_size) {
    state->observed_max_realloc_size = size;
  }
  if (state != NULL && size > state->max_realloc_size) {
    return NULL;
  }
  return realloc(ptr, size);
}

static void test_realloc_limit_free(void *ctx, void *ptr) {
  (void)ctx;
  free(ptr);
}

#ifndef NDEBUG
static void *test_misaligned_allocator_malloc(void *ctx, size_t size) {
  unsigned char *raw;

  (void)ctx;
  raw = (unsigned char *)malloc(size + 1u);
  if (raw == NULL) {
    return NULL;
  }
  return raw + 1u;
}

static void *test_misaligned_allocator_realloc(void *ctx, void *ptr,
                                               size_t size) {
  unsigned char *raw;

  (void)ctx;
  if (ptr != NULL) {
    free(((unsigned char *)ptr) - 1u);
  }
  raw = (unsigned char *)malloc(size + 1u);
  if (raw == NULL) {
    return NULL;
  }
  return raw + 1u;
}

static void test_misaligned_allocator_free(void *ctx, void *ptr) {
  (void)ctx;
  if (ptr != NULL) {
    free(((unsigned char *)ptr) - 1u);
  }
}
#endif

static void test_allocator_init(test_allocator_state *state) {
  memset(state, 0, sizeof(*state));
  state->allocator = lonejson_default_allocator();
  state->allocator.malloc_fn = test_allocator_malloc;
  state->allocator.realloc_fn = test_allocator_realloc;
  state->allocator.free_fn = test_allocator_free;
  state->allocator.stats = &state->stats;
}

#if LONEJSON_TEST_HAS_PTHREAD
static void *test_serial_allocator_malloc(void *ctx, size_t size) {
  test_serial_allocator_state *state = (test_serial_allocator_state *)ctx;
  void *ptr;

  pthread_mutex_lock(&state->mutex);
  state->active_calls += 1;
  if (state->active_calls > 1) {
    state->overlap_detected = 1;
  }
  usleep(1000);
  ptr = malloc(size);
  state->active_calls -= 1;
  pthread_mutex_unlock(&state->mutex);
  return ptr;
}

static void *test_serial_allocator_realloc(void *ctx, void *ptr, size_t size) {
  test_serial_allocator_state *state = (test_serial_allocator_state *)ctx;
  void *next;

  pthread_mutex_lock(&state->mutex);
  state->active_calls += 1;
  if (state->active_calls > 1) {
    state->overlap_detected = 1;
  }
  usleep(1000);
  next = realloc(ptr, size);
  state->active_calls -= 1;
  pthread_mutex_unlock(&state->mutex);
  return next;
}

static void test_serial_allocator_free(void *ctx, void *ptr) {
  test_serial_allocator_state *state = (test_serial_allocator_state *)ctx;

  pthread_mutex_lock(&state->mutex);
  state->active_calls += 1;
  if (state->active_calls > 1) {
    state->overlap_detected = 1;
  }
  usleep(1000);
  free(ptr);
  state->active_calls -= 1;
  pthread_mutex_unlock(&state->mutex);
}

static void test_serial_allocator_init(test_serial_allocator_state *state) {
  memset(state, 0, sizeof(*state));
  state->allocator = lonejson_default_allocator();
  state->allocator.ctx = state;
  state->allocator.malloc_fn = test_serial_allocator_malloc;
  state->allocator.realloc_fn = test_serial_allocator_realloc;
  state->allocator.free_fn = test_serial_allocator_free;
  state->allocator.stats = &state->stats;
  (void)pthread_mutex_init(&state->mutex, NULL);
}

static void test_serial_allocator_destroy(test_serial_allocator_state *state) {
  (void)pthread_mutex_destroy(&state->mutex);
}
#endif

typedef struct test_fail_after_allocator_state {
  lonejson_allocator allocator;
  size_t calls;
  size_t successful_calls_before_failure;
} test_fail_after_allocator_state;

static void *test_fail_after_allocator_malloc(void *ctx, size_t size) {
  test_fail_after_allocator_state *state =
      (test_fail_after_allocator_state *)ctx;

  if (state->calls >= state->successful_calls_before_failure) {
    return NULL;
  }
  state->calls++;
  return malloc(size);
}

static void *test_fail_after_allocator_realloc(void *ctx, void *ptr,
                                               size_t size) {
  test_fail_after_allocator_state *state =
      (test_fail_after_allocator_state *)ctx;

  if (state->calls >= state->successful_calls_before_failure) {
    return NULL;
  }
  state->calls++;
  return realloc(ptr, size);
}

static void test_fail_after_allocator_free(void *ctx, void *ptr) {
  (void)ctx;
  free(ptr);
}

static void
test_fail_after_allocator_init(test_fail_after_allocator_state *state,
                               size_t successful_calls_before_failure) {
  memset(state, 0, sizeof(*state));
  state->allocator = lonejson_default_allocator();
  state->allocator.ctx = state;
  state->allocator.malloc_fn = test_fail_after_allocator_malloc;
  state->allocator.realloc_fn = test_fail_after_allocator_realloc;
  state->allocator.free_fn = test_fail_after_allocator_free;
  state->successful_calls_before_failure = successful_calls_before_failure;
}

typedef struct test_address {
  char city[16];
  lonejson_int64 zip;
} test_address;

typedef struct test_item {
  lonejson_int64 id;
  char label[12];
} test_item;

typedef struct test_fixed_log {
  char name[8];
  lonejson_i64_array codes;
} test_fixed_log;

typedef struct test_event {
  char id[8];
  bool ok;
} test_event;

typedef struct test_large_meta {
  lonejson_int64 shard;
  bool ok;
} test_large_meta;

typedef struct test_large_stream_record {
  lonejson_int64 id;
  char name[16];
  char kind[8];
  char payload[32];
  char base64[16];
  test_large_meta meta;
} test_large_stream_record;

typedef struct test_spool_doc {
  char id[16];
  lonejson_spooled text;
  lonejson_spooled bytes;
} test_spool_doc;

typedef struct test_spool_limits_doc {
  lonejson_spooled text;
} test_spool_limits_doc;

typedef struct test_source_doc {
  char id[16];
  lonejson_source text;
  lonejson_source bytes;
} test_source_doc;

typedef struct test_json_value_doc {
  char id[16];
  lonejson_json_value selector;
  lonejson_json_value fields;
  lonejson_json_value last_error;
} test_json_value_doc;

typedef struct test_nested_json_value_response {
  char status[16];
  lonejson_json_value payload;
} test_nested_json_value_response;

typedef struct test_nested_json_value_doc {
  char type[32];
  test_nested_json_value_response response;
} test_nested_json_value_doc;

typedef struct test_alloc_parse_doc {
  char *name;
  lonejson_spooled body;
  lonejson_string_array tags;
} test_alloc_parse_doc;

typedef struct test_response_json_doc {
  char *response_json;
} test_response_json_doc;

typedef struct test_alloc_json_value_doc {
  char id[16];
  lonejson_json_value selector;
} test_alloc_json_value_doc;

typedef struct test_sse_large_event_doc {
  char *type;
  char *status;
  char *text;
} test_sse_large_event_doc;

typedef struct test_fixed_result_row {
  char name[32];
  char group[16];
} test_fixed_result_row;

typedef struct test_fixed_result_run {
  lonejson_object_array results;
  test_fixed_result_row result_storage[4];
} test_fixed_result_run;

typedef struct test_aligned_item {
  char name[8];
  long double weight;
} test_aligned_item;

typedef struct test_aligned_doc {
  lonejson_object_array items;
} test_aligned_doc;

typedef struct test_omit_reasoning {
  char *effort;
  char *summary;
} test_omit_reasoning;

typedef struct test_omit_deep_leaf {
  char *value;
} test_omit_deep_leaf;

typedef struct test_omit_deep_middle {
  test_omit_deep_leaf leaf;
} test_omit_deep_middle;

typedef struct test_omit_deep_doc {
  char *id;
  test_omit_deep_middle middle;
} test_omit_deep_doc;

typedef struct test_omit_doc {
  char *model;
  char *instructions;
  lonejson_int64 max_output_tokens;
  int has_max_output_tokens;
  lonejson_uint64 seed;
  int has_seed;
  double temperature;
  int has_temperature;
  bool stream;
  int has_stream;
  test_omit_reasoning reasoning;
  lonejson_string_array tags;
  lonejson_object_array tools;
  test_item tool_storage[1];
  lonejson_source payload;
  lonejson_json_value metadata;
} test_omit_doc;

typedef struct test_nullable_child {
  lonejson_int64 priority;
  int has_priority;
} test_nullable_child;

typedef struct test_nullable_primitives_doc {
  lonejson_int64 count;
  int has_count;
  lonejson_uint64 seed;
  int has_seed;
  double ratio;
  int has_ratio;
  bool enabled;
  int has_enabled;
  lonejson_int64 required_count;
  test_nullable_child child;
} test_nullable_primitives_doc;

typedef struct test_conditional_doc {
  char *dyn;
  char fixed[8];
  lonejson_spooled text;
  lonejson_spooled bytes;
  lonejson_i64_array ints;
  lonejson_u64_array uints;
  lonejson_f64_array nums;
  lonejson_bool_array bools;
} test_conditional_doc;

typedef struct test_two_alloc_string_doc {
  char *first;
  char *second;
} test_two_alloc_string_doc;

typedef struct test_large_fixed_string_doc {
  char payload[8192];
} test_large_fixed_string_doc;

typedef struct test_alloc_fixed_combo_doc {
  char *dyn;
  char payload[8192];
} test_alloc_fixed_combo_doc;

typedef struct test_trunc_array_doc {
  lonejson_i64_array ints;
  lonejson_string_array tags;
  lonejson_object_array items;
} test_trunc_array_doc;

typedef struct test_reader_state {
  const char *json;
  size_t offset;
  size_t chunk_size;
} test_reader_state;

typedef struct test_empty_read_state {
  test_reader_state inner;
  size_t empty_reads_remaining;
} test_empty_read_state;

typedef struct test_intermittent_empty_read_state {
  test_reader_state inner;
  int return_empty_next;
  size_t empty_reads;
} test_intermittent_empty_read_state;

typedef struct test_would_block_read_state {
  const char *json;
  size_t offset;
  size_t chunk_size;
  size_t block_after;
  int blocked;
} test_would_block_read_state;

typedef struct test_would_block_tripwire_read_state {
  const char *json;
  size_t offset;
  size_t chunk_size;
  size_t block_after;
  int blocked;
  size_t call_count;
  int reentered_after_block;
} test_would_block_tripwire_read_state;

typedef struct test_counting_sink {
  size_t total;
} test_counting_sink;

typedef struct test_buffer_sink {
  unsigned char *buffer;
  size_t capacity;
  size_t length;
} test_buffer_sink;

typedef struct test_failing_sink {
  size_t fail_after;
  size_t total;
} test_failing_sink;

typedef struct test_sse_state {
  size_t count;
  size_t begin_count;
  size_t data_count;
  char event[32];
  char id[32];
  char data[256];
  size_t data_len;
  unsigned long retry_ms;
  int has_retry;
  int fail;
} test_sse_state;

typedef struct test_sse_json_state {
  size_t count;
  char event[32];
  char id[32];
  size_t data_len;
  unsigned long retry_ms;
  int has_retry;
  int invalid;
} test_sse_json_state;

typedef struct test_multipart_state {
  size_t begin_count;
  size_t data_count;
  size_t end_count;
  size_t data_chunks[8];
  char names[4][32];
  char content_types[4][64];
  lonejson_int64 lengths[4];
  char data[512];
  size_t data_len;
  const lonejson_allocator_stats *stats;
  size_t alloc_calls_after_headers;
  size_t realloc_calls_after_headers;
  int check_no_body_alloc;
  int saw_body_alloc;
  int fail_begin;
  int fail_data;
  int fail_end;
} test_multipart_state;

typedef struct test_visit_state {
  char log[4096];
  size_t len;
} test_visit_state;

typedef struct test_visit_chunk_state {
  size_t object_count;
  size_t array_count;
  size_t key_begin_count;
  size_t key_chunk_count;
  size_t key_end_count;
  size_t string_begin_count;
  size_t string_chunk_count;
  size_t string_end_count;
  size_t number_begin_count;
  size_t number_chunk_count;
  size_t number_end_count;
  size_t bool_count;
  size_t null_count;
  size_t callback_count;
  size_t fail_after;
  char last_key[256];
  size_t last_key_len;
  char last_string[256];
  size_t last_string_len;
  char last_number[128];
  size_t last_number_len;
} test_visit_chunk_state;

typedef struct test_person {
  char *name;
  char nickname[8];
  lonejson_int64 age;
  double score;
  bool active;
  test_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_string_array tags;
  lonejson_object_array items;
} test_person;

static const lonejson_field test_address_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_address, city, "city",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(test_address, zip, "zip")};
LONEJSON_MAP_DEFINE(test_address_map, test_address, test_address_fields);

static const lonejson_field test_item_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_item, id, "id"),
    LONEJSON_FIELD_STRING_FIXED(test_item, label, "label",
                                LONEJSON_OVERFLOW_TRUNCATE)};
LONEJSON_MAP_DEFINE(test_item_map, test_item, test_item_fields);

static const lonejson_field test_fixed_log_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_fixed_log, name, "name",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64_ARRAY(test_fixed_log, codes, "codes",
                             LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_fixed_log_map, test_fixed_log, test_fixed_log_fields);

static const lonejson_field test_event_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_event, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BOOL(test_event, ok, "ok")};
LONEJSON_MAP_DEFINE(test_event_map, test_event, test_event_fields);

static const lonejson_field test_large_meta_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_large_meta, shard, "shard"),
    LONEJSON_FIELD_BOOL_REQ(test_large_meta, ok, "ok")};
LONEJSON_MAP_DEFINE(test_large_meta_map, test_large_meta,
                    test_large_meta_fields);

static const lonejson_field test_large_stream_record_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_large_stream_record, id, "id"),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, name, "name",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, kind, "kind",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, payload,
                                    "payload", LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, base64, "base64",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_OBJECT_REQ(test_large_stream_record, meta, "meta",
                              &test_large_meta_map)};
LONEJSON_MAP_DEFINE(test_large_stream_record_map, test_large_stream_record,
                    test_large_stream_record_fields);

static const lonejson__spool_options test_spool_small_options = {64u, 0u, NULL};
static const lonejson__spool_options test_spool_limited_options = {32u, 96u,
                                                                   NULL};

static const lonejson_field test_spool_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_spool_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_STREAM_OPTS(test_spool_doc, text, "text",
                                      &test_spool_small_options),
    LONEJSON_FIELD_BASE64_STREAM_OPTS(test_spool_doc, bytes, "bytes",
                                      &test_spool_small_options)};
LONEJSON_MAP_DEFINE(test_spool_doc_map, test_spool_doc, test_spool_doc_fields);

static const lonejson_field test_spool_limits_doc_fields[] = {
    LONEJSON_FIELD_STRING_STREAM_OPTS(test_spool_limits_doc, text, "text",
                                      &test_spool_limited_options)};
LONEJSON_MAP_DEFINE(test_spool_limits_doc_map, test_spool_limits_doc,
                    test_spool_limits_doc_fields);

static const lonejson_field test_source_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_source_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_SOURCE(test_source_doc, text, "text"),
    LONEJSON_FIELD_BASE64_SOURCE(test_source_doc, bytes, "bytes")};
LONEJSON_MAP_DEFINE(test_source_doc_map, test_source_doc,
                    test_source_doc_fields);

static const lonejson_field test_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_json_value_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(test_json_value_doc, selector, "selector"),
    LONEJSON_FIELD_JSON_VALUE(test_json_value_doc, fields, "fields"),
    LONEJSON_FIELD_JSON_VALUE(test_json_value_doc, last_error, "last_error")};
LONEJSON_MAP_DEFINE(test_json_value_doc_map, test_json_value_doc,
                    test_json_value_doc_fields);

static const lonejson_field test_nested_json_value_response_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_nested_json_value_response, status,
                                "status", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(test_nested_json_value_response, payload,
                                  "payload")};
LONEJSON_MAP_DEFINE(test_nested_json_value_response_map,
                    test_nested_json_value_response,
                    test_nested_json_value_response_fields);

static const lonejson_field test_nested_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_nested_json_value_doc, type, "type",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_REQ(test_nested_json_value_doc, response, "response",
                              &test_nested_json_value_response_map)};
LONEJSON_MAP_DEFINE(test_nested_json_value_doc_map, test_nested_json_value_doc,
                    test_nested_json_value_doc_fields);

static const lonejson_field test_alloc_parse_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_alloc_parse_doc, name, "name"),
    LONEJSON_FIELD_STRING_STREAM_OPTS(test_alloc_parse_doc, body, "body",
                                      &test_spool_small_options),
    LONEJSON_FIELD_STRING_ARRAY(test_alloc_parse_doc, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_alloc_parse_doc_map, test_alloc_parse_doc,
                    test_alloc_parse_doc_fields);

static const lonejson_field test_response_json_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_response_json_doc, response_json,
                                    "response_json")};
LONEJSON_MAP_DEFINE(test_response_json_doc_map, test_response_json_doc,
                    test_response_json_doc_fields);

static const lonejson_field test_alloc_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_alloc_json_value_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(test_alloc_json_value_doc, selector,
                                  "selector")};
LONEJSON_MAP_DEFINE(test_alloc_json_value_doc_map, test_alloc_json_value_doc,
                    test_alloc_json_value_doc_fields);

static const lonejson_field test_sse_large_event_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_sse_large_event_doc, type, "type"),
    LONEJSON_FIELD_STRING_ALLOC(test_sse_large_event_doc, status, "status"),
    LONEJSON_FIELD_STRING_ALLOC(test_sse_large_event_doc, text, "text")};
LONEJSON_MAP_DEFINE(test_sse_large_event_doc_map, test_sse_large_event_doc,
                    test_sse_large_event_doc_fields);

static const lonejson_field test_fixed_result_row_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_fixed_result_row, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_fixed_result_row, group, "group",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_fixed_result_row_map, test_fixed_result_row,
                    test_fixed_result_row_fields);

static const lonejson_field test_fixed_result_run_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(
        test_fixed_result_run, results, "results", test_fixed_result_row,
        &test_fixed_result_row_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_fixed_result_run_map, test_fixed_result_run,
                    test_fixed_result_run_fields);

static const lonejson_field test_aligned_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_aligned_item, name, "name",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_aligned_item_map, test_aligned_item,
                    test_aligned_item_fields);

static const lonejson_field test_aligned_doc_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(test_aligned_doc, items, "items",
                                test_aligned_item, &test_aligned_item_map,
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_aligned_doc_map, test_aligned_doc,
                    test_aligned_doc_fields);

static const lonejson_field test_omit_reasoning_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_reasoning, effort,
                                          "effort"),
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_reasoning, summary,
                                          "summary")};
LONEJSON_MAP_DEFINE(test_omit_reasoning_map, test_omit_reasoning,
                    test_omit_reasoning_fields);

static const lonejson_field test_omit_deep_leaf_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_deep_leaf, value, "value")};
LONEJSON_MAP_DEFINE(test_omit_deep_leaf_map, test_omit_deep_leaf,
                    test_omit_deep_leaf_fields);

static const lonejson_field test_omit_deep_middle_fields[] = {
    LONEJSON_FIELD_OBJECT_OMIT_EMPTY(test_omit_deep_middle, leaf, "leaf",
                                     &test_omit_deep_leaf_map)};
LONEJSON_MAP_DEFINE(test_omit_deep_middle_map, test_omit_deep_middle,
                    test_omit_deep_middle_fields);

static const lonejson_field test_omit_deep_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_deep_doc, id, "id"),
    LONEJSON_FIELD_OBJECT_OMIT_EMPTY(test_omit_deep_doc, middle, "middle",
                                     &test_omit_deep_middle_map)};
LONEJSON_MAP_DEFINE(test_omit_deep_doc_map, test_omit_deep_doc,
                    test_omit_deep_doc_fields);

static const lonejson_field test_omit_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_doc, model, "model"),
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_doc, instructions,
                                          "instructions"),
    LONEJSON_FIELD_I64_PRESENT(test_omit_doc, max_output_tokens,
                               has_max_output_tokens, "max_output_tokens"),
    LONEJSON_FIELD_U64_PRESENT(test_omit_doc, seed, has_seed, "seed"),
    LONEJSON_FIELD_F64_PRESENT(test_omit_doc, temperature, has_temperature,
                               "temperature"),
    LONEJSON_FIELD_BOOL_PRESENT(test_omit_doc, stream, has_stream, "stream"),
    LONEJSON_FIELD_OBJECT_OMIT_EMPTY(test_omit_doc, reasoning, "reasoning",
                                     &test_omit_reasoning_map),
    LONEJSON_FIELD_STRING_ARRAY_OMIT_EMPTY(test_omit_doc, tags, "tags",
                                           LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY_OMIT_EMPTY(test_omit_doc, tools, "tools",
                                           test_item, &test_item_map,
                                           LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_SOURCE_OMIT_NULL(test_omit_doc, payload, "payload"),
    LONEJSON_FIELD_JSON_VALUE_OMIT_NULL(test_omit_doc, metadata, "metadata")};
LONEJSON_MAP_DEFINE(test_omit_doc_map, test_omit_doc, test_omit_doc_fields);

static const lonejson_field test_bad_required_omit_fields[] = {
    {"model", LONEJSON__KEY_LEN("model"), LONEJSON__KEY_FIRST("model"),
     LONEJSON__KEY_LAST("model"), offsetof(test_omit_doc, model),
     LONEJSON_FIELD_KIND_STRING, LONEJSON_STORAGE_DYNAMIC,
     LONEJSON_OVERFLOW_FAIL, LONEJSON_FIELD_REQUIRED | LONEJSON_FIELD_OMIT_NULL,
     0u, 0u, NULL, NULL, 0u, LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_bad_required_omit_map, test_omit_doc,
                    test_bad_required_omit_fields);

static const lonejson_field test_bad_required_presence_fields[] = {
    {"max_output_tokens", LONEJSON__KEY_LEN("max_output_tokens"),
     LONEJSON__KEY_FIRST("max_output_tokens"),
     LONEJSON__KEY_LAST("max_output_tokens"),
     offsetof(test_omit_doc, max_output_tokens), LONEJSON_FIELD_KIND_I64,
     LONEJSON_STORAGE_FIXED, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_REQUIRED | LONEJSON_FIELD_HAS_PRESENCE, 0u, 0u, NULL, NULL,
     offsetof(test_omit_doc, has_max_output_tokens),
     LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_bad_required_presence_map, test_omit_doc,
                    test_bad_required_presence_fields);

static const lonejson_field test_bad_presence_kind_fields[] = {
    {"instructions", LONEJSON__KEY_LEN("instructions"),
     LONEJSON__KEY_FIRST("instructions"), LONEJSON__KEY_LAST("instructions"),
     offsetof(test_omit_doc, instructions), LONEJSON_FIELD_KIND_STRING,
     LONEJSON_STORAGE_DYNAMIC, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_HAS_PRESENCE, 0u, 0u, NULL, NULL,
     offsetof(test_omit_doc, has_stream), LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_bad_presence_kind_map, test_omit_doc,
                    test_bad_presence_kind_fields);

static const lonejson_field test_bad_presence_offset_fields[] = {
    {"max_output_tokens", LONEJSON__KEY_LEN("max_output_tokens"),
     LONEJSON__KEY_FIRST("max_output_tokens"),
     LONEJSON__KEY_LAST("max_output_tokens"),
     offsetof(test_omit_doc, max_output_tokens), LONEJSON_FIELD_KIND_I64,
     LONEJSON_STORAGE_FIXED, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_HAS_PRESENCE, 0u, 0u, NULL, NULL, sizeof(test_omit_doc),
     LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_bad_presence_offset_map, test_omit_doc,
                    test_bad_presence_offset_fields);

static const lonejson_field test_nullable_child_fields[] = {
    LONEJSON_FIELD_I64_PRESENT_NULLABLE(test_nullable_child, priority,
                                        has_priority, "priority")};
LONEJSON_MAP_DEFINE(test_nullable_child_map, test_nullable_child,
                    test_nullable_child_fields);

static const lonejson_field test_nullable_primitives_fields[] = {
    LONEJSON_FIELD_I64_PRESENT_NULLABLE(test_nullable_primitives_doc, count,
                                        has_count, "count"),
    LONEJSON_FIELD_U64_PRESENT_NULLABLE(test_nullable_primitives_doc, seed,
                                        has_seed, "seed"),
    LONEJSON_FIELD_F64_PRESENT_NULLABLE(test_nullable_primitives_doc, ratio,
                                        has_ratio, "ratio"),
    LONEJSON_FIELD_BOOL_PRESENT_NULLABLE(test_nullable_primitives_doc, enabled,
                                         has_enabled, "enabled"),
    LONEJSON_FIELD_I64_REQ(test_nullable_primitives_doc, required_count,
                           "required_count"),
    LONEJSON_FIELD_OBJECT(test_nullable_primitives_doc, child, "child",
                          &test_nullable_child_map)};
LONEJSON_MAP_DEFINE(test_nullable_primitives_map, test_nullable_primitives_doc,
                    test_nullable_primitives_fields);

static const lonejson_field test_bad_nullable_without_presence_fields[] = {
    {"count", LONEJSON__KEY_LEN("count"), LONEJSON__KEY_FIRST("count"),
     LONEJSON__KEY_LAST("count"), offsetof(test_nullable_primitives_doc, count),
     LONEJSON_FIELD_KIND_I64, LONEJSON_STORAGE_FIXED, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_ACCEPT_NULL, 0u, 0u, NULL, NULL, 0u,
     LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_bad_nullable_without_presence_map,
                    test_nullable_primitives_doc,
                    test_bad_nullable_without_presence_fields);

static const lonejson_field test_bad_nullable_kind_fields[] = {
    {"child", LONEJSON__KEY_LEN("child"), LONEJSON__KEY_FIRST("child"),
     LONEJSON__KEY_LAST("child"), offsetof(test_nullable_primitives_doc, child),
     LONEJSON_FIELD_KIND_OBJECT, LONEJSON_STORAGE_FIXED, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL, 0u, 0u,
     &test_nullable_child_map, NULL,
     offsetof(test_nullable_primitives_doc, has_count),
     LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_bad_nullable_kind_map, test_nullable_primitives_doc,
                    test_bad_nullable_kind_fields);

static const lonejson_field test_pretty_omit_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_doc, instructions,
                                          "instructions"),
    LONEJSON_FIELD_I64_PRESENT(test_omit_doc, max_output_tokens,
                               has_max_output_tokens, "max_output_tokens"),
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_doc, model, "model"),
    LONEJSON_FIELD_JSON_VALUE_OMIT_NULL(test_omit_doc, metadata, "metadata")};
LONEJSON_MAP_DEFINE(test_pretty_omit_map, test_omit_doc,
                    test_pretty_omit_fields);

static const lonejson_field test_omit_empty_null_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_doc, model, "model"),
    {"instructions", LONEJSON__KEY_LEN("instructions"),
     LONEJSON__KEY_FIRST("instructions"), LONEJSON__KEY_LAST("instructions"),
     offsetof(test_omit_doc, instructions), LONEJSON_FIELD_KIND_STRING,
     LONEJSON_STORAGE_DYNAMIC, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_OMIT_EMPTY, 0u, 0u, NULL, NULL, 0u,
     LONEJSON_SPOOL_CLASS_DEFAULT},
    {"payload", LONEJSON__KEY_LEN("payload"), LONEJSON__KEY_FIRST("payload"),
     LONEJSON__KEY_LAST("payload"), offsetof(test_omit_doc, payload),
     LONEJSON_FIELD_KIND_STRING_SOURCE, LONEJSON_STORAGE_DYNAMIC,
     LONEJSON_OVERFLOW_FAIL, LONEJSON_FIELD_OMIT_EMPTY, 0u, 0u, NULL, NULL, 0u,
     LONEJSON_SPOOL_CLASS_DEFAULT},
    {"metadata", LONEJSON__KEY_LEN("metadata"), LONEJSON__KEY_FIRST("metadata"),
     LONEJSON__KEY_LAST("metadata"), offsetof(test_omit_doc, metadata),
     LONEJSON_FIELD_KIND_JSON_VALUE, LONEJSON_STORAGE_DYNAMIC,
     LONEJSON_OVERFLOW_FAIL, LONEJSON_FIELD_OMIT_EMPTY, 0u, 0u, NULL, NULL, 0u,
     LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(test_omit_empty_null_map, test_omit_doc,
                    test_omit_empty_null_fields);

static const lonejson_field test_conditional_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_EMPTY(test_conditional_doc, dyn, "dyn"),
    LONEJSON_FIELD_STRING_FIXED_OMIT_EMPTY(test_conditional_doc, fixed, "fixed",
                                           LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_STREAM_OMIT_EMPTY(test_conditional_doc, text, "text"),
    LONEJSON_FIELD_BASE64_STREAM_OMIT_EMPTY(test_conditional_doc, bytes,
                                            "bytes"),
    LONEJSON_FIELD_I64_ARRAY_OMIT_EMPTY(test_conditional_doc, ints, "ints",
                                        LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_U64_ARRAY_OMIT_EMPTY(test_conditional_doc, uints, "uints",
                                        LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_F64_ARRAY_OMIT_EMPTY(test_conditional_doc, nums, "nums",
                                        LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BOOL_ARRAY_OMIT_EMPTY(test_conditional_doc, bools, "bools",
                                         LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_conditional_doc_map, test_conditional_doc,
                    test_conditional_doc_fields);

static const lonejson_field test_two_alloc_string_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(test_two_alloc_string_doc, first, "first"),
    LONEJSON_FIELD_STRING_ALLOC(test_two_alloc_string_doc, second, "second")};
LONEJSON_MAP_DEFINE(test_two_alloc_string_doc_map, test_two_alloc_string_doc,
                    test_two_alloc_string_doc_fields);

static const lonejson_field test_large_fixed_string_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_large_fixed_string_doc, payload, "payload",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_large_fixed_string_doc_map,
                    test_large_fixed_string_doc,
                    test_large_fixed_string_doc_fields);

static const lonejson_field test_alloc_fixed_combo_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(test_alloc_fixed_combo_doc, dyn, "dyn"),
    LONEJSON_FIELD_STRING_FIXED(test_alloc_fixed_combo_doc, payload, "payload",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_alloc_fixed_combo_doc_map, test_alloc_fixed_combo_doc,
                    test_alloc_fixed_combo_doc_fields);

static const lonejson_field test_trunc_array_doc_fields[] = {
    LONEJSON_FIELD_I64_ARRAY(test_trunc_array_doc, ints, "ints",
                             LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_ARRAY(test_trunc_array_doc, tags, "tags",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_OBJECT_ARRAY(test_trunc_array_doc, items, "items", test_item,
                                &test_item_map, LONEJSON_OVERFLOW_TRUNCATE)};
LONEJSON_MAP_DEFINE(test_trunc_array_doc_map, test_trunc_array_doc,
                    test_trunc_array_doc_fields);

static const lonejson_field test_silent_array_doc_fields[] = {
    LONEJSON_FIELD_I64_ARRAY(test_trunc_array_doc, ints, "ints",
                             LONEJSON_OVERFLOW_TRUNCATE_SILENT),
    LONEJSON_FIELD_STRING_ARRAY(test_trunc_array_doc, tags, "tags",
                                LONEJSON_OVERFLOW_TRUNCATE_SILENT),
    LONEJSON_FIELD_OBJECT_ARRAY(test_trunc_array_doc, items, "items", test_item,
                                &test_item_map,
                                LONEJSON_OVERFLOW_TRUNCATE_SILENT)};
LONEJSON_MAP_DEFINE(test_silent_array_doc_map, test_trunc_array_doc,
                    test_silent_array_doc_fields);

static const lonejson_field test_person_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_person, name, "name"),
    LONEJSON_FIELD_STRING_FIXED(test_person, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(test_person, age, "age"),
    LONEJSON_FIELD_F64(test_person, score, "score"),
    LONEJSON_FIELD_BOOL(test_person, active, "active"),
    LONEJSON_FIELD_OBJECT_REQ(test_person, address, "address",
                              &test_address_map),
    LONEJSON_FIELD_I64_ARRAY(test_person, lucky_numbers, "lucky_numbers",
                             LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ARRAY(test_person, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY(test_person, items, "items", test_item,
                                &test_item_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_person_map, test_person, test_person_fields);

static int g_failures = 0;

static void expect_true(int cond, const char *expr, const char *file,
                        int line) {
  if (!cond) {
    fprintf(stderr, "%s:%d: expectation failed: %s\n", file, line, expr);
    g_failures++;
  }
}

#define EXPECT(expr) expect_true((expr), #expr, __FILE__, __LINE__)

static void poison_bytes(void *ptr, size_t len, unsigned char value) {
  unsigned char *bytes = (unsigned char *)ptr;
  size_t i;

  for (i = 0; i < len; ++i) {
    bytes[i] = value;
  }
}

static lonejson_read_result test_state_reader(void *user, unsigned char *buffer,
                                              size_t capacity) {
  test_reader_state *st = (test_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (chunk > remaining) {
    chunk = remaining;
  }
  if (chunk > capacity) {
    chunk = capacity;
  }
  memcpy(buffer, st->json + st->offset, chunk);
  st->offset += chunk;
  rr.bytes_read = chunk;
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

static lonejson_read_result test_empty_then_state_reader(void *user,
                                                         unsigned char *buffer,
                                                         size_t capacity) {
  test_empty_read_state *st = (test_empty_read_state *)user;
  lonejson_read_result rr;

  memset(&rr, 0, sizeof(rr));
  if (st->empty_reads_remaining != 0u) {
    st->empty_reads_remaining--;
    return rr;
  }
  return test_state_reader(&st->inner, buffer, capacity);
}

static lonejson_read_result
test_intermittent_empty_reader(void *user, unsigned char *buffer,
                               size_t capacity) {
  test_intermittent_empty_read_state *st =
      (test_intermittent_empty_read_state *)user;
  lonejson_read_result rr;

  memset(&rr, 0, sizeof(rr));
  if (st->return_empty_next && st->inner.json[st->inner.offset] != '\0') {
    st->return_empty_next = 0;
    st->empty_reads++;
    return rr;
  }
  rr = test_state_reader(&st->inner, buffer, capacity);
  if (rr.bytes_read != 0u) {
    st->return_empty_next = 1;
  }
  return rr;
}

static lonejson_read_result test_would_block_once_reader(void *user,
                                                         unsigned char *buffer,
                                                         size_t capacity) {
  test_would_block_read_state *st = (test_would_block_read_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  if (!st->blocked && st->offset >= st->block_after) {
    st->blocked = 1;
    rr.would_block = 1;
    return rr;
  }
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (!st->blocked && st->offset + chunk > st->block_after) {
    chunk = st->block_after - st->offset;
  }
  if (chunk > remaining) {
    chunk = remaining;
  }
  if (chunk > capacity) {
    chunk = capacity;
  }
  if (chunk == 0u) {
    rr.would_block = 1;
    st->blocked = 1;
    return rr;
  }
  memcpy(buffer, st->json + st->offset, chunk);
  st->offset += chunk;
  rr.bytes_read = chunk;
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

static lonejson_read_result
test_would_block_tripwire_reader(void *user, unsigned char *buffer,
                                 size_t capacity) {
  test_would_block_tripwire_read_state *st =
      (test_would_block_tripwire_read_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  st->call_count++;
  if (st->blocked) {
    st->reentered_after_block = 1;
    rr.error_code = EDEADLK;
    return rr;
  }
  if (st->offset >= st->block_after) {
    st->blocked = 1;
    rr.would_block = 1;
    return rr;
  }
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (st->offset + chunk > st->block_after) {
    chunk = st->block_after - st->offset;
  }
  if (chunk > remaining) {
    chunk = remaining;
  }
  if (chunk > capacity) {
    chunk = capacity;
  }
  if (chunk == 0u) {
    st->blocked = 1;
    rr.would_block = 1;
    return rr;
  }
  memcpy(buffer, st->json + st->offset, chunk);
  st->offset += chunk;
  rr.bytes_read = chunk;
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

static lonejson_read_result
test_framed_reader(void *user, unsigned char *buffer, size_t capacity) {
  test_reader_state *st = (test_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = remaining < st->chunk_size ? remaining : st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  if (chunk > capacity) {
    chunk = capacity;
  }
  if (chunk != 0u) {
    memcpy(buffer, st->json + st->offset, chunk);
    st->offset += chunk;
    rr.bytes_read = chunk;
  }
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

static char *make_repeating_text(size_t len) {
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789-_:";
  char *text;
  size_t i;

  text = (char *)malloc(len + 1u);
  if (text == NULL) {
    return NULL;
  }
  for (i = 0u; i < len; ++i) {
    text[i] = alphabet[i % (sizeof(alphabet) - 1u)];
  }
  text[len] = '\0';
  return text;
}

static char *base64_encode_bytes(const unsigned char *data, size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *encoded;
  size_t out_len;
  size_t in;
  size_t out;

  out_len = ((len + 2u) / 3u) * 4u;
  encoded = (char *)malloc(out_len + 1u);
  if (encoded == NULL) {
    return NULL;
  }
  in = 0u;
  out = 0u;
  while (in < len) {
    unsigned char a = data[in++];
    unsigned char b = 0u;
    unsigned char c = 0u;
    int have_b = 0;
    int have_c = 0;

    if (in < len) {
      b = data[in++];
      have_b = 1;
    }
    if (in < len) {
      c = data[in++];
      have_c = 1;
    }

    encoded[out++] = alphabet[(a >> 2u) & 0x3Fu];
    encoded[out++] = alphabet[((a & 0x03u) << 4u) | (b >> 4u)];
    encoded[out++] = have_b ? alphabet[((b & 0x0Fu) << 2u) | (c >> 6u)] : '=';
    encoded[out++] = have_c ? alphabet[c & 0x3Fu] : '=';
  }
  encoded[out] = '\0';
  return encoded;
}

static char *make_spool_json(const char *id, const char *text,
                             const char *base64_text) {
  const char *fmt = "{\"id\":\"%s\",\"text\":\"%s\",\"bytes\":\"%s\"}";
  size_t need = snprintf(NULL, 0, fmt, id, text, base64_text);
  char *json = (char *)malloc(need + 1u);

  if (json == NULL) {
    return NULL;
  }
  snprintf(json, need + 1u, fmt, id, text, base64_text);
  return json;
}

static size_t read_spooled_all(lonejson_spooled *value, unsigned char *buffer,
                               size_t capacity) {
  size_t total;
  lonejson_error error;

  total = 0u;
  if (lonejson_spooled_rewind(value, &error) != LONEJSON_STATUS_OK) {
    return 0u;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(value, buffer + total, capacity - total);
    if (chunk.error_code != 0) {
      return total;
    }
    total += chunk.bytes_read;
    if (chunk.eof) {
      break;
    }
    if (total >= capacity) {
      break;
    }
  }
  return total;
}

static lonejson_status test_counting_sink_write(void *user, const void *data,
                                                size_t len,
                                                lonejson_error *error) {
  test_counting_sink *sink = (test_counting_sink *)user;
  (void)data;
  (void)error;
  sink->total += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_buffer_sink_write(void *user, const void *data,
                                              size_t len,
                                              lonejson_error *error) {
  test_buffer_sink *sink = (test_buffer_sink *)user;

  if (sink->length + len >= sink->capacity) {
    return lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                               "test sink buffer too small");
  }
  memcpy(sink->buffer + sink->length, data, len);
  sink->length += len;
  sink->buffer[sink->length] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_failing_sink_write(void *user, const void *data,
                                               size_t len,
                                               lonejson_error *error) {
  test_failing_sink *sink = (test_failing_sink *)user;
  (void)data;
  sink->total += len;
  if (sink->total >= sink->fail_after) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "intentional sink failure");
  }
  return LONEJSON_STATUS_OK;
}

static void test_copy_cstr(char *dst, size_t capacity, const char *src) {
  size_t len;

  if (capacity == 0u) {
    return;
  }
  if (src == NULL) {
    dst[0] = '\0';
    return;
  }
  len = strlen(src);
  if (len >= capacity) {
    len = capacity - 1u;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

static lonejson_status test_sse_begin_cb(void *user,
                                         const lonejson_sse_event *event,
                                         lonejson_error *error) {
  test_sse_state *state;

  (void)error;
  state = (test_sse_state *)user;
  ++state->begin_count;
  test_copy_cstr(state->event, sizeof(state->event), event->event);
  test_copy_cstr(state->id, sizeof(state->id), event->id);
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_sse_data_cb(void *user, const void *bytes,
                                        size_t len, lonejson_error *error) {
  test_sse_state *state;
  size_t copy_len;

  state = (test_sse_state *)user;
  if (state->fail) {
    (void)error;
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  ++state->data_count;
  if (state->data_len < sizeof(state->data) - 1u) {
    copy_len = len;
    if (copy_len > sizeof(state->data) - 1u - state->data_len) {
      copy_len = sizeof(state->data) - 1u - state->data_len;
    }
    memcpy(state->data + state->data_len, bytes, copy_len);
    state->data_len += copy_len;
    state->data[state->data_len] = '\0';
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_sse_end_cb(void *user,
                                       const lonejson_sse_event *event,
                                       lonejson_error *error) {
  test_sse_state *state;

  (void)error;
  state = (test_sse_state *)user;
  ++state->count;
  test_copy_cstr(state->event, sizeof(state->event), event->event);
  test_copy_cstr(state->id, sizeof(state->id), event->id);
  state->data_len = event->data_len;
  state->retry_ms = event->retry_ms;
  state->has_retry = event->has_retry;
  return LONEJSON_STATUS_OK;
}

static lonejson_sse_handler test_sse_handler(void) {
  lonejson_sse_handler handler;
  memset(&handler, 0, sizeof(handler));
  handler.begin_event = test_sse_begin_cb;
  handler.data_chunk = test_sse_data_cb;
  handler.end_event = test_sse_end_cb;
  return handler;
}

static lonejson_status test_sse_json_event_cb(void *user,
                                              const lonejson_sse_event *event,
                                              void *dst,
                                              lonejson_error *error) {
  test_sse_json_state *state;
  test_event *record;

  (void)error;
  state = (test_sse_json_state *)user;
  record = (test_event *)dst;
  if (strcmp(record->id, "evtjsn") != 0 || record->ok != true) {
    state->invalid = 1;
  }
  ++state->count;
  test_copy_cstr(state->event, sizeof(state->event), event->event);
  test_copy_cstr(state->id, sizeof(state->id), event->id);
  state->data_len = event->data_len;
  state->retry_ms = event->retry_ms;
  state->has_retry = event->has_retry;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_sse_json_collect_event_cb(void *user, const lonejson_sse_event *event,
                               void *dst, lonejson_error *error) {
  test_sse_json_state *state;

  (void)dst;
  (void)error;
  state = (test_sse_json_state *)user;
  ++state->count;
  test_copy_cstr(state->event, sizeof(state->event), event->event);
  test_copy_cstr(state->id, sizeof(state->id), event->id);
  state->data_len = event->data_len;
  state->retry_ms = event->retry_ms;
  state->has_retry = event->has_retry;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_multipart_begin_cb(void *user, const lonejson_multipart_part *part,
                        lonejson_error *error) {
  test_multipart_state *state;
  size_t index;

  (void)error;
  state = (test_multipart_state *)user;
  if (state->fail_begin) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  index = state->begin_count;
  if (index < 4u) {
    size_t i;

    for (i = 0u; i < part->header_count; ++i) {
    }
    test_copy_cstr(state->names[index], sizeof(state->names[index]),
                   part->name);
    test_copy_cstr(state->content_types[index],
                   sizeof(state->content_types[index]), part->content_type);
    state->lengths[index] = part->content_length;
  }
  ++state->begin_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_multipart_data_cb(void *user, const void *bytes,
                                              size_t len,
                                              lonejson_error *error) {
  test_multipart_state *state;

  state = (test_multipart_state *)user;
  if (state->check_no_body_alloc && state->stats != NULL) {
    if (state->stats->alloc_calls != state->alloc_calls_after_headers ||
        state->stats->realloc_calls != state->realloc_calls_after_headers) {
      state->saw_body_alloc = 1;
    }
  }
  if (state->fail_data) {
    (void)error;
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (state->data_len + len < sizeof(state->data)) {
    memcpy(state->data + state->data_len, bytes, len);
    state->data_len += len;
    state->data[state->data_len] = '\0';
  }
  if (state->data_count <
      sizeof(state->data_chunks) / sizeof(state->data_chunks[0])) {
    state->data_chunks[state->data_count] = len;
  }
  ++state->data_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_multipart_end_cb(void *user, const lonejson_multipart_part *part,
                      lonejson_error *error) {
  test_multipart_state *state;

  (void)part;
  (void)error;
  if (part != NULL) {
    size_t i;

    for (i = 0u; i < part->header_count; ++i) {
    }
  }
  state = (test_multipart_state *)user;
  if (state->fail_end) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  ++state->end_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_append(test_visit_state *state,
                                         const char *text) {
  size_t len = strlen(text);
  if (state->len + len + 1u >= sizeof(state->log)) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(state->log + state->len, text, len);
  state->len += len;
  state->log[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_object_begin(void *user,
                                               lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "{");
}

static lonejson_status test_visit_object_end(void *user,
                                             lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "}");
}

static lonejson_status test_visit_array_begin(void *user,
                                              lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "[");
}

static lonejson_status test_visit_array_end(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "]");
}

static lonejson_status test_visit_key_begin(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "K(");
}

static lonejson_status test_visit_key_chunk(void *user, const char *data,
                                            size_t len, lonejson_error *error) {
  test_visit_state *state = (test_visit_state *)user;
  (void)error;
  if (state->len + len + 1u >= sizeof(state->log)) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(state->log + state->len, data, len);
  state->len += len;
  state->log[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_key_end(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, ")");
}

static lonejson_status test_visit_string_begin(void *user,
                                               lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "S(");
}

static lonejson_status test_visit_string_end(void *user,
                                             lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, ")");
}

static lonejson_status test_visit_number_begin(void *user,
                                               lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "N(");
}

static lonejson_status test_visit_number_end(void *user,
                                             lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, ")");
}

static lonejson_status test_visit_bool(void *user, int value,
                                       lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, value ? "T" : "F");
}

static lonejson_status test_visit_null(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "Z");
}

static lonejson_value_visitor test_value_visitor(void) {
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_visit_object_begin;
  visitor.object_end = test_visit_object_end;
  visitor.object_key_begin = test_visit_key_begin;
  visitor.object_key_chunk = test_visit_key_chunk;
  visitor.object_key_end = test_visit_key_end;
  visitor.array_begin = test_visit_array_begin;
  visitor.array_end = test_visit_array_end;
  visitor.string_begin = test_visit_string_begin;
  visitor.string_chunk = test_visit_key_chunk;
  visitor.string_end = test_visit_string_end;
  visitor.number_begin = test_visit_number_begin;
  visitor.number_chunk = test_visit_key_chunk;
  visitor.number_end = test_visit_number_end;
  visitor.boolean_value = test_visit_bool;
  visitor.null_value = test_visit_null;
  return visitor;
}

static lonejson_status
test_visit_counter_maybe_fail(test_visit_chunk_state *state,
                              lonejson_error *error) {
  ++state->callback_count;
  if (state->fail_after != 0u && state->callback_count >= state->fail_after) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "intentional visitor failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_count_object_begin(void *user,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->object_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_object_end(void *user,
                                                   lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_array_begin(void *user,
                                                    lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->array_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_array_end(void *user,
                                                  lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_key_begin(void *user,
                                                  lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->key_begin_count;
  state->last_key_len = 0u;
  state->last_key[0] = '\0';
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_key_chunk(void *user, const char *data,
                                                  size_t len,
                                                  lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  size_t copy_len = len;

  ++state->key_chunk_count;
  if (state->last_key_len + copy_len >= sizeof(state->last_key)) {
    copy_len = sizeof(state->last_key) - state->last_key_len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->last_key + state->last_key_len, data, copy_len);
    state->last_key_len += copy_len;
    state->last_key[state->last_key_len] = '\0';
  }
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_key_end(void *user,
                                                lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->key_end_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_string_begin(void *user,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->string_begin_count;
  state->last_string_len = 0u;
  state->last_string[0] = '\0';
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_string_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  size_t copy_len = len;

  ++state->string_chunk_count;
  if (state->last_string_len + copy_len >= sizeof(state->last_string)) {
    copy_len = sizeof(state->last_string) - state->last_string_len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->last_string + state->last_string_len, data, copy_len);
    state->last_string_len += copy_len;
    state->last_string[state->last_string_len] = '\0';
  }
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_string_end(void *user,
                                                   lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->string_end_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_number_begin(void *user,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->number_begin_count;
  state->last_number_len = 0u;
  state->last_number[0] = '\0';
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_number_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  size_t copy_len = len;

  ++state->number_chunk_count;
  if (state->last_number_len + copy_len >= sizeof(state->last_number)) {
    copy_len = sizeof(state->last_number) - state->last_number_len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->last_number + state->last_number_len, data, copy_len);
    state->last_number_len += copy_len;
    state->last_number[state->last_number_len] = '\0';
  }
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_number_end(void *user,
                                                   lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->number_end_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_bool(void *user, int value,
                                             lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  (void)value;
  ++state->bool_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_null(void *user,
                                             lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->null_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_value_visitor test_counting_value_visitor(void) {
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_visit_count_object_begin;
  visitor.object_end = test_visit_count_object_end;
  visitor.object_key_begin = test_visit_count_key_begin;
  visitor.object_key_chunk = test_visit_count_key_chunk;
  visitor.object_key_end = test_visit_count_key_end;
  visitor.array_begin = test_visit_count_array_begin;
  visitor.array_end = test_visit_count_array_end;
  visitor.string_begin = test_visit_count_string_begin;
  visitor.string_chunk = test_visit_count_string_chunk;
  visitor.string_end = test_visit_count_string_end;
  visitor.number_begin = test_visit_count_number_begin;
  visitor.number_chunk = test_visit_count_number_chunk;
  visitor.number_end = test_visit_count_number_end;
  visitor.boolean_value = test_visit_count_bool;
  visitor.null_value = test_visit_count_null;
  return visitor;
}

static int write_temp_text_file(char *path_template, const char *text) {
  int fd;

  fd = mkstemp(path_template);
  if (fd < 0) {
    return -1;
  }
  if (write(fd, text, strlen(text)) != (ssize_t)strlen(text)) {
    close(fd);
    unlink(path_template);
    return -1;
  }
  return fd;
}

static char *fixture_path(const char *name) {
  const char *root = LONEJSON_SOURCE_DIR;
  const char *prefix = "/tests/fixtures/";
  size_t len = strlen(root) + strlen(prefix) + strlen(name) + 1u;
  char *path = (char *)malloc(len);

  if (path == NULL) {
    return NULL;
  }
  strcpy(path, root);
  strcat(path, prefix);
  strcat(path, name);
  return path;
}

static char *generated_fixture_path(const char *name) {
  const char *root = LONEJSON_GENERATED_FIXTURE_DIR;
  size_t len = strlen(root) + 1u + strlen(name) + 1u;
  char *path = (char *)malloc(len);

  if (path == NULL) {
    return NULL;
  }
  strcpy(path, root);
  strcat(path, "/");
  strcat(path, name);
  return path;
}

static char *vendor_fixture_path(const char *name) {
  const char *root = LONEJSON_SOURCE_DIR;
  const char *prefix = "/tests/fixtures/vendor/json_test_suite/test_parsing/";
  size_t len = strlen(root) + strlen(prefix) + strlen(name) + 1u;
  char *path = (char *)malloc(len);

  if (path == NULL) {
    return NULL;
  }
  strcpy(path, root);
  strcat(path, prefix);
  strcat(path, name);
  return path;
}

static void test_expect_compact_buffer_matches_sink(const lonejson_map *map,
                                                    const void *src,
                                                    const char *expected) {
  char buffer_json[4096];
  char sink_json[4096];
  test_buffer_sink sink;
  lonejson_error error;
  lonejson_status status;
  size_t needed = 0u;

  status = test_serialize_buffer(map, src, buffer_json, sizeof(buffer_json),
                                 &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(needed == strlen(buffer_json));
  if (expected != NULL) {
    EXPECT(strcmp(buffer_json, expected) == 0);
  }

  memset(&sink, 0, sizeof(sink));
  sink.buffer = (unsigned char *)sink_json;
  sink.capacity = sizeof(sink_json);
  status = test_serialize_sink(map, src, test_buffer_sink_write, &sink, NULL,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer_json, sink_json) == 0);
}
