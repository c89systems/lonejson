#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lonejson.h"

#ifndef LONEJSON_BENCH_TOOLCHAIN
#error "Build benchmarks through CMake with the pinned toolchain"
#endif

#define BENCH_MAX_RESULTS 88u
#define BENCH_MAX_FILES 512u
#define BENCH_PARSE_ITEM_CAPACITY 8u
#define BENCH_JSONL_RECORDS 8u
#include "lonejson_measure.h"
#define BENCH_NOISE_DELTA_PCT 3.0
#define BENCH_MATERIAL_DELTA_PCT 10.0
#define BENCH_REVIEW_IMPROVEMENT_PCT 15.0
#define BENCH_SCHEMA_VERSION 21u

typedef enum bench_doc_kind {
  BENCH_DOC_VALID = 1,
  BENCH_DOC_INVALID = 2,
  BENCH_DOC_IMPL_DEFINED = 3
} bench_doc_kind;

typedef struct bench_doc {
  char *path;
  unsigned char *data;
  size_t len;
  bench_doc_kind kind;
} bench_doc;

typedef struct bench_result {
  char name[64];
  char group[24];
  lonejson_uint64 elapsed_ns;
  lonejson_uint64 total_bytes;
  lonejson_uint64 total_documents;
  lonejson_uint64 mismatch_count;
  double mib_per_sec;
  double docs_per_sec;
  double ns_per_byte;
  lonejson_uint64 batch_iterations;
  lonejson_uint64 target_sample_ns;
  lonejson_uint64 warmup_elapsed_ns;
  double cv_pct;
  double drift_pct;
  double mib_per_sec_low;
  double mib_per_sec_high;
  double relative_error_pct;
  double drift_ci_low_pct;
  double drift_ci_high_pct;
  lonejson_object_array samples;
} bench_result;

typedef struct bench_run {
  lonejson_uint64 schema_version;
  char method[32];
  char affinity[32];
  char source_revision[96];
  char source_src_tree[65];
  char source_include_tree[65];
  char source_lua_tree[65];
  lonejson_uint64 implementation_count;
  lonejson_uint64 sample_count;
  lonejson_uint64 process_count;
  lonejson_uint64 samples_per_process;
  lonejson_uint64 warmup_count;
  lonejson_uint64 target_sample_ns;
  double confidence_pct;
  double max_relative_error_pct;
  double max_drift_pct;
  lonejson_uint64 timestamp_epoch_ns;
  char timestamp_utc[32];
  char host[64];
  char compiler[64];
  char toolchain[128];
  char lua_version[32];
  char corpus_path[256];
  lonejson_uint64 corpus_file_count;
  lonejson_uint64 corpus_total_bytes;
  lonejson_uint64 corpus_y_count;
  lonejson_uint64 corpus_n_count;
  lonejson_uint64 corpus_i_count;
  lonejson_uint64 iterations;
  lonejson_uint64 parser_buffer_size;
  lonejson_uint64 push_parser_buffer_size;
  lonejson_uint64 reader_buffer_size;
  lonejson_uint64 stream_buffer_size;
  lonejson_object_array results;
  bench_result result_storage[BENCH_MAX_RESULTS];
} bench_run;

typedef struct bench_address {
  char city[24];
  lonejson_int64 zip;
} bench_address;

typedef struct bench_item {
  lonejson_int64 id;
  char label[16];
} bench_item;

typedef struct bench_record_fixed {
  char name[24];
  char nickname[16];
  lonejson_int64 age;
  double score;
  bool active;
  bench_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_object_array items;
} bench_record_fixed;

typedef struct bench_record_dynamic {
  char *name;
  char nickname[16];
  lonejson_int64 age;
  double score;
  bool active;
  bench_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_string_array tags;
  lonejson_object_array items;
} bench_record_dynamic;

typedef struct bench_json_value_doc {
  char id[24];
  lonejson_json_value selector;
  lonejson_json_value fields;
  lonejson_json_value last_error;
} bench_json_value_doc;

typedef struct bench_fixed_target {
  bench_record_fixed record;
  lonejson_int64 numbers_storage[BENCH_PARSE_ITEM_CAPACITY];
  bench_item item_storage[BENCH_PARSE_ITEM_CAPACITY];
} bench_fixed_target;

static int bench_validate_loaded_run(const bench_run *run);
static size_t bench_count_result_mismatches(const bench_run *run);
static size_t bench_count_imprecise_results(const bench_run *run);

typedef struct bench_mem_reader {
  const unsigned char *data;
  size_t len;
  size_t offset;
  size_t chunk_size;
} bench_mem_reader;

typedef struct bench_validation_case {
  const bench_doc *docs;
  size_t doc_count;
  int (*validate_fn)(const unsigned char *data, size_t len);
} bench_validation_case;

typedef struct bench_parse_case {
  lonejson *runtime;
  const lonejson_map *map;
  const char *json;
  size_t json_len;
  size_t object_count;
} bench_parse_case;

typedef struct bench_parse_case_short {
  lonejson *runtime;
  const lj_map *map;
  const char *json;
  size_t json_len;
  size_t object_count;
} bench_parse_case_short;

typedef struct bench_stream_case {
  lonejson *runtime;
  const lonejson_map *map;
  const unsigned char *stream_json;
  size_t stream_len;
  size_t object_count;
} bench_stream_case;

typedef struct bench_array_stream_case {
  const char *path;
  const unsigned char *json;
  size_t json_len;
  size_t item_count;
  size_t chunk_size;
} bench_array_stream_case;

typedef struct bench_array_stream_count {
  size_t count;
  size_t bytes;
} bench_array_stream_count;

typedef struct bench_workload_phase_stats {
  lonejson_int64 ops;
  lonejson_int64 errors;
} bench_workload_phase_stats;

typedef struct bench_workload_phases {
  bench_workload_phase_stats acquire;
  bench_workload_phase_stats attach;
  bench_workload_phase_stats delete_phase;
  bench_workload_phase_stats get_phase;
  bench_workload_phase_stats release;
  bench_workload_phase_stats total;
  bench_workload_phase_stats update;
  bench_workload_phase_stats query;
  bench_workload_phase_stats acquire_a;
  bench_workload_phase_stats acquire_b;
  bench_workload_phase_stats decide;
  bench_workload_phase_stats update_a;
  bench_workload_phase_stats update_b;
} bench_workload_phases;

typedef struct bench_workload_case_result {
  char workload[24];
  lonejson_int64 size;
  lonejson_int64 ops;
  lonejson_int64 payload_bytes;
  lonejson_int64 attachment_bytes;
  bench_workload_phases phases;
} bench_workload_case_result;

typedef struct bench_workload_git_info {
  char short_sha[16];
  bool dirty;
} bench_workload_git_info;

typedef struct bench_workload_run_config {
  char ha_mode[16];
  lonejson_int64 concurrency;
  lonejson_int64 runs;
  lonejson_int64 warmup;
  char query_return[16];
  bool qrf_disabled;
} bench_workload_run_config;

typedef struct bench_workload_run_record {
  char run_id[64];
  char timestamp[32];
  char history_branch[32];
  char preset[16];
  char backend[16];
  char store[32];
  char disk_root[128];
  bench_workload_git_info git;
  bool golden;
  bench_workload_run_config config;
  lonejson_object_array cases;
} bench_workload_run_record;

typedef struct bench_workload_target {
  bench_workload_run_record record;
  bench_workload_case_result case_storage[16];
} bench_workload_target;

typedef struct bench_workload_case {
  const lonejson_map *map;
  const unsigned char *jsonl;
  size_t jsonl_len;
  size_t object_count;
} bench_workload_case;

typedef enum bench_vendor_doc_kind {
  BENCH_VENDOR_LONG_STRINGS = 1,
  BENCH_VENDOR_EXTREME_NUMBERS,
  BENCH_VENDOR_STRING_UNICODE,
  BENCH_VENDOR_JAPANESE_UTF8,
  BENCH_VENDOR_HEBREW_UTF8,
  BENCH_VENDOR_ARABIC_UTF8,
  BENCH_VENDOR_JAPANESE_WIDE,
  BENCH_VENDOR_HEBREW_WIDE,
  BENCH_VENDOR_ARABIC_WIDE
} bench_vendor_doc_kind;

typedef struct bench_vendor_long_item {
  char id[48];
} bench_vendor_long_item;

typedef struct bench_vendor_long_doc {
  lonejson_object_array x;
  char id[48];
} bench_vendor_long_doc;

typedef struct bench_vendor_long_target {
  bench_vendor_long_doc doc;
  bench_vendor_long_item items[2];
} bench_vendor_long_target;

typedef struct bench_vendor_numbers_doc {
  double min;
  double max;
} bench_vendor_numbers_doc;

typedef struct bench_vendor_unicode_doc {
  char title[2048];
} bench_vendor_unicode_doc;

typedef struct bench_vendor_language_metrics {
  lonejson_int64 chars;
  lonejson_int64 segments;
} bench_vendor_language_metrics;

typedef struct bench_vendor_language_wide_doc {
  char lang[8];
  char script[24];
  char source[32];
  char title[64];
  char summary[256];
  char body[2048];
  bench_vendor_language_metrics metrics;
} bench_vendor_language_wide_doc;

typedef struct bench_vendor_doc_case {
  bench_vendor_doc_kind kind;
  const lonejson_map *map;
  const unsigned char *json;
  size_t json_len;
} bench_vendor_doc_case;

typedef struct bench_count_sink {
  size_t total;
} bench_count_sink;

typedef struct bench_serialize_case {
  lonejson *runtime;
  const lonejson_map *map;
  const void *src;
  const char *expected_json;
  size_t expected_len;
} bench_serialize_case;

typedef struct bench_jsonl_serialize_case {
  lonejson *runtime;
  const lonejson_map *map;
  const void *items;
  size_t count;
  size_t stride;
  const char *expected_jsonl;
  size_t expected_len;
} bench_jsonl_serialize_case;

typedef struct bench_visit_state {
  size_t object_count;
  size_t array_count;
  size_t key_bytes;
  size_t string_bytes;
  size_t number_bytes;
  size_t bool_count;
  size_t null_count;
} bench_visit_state;

typedef struct bench_visit_case {
  const unsigned char *json;
  size_t json_len;
  size_t reader_chunk_size;
  size_t min_key_bytes;
  size_t min_string_bytes;
  size_t min_number_bytes;
  size_t min_objects;
  size_t min_arrays;
  size_t min_bools;
  size_t min_nulls;
  int use_reader;
} bench_visit_case;

typedef struct bench_writer_value_stream_bench_case {
  const unsigned char *json;
  size_t json_len;
  size_t chunk_size;
} bench_writer_value_stream_bench_case;

typedef int (*bench_validate_fn)(const unsigned char *data, size_t len);
typedef int (*bench_simple_case_fn)(void *ctx);

static volatile lonejson_uint64 g_bench_sink = 0u;
static lonejson *g_bench_runtime_default = NULL;
static lonejson *g_bench_runtime_reuse = NULL;
static lonejson *g_bench_runtime_pretty = NULL;

static int bench_init_runtimes(void) {
  lonejson_config config;
  lonejson_error error;

  config = lonejson_default_config();
  g_bench_runtime_default = lonejson_new(&config, &error);
  if (g_bench_runtime_default == NULL) {
    return 1;
  }

  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  g_bench_runtime_reuse = lonejson_new(&config, &error);
  if (g_bench_runtime_reuse == NULL) {
    lonejson_free(g_bench_runtime_default);
    g_bench_runtime_default = NULL;
    return 1;
  }

  config = lonejson_default_config();
  config.write_pretty = 1;
  g_bench_runtime_pretty = lonejson_new(&config, &error);
  if (g_bench_runtime_pretty == NULL) {
    lonejson_free(g_bench_runtime_reuse);
    lonejson_free(g_bench_runtime_default);
    g_bench_runtime_reuse = NULL;
    g_bench_runtime_default = NULL;
    return 1;
  }
  return 0;
}

static void bench_cleanup_runtimes(void) {
  lonejson_free(g_bench_runtime_pretty);
  lonejson_free(g_bench_runtime_reuse);
  lonejson_free(g_bench_runtime_default);
  g_bench_runtime_pretty = NULL;
  g_bench_runtime_reuse = NULL;
  g_bench_runtime_default = NULL;
}

static void bench_init_fixed_target(bench_fixed_target *target);
static int bench_check_parse_fixed_target(const bench_fixed_target *target);
static int bench_check_stream_fixed_target(const bench_fixed_target *target);
static int bench_check_dynamic_record(const bench_record_dynamic *record);
static int bench_prepare_benchmark_cases(
    bench_parse_case *parse_fixed_case,
    bench_parse_case_short *parse_fixed_short_case,
    bench_parse_case *parse_dynamic_case, bench_stream_case *stream_case,
    bench_serialize_case *serialize_case,
    bench_serialize_case *serialize_pretty_case,
    bench_parse_case *parse_json_value_case,
    bench_serialize_case *json_value_serialize_case,
    bench_serialize_case *json_value_serialize_pretty_case,
    bench_serialize_case *json_value_source_serialize_case,
    bench_serialize_case *json_value_source_serialize_pretty_case,
    bench_jsonl_serialize_case *jsonl_case,
    bench_jsonl_serialize_case *jsonl_pretty_case,
    bench_record_fixed *serialize_record, bench_record_fixed *jsonl_records,
    bench_json_value_doc *json_value_record,
    bench_json_value_doc *json_value_source_record, char *expected_json,
    size_t expected_json_capacity, char *expected_pretty_json,
    size_t expected_pretty_json_capacity, char *jsonl_buffer,
    size_t jsonl_buffer_capacity, char *pretty_jsonl_buffer,
    size_t pretty_jsonl_buffer_capacity, char *stream_buffer,
    size_t stream_buffer_capacity, char *json_value_path,
    size_t json_value_path_capacity);

static const char bench_parse_json[] =
    "{\"name\":\"Alice\",\"nickname\":\"Wonderland\",\"age\":34,"
    "\"score\":99.5,\"active\":true,"
    "\"address\":{\"city\":\"Stockholm\",\"zip\":12345},"
    "\"lucky_numbers\":[7,11,42],"
    "\"tags\":[\"admin\",\"ops\"],"
    "\"items\":[{\"id\":1,\"label\":\"alpha\"},{\"id\":2,"
    "\"label\":\"bravo-longer-than-fit\"}]}";

static const char bench_array_stream_json[] =
    "{\"meta\":{\"skip\":true,\"source\":\"bench\"},"
    "\"tags\":[\"admin\",\"ops\"],"
    "\"items\":[{\"id\":1,\"label\":\"alpha\"},{\"id\":2,"
    "\"label\":\"bravo\"}],\"tail\":3}";

static const char bench_json_value_parse_json[] =
    "{\"id\":\"q-42\","
    "\"selector\":{\"op\":\"and\",\"clauses\":[{\"field\":\"status\","
    "\"value\":\"open\"},{\"field\":\"priority\",\"gte\":3}]},"
    "\"fields\":[\"id\",\"title\",{\"metrics\":[\"size\",\"latency\",true,"
    "null,3.14]}],"
    "\"last_error\":{\"code\":\"E_IO\",\"retry\":false,"
    "\"details\":{\"attempt\":2,\"backoff_ms\":250}}}";

static const char bench_json_value_selector_json[] =
    "{\"op\":\"and\",\"clauses\":[{\"field\":\"status\",\"value\":\"open\"},"
    "{\"field\":\"priority\",\"gte\":3}]}";

static const char bench_json_value_fields_json[] =
    "[\"id\",\"title\",{\"metrics\":[\"size\",\"latency\",true,null,3.14]}]";

static const char bench_json_value_last_error_json[] =
    "{\"code\":\"E_IO\",\"retry\":false,"
    "\"details\":{\"attempt\":2,\"backoff_ms\":250}}";

static const char bench_visit_mixed_value_json[] =
    "{\"selector\":{\"op\":\"and\",\"clauses\":[{\"field\":\"status\","
    "\"value\":\"open\"},{\"field\":\"priority\",\"gte\":3}]},"
    "\"fields\":[\"id\",\"title\",{\"metrics\":[\"size\",\"latency\",true,"
    "null,3.14,-12345.678e-9]}],\"ok\":true,\"missing\":null}";

static const char bench_vendor_long_strings_json[] =
    "{\"x\":[{\"id\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}], "
    "\"id\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}";

static const char bench_vendor_extreme_numbers_json[] =
    "{ \"min\": -1.0e+28, \"max\": 1.0e+28 }";

static const char bench_vendor_string_unicode_json[] =
    "{\"title\":\"\\u041f\\u043e\\u043b\\u0442\\u043e\\u0440\\u0430 "
    "\\u0417\\u0435\\u043c\\u043b\\u0435\\u043a\\u043e\\u043f\\u0430\" }";

static const char bench_vendor_japanese_prefix[] =
    "行く川のながれは絶えずして、しかも本の水にあらず。";
static const char bench_vendor_hebrew_prefix[] = "השועל והחסידה הוא משל.";
static const char bench_vendor_arabic_prefix[] =
    "قدمها بهنود بن سحوان ويعرف بعلي بن الشاه الفارسي.";
static const char bench_vendor_japanese_wide_source[] = "hojoki";
static const char bench_vendor_hebrew_wide_source[] = "aesop_wikisource";
static const char bench_vendor_arabic_wide_source[] = "kalila_dimna";

static const char *const bench_workload_expected_run_ids[5] = {
    "20260322T130525Z-b62770c-dirty-fast-disk",
    "20260322T141810Z-b62770c-dirty-fast-disk",
    "20260323T003016Z-08a87c0-clean-fast-disk",
    "20260325T004115Z-55f841b-dirty-fast-disk",
    "20260325T155329Z-e66dc79-dirty-fast-disk"};

static const char *const bench_workload_expected_short_shas[5] = {
    "b62770c", "b62770c", "08a87c0", "55f841b", "e66dc79"};

static const int bench_workload_expected_dirty[5] = {1, 1, 0, 1, 1};

static const lonejson_field bench_address_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_address, city, "city",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(bench_address, zip, "zip")};
LONEJSON_MAP_DEFINE(bench_address_map, bench_address, bench_address_fields);

static const lonejson_field bench_item_fields[] = {
    LONEJSON_FIELD_I64_REQ(bench_item, id, "id"),
    LONEJSON_FIELD_STRING_FIXED(bench_item, label, "label",
                                LONEJSON_OVERFLOW_TRUNCATE)};
LONEJSON_MAP_DEFINE(bench_item_map, bench_item, bench_item_fields);

static const lonejson_field bench_record_fixed_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_record_fixed, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(bench_record_fixed, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(bench_record_fixed, age, "age"),
    LONEJSON_FIELD_F64(bench_record_fixed, score, "score"),
    LONEJSON_FIELD_BOOL(bench_record_fixed, active, "active"),
    LONEJSON_FIELD_OBJECT_REQ(bench_record_fixed, address, "address",
                              &bench_address_map),
    LONEJSON_FIELD_I64_ARRAY(bench_record_fixed, lucky_numbers, "lucky_numbers",
                             LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY(bench_record_fixed, items, "items", bench_item,
                                &bench_item_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_record_fixed_map, bench_record_fixed,
                    bench_record_fixed_fields);

static const lonejson_field bench_record_dynamic_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(bench_record_dynamic, name, "name"),
    LONEJSON_FIELD_STRING_FIXED(bench_record_dynamic, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(bench_record_dynamic, age, "age"),
    LONEJSON_FIELD_F64(bench_record_dynamic, score, "score"),
    LONEJSON_FIELD_BOOL(bench_record_dynamic, active, "active"),
    LONEJSON_FIELD_OBJECT_REQ(bench_record_dynamic, address, "address",
                              &bench_address_map),
    LONEJSON_FIELD_I64_ARRAY(bench_record_dynamic, lucky_numbers,
                             "lucky_numbers", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ARRAY(bench_record_dynamic, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY(bench_record_dynamic, items, "items",
                                bench_item, &bench_item_map,
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_record_dynamic_map, bench_record_dynamic,
                    bench_record_dynamic_fields);

static const lonejson_field bench_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_json_value_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(bench_json_value_doc, selector, "selector"),
    LONEJSON_FIELD_JSON_VALUE_REQ(bench_json_value_doc, fields, "fields"),
    LONEJSON_FIELD_JSON_VALUE(bench_json_value_doc, last_error, "last_error")};
LONEJSON_MAP_DEFINE(bench_json_value_doc_map, bench_json_value_doc,
                    bench_json_value_doc_fields);

static const lonejson_field bench_vendor_long_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_long_item, id, "id",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_vendor_long_item_map, bench_vendor_long_item,
                    bench_vendor_long_item_fields);

static const lonejson_field bench_vendor_long_doc_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(
        bench_vendor_long_doc, x, "x", bench_vendor_long_item,
        &bench_vendor_long_item_map, LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_long_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_vendor_long_doc_map, bench_vendor_long_doc,
                    bench_vendor_long_doc_fields);

static const lonejson_field bench_vendor_numbers_fields[] = {
    LONEJSON_FIELD_F64_REQ(bench_vendor_numbers_doc, min, "min"),
    LONEJSON_FIELD_F64_REQ(bench_vendor_numbers_doc, max, "max")};
LONEJSON_MAP_DEFINE(bench_vendor_numbers_map, bench_vendor_numbers_doc,
                    bench_vendor_numbers_fields);

static const lonejson_field bench_vendor_unicode_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_unicode_doc, title, "title",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_vendor_unicode_map, bench_vendor_unicode_doc,
                    bench_vendor_unicode_fields);

static const lonejson_field bench_vendor_language_metrics_fields[] = {
    LONEJSON_FIELD_I64_REQ(bench_vendor_language_metrics, chars, "chars"),
    LONEJSON_FIELD_I64_REQ(bench_vendor_language_metrics, segments,
                           "segments")};
LONEJSON_MAP_DEFINE(bench_vendor_language_metrics_map,
                    bench_vendor_language_metrics,
                    bench_vendor_language_metrics_fields);

static const lonejson_field bench_vendor_language_wide_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_language_wide_doc, lang,
                                    "lang", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_language_wide_doc, script,
                                    "script", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_language_wide_doc, source,
                                    "source", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_language_wide_doc, title,
                                    "title", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_language_wide_doc, summary,
                                    "summary", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_vendor_language_wide_doc, body,
                                    "body", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_REQ(bench_vendor_language_wide_doc, metrics,
                              "metrics", &bench_vendor_language_metrics_map)};
LONEJSON_MAP_DEFINE(bench_vendor_language_wide_map,
                    bench_vendor_language_wide_doc,
                    bench_vendor_language_wide_fields);

static const lonejson_field bench_workload_phase_stats_fields[] = {
    LONEJSON_FIELD_I64(bench_workload_phase_stats, ops, "ops"),
    LONEJSON_FIELD_I64(bench_workload_phase_stats, errors, "errors")};
LONEJSON_MAP_DEFINE(bench_workload_phase_stats_map, bench_workload_phase_stats,
                    bench_workload_phase_stats_fields);

static const lonejson_field bench_workload_phases_fields[] = {
    LONEJSON_FIELD_OBJECT(bench_workload_phases, acquire, "acquire",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, attach, "attach",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, delete_phase, "delete",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, get_phase, "get",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, release, "release",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, total, "total",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, update, "update",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, query, "query",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, acquire_a, "acquire-a",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, acquire_b, "acquire-b",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, decide, "decide",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, update_a, "update-a",
                          &bench_workload_phase_stats_map),
    LONEJSON_FIELD_OBJECT(bench_workload_phases, update_b, "update-b",
                          &bench_workload_phase_stats_map)};
LONEJSON_MAP_DEFINE(bench_workload_phases_map, bench_workload_phases,
                    bench_workload_phases_fields);

static const lonejson_field bench_workload_case_result_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_case_result, workload,
                                    "workload", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64(bench_workload_case_result, size, "size"),
    LONEJSON_FIELD_I64_REQ(bench_workload_case_result, ops, "ops"),
    LONEJSON_FIELD_I64(bench_workload_case_result, payload_bytes,
                       "payload_bytes"),
    LONEJSON_FIELD_I64(bench_workload_case_result, attachment_bytes,
                       "attachment_bytes"),
    LONEJSON_FIELD_OBJECT_REQ(bench_workload_case_result, phases, "phases",
                              &bench_workload_phases_map)};
LONEJSON_MAP_DEFINE(bench_workload_case_result_map, bench_workload_case_result,
                    bench_workload_case_result_fields);

static const lonejson_field bench_workload_git_info_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_git_info, short_sha,
                                    "short_sha", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BOOL_REQ(bench_workload_git_info, dirty, "dirty")};
LONEJSON_MAP_DEFINE(bench_workload_git_info_map, bench_workload_git_info,
                    bench_workload_git_info_fields);

static const lonejson_field bench_workload_run_config_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_run_config, ha_mode,
                                    "ha_mode", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(bench_workload_run_config, concurrency,
                           "concurrency"),
    LONEJSON_FIELD_I64_REQ(bench_workload_run_config, runs, "runs"),
    LONEJSON_FIELD_I64_REQ(bench_workload_run_config, warmup, "warmup"),
    LONEJSON_FIELD_STRING_FIXED(bench_workload_run_config, query_return,
                                "query_return", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BOOL(bench_workload_run_config, qrf_disabled,
                        "qrf_disabled")};
LONEJSON_MAP_DEFINE(bench_workload_run_config_map, bench_workload_run_config,
                    bench_workload_run_config_fields);

static const lonejson_field bench_workload_run_record_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_run_record, run_id, "run_id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_run_record, timestamp,
                                    "timestamp", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(bench_workload_run_record, history_branch,
                                "history_branch", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_run_record, preset, "preset",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_workload_run_record, backend,
                                    "backend", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(bench_workload_run_record, store, "store",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(bench_workload_run_record, disk_root,
                                "disk_root", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_REQ(bench_workload_run_record, git, "git",
                              &bench_workload_git_info_map),
    LONEJSON_FIELD_BOOL(bench_workload_run_record, golden, "golden"),
    LONEJSON_FIELD_OBJECT_REQ(bench_workload_run_record, config, "config",
                              &bench_workload_run_config_map),
    LONEJSON_FIELD_OBJECT_ARRAY(
        bench_workload_run_record, cases, "cases", bench_workload_case_result,
        &bench_workload_case_result_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_workload_run_record_map, bench_workload_run_record,
                    bench_workload_run_record_fields);

static const lonejson_field bench_sample_fields[] = {
    LONEJSON_FIELD_U64_REQ(bench_sample, elapsed_ns, "elapsed_ns"),
    LONEJSON_FIELD_U64_REQ(bench_sample, cpu_ns, "cpu_ns"),
    LONEJSON_FIELD_U64_REQ(bench_sample, iterations, "iterations"),
    LONEJSON_FIELD_U64_REQ(bench_sample, process_index, "process_index"),
    LONEJSON_FIELD_U64_REQ(bench_sample, mismatch_count, "mismatch_count")};
LONEJSON_MAP_DEFINE(bench_sample_map, bench_sample, bench_sample_fields);

static const lonejson_field bench_result_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_result, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_result, group, "group",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_U64_REQ(bench_result, elapsed_ns, "elapsed_ns"),
    LONEJSON_FIELD_U64_REQ(bench_result, total_bytes, "total_bytes"),
    LONEJSON_FIELD_U64_REQ(bench_result, total_documents, "total_documents"),
    LONEJSON_FIELD_U64_REQ(bench_result, mismatch_count, "mismatch_count"),
    LONEJSON_FIELD_F64(bench_result, mib_per_sec, "mib_per_sec"),
    LONEJSON_FIELD_F64(bench_result, docs_per_sec, "docs_per_sec"),
    LONEJSON_FIELD_F64(bench_result, ns_per_byte, "ns_per_byte"),
    LONEJSON_FIELD_U64_REQ(bench_result, target_sample_ns, "target_sample_ns"),
    LONEJSON_FIELD_U64_REQ(bench_result, batch_iterations, "batch_iterations"),
    LONEJSON_FIELD_U64_REQ(bench_result, warmup_elapsed_ns,
                           "warmup_elapsed_ns"),
    LONEJSON_FIELD_F64(bench_result, cv_pct, "cv_pct"),
    LONEJSON_FIELD_F64(bench_result, drift_pct, "drift_pct"),
    LONEJSON_FIELD_F64_REQ(bench_result, mib_per_sec_low, "mib_per_sec_low"),
    LONEJSON_FIELD_F64_REQ(bench_result, mib_per_sec_high, "mib_per_sec_high"),
    LONEJSON_FIELD_F64_REQ(bench_result, relative_error_pct,
                           "relative_error_pct"),
    LONEJSON_FIELD_F64_REQ(bench_result, drift_ci_low_pct, "drift_ci_low_pct"),
    LONEJSON_FIELD_F64_REQ(bench_result, drift_ci_high_pct,
                           "drift_ci_high_pct"),
    LONEJSON_FIELD_OBJECT_ARRAY(bench_result, samples, "samples", bench_sample,
                                &bench_sample_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_result_map, bench_result, bench_result_fields);

static const lonejson_field bench_run_fields[] = {
    LONEJSON_FIELD_U64_REQ(bench_run, schema_version, "schema_version"),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, method, "method",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, affinity, "affinity",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, source_revision,
                                    "source_revision", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, source_src_tree,
                                    "source_src_tree", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, source_include_tree,
                                    "source_include_tree",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, source_lua_tree,
                                    "source_lua_tree", LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_U64_REQ(bench_run, implementation_count,
                           "implementation_count"),
    LONEJSON_FIELD_U64_REQ(bench_run, sample_count, "sample_count"),
    LONEJSON_FIELD_U64_REQ(bench_run, process_count, "process_count"),
    LONEJSON_FIELD_U64_REQ(bench_run, samples_per_process,
                           "samples_per_process"),
    LONEJSON_FIELD_U64_REQ(bench_run, warmup_count, "warmup_count"),
    LONEJSON_FIELD_U64_REQ(bench_run, target_sample_ns, "target_sample_ns"),
    LONEJSON_FIELD_F64_REQ(bench_run, confidence_pct, "confidence_pct"),
    LONEJSON_FIELD_F64_REQ(bench_run, max_relative_error_pct,
                           "max_relative_error_pct"),
    LONEJSON_FIELD_F64_REQ(bench_run, max_drift_pct, "max_drift_pct"),
    LONEJSON_FIELD_U64_REQ(bench_run, timestamp_epoch_ns, "timestamp_epoch_ns"),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, timestamp_utc, "timestamp_utc",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, host, "host",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, compiler, "compiler",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(bench_run, toolchain, "toolchain",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(bench_run, lua_version, "lua_version",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED(bench_run, corpus_path, "corpus_path",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_U64(bench_run, corpus_file_count, "corpus_file_count"),
    LONEJSON_FIELD_U64(bench_run, corpus_total_bytes, "corpus_total_bytes"),
    LONEJSON_FIELD_U64(bench_run, corpus_y_count, "corpus_y_count"),
    LONEJSON_FIELD_U64(bench_run, corpus_n_count, "corpus_n_count"),
    LONEJSON_FIELD_U64(bench_run, corpus_i_count, "corpus_i_count"),
    LONEJSON_FIELD_U64_REQ(bench_run, iterations, "iterations"),
    LONEJSON_FIELD_U64(bench_run, parser_buffer_size, "parser_buffer_size"),
    LONEJSON_FIELD_U64(bench_run, push_parser_buffer_size,
                       "push_parser_buffer_size"),
    LONEJSON_FIELD_U64(bench_run, reader_buffer_size, "reader_buffer_size"),
    LONEJSON_FIELD_U64(bench_run, stream_buffer_size, "stream_buffer_size"),
    LONEJSON_FIELD_OBJECT_ARRAY(bench_run, results, "results", bench_result,
                                &bench_result_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(bench_run_map, bench_run, bench_run_fields);

static void bench_run_prepare(bench_run *run) {
  memset(run, 0, sizeof(*run));
  run->results.items = run->result_storage;
  run->results.capacity = BENCH_MAX_RESULTS;
  run->results.elem_size = sizeof(run->result_storage[0]);
  run->results.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
}

/* Measured samples use caller-owned fixed storage. Parsed reports use the
 * library's own allocations; never mark plain malloc storage as library-owned.
 */
static void bench_result_cleanup(bench_result *result) {
  void *storage;
  storage = (result->samples.flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u
                ? result->samples.items
                : NULL;
  lonejson_cleanup(&bench_result_map, result, NULL);
  memset(result, 0, sizeof(*result));
  free(storage);
}

static void bench_run_cleanup(bench_run *run) {
  size_t i;
  for (i = 0u; i < run->results.count; ++i)
    bench_result_cleanup(&run->result_storage[i]);
  lonejson_cleanup(&bench_run_map, run, NULL);
}

static bench_run *g_bench_plan;
static const char *g_bench_filter;
static const bench_result *bench_find_result(const bench_run *, const char *);
static int bench_read_run(const char *, bench_run *);
static const char *g_bench_corpus =
    "tests/fixtures/vendor/json_test_suite/test_parsing";

static void bench_fill_measurement_metadata(bench_run *run) {
  const char *affinity;
  const char *revision;
  const char *value;
  affinity = getenv("LONEJSON_BENCH_AFFINITY");
  revision = getenv("LONEJSON_BENCH_SOURCE_REVISION");
  if (revision == NULL || revision[0] == '\0' ||
      strlen(revision) >= sizeof(run->source_revision)) {
    fputs(
        "set LONEJSON_BENCH_SOURCE_REVISION or use scripts/run_benchmark.sh\n",
        stderr);
    exit(1);
  }

  snprintf(run->method, sizeof(run->method), "%s", BENCH_MEASUREMENT_METHOD);
  snprintf(run->affinity, sizeof(run->affinity), "%s",
           affinity == NULL ? "unbound" : affinity);
  snprintf(run->source_revision, sizeof(run->source_revision), "%s",
           revision == NULL ? "unknown" : revision);
  value = getenv("LONEJSON_BENCH_SRC_TREE");
  snprintf(run->source_src_tree, sizeof(run->source_src_tree), "%s",
           value == NULL ? "unversioned" : value);
  value = getenv("LONEJSON_BENCH_INCLUDE_TREE");
  snprintf(run->source_include_tree, sizeof(run->source_include_tree), "%s",
           value == NULL ? "unversioned" : value);
  value = getenv("LONEJSON_BENCH_LUA_TREE");
  snprintf(run->source_lua_tree, sizeof(run->source_lua_tree), "%s",
           value == NULL ? "unversioned" : value);
  value = getenv("LONEJSON_BENCH_IMPLEMENTATION_COUNT");
  if (value == NULL || strcmp(value, "1") == 0)
    run->implementation_count = 1u;
  else if (strcmp(value, "2") == 0)
    run->implementation_count = 2u;
  else {
    fputs("invalid benchmark implementation count\n", stderr);
    exit(1);
  }
  run->sample_count = BENCH_SAMPLE_COUNT;
  run->process_count = BENCH_PROCESS_COUNT;
  run->samples_per_process = BENCH_PROCESS_SAMPLE_COUNT;
  run->warmup_count = BENCH_WARMUP_COUNT;
  run->target_sample_ns = BENCH_TARGET_SAMPLE_NS;
  run->confidence_pct = BENCH_CONFIDENCE_PCT;
  run->max_relative_error_pct = BENCH_MAX_RELATIVE_ERROR_PCT;
  run->max_drift_pct = BENCH_MAX_DRIFT_PCT;
}

static void bench_u64_to_string(lonejson_uint64 value, char *buffer,
                                size_t capacity) {
  char tmp[32];
  size_t idx;
  size_t out_len;

  if (capacity == 0u) {
    return;
  }
  idx = sizeof(tmp);
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    --idx;
    tmp[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  out_len = sizeof(tmp) - idx;
  if (out_len >= capacity) {
    out_len = capacity - 1u;
  }
  memcpy(buffer, tmp + idx, out_len);
  buffer[out_len] = '\0';
}

static void bench_free_docs(bench_doc *docs, size_t count) {
  size_t i;

  if (docs == NULL) {
    return;
  }
  for (i = 0; i < count; ++i) {
    free(docs[i].path);
    free(docs[i].data);
  }
  free(docs);
}

static int bench_is_json_filename(const char *name) {
  size_t len = strlen(name);
  return len >= 5u && strcmp(name + len - 5u, ".json") == 0;
}

static bench_doc_kind bench_doc_kind_from_name(const char *name) {
  if (strncmp(name, "y_", 2u) == 0) {
    return BENCH_DOC_VALID;
  }
  if (strncmp(name, "n_", 2u) == 0) {
    return BENCH_DOC_INVALID;
  }
  return BENCH_DOC_IMPL_DEFINED;
}

static int bench_compare_doc_path(const void *lhs, const void *rhs) {
  const bench_doc *a = (const bench_doc *)lhs;
  const bench_doc *b = (const bench_doc *)rhs;
  return strcmp(a->path, b->path);
}

static int bench_read_file(const char *path, unsigned char **out_data,
                           size_t *out_len) {
  FILE *fp;
  long size_long;
  size_t size;
  unsigned char *data;

  *out_data = NULL;
  *out_len = 0u;
  fp = fopen(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
    return 1;
  }
  if (fseek(fp, 0L, SEEK_END) != 0) {
    fclose(fp);
    return 1;
  }
  size_long = ftell(fp);
  if (size_long < 0) {
    fclose(fp);
    return 1;
  }
  if (fseek(fp, 0L, SEEK_SET) != 0) {
    fclose(fp);
    return 1;
  }
  size = (size_t)size_long;
  data =
      size == 0u ? (unsigned char *)malloc(1u) : (unsigned char *)malloc(size);
  if (data == NULL) {
    fclose(fp);
    return 1;
  }
  if (size != 0u && fread(data, 1u, size, fp) != size) {
    free(data);
    fclose(fp);
    return 1;
  }
  fclose(fp);
  *out_data = data;
  *out_len = size;
  return 0;
}

static int bench_read_text_file(const char *path, char *buffer,
                                size_t capacity) {
  unsigned char *data;
  size_t len;

  if (bench_read_file(path, &data, &len) != 0) {
    return 1;
  }
  if (len + 1u > capacity) {
    free(data);
    fprintf(stderr, "fixture %s does not fit in buffer\n", path);
    return 1;
  }
  if (len != 0u) {
    memcpy(buffer, data, len);
  }
  buffer[len] = '\0';
  free(data);
  return 0;
}

static int bench_load_corpus(const char *dir_path, bench_doc **out_docs,
                             size_t *out_count,
                             lonejson_uint64 *out_total_bytes,
                             lonejson_uint64 *out_y_count,
                             lonejson_uint64 *out_n_count,
                             lonejson_uint64 *out_i_count) {
  DIR *dir;
  struct dirent *entry;
  bench_doc *docs;
  size_t count;
  lonejson_uint64 total_bytes;
  lonejson_uint64 y_count;
  lonejson_uint64 n_count;
  lonejson_uint64 i_count;

  *out_docs = NULL;
  *out_count = 0u;
  count = 0u;
  total_bytes = 0u;
  y_count = 0u;
  n_count = 0u;
  i_count = 0u;

  docs = (bench_doc *)calloc(BENCH_MAX_FILES, sizeof(*docs));
  if (docs == NULL) {
    return 1;
  }

  dir = opendir(dir_path);
  if (dir == NULL) {
    free(docs);
    return 1;
  }

  while ((entry = readdir(dir)) != NULL) {
    char path[PATH_MAX];

    if (entry->d_name[0] == '.' || !bench_is_json_filename(entry->d_name)) {
      continue;
    }
    if (count == BENCH_MAX_FILES) {
      closedir(dir);
      bench_free_docs(docs, count);
      return 1;
    }
    snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
    docs[count].path = strdup(path);
    if (docs[count].path == NULL ||
        bench_read_file(path, &docs[count].data, &docs[count].len) != 0) {
      closedir(dir);
      bench_free_docs(docs, count + 1u);
      return 1;
    }
    docs[count].kind = bench_doc_kind_from_name(entry->d_name);
    total_bytes += (lonejson_uint64)docs[count].len;
    if (docs[count].kind == BENCH_DOC_VALID) {
      ++y_count;
    } else if (docs[count].kind == BENCH_DOC_INVALID) {
      ++n_count;
    } else {
      ++i_count;
    }
    ++count;
  }

  closedir(dir);
  if (count == 0u || total_bytes == 0u) {
    fprintf(stderr, "benchmark corpus contains no input bytes: %s\n", dir_path);
    bench_free_docs(docs, count);
    return 1;
  }
  qsort(docs, count, sizeof(*docs), bench_compare_doc_path);
  *out_docs = docs;
  *out_count = count;
  *out_total_bytes = total_bytes;
  *out_y_count = y_count;
  *out_n_count = n_count;
  *out_i_count = i_count;
  return 0;
}

static lonejson_uint64 bench_now_ns(clockid_t clock_id) {
  struct timespec ts;
  if (clock_gettime(clock_id, &ts) != 0) {
    return 0u;
  }
  return (lonejson_uint64)ts.tv_sec * (lonejson_uint64)1000000000u +
         (lonejson_uint64)ts.tv_nsec;
}

static void bench_fill_timestamp(bench_run *run) {
  struct timespec ts;
  time_t now;
  struct tm tmv;

  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    memset(run->timestamp_utc, 0, sizeof(run->timestamp_utc));
    return;
  }
  run->timestamp_epoch_ns =
      (lonejson_uint64)ts.tv_sec * (lonejson_uint64)1000000000u +
      (lonejson_uint64)ts.tv_nsec;
  now = (time_t)ts.tv_sec;
  if (gmtime_r(&now, &tmv) != NULL) {
    strftime(run->timestamp_utc, sizeof(run->timestamp_utc),
             "%Y-%m-%dT%H:%M:%SZ", &tmv);
  }
}

static void bench_fill_host_and_compiler(bench_run *run) {
  const char *host_id;

  snprintf(run->toolchain, sizeof(run->toolchain), "%s",
           LONEJSON_BENCH_TOOLCHAIN);
  host_id = getenv("LONEJSON_BENCH_HOST_ID");
  if (host_id != NULL && host_id[0] != '\0') {
    snprintf(run->host, sizeof(run->host), "%s", host_id);
  } else if (gethostname(run->host, sizeof(run->host) - 1u) != 0) {
    snprintf(run->host, sizeof(run->host), "unknown");
  }
  run->host[sizeof(run->host) - 1u] = '\0';
  snprintf(run->compiler, sizeof(run->compiler), "%s", LONEJSON_BENCH_COMPILER);
}

static void bench_store_portable_path(char *dst, size_t dst_size,
                                      const char *path) {
  char cwd[PATH_MAX];
  size_t cwd_len;

  if (dst == NULL || dst_size == 0u) {
    return;
  }
  dst[0] = '\0';
  if (path == NULL || path[0] == '\0') {
    return;
  }
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    cwd_len = strlen(cwd);
    if (strncmp(path, cwd, cwd_len) == 0) {
      const char *relative = path + cwd_len;
      if (*relative == '/') {
        ++relative;
      }
      snprintf(dst, dst_size, "%s", relative);
      return;
    }
  }
  snprintf(dst, dst_size, "%s", path);
}

static int bench_lonejson_validate(const unsigned char *data, size_t len) {
  lonejson_error error;
  return lonejson_validate_buffer(g_bench_runtime_default, data, len, &error) ==
         LONEJSON_STATUS_OK;
}

static int bench_validation_once(const bench_validation_case *ctx,
                                 lonejson_uint64 *out_mismatches) {
  size_t i;
  lonejson_uint64 mismatches;

  mismatches = 0u;
  for (i = 0; i < ctx->doc_count; ++i) {
    int ok;
    int expect_ok;

    ok = ctx->validate_fn(ctx->docs[i].data, ctx->docs[i].len);
    expect_ok = ctx->docs[i].kind == BENCH_DOC_VALID ? 1 : 0;
    if (ctx->docs[i].kind == BENCH_DOC_IMPL_DEFINED) {
      continue;
    }
    if ((ok != 0) != (expect_ok != 0)) {
      ++mismatches;
    }
  }
  *out_mismatches = mismatches;
  return mismatches == 0u;
}

static lonejson_read_result
bench_mem_reader_fn(void *user, unsigned char *buffer, size_t capacity) {
  bench_mem_reader *reader;
  lonejson_read_result rr;
  size_t remaining;
  size_t chunk;

  reader = (bench_mem_reader *)user;
  memset(&rr, 0, sizeof(rr));
  remaining = reader->len - reader->offset;
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  chunk = reader->chunk_size;
  if (chunk > remaining) {
    chunk = remaining;
  }
  if (chunk > capacity) {
    chunk = capacity;
  }
  memcpy(buffer, reader->data + reader->offset, chunk);
  reader->offset += chunk;
  rr.bytes_read = chunk;
  rr.eof = reader->offset == reader->len ? 1 : 0;
  return rr;
}

static void bench_init_fixed_target(bench_fixed_target *target) {
  memset(target, 0, sizeof(*target));
  target->record.lucky_numbers.items = target->numbers_storage;
  target->record.lucky_numbers.capacity = BENCH_PARSE_ITEM_CAPACITY;
  target->record.lucky_numbers.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  target->record.items.items = target->item_storage;
  target->record.items.capacity = BENCH_PARSE_ITEM_CAPACITY;
  target->record.items.elem_size = sizeof(target->item_storage[0]);
  target->record.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
}

static int bench_check_fixed_target_with_label(const bench_fixed_target *target,
                                               const char *expected_label) {
  const bench_item *items;

  items = (const bench_item *)target->record.items.items;
  if (strcmp(target->record.name, "Alice") != 0 ||
      strcmp(target->record.nickname, "Wonderland") != 0 ||
      target->record.age != 34 || target->record.score != 99.5 ||
      !target->record.active ||
      strcmp(target->record.address.city, "Stockholm") != 0 ||
      target->record.address.zip != 12345 ||
      target->record.lucky_numbers.count != 3u ||
      target->record.lucky_numbers.items[0] != 7 ||
      target->record.lucky_numbers.items[1] != 11 ||
      target->record.lucky_numbers.items[2] != 42 ||
      target->record.items.count != 2u || items[0].id != 1 ||
      strcmp(items[0].label, "alpha") != 0 || items[1].id != 2 ||
      strcmp(items[1].label, expected_label) != 0) {
    return 0;
  }
  return 1;
}

static int bench_check_parse_fixed_target(const bench_fixed_target *target) {
  return bench_check_fixed_target_with_label(target, "bravo-longer-th");
}

static int bench_check_stream_fixed_target(const bench_fixed_target *target) {
  return bench_check_fixed_target_with_label(target, "bravo");
}

static int bench_check_dynamic_record(const bench_record_dynamic *record) {
  const bench_item *items;

  items = (const bench_item *)record->items.items;
  if (record->name == NULL || strcmp(record->name, "Alice") != 0 ||
      strcmp(record->nickname, "Wonderland") != 0 || record->age != 34 ||
      record->score != 99.5 || !record->active ||
      strcmp(record->address.city, "Stockholm") != 0 ||
      record->address.zip != 12345 || record->lucky_numbers.count != 3u ||
      record->lucky_numbers.items[0] != 7 ||
      record->lucky_numbers.items[1] != 11 ||
      record->lucky_numbers.items[2] != 42 || record->tags.count != 2u ||
      record->tags.items[0] == NULL ||
      strcmp(record->tags.items[0], "admin") != 0 ||
      record->tags.items[1] == NULL ||
      strcmp(record->tags.items[1], "ops") != 0 || record->items.count != 2u ||
      items[0].id != 1 || strcmp(items[0].label, "alpha") != 0 ||
      items[1].id != 2 || strcmp(items[1].label, "bravo-longer-th") != 0) {
    return 0;
  }
  return 1;
}

static void bench_init_json_value_doc(bench_json_value_doc *doc) {
  memset(doc, 0, sizeof(*doc));
  lonejson_json_value_init(g_bench_runtime_default, &doc->selector);
  lonejson_json_value_init(g_bench_runtime_default, &doc->fields);
  lonejson_json_value_init(g_bench_runtime_default, &doc->last_error);
}

static void bench_cleanup_json_value_doc(bench_json_value_doc *doc) {
  lonejson_cleanup(&bench_json_value_doc_map, doc, NULL);
}

static int bench_check_json_value_doc(const bench_json_value_doc *doc) {
  return strcmp(doc->id, "q-42") == 0 &&
         doc->selector.kind == LONEJSON_JSON_VALUE_BUFFER &&
         doc->fields.kind == LONEJSON_JSON_VALUE_BUFFER &&
         doc->last_error.kind == LONEJSON_JSON_VALUE_BUFFER &&
         doc->selector.json != NULL &&
         strcmp(doc->selector.json, bench_json_value_selector_json) == 0 &&
         doc->fields.json != NULL &&
         strcmp(doc->fields.json, bench_json_value_fields_json) == 0 &&
         doc->last_error.json != NULL &&
         strcmp(doc->last_error.json, bench_json_value_last_error_json) == 0;
}

static int bench_prepare_json_value_doc(bench_json_value_doc *doc) {
  lonejson_error error;

  bench_init_json_value_doc(doc);
  memcpy(doc->id, "q-42", 5u);
  if (lonejson_json_value_set_buffer(&doc->selector,
                                     bench_json_value_selector_json,
                                     strlen(bench_json_value_selector_json),
                                     &error) != LONEJSON_STATUS_OK ||
      lonejson_json_value_set_buffer(&doc->fields, bench_json_value_fields_json,
                                     strlen(bench_json_value_fields_json),
                                     &error) != LONEJSON_STATUS_OK ||
      lonejson_json_value_set_buffer(&doc->last_error,
                                     bench_json_value_last_error_json,
                                     strlen(bench_json_value_last_error_json),
                                     &error) != LONEJSON_STATUS_OK) {
    bench_cleanup_json_value_doc(doc);
    return 0;
  }
  return 1;
}

static int bench_prepare_json_value_source_doc(bench_json_value_doc *doc,
                                               const char *fields_path) {
  lonejson_error error;

  if (!bench_prepare_json_value_doc(doc)) {
    return 0;
  }
  if (lonejson_json_value_set_path(&doc->fields, fields_path, &error) !=
      LONEJSON_STATUS_OK) {
    bench_cleanup_json_value_doc(doc);
    return 0;
  }
  return 1;
}

static void bench_init_vendor_long_target(bench_vendor_long_target *target) {
  memset(target, 0, sizeof(*target));
  target->doc.x.items = target->items;
  target->doc.x.capacity = sizeof(target->items) / sizeof(target->items[0]);
  target->doc.x.elem_size = sizeof(target->items[0]);
  target->doc.x.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
}

static void bench_init_workload_target(bench_workload_target *target) {
  memset(target, 0, sizeof(*target));
  target->record.cases.items = target->case_storage;
  target->record.cases.capacity =
      sizeof(target->case_storage) / sizeof(target->case_storage[0]);
  target->record.cases.elem_size = sizeof(target->case_storage[0]);
  target->record.cases.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
}

static int bench_check_workload_record(const bench_workload_target *target,
                                       size_t index) {
  const bench_workload_case_result *cases;

  if (index >= 5u) {
    return 0;
  }
  cases = (const bench_workload_case_result *)target->record.cases.items;
  if (strcmp(target->record.run_id, bench_workload_expected_run_ids[index]) !=
          0 ||
      strcmp(target->record.git.short_sha,
             bench_workload_expected_short_shas[index]) != 0 ||
      target->record.git.dirty !=
          (bench_workload_expected_dirty[index] != 0 ? true : false) ||
      target->record.config.concurrency != 8 ||
      target->record.cases.count != 9u) {
    return 0;
  }
  if (strcmp(cases[0].workload, "attachments") != 0 ||
      strcmp(cases[4].workload, "public-read") != 0 ||
      strcmp(cases[8].workload, "xa-rollback") != 0 ||
      cases[0].phases.acquire.ops != 4000) {
    return 0;
  }
  return 1;
}

static int
bench_check_vendor_long_target(const bench_vendor_long_target *target) {
  return strcmp(target->doc.id, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") ==
             0 &&
         target->doc.x.count == 1u &&
         strcmp(target->items[0].id,
                "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") == 0;
}

static int bench_check_vendor_numbers(const bench_vendor_numbers_doc *doc) {
  return doc->min < -9.9e27 && doc->min > -1.1e28 && doc->max > 9.9e27 &&
         doc->max < 1.1e28;
}

static int bench_check_vendor_unicode(const bench_vendor_unicode_doc *doc) {
  if (doc == NULL) {
    return 0;
  }
  return strcmp(doc->title, "\320\237\320\276\320\273\321\202\320\276\321\200"
                            "\320\260 \320\227\320\265\320\274\320\273\320\265"
                            "\320\272\320\276\320\277\320\260") == 0;
}

static int
bench_check_vendor_language_text(bench_vendor_doc_kind kind,
                                 const bench_vendor_unicode_doc *doc) {
  const char *prefix;
  size_t prefix_len;
  size_t len;

  if (doc == NULL) {
    return 0;
  }
  switch (kind) {
  case BENCH_VENDOR_JAPANESE_UTF8:
    prefix = bench_vendor_japanese_prefix;
    break;
  case BENCH_VENDOR_HEBREW_UTF8:
    prefix = bench_vendor_hebrew_prefix;
    break;
  case BENCH_VENDOR_ARABIC_UTF8:
    prefix = bench_vendor_arabic_prefix;
    break;
  default:
    return 0;
  }
  prefix_len = strlen(prefix);
  len = strlen(doc->title);
  return len > prefix_len && strncmp(doc->title, prefix, prefix_len) == 0;
}

static int
bench_check_vendor_language_wide(bench_vendor_doc_kind kind,
                                 const bench_vendor_language_wide_doc *doc) {
  const char *lang;
  const char *script;
  const char *source;
  const char *summary_prefix;
  const char *body_prefix;
  lonejson_int64 min_chars;

  if (doc == NULL) {
    return 0;
  }
  switch (kind) {
  case BENCH_VENDOR_JAPANESE_WIDE:
    lang = "ja";
    script = "japanese";
    source = bench_vendor_japanese_wide_source;
    summary_prefix = bench_vendor_japanese_prefix;
    body_prefix = bench_vendor_japanese_prefix;
    min_chars = 180;
    break;
  case BENCH_VENDOR_HEBREW_WIDE:
    lang = "he";
    script = "hebrew";
    source = bench_vendor_hebrew_wide_source;
    summary_prefix = "השועל מזמין את החסידה לביתו";
    body_prefix = bench_vendor_hebrew_prefix;
    min_chars = 220;
    break;
  case BENCH_VENDOR_ARABIC_WIDE:
    lang = "ar";
    script = "arabic";
    source = bench_vendor_arabic_wide_source;
    summary_prefix = "مقدمة تشرح سبب وضع كتاب كليلة";
    body_prefix = bench_vendor_arabic_prefix;
    min_chars = 220;
    break;
  default:
    return 0;
  }
  return strcmp(doc->lang, lang) == 0 && strcmp(doc->script, script) == 0 &&
         strcmp(doc->source, source) == 0 && doc->title[0] != '\0' &&
         strncmp(doc->summary, summary_prefix, strlen(summary_prefix)) == 0 &&
         strncmp(doc->body, body_prefix, strlen(body_prefix)) == 0 &&
         doc->metrics.segments == 4 && doc->metrics.chars >= min_chars;
}

static void bench_cleanup_fixed_target(const lonejson_map *map,
                                       bench_fixed_target *target) {
  if (((target->record.items.flags | target->record.lucky_numbers.flags) &
       LONEJSON_ARRAY_OWNS_ITEMS) != 0u)
    lonejson_cleanup(map, &target->record, NULL);
}

static int bench_parse_fixed_buffer_case(void *user) {
  bench_parse_case *ctx;
  bench_fixed_target target;
  lonejson_error error;
  lonejson_status status;
  int ok;

  ctx = (bench_parse_case *)user;
  bench_init_fixed_target(&target);
  status = lonejson_parse_buffer(ctx->runtime, ctx->map, &target.record,
                                 ctx->json, ctx->json_len, &error);
  ok = (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
       bench_check_parse_fixed_target(&target);
  if (ok)
    g_bench_sink += (lonejson_uint64)target.record.lucky_numbers.count;
  bench_cleanup_fixed_target(ctx->map, &target);
  return ok;
}

static int bench_parse_fixed_buffer_short_case(void *user) {
  bench_parse_case_short *ctx;
  bench_fixed_target target;
  lj_error error;
  lj_status status;
  int ok;

  ctx = (bench_parse_case_short *)user;
  bench_init_fixed_target(&target);
  status = lj_parse_buffer(ctx->runtime, ctx->map, &target.record, ctx->json,
                           ctx->json_len, &error);
  ok = (status == LJ_STATUS_OK || status == LJ_STATUS_TRUNCATED) &&
       bench_check_parse_fixed_target(&target);
  if (ok)
    g_bench_sink += (lj_uint64)target.record.lucky_numbers.count;
  bench_cleanup_fixed_target(ctx->map, &target);
  return ok;
}

static int bench_parse_dynamic_buffer_case(void *user) {
  bench_parse_case *ctx;
  bench_record_dynamic record;
  lonejson_error error;
  lonejson_status status;
  int ok;

  ctx = (bench_parse_case *)user;
  memset(&record, 0, sizeof(record));
  status = lonejson_parse_buffer(ctx->runtime, ctx->map, &record, ctx->json,
                                 ctx->json_len, &error);
  ok = (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
       bench_check_dynamic_record(&record);
  if (ok) {
    g_bench_sink += (lonejson_uint64)record.tags.count;
  }
  lonejson_cleanup(ctx->map, &record, NULL);
  return ok;
}

static int bench_parse_json_value_buffer_case(void *user) {
  bench_parse_case *ctx;
  bench_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  int ok;

  ctx = (bench_parse_case *)user;
  bench_init_json_value_doc(&doc);
  if (lonejson_json_value_enable_parse_capture(&doc.selector, &error) !=
          LONEJSON_STATUS_OK ||
      lonejson_json_value_enable_parse_capture(&doc.fields, &error) !=
          LONEJSON_STATUS_OK ||
      lonejson_json_value_enable_parse_capture(&doc.last_error, &error) !=
          LONEJSON_STATUS_OK) {
    bench_cleanup_json_value_doc(&doc);
    return 0;
  }
  status = lonejson_parse_buffer(ctx->runtime, ctx->map, &doc, ctx->json,
                                 ctx->json_len, &error);
  ok = status == LONEJSON_STATUS_OK && bench_check_json_value_doc(&doc);
  if (ok) {
    g_bench_sink += (lonejson_uint64)doc.fields.len;
  }
  bench_cleanup_json_value_doc(&doc);
  return ok;
}

static lonejson_status bench_visit_object_begin(void *user,
                                                lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)error;
  ++state->object_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_visit_array_begin(void *user,
                                               lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)error;
  ++state->array_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_visit_key_chunk(void *user, const char *data,
                                             size_t len,
                                             lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)data;
  (void)error;
  state->key_bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_visit_string_chunk(void *user, const char *data,
                                                size_t len,
                                                lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)data;
  (void)error;
  state->string_bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_visit_number_chunk(void *user, const char *data,
                                                size_t len,
                                                lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)data;
  (void)error;
  state->number_bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_visit_bool(void *user, int value,
                                        lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)value;
  (void)error;
  ++state->bool_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_visit_null(void *user, lonejson_error *error) {
  bench_visit_state *state = (bench_visit_state *)user;
  (void)error;
  ++state->null_count;
  return LONEJSON_STATUS_OK;
}

static int bench_visit_value_case(void *user) {
  bench_visit_case *ctx = (bench_visit_case *)user;
  bench_visit_state state;
  lonejson_value_visitor visitor;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  memset(&visitor, 0, sizeof(visitor));
  visitor.object_begin = bench_visit_object_begin;
  visitor.array_begin = bench_visit_array_begin;
  visitor.object_key_chunk = bench_visit_key_chunk;
  visitor.string_chunk = bench_visit_string_chunk;
  visitor.number_chunk = bench_visit_number_chunk;
  visitor.boolean_value = bench_visit_bool;
  visitor.null_value = bench_visit_null;

  if (ctx->use_reader) {
    bench_mem_reader reader;

    reader.data = ctx->json;
    reader.len = ctx->json_len;
    reader.offset = 0u;
    reader.chunk_size = ctx->reader_chunk_size;
    status = lonejson_visit_value_reader(g_bench_runtime_default,
                                         bench_mem_reader_fn, &reader, &visitor,
                                         &state, &error);
  } else {
    status =
        lonejson_visit_value_buffer(g_bench_runtime_default, ctx->json,
                                    ctx->json_len, &visitor, &state, &error);
  }
  if (status != LONEJSON_STATUS_OK) {
    return 0;
  }
  if (state.object_count < ctx->min_objects ||
      state.array_count < ctx->min_arrays ||
      state.key_bytes < ctx->min_key_bytes ||
      state.string_bytes < ctx->min_string_bytes ||
      state.number_bytes < ctx->min_number_bytes ||
      state.bool_count < ctx->min_bools || state.null_count < ctx->min_nulls) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)(state.object_count + state.array_count +
                                    state.key_bytes + state.string_bytes +
                                    state.number_bytes + state.bool_count +
                                    state.null_count);
  return 1;
}

static int bench_parse_reader_fixed_case(void *user) {
  bench_parse_case *ctx;
  bench_fixed_target target;
  bench_mem_reader reader;
  lonejson_error error;
  lonejson_status status;
  int ok;
  ctx = (bench_parse_case *)user;
  bench_init_fixed_target(&target);
  reader.data = (const unsigned char *)ctx->json;
  reader.len = ctx->json_len;
  reader.offset = 0u;
  reader.chunk_size = 97u;
  status = lonejson_parse_reader(ctx->runtime, ctx->map, &target.record,
                                 bench_mem_reader_fn, &reader, &error);
  ok = (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) &&
       bench_check_parse_fixed_target(&target);
  if (ok)
    g_bench_sink += (lonejson_uint64)target.record.items.count;
  bench_cleanup_fixed_target(ctx->map, &target);
  return ok;
}

static int bench_stream_fixed_case(void *user) {
  bench_stream_case *ctx;
  bench_mem_reader reader;
  lonejson_stream *stream;
  lonejson_stream_result result;
  lonejson_error error;
  bench_fixed_target targets[2];
  size_t seen;

  ctx = (bench_stream_case *)user;
  reader.data = ctx->stream_json;
  reader.len = ctx->stream_len;
  reader.offset = 0u;
  reader.chunk_size = 83u;
  seen = 0u;
  stream = lonejson_stream_open_reader(ctx->runtime, ctx->map,
                                       bench_mem_reader_fn, &reader, &error);
  if (stream == NULL) {
    return 0;
  }
  for (;;) {
    bench_fixed_target *target = &targets[seen & 1u];
    bench_init_fixed_target(target);
    result = lonejson_stream_next(stream, &target->record, &error);
    if (result == LONEJSON_STREAM_EOF) {
      break;
    }
    if (result != LONEJSON_STREAM_OBJECT) {
      bench_cleanup_fixed_target(ctx->map, target);
      lonejson_stream_close(stream);
      return 0;
    }
    if (!bench_check_stream_fixed_target(target)) {
      bench_cleanup_fixed_target(ctx->map, target);
      lonejson_stream_close(stream);
      return 0;
    }
    ++seen;
    g_bench_sink += (lonejson_uint64)target->record.items.count;
    bench_cleanup_fixed_target(ctx->map, target);
  }
  lonejson_stream_close(stream);
  return seen == ctx->object_count;
}

static int bench_stream_vendor_doc_case(void *user) {
  bench_vendor_doc_case *ctx;
  bench_mem_reader reader;
  lonejson_stream *stream;
  lonejson_stream_result result;
  lonejson_error error;
  ctx = (bench_vendor_doc_case *)user;
  reader.data = ctx->json;
  reader.len = ctx->json_len;
  reader.offset = 0u;
  /* Use the configured stream-sized reader path here. Tiny synthetic chunks
   * overstress callback overhead and were the remaining source of full-suite
   * instability in the vendor stream lanes. */
  reader.chunk_size = LONEJSON_STREAM_BUFFER_SIZE;
  stream = lonejson_stream_open_reader(
      ctx->kind == BENCH_VENDOR_LONG_STRINGS ? g_bench_runtime_reuse
                                             : g_bench_runtime_default,
      ctx->map, bench_mem_reader_fn, &reader, &error);
  if (stream == NULL) {
    return 0;
  }
  if (ctx->kind == BENCH_VENDOR_LONG_STRINGS) {
    bench_vendor_long_target target;
    bench_init_vendor_long_target(&target);
    result = lonejson_stream_next(stream, &target.doc, &error);
    lonejson_stream_close(stream);
    return result == LONEJSON_STREAM_OBJECT &&
           bench_check_vendor_long_target(&target);
  }
  if (ctx->kind == BENCH_VENDOR_EXTREME_NUMBERS) {
    bench_vendor_numbers_doc doc;
    memset(&doc, 0, sizeof(doc));
    result = lonejson_stream_next(stream, &doc, &error);
    lonejson_stream_close(stream);
    return result == LONEJSON_STREAM_OBJECT && bench_check_vendor_numbers(&doc);
  }
  if (ctx->kind == BENCH_VENDOR_STRING_UNICODE) {
    bench_vendor_unicode_doc doc;
    memset(&doc, 0, sizeof(doc));
    result = lonejson_stream_next(stream, &doc, &error);
    lonejson_stream_close(stream);
    return result == LONEJSON_STREAM_OBJECT && bench_check_vendor_unicode(&doc);
  }
  if (ctx->kind == BENCH_VENDOR_JAPANESE_UTF8 ||
      ctx->kind == BENCH_VENDOR_HEBREW_UTF8 ||
      ctx->kind == BENCH_VENDOR_ARABIC_UTF8) {
    bench_vendor_unicode_doc doc;
    memset(&doc, 0, sizeof(doc));
    result = lonejson_stream_next(stream, &doc, &error);
    lonejson_stream_close(stream);
    return result == LONEJSON_STREAM_OBJECT &&
           bench_check_vendor_language_text(ctx->kind, &doc);
  }
  if (ctx->kind == BENCH_VENDOR_JAPANESE_WIDE ||
      ctx->kind == BENCH_VENDOR_HEBREW_WIDE ||
      ctx->kind == BENCH_VENDOR_ARABIC_WIDE) {
    bench_vendor_language_wide_doc doc;
    memset(&doc, 0, sizeof(doc));
    result = lonejson_stream_next(stream, &doc, &error);
    lonejson_stream_close(stream);
    return result == LONEJSON_STREAM_OBJECT &&
           bench_check_vendor_language_wide(ctx->kind, &doc);
  }
  lonejson_stream_close(stream);
  return 0;
}

static int bench_stream_workload_case(void *user) {
  bench_workload_case *ctx;
  bench_mem_reader reader;
  lonejson_stream *stream;
  lonejson_stream_result result;
  lonejson_error error;
  bench_workload_target target;
  size_t seen;

  ctx = (bench_workload_case *)user;
  bench_init_workload_target(&target);
  reader.data = ctx->jsonl;
  reader.len = ctx->jsonl_len;
  reader.offset = 0u;
  reader.chunk_size = 257u;
  stream = lonejson_stream_open_reader(g_bench_runtime_default, ctx->map,
                                       bench_mem_reader_fn, &reader, &error);
  if (stream == NULL) {
    return 0;
  }
  seen = 0u;
  for (;;) {
    result = lonejson_stream_next(stream, &target.record, &error);
    if (result == LONEJSON_STREAM_EOF) {
      break;
    }
    if (result != LONEJSON_STREAM_OBJECT ||
        !bench_check_workload_record(&target, seen)) {
      lonejson_stream_close(stream);
      return 0;
    }
    g_bench_sink += (lonejson_uint64)target.record.cases.count;
    ++seen;
  }
  lonejson_stream_close(stream);
  return seen == ctx->object_count;
}

static int bench_check_array_stream_item(const bench_item *item, size_t index) {
  if (index == 0u) {
    return item->id == 1 && strcmp(item->label, "alpha") == 0;
  }
  if (index == 1u) {
    return item->id == 2 && strcmp(item->label, "bravo") == 0;
  }
  return 0;
}

static lonejson_status bench_array_stream_item_cb(void *user, void *dst) {
  bench_array_stream_count *state;
  bench_item *item;

  state = (bench_array_stream_count *)user;
  item = (bench_item *)dst;
  if (!bench_check_array_stream_item(item, state->count)) {
    return LONEJSON_STATUS_INVALID_JSON;
  }
  ++state->count;
  state->bytes += strlen(item->label);
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_array_stream_string_begin(void *user,
                                                       lonejson_error *error) {
  bench_array_stream_count *state;
  (void)error;
  state = (bench_array_stream_count *)user;
  ++state->count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_array_stream_string_chunk(void *user,
                                                       const char *data,
                                                       size_t len,
                                                       lonejson_error *error) {
  bench_array_stream_count *state;
  (void)data;
  (void)error;
  state = (bench_array_stream_count *)user;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status bench_array_stream_string_end(void *user,
                                                     lonejson_error *error) {
  (void)user;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
bench_array_stream_string_item_cb(void *user, const char *data, size_t len,
                                  lonejson_error *error) {
  bench_array_stream_count *state;
  (void)error;
  if (data == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  state = (bench_array_stream_count *)user;
  if ((state->count == 0u && (len != 5u || strncmp(data, "admin", len) != 0)) ||
      (state->count == 1u && (len != 3u || strncmp(data, "ops", len) != 0))) {
    return LONEJSON_STATUS_INVALID_JSON;
  }
  ++state->count;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static int bench_array_stream_pull_mapped_case(void *user) {
  bench_array_stream_case *ctx;
  bench_mem_reader reader;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  bench_item item;
  size_t seen;

  ctx = (bench_array_stream_case *)user;
  reader.data = ctx->json;
  reader.len = ctx->json_len;
  reader.offset = 0u;
  reader.chunk_size = ctx->chunk_size;
  stream = lonejson_array_stream_open_reader(
      g_bench_runtime_default, ctx->path, bench_mem_reader_fn, &reader, &error);
  if (stream == NULL) {
    return 0;
  }
  seen = 0u;
  for (;;) {
    memset(&item, 0, sizeof(item));
    result = lonejson_array_stream_next(stream, &bench_item_map, &item, &error);
    if (result == LONEJSON_ARRAY_STREAM_EOF) {
      break;
    }
    if (result != LONEJSON_ARRAY_STREAM_ITEM ||
        !bench_check_array_stream_item(&item, seen)) {
      lonejson_array_stream_close(stream);
      return 0;
    }
    ++seen;
    g_bench_sink += (lonejson_uint64)item.id;
  }
  lonejson_array_stream_close(stream);
  return seen == ctx->item_count;
}

static int bench_array_stream_pull_value_case(void *user) {
  bench_array_stream_case *ctx;
  bench_mem_reader reader;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_json_value value;
  lonejson_error error;
  size_t seen;

  ctx = (bench_array_stream_case *)user;
  reader.data = ctx->json;
  reader.len = ctx->json_len;
  reader.offset = 0u;
  reader.chunk_size = ctx->chunk_size;
  stream = lonejson_array_stream_open_reader(
      g_bench_runtime_default, ctx->path, bench_mem_reader_fn, &reader, &error);
  if (stream == NULL) {
    return 0;
  }
  lonejson_json_value_init(g_bench_runtime_default, &value);
  seen = 0u;
  for (;;) {
    lonejson_json_value_reset(&value);
    result = lonejson_array_stream_next_value(stream, &value, &error);
    if (result == LONEJSON_ARRAY_STREAM_EOF) {
      break;
    }
    if (result != LONEJSON_ARRAY_STREAM_ITEM ||
        value.kind != LONEJSON_JSON_VALUE_BUFFER || value.json == NULL ||
        value.len < 10u) {
      lonejson_json_value_cleanup(&value);
      lonejson_array_stream_close(stream);
      return 0;
    }
    ++seen;
    g_bench_sink += (lonejson_uint64)value.len;
  }
  lonejson_json_value_cleanup(&value);
  lonejson_array_stream_close(stream);
  return seen == ctx->item_count;
}

static int bench_array_stream_push_mapped_case(void *user) {
  bench_array_stream_case *ctx;
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  bench_item item;
  bench_array_stream_count state;
  size_t offset;

  ctx = (bench_array_stream_case *)user;
  stream = lonejson_array_stream_open_push(g_bench_runtime_default, ctx->path,
                                           &error);
  if (stream == NULL) {
    return 0;
  }
  memset(&item, 0, sizeof(item));
  memset(&state, 0, sizeof(state));
  offset = 0u;
  while (offset < ctx->json_len) {
    size_t chunk = ctx->chunk_size;
    if (chunk > ctx->json_len - offset) {
      chunk = ctx->json_len - offset;
    }
    status = lonejson_array_stream_push(
        stream, &bench_item_map, &item, ctx->json + offset, chunk,
        bench_array_stream_item_cb, &state, &error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_array_stream_close(stream);
      return 0;
    }
    offset += chunk;
  }
  status =
      lonejson_array_stream_finish(stream, &bench_item_map, &item,
                                   bench_array_stream_item_cb, &state, &error);
  lonejson_array_stream_close(stream);
  if (status != LONEJSON_STATUS_OK || state.count != ctx->item_count) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)state.bytes;
  return 1;
}

static int bench_array_stream_push_string_case(void *user) {
  bench_array_stream_case *ctx;
  lonejson_array_stream *stream;
  lonejson_array_stream_string_handler handler;
  lonejson_error error;
  lonejson_status status;
  bench_array_stream_count state;
  size_t offset;

  ctx = (bench_array_stream_case *)user;
  stream = lonejson_array_stream_open_push(g_bench_runtime_default, ctx->path,
                                           &error);
  if (stream == NULL) {
    return 0;
  }
  memset(&handler, 0, sizeof(handler));
  handler.begin = bench_array_stream_string_begin;
  handler.chunk = bench_array_stream_string_chunk;
  handler.end = bench_array_stream_string_end;
  memset(&state, 0, sizeof(state));
  offset = 0u;
  while (offset < ctx->json_len) {
    size_t chunk = ctx->chunk_size;
    if (chunk > ctx->json_len - offset) {
      chunk = ctx->json_len - offset;
    }
    status = lonejson_array_stream_push_string(stream, ctx->json + offset,
                                               chunk, &handler, &state, &error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_array_stream_close(stream);
      return 0;
    }
    offset += chunk;
  }
  status =
      lonejson_array_stream_finish_string(stream, &handler, &state, &error);
  lonejson_array_stream_close(stream);
  if (status != LONEJSON_STATUS_OK || state.count != ctx->item_count ||
      state.bytes != 8u) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)state.bytes;
  return 1;
}

static int bench_array_stream_push_string_items_case(void *user) {
  bench_array_stream_case *ctx;
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  bench_array_stream_count state;
  size_t offset;

  ctx = (bench_array_stream_case *)user;
  stream = lonejson_array_stream_open_push(g_bench_runtime_default, ctx->path,
                                           &error);
  if (stream == NULL) {
    return 0;
  }
  memset(&state, 0, sizeof(state));
  offset = 0u;
  while (offset < ctx->json_len) {
    size_t chunk = ctx->chunk_size;
    if (chunk > ctx->json_len - offset) {
      chunk = ctx->json_len - offset;
    }
    status = lonejson_array_stream_push_string_items(
        stream, ctx->json + offset, chunk, bench_array_stream_string_item_cb,
        &state, &error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_array_stream_close(stream);
      return 0;
    }
    offset += chunk;
  }
  status = lonejson_array_stream_finish_string_items(
      stream, bench_array_stream_string_item_cb, &state, &error);
  lonejson_array_stream_close(stream);
  if (status != LONEJSON_STATUS_OK || state.count != ctx->item_count ||
      state.bytes != 8u) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)state.bytes;
  return 1;
}

static lonejson_status bench_count_sink_write(void *user, const void *data,
                                              size_t len,
                                              lonejson_error *error) {
  bench_count_sink *sink;
  (void)data;
  (void)error;
  sink = (bench_count_sink *)user;
  sink->total += len;
  return LONEJSON_STATUS_OK;
}

static int bench_serialize_sink_case(void *user) {
  bench_serialize_case *ctx;
  bench_count_sink sink;
  lonejson_error error;

  ctx = (bench_serialize_case *)user;
  memset(&sink, 0, sizeof(sink));
  if (lonejson_serialize_sink(ctx->runtime, ctx->map, ctx->src,
                              bench_count_sink_write, &sink,
                              &error) != LONEJSON_STATUS_OK) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)sink.total;
  return sink.total == ctx->expected_len;
}

static int bench_serialize_buffer_case(void *user) {
  bench_serialize_case *ctx;
  char buffer[1024];
  lonejson_error error;
  lonejson_status status;
  size_t needed;

  ctx = (bench_serialize_case *)user;
  status = lonejson_serialize_buffer(ctx->runtime, ctx->map, ctx->src, buffer,
                                     sizeof(buffer), &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)needed;
  return strcmp(buffer, ctx->expected_json) == 0;
}

static int bench_serialize_alloc_case(void *user) {
  bench_serialize_case *ctx;
  lonejson_error error;
  char *json;
  int ok;

  ctx = (bench_serialize_case *)user;
  json =
      lonejson_serialize_alloc(ctx->runtime, ctx->map, ctx->src, NULL, &error);
  if (json == NULL) {
    return 0;
  }
  ok = strcmp(json, ctx->expected_json) == 0;
  if (ok) {
    g_bench_sink += (lonejson_uint64)strlen(json);
  }
  LONEJSON_FREE(json);
  return ok;
}

static int bench_serialize_jsonl_sink_case(void *user) {
  bench_jsonl_serialize_case *ctx;
  bench_count_sink sink;
  lonejson_error error;

  ctx = (bench_jsonl_serialize_case *)user;
  memset(&sink, 0, sizeof(sink));
  if (lonejson_serialize_jsonl_sink(
          ctx->runtime, ctx->map, ctx->items, ctx->count, ctx->stride,
          bench_count_sink_write, &sink, &error) != LONEJSON_STATUS_OK) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)sink.total;
  return sink.total == ctx->expected_len;
}

static int bench_serialize_jsonl_buffer_case(void *user) {
  bench_jsonl_serialize_case *ctx;
  char buffer[4096];
  lonejson_error error;
  lonejson_status status;
  size_t needed;

  ctx = (bench_jsonl_serialize_case *)user;
  status = lonejson_serialize_jsonl_buffer(ctx->runtime, ctx->map, ctx->items,
                                           ctx->count, ctx->stride, buffer,
                                           sizeof(buffer), &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)needed;
  return strcmp(buffer, ctx->expected_jsonl) == 0;
}

static int bench_serialize_jsonl_alloc_case(void *user) {
  bench_jsonl_serialize_case *ctx;
  lonejson_error error;
  char *jsonl;
  int ok;

  ctx = (bench_jsonl_serialize_case *)user;
  jsonl = lonejson_serialize_jsonl_alloc(ctx->runtime, ctx->map, ctx->items,
                                         ctx->count, ctx->stride, NULL, &error);
  if (jsonl == NULL) {
    return 0;
  }
  ok = strcmp(jsonl, ctx->expected_jsonl) == 0;
  if (ok) {
    g_bench_sink += (lonejson_uint64)strlen(jsonl);
  }
  LONEJSON_FREE(jsonl);
  return ok;
}

static int bench_writer_value_stream_sink_case(void *user) {
  bench_writer_value_stream_bench_case *ctx;
  bench_count_sink sink;
  lonejson_writer writer;
  lonejson_writer_value_stream stream;
  lonejson_error error;
  lonejson_status status;
  size_t off;

  ctx = (bench_writer_value_stream_bench_case *)user;
  memset(&sink, 0, sizeof(sink));
  memset(&stream, 0, sizeof(stream));
  status = lonejson_writer_init_sink(g_bench_runtime_default, &writer,
                                     bench_count_sink_write, &sink, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 0;
  }
  status = lonejson_writer_begin_object(&writer, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_key(&writer, "result", 6u, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_value_stream_open(&stream, &writer, &error);
  }
  off = 0u;
  while (status == LONEJSON_STATUS_OK && off < ctx->json_len) {
    size_t take = ctx->json_len - off;
    if (take > ctx->chunk_size) {
      take = ctx->chunk_size;
    }
    status = lonejson_writer_value_stream_push(&stream, ctx->json + off, take,
                                               &error);
    off += take;
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_value_stream_close(&stream, &error);
  } else {
    lonejson_writer_value_stream_cleanup(&stream);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_end_object(&writer, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, &error);
  }
  lonejson_writer_cleanup(&writer);
  if (status != LONEJSON_STATUS_OK) {
    return 0;
  }
  g_bench_sink += (lonejson_uint64)sink.total;
  return sink.total >= ctx->json_len;
}

static void bench_fill_rates(bench_result *result) {
  double elapsed_seconds;
  bench_measurement measurement;
  double bytes_per_call;

  if (result->elapsed_ns == 0u || result->total_bytes == 0u) {
    return;
  }
  elapsed_seconds = (double)result->elapsed_ns / 1000000000.0;
  result->mib_per_sec =
      ((double)result->total_bytes / (1024.0 * 1024.0)) / elapsed_seconds;
  result->docs_per_sec = (double)result->total_documents / elapsed_seconds;
  result->ns_per_byte =
      (double)result->elapsed_ns / (double)result->total_bytes;
  memset(&measurement, 0, sizeof(measurement));
  measurement.batch_iterations = (unsigned long)result->batch_iterations;
  measurement.sample_count = result->samples.count;
  memcpy(measurement.samples, result->samples.items,
         result->samples.count * sizeof(bench_sample));
  if (!bench_measure_summarize(&measurement))
    return;
  bytes_per_call =
      (double)result->total_bytes / (double)result->batch_iterations;
  result->mib_per_sec =
      bytes_per_call * 1000000000.0 / (measurement.mean_cost_ns * 1048576.0);
  result->docs_per_sec =
      ((double)result->total_documents / (double)result->batch_iterations) *
      1000000000.0 / measurement.mean_cost_ns;
  result->ns_per_byte = measurement.mean_cost_ns / bytes_per_call;
  result->mib_per_sec_low =
      bytes_per_call * 1000000000.0 / (measurement.cost_high_ns * 1048576.0);
  result->mib_per_sec_high =
      bytes_per_call * 1000000000.0 / (measurement.cost_low_ns * 1048576.0);
  result->relative_error_pct = measurement.relative_error_pct;
  result->drift_ci_low_pct = measurement.drift_ci_low_pct;
  result->drift_ci_high_pct = measurement.drift_ci_high_pct;
}

typedef struct bench_simple_batch_context {
  bench_simple_case_fn fn;
  void *context;
} bench_simple_batch_context;

static lonejson_uint64 bench_measure_clock(void *context, int cpu) {
  (void)context;
  return bench_now_ns(cpu ? CLOCK_PROCESS_CPUTIME_ID : CLOCK_MONOTONIC);
}

static lonejson_uint64 bench_simple_batch(void *context, unsigned long count) {
  bench_simple_batch_context *batch;
  lonejson_uint64 mismatches;
  unsigned long i;
  batch = (bench_simple_batch_context *)context;
  mismatches = 0u;
  for (i = 0u; i < count; ++i) {
    if (!batch->fn(batch->context))
      ++mismatches;
  }
  return mismatches;
}

static int bench_validation_adapter(void *context) {
  lonejson_uint64 mismatches;
  bench_validation_case *validation;
  validation = (bench_validation_case *)context;
  (void)bench_validation_once(validation, &mismatches);
  g_bench_sink += (lonejson_uint64)validation->doc_count;
  return mismatches == 0u;
}

typedef struct bench_case_entry {
  bench_result *result;
  const char *name;
  const char *group;
  bench_simple_case_fn fn;
  void *context;
  unsigned iterations;
  size_t bytes;
  size_t documents;
  bench_validation_case validation;
} bench_case_entry;

static bench_case_entry g_bench_cases[BENCH_MAX_RESULTS];
static size_t g_bench_case_count;

static void bench_add_case(bench_result *result, const char *name,
                           const char *group, bench_simple_case_fn fn,
                           void *context, unsigned iterations, size_t bytes,
                           size_t documents) {
  bench_case_entry *entry;
  if (g_bench_filter != NULL && strcmp(g_bench_filter, name) != 0)
    return;
  if (g_bench_case_count == BENCH_MAX_RESULTS)
    exit(1);
  entry = &g_bench_cases[g_bench_case_count++];
  entry->result = result;
  entry->name = name;
  entry->group = group;
  entry->fn = fn;
  entry->context = context;
  entry->iterations = iterations;
  entry->bytes = bytes;
  entry->documents = documents;
  if (fn == bench_validation_adapter) {
    entry->validation = *(bench_validation_case *)context;
    entry->context = &entry->validation;
  }
}

/* A flat case table is shared with Lua. Load it once per worker. */
typedef struct bench_budget {
  char name[64];
  unsigned long ns;
} bench_budget;
static bench_budget g_bench_budgets[128];
static size_t g_bench_budget_count;
static unsigned long bench_case_budget(const char *name) {
  size_t i;
  if (g_bench_budget_count == 0u) {
    const char *path = getenv("LONEJSON_BENCH_PLAN");
    FILE *file = fopen(path == NULL ? "bench/workloads.tsv" : path, "r");
    char runtime[8];
    if (file == NULL) {
      perror("benchmark case budgets");
      exit(1);
    }
    while (g_bench_budget_count < 128u) {
      bench_budget *entry = &g_bench_budgets[g_bench_budget_count];
      int fields =
          fscanf(file, "%7s %63s %lu", runtime, entry->name, &entry->ns);
      if (fields == EOF)
        break;
      if (fields != 3 || entry->ns == 0u || entry->ns > 1000000000u)
        exit(1);
      ++g_bench_budget_count;
    }
    fclose(file);
  }
  for (i = 0; i < g_bench_budget_count; ++i)
    if (strcmp(name, g_bench_budgets[i].name) == 0)
      return g_bench_budgets[i].ns;
  fprintf(stderr, "missing benchmark budget: %s\n", name);
  exit(1);
  return 0u;
}

static void bench_run_simple_case(bench_result *result, const char *name,
                                  const char *group, bench_simple_case_fn fn,
                                  void *context, unsigned iterations,
                                  size_t bytes_per_call, size_t docs_per_call) {
  bench_measurement measurement;
  bench_simple_batch_context batch;
  if (g_bench_filter != NULL && strcmp(g_bench_filter, name) != 0)
    return;
  memset(result, 0, sizeof(*result));
  snprintf(result->name, sizeof(result->name), "%s", name);
  snprintf(result->group, sizeof(result->group), "%s", group);
  result->target_sample_ns = bench_case_budget(name);
  batch.fn = fn;
  batch.context = context;
  if (bytes_per_call == 0u || docs_per_call == 0u) {
    fprintf(stderr, "benchmark workload is empty for %s\n", name);
    result->mismatch_count = 1u;
    return;
  }
  if (g_bench_plan != NULL) {
    const bench_result *planned = bench_find_result(g_bench_plan, name);
    if (planned == NULL ||
        planned->target_sample_ns != result->target_sample_ns ||
        planned->batch_iterations == 0u ||
        planned->batch_iterations > ULONG_MAX) {
      fputs("missing or invalid benchmark plan\n", stderr);
      exit(1);
    }
    iterations = (unsigned)planned->batch_iterations;
    if (planned->batch_iterations > UINT_MAX) {
      fputs("benchmark plan exceeds supported work count\n", stderr);
      exit(1);
    }
  }
  if (!bench_measure(&measurement, bench_measure_clock, NULL,
                     bench_simple_batch, &batch, iterations,
                     g_bench_plan != NULL, result->target_sample_ns)) {
    fprintf(stderr,
            "benchmark measurement failed for %s: calibration, clock or "
            "correctness failure\n",
            name);
    result->mismatch_count =
        measurement.mismatch_count != 0u ? measurement.mismatch_count : 1u;
    return;
  }
  if ((lonejson_uint64)measurement.batch_iterations >
          (lonejson_uint64)-1 / (lonejson_uint64)bytes_per_call ||
      (lonejson_uint64)measurement.batch_iterations >
          (lonejson_uint64)-1 / (lonejson_uint64)docs_per_call) {
    fprintf(stderr, "benchmark work counter overflow for %s\n", name);
    result->mismatch_count = 1u;
    return;
  }
  result->samples.items = calloc(BENCH_SAMPLE_COUNT, sizeof(bench_sample));
  if (result->samples.items == NULL) {
    perror("benchmark samples");
    exit(1);
  }
  memcpy(result->samples.items, measurement.samples,
         sizeof(measurement.samples));
  result->samples.count = measurement.sample_count;
  result->samples.capacity = BENCH_SAMPLE_COUNT;
  result->samples.elem_size = sizeof(bench_sample);
  result->samples.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  result->batch_iterations = (lonejson_uint64)measurement.batch_iterations;
  result->warmup_elapsed_ns = measurement.warmup_elapsed_ns;
  result->mismatch_count = measurement.mismatch_count;
  result->cv_pct = measurement.cv_pct;
  result->drift_pct = measurement.drift_pct;
  result->elapsed_ns = bench_measure_elapsed_ns(&measurement);
  result->total_bytes =
      (lonejson_uint64)bytes_per_call * result->batch_iterations;
  result->total_documents =
      (lonejson_uint64)docs_per_call * result->batch_iterations;
  bench_fill_rates(result);
}

static void bench_run_validation_case(bench_result *result, const char *name,
                                      bench_validate_fn fn,
                                      const bench_doc *docs, size_t doc_count,
                                      unsigned iterations,
                                      lonejson_uint64 total_bytes) {
  bench_validation_case context;
  context.docs = docs;
  context.doc_count = doc_count;
  context.validate_fn = fn;
  bench_add_case(result, name, "validation", bench_validation_adapter, &context,
                 iterations, (size_t)total_bytes, doc_count);
}

static void bench_print_run_report(const bench_run *run) {
  size_t i;

  printf("benchmark run %s on %s\n", run->timestamp_utc, run->host);
  printf("corpus: %s (%.0f files, %.0f bytes)\n", run->corpus_path,
         (double)run->corpus_file_count, (double)run->corpus_total_bytes);
  printf(
      "config: parser=%.0f push=%.0f reader=%.0f stream=%.0f iterations=%.0f\n",
      (double)run->parser_buffer_size, (double)run->push_parser_buffer_size,
      (double)run->reader_buffer_size, (double)run->stream_buffer_size,
      (double)run->iterations);
  printf("%-40s %-12s %11s %12s %12s %10s\n", "benchmark", "group", "MiB/s",
         "docs/s", "ns/byte", "mismatch");
  printf("%-40s %-12s %11s %12s %12s %10s\n",
         "----------------------------------------", "------------",
         "-----------", "------------", "------------", "----------");
  for (i = 0; i < run->results.count; ++i) {
    const bench_result *result;
    result = &run->result_storage[i];
    printf("%-40s %-12s %11.3f %12.1f %12.3f %10.0f\n", result->name,
           result->group, result->mib_per_sec, result->docs_per_sec,
           result->ns_per_byte, (double)result->mismatch_count);
  }
}

static int bench_make_parent_dirs(const char *path) {
  char tmp[PATH_MAX];
  char *slash;

  snprintf(tmp, sizeof(tmp), "%s", path);
  slash = strrchr(tmp, '/');
  if (slash == NULL) {
    return 0;
  }
  *slash = '\0';
  if (tmp[0] == '\0') {
    return 0;
  }
  if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
    return 1;
  }
  return 0;
}

static int bench_write_archive(const bench_run *run, const char *archive_dir) {
  char archive_path[PATH_MAX];
  char stamp[32];
  lonejson_error error;
  struct stat st;

  if (stat(archive_dir, &st) != 0) {
    if (mkdir(archive_dir, 0777) != 0 && errno != EEXIST) {
      return 1;
    }
  }
  bench_u64_to_string(run->timestamp_epoch_ns, stamp, sizeof(stamp));
  snprintf(archive_path, sizeof(archive_path), "%s/%s.json", archive_dir,
           stamp);
  return lonejson_serialize_path(g_bench_runtime_default, &bench_run_map, run,
                                 archive_path, &error) == LONEJSON_STATUS_OK
             ? 0
             : 1;
}

static int bench_write_outputs(const bench_run *run, const char *latest_path,
                               const char *history_path,
                               const char *archive_dir) {
  FILE *history_fp;
  lonejson_error error;

  if (bench_make_parent_dirs(latest_path) != 0 ||
      bench_make_parent_dirs(history_path) != 0 ||
      bench_make_parent_dirs(archive_dir) != 0) {
    return 1;
  }
  if (lonejson_serialize_path(g_bench_runtime_default, &bench_run_map, run,
                              latest_path, &error) != LONEJSON_STATUS_OK) {
    return 1;
  }
  history_fp = fopen(history_path, "ab");
  if (history_fp == NULL) {
    return 1;
  }
  if (lonejson_serialize_jsonl_filep(g_bench_runtime_default, &bench_run_map,
                                     run, 1u, sizeof(*run), history_fp,
                                     &error) != LONEJSON_STATUS_OK) {
    fclose(history_fp);
    return 1;
  }
  fclose(history_fp);
  return bench_write_archive(run, archive_dir);
}

static void bench_prepare_serialization_source(bench_record_fixed *record) {
  static lonejson_int64 numbers[] = {7, 11, 42};
  static bench_item items[] = {{1, "alpha"}, {2, "bravo"}};

  memset(record, 0, sizeof(*record));
  strcpy(record->name, "Alice");
  strcpy(record->nickname, "Wonderland");
  record->age = 34;
  record->score = 99.5;
  record->active = true;
  strcpy(record->address.city, "Stockholm");
  record->address.zip = 12345;
  record->lucky_numbers.items = numbers;
  record->lucky_numbers.count = 3u;
  record->lucky_numbers.capacity = 3u;
  record->lucky_numbers.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  record->items.items = items;
  record->items.count = 2u;
  record->items.capacity = 2u;
  record->items.elem_size = sizeof(items[0]);
  record->items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
}

static void bench_prepare_jsonl_source(bench_record_fixed *records,
                                       size_t record_count, char *out_jsonl,
                                       size_t out_jsonl_capacity) {
  size_t i;
  size_t used;
  lonejson_error error;

  used = 0u;
  for (i = 0; i < record_count; ++i) {
    bench_prepare_serialization_source(&records[i]);
    records[i].age += (lonejson_int64)i;
    records[i].address.zip += (lonejson_int64)i;
  }
  if (lonejson_serialize_jsonl_buffer(
          g_bench_runtime_default, &bench_record_fixed_map, records,
          record_count, sizeof(records[0]), out_jsonl, out_jsonl_capacity,
          &used, &error) != LONEJSON_STATUS_OK) {
    fprintf(stderr, "failed to prepare benchmark jsonl source: %s\n",
            error.message);
    exit(1);
  }
}

static int bench_prepare_benchmark_cases(
    bench_parse_case *parse_fixed_case,
    bench_parse_case_short *parse_fixed_short_case,
    bench_parse_case *parse_dynamic_case, bench_stream_case *stream_case,
    bench_serialize_case *serialize_case,
    bench_serialize_case *serialize_pretty_case,
    bench_parse_case *parse_json_value_case,
    bench_serialize_case *json_value_serialize_case,
    bench_serialize_case *json_value_serialize_pretty_case,
    bench_serialize_case *json_value_source_serialize_case,
    bench_serialize_case *json_value_source_serialize_pretty_case,
    bench_jsonl_serialize_case *jsonl_case,
    bench_jsonl_serialize_case *jsonl_pretty_case,
    bench_record_fixed *serialize_record, bench_record_fixed *jsonl_records,
    bench_json_value_doc *json_value_record,
    bench_json_value_doc *json_value_source_record, char *expected_json,
    size_t expected_json_capacity, char *expected_pretty_json,
    size_t expected_pretty_json_capacity, char *jsonl_buffer,
    size_t jsonl_buffer_capacity, char *pretty_jsonl_buffer,
    size_t pretty_jsonl_buffer_capacity, char *stream_buffer,
    size_t stream_buffer_capacity, char *json_value_path,
    size_t json_value_path_capacity) {
  const char *json_value_source_file =
      "tests/fixtures/languages/japanese_hojoki_wide.json";
  char *json_value_expected = NULL;
  char *json_value_pretty_expected = NULL;
  char *json_value_source_expected = NULL;
  char *json_value_source_pretty_expected = NULL;
  size_t offset;
  size_t i;

  bench_prepare_serialization_source(serialize_record);
  if (bench_read_text_file("tests/golden/bench_record_compact.json",
                           expected_json, expected_json_capacity) != 0) {
    return 1;
  }
  if (bench_read_text_file("tests/golden/bench_record_pretty.json",
                           expected_pretty_json,
                           expected_pretty_json_capacity) != 0) {
    return 1;
  }

  parse_fixed_case->runtime = g_bench_runtime_default;
  parse_fixed_case->map = &bench_record_fixed_map;
  parse_fixed_case->json = bench_parse_json;
  parse_fixed_case->json_len = strlen(bench_parse_json);
  parse_fixed_case->object_count = 1u;
  parse_fixed_short_case->runtime = g_bench_runtime_default;
  parse_fixed_short_case->map = &bench_record_fixed_map;
  parse_fixed_short_case->json = bench_parse_json;
  parse_fixed_short_case->json_len = strlen(bench_parse_json);
  parse_fixed_short_case->object_count = 1u;

  parse_dynamic_case->runtime = g_bench_runtime_default;
  parse_dynamic_case->map = &bench_record_dynamic_map;
  parse_dynamic_case->json = bench_parse_json;
  parse_dynamic_case->json_len = strlen(bench_parse_json);
  parse_dynamic_case->object_count = 1u;

  bench_prepare_jsonl_source(jsonl_records, BENCH_JSONL_RECORDS, jsonl_buffer,
                             jsonl_buffer_capacity);
  if (bench_read_text_file("tests/golden/bench_record_compact.jsonl",
                           jsonl_buffer, jsonl_buffer_capacity) != 0) {
    return 1;
  }
  if (bench_read_text_file("tests/golden/bench_record_pretty.jsonl",
                           pretty_jsonl_buffer,
                           pretty_jsonl_buffer_capacity) != 0) {
    return 1;
  }
  jsonl_case->runtime = g_bench_runtime_default;
  jsonl_case->map = &bench_record_fixed_map;
  jsonl_case->items = jsonl_records;
  jsonl_case->count = BENCH_JSONL_RECORDS;
  jsonl_case->stride = sizeof(jsonl_records[0]);
  jsonl_case->expected_jsonl = jsonl_buffer;
  jsonl_case->expected_len = strlen(jsonl_buffer);
  *jsonl_pretty_case = *jsonl_case;
  jsonl_pretty_case->runtime = g_bench_runtime_pretty;
  jsonl_pretty_case->expected_jsonl = pretty_jsonl_buffer;
  jsonl_pretty_case->expected_len = strlen(pretty_jsonl_buffer);

  offset = 0u;
  for (i = 0u; i < BENCH_JSONL_RECORDS; ++i) {
    size_t len;
    len = strlen(expected_json);
    if (offset + len > stream_buffer_capacity) {
      fprintf(stderr, "benchmark stream buffer too small\n");
      return 1;
    }
    memcpy(stream_buffer + offset, expected_json, len);
    offset += len;
  }
  stream_case->runtime = g_bench_runtime_default;
  stream_case->map = &bench_record_fixed_map;
  stream_case->stream_json = (const unsigned char *)stream_buffer;
  stream_case->stream_len = offset;
  stream_case->object_count = BENCH_JSONL_RECORDS;

  serialize_case->runtime = g_bench_runtime_default;
  serialize_case->map = &bench_record_fixed_map;
  serialize_case->src = serialize_record;
  serialize_case->expected_json = expected_json;
  serialize_case->expected_len = strlen(expected_json);
  *serialize_pretty_case = *serialize_case;
  serialize_pretty_case->runtime = g_bench_runtime_pretty;
  serialize_pretty_case->expected_json = expected_pretty_json;
  serialize_pretty_case->expected_len = strlen(expected_pretty_json);

  parse_json_value_case->runtime = g_bench_runtime_reuse;
  parse_json_value_case->map = &bench_json_value_doc_map;
  parse_json_value_case->json = bench_json_value_parse_json;
  parse_json_value_case->json_len = strlen(bench_json_value_parse_json);
  parse_json_value_case->object_count = 1u;

  if (!bench_prepare_json_value_doc(json_value_record)) {
    return 1;
  }
  if (snprintf(json_value_path, json_value_path_capacity, "%s",
               json_value_source_file) >= (int)json_value_path_capacity) {
    bench_cleanup_json_value_doc(json_value_record);
    return 1;
  }
  if (!bench_prepare_json_value_source_doc(json_value_source_record,
                                           json_value_path)) {
    bench_cleanup_json_value_doc(json_value_record);
    return 1;
  }
  json_value_expected = lonejson_serialize_alloc(g_bench_runtime_default,
                                                 &bench_json_value_doc_map,
                                                 json_value_record, NULL, NULL);
  json_value_pretty_expected = lonejson_serialize_alloc(
      g_bench_runtime_pretty, &bench_json_value_doc_map, json_value_record,
      NULL, NULL);
  json_value_source_expected = lonejson_serialize_alloc(
      g_bench_runtime_default, &bench_json_value_doc_map,
      json_value_source_record, NULL, NULL);
  json_value_source_pretty_expected = lonejson_serialize_alloc(
      g_bench_runtime_pretty, &bench_json_value_doc_map,
      json_value_source_record, NULL, NULL);
  if (json_value_expected == NULL || json_value_pretty_expected == NULL ||
      json_value_source_expected == NULL ||
      json_value_source_pretty_expected == NULL) {
    LONEJSON_FREE(json_value_expected);
    LONEJSON_FREE(json_value_pretty_expected);
    LONEJSON_FREE(json_value_source_expected);
    LONEJSON_FREE(json_value_source_pretty_expected);
    bench_cleanup_json_value_doc(json_value_record);
    bench_cleanup_json_value_doc(json_value_source_record);
    return 1;
  }
  json_value_serialize_case->runtime = g_bench_runtime_default;
  json_value_serialize_case->map = &bench_json_value_doc_map;
  json_value_serialize_case->src = json_value_record;
  json_value_serialize_case->expected_json = json_value_expected;
  json_value_serialize_case->expected_len = strlen(json_value_expected);
  *json_value_serialize_pretty_case = *json_value_serialize_case;
  json_value_serialize_pretty_case->runtime = g_bench_runtime_pretty;
  json_value_serialize_pretty_case->expected_json = json_value_pretty_expected;
  json_value_serialize_pretty_case->expected_len =
      strlen(json_value_pretty_expected);

  json_value_source_serialize_case->runtime = g_bench_runtime_default;
  json_value_source_serialize_case->map = &bench_json_value_doc_map;
  json_value_source_serialize_case->src = json_value_source_record;
  json_value_source_serialize_case->expected_json = json_value_source_expected;
  json_value_source_serialize_case->expected_len =
      strlen(json_value_source_expected);
  *json_value_source_serialize_pretty_case = *json_value_source_serialize_case;
  json_value_source_serialize_pretty_case->runtime = g_bench_runtime_pretty;
  json_value_source_serialize_pretty_case->expected_json =
      json_value_source_pretty_expected;
  json_value_source_serialize_pretty_case->expected_len =
      strlen(json_value_source_pretty_expected);
  return 0;
}

static void bench_prepare_workload_case(bench_workload_case *workload_case,
                                        unsigned char *jsonl_data,
                                        size_t jsonl_len) {
  workload_case->map = &bench_workload_run_record_map;
  workload_case->jsonl = jsonl_data;
  workload_case->jsonl_len = jsonl_len;
  workload_case->object_count = 5u;
}

static int bench_run_command(const char *corpus_dir, const char *latest_path,
                             unsigned iterations) {
  bench_doc *docs;
  bench_run run = {0};
  bench_parse_case parse_fixed_case;
  bench_parse_case_short parse_fixed_short_case;
  bench_parse_case parse_fixed_prepared_case;
  bench_parse_case parse_dynamic_case;
  bench_parse_case parse_json_value_case;
  bench_stream_case stream_case;
  bench_stream_case stream_prepared_case;
  bench_array_stream_case array_items_case;
  bench_array_stream_case array_items_push_case;
  bench_array_stream_case array_items_push_tiny_case;
  bench_array_stream_case array_tags_case;
  bench_array_stream_case array_tags_push_tiny_case;
  bench_workload_case workload_case;
  bench_vendor_doc_case vendor_long_case;
  bench_vendor_doc_case vendor_numbers_case;
  bench_vendor_doc_case vendor_unicode_case;
  bench_vendor_doc_case vendor_japanese_case;
  bench_vendor_doc_case vendor_hebrew_case;
  bench_vendor_doc_case vendor_arabic_case;
  bench_vendor_doc_case vendor_japanese_wide_case;
  bench_vendor_doc_case vendor_hebrew_wide_case;
  bench_vendor_doc_case vendor_arabic_wide_case;
  bench_serialize_case serialize_case;
  bench_serialize_case serialize_pretty_case;
  bench_serialize_case json_value_serialize_case;
  bench_serialize_case json_value_serialize_pretty_case;
  bench_serialize_case json_value_source_serialize_case;
  bench_serialize_case json_value_source_serialize_pretty_case;
  bench_writer_value_stream_bench_case writer_value_stream_case;
  bench_writer_value_stream_bench_case writer_value_stream_tiny_case;
  bench_visit_case visit_selector_case;
  bench_visit_case visit_selector_reader_case;
  bench_visit_case visit_mixed_case;
  bench_visit_case visit_japanese_wide_case;
  bench_jsonl_serialize_case jsonl_case;
  bench_jsonl_serialize_case jsonl_pretty_case;
  bench_record_fixed serialize_record;
  bench_record_fixed jsonl_records[BENCH_JSONL_RECORDS];
  bench_json_value_doc json_value_record;
  bench_json_value_doc json_value_source_record;
  char expected_json[1024];
  char expected_pretty_json[2048];
  char jsonl_buffer[4096];
  char pretty_jsonl_buffer[8192];
  char stream_buffer[8192];
  char json_value_path[PATH_MAX];
  unsigned char *workload_jsonl_data;
  size_t workload_jsonl_len;
  unsigned char *japanese_json_data;
  size_t japanese_json_len;
  unsigned char *hebrew_json_data;
  size_t hebrew_json_len;
  unsigned char *arabic_json_data;
  size_t arabic_json_len;
  unsigned char *japanese_wide_json_data;
  size_t japanese_wide_json_len;
  unsigned char *hebrew_wide_json_data;
  size_t hebrew_wide_json_len;
  unsigned char *arabic_wide_json_data;
  size_t arabic_wide_json_len;
  size_t doc_count;
  lonejson_uint64 total_bytes;
  lonejson_uint64 y_count;
  lonejson_uint64 n_count;
  lonejson_uint64 i_count;

  workload_jsonl_data = NULL;
  workload_jsonl_len = 0u;
  japanese_json_data = NULL;
  japanese_json_len = 0u;
  hebrew_json_data = NULL;
  hebrew_json_len = 0u;
  arabic_json_data = NULL;
  arabic_json_len = 0u;
  japanese_wide_json_data = NULL;
  japanese_wide_json_len = 0u;
  hebrew_wide_json_data = NULL;
  hebrew_wide_json_len = 0u;
  arabic_wide_json_data = NULL;
  arabic_wide_json_len = 0u;
  if (bench_load_corpus(corpus_dir, &docs, &doc_count, &total_bytes, &y_count,
                        &n_count, &i_count) != 0) {
    bench_run_cleanup(&run);
    return 1;
  }
  if (bench_read_file("tests/fixtures/workloadbench.jsonl",
                      &workload_jsonl_data, &workload_jsonl_len) != 0) {
    bench_free_docs(docs, doc_count);
    bench_run_cleanup(&run);
    return 1;
  }
  if (bench_read_file("tests/fixtures/languages/japanese_hojoki.json",
                      &japanese_json_data, &japanese_json_len) != 0 ||
      bench_read_file("tests/fixtures/languages/hebrew_fox_and_stork.json",
                      &hebrew_json_data, &hebrew_json_len) != 0 ||
      bench_read_file("tests/fixtures/languages/arabic_kalila_intro.json",
                      &arabic_json_data, &arabic_json_len) != 0 ||
      bench_read_file("tests/fixtures/languages/japanese_hojoki_wide.json",
                      &japanese_wide_json_data, &japanese_wide_json_len) != 0 ||
      bench_read_file("tests/fixtures/languages/hebrew_fox_and_stork_wide.json",
                      &hebrew_wide_json_data, &hebrew_wide_json_len) != 0 ||
      bench_read_file("tests/fixtures/languages/arabic_kalila_intro_wide.json",
                      &arabic_wide_json_data, &arabic_wide_json_len) != 0) {
    free(workload_jsonl_data);
    free(japanese_json_data);
    free(hebrew_json_data);
    free(arabic_json_data);
    free(japanese_wide_json_data);
    free(hebrew_wide_json_data);
    free(arabic_wide_json_data);
    bench_free_docs(docs, doc_count);
    bench_run_cleanup(&run);
    return 1;
  }

  bench_run_prepare(&run);
  run.schema_version = BENCH_SCHEMA_VERSION;
  bench_fill_measurement_metadata(&run);
  bench_fill_timestamp(&run);
  bench_fill_host_and_compiler(&run);
  bench_store_portable_path(run.corpus_path, sizeof(run.corpus_path),
                            corpus_dir);
  run.corpus_file_count = (lonejson_uint64)doc_count;
  run.corpus_total_bytes = total_bytes;
  run.corpus_y_count = y_count;
  run.corpus_n_count = n_count;
  run.corpus_i_count = i_count;
  run.iterations = (lonejson_uint64)iterations;
  run.parser_buffer_size = (lonejson_uint64)LONEJSON_PARSER_BUFFER_SIZE;
  run.push_parser_buffer_size =
      (lonejson_uint64)LONEJSON_PUSH_PARSER_BUFFER_SIZE;
  run.reader_buffer_size = (lonejson_uint64)LONEJSON_READER_BUFFER_SIZE;
  run.stream_buffer_size = (lonejson_uint64)LONEJSON_STREAM_BUFFER_SIZE;
  run.results.count = 0u;

  if (bench_prepare_benchmark_cases(
          &parse_fixed_case, &parse_fixed_short_case, &parse_dynamic_case,
          &stream_case, &serialize_case, &serialize_pretty_case,
          &parse_json_value_case, &json_value_serialize_case,
          &json_value_serialize_pretty_case, &json_value_source_serialize_case,
          &json_value_source_serialize_pretty_case, &jsonl_case,
          &jsonl_pretty_case, &serialize_record, jsonl_records,
          &json_value_record, &json_value_source_record, expected_json,
          sizeof(expected_json), expected_pretty_json,
          sizeof(expected_pretty_json), jsonl_buffer, sizeof(jsonl_buffer),
          pretty_jsonl_buffer, sizeof(pretty_jsonl_buffer), stream_buffer,
          sizeof(stream_buffer), json_value_path,
          sizeof(json_value_path)) != 0) {
    free(workload_jsonl_data);
    free(japanese_json_data);
    free(hebrew_json_data);
    free(arabic_json_data);
    free(japanese_wide_json_data);
    free(hebrew_wide_json_data);
    free(arabic_wide_json_data);
    bench_free_docs(docs, doc_count);
    bench_run_cleanup(&run);
    return 1;
  }
  parse_fixed_prepared_case = parse_fixed_case;
  parse_fixed_prepared_case.runtime = g_bench_runtime_reuse;
  stream_prepared_case = stream_case;
  stream_prepared_case.runtime = g_bench_runtime_reuse;
  array_items_case.path = "items";
  array_items_case.json = (const unsigned char *)bench_array_stream_json;
  array_items_case.json_len = strlen(bench_array_stream_json);
  array_items_case.item_count = 2u;
  array_items_case.chunk_size = LONEJSON_READER_BUFFER_SIZE;
  array_items_push_case = array_items_case;
  array_items_push_case.chunk_size = LONEJSON_PUSH_PARSER_BUFFER_SIZE;
  array_items_push_tiny_case = array_items_case;
  array_items_push_tiny_case.chunk_size = 7u;
  array_tags_case.path = "tags";
  array_tags_case.json = (const unsigned char *)bench_array_stream_json;
  array_tags_case.json_len = strlen(bench_array_stream_json);
  array_tags_case.item_count = 2u;
  array_tags_case.chunk_size = LONEJSON_PUSH_PARSER_BUFFER_SIZE;
  array_tags_push_tiny_case = array_tags_case;
  array_tags_push_tiny_case.chunk_size = 5u;
  writer_value_stream_case.json =
      (const unsigned char *)bench_json_value_fields_json;
  writer_value_stream_case.json_len = strlen(bench_json_value_fields_json);
  writer_value_stream_case.chunk_size = 4096u;
  writer_value_stream_tiny_case = writer_value_stream_case;
  writer_value_stream_tiny_case.chunk_size = 7u;
  bench_prepare_workload_case(&workload_case, workload_jsonl_data,
                              workload_jsonl_len);
  vendor_long_case.kind = BENCH_VENDOR_LONG_STRINGS;
  vendor_long_case.map = &bench_vendor_long_doc_map;
  vendor_long_case.json = (const unsigned char *)bench_vendor_long_strings_json;
  vendor_long_case.json_len = strlen(bench_vendor_long_strings_json);
  vendor_numbers_case.kind = BENCH_VENDOR_EXTREME_NUMBERS;
  vendor_numbers_case.map = &bench_vendor_numbers_map;
  vendor_numbers_case.json =
      (const unsigned char *)bench_vendor_extreme_numbers_json;
  vendor_numbers_case.json_len = strlen(bench_vendor_extreme_numbers_json);
  vendor_unicode_case.kind = BENCH_VENDOR_STRING_UNICODE;
  vendor_unicode_case.map = &bench_vendor_unicode_map;
  vendor_unicode_case.json =
      (const unsigned char *)bench_vendor_string_unicode_json;
  vendor_unicode_case.json_len = strlen(bench_vendor_string_unicode_json);
  vendor_japanese_case.kind = BENCH_VENDOR_JAPANESE_UTF8;
  vendor_japanese_case.map = &bench_vendor_unicode_map;
  vendor_japanese_case.json = japanese_json_data;
  vendor_japanese_case.json_len = japanese_json_len;
  vendor_hebrew_case.kind = BENCH_VENDOR_HEBREW_UTF8;
  vendor_hebrew_case.map = &bench_vendor_unicode_map;
  vendor_hebrew_case.json = hebrew_json_data;
  vendor_hebrew_case.json_len = hebrew_json_len;
  vendor_arabic_case.kind = BENCH_VENDOR_ARABIC_UTF8;
  vendor_arabic_case.map = &bench_vendor_unicode_map;
  vendor_arabic_case.json = arabic_json_data;
  vendor_arabic_case.json_len = arabic_json_len;
  vendor_japanese_wide_case.kind = BENCH_VENDOR_JAPANESE_WIDE;
  vendor_japanese_wide_case.map = &bench_vendor_language_wide_map;
  vendor_japanese_wide_case.json = japanese_wide_json_data;
  vendor_japanese_wide_case.json_len = japanese_wide_json_len;
  vendor_hebrew_wide_case.kind = BENCH_VENDOR_HEBREW_WIDE;
  vendor_hebrew_wide_case.map = &bench_vendor_language_wide_map;
  vendor_hebrew_wide_case.json = hebrew_wide_json_data;
  vendor_hebrew_wide_case.json_len = hebrew_wide_json_len;
  vendor_arabic_wide_case.kind = BENCH_VENDOR_ARABIC_WIDE;
  vendor_arabic_wide_case.map = &bench_vendor_language_wide_map;
  vendor_arabic_wide_case.json = arabic_wide_json_data;
  vendor_arabic_wide_case.json_len = arabic_wide_json_len;
  memset(&visit_selector_case, 0, sizeof(visit_selector_case));
  visit_selector_case.json =
      (const unsigned char *)bench_json_value_selector_json;
  visit_selector_case.json_len = strlen(bench_json_value_selector_json);
  visit_selector_case.reader_chunk_size = 0u;
  visit_selector_case.min_key_bytes = 20u;
  visit_selector_case.min_string_bytes = 18u;
  visit_selector_case.min_number_bytes = 1u;
  visit_selector_case.min_objects = 3u;
  visit_selector_case.min_arrays = 1u;
  visit_selector_case.use_reader = 0;
  visit_selector_reader_case = visit_selector_case;
  visit_selector_reader_case.use_reader = 1;
  visit_selector_reader_case.reader_chunk_size = 17u;
  memset(&visit_mixed_case, 0, sizeof(visit_mixed_case));
  visit_mixed_case.json = (const unsigned char *)bench_visit_mixed_value_json;
  visit_mixed_case.json_len = strlen(bench_visit_mixed_value_json);
  visit_mixed_case.reader_chunk_size = 0u;
  visit_mixed_case.min_key_bytes = 30u;
  visit_mixed_case.min_string_bytes = 25u;
  visit_mixed_case.min_number_bytes = 15u;
  visit_mixed_case.min_objects = 5u;
  visit_mixed_case.min_arrays = 2u;
  visit_mixed_case.min_bools = 2u;
  visit_mixed_case.min_nulls = 1u;
  visit_mixed_case.use_reader = 0;
  memset(&visit_japanese_wide_case, 0, sizeof(visit_japanese_wide_case));
  visit_japanese_wide_case.json = japanese_wide_json_data;
  visit_japanese_wide_case.json_len = japanese_wide_json_len;
  visit_japanese_wide_case.reader_chunk_size = 257u;
  visit_japanese_wide_case.min_key_bytes = 30u;
  visit_japanese_wide_case.min_string_bytes = 400u;
  visit_japanese_wide_case.min_number_bytes = 1u;
  visit_japanese_wide_case.min_objects = 2u;
  visit_japanese_wide_case.min_arrays = 0u;
  visit_japanese_wide_case.use_reader = 1;

  {
    size_t result_index = 0u;

    bench_run_validation_case(
        &run.result_storage[result_index++], "validate/corpus/lonejson",
        bench_lonejson_validate, docs, doc_count, iterations, total_bytes);
    bench_add_case(&run.result_storage[result_index++],
                   "parse/buffer_fixed/lonejson", "parse",
                   bench_parse_fixed_buffer_case, &parse_fixed_case, iterations,
                   parse_fixed_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "parse/buffer_dynamic/lonejson", "parse",
                   bench_parse_dynamic_buffer_case, &parse_dynamic_case,
                   iterations, parse_dynamic_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++], "parse/buffer_fixed/lj",
                   "parse", bench_parse_fixed_buffer_short_case,
                   &parse_fixed_short_case, iterations,
                   parse_fixed_short_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "parse/buffer_fixed_prepared/lonejson", "parse",
                   bench_parse_fixed_buffer_case, &parse_fixed_prepared_case,
                   iterations, parse_fixed_prepared_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "parse/reader_fixed/lonejson", "parse",
                   bench_parse_reader_fixed_case, &parse_fixed_case, iterations,
                   parse_fixed_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "parse/reader_fixed_prepared/lonejson", "parse",
                   bench_parse_reader_fixed_case, &parse_fixed_prepared_case,
                   iterations, parse_fixed_prepared_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/object_fixed/lonejson", "stream",
                   bench_stream_fixed_case, &stream_case, iterations,
                   stream_case.stream_len, stream_case.object_count);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/object_fixed_prepared/lonejson", "stream",
                   bench_stream_fixed_case, &stream_prepared_case, iterations,
                   stream_prepared_case.stream_len,
                   stream_prepared_case.object_count);
    bench_add_case(&run.result_storage[result_index++],
                   "array_stream/pull_mapped_items/lonejson", "array_stream",
                   bench_array_stream_pull_mapped_case, &array_items_case,
                   iterations, array_items_case.json_len,
                   array_items_case.item_count);
    bench_add_case(&run.result_storage[result_index++],
                   "array_stream/pull_raw_items/lonejson", "array_stream",
                   bench_array_stream_pull_value_case, &array_items_case,
                   iterations, array_items_case.json_len,
                   array_items_case.item_count);
    bench_add_case(&run.result_storage[result_index++],
                   "array_stream/push_mapped_items/lonejson", "array_stream",
                   bench_array_stream_push_mapped_case, &array_items_push_case,
                   iterations, array_items_push_case.json_len,
                   array_items_push_case.item_count);
    bench_add_case(&run.result_storage[result_index++],
                   "array_stream/push_mapped_items_tiny/lonejson",
                   "array_stream", bench_array_stream_push_mapped_case,
                   &array_items_push_tiny_case, iterations,
                   array_items_push_tiny_case.json_len,
                   array_items_push_tiny_case.item_count);
    bench_add_case(&run.result_storage[result_index++],
                   "array_stream/push_string_chunks/lonejson", "array_stream",
                   bench_array_stream_push_string_case, &array_tags_case,
                   iterations, array_tags_case.json_len,
                   array_tags_case.item_count);
    bench_add_case(&run.result_storage[result_index++],
                   "array_stream/push_string_items_tiny/lonejson",
                   "array_stream", bench_array_stream_push_string_items_case,
                   &array_tags_push_tiny_case, iterations,
                   array_tags_push_tiny_case.json_len,
                   array_tags_push_tiny_case.item_count);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_long_strings/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_long_case, iterations,
                   vendor_long_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_extreme_numbers/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_numbers_case,
                   iterations, vendor_numbers_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_string_unicode/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_unicode_case,
                   iterations, vendor_unicode_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/workloadbench/lonejson", "stream",
                   bench_stream_workload_case, &workload_case, iterations,
                   workload_case.jsonl_len, workload_case.object_count);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_japanese/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_japanese_case,
                   iterations, vendor_japanese_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_hebrew/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_hebrew_case,
                   iterations, vendor_hebrew_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_arabic/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_arabic_case,
                   iterations, vendor_arabic_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_japanese_wide/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_japanese_wide_case,
                   iterations, vendor_japanese_wide_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_hebrew_wide/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_hebrew_wide_case,
                   iterations, vendor_hebrew_wide_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "stream/doc_arabic_wide/lonejson", "stream",
                   bench_stream_vendor_doc_case, &vendor_arabic_wide_case,
                   iterations, vendor_arabic_wide_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/sink/lonejson", "serialize",
                   bench_serialize_sink_case, &serialize_case, iterations,
                   serialize_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/buffer/lonejson", "serialize",
                   bench_serialize_buffer_case, &serialize_case, iterations,
                   serialize_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/alloc/lonejson", "serialize",
                   bench_serialize_alloc_case, &serialize_case, iterations,
                   serialize_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/jsonl_sink/lonejson", "serialize",
                   bench_serialize_jsonl_sink_case, &jsonl_case, iterations,
                   jsonl_case.expected_len, BENCH_JSONL_RECORDS);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/jsonl_buffer/lonejson", "serialize",
                   bench_serialize_jsonl_buffer_case, &jsonl_case, iterations,
                   jsonl_case.expected_len, BENCH_JSONL_RECORDS);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/jsonl_alloc/lonejson", "serialize",
                   bench_serialize_jsonl_alloc_case, &jsonl_case, iterations,
                   jsonl_case.expected_len, BENCH_JSONL_RECORDS);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/sink_pretty/lonejson", "serialize",
                   bench_serialize_sink_case, &serialize_pretty_case,
                   iterations, serialize_pretty_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/buffer_pretty/lonejson", "serialize",
                   bench_serialize_buffer_case, &serialize_pretty_case,
                   iterations, serialize_pretty_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/alloc_pretty/lonejson", "serialize",
                   bench_serialize_alloc_case, &serialize_pretty_case,
                   iterations, serialize_pretty_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/jsonl_sink_pretty/lonejson", "serialize",
                   bench_serialize_jsonl_sink_case, &jsonl_pretty_case,
                   iterations, jsonl_pretty_case.expected_len,
                   BENCH_JSONL_RECORDS);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/jsonl_buffer_pretty/lonejson", "serialize",
                   bench_serialize_jsonl_buffer_case, &jsonl_pretty_case,
                   iterations, jsonl_pretty_case.expected_len,
                   BENCH_JSONL_RECORDS);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/jsonl_alloc_pretty/lonejson", "serialize",
                   bench_serialize_jsonl_alloc_case, &jsonl_pretty_case,
                   iterations, jsonl_pretty_case.expected_len,
                   BENCH_JSONL_RECORDS);
    bench_add_case(&run.result_storage[result_index++],
                   "parse/buffer_json_value/lonejson", "parse",
                   bench_parse_json_value_buffer_case, &parse_json_value_case,
                   iterations, parse_json_value_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/json_value_sink/lonejson", "serialize",
                   bench_serialize_sink_case, &json_value_serialize_case,
                   iterations, json_value_serialize_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/json_value_alloc/lonejson", "serialize",
                   bench_serialize_alloc_case, &json_value_serialize_case,
                   iterations, json_value_serialize_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/json_value_sink_pretty/lonejson", "serialize",
                   bench_serialize_sink_case, &json_value_serialize_pretty_case,
                   iterations, json_value_serialize_pretty_case.expected_len,
                   1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/json_value_alloc_pretty/lonejson", "serialize",
                   bench_serialize_alloc_case,
                   &json_value_serialize_pretty_case, iterations,
                   json_value_serialize_pretty_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/json_value_source_alloc/lonejson", "serialize",
                   bench_serialize_alloc_case,
                   &json_value_source_serialize_case, iterations,
                   json_value_source_serialize_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "serialize/json_value_source_alloc_pretty/lonejson",
                   "serialize", bench_serialize_alloc_case,
                   &json_value_source_serialize_pretty_case, iterations,
                   json_value_source_serialize_pretty_case.expected_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "writer/value_stream_sink/lonejson", "writer",
                   bench_writer_value_stream_sink_case,
                   &writer_value_stream_case, iterations,
                   writer_value_stream_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "writer/value_stream_sink_tiny/lonejson", "writer",
                   bench_writer_value_stream_sink_case,
                   &writer_value_stream_tiny_case, iterations,
                   writer_value_stream_tiny_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "visit/value_selector/lonejson", "visit",
                   bench_visit_value_case, &visit_selector_case, iterations,
                   visit_selector_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "visit/value_selector_reader/lonejson", "visit",
                   bench_visit_value_case, &visit_selector_reader_case,
                   iterations, visit_selector_reader_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "visit/value_mixed/lonejson", "visit",
                   bench_visit_value_case, &visit_mixed_case, iterations,
                   visit_mixed_case.json_len, 1u);
    bench_add_case(&run.result_storage[result_index++],
                   "visit/value_japanese_wide/lonejson", "visit",
                   bench_visit_value_case, &visit_japanese_wide_case,
                   iterations, visit_japanese_wide_case.json_len, 1u);
    run.results.count = result_index;
  }
  {
    const char *round_text = getenv("LONEJSON_BENCH_ROUND");
    unsigned round = round_text == NULL ? 0u : (unsigned)atoi(round_text);
    size_t i;
    for (i = 0u; i < g_bench_case_count; ++i) {
      size_t index = bench_measure_case_index(g_bench_case_count, round, i);
      bench_case_entry *entry = &g_bench_cases[index];
      bench_run_simple_case(entry->result, entry->name, entry->group, entry->fn,
                            entry->context, entry->iterations, entry->bytes,
                            entry->documents);
    }
  }
  {
    size_t i, count = 0u;
    lonejson_error error;
    for (i = 0u; i < run.results.count; ++i) {
      if (run.result_storage[i].name[0] == '\0')
        continue;
      if (i != count) {
        run.result_storage[count] = run.result_storage[i];
        memset(&run.result_storage[i], 0, sizeof(run.result_storage[i]));
      }
      ++count;
    }
    run.results.count = count;
    if (count == 0u || bench_count_result_mismatches(&run) != 0u ||
        lonejson_serialize_path(g_bench_runtime_default, &bench_run_map, &run,
                                latest_path, &error) != LONEJSON_STATUS_OK) {
      fputs("benchmark round failed: empty, incorrect or unwritable result\n",
            stderr);
      exit(1);
    }
  }
  bench_cleanup_json_value_doc(&json_value_record);
  bench_cleanup_json_value_doc(&json_value_source_record);
  LONEJSON_FREE((void *)json_value_serialize_case.expected_json);
  LONEJSON_FREE((void *)json_value_serialize_pretty_case.expected_json);
  LONEJSON_FREE((void *)json_value_source_serialize_case.expected_json);
  LONEJSON_FREE((void *)json_value_source_serialize_pretty_case.expected_json);
  free(workload_jsonl_data);
  free(japanese_json_data);
  free(hebrew_json_data);
  free(arabic_json_data);
  free(japanese_wide_json_data);
  free(hebrew_wide_json_data);
  free(arabic_wide_json_data);
  bench_free_docs(docs, doc_count);
  bench_run_cleanup(&run);
  return 0;
}

static int bench_read_run(const char *path, bench_run *run) {
  lonejson_error error;

  bench_run_prepare(run);
  if (lonejson_parse_path(g_bench_runtime_reuse, &bench_run_map, run, path,
                          &error) != LONEJSON_STATUS_OK) {
    fprintf(stderr, "cannot read benchmark run %s: %s\n", path, error.message);
    return 1;
  }
  run->results.items = run->result_storage;
  return 0;
}

static int bench_read_last_history_line(const char *history_path,
                                        char **line_out, size_t *len_out) {
  FILE *input;
  char *line;
  char *last;
  size_t capacity;
  size_t last_length;
  ssize_t length;
  if (line_out == NULL || len_out == NULL)
    return 1;
  input = fopen(history_path, "rb");
  if (input == NULL) {
    perror("benchmark history");
    return 1;
  }
  line = NULL;
  last = NULL;
  capacity = 0u;
  last_length = 0u;
  while ((length = getline(&line, &capacity, input)) >= 0) {
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
      --length;
    if (length == 0)
      continue;
    free(last);
    last = (char *)malloc((size_t)length + 1u);
    if (last == NULL) {
      free(line);
      fclose(input);
      return 1;
    }
    memcpy(last, line, (size_t)length);
    last[length] = '\0';
    last_length = (size_t)length;
  }
  free(line);
  if (ferror(input)) {
    free(last);
    fclose(input);
    return 1;
  }
  fclose(input);
  if (last == NULL)
    return 1;
  *line_out = last;
  *len_out = last_length;
  return 0;
}

static int bench_tree_id_valid(const char *value) {
  size_t i;
  size_t length = strlen(value);
  if (length != 40u && length != 64u)
    return 0;
  for (i = 0u; i < length; ++i)
    if (!((value[i] >= '0' && value[i] <= '9') ||
          (value[i] >= 'a' && value[i] <= 'f')))
      return 0;
  return 1;
}

static int bench_freeze_baseline_command(const char *history_path,
                                         const char *baseline_path) {
  bench_run last = {0};
  int status;
  lonejson_error error;
  char *last_line;
  size_t last_len;

  last_line = NULL;
  last_len = 0u;
  if (bench_read_last_history_line(history_path, &last_line, &last_len) != 0) {
    bench_run_cleanup(&last);
    return 1;
  }
  bench_run_prepare(&last);
  if (lonejson_parse_buffer(g_bench_runtime_reuse, &bench_run_map, &last,
                            last_line, last_len,
                            &error) != LONEJSON_STATUS_OK) {
    free(last_line);
    bench_run_cleanup(&last);
    return 1;
  }
  free(last_line);
  if (strstr(last.source_revision, ":dirty") != NULL ||
      !bench_tree_id_valid(last.source_src_tree) ||
      !bench_tree_id_valid(last.source_include_tree) ||
      !bench_tree_id_valid(last.source_lua_tree)) {
    fputs(
        "benchmark freeze requires committed, versioned implementation trees\n",
        stderr);
    bench_run_cleanup(&last);
    return 1;
  }
  if (!bench_validate_loaded_run(&last) ||
      bench_count_result_mismatches(&last) != 0u ||
      bench_count_imprecise_results(&last) != 0u) {
    fprintf(stderr,
            "benchmark freeze-baseline failed: history contains incomplete or "
            "corrupted run data\n");
    bench_run_cleanup(&last);
    return 1;
  }
  if (bench_make_parent_dirs(baseline_path) != 0) {
    bench_run_cleanup(&last);
    return 1;
  }
  status =
      lonejson_serialize_path(g_bench_runtime_default, &bench_run_map, &last,
                              baseline_path, &error) == LONEJSON_STATUS_OK
          ? 0
          : 1;
  bench_run_cleanup(&last);
  return status;
}

static const bench_result *bench_find_result(const bench_run *run,
                                             const char *name) {
  size_t i;

  for (i = 0; i < run->results.count; ++i) {
    if (strcmp(run->result_storage[i].name, name) == 0) {
      return &run->result_storage[i];
    }
  }
  return NULL;
}

static int bench_result_measurement_valid(const bench_result *result) {
  bench_measurement measured;
  double expected;
  size_t i;
  lonejson_uint64 mismatches;
  size_t processes[BENCH_PROCESS_COUNT] = {0};
  lonejson_uint64 work[BENCH_PROCESS_COUNT] = {0};
  if (result->samples.count != BENCH_SAMPLE_COUNT ||
      result->samples.items == NULL || result->target_sample_ns == 0u ||
      result->target_sample_ns > 1000000000u ||
      result->batch_iterations == 0u || result->batch_iterations > ULONG_MAX ||
      result->warmup_elapsed_ns == 0u || result->total_bytes == 0u ||
      result->total_documents == 0u || !(result->mib_per_sec > 0.0) ||
      !(result->docs_per_sec > 0.0) || !(result->ns_per_byte > 0.0) ||
      !(result->cv_pct >= 0.0) || !(result->drift_pct >= 0.0) ||
      !(result->drift_ci_low_pct <= result->drift_ci_high_pct) ||
      result->total_bytes % result->batch_iterations != 0u ||
      result->total_documents % result->batch_iterations != 0u)
    return 0;
  memset(&measured, 0, sizeof(measured));
  measured.batch_iterations = (unsigned long)result->batch_iterations;
  measured.sample_count = BENCH_SAMPLE_COUNT;
  memcpy(measured.samples, result->samples.items, sizeof(measured.samples));
  if (!bench_measure_summarize(&measured))
    return 0;
  mismatches = 0u;
  for (i = 0u; i < BENCH_SAMPLE_COUNT; ++i) {
    size_t process;
    if (measured.samples[i].iterations != result->batch_iterations ||
        measured.samples[i].process_index == 0u ||
        measured.samples[i].process_index > BENCH_PROCESS_COUNT)
      return 0;
    process = (size_t)measured.samples[i].process_index - 1u;
    if (process != i / BENCH_PROCESS_SAMPLE_COUNT)
      return 0;
    if (processes[process] != 0u &&
        work[process] != measured.samples[i].iterations)
      return 0;
    ++processes[process];
    work[process] = measured.samples[i].iterations;
    if (measured.samples[i].mismatch_count > (lonejson_uint64)-1 - mismatches)
      return 0;
    mismatches += measured.samples[i].mismatch_count;
  }
  for (i = 0u; i < BENCH_PROCESS_COUNT; ++i)
    if (processes[i] != BENCH_PROCESS_SAMPLE_COUNT)
      return 0;
  if (mismatches != result->mismatch_count ||
      result->elapsed_ns != bench_measure_elapsed_ns(&measured) ||
      fabs(measured.cv_pct - result->cv_pct) > 0.000001 ||
      fabs(measured.drift_pct - result->drift_pct) > 0.000001 ||
      fabs(measured.relative_error_pct - result->relative_error_pct) >
          0.000001 ||
      fabs(measured.drift_ci_low_pct - result->drift_ci_low_pct) > 0.000001 ||
      fabs(measured.drift_ci_high_pct - result->drift_ci_high_pct) > 0.000001)
    return 0;
  expected = (double)result->total_bytes * 1000000000.0 /
             ((double)result->elapsed_ns * 1048576.0);
  if (fabs(expected - result->mib_per_sec) > expected * 0.000001)
    return 0;
  expected = (double)result->total_documents * 1000000000.0 /
             (double)result->elapsed_ns;
  if (fabs(expected - result->docs_per_sec) > expected * 0.000001)
    return 0;
  {
    bench_result recomputed = *result;
    bench_fill_rates(&recomputed);
    if (!(result->mib_per_sec_low > 0.0) ||
        !(result->mib_per_sec_high >= result->mib_per_sec_low) ||
        !(result->relative_error_pct >= 0.0) ||
        fabs(recomputed.mib_per_sec_low - result->mib_per_sec_low) >
            recomputed.mib_per_sec_low * 0.000001 ||
        fabs(recomputed.mib_per_sec_high - result->mib_per_sec_high) >
            recomputed.mib_per_sec_high * 0.000001)
      return 0;
  }
  expected = (double)result->elapsed_ns / (double)result->total_bytes;
  return fabs(expected - result->ns_per_byte) <= expected * 0.000001;
}

static int bench_performance_tracked(const bench_result *result) {
  size_t len = strlen(result->name);
  return len < 6u || strcmp(result->name + len - 6u, "/cjson") != 0;
}

static size_t bench_tracked_count(const bench_run *run) {
  size_t i, count = 0u;
  for (i = 0u; i < run->results.count; ++i)
    if (bench_performance_tracked(&run->result_storage[i]))
      ++count;
  return count;
}

static int bench_result_precise(const bench_result *result) {
  return result->relative_error_pct <= BENCH_MAX_RELATIVE_ERROR_PCT &&
         result->drift_ci_low_pct <= BENCH_MAX_DRIFT_PCT &&
         result->drift_ci_high_pct >= -BENCH_MAX_DRIFT_PCT;
}

static size_t bench_count_imprecise_results(const bench_run *run) {
  size_t i;
  size_t count;
  count = 0u;
  for (i = 0u; i < run->results.count; ++i) {
    const bench_result *result;
    result = &run->result_storage[i];
    if (bench_performance_tracked(result) && !bench_result_precise(result)) {
      fprintf(stderr,
              "imprecise benchmark %s: relative error=%.2f%% (limit %.1f%%), "
              "cv=%.2f%%, drift=%.2f%%, drift CI=[%.2f%%,%.2f%%]\n",
              result->name, result->relative_error_pct,
              BENCH_MAX_RELATIVE_ERROR_PCT, result->cv_pct, result->drift_pct,
              result->drift_ci_low_pct, result->drift_ci_high_pct);
      ++count;
    }
  }
  return count;
}

static int bench_validate_loaded_run(const bench_run *run) {
  size_t i;
  size_t j;

  if (run == NULL) {
    return 0;
  }
  if (strcmp(run->method, BENCH_MEASUREMENT_METHOD) != 0 ||
      run->sample_count != BENCH_SAMPLE_COUNT ||
      run->process_count != BENCH_PROCESS_COUNT ||
      run->samples_per_process != BENCH_PROCESS_SAMPLE_COUNT ||
      run->warmup_count != BENCH_WARMUP_COUNT ||
      run->target_sample_ns != BENCH_TARGET_SAMPLE_NS ||
      run->confidence_pct != BENCH_CONFIDENCE_PCT ||
      run->max_relative_error_pct != BENCH_MAX_RELATIVE_ERROR_PCT ||
      run->max_drift_pct != BENCH_MAX_DRIFT_PCT ||
      run->source_revision[0] == '\0' || run->source_src_tree[0] == '\0' ||
      run->source_include_tree[0] == '\0' || run->source_lua_tree[0] == '\0' ||
      (run->implementation_count != 1u && run->implementation_count != 2u))
    return 0;
  if (run->results.count == 0u) {
    return 0;
  }
  for (i = 0; i < run->results.count; ++i) {
    if (run->result_storage[i].name[0] == '\0' ||
        run->result_storage[i].group[0] == '\0' ||
        !bench_result_measurement_valid(&run->result_storage[i])) {
      return 0;
    }
    for (j = i + 1u; j < run->results.count; ++j) {
      if (strcmp(run->result_storage[i].name, run->result_storage[j].name) ==
          0) {
        return 0;
      }
    }
  }
  return 1;
}

static size_t bench_count_result_mismatches(const bench_run *run) {
  size_t i;
  size_t mismatch_count;

  if (run == NULL) {
    return 0u;
  }
  mismatch_count = 0u;
  for (i = 0u; i < run->results.count; ++i) {
    if (run->result_storage[i].mismatch_count != 0u) {
      ++mismatch_count;
    }
  }
  return mismatch_count;
}

static const char *bench_delta_classification(double delta_pct) {
  if (delta_pct <= -BENCH_MATERIAL_DELTA_PCT) {
    return "material-reg";
  }
  if (delta_pct <= -BENCH_NOISE_DELTA_PCT) {
    return "small-reg";
  }
  if (delta_pct >= BENCH_REVIEW_IMPROVEMENT_PCT) {
    return "review-imp";
  }
  if (delta_pct >= BENCH_MATERIAL_DELTA_PCT) {
    return "material-imp";
  }
  if (delta_pct >= BENCH_NOISE_DELTA_PCT) {
    return "small-imp";
  }
  return "noise";
}

static void bench_print_compare_result(const bench_result *baseline,
                                       const bench_result *latest) {
  double mib_delta_pct;
  double docs_delta_pct;
  const char *delta_class;

  mib_delta_pct = 0.0;
  docs_delta_pct = 0.0;
  if (baseline->mib_per_sec != 0.0) {
    mib_delta_pct = ((latest->mib_per_sec - baseline->mib_per_sec) /
                     baseline->mib_per_sec) *
                    100.0;
  }
  if (baseline->docs_per_sec != 0.0) {
    docs_delta_pct = ((latest->docs_per_sec - baseline->docs_per_sec) /
                      baseline->docs_per_sec) *
                     100.0;
  }
  if (baseline->mismatch_count != 0u || latest->mismatch_count != 0u) {
    delta_class = "broken";
  } else if (!bench_result_precise(baseline) || !bench_result_precise(latest)) {
    delta_class = "imprecise";
  } else {
    delta_class = bench_delta_classification(mib_delta_pct);
  }
  printf("%-40s %11.3f %11.3f %9.2f%% %-12s %10.2f%% %10.0f\n", latest->name,
         latest->mib_per_sec, baseline->mib_per_sec, mib_delta_pct, delta_class,
         docs_delta_pct, (double)latest->mismatch_count);
}

static int bench_is_small_regression(const bench_result *baseline,
                                     const bench_result *latest) {
  double mib_delta_pct;

  if (baseline->mib_per_sec <= 0.0 ||
      latest->mib_per_sec >= baseline->mib_per_sec) {
    return 0;
  }
  mib_delta_pct =
      ((latest->mib_per_sec - baseline->mib_per_sec) / baseline->mib_per_sec) *
      100.0;
  return strcmp(bench_delta_classification(mib_delta_pct), "small-reg") == 0;
}

static int bench_is_material_regression(const bench_result *baseline,
                                        const bench_result *latest) {
  double mib_delta_pct;

  if (baseline->mib_per_sec <= 0.0 ||
      latest->mib_per_sec >= baseline->mib_per_sec) {
    return 0;
  }
  mib_delta_pct =
      ((latest->mib_per_sec - baseline->mib_per_sec) / baseline->mib_per_sec) *
      100.0;
  return strcmp(bench_delta_classification(mib_delta_pct), "material-reg") == 0;
}

static int bench_is_large_improvement(const bench_result *baseline,
                                      const bench_result *latest) {
  double mib_delta_pct;

  if (baseline->mib_per_sec <= 0.0 ||
      latest->mib_per_sec <= baseline->mib_per_sec) {
    return 0;
  }
  mib_delta_pct =
      ((latest->mib_per_sec - baseline->mib_per_sec) / baseline->mib_per_sec) *
      100.0;
  return strcmp(bench_delta_classification(mib_delta_pct), "review-imp") == 0;
}

static int bench_environment_matches(const bench_run *baseline,
                                     const bench_run *latest) {
  int matches;

  matches = 1;
  if (strcmp(baseline->affinity, latest->affinity) != 0) {
    fprintf(stderr, "benchmark CPU affinity mismatch: baseline=%s latest=%s\n",
            baseline->affinity, latest->affinity);
    matches = 0;
  }
  if (strcmp(baseline->host, latest->host) != 0) {
    fprintf(stderr, "benchmark host mismatch: baseline=%s latest=%s\n",
            baseline->host, latest->host);
    matches = 0;
  }
  if (strcmp(baseline->compiler, latest->compiler) != 0) {
    fprintf(stderr, "benchmark compiler mismatch: baseline=%s latest=%s\n",
            baseline->compiler, latest->compiler);
    matches = 0;
  }
  if (strcmp(baseline->toolchain, latest->toolchain) != 0) {
    fprintf(stderr, "benchmark toolchain mismatch: baseline=%s latest=%s\n",
            baseline->toolchain, latest->toolchain);
    matches = 0;
  }
  if (!matches) {
    fputs("benchmark runs are not comparable; measure the reference and "
          "candidate on the same host with the same toolchain\n",
          stderr);
  }
  return matches;
}

static int bench_configuration_matches(const bench_run *baseline,
                                       const bench_run *latest) {
  if (baseline->implementation_count != latest->implementation_count) {
    fputs("benchmark implementation-count mismatch\n", stderr);
    return 0;
  }
  if (strcmp(baseline->lua_version, latest->lua_version) != 0 ||
      baseline->schema_version != latest->schema_version ||
      baseline->iterations != latest->iterations ||
      baseline->parser_buffer_size != latest->parser_buffer_size ||
      baseline->push_parser_buffer_size != latest->push_parser_buffer_size ||
      baseline->reader_buffer_size != latest->reader_buffer_size ||
      baseline->stream_buffer_size != latest->stream_buffer_size ||
      baseline->corpus_file_count != latest->corpus_file_count ||
      baseline->corpus_total_bytes != latest->corpus_total_bytes ||
      baseline->corpus_y_count != latest->corpus_y_count ||
      baseline->corpus_n_count != latest->corpus_n_count ||
      baseline->corpus_i_count != latest->corpus_i_count) {
    fputs("benchmark configuration mismatch: measure the reference and "
          "candidate with identical inputs\n",
          stderr);
    return 0;
  }
  {
    size_t i;
    for (i = 0; i < latest->results.count; ++i) {
      const bench_result *old =
          bench_find_result(baseline, latest->result_storage[i].name);
      if (old != NULL &&
          old->target_sample_ns != latest->result_storage[i].target_sample_ns) {
        fputs("benchmark case budget mismatch\n", stderr);
        return 0;
      }
    }
  }
  return 1;
}

static int bench_compare_command(const char *baseline_path,
                                 const char *latest_path) {
  bench_run baseline = {0};
  bench_run latest = {0};
  size_t i;
  size_t missing_count;

  if (bench_read_run(baseline_path, &baseline) != 0 ||
      bench_read_run(latest_path, &latest) != 0) {
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }
  if (!bench_validate_loaded_run(&baseline) ||
      !bench_validate_loaded_run(&latest)) {
    fprintf(stderr,
            "benchmark compare failed: loaded run data is incomplete or "
            "corrupted; rerun 'make bench' and 'make bench-freeze-baseline'\n");
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }

  if (!bench_environment_matches(&baseline, &latest) ||
      !bench_configuration_matches(&baseline, &latest)) {
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }
  printf("compare baseline=%s latest=%s\n", baseline_path, latest_path);
  printf("%-40s %11s %11s %10s %-12s %11s %10s\n", "benchmark", "latest MiB/s",
         "base MiB/s", "delta", "class", "docs delta", "mismatch");
  printf("%-40s %11s %11s %10s %-12s %11s %10s\n",
         "----------------------------------------", "-----------",
         "-----------", "----------", "------------", "-----------",
         "----------");
  missing_count = 0u;
  for (i = 0; i < latest.results.count; ++i) {
    const bench_result *base;
    base = bench_find_result(&baseline, latest.result_storage[i].name);
    if (base == NULL && !bench_performance_tracked(&latest.result_storage[i]))
      continue;
    if (base == NULL) {
      printf("%-40s %11s %11s %10s %-12s %11s %10s\n",
             latest.result_storage[i].name, "missing", "missing", "missing",
             "missing", "missing", "missing");
      ++missing_count;
      continue;
    }
    bench_print_compare_result(base, &latest.result_storage[i]);
  }
  if (missing_count != 0u) {
    printf(
        "warning: baseline is missing %lu benchmark results; run "
        "'make bench-freeze-baseline' to freeze the current full result set\n",
        (unsigned long)missing_count);
  }
  bench_run_cleanup(&baseline);
  bench_run_cleanup(&latest);
  return 0;
}

static int bench_gate_command(const char *baseline_path,
                              const char *latest_path) {
  bench_run baseline = {0};
  bench_run latest = {0};
  size_t i;
  size_t missing_count;
  size_t small_regression_count;
  size_t material_regression_count;
  size_t large_improvement_count;
  size_t uncertain_comparison_count;
  size_t mismatch_count;
  size_t imprecise_count;
  int schema_mismatch;
  int config_mismatch;

  if (bench_read_run(baseline_path, &baseline) != 0 ||
      bench_read_run(latest_path, &latest) != 0) {
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }
  if (!bench_validate_loaded_run(&baseline) ||
      !bench_validate_loaded_run(&latest)) {
    fprintf(stderr,
            "benchmark gate failed: loaded run data is incomplete or "
            "corrupted; rerun 'make bench' and 'make bench-freeze-baseline'\n");
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }

  if (!bench_environment_matches(&baseline, &latest) ||
      !bench_configuration_matches(&baseline, &latest)) {
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }
  schema_mismatch = baseline.schema_version != latest.schema_version;
  config_mismatch =
      baseline.parser_buffer_size != latest.parser_buffer_size ||
      baseline.push_parser_buffer_size != latest.push_parser_buffer_size ||
      baseline.reader_buffer_size != latest.reader_buffer_size ||
      baseline.stream_buffer_size != latest.stream_buffer_size ||
      baseline.iterations != latest.iterations;
  missing_count = 0u;
  small_regression_count = 0u;
  material_regression_count = 0u;
  large_improvement_count = 0u;
  uncertain_comparison_count = 0u;
  mismatch_count = bench_count_result_mismatches(&latest) +
                   bench_count_result_mismatches(&baseline);
  imprecise_count = bench_count_imprecise_results(&latest) +
                    bench_count_imprecise_results(&baseline);
  if (bench_tracked_count(&baseline) != bench_tracked_count(&latest))
    ++missing_count;
  for (i = 0; i < latest.results.count; ++i) {
    const bench_result *base;

    if (!bench_performance_tracked(&latest.result_storage[i]))
      continue;

    base = bench_find_result(&baseline, latest.result_storage[i].name);
    if (base == NULL) {
      ++missing_count;
      continue;
    }
    if (!bench_result_precise(base) ||
        !bench_result_precise(&latest.result_storage[i]) ||
        base->mismatch_count != 0u ||
        latest.result_storage[i].mismatch_count != 0u)
      continue;
    if (latest.result_storage[i].mib_per_sec_low / base->mib_per_sec_high <
        1.0 - BENCH_MATERIAL_DELTA_PCT / 100.0)
      ++uncertain_comparison_count;
    if (bench_is_small_regression(base, &latest.result_storage[i])) {
      ++small_regression_count;
      continue;
    }
    if (bench_is_material_regression(base, &latest.result_storage[i])) {
      ++material_regression_count;
      continue;
    }
    if (bench_is_large_improvement(base, &latest.result_storage[i])) {
      ++large_improvement_count;
    }
  }

  printf("gate baseline=%s latest=%s\n", baseline_path, latest_path);
  printf("  schema mismatch: %s\n", schema_mismatch ? "yes" : "no");
  printf("  config mismatch: %s\n", config_mismatch ? "yes" : "no");
  printf("  missing results: %lu\n", (unsigned long)missing_count);
  printf("  result mismatches: %lu\n", (unsigned long)mismatch_count);
  printf("  imprecise results: %lu\n", (unsigned long)imprecise_count);
  printf("  uncertain comparisons: %lu\n",
         (unsigned long)uncertain_comparison_count);
  printf("  small regressions: %lu\n", (unsigned long)small_regression_count);
  printf("  material regressions: %lu\n",
         (unsigned long)material_regression_count);
  printf("  large improvements to review: %lu\n",
         (unsigned long)large_improvement_count);

  if (schema_mismatch || config_mismatch || missing_count != 0u ||
      mismatch_count != 0u || material_regression_count != 0u ||
      imprecise_count != 0u || uncertain_comparison_count != 0u) {
    if (schema_mismatch || config_mismatch || missing_count != 0u) {
      fprintf(stderr,
              "benchmark gate failed: fix the regressions, and refresh the "
              "baseline only if the benchmark schema/config/result set "
              "intentionally changed\n");
    } else if (mismatch_count != 0u) {
      fprintf(stderr,
              "benchmark gate failed: benchmark case mismatches must be zero "
              "before landing\n");
    } else if (imprecise_count != 0u || (uncertain_comparison_count != 0u &&
                                         material_regression_count == 0u)) {
      fputs("benchmark gate inconclusive: confidence bounds do not establish "
            "non-regression; inspect the retained samples\n",
            stderr);
      bench_run_cleanup(&baseline);
      bench_run_cleanup(&latest);
      return 2;
    } else {
      fprintf(stderr,
              "benchmark gate failed: fix the regressions before landing\n");
    }
    bench_run_cleanup(&baseline);
    bench_run_cleanup(&latest);
    return 1;
  }
  bench_run_cleanup(&baseline);
  bench_run_cleanup(&latest);
  return 0;
}

static int bench_merge_command(int count, char **paths, const char *latest,
                               const char *history, const char *archive) {
  bench_run combined, worker;
  size_t i, j;
  int process;
  memset(&combined, 0, sizeof(combined));
  memset(&worker, 0, sizeof(worker));
  if (count != BENCH_PROCESS_COUNT) {
    fputs("merge requires every confirmation round\n", stderr);
    return 1;
  }
  for (process = 0; process < count; ++process) {
    if (bench_read_run(paths[process], &worker) != 0)
      goto fail;
    if (worker.sample_count != BENCH_SAMPLE_COUNT ||
        worker.process_count != BENCH_PROCESS_COUNT ||
        worker.warmup_count != BENCH_WARMUP_COUNT ||
        worker.target_sample_ns != BENCH_TARGET_SAMPLE_NS ||
        worker.confidence_pct != BENCH_CONFIDENCE_PCT ||
        worker.max_relative_error_pct != BENCH_MAX_RELATIVE_ERROR_PCT ||
        worker.max_drift_pct != BENCH_MAX_DRIFT_PCT ||
        worker.samples_per_process != 1u ||
        strcmp(worker.method, BENCH_MEASUREMENT_METHOD) != 0 ||
        worker.results.count == 0u || bench_count_result_mismatches(&worker))
      goto fail;
    if (process == 0) {
      combined = worker;
      combined.results.items = combined.result_storage;
      memset(&worker, 0, sizeof(worker));
      for (i = 0; i < combined.results.count; ++i) {
        bench_result *r = &combined.result_storage[i];
        bench_sample first;
        if (r->samples.count != 1u)
          goto fail;
        first = ((bench_sample *)r->samples.items)[0];
        /* Parsed storage belongs to lonejson; new aggregate storage is ours. */
        {
          bench_result storage = {0};
          storage.samples = r->samples;
          lonejson_cleanup(&bench_result_map, &storage, NULL);
        }
        r->samples.items = calloc(BENCH_SAMPLE_COUNT, sizeof(bench_sample));
        if (r->samples.items == NULL)
          goto fail;
        r->samples.capacity = BENCH_SAMPLE_COUNT;
        r->samples.count = 1u;
        r->samples.elem_size = sizeof(bench_sample);
        r->samples.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
        ((bench_sample *)r->samples.items)[0] = first;
        ((bench_sample *)r->samples.items)[0].process_index = 1u;
      }
      continue;
    }
    if (worker.results.count != combined.results.count ||
        !bench_configuration_matches(&combined, &worker) ||
        strcmp(worker.lua_version, combined.lua_version) ||
        worker.schema_version != combined.schema_version ||
        strcmp(worker.host, combined.host) ||
        strcmp(worker.compiler, combined.compiler) ||
        strcmp(worker.toolchain, combined.toolchain) ||
        strcmp(worker.affinity, combined.affinity) ||
        strcmp(worker.source_revision, combined.source_revision) ||
        strcmp(worker.source_src_tree, combined.source_src_tree) ||
        strcmp(worker.source_include_tree, combined.source_include_tree) ||
        strcmp(worker.source_lua_tree, combined.source_lua_tree) ||
        worker.implementation_count != combined.implementation_count)
      goto fail;
    for (i = 0; i < combined.results.count; ++i) {
      bench_result *r = &combined.result_storage[i];
      const bench_result *w = bench_find_result(&worker, r->name);
      bench_sample sample;
      if (w == NULL || strcmp(w->group, r->group) || w->samples.count != 1u ||
          w->target_sample_ns != r->target_sample_ns ||
          w->batch_iterations != r->batch_iterations ||
          w->total_bytes != r->total_bytes ||
          w->total_documents != r->total_documents || w->mismatch_count != 0u)
        goto fail;
      sample = ((bench_sample *)w->samples.items)[0];
      if (sample.iterations != r->batch_iterations || sample.elapsed_ns == 0u ||
          sample.cpu_ns == 0u || sample.mismatch_count != 0u)
        goto fail;
      sample.process_index = (lonejson_uint64)process + 1u;
      ((bench_sample *)r->samples.items)[process] = sample;
      ++r->samples.count;
      r->warmup_elapsed_ns += w->warmup_elapsed_ns;
    }
    bench_run_cleanup(&worker);
    memset(&worker, 0, sizeof(worker));
  }
  for (i = 0; i < combined.results.count; ++i) {
    bench_result *r = &combined.result_storage[i];
    bench_measurement m;
    memset(&m, 0, sizeof(m));
    m.batch_iterations = (unsigned long)r->batch_iterations;
    m.sample_count = BENCH_SAMPLE_COUNT;
    for (j = 0; j < m.sample_count; ++j)
      m.samples[j] = ((bench_sample *)r->samples.items)[j];
    if (!bench_measure_summarize(&m))
      goto fail;
    r->elapsed_ns = bench_measure_elapsed_ns(&m);
    r->cv_pct = m.cv_pct;
    r->drift_pct = m.drift_pct;
    bench_fill_rates(r);
  }
  if (!bench_validate_loaded_run(&combined))
    goto fail;
  process = bench_write_outputs(&combined, latest, history, archive);
  bench_print_run_report(&combined);
  bench_run_cleanup(&combined);
  return process;
fail:
  fputs("invalid or incomplete whole-suite rounds\n", stderr);
  bench_run_cleanup(&worker);
  bench_run_cleanup(&combined);
  return 1;
}

static void bench_usage(const char *program) {
  fprintf(stderr,
          "usage: %s round <corpus> <output> <pilot-or-dash> [case]\n"
          "       %s merge <latest> <history> <archive> <rounds...>\n"
          "       %s compare|gate <baseline> <latest>\n"
          "       %s freeze-baseline <history> <baseline>\n",
          program, program, program, program);
}

int main(int argc, char **argv) {
  int rc = 1;
  bench_run plan;
  memset(&plan, 0, sizeof(plan));
  if (argc < 2) {
    bench_usage(argv[0]);
    return 1;
  }
  if (bench_init_runtimes() != 0)
    return 1;
  if (strcmp(argv[1], "round") == 0 && (argc == 5 || argc == 6)) {
    g_bench_corpus = argv[2];
    g_bench_filter = argc == 6 ? argv[5] : NULL;
    if (strcmp(argv[4], "-") != 0) {
      if (bench_read_run(argv[4], &plan))
        goto done;
      g_bench_plan = &plan;
    }
    rc = bench_run_command(argv[2], argv[3], 1u);
  } else if (strcmp(argv[1], "merge") == 0 && argc >= 6) {
    rc = bench_merge_command(argc - 5, argv + 5, argv[2], argv[3], argv[4]);
  } else if (strcmp(argv[1], "freeze-baseline") == 0 && argc == 4) {
    rc = bench_freeze_baseline_command(argv[2], argv[3]);
  } else if (strcmp(argv[1], "compare") == 0 && argc == 4) {
    rc = bench_compare_command(argv[2], argv[3]);
  } else if (strcmp(argv[1], "gate") == 0 && argc == 4) {
    rc = bench_gate_command(argv[2], argv[3]);
  } else
    bench_usage(argv[0]);
done:
  if (g_bench_plan != NULL)
    bench_run_cleanup(&plan);
  bench_cleanup_runtimes();
  return rc;
}
