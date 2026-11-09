/*
 * MIT License
 *
 * lonejson is Copyright (c) 2026 Michel Blomgren <mike@c89.systems>
 * https://c89.systems
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LONEJSON_H
#define LONEJSON_H

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#if defined(LJ_DISABLE_SHORT_NAMES) && !defined(LONEJSON_DISABLE_SHORT_NAMES)
#define LONEJSON_DISABLE_SHORT_NAMES 1
#endif

#if defined(LJ_IMPLEMENTATION) && !defined(LONEJSON_IMPLEMENTATION)
#define LONEJSON_IMPLEMENTATION
#endif
#if defined(LJ_WITH_CURL) && !defined(LONEJSON_WITH_CURL)
#define LONEJSON_WITH_CURL
#endif
#if defined(LJ_WITH_OPENSSL) && !defined(LONEJSON_WITH_OPENSSL)
#define LONEJSON_WITH_OPENSSL
#endif
#if defined(LJ_WITH_JWT) && !defined(LONEJSON_WITH_JWT)
#define LONEJSON_WITH_JWT
#endif
#if defined(LJ_WITH_OIDC) && !defined(LONEJSON_WITH_OIDC)
#define LONEJSON_WITH_OIDC
#endif
#if defined(LONEJSON_WITH_OIDC) && !defined(LONEJSON_WITH_JWT)
#error "LONEJSON_WITH_OIDC requires LONEJSON_WITH_JWT"
#endif
#if defined(LONEJSON_WITH_OPENSSL)
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <stdlib.h>
#include <string.h>
#endif
#if defined(LJ_MALLOC) && !defined(LONEJSON_MALLOC)
#define LONEJSON_MALLOC LJ_MALLOC
#endif
#if defined(LJ_CALLOC) && !defined(LONEJSON_CALLOC)
#define LONEJSON_CALLOC LJ_CALLOC
#endif
#if defined(LJ_REALLOC) && !defined(LONEJSON_REALLOC)
#define LONEJSON_REALLOC LJ_REALLOC
#endif
#if defined(LJ_FREE) && !defined(LONEJSON_FREE)
#define LONEJSON_FREE LJ_FREE
#endif
#if defined(LJ_PARSER_BUFFER_SIZE) && !defined(LONEJSON_PARSER_BUFFER_SIZE)
#define LONEJSON_PARSER_BUFFER_SIZE LJ_PARSER_BUFFER_SIZE
#endif
#if defined(LJ_READER_BUFFER_SIZE) && !defined(LONEJSON_READER_BUFFER_SIZE)
#define LONEJSON_READER_BUFFER_SIZE LJ_READER_BUFFER_SIZE
#endif
#if defined(LJ_PUSH_PARSER_BUFFER_SIZE) &&                                     \
    !defined(LONEJSON_PUSH_PARSER_BUFFER_SIZE)
#define LONEJSON_PUSH_PARSER_BUFFER_SIZE LJ_PUSH_PARSER_BUFFER_SIZE
#endif
#if defined(LJ_STREAM_BUFFER_SIZE) && !defined(LONEJSON_STREAM_BUFFER_SIZE)
#define LONEJSON_STREAM_BUFFER_SIZE LJ_STREAM_BUFFER_SIZE
#endif
#if defined(LJ_SPOOL_MEMORY_LIMIT) && !defined(LONEJSON_SPOOL_MEMORY_LIMIT)
#define LONEJSON_SPOOL_MEMORY_LIMIT LJ_SPOOL_MEMORY_LIMIT
#endif
#if defined(LJ_WRITE_MAX_OUTPUT_BYTES) &&                                      \
    !defined(LONEJSON_WRITE_MAX_OUTPUT_BYTES)
#define LONEJSON_WRITE_MAX_OUTPUT_BYTES LJ_WRITE_MAX_OUTPUT_BYTES
#endif
#if defined(LJ_SPOOL_TEMP_PATH_CAPACITY) &&                                    \
    !defined(LONEJSON_SPOOL_TEMP_PATH_CAPACITY)
#define LONEJSON_SPOOL_TEMP_PATH_CAPACITY LJ_SPOOL_TEMP_PATH_CAPACITY
#endif
#if defined(LJ_TRACK_WORKSPACE_USAGE) &&                                       \
    !defined(LONEJSON_TRACK_WORKSPACE_USAGE)
#define LONEJSON_TRACK_WORKSPACE_USAGE LJ_TRACK_WORKSPACE_USAGE
#endif

#if !defined(__cplusplus)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#elif !defined(bool)
typedef int bool;
#define bool bool
#define true 1
#define false 0
#endif
#endif

#if defined(_MSC_VER)
/** Unsigned 32-bit integer type used by lonejson public structs and APIs. */
typedef unsigned __int32 lonejson_uint32;
/** Signed 64-bit integer type used by lonejson public structs and APIs. */
typedef __int64 lonejson_int64;
/** Unsigned 64-bit integer type used by lonejson public structs and APIs. */
typedef unsigned __int64 lonejson_uint64;
#elif defined(__GNUC__) || defined(__clang__)
/** Unsigned 32-bit integer type used by lonejson public structs and APIs. */
__extension__ typedef unsigned long lonejson_uint32;
/** Signed 64-bit integer type used by lonejson public structs and APIs. */
__extension__ typedef signed long long lonejson_int64;
/** Unsigned 64-bit integer type used by lonejson public structs and APIs. */
__extension__ typedef unsigned long long lonejson_uint64;
#else
/** Unsigned 32-bit integer type used by lonejson public structs and APIs. */
typedef unsigned long lonejson_uint32;
/** Signed 64-bit integer type used by lonejson public structs and APIs. */
typedef signed long long lonejson_int64;
/** Unsigned 64-bit integer type used by lonejson public structs and APIs. */
typedef unsigned long long lonejson_uint64;
#endif

#if !defined(SIZE_MAX)
#define SIZE_MAX ((size_t)-1)
#endif

#if !defined(LONEJSON_UINT64_MAX)
#define LONEJSON_UINT64_MAX ((lonejson_uint64) ~(lonejson_uint64)0)
#endif

#if defined(_MSC_VER)
#define LONEJSON_SHORT_ALIAS_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define LONEJSON_SHORT_ALIAS_INLINE                                            \
  static __inline__ __attribute__((unused, always_inline))
#else
#define LONEJSON_SHORT_ALIAS_INLINE static
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file lonejson.h
 *
 * lonejson is a schema-guided JSON library for C. In the source tree and in
 * the binary release archives, this header is the public declarations-only
 * interface used with the compiled `liblonejson` library. The release process
 * also produces a separate standalone single-header artifact for projects that
 * prefer to vendor one file directly. lonejson is meant for programs that
 * already know the shape of the data they care about and want to move between
 * JSON objects and C structs without dragging a generic DOM through the middle
 * of the program. The library can parse from memory, files, file descriptors,
 * and object-framed streams; it can serialize to buffers, sinks, files, and
 * JSON Lines; and it can keep very large field values out of memory by
 * spilling inbound values to temporary files or by serializing outbound values
 * directly from paths, `FILE *`, or file descriptors.
 *
 * The usual lonejson workflow is simple. You define a C struct, describe its
 * JSON-visible fields with `LONEJSON_FIELD_*` macros, bind those fields into a
 * `lonejson_map`, and then parse or serialize through that map. The design is
 * deliberately schema-guided: lonejson is at its best when you want stable,
 * typed decoding into known structures, predictable fixed-capacity behavior,
 * and efficient object-framed streaming instead of a line-delimited protocol.
 *
 * The project home, release artifacts, and full example programs are at:
 *
 *   https://github.com/c89systems/lonejson
 *   https://github.com/c89systems/lonejson/tree/trunk/examples
 *
 * In the normal linked-library build, include this header and link
 * `liblonejson`:
 *
 * ```c
 * #include "lonejson.h"
 * ```
 *
 * If you are using the generated standalone single-header release artifact,
 * define `LONEJSON_IMPLEMENTATION` in exactly one translation unit before
 * including that generated header:
 *
 * ```c
 * #define LONEJSON_IMPLEMENTATION
 * #include "lonejson.h"
 * ```
 *
 * Short aliases are enabled by default. The long names remain canonical, but
 * the library also exposes `lj_*` and `LJ_*` aliases for projects that prefer
 * a shorter prefix. If those names collide with another dependency, disable
 * them before the include:
 *
 * ```c
 * #define LONEJSON_DISABLE_SHORT_NAMES 1
 * #include "lonejson.h"
 * ```
 *
 * The public API falls into a few coherent groups. Mapping is done with
 * `LONEJSON_FIELD_*` and `LONEJSON_MAP_DEFINE(...)`. One-shot parsing is
 * handled by `lonejson_parse_cstr`, `lonejson_parse_buffer`,
 * `lonejson_parse_reader`, `lonejson_parse_filep`, and `lonejson_parse_path`,
 * with `lonejson_validate_*` variants available when you only need syntax
 * validation. Object-framed streams emit mapped top-level JSON objects only;
 * they do not enumerate arbitrary JSON values. They use
 * `lonejson_stream_open_*`, `lonejson_stream_next`, `lonejson_stream_error`,
 * and `lonejson_stream_close`. Selected-array streams use
 * `lonejson_array_stream_open_*`, `lonejson_array_stream_next`,
 * `lonejson_array_stream_next_value`, `lonejson_array_stream_error`, and
 * `lonejson_array_stream_close`. Serialization is exposed through
 * `lonejson_serialize_*` and `lonejson_serialize_jsonl_*`. Ownership and reuse
 * of mapped values is handled by `lonejson_cleanup` and `lonejson_reset`.
 * Large inbound values are represented by `lonejson_spooled`, large outbound
 * values by `lonejson_source`, and arbitrary embedded JSON values by
 * `lonejson_json_value`. One arbitrary JSON value can also be consumed without
 * a schema through `lonejson_value_visitor` and `lonejson_visit_value_*`.
 * Behavior is configured through one instantiated `lonejson` runtime and its
 * `lonejson_config`.
 *
 * The small examples below are meant as orientation, not as exhaustive
 * coverage. The repository examples are fuller programs with proper error
 * paths, build instructions, and end-to-end behavior.
 *
 * Parsing one JSON object into a struct looks like this:
 *
 * ```c
 * #include "lonejson.h"
 *
 * typedef struct user_doc {
 *   char *name;
 *   lonejson_int64 age;
 * } user_doc;
 *
 * static const lonejson_field user_doc_fields[] = {
 *   LONEJSON_FIELD_STRING_ALLOC_REQ(user_doc, name, "name"),
 *   LONEJSON_FIELD_I64(user_doc, age, "age")
 * };
 *
 * LONEJSON_MAP_DEFINE(user_doc_map, user_doc, user_doc_fields);
 *
 * user_doc doc;
 * lonejson_error error;
 * lonejson_status status;
 *
 * lonejson *runtime = lonejson_new(NULL, &error);
 * if (runtime == NULL) {
 *   return;
 * }
 *
 * status = lonejson_parse_cstr(
 *   runtime, &user_doc_map, &doc, "{\"name\":\"Ada\",\"age\":37}", &error);
 * if (status != LONEJSON_STATUS_OK) {
 *   lonejson_free(runtime);
 *   return;
 * }
 *
 * lonejson_cleanup(&user_doc_map, &doc, NULL);
 * lonejson_free(runtime);
 * ```
 *
 * Object-framed streaming is the right fit when the input consists of
 * consecutive JSON objects with optional whitespace between them. lonejson does
 * not treat newlines as framing; it treats completed objects as framing. It
 * does not iterate scalar or array roots; use `lonejson_visit_value_*` when
 * processing exactly one arbitrary JSON value without a schema.
 *
 * ```c
 * lonejson_stream *stream;
 * lonejson_stream_result result;
 * user_doc doc;
 * lonejson_error error;
 *
 * lonejson *runtime = lonejson_new(NULL, &error);
 * if (runtime == NULL) {
 *   return;
 * }
 *
 * stream = lonejson_stream_open_fd(runtime, &user_doc_map, fd, &error);
 * if (stream == NULL) {
 *   lonejson_free(runtime);
 *   return;
 * }
 *
 * for (;;) {
 *   result = lonejson_stream_next(stream, &doc, &error);
 *   if (result == LONEJSON_STREAM_OBJECT) {
 *     lonejson_cleanup(&user_doc_map, &doc, NULL);
 *     continue;
 *   }
 *   if (result == LONEJSON_STREAM_EOF) {
 *     break;
 *   }
 *   break;
 * }
 *
 * lonejson_stream_close(stream);
 * lonejson_free(runtime);
 * ```
 *
 * Selected-array streaming is the right fit when one valid JSON document
 * contains an array that should be consumed item by item. `""` selects a root
 * array. A non-empty v1 path selects one direct root object key such as
 * `"items"`; implicit fan-out paths such as `"boards.items"` are rejected.
 *
 * ```c
 * lonejson_array_stream *items;
 * lonejson_array_stream_result result;
 * user_doc doc;
 * lonejson_error error;
 *
 * lonejson *runtime = lonejson_new(NULL, &error);
 * if (runtime == NULL) {
 *   return;
 * }
 *
 * items = lonejson_array_stream_open_path(runtime, "items", "/tmp/users.json",
 *                                         &error);
 * if (items == NULL) {
 *   lonejson_free(runtime);
 *   return;
 * }
 *
 * for (;;) {
 *   result = lonejson_array_stream_next(items, &user_doc_map, &doc, &error);
 *   if (result == LONEJSON_ARRAY_STREAM_ITEM) {
 *     lonejson_cleanup(&user_doc_map, &doc, NULL);
 *     continue;
 *   }
 *   if (result == LONEJSON_ARRAY_STREAM_EOF) {
 *     break;
 *   }
 *   break;
 * }
 *
 * lonejson_array_stream_close(items);
 * lonejson_free(runtime);
 * ```
 *
 * Serializing one mapped object is symmetrical:
 *
 * ```c
 * char buffer[256];
 * size_t written = 0u;
 * lonejson_config config = lonejson_default_config();
 * lonejson *runtime;
 * lonejson_error error;
 *
 * config.write_pretty = 1;
 * runtime = lonejson_new(&config, &error);
 * if (runtime == NULL) {
 *   return;
 * }
 *
 * if (lonejson_serialize_buffer(
 *       runtime, &user_doc_map, &doc, buffer, sizeof(buffer), &written,
 *       &error) != LONEJSON_STATUS_OK) {
 *   lonejson_free(runtime);
 *   return;
 * }
 * lonejson_free(runtime);
 * ```
 *
 * JSON Lines output is available when you need one compact object per line:
 *
 * ```c
 * user_doc docs[2];
 * char out[256];
 * size_t written = 0u;
 * lonejson_error error;
 * lonejson *runtime = lonejson_new(NULL, &error);
 *
 * lonejson_serialize_jsonl_buffer(
 *   runtime, &user_doc_map, docs, 2u, 0u, out, sizeof(out), &written, &error);
 * lonejson_free(runtime);
 * ```
 *
 * Very large inbound values can spill to disk instead of growing without
 * bound in memory:
 *
 * ```c
 * typedef struct blob_doc {
 *   lonejson_spooled body;
 * } blob_doc;
 *
 * lonejson_config config = lonejson_default_config();
 * lonejson *runtime;
 *
 * config.spool_blob.memory_limit = 64u * 1024u;
 * config.spool_blob.max_bytes = 32u * 1024u * 1024u;
 * config.spool_blob.temp_dir = "/tmp";
 * runtime = lonejson_new(&config, &error);
 * if (runtime == NULL) {
 *   return;
 * }
 *
 * static const lonejson_field blob_doc_fields[] = {
 *   LONEJSON_FIELD_STRING_STREAM_CLASS(blob_doc, body, "body",
 *                                     LONEJSON_SPOOL_CLASS_BLOB)
 * };
 *
 * LONEJSON_MAP_DEFINE(blob_doc_map, blob_doc, blob_doc_fields);
 *
 * blob_doc doc;
 *
 * lonejson_parse_path(runtime, &blob_doc_map, &doc, "input.json", &error);
 * lonejson_spooled_write_to_sink(&doc.body, sink_fn, sink_user);
 * lonejson_cleanup(&blob_doc_map, &doc, NULL);
 * lonejson_free(runtime);
 * ```
 *
 * Outbound source-backed fields cover the opposite direction. They let you
 * serialize a JSON string or base64 field directly from a path, `FILE *`, or
 * file descriptor without first building a spool:
 *
 * ```c
 * typedef struct outbound_doc {
 *   lonejson_source payload;
 * } outbound_doc;
 *
 * static const lonejson_field outbound_doc_fields[] = {
 *   LONEJSON_FIELD_BASE64_SOURCE_REQ(outbound_doc, payload, "payload")
 * };
 *
 * LONEJSON_MAP_DEFINE(outbound_doc_map, outbound_doc, outbound_doc_fields);
 *
 * outbound_doc doc;
 * lonejson_error error;
 * lonejson *runtime = lonejson_new(NULL, &error);
 * lonejson_source_set_path(&doc.payload, "payload.bin");
 * lonejson_serialize_path(runtime, &outbound_doc_map, &doc, "request.json",
 *                         &error);
 * lonejson_cleanup(&outbound_doc_map, &doc, NULL);
 * lonejson_free(runtime);
 * ```
 *
 * Embedded arbitrary JSON values are stream-first on parse. The caller
 * configures whether the nested value should be streamed to a raw sink,
 * streamed as structured visitor callbacks, or explicitly captured for later
 * reuse:
 *
 * ```c
 * typedef struct query_doc {
 *   char namespace_[16];
 *   lonejson_json_value selector;
 * } query_doc;
 *
 * static const lonejson_field query_doc_fields[] = {
 *   LONEJSON_FIELD_STRING_FIXED_REQ(query_doc, namespace_, "namespace",
 *                                   LONEJSON_OVERFLOW_FAIL),
 *   LONEJSON_FIELD_JSON_VALUE_REQ(query_doc, selector, "selector")
 * };
 *
 * LONEJSON_MAP_DEFINE(query_doc_map, query_doc, query_doc_fields);
 *
 * query_doc doc;
 * lonejson_config config = lonejson_default_config();
 * lonejson *runtime;
 *
 * config.clear_destination_by_default = 0;
 * lj = lonejson_new(&config, &error);
 * lonejson_init(lj, &query_doc_map, &doc, NULL);
 * lonejson_json_value_enable_parse_capture(&doc.selector, &error);
 * lonejson_parse_cstr(lj, &query_doc_map, &doc,
 *                     "{\"namespace\":\"ops\",\"selector\":{\"op\":\"and\"}}",
 *                     &error);
 * lonejson_json_value_write_to_sink(&doc.selector, sink_fn, sink_user, &error);
 * lonejson_cleanup(&query_doc_map, &doc, NULL);
 * lonejson_free(lj);
 * ```
 *
 * The lower-level visitor API is useful when you need to parse exactly one
 * JSON value without a schema and without retaining it:
 *
 * ```c
 * lonejson_value_visitor visitor = lonejson_default_value_visitor();
 * lonejson_config config = lonejson_default_config();
 * lonejson *runtime;
 *
 * visitor.object_begin = on_object_begin;
 * visitor.object_key_chunk = on_object_key_chunk;
 * visitor.string_chunk = on_string_chunk;
 * visitor.number_chunk = on_number_chunk;
 * config.json_value_max_total_bytes = 512u * 1024u;
 * lj = lonejson_new(&config, &error);
 *
 * lonejson_visit_value_cstr(lj, "{\"ok\":true,\"n\":42}", &visitor, user,
 *                           &error);
 * lonejson_free(lj);
 * ```
 *
 * The repository examples expand these patterns into complete programs covering
 * one-shot parsing, object-framed streams, fixed-capacity fields, spool-to-disk
 * fields, source-backed outbound fields, JSONL output, optional curl
 * integration, and the Lua binding. For public structs, prefer lonejson's
 * explicit initializers over manual `memset` or `{0}`:
 * `lonejson_new`, `lonejson_default_config`, `lonejson_init`,
 * `lonejson_error_init`, `lonejson_default_value_visitor`, and the
 * handle-specific `*_init` helpers.
 */

/** Major component of the lonejson header version. */
#define LONEJSON_VERSION_MAJOR 0
/** Minor component of the lonejson header version. */
#define LONEJSON_VERSION_MINOR 1
/** Patch component of the lonejson header version. */
#define LONEJSON_VERSION_PATCH 0
/** Shared-library ABI / SONAME version for binary compatibility tracking. */
#define LONEJSON_ABI_VERSION 28

/** Marks a mapping field as required during parse. */
#define LONEJSON_FIELD_REQUIRED (1u << 0)
/** Omits an optional field during serialization when its value is JSON `null`.
 */
#define LONEJSON_FIELD_OMIT_NULL (1u << 1)
/** Omits an optional field during serialization when its value is empty. This
 * also implies `LONEJSON_FIELD_OMIT_NULL` behavior. */
#define LONEJSON_FIELD_OMIT_EMPTY (1u << 2)
/** Internal flag used by presence-gated primitive field macros. */
#define LONEJSON_FIELD_HAS_PRESENCE (1u << 3)
/** Internal flag allowing JSON `null` to parse as an absent optional primitive
 * presence field. */
#define LONEJSON_FIELD_ACCEPT_NULL (1u << 4)
/* Internal-only flag used by implementation-generated maps. */
#define LONEJSON__FIELD_JSON_VALUE_DEFAULT_CAPTURE (1u << 31)
#define LONEJSON_FIELD_JSON_VALUE_DEFAULT_CAPTURE                              \
  LONEJSON__FIELD_JSON_VALUE_DEFAULT_CAPTURE

/** Internal/runtime flag indicating that an array container owns its backing
 * allocation. */
#define LONEJSON_ARRAY_OWNS_ITEMS (1u << 0)
/** Marks an array container as using caller-owned fixed-capacity storage. */
#define LONEJSON_ARRAY_FIXED_CAPACITY (1u << 1)
/** Overrides lonejson's internal `malloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_MALLOC
#define LONEJSON_MALLOC malloc
#endif
/** Overrides lonejson's internal `calloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_CALLOC
#define LONEJSON_CALLOC calloc
#endif
/** Overrides lonejson's internal `realloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_REALLOC
#define LONEJSON_REALLOC realloc
#endif
/** Overrides lonejson's internal `free` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_FREE
#define LONEJSON_FREE free
#endif

/** Total internal parser workspace budget used by one-shot parse/validate
 * entry points. */
#ifndef LONEJSON_PARSER_BUFFER_SIZE
#define LONEJSON_PARSER_BUFFER_SIZE 4096u
#endif
/** Private read-buffer size used by one-shot reader-based parse/validate entry
 * points. */
#ifndef LONEJSON_READER_BUFFER_SIZE
#define LONEJSON_READER_BUFFER_SIZE 1024u
#endif
/** Total internal parser workspace budget used by the push parser.
 * Defaults to `LONEJSON_PARSER_BUFFER_SIZE`. */
#ifndef LONEJSON_PUSH_PARSER_BUFFER_SIZE
#define LONEJSON_PUSH_PARSER_BUFFER_SIZE LONEJSON_PARSER_BUFFER_SIZE
#endif
/** Private read-buffer size used by object-framed streaming APIs. Defaults to
 * `LONEJSON_READER_BUFFER_SIZE`. */
#ifndef LONEJSON_STREAM_BUFFER_SIZE
#define LONEJSON_STREAM_BUFFER_SIZE LONEJSON_READER_BUFFER_SIZE
#endif
/** Default in-memory threshold before streamed fields spill into a temporary
 * file. */
#ifndef LONEJSON_SPOOL_MEMORY_LIMIT
#define LONEJSON_SPOOL_MEMORY_LIMIT 65536u
#endif
/** Default hard cap for spool-backed streamed text/base64 fields. */
#ifndef LONEJSON_SPOOL_MAX_BYTES
#define LONEJSON_SPOOL_MAX_BYTES (10u * 1024u * 1024u)
#endif
/** Default decoded-byte cap for one dynamically allocated mapped string
 * field during parse. Zero disables the ceiling when set on
 * `lonejson_config.max_dynamic_string_bytes`.
 */
#ifndef LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES
#define LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES (128u * 1024u)
#endif
/** Default hard cap for lonejson-owned heap bytes kept live by one parse call.
 * Zero disables the ceiling when set on `lonejson_config.max_alloc_bytes`.
 */
#ifndef LONEJSON_PARSE_MAX_ALLOC_BYTES
#define LONEJSON_PARSE_MAX_ALLOC_BYTES (1024u * 1024u)
#endif
/** Default hard cap for serializer-owned output buffers. Set explicit
 * `lonejson_config.write_max_output_bytes` when a runtime needs a larger
 * materialized result.
 */
#ifndef LONEJSON_WRITE_MAX_OUTPUT_BYTES
#define LONEJSON_WRITE_MAX_OUTPUT_BYTES (8u * 1024u * 1024u)
#endif
/** Fixed path-buffer capacity used for named temporary spool files. */
#ifndef LONEJSON_SPOOL_TEMP_PATH_CAPACITY
#define LONEJSON_SPOOL_TEMP_PATH_CAPACITY 512u
#endif

#ifndef LONEJSON_TRACK_WORKSPACE_USAGE
/** Enables parser workspace high-water tracking when non-zero. */
#define LONEJSON_TRACK_WORKSPACE_USAGE 0
#endif

/** Outcome codes returned by lonejson parse, stream, and serialization APIs.
 */
typedef enum lonejson_status {
  /** Operation completed successfully. */
  LONEJSON_STATUS_OK = 0,
  /** Caller supplied an invalid pointer, size, or incompatible argument. */
  LONEJSON_STATUS_INVALID_ARGUMENT,
  /** Input was not syntactically valid JSON. */
  LONEJSON_STATUS_INVALID_JSON,
  /** A JSON value did not match the mapped destination field type. */
  LONEJSON_STATUS_TYPE_MISMATCH,
  /** A field marked required by the map was not present in the object. */
  LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
  /** Duplicate object keys were rejected by parse options. */
  LONEJSON_STATUS_DUPLICATE_FIELD,
  /** A fixed-capacity destination or configured spool limit was exceeded. */
  LONEJSON_STATUS_OVERFLOW,
  /** Output was truncated under a non-failing overflow policy. */
  LONEJSON_STATUS_TRUNCATED,
  /** lonejson could not allocate internal or mapped dynamic storage. */
  LONEJSON_STATUS_ALLOCATION_FAILED,
  /** A caller-provided sink or reader callback reported failure. */
  LONEJSON_STATUS_CALLBACK_FAILED,
  /** An underlying file descriptor or `FILE *` operation failed. */
  LONEJSON_STATUS_IO_ERROR,
  /** The operation is valid, but unsupported for the selected runtime shape. */
  LONEJSON_STATUS_UNSUPPORTED,
  /** lonejson encountered an unexpected internal state. */
  LONEJSON_STATUS_INTERNAL_ERROR
} lonejson_status;

/** Internal field categories understood by `lonejson_field` maps. Most users
 * select these indirectly through the `LONEJSON_FIELD_*` macros. */
typedef enum lonejson_field_kind {
  /** JSON string stored in a fixed buffer or allocated `char *`. */
  LONEJSON_FIELD_KIND_STRING = 0,
  /** JSON string streamed into a `lonejson_spooled` handle. */
  LONEJSON_FIELD_KIND_STRING_STREAM = 1,
  /** Base64 JSON string decoded into a `lonejson_spooled` handle. */
  LONEJSON_FIELD_KIND_BASE64_STREAM = 2,
  /** Serialize-only JSON string streamed from a `lonejson_source` handle. */
  LONEJSON_FIELD_KIND_STRING_SOURCE = 3,
  /** Serialize-only Base64 JSON string streamed from a `lonejson_source`
   * handle. */
  LONEJSON_FIELD_KIND_BASE64_SOURCE = 4,
  /** Arbitrary embedded JSON value stored or streamed through
   * `lonejson_json_value`. */
  LONEJSON_FIELD_KIND_JSON_VALUE = 5,
  /** JSON integer stored in a `lonejson_int64`. */
  LONEJSON_FIELD_KIND_I64 = 6,
  /** JSON unsigned integer stored in a `lonejson_uint64`. */
  LONEJSON_FIELD_KIND_U64 = 7,
  /** JSON number stored in a `double`. */
  LONEJSON_FIELD_KIND_F64 = 8,
  /** JSON boolean stored in a `bool`. */
  LONEJSON_FIELD_KIND_BOOL = 9,
  /** Nested JSON object mapped by a nested `lonejson_map`. */
  LONEJSON_FIELD_KIND_OBJECT = 10,
  /** JSON array of strings stored in `lonejson_string_array`. */
  LONEJSON_FIELD_KIND_STRING_ARRAY = 11,
  /** JSON array of integers stored in `lonejson_i64_array`. */
  LONEJSON_FIELD_KIND_I64_ARRAY = 12,
  /** JSON array of unsigned integers stored in `lonejson_u64_array`. */
  LONEJSON_FIELD_KIND_U64_ARRAY = 13,
  /** JSON array of numbers stored in `lonejson_f64_array`. */
  LONEJSON_FIELD_KIND_F64_ARRAY = 14,
  /** JSON array of booleans stored in `lonejson_bool_array`. */
  LONEJSON_FIELD_KIND_BOOL_ARRAY = 15,
  /** JSON array of nested objects stored in `lonejson_object_array`. */
  LONEJSON_FIELD_KIND_OBJECT_ARRAY = 16,
  /** JSON array of strings streamed through `lonejson_string_array_stream`. */
  LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM = 17,
  /** JSON array of mapped objects streamed item-by-item. */
  LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM = 18
} lonejson_field_kind;

/** Backing source kind used by `lonejson_source` outbound field handles. */
typedef enum lonejson_source_kind {
  /** No source configured. Serializes as JSON `null`. */
  LONEJSON_SOURCE_NONE = 0,
  /** Read raw bytes from a caller-provided `FILE *`. */
  LONEJSON_SOURCE_FILE = 1,
  /** Read raw bytes from a caller-provided file descriptor. */
  LONEJSON_SOURCE_FD = 2,
  /** Open and read a filesystem path on each serialization. */
  LONEJSON_SOURCE_PATH = 3
} lonejson_source_kind;

/** Backing mode used by `lonejson_json_value` opaque embedded JSON handles. */
typedef enum lonejson_json_value_kind {
  /** No value configured. Serializes as JSON `null`. */
  LONEJSON_JSON_VALUE_NULL = 0,
  /** Owned compact JSON bytes retained in memory. */
  LONEJSON_JSON_VALUE_BUFFER = 1,
  /** Read JSON bytes from a caller-provided reader callback. */
  LONEJSON_JSON_VALUE_READER = 2,
  /** Read JSON bytes from a caller-provided `FILE *`. */
  LONEJSON_JSON_VALUE_FILE = 3,
  /** Read JSON bytes from a caller-provided file descriptor. */
  LONEJSON_JSON_VALUE_FD = 4,
  /** Open and read JSON bytes from a filesystem path on each serialization. */
  LONEJSON_JSON_VALUE_PATH = 5
} lonejson_json_value_kind;

/** JSON value category reported to old-value-dependent rewrite callbacks. */
typedef enum lonejson_value_type {
  /** No existing selected value was present. */
  LONEJSON_VALUE_ABSENT = 0,
  /** Existing selected value is a JSON object. */
  LONEJSON_VALUE_OBJECT = 1,
  /** Existing selected value is a JSON array. */
  LONEJSON_VALUE_ARRAY = 2,
  /** Existing selected value is a JSON string. */
  LONEJSON_VALUE_STRING = 3,
  /** Existing selected value is a JSON number. */
  LONEJSON_VALUE_NUMBER = 4,
  /** Existing selected value is a JSON boolean. */
  LONEJSON_VALUE_BOOL = 5,
  /** Existing selected value is JSON `null`. */
  LONEJSON_VALUE_NULL = 6
} lonejson_value_type;

/** Storage model used by a mapped field. */
typedef enum lonejson_storage_kind {
  /** lonejson owns and allocates storage as needed. */
  LONEJSON_STORAGE_DYNAMIC = 0,
  /** Caller provides fixed-capacity storage inside the mapped struct. */
  LONEJSON_STORAGE_FIXED = 1
} lonejson_storage_kind;

/** Behavior used when a fixed-capacity output or mapped field is too small. */
typedef enum lonejson_overflow_policy {
  /** Treat overflow as an error and stop. */
  LONEJSON_OVERFLOW_FAIL = 0,
  /** Truncate but report `LONEJSON_STATUS_TRUNCATED`. */
  LONEJSON_OVERFLOW_TRUNCATE = 1,
  /** Truncate silently while still setting `error.truncated`. */
  LONEJSON_OVERFLOW_TRUNCATE_SILENT = 2
} lonejson_overflow_policy;

/** Detailed error information populated by most public APIs. Caller-provided
 * error outputs do not need prior initialization because lonejson overwrites
 * them on entry, but `lonejson_error_init` is available when you want an
 * explicit known-empty state.
 */
typedef struct lonejson_error {
  /** Primary status code for the failure or warning. */
  lonejson_status code;
  /** 1-based input line number when relevant. */
  size_t line;
  /** 1-based input column number when relevant. */
  size_t column;
  /** 1-based byte offset into the current input stream or buffer. */
  size_t offset;
  /** `errno`-style code for I/O and callback-backed failures. */
  int system_errno;
  /** Non-zero when truncation occurred under a permissive overflow policy. */
  int truncated;
  /** Human-readable summary suitable for logs and diagnostics. */
  char message[160];
} lonejson_error;

/** Optional allocation counters used by custom allocators. lonejson updates
 * these counters only in debug builds. Release builds leave them untouched.
 */
typedef struct lonejson_allocator_stats {
  /** Number of successful allocate calls performed by lonejson. */
  size_t alloc_calls;
  /** Number of successful realloc calls performed by lonejson. */
  size_t realloc_calls;
  /** Number of free calls performed by lonejson. */
  size_t free_calls;
  /** Current user-visible bytes owned by lonejson through this allocator. */
  size_t bytes_live;
  /** Peak user-visible bytes owned by lonejson through this allocator. */
  size_t peak_bytes_live;
} lonejson_allocator_stats;

/** Callback signature used for lonejson-owned allocations. */
typedef void *(*lonejson_malloc_fn)(void *ctx, size_t size);
/** Callback signature used for lonejson-owned reallocations. */
typedef void *(*lonejson_realloc_fn)(void *ctx, void *ptr, size_t size);
/** Callback signature used for lonejson-owned frees. */
typedef void (*lonejson_free_fn)(void *ctx, void *ptr);

/** Allocator vtable used by parser, spool, stream, and lonejson-owned output
 * buffers. When all three callbacks are `NULL`, lonejson falls back to
 * `lonejson_default_allocator()`. Partial callback sets are invalid; callers
 * must provide either all callbacks or none. `malloc_fn` and `realloc_fn` must
 * return pointers aligned at least as strictly as standard `malloc`; returning
 * weaker-aligned storage is undefined behavior because lonejson may place
 * internal owned-allocation headers at those addresses. Custom allocators that
 * prepend private headers must over-align the returned payload pointer, for
 * example by using a header union containing `void *`, integer, `double`, and
 * `long double` members.
 */
typedef struct lonejson_allocator {
  /** Allocation callback. `NULL` means use the default allocator. */
  lonejson_malloc_fn malloc_fn;
  /** Reallocation callback. `NULL` means use the default allocator. */
  lonejson_realloc_fn realloc_fn;
  /** Free callback. `NULL` means use the default allocator. */
  lonejson_free_fn free_fn;
  /** Opaque allocator context passed to every callback. */
  void *ctx;
  /** Optional debug allocation counters updated by lonejson in debug builds. */
  lonejson_allocator_stats *stats;
} lonejson_allocator;

/** Forward declaration for the optional auth provider vtable. */
typedef struct lonejson_auth_provider lonejson_auth_provider;
/** Forward declaration for the optional auth HTTP provider vtable. */
typedef struct lonejson_http_provider lonejson_http_provider;

/** Named spool policy selectors used by streamed text and base64 fields. */
typedef enum lonejson_spool_class {
  /** Use the runtime's default spool policy. */
  LONEJSON_SPOOL_CLASS_DEFAULT = 0,
  /** Use the runtime's blob-oriented spool policy. */
  LONEJSON_SPOOL_CLASS_BLOB = 1,
  /** Use the runtime's large-text spool policy. */
  LONEJSON_SPOOL_CLASS_LARGE_TEXT = 2
} lonejson_spool_class;

/** Runtime spool policy configuration. */
typedef struct lonejson_spool_policy {
  /** Maximum bytes retained in memory before spilling additional data. */
  size_t memory_limit;
  /** Hard maximum logical size in bytes. Zero means unbounded. */
  size_t max_bytes;
  /** Optional temporary directory for named spool files. `lonejson_new()`
   * copies this path into runtime-owned storage.
   */
  const char *temp_dir;
} lonejson_spool_policy;

/** Runtime-wide lonejson configuration.
 *
 * Pass this to `lonejson_new()` to establish one parser/serializer runtime and
 * its default policy. Passing `NULL` to `lonejson_new()` resolves the library
 * defaults.
 */
typedef struct lonejson_config {
  /** Maximum lonejson-owned live parse heap bytes. Zero disables the ceiling.
   */
  size_t max_alloc_bytes;
  /** Maximum decoded byte length for one dynamically allocated mapped string.
   * Zero disables the ceiling.
   */
  size_t max_dynamic_string_bytes;
  /** Maximum mapped parse depth. */
  size_t max_depth;
  /** Maximum bytes accepted while parsing one arbitrary `JSON_VALUE`. Zero
   * disables the ceiling.
   */
  size_t json_value_max_total_bytes;
  /** Maximum decoded string bytes accepted inside one `JSON_VALUE`. */
  size_t json_value_max_string_bytes;
  /** Maximum decoded object-key bytes accepted inside one `JSON_VALUE`. */
  size_t json_value_max_key_bytes;
  /** Maximum raw number-token bytes accepted inside one `JSON_VALUE`. */
  size_t json_value_max_number_bytes;
  /** Maximum nested depth accepted inside one `JSON_VALUE`. */
  size_t json_value_max_depth;
  /** Default for mapped parse destination clearing. */
  int clear_destination_by_default;
  /** Default for duplicate-key rejection in mapped parses. */
  int reject_duplicate_keys_by_default;
  /** Default serializer overflow policy for fixed-capacity outputs. */
  lonejson_overflow_policy write_overflow_policy;
  /** Default serializer pretty-print toggle. */
  int write_pretty;
  /** Maximum serializer-owned output bytes. Zero keeps the library default. */
  size_t write_max_output_bytes;
  /** Default spool policy for streamed mapped fields. */
  lonejson_spool_policy spool_default;
  /** Blob-oriented spool policy for streamed mapped fields. */
  lonejson_spool_policy spool_blob;
  /** Large-text spool policy for streamed mapped fields. */
  lonejson_spool_policy spool_large_text;
  /** Optional caller-owned scratch used to stage reused fixed-string fields
   * without allocating temporary parse-owned storage. */
  void *fixed_string_scratch;
  /** Byte size of `fixed_string_scratch`. Zero disables caller-provided fixed
   * string staging scratch.
   */
  size_t fixed_string_scratch_size;
  /** Optional allocator used by lonejson-owned runtime allocations.
   * `lonejson_new()` copies this vtable by value into the runtime; any custom
   * allocator context or stats pointers referenced from it must outlive the
   * runtime and every object or value initialized from that runtime. That
   * includes streams, array streams, writers, generators, spooled values,
   * `lonejson_json_value` handles, and mapped records that still need cleanup
   * after `lonejson_free()`. The copied-runtime alias bookkeeping used to
   * retire by-value `lonejson` handle copies is implementation-owned process
   * metadata and does not route through this allocator.
   */
  const lonejson_allocator *allocator;
  /** Optional auth provider used by runtime-backed JWT/OIDC trust helpers.
   * `lonejson_new()` copies this vtable by value. The provider's `user_data`
   * remains caller-owned and must outlive the runtime.
   */
  const lonejson_auth_provider *auth_provider;
  /** Optional HTTP provider used by runtime-backed OIDC/OAuth2 helpers.
   * `lonejson_new()` copies this vtable by value. The provider's `user_data`
   * remains caller-owned and must outlive the runtime.
   */
  const lonejson_http_provider *http_provider;
  /** Internal initialization marker set by `lonejson_default_config()`.
   * Build configs from that helper before overriding fields; raw zeroed or
   * partial designated literals are rejected because `0` is a meaningful
   * override for several ceilings and policies.
   */
  unsigned _config_cookie;
} lonejson_config;

/** Instantiated lonejson runtime handle.
 *
 * Construct one with `lonejson_new()` and pass the returned pointer to the
 * free-function APIs, or call the equivalent method pointers on the runtime
 * itself. The full owner-only semantics and method surface are documented on
 * `struct lonejson` below.
 */
typedef struct lonejson lonejson;

/** Internal spool threshold layout retained for lonejson's implementation.
 * Public callers configure spool behavior through `lonejson_config` and named
 * spool classes.
 */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
typedef struct lonejson__spool_options {
  /** Maximum bytes retained in memory before lonejson spills additional data to
   * a temporary file. */
  size_t memory_limit;
  /** Optional hard maximum logical size in bytes. Zero means unbounded. */
  size_t max_bytes;
  /** Optional temporary directory for named spool files. `NULL` uses the
   * platform default temporary-file mechanism. */
  const char *temp_dir;
} lonejson__spool_options;
#endif

/** Result returned by reader callbacks and `lonejson_spooled_read`. Use
 * `lonejson_default_read_result()` when constructing one manually inside a
 * reader callback instead of open-coding `memset` or `{0}`.
 *
 * `would_block` is only supported by lonejson APIs that retain resumable
 * state across calls, such as object streams, array streams, and push or
 * backpressure writer surfaces. One-shot reader helpers fail closed on
 * `would_block` instead of spinning or attempting an implicit retry.
 */
typedef struct lonejson_read_result {
  /** Number of bytes placed into the supplied buffer. */
  size_t bytes_read;
  /** Non-zero when the input source reached end-of-stream. */
  int eof;
  /** Non-zero when the source would block and the caller should retry later. */
  int would_block;
  /** `errno`-style failure code. Zero indicates success. */
  int error_code;
  /** Reserved for ABI-stable padding; must remain zero. */
  int reserved;
} lonejson_read_result;

/** Callback type used by reader-backed parse and stream APIs. */
typedef lonejson_read_result (*lonejson_reader_fn)(void *user,
                                                   unsigned char *buffer,
                                                   size_t capacity);
/** Generic sink callback used by serializer APIs and raw spool writers. */
typedef lonejson_status (*lonejson_sink_fn)(void *user, const void *data,
                                            size_t len, lonejson_error *error);

/** Base64 alphabet and padding policy. */
typedef enum lonejson_base64_variant {
  /** RFC 4648 standard alphabet with `=` padding. */
  LONEJSON_BASE64_STANDARD = 0,
  /** RFC 4648 standard alphabet without `=` padding. */
  LONEJSON_BASE64_STANDARD_RAW = 1,
  /** RFC 4648 URL-safe alphabet with `=` padding. */
  LONEJSON_BASE64_URL = 2,
  /** RFC 4648 URL-safe alphabet without `=` padding, as used by JWT/JWS. */
  LONEJSON_BASE64_URL_RAW = 3
} lonejson_base64_variant;

/** Spill-backed storage used by streamed text and decoded byte fields.
 * Applications typically treat this as an opaque handle and interact through
 * the `lonejson_spooled_*` helpers. */
typedef struct lonejson_spooled {
  /** In-memory prefix retained before spill-to-disk. */
  unsigned char *memory;
  /** Number of bytes currently stored in `memory`. */
  size_t memory_len;
  /** Total logical bytes held by the handle across memory and any spill file.
   */
  size_t size;
  /** Current raw-read cursor used by `lonejson_spooled_read`. */
  size_t read_offset;
  /** Effective in-memory threshold applied to this handle. */
  size_t memory_limit;
  /** Effective hard maximum size applied to this handle. Zero means unbounded.
   */
  size_t max_bytes;
  /** Temporary directory associated with the handle, if any. */
  const char *temp_dir;
  /** Backing temporary file once the handle has spilled. */
  FILE *spill_fp;
  /** Non-zero once data has been written beyond the in-memory prefix. */
  int spilled;
  /** Named temporary path when the configured temp directory path is used.
   * Empty for anonymous temporary files. */
  char temp_path[LONEJSON_SPOOL_TEMP_PATH_CAPACITY];
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
  /** Internal owned duplicate of `temp_dir` when lonejson must retain it. */
  char *owned_temp_dir;
#else
  void *_reserved_owned_temp_dir;
#endif
  /** Allocator used for lonejson-owned spool metadata and buffers. */
  lonejson_allocator allocator;
  /** Internal state marker used by lonejson to distinguish initialized
   * handles from uninitialized memory. Treat as opaque.
   */
  unsigned _lonejson_magic;
  /** Clears stored contents while preserving configured spool limits. */
  void (*reset)(struct lonejson_spooled *value);
  /** Releases all resources owned by the spool handle. */
  void (*cleanup)(struct lonejson_spooled *value);
  /** Returns the logical byte length held by the handle. */
  size_t (*size_fn)(const struct lonejson_spooled *value);
  /** Returns non-zero once the handle has spilled beyond memory. */
  int (*spilled_fn)(const struct lonejson_spooled *value);
  /** Appends raw bytes to the handle. */
  lonejson_status (*append)(struct lonejson_spooled *value, const void *data,
                            size_t len, lonejson_error *error);
  /** Rewinds the raw read cursor to the beginning. */
  lonejson_status (*rewind)(struct lonejson_spooled *value,
                            lonejson_error *error);
  /** Reads raw bytes sequentially from the current read cursor. */
  lonejson_read_result (*read)(struct lonejson_spooled *value,
                               unsigned char *buffer, size_t capacity);
  /** Streams the stored bytes to a generic sink callback. */
  lonejson_status (*write_to_sink)(const struct lonejson_spooled *value,
                                   lonejson_sink_fn sink, void *user,
                                   lonejson_error *error);
} lonejson_spooled;

/** Serialize-only outbound source used by streamed text/base64 source fields.
 * Unlike `lonejson_spooled`, this handle never buffers payload bytes inside
 * lonejson; it streams directly from a file, fd, or reopenable path when a
 * mapped struct is serialized. */
typedef struct lonejson_source {
  /** Current configured source kind. `LONEJSON_SOURCE_NONE` emits `null`. */
  lonejson_source_kind kind;
  /** Caller-provided file handle when `kind == LONEJSON_SOURCE_FILE`. */
  FILE *fp;
  /** Caller-provided file descriptor when `kind == LONEJSON_SOURCE_FD`. */
  int fd;
  /** Owned filesystem path when `kind == LONEJSON_SOURCE_PATH`. */
  char *path;
  /** Resets the handle to the empty `null` state and releases owned path
   * storage.
   */
  void (*cleanup)(struct lonejson_source *value);
  /** Alias for `cleanup()` in receiver-method form. */
  void (*reset)(struct lonejson_source *value);
  /** Configures the handle to stream from an open `FILE *`. */
  lonejson_status (*set_file)(struct lonejson_source *value, FILE *fp,
                              lonejson_error *error);
  /** Configures the handle to stream from a file descriptor. */
  lonejson_status (*set_fd)(struct lonejson_source *value, int fd,
                            lonejson_error *error);
  /** Configures the handle to stream from a reopenable filesystem path. */
  lonejson_status (*set_path)(struct lonejson_source *value, const char *path,
                              lonejson_error *error);
  /** Streams the configured source bytes to a sink callback. */
  lonejson_status (*write_to_sink)(const struct lonejson_source *value,
                                   lonejson_sink_fn sink, void *user,
                                   lonejson_error *error);
  /** Returns non-zero when the configured source can be replayed. */
  int (*is_rewindable)(const struct lonejson_source *value);
} lonejson_source;

/** Dynamically sized array of NUL-terminated strings. */
typedef struct lonejson_string_array {
  /** Element storage. lonejson owns this buffer when `flags` contains
   * `LONEJSON_ARRAY_OWNS_ITEMS`. */
  char **items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_string_array;

/** Dynamically sized array of `lonejson_int64` values. */
typedef struct lonejson_i64_array {
  /** Element storage. lonejson owns this buffer when `flags` contains
   * `LONEJSON_ARRAY_OWNS_ITEMS`. */
  lonejson_int64 *items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_i64_array;

/** Dynamically sized array of `lonejson_uint64` values. */
typedef struct lonejson_u64_array {
  /** Element storage. lonejson owns this buffer when `flags` contains
   * `LONEJSON_ARRAY_OWNS_ITEMS`. */
  lonejson_uint64 *items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_u64_array;

/** Dynamically sized array of `double` values. */
typedef struct lonejson_f64_array {
  /** Element storage. lonejson owns this buffer when `flags` contains
   * `LONEJSON_ARRAY_OWNS_ITEMS`. */
  double *items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_f64_array;

/** Dynamically sized array of boolean values. */
typedef struct lonejson_bool_array {
  /** Element storage. lonejson owns this buffer when `flags` contains
   * `LONEJSON_ARRAY_OWNS_ITEMS`. */
  bool *items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_bool_array;

/** Dynamically sized array of nested structs described by another map. */
typedef struct lonejson_object_array {
  /** Raw element storage. Cast to the mapped element type when consuming. */
  void *items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Size in bytes of each element struct. */
  size_t elem_size;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_object_array;

/** Forward declaration for one mapped JSON field descriptor. */
typedef struct lonejson_field lonejson_field;
/** Forward declaration for one schema map describing a C struct. */
typedef struct lonejson_map lonejson_map;
/** Forward declarations for rewrite option struct tags used by runtime methods.
 */
struct lonejson_array_rewrite_options;
struct lonejson_value_rewrite_options;
struct lonejson_value_rewrite_selector_options;
typedef struct lonejson_curl_parse lonejson_curl_parse;
typedef struct lonejson_curl_array_parse lonejson_curl_array_parse;
typedef struct lonejson_curl_string_array_parse
    lonejson_curl_string_array_parse;
typedef struct lonejson_curl_string_items_parse
    lonejson_curl_string_items_parse;
typedef struct lonejson_curl_upload lonejson_curl_upload;
#ifdef LONEJSON_WITH_CURL
#ifdef LONEJSON_WITH_OIDC
typedef struct lonejson_oidc_jwks_cache_parse lonejson_oidc_jwks_cache_parse;
#endif
#endif
/** Object-framed JSON stream cursor. */
typedef struct lonejson_stream lonejson_stream;
/** Selected-array item stream cursor. */
typedef struct lonejson_array_stream lonejson_array_stream;
struct lonejson_array_stream_string_handler;
#ifdef LONEJSON_WITH_JWT
typedef struct lonejson_jws_verify_request lonejson_jws_verify_request;
/** Caller-owned JWT compact-serialization segment. */
typedef struct lonejson_jwt_segment {
  /** Pointer into the original token or caller-provided segment buffer. */
  const char *data;
  /** Number of bytes in `data`. */
  size_t len;
} lonejson_jwt_segment;

/** Parsed JWT compact serialization. Parsing only splits and checks syntax; it
 * does not validate trust, claims, signatures, issuers, or algorithms.
 */
typedef struct lonejson_jwt_compact {
  /** Base64url-encoded JOSE header segment. */
  lonejson_jwt_segment header;
  /** Base64url-encoded claims payload segment. */
  lonejson_jwt_segment payload;
  /** Base64url-encoded signature segment, which may be empty. */
  lonejson_jwt_segment signature;
  /** Header, separator, and payload bytes covered by a JWS signature. */
  lonejson_jwt_segment signing_input;
} lonejson_jwt_compact;

/** Parsed JSON Web Key with lonejson-owned string fields. Parsing only checks
 * JWK shape and base64url syntax; it does not establish trust.
 */
typedef struct lonejson_jwk {
  /** Key type such as `RSA`, `EC`, or `oct`. Required. */
  char *kty;
  /** Optional key identifier. */
  char *kid;
  /** Optional intended algorithm. */
  char *alg;
  /** Optional public key use such as `sig`. */
  char *use;
  /** Optional permitted key operations such as `verify`. */
  lonejson_string_array key_ops;
  /** Optional curve name for EC keys. */
  char *crv;
  /** RSA modulus, base64url encoded. */
  char *n;
  /** RSA exponent, base64url encoded. */
  char *e;
  /** EC public x coordinate, base64url encoded. */
  char *x;
  /** EC public y coordinate, base64url encoded. */
  char *y;
  /** Symmetric key bytes, base64url encoded. */
  char *k;
  /** Optional X.509 certificate SHA-1 thumbprint, base64url encoded. */
  char *x5t;
  /** Optional X.509 certificate SHA-256 thumbprint, base64url encoded. */
  char *x5t_s256;
  /** Optional X.509 certificate chain, each entry base64 DER encoded. */
  lonejson_string_array x5c;
} lonejson_jwk;

/** Parsed JWK Set. `keys.items` contains `lonejson_jwk` elements. */
typedef struct lonejson_jwks {
  lonejson_object_array keys;
} lonejson_jwks;

/** Optional JWK selection filters. NULL filter members are ignored. */
typedef struct lonejson_jwk_select_options {
  const char *kid;
  const char *kty;
  const char *alg;
  const char *use;
} lonejson_jwk_select_options;

/** Decoded JWT JOSE header fields retained by lonejson. */
typedef struct lonejson_jwt_header {
  /** JOSE algorithm. Required for validation. */
  char *alg;
  /** Optional key identifier. */
  char *kid;
  /** Optional token type. */
  char *typ;
  /** Optional critical JOSE header parameters. */
  lonejson_string_array crit;
  /** Optional X.509 certificate SHA-1 thumbprint, base64url encoded. */
  char *x5t;
  /** Optional X.509 certificate SHA-256 thumbprint, base64url encoded. */
  char *x5t_s256;
  /** Optional X.509 certificate chain, each entry base64 DER encoded. */
  lonejson_string_array x5c;
} lonejson_jwt_header;

/** Decoded JWT claims retained by lonejson.
 *
 * This struct is parse output only. Values are not trusted until validated
 * with an explicit `lonejson_jwt_claim_policy`.
 */
typedef struct lonejson_jwt_claims {
  /** Issuer claim. */
  char *iss;
  /** Subject claim. */
  char *sub;
  /** OIDC nonce claim, when present. */
  char *nonce;
  /** OIDC authorized party claim, when present. */
  char *azp;
  /** OAuth2 scope claim when encoded as a space-delimited string. */
  char *scope;
  /** OAuth2 `scp` claim values when encoded as a string or array. */
  lonejson_string_array scp;
  /** String audience claim when `aud` is encoded as a JSON string. */
  char *aud;
  /** Audience array when `aud` is encoded as a JSON array of strings. */
  lonejson_string_array aud_array;
  /** Expiration time seconds since Unix epoch, present when `has_exp != 0`. */
  lonejson_int64 exp;
  /** Not-before time seconds since Unix epoch, present when `has_nbf != 0`. */
  lonejson_int64 nbf;
  /** Issued-at time seconds since Unix epoch, present when `has_iat != 0`. */
  lonejson_int64 iat;
  int has_exp;
  int has_nbf;
  int has_iat;
} lonejson_jwt_claims;

/** Explicit trust policy for validating parsed JWT header and claims. */
typedef struct lonejson_jwt_claim_policy {
  /** Accepted JOSE algorithms. Required and must not include `none`. */
  const char *const *accepted_algs;
  size_t accepted_alg_count;
  /** Accepted issuer values. Required. */
  const char *const *accepted_issuers;
  size_t accepted_issuer_count;
  /** Accepted audience values. Required. */
  const char *const *accepted_audiences;
  size_t accepted_audience_count;
  /** Optional expected OIDC nonce. When non-NULL, `claims->nonce` must match.
   */
  const char *expected_nonce;
  /** Optional expected OIDC authorized party. */
  const char *expected_azp;
  /** Critical JOSE header names explicitly handled by the caller. */
  const char *const *accepted_crit;
  size_t accepted_crit_count;
  /** Required OAuth2 scopes. They may appear in `scope` or `scp`. */
  const char *const *required_scopes;
  size_t required_scope_count;
  /** Non-zero requires `azp` when `aud` contains multiple audiences. */
  int require_azp_when_multiple_audiences;
  /** Non-zero requires every JWT audience to be in `accepted_audiences`. */
  int require_all_audiences_accepted;
  /** Required claim names such as `sub` or `iat`. */
  const char *const *required_claims;
  size_t required_claim_count;
  /** Current time in seconds since Unix epoch. */
  lonejson_int64 now;
  /** Allowed clock skew in seconds. Negative values are invalid. */
  lonejson_int64 allowed_clock_skew;
  /** Maximum compact token size accepted by `lonejson_jwt_decode_compact`. */
  size_t max_token_bytes;
  /** Maximum decoded JOSE header JSON size. Zero means the default limit. */
  size_t max_decoded_header_bytes;
  /** Maximum decoded claims JSON size. Zero means the default limit. */
  size_t max_decoded_claims_bytes;
} lonejson_jwt_claim_policy;

/** High-level JWS verification request passed to auth crypto providers. */
struct lonejson_jws_verify_request {
  /** Parsed compact JWT/JWS segments. All slices are caller-owned. */
  const lonejson_jwt_compact *jwt;
  /** Decoded JOSE header associated with `jwt`. */
  const lonejson_jwt_header *header;
  /** Selected candidate verification key. */
  const lonejson_jwk *jwk;
};

/** Auth provider vtable for provider-backed JWT/OIDC/M2M helpers.
 *
 * The caller owns `user_data` and must keep it alive until no runtime using
 * this provider can call auth APIs. `verify_jws` is required for runtime JWT
 * signature validation, `sha256` is required for PKCE challenge computation,
 * and both `random_bytes` and `sha256` are required for PKCE generation and
 * M2M credential/signup helpers. Callback implementations may delegate to
 * OpenSSL, another crypto stack, or test fixtures; lonejson only depends on
 * this narrow public surface.
 */
struct lonejson_auth_provider {
  /** Caller-owned provider context passed to auth callbacks. */
  void *user_data;
  /** Verifies one JWS signature against the selected JWK. */
  lonejson_status (*verify_jws)(void *user_data,
                                const lonejson_jws_verify_request *request,
                                lonejson_error *error);
  /** Fills `dst` with cryptographically strong random bytes. */
  lonejson_status (*random_bytes)(void *user_data, unsigned char *dst,
                                  size_t len, lonejson_error *error);
  /** Computes SHA-256 over `data` into the 32-byte output buffer. */
  lonejson_status (*sha256)(void *user_data, const void *data, size_t len,
                            unsigned char out[32], lonejson_error *error);
};

#ifdef LONEJSON_WITH_OPENSSL
/** Optional OpenSSL provider configuration. NULL fields use OpenSSL defaults.
 *
 * `x509_store` may point to an OpenSSL `X509_STORE` owned by the caller. The
 * OpenSSL adapter copies this pointer value into `lonejson_auth_provider`, but
 * does not take ownership of the store object. When NULL, the OpenSSL provider
 * uses a temporary store with OpenSSL default verify paths. The caller must
 * keep a non-NULL store object alive until no runtime using the provider can
 * call auth APIs.
 */
typedef struct lonejson_openssl_auth_provider_config {
  /** Optional OpenSSL library context. */
  void *libctx;
  /** Optional OpenSSL property query string. */
  const char *propq;
  /** Optional caller-owned `X509_STORE *` trust store. */
  void *x509_store;
} lonejson_openssl_auth_provider_config;
#endif
#endif
#ifdef LONEJSON_WITH_OIDC
typedef struct lonejson_http_request lonejson_http_request;
typedef struct lonejson_http_response lonejson_http_response;
typedef struct lonejson_http_provider_config lonejson_http_provider_config;
/** Parsed OpenID Connect discovery metadata retained by lonejson.
 *
 * This object is metadata only. Fetching is normally performed through a
 * caller-installed HTTP provider via `lonejson_oidc_fetch_discovery()`, or by
 * caller-owned HTTP code followed by `lonejson_oidc_discovery_parse_json()`.
 */
typedef struct lonejson_oidc_discovery {
  /** Discovery issuer. Required and must match the expected issuer before use.
   */
  char *issuer;
  /** OAuth2/OIDC authorization endpoint, when advertised. */
  char *authorization_endpoint;
  /** OAuth2 token endpoint. Required for client-credentials flows. */
  char *token_endpoint;
  /** JWK Set endpoint used for JWT signature key retrieval. Required. */
  char *jwks_uri;
  /** OAuth2 token introspection endpoint, when advertised. */
  char *introspection_endpoint;
  /** OAuth2 token revocation endpoint, when advertised. */
  char *revocation_endpoint;
  /** OIDC UserInfo endpoint, when advertised. */
  char *userinfo_endpoint;
} lonejson_oidc_discovery;

/** Explicit policy for installing and selecting a cached JWKS document. */
typedef struct lonejson_oidc_jwks_cache_policy {
  /** Expected issuer owning the JWKS document. Required and must be HTTPS. */
  const char *issuer;
  /** JWKS endpoint URI used for the fetch. Required and must be HTTPS. */
  const char *jwks_uri;
  /** Maximum JWKS JSON response size in bytes. Zero means the default limit. */
  size_t max_jwks_bytes;
  /** Current time in seconds since Unix epoch. */
  lonejson_int64 now;
  /** Cache lifetime in seconds. Must be positive. */
  lonejson_int64 ttl_seconds;
} lonejson_oidc_jwks_cache_policy;

/** Parsed, bounded JWKS cache for one issuer/JWKS URI pair.
 *
 * The cache owns all strings and keys. It never fetches on its own; callers or
 * curl adapters provide JWKS JSON bytes explicitly.
 */
typedef struct lonejson_oidc_jwks_cache {
  /** Issuer this cache was installed for. */
  char *issuer;
  /** JWKS URI this cache was installed from. */
  char *jwks_uri;
  /** Fetch/install time in seconds since Unix epoch. */
  lonejson_int64 fetched_at;
  /** Expiry time in seconds since Unix epoch. */
  lonejson_int64 expires_at;
  /** Maximum JWKS JSON bytes accepted when the cache was installed. */
  size_t max_jwks_bytes;
  /** Non-zero when `jwks` contains an installed key set. */
  int has_jwks;
  /** Parsed key set owned by this cache. */
  lonejson_jwks jwks;
} lonejson_oidc_jwks_cache;

/** OAuth2 client-credentials request body options.
 *
 * This models the `client_secret_post` token endpoint authentication method.
 * Network transfer, TLS policy, and durable credential storage remain
 * caller-owned. Higher-level token-flow helpers may layer bounded retry and
 * refresh behavior over these request-body primitives.
 */
typedef struct lonejson_oauth2_client_credentials {
  /** OAuth2 client identifier. Required. */
  const char *client_id;
  /** OAuth2 client secret. Required. */
  const char *client_secret;
  /** Optional OAuth2 scope string. */
  const char *scope;
  /** Optional provider-specific audience parameter. */
  const char *audience;
  /** Optional RFC 8707 resource indicator. */
  const char *resource;
  /** Maximum encoded request body bytes. Zero means the default limit. */
  size_t max_body_bytes;
} lonejson_oauth2_client_credentials;

/** OAuth2 refresh-token request body options. */
typedef struct lonejson_oauth2_refresh_token {
  /** Refresh token previously issued by the authorization server. Required. */
  const char *refresh_token;
  /** Optional OAuth2 client identifier. Required when `client_secret` is set.
   */
  const char *client_id;
  /** Optional OAuth2 client secret for `client_secret_post` authentication. */
  const char *client_secret;
  /** Optional OAuth2 scope string for scope narrowing. */
  const char *scope;
  /** Maximum encoded request body bytes. Zero means the default limit. */
  size_t max_body_bytes;
} lonejson_oauth2_refresh_token;

/** OAuth2 token introspection request body options. */
typedef struct lonejson_oauth2_token_introspection {
  /** Access or refresh token to introspect. Required. */
  const char *token;
  /** Optional token type hint, commonly `access_token` or `refresh_token`. */
  const char *token_type_hint;
  /** Optional OAuth2 client identifier for `client_secret_post`. */
  const char *client_id;
  /** Optional OAuth2 client secret for `client_secret_post`. */
  const char *client_secret;
  /** Use `client_secret_basic` on provider-backed requests instead of posting
   * client credentials in the form body.
   */
  int use_basic_auth;
  /** Maximum encoded request body bytes. Zero means the default limit. */
  size_t max_body_bytes;
} lonejson_oauth2_token_introspection;

/** OAuth2 token revocation request body options. */
typedef struct lonejson_oauth2_token_revocation {
  /** Access or refresh token to revoke. Required. */
  const char *token;
  /** Optional token type hint, commonly `access_token` or `refresh_token`. */
  const char *token_type_hint;
  /** Optional OAuth2 client identifier for `client_secret_post`. */
  const char *client_id;
  /** Optional OAuth2 client secret for `client_secret_post`. */
  const char *client_secret;
  /** Use `client_secret_basic` on provider-backed requests instead of posting
   * client credentials in the form body.
   */
  int use_basic_auth;
  /** Maximum encoded request body bytes. Zero means the default limit. */
  size_t max_body_bytes;
} lonejson_oauth2_token_revocation;

/** OIDC/OAuth2 authorization-code token request body options. */
typedef struct lonejson_oidc_authorization_code_token {
  /** OAuth2 client identifier. Required. */
  const char *client_id;
  /** Authorization code received from the redirect callback. Required. */
  const char *code;
  /** Redirect URI used in the authorization request. Required. */
  const char *redirect_uri;
  /** PKCE code verifier matching the authorization request challenge. Required.
   */
  const char *code_verifier;
  /** Optional OAuth2 client secret for confidential clients. */
  const char *client_secret;
  /** Maximum encoded request body bytes. Zero means the default limit. */
  size_t max_body_bytes;
} lonejson_oidc_authorization_code_token;

/** Parsed successful OAuth2 token endpoint response.
 *
 * This object owns all strings. An access token is credential material; callers
 * are responsible for storage, logging, and lifetime policy.
 */
typedef struct lonejson_oauth2_token_response {
  /** Bearer access token returned by the token endpoint. Required on success.
   */
  char *access_token;
  /** Token type, normally `Bearer`. Required on success. */
  char *token_type;
  /** Optional refresh token for later transparent refresh. */
  char *refresh_token;
  /** Granted or narrowed OAuth2 scope. */
  char *scope;
  /** Optional OIDC ID token. */
  char *id_token;
  /** OAuth2 error code when parsing an error-shaped response. */
  char *error;
  /** Optional provider diagnostic for `error`. */
  char *error_description;
  /** Optional provider documentation URI for `error`. */
  char *error_uri;
  /** Access-token lifetime in seconds, meaningful when `has_expires_in != 0`.
   */
  lonejson_int64 expires_in;
  /** Non-zero when `expires_in` was present. */
  int has_expires_in;
} lonejson_oauth2_token_response;

/** OAuth2/OIDC token-flow state retained by lonejson helpers.
 *
 * This object owns copied token strings. It is not durable storage and does
 * not perform browser interaction; callers decide where to persist it and how
 * to resume interactive flows.
 */
typedef struct lonejson_oauth2_token_flow {
  /** Current Bearer access token, when known. */
  char *access_token;
  /** Token type, normally `Bearer`. */
  char *token_type;
  /** Refresh token used for transparent refresh, when available. */
  char *refresh_token;
  /** Granted or narrowed scope, when returned by the provider. */
  char *scope;
  /** OIDC ID token, when returned by the provider. */
  char *id_token;
  /** Access-token expiry time in seconds since Unix epoch. */
  lonejson_int64 expires_at;
  /** Non-zero when `expires_at` is meaningful. */
  int has_expires_at;
} lonejson_oauth2_token_flow;

/** Token-flow state returned by `lonejson_oauth2_token_flow_ensure()`. */
typedef enum lonejson_oauth2_token_flow_state {
  /** Access token was already usable under the configured policy. */
  LONEJSON_OAUTH2_TOKEN_FLOW_READY = 0,
  /** Access token was refreshed through the runtime HTTP provider. */
  LONEJSON_OAUTH2_TOKEN_FLOW_REFRESHED = 1,
  /** The helper cannot continue without caller/user interaction. */
  LONEJSON_OAUTH2_TOKEN_FLOW_NEEDS_INTERACTION = 2,
  /** Refresh was attempted and failed. Inspect the returned status/error. */
  LONEJSON_OAUTH2_TOKEN_FLOW_FAILED = 3
} lonejson_oauth2_token_flow_state;

/** Policy for ensuring an OAuth2/OIDC token flow.
 *
 * Zero-initialized fields use conservative defaults. `token_endpoint`,
 * `client_id`, and `client_secret` are required only when refresh may be
 * attempted.
 */
typedef struct lonejson_oauth2_token_flow_policy {
  /** OAuth2 token endpoint used for refresh. Required for refresh. */
  const char *token_endpoint;
  /** OAuth2 client identifier used for refresh. Required for refresh. */
  const char *client_id;
  /** OAuth2 client secret used for refresh when required by the provider. */
  const char *client_secret;
  /** Optional scope narrowing for refresh requests. */
  const char *scope;
  /** Current time in seconds since Unix epoch. Required. */
  lonejson_int64 now;
  /** Refresh before expiry by this many seconds. Zero means 60 seconds. */
  lonejson_int64 refresh_skew_seconds;
  /** Maximum token endpoint response bytes. Zero means the default limit. */
  size_t max_response_bytes;
  /** Additional retries after the first refresh attempt. Zero means 2. */
  unsigned max_retries;
  /** Non-zero disables transparent refresh. */
  int disable_refresh;
  /** Non-zero disables retry; exactly one refresh attempt is made. */
  int disable_retry;
} lonejson_oauth2_token_flow_policy;

/** Observable result from token-flow ensure. */
typedef struct lonejson_oauth2_token_flow_result {
  lonejson_oauth2_token_flow_state state;
  /** Number of token endpoint attempts performed by this call. */
  unsigned attempts;
  /** Non-zero when `flow` was updated from a refresh response. */
  int refreshed;
} lonejson_oauth2_token_flow_result;

/** Parsed OAuth2 token introspection response.
 *
 * The response owns all strings. `active` is required by RFC 7662. Other
 * fields are optional provider facts; applications still own authorization.
 */
typedef struct lonejson_oauth2_introspection_response {
  /** RFC 7662 active flag. Required in valid introspection responses. */
  int active;
  /** Non-zero when `active` was present. */
  int has_active;
  /** Optional space-delimited scope string. */
  char *scope;
  /** Optional client identifier associated with the token. */
  char *client_id;
  /** Optional provider username associated with the token. */
  char *username;
  /** Optional token type. */
  char *token_type;
  /** Optional subject claim. */
  char *sub;
  /** Optional audience value retained as a string when present. */
  char *aud;
  /** Optional issuer value. */
  char *iss;
  /** Optional token identifier. */
  char *jti;
  /** Optional expiration time seconds since Unix epoch. */
  lonejson_int64 exp;
  /** Non-zero when `exp` was present. */
  int has_exp;
  /** Optional issued-at time seconds since Unix epoch. */
  lonejson_int64 iat;
  /** Non-zero when `iat` was present. */
  int has_iat;
  /** Optional not-before time seconds since Unix epoch. */
  lonejson_int64 nbf;
  /** Non-zero when `nbf` was present. */
  int has_nbf;
} lonejson_oauth2_introspection_response;

/** OIDC UserInfo request options. */
typedef struct lonejson_oidc_userinfo_request {
  /** Bearer access token. Required. */
  const char *access_token;
  /** Maximum JSON response bytes. Zero means the default limit. */
  size_t max_response_bytes;
} lonejson_oidc_userinfo_request;

/** Parsed OIDC UserInfo response.
 *
 * The helper validates that the response is bounded JSON and retains the exact
 * JSON bytes. Common claims are copied when present; provider-specific claims
 * remain available in `json`.
 */
typedef struct lonejson_oidc_userinfo_response {
  /** Exact bounded JSON response bytes retained for provider-specific claims.
   */
  char *json;
  /** Number of bytes in `json`. */
  size_t len;
  /** Optional subject claim. */
  char *sub;
  /** Optional display name claim. */
  char *name;
  /** Optional preferred username claim. */
  char *preferred_username;
  /** Optional email claim. */
  char *email;
  /** Optional email verification flag. */
  int email_verified;
  /** Non-zero when `email_verified` was present. */
  int has_email_verified;
} lonejson_oidc_userinfo_response;

/** Generated OIDC/OAuth2 PKCE verifier and S256 challenge pair.
 *
 * Both strings are lonejson-owned and must be released with
 * `lonejson_oidc_pkce_cleanup()`.
 */
typedef struct lonejson_oidc_pkce {
  char *code_verifier;
  char *code_challenge;
} lonejson_oidc_pkce;

/** OIDC/OAuth2 authorization-code request URL inputs.
 *
 * The helper builds an HTTPS authorization URL with PKCE S256 parameters.
 * `authorization_endpoint`, `client_id`, `redirect_uri`, `state`, `nonce`, and
 * `code_challenge` are required. Fragment-bearing endpoints are rejected so
 * generated OAuth parameters cannot be placed after `#`.
 */
typedef struct lonejson_oidc_authorization_request {
  /** HTTPS authorization endpoint. Required and must not contain a fragment. */
  const char *authorization_endpoint;
  /** OAuth2 client identifier. Required. */
  const char *client_id;
  /** Redirect URI registered with the authorization server. Required. */
  const char *redirect_uri;
  /** Optional OAuth2/OIDC scope string. */
  const char *scope;
  /** Opaque CSRF state value. Required. */
  const char *state;
  /** OIDC nonce value. Required. */
  const char *nonce;
  /** PKCE S256 code challenge. Required. */
  const char *code_challenge;
  /** Optional provider-specific audience parameter. */
  const char *audience;
  /** Optional RFC 8707 resource indicator. */
  const char *resource;
  /** Maximum generated URL bytes. Zero means the default limit. */
  size_t max_url_bytes;
} lonejson_oidc_authorization_request;

/** Parsed authorization-code callback query.
 *
 * Returned strings are lonejson-owned. Parsing rejects duplicate known fields,
 * malformed percent encoding, decoded NUL bytes, provider error callbacks, and
 * state mismatches.
 */
typedef struct lonejson_oidc_authorization_callback {
  /** Authorization code returned by the provider on success. */
  char *code;
  /** State value returned by the provider. */
  char *state;
  /** OAuth2/OIDC error code for error callbacks. */
  char *error;
  /** Optional provider diagnostic for `error`. */
  char *error_description;
  /** Optional provider documentation URI for `error`. */
  char *error_uri;
} lonejson_oidc_authorization_callback;

/** Framework-neutral bearer-token authentication failure class. */
typedef enum lonejson_auth_failure {
  LONEJSON_AUTH_FAILURE_NONE = 0,
  LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS,
  LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN,
  LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE,
  LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND,
  LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE,
  LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN,
  LONEJSON_AUTH_FAILURE_NOT_YET_VALID,
  LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH,
  LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH,
  LONEJSON_AUTH_FAILURE_CLAIMS_INVALID
} lonejson_auth_failure;

/** Server-side bearer-token validation inputs.
 *
 * This helper does not fetch keys, write responses, route requests, or depend
 * on any framework. Callers supply the HTTP Authorization header value, a
 * fresh JWKS cache policy, a JWKS cache previously filled by caller-owned
 * network code, and an explicit JWT claim policy.
 */
typedef struct lonejson_oidc_bearer_validation_request {
  /** Raw HTTP Authorization header value. Required. */
  const char *authorization_header;
  /** Fresh caller-owned JWKS cache. Required. */
  const lonejson_oidc_jwks_cache *jwks_cache;
  /** Policy matching `jwks_cache`. Required. */
  const lonejson_oidc_jwks_cache_policy *jwks_policy;
  /** Explicit JWT claim policy. Required. */
  const lonejson_jwt_claim_policy *claim_policy;
} lonejson_oidc_bearer_validation_request;

/** Server-side bearer-token validation result.
 *
 * On success, `header` and `claims` contain validated JWT data and `jwk`
 * points into the caller-owned JWKS cache. On failure, `failure` classifies the
 * denial and the remaining fields are cleared.
 */
typedef struct lonejson_oidc_bearer_validation {
  /** Failure classification, or `LONEJSON_AUTH_FAILURE_NONE` on success. */
  lonejson_auth_failure failure;
  /** Validated JOSE header on success. */
  lonejson_jwt_header header;
  /** Validated JWT claims on success. */
  lonejson_jwt_claims claims;
  /** Selected JWK inside the caller-owned JWKS cache on success. */
  const lonejson_jwk *jwk;
} lonejson_oidc_bearer_validation;

#endif
/** Callback invoked after one push-fed selected array item has been parsed into
 * `dst`. By default the push stream cleans up and reuses `dst` after the
 * callback returns, so retaining an item requires a copy. With per-stream
 * record reuse enabled, the caller owns reset and cleanup instead.
 */
typedef lonejson_status (*lonejson_array_stream_item_fn)(void *user, void *dst);
/** Callback invoked after one push-fed selected string-array item has been
 * decoded into a bounded, NUL-terminated temporary buffer. The pointer is valid
 * only for the duration of the callback.
 */
typedef lonejson_status (*lonejson_array_stream_string_fn)(
    void *user, const char *data, size_t len, lonejson_error *error);
/** Incremental Server-Sent Events parser receiver. */
typedef struct lonejson_sse lonejson_sse;
/** Incremental MIME multipart parser receiver. */
typedef struct lonejson_multipart lonejson_multipart;
/** Pull-style JSON generator state. */
typedef struct lonejson_generator lonejson_generator;
/** Streaming JSON writer state.
 *
 * A writer owns JSON syntax for dynamically shaped documents: object and array
 * delimiters, commas, keys, string escaping, scalar emission, and final-state
 * validation. Initialize it with `lonejson_writer_init_sink()` for push/sink
 * output, or use `lonejson_writer_generator_init()` when a pull-style
 * transport should drive the same writer events through
 * `lonejson_generator_read()`.
 */
typedef struct lonejson_writer lonejson_writer;
/** Public state for a push-fed arbitrary JSON value inside a writer.
 *
 * If you use the receiver-method surface (`stream.open(...)`), initialize the
 * struct first with `lonejson_writer_value_stream_init()` or
 * `LONEJSON_WRITER_VALUE_STREAM_INIT`. Plain zero-initialization remains valid
 * when you only use the free functions such as
 * `lonejson_writer_value_stream_open(...)`. The `state` pointer is owned by
 * lonejson after a successful open call. Feed chunks with `push()`, validate
 * EOF and return the writer to normal use with `close()`, and release any
 * remaining stream resources with `cleanup()`.
 */
typedef struct lonejson_writer_value_stream lonejson_writer_value_stream;

/** Static initializer for one writer value stream receiver. */
#define LONEJSON_WRITER_VALUE_STREAM_INIT                                      \
  {NULL,                                                                       \
   {0},                                                                        \
   lonejson_writer_value_stream_open,                                          \
   lonejson_writer_value_stream_push,                                          \
   lonejson_writer_value_stream_close,                                         \
   lonejson_writer_value_stream_cleanup}

/* lonejson runtime is defined later in the header once callback and helper
 * object types are available.
 */

/** Runtime metadata describing one mapped JSON field. Applications normally
 * use the `LONEJSON_FIELD_*` macros instead of writing this struct manually. */
struct lonejson_field {
  /** JSON object key matched against incoming data and emitted on serialize. */
  const char *json_key;
  /** Cached byte length of `json_key`. */
  size_t json_key_len;
  /** Cached first key byte used by the fast field matcher. */
  unsigned char json_key_first;
  /** Cached last key byte used by the fast field matcher. */
  unsigned char json_key_last;
  /** `offsetof(struct_type, member)` for the mapped destination member. */
  size_t struct_offset;
  /** Field category selected by the mapping macro. */
  lonejson_field_kind kind;
  /** Fixed versus dynamically allocated destination storage. */
  lonejson_storage_kind storage;
  /** Overflow policy for fixed-capacity strings and arrays. */
  lonejson_overflow_policy overflow_policy;
  /** Additional field flags such as `LONEJSON_FIELD_REQUIRED`. */
  unsigned flags;
  /** Fixed-capacity byte count for fixed strings, otherwise zero. */
  size_t fixed_capacity;
  /** Element size for object/number arrays when applicable. */
  size_t elem_size;
  /** Nested object map for object and object-array fields. */
  const lonejson_map *submap;
  /** Reserved internal policy slot. Public callers should select one named
   * runtime spool class through `LONEJSON_FIELD_*_STREAM_CLASS`.
   */
  const void *reserved_policy;
  /** Optional `int` presence flag offset for presence-gated primitive fields.
   * Meaningful only when `flags` contains `LONEJSON_FIELD_HAS_PRESENCE`.
   */
  size_t presence_offset;
  /** Named spool policy for streamed text and base64 fields. */
  lonejson_spool_class spool_class;
};

#define LONEJSON__KEY_LEN(key) (sizeof(key) - 1u)
#define LONEJSON__KEY_FIRST(key)                                               \
  ((unsigned char)((LONEJSON__KEY_LEN(key) != 0u) ? (key)[0] : '\0'))
#define LONEJSON__KEY_LAST(key)                                                \
  ((unsigned char)((LONEJSON__KEY_LEN(key) != 0u)                              \
                       ? (key)[LONEJSON__KEY_LEN(key) - 1u]                    \
                       : '\0'))
#define LONEJSON__MAP_COOKIE_VALUE                                             \
  ((((lonejson_uint64)0x4c4a4d41u) << 32) | (lonejson_uint64)0x50434143u)
#define LONEJSON_MAP_COOKIE_VALUE LONEJSON__MAP_COOKIE_VALUE
#define LONEJSON__MAP_STATIC_COOKIE(line, struct_size, field_count)            \
  (LONEJSON__MAP_COOKIE_VALUE ^                                                \
   (((lonejson_uint64)(line) & (lonejson_uint64)0xffffu) << 48u) ^             \
   (((lonejson_uint64)(struct_size) & (lonejson_uint64)0xffffffu) << 20u) ^    \
   (((lonejson_uint64)(field_count) & (lonejson_uint64)0xfffffu)))

/** Top-level schema description used for mapped parse and serialize
 * operations. */
struct lonejson_map {
  /** Human-readable schema name, typically the C struct type name. */
  const char *name;
  /** Size in bytes of the mapped destination struct. */
  size_t struct_size;
  /** Field table built from `LONEJSON_FIELD_*` macro entries. */
  const lonejson_field *fields;
  /** Number of entries in `fields`. */
  size_t field_count;
  /** Opaque self-identity used to recognize cacheable singleton maps. */
  const struct lonejson_map *_map_identity;
  /** Opaque internal schema cookie populated by `LONEJSON_MAP_DEFINE`. */
  lonejson_uint64 _map_cookie;
};

/** ABI version for public borrowed schema and record view structs. */
#define LONEJSON_VIEW_ABI_VERSION 1u

/** Lua-agnostic borrowed view of a lonejson schema owned by another API layer.
 *
 * Callers must initialize `size` to `sizeof(lonejson_schema_view)` and
 * `abi_version` to `LONEJSON_VIEW_ABI_VERSION` before passing the view to an
 * adapter API. The returned pointers are borrowed from the owning layer and do
 * not transfer ownership. They remain valid only under that layer's documented
 * lifetime rules.
 */
typedef struct lonejson_schema_view {
  size_t size;
  unsigned int abi_version;
  lonejson *runtime;
  const lonejson_map *map;
  size_t record_size;
  unsigned int flags;
} lonejson_schema_view;

/** Lua-agnostic borrowed view of a mapped record owned by another API layer.
 *
 * Callers must initialize `size` to `sizeof(lonejson_record_view)` and
 * `abi_version` to `LONEJSON_VIEW_ABI_VERSION` before passing the view as an
 * output parameter. `record` is borrowed and remains valid only under the
 * owning layer's documented lifetime rules. Callers must not free, resize, or
 * retain the raw `record` pointer beyond that lifetime.
 */
typedef struct lonejson_record_view {
  size_t size;
  unsigned int abi_version;
  lonejson_schema_view schema;
  void *record;
  unsigned int flags;
} lonejson_record_view;

/** Optional controls for mapped parsing and streaming. Use
 * `lonejson__default_parse_options()` instead of manual zeroing so new fields
 * keep their intended defaults.
 */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
typedef struct lonejson__parse_options {
  /** When non-zero, lonejson initializes the destination before parsing. */
  int clear_destination;
  /** When non-zero, duplicate object keys cause
   * `LONEJSON_STATUS_DUPLICATE_FIELD`.
   */
  int reject_duplicate_keys;
  /** Maximum allowed nesting depth before parsing fails with overflow. */
  size_t max_depth;
  /** Maximum decoded byte length accepted for one dynamically allocated mapped
   * string field. Zero disables this ceiling.
   */
  size_t max_dynamic_string_bytes;
  /** Maximum lonejson-owned heap bytes one parse call may keep live at once
   * across parser-created dynamic fields and parse-time owned buffers. Zero
   * disables this ceiling. Bytes spilled to spool files do not count.
   */
  size_t max_alloc_bytes;
  /** Optional caller-owned scratch used to stage reused `STRING_FIXED` fields
   * while parsing with `clear_destination = 0`. When this scratch is large
   * enough for the field's fixed capacity, lonejson uses it instead of
   * allocating temporary parse-owned staging storage.
   */
  void *fixed_string_scratch;
  /** Byte size of `fixed_string_scratch`. Zero disables caller-provided fixed
   * string staging scratch.
   */
  size_t fixed_string_scratch_size;
  /** Optional allocator used for parser-owned runtime state and lonejson-owned
   * field allocations created while parsing.
   */
  const lonejson_allocator *allocator;
} lonejson__parse_options;
#endif

struct lonejson_json_value;

/** Header name/value pair exposed while processing multipart part headers.
 * Pointers are valid only for the duration of the callback currently using
 * them.
 */
typedef struct lonejson_header {
  /** Header field name. */
  char *name;
  /** Header field value after the separating colon. */
  char *value;
} lonejson_header;

/** Limits and allocator selection for incremental SSE parsing. Zero limit
 * fields use the library defaults. `lonejson_sse_push` streams `data:` field
 * bytes to the caller as lines arrive; `lonejson_sse_push_json` streams
 * selected `data:` fields into the JSON parser as lines arrive.
 */
typedef struct lonejson_sse_options {
  /** Maximum bytes accepted in one physical SSE line. Default: 64 KiB. */
  size_t max_line_bytes;
  /** Maximum concatenated `data:` bytes accepted for one plain SSE event.
   * Default: 1 MiB. `lonejson_sse_push` counts these bytes while streaming
   * them instead of retaining the complete event body. */
  size_t max_event_data_bytes;
  /** Maximum bytes retained in each parser-owned metadata buffer. Default: 1
   * MiB. */
  size_t max_buffered_bytes;
  /** Optional allocator used for parser-owned buffers. */
  const lonejson_allocator *allocator;
} lonejson_sse_options;

/** Metadata for one Server-Sent Event. Field pointers are valid only during
 * the callback currently receiving them. `data_len` is the number of streamed
 * data bytes for the event, including SSE-inserted newlines between multiple
 * `data:` fields.
 */
typedef struct lonejson_sse_event {
  /** Optional SSE `event:` field, or `NULL` when the event has no type. */
  const char *event;
  /** Optional SSE `id:` field, or `NULL` when the event has no id. */
  const char *id;
  /** Number of streamed `data:` bytes delivered for this event. */
  size_t data_len;
  /** Parsed SSE `retry:` value in milliseconds when `has_retry` is non-zero. */
  unsigned long retry_ms;
  /** Non-zero when the event supplied a valid `retry:` field. */
  int has_retry;
} lonejson_sse_event;

/** Streaming callbacks for `lonejson_sse_push`. `begin_event` is called before
 * the first data chunk, or at dispatch for metadata-only events; its metadata
 * reflects fields parsed so far. `end_event` receives the final metadata for
 * the dispatched event.
 */
typedef struct lonejson_sse_handler {
  /** Called before data delivery for an event, or at dispatch for metadata-only
   * events. */
  lonejson_status (*begin_event)(void *user, const lonejson_sse_event *event,
                                 lonejson_error *error);
  /** Delivers one streamed chunk of concatenated SSE event data. */
  lonejson_status (*data_chunk)(void *user, const void *bytes, size_t len,
                                lonejson_error *error);
  /** Called after all data chunks for one event have been delivered. */
  lonejson_status (*end_event)(void *user, const lonejson_sse_event *event,
                               lonejson_error *error);
} lonejson_sse_handler;

/** Controls JSON decoding for SSE events. When `event_names` is non-NULL and
 * `event_name_count` is non-zero, only matching event names are decoded.
 * Events without an explicit SSE `event:` field match the empty string. For
 * filtered streaming, the `event:` field must arrive before the first `data:`
 * field in that event.
 */
typedef struct lonejson_sse_json_options {
  /** Optional allow-list of event names to decode as JSON. */
  const char *const *event_names;
  /** Number of entries in `event_names`. */
  size_t event_name_count;
} lonejson_sse_json_options;

/** Called after a selected SSE event's streamed JSON data has been decoded.
 * `event->data_len` is the number of JSON data bytes streamed to the parser,
 * including SSE-inserted newlines between multiple `data:` fields.
 */
typedef lonejson_status (*lonejson_sse_json_event_fn)(
    void *user, const lonejson_sse_event *event, void *dst,
    lonejson_error *error);

/** Called after a selected SSE event's streamed JSON data has been decoded
 * into one configured `lonejson_json_value` handle. Prepare the handle with
 * `lonejson_json_value_set_parse_sink()`,
 * `lonejson_json_value_set_parse_visitor()`, or
 * `lonejson_json_value_enable_parse_capture()` before starting the SSE stream.
 * The same handle is reused for each selected event; lonejson preserves the
 * configured parse mode and clears only the per-event runtime payload between
 * events. `event->data_len` is the number of JSON data bytes streamed to the
 * parser, including SSE-inserted newlines between multiple `data:` fields.
 */
typedef lonejson_status (*lonejson_sse_json_value_event_fn)(
    void *user, const lonejson_sse_event *event,
    struct lonejson_json_value *value, lonejson_error *error);

/** Limits and allocator selection for incremental multipart parsing. Zero
 * limit fields use the library defaults. Parts with `Content-Length` stream
 * body bytes directly to `part_data` after headers are parsed; parts without
 * `Content-Length` stream body bytes while retaining only bounded boundary
 * lookahead.
 */
typedef struct lonejson_multipart_options {
  /** Maximum accepted boundary length. Default: 200 bytes. */
  size_t max_boundary_bytes;
  /** Maximum bytes accepted in one header line. Default: 64 KiB. */
  size_t max_header_line_bytes;
  /** Maximum number of headers retained for one part. Default: 64. */
  size_t max_header_count;
  /** Maximum bytes buffered for one part without Content-Length. Default: 1
   * MiB. */
  size_t max_part_buffered_bytes;
  /** Optional allocator used for parser-owned buffers. */
  const lonejson_allocator *allocator;
} lonejson_multipart_options;

/** Metadata for the current multipart part. Pointers are valid only for the
 * callback currently using them.
 */
typedef struct lonejson_multipart_part {
  /** Optional parsed `name` parameter from Content-Disposition. */
  const char *name;
  /** Optional part Content-Type value. */
  const char *content_type;
  /** Parsed Content-Length, or `-1` when the part has no length header. */
  lonejson_int64 content_length;
  /** Retained part headers. */
  const lonejson_header *headers;
  /** Number of entries in `headers`. */
  size_t header_count;
} lonejson_multipart_part;

/** Streaming callbacks for `lonejson_multipart_push`. Part header metadata is
 * available in `begin_part` before body bytes are delivered. Each callback may
 * return a non-OK status to stop parsing.
 */
typedef struct lonejson_multipart_handler {
  /** Called after a part's headers have been parsed and before body bytes. */
  lonejson_status (*begin_part)(void *user, const lonejson_multipart_part *part,
                                lonejson_error *error);
  /** Delivers one streamed chunk of the current part body. */
  lonejson_status (*part_data)(void *user, const void *bytes, size_t len,
                               lonejson_error *error);
  /** Called after the current part body is complete. */
  lonejson_status (*end_part)(void *user, const lonejson_multipart_part *part,
                              lonejson_error *error);
} lonejson_multipart_handler;

/** Controls how a `lonejson_json_value` field receives inbound parsed JSON.
 * Parse is stream-first by default; callers must deliberately choose one of
 * these modes before decoding into a `JSON_VALUE` field.
 */
typedef enum lonejson_json_value_parse_mode {
  /** Parsing a `JSON_VALUE` field fails unless the caller configures a parse
   * sink or enables explicit capture first.
   */
  LONEJSON_JSON_VALUE_PARSE_NONE = 0,
  /** Parsing streams compact JSON bytes incrementally to a caller sink. */
  LONEJSON_JSON_VALUE_PARSE_SINK = 1,
  /** Parsing streams structured JSON visitor events incrementally. */
  LONEJSON_JSON_VALUE_PARSE_VISITOR = 2,
  /** Parsing captures compact JSON bytes into owned storage. */
  LONEJSON_JSON_VALUE_PARSE_CAPTURE = 3,
  /** Parsing streams structured JSON visitor events with decoded path context
   * incrementally.
   */
  LONEJSON_JSON_VALUE_PARSE_PATH_VISITOR = 4
} lonejson_json_value_parse_mode;

/** Internal JSON-value visitor limit layout retained for lonejson's
 * implementation. Public callers configure these ceilings through
 * `lonejson_config`.
 */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
typedef struct lonejson__value_limits {
  /** Maximum allowed nesting depth while parsing a value. */
  size_t max_depth;
  /** Maximum decoded byte length for one JSON string value. */
  size_t max_string_bytes;
  /** Maximum raw byte length for one JSON number token. */
  size_t max_number_bytes;
  /** Maximum decoded byte length for one object key. */
  size_t max_key_bytes;
  /** Maximum raw byte length consumed while parsing the value. Zero means
   * unlimited.
   */
  size_t max_total_bytes;
} lonejson__value_limits;
#endif

/** Callback signature for structure and boundary events during arbitrary JSON
 * value visiting.
 */
typedef lonejson_status (*lonejson_value_event_fn)(void *user,
                                                   lonejson_error *error);
/** Callback signature for chunked decoded text delivery during arbitrary JSON
 * value visiting.
 */
typedef lonejson_status (*lonejson_value_chunk_fn)(void *user, const char *data,
                                                   size_t len,
                                                   lonejson_error *error);
/** Callback signature for boolean scalar delivery during arbitrary JSON value
 * visiting.
 */
typedef lonejson_status (*lonejson_value_bool_fn)(void *user, int value,
                                                  lonejson_error *error);

/** One decoded path segment in a parser-owned arbitrary JSON value path.
 *
 * Object member segments are decoded UTF-8 key bytes. Array item segments are
 * decimal index bytes. Segment storage is valid only for the duration of the
 * callback receiving the enclosing `lonejson_value_path`.
 */
typedef struct lonejson_path_segment {
  const char *data;
  size_t len;
} lonejson_path_segment;

/** Current normalized location while visiting one arbitrary JSON value.
 *
 * The root value is represented by `segment_count == 0`. The path is supplied
 * as decoded segments rather than JSON Pointer text so callers can match
 * without reparsing or escaping a string form.
 */
typedef struct lonejson_value_path {
  const lonejson_path_segment *segments;
  size_t segment_count;
} lonejson_value_path;

/** Path-aware callback signature for structure and boundary events during
 * arbitrary JSON value visiting.
 */
typedef lonejson_status (*lonejson_path_value_event_fn)(
    void *user, const lonejson_value_path *path, lonejson_error *error);
/** Path-aware callback signature for chunked decoded text delivery during
 * arbitrary JSON value visiting.
 */
typedef lonejson_status (*lonejson_path_value_chunk_fn)(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error);
/** Path-aware callback signature for JSON boolean delivery during arbitrary
 * JSON value visiting.
 */
typedef lonejson_status (*lonejson_path_value_bool_fn)(
    void *user, const lonejson_value_path *path, int value,
    lonejson_error *error);

/** Handler invoked while a mapped string-array stream field is decoded.
 * `chunk` receives decoded UTF-8 string bytes and may be called more than once
 * per item. `end` is called only after the string item is complete; the JSON
 * array and enclosing document are still validated before parse completion.
 */
typedef struct lonejson_array_stream_string_handler {
  /** Called before chunks for one decoded string item are delivered. */
  lonejson_value_event_fn begin;
  /** Delivers one decoded UTF-8 chunk for the current string item. */
  lonejson_value_chunk_fn chunk;
  /** Called after the current string item is complete. */
  lonejson_value_event_fn end;
} lonejson_array_stream_string_handler;

/** Destination member for `LONEJSON_FIELD_STRING_ARRAY_STREAM*` fields.
 *
 * This is a configured parse-field helper, not a standalone receiver handle.
 * Configure it with `lonejson_string_array_stream_set_handler()` before
 * parsing; lonejson preserves the configured callbacks across reset and
 * per-parse destination clearing, and only resets per-parse internal state.
 * The free-function setup
 * helper accepts either explicit `lonejson_string_array_stream_init()` output
 * or plain zeroed storage and auto-initializes the field when needed.
 */
typedef struct lonejson_string_array_stream {
  /** Configured callbacks used while parsing the string-array field. */
  lonejson_array_stream_string_handler handler;
  /** Opaque user pointer passed to `handler` callbacks. */
  void *user;
  /** Internal non-zero marker while a parse is inside this stream field. */
  int active;
  /** Internal state marker used by lonejson. Treat as opaque. */
  unsigned _lonejson_magic;
  /** Configures callbacks for this stream field. */
  lonejson_status (*set_handler)(
      struct lonejson_string_array_stream *stream,
      const lonejson_array_stream_string_handler *handler, void *user,
      lonejson_error *error);
} lonejson_string_array_stream;

/** Static initializer for one mapped string-array stream field helper. */
#define LONEJSON_STRING_ARRAY_STREAM_INIT                                      \
  {{NULL, NULL, NULL}, NULL, 0, 0u, lonejson_string_array_stream_set_handler}

/** Callback invoked after one mapped array-stream item has been parsed into
 * the configured reusable item destination. The destination is valid only for
 * the duration of the callback; lonejson cleans and reuses it before parsing
 * the next item.
 */
typedef lonejson_status (*lonejson_mapped_array_stream_item_fn)(
    void *user, void *item, lonejson_error *error);

/** Handler configured on a `lonejson_mapped_array_stream` field before
 * parsing. `item_map` must describe one object item and `item_dst` is reused
 * for each item.
 */
typedef struct lonejson_mapped_array_stream_handler {
  /** Map that describes one object item in the streamed array. */
  const lonejson_map *item_map;
  /** Reusable mapped destination populated before each item callback. */
  void *item_dst;
  /** Callback invoked after each item has been parsed into `item_dst`. */
  lonejson_mapped_array_stream_item_fn item;
  /** Opaque user pointer passed to `item`. */
  void *user;
} lonejson_mapped_array_stream_handler;

/** Destination member for `LONEJSON_FIELD_MAPPED_ARRAY_STREAM*` fields.
 *
 * This streams mapped object items from a JSON array field during normal mapped
 * parsing. Item callbacks are invoked in source JSON order. Sibling fields
 * that appear later in the containing object have not been parsed yet when an
 * earlier streamed array item is delivered. If callbacks need envelope or
 * parent metadata, serialize those fields before the streamed array field or
 * defer that work until the containing parse has completed.
 *
 * The selected array and enclosing document are not materialized. Each item is
 * parsed into the configured reusable destination, delivered to the callback,
 * then cleaned before the next item. Item maps may themselves contain mapped
 * array-stream fields, so nested streamed arrays compose through normal maps.
 *
 * This is a configured parse-field helper, not a standalone receiver handle.
 * Configure it with `lonejson_mapped_array_stream_set_handler()` before
 * parsing. The free-function setup helper accepts either explicit
 * `lonejson_mapped_array_stream_init()` output or plain zeroed storage and
 * auto-initializes the field when needed.
 */
typedef struct lonejson_mapped_array_stream {
  /** Configured item map, reusable destination, callback, and user pointer. */
  lonejson_mapped_array_stream_handler handler;
  /** Internal non-zero marker while a parse is inside this stream field. */
  int active;
  /** Internal state marker used by lonejson. Treat as opaque. */
  unsigned _lonejson_magic;
  /** Configures callbacks and reusable item storage for this stream field. */
  lonejson_status (*set_handler)(
      struct lonejson_mapped_array_stream *stream,
      const lonejson_mapped_array_stream_handler *handler,
      lonejson_error *error);
} lonejson_mapped_array_stream;

/** Static initializer for one mapped object-array stream field helper. */
#define LONEJSON_MAPPED_ARRAY_STREAM_INIT                                      \
  {{NULL, NULL, NULL, NULL}, 0, 0u, lonejson_mapped_array_stream_set_handler}

/** Visitor callbacks for one arbitrary JSON value. String values and object
 * keys are delivered as decoded UTF-8 in chunks. Number values are delivered as
 * raw token bytes in chunks. Any callback may be `NULL` when the caller does
 * not need that event. Use `lonejson_default_value_visitor()` instead of
 * manual zeroing when you want the empty visitor state. The callback sequence
 * is balanced:
 * `object_begin/object_end`, `array_begin/array_end`, and matching
 * begin/chunk/end triplets for keys, strings, and numbers.
 */
typedef struct lonejson_value_visitor {
  /** Called when an object begins. */
  lonejson_value_event_fn object_begin;
  /** Called when an object ends. */
  lonejson_value_event_fn object_end;
  /** Called before chunks for an object key are delivered. */
  lonejson_value_event_fn object_key_begin;
  /** Delivers one decoded UTF-8 object-key chunk. */
  lonejson_value_chunk_fn object_key_chunk;
  /** Called after all chunks for an object key have been delivered. */
  lonejson_value_event_fn object_key_end;
  /** Called when an array begins. */
  lonejson_value_event_fn array_begin;
  /** Called when an array ends. */
  lonejson_value_event_fn array_end;
  /** Called before chunks for a string value are delivered. */
  lonejson_value_event_fn string_begin;
  /** Delivers one decoded UTF-8 string chunk. */
  lonejson_value_chunk_fn string_chunk;
  /** Called after all chunks for a string value have been delivered. */
  lonejson_value_event_fn string_end;
  /** Called before chunks for a number token are delivered. */
  lonejson_value_event_fn number_begin;
  /** Delivers one raw JSON number-token chunk. */
  lonejson_value_chunk_fn number_chunk;
  /** Called after all chunks for a number token have been delivered. */
  lonejson_value_event_fn number_end;
  /** Delivers one JSON boolean value as non-zero for `true` and zero for
   * `false`. */
  lonejson_value_bool_fn boolean_value;
  /** Called for a JSON `null` value. */
  lonejson_value_event_fn null_value;
} lonejson_value_visitor;

/** Path-aware visitor callbacks for one arbitrary JSON value. String values
 * and object keys are delivered as decoded UTF-8 in chunks. Number values are
 * delivered as raw token bytes in chunks. Any callback may be `NULL` when the
 * caller does not need that event. Use
 * `lonejson_default_path_value_visitor()` instead of manual zeroing when you
 * want the empty visitor state.
 *
 * The callback sequence is balanced like `lonejson_value_visitor`. Object-key
 * begin/chunk/end callbacks receive the parent object path because the member
 * value path is entered only after the key and colon have been parsed.
 * Structure begin/end callbacks receive the container value path. Scalar
 * begin/chunk/end callbacks receive the scalar value path, and every chunk for
 * one scalar receives the same path.
 */
typedef struct lonejson_path_value_visitor {
  /** Called when an object begins at `path`. */
  lonejson_path_value_event_fn object_begin;
  /** Called when an object ends at `path`. */
  lonejson_path_value_event_fn object_end;
  /** Called before chunks for an object key are delivered at the parent path.
   */
  lonejson_path_value_event_fn object_key_begin;
  /** Delivers one decoded UTF-8 object-key chunk at the parent path. */
  lonejson_path_value_chunk_fn object_key_chunk;
  /** Called after all chunks for an object key at the parent path. */
  lonejson_path_value_event_fn object_key_end;
  /** Called when an array begins at `path`. */
  lonejson_path_value_event_fn array_begin;
  /** Called when an array ends at `path`. */
  lonejson_path_value_event_fn array_end;
  /** Called before chunks for a string value are delivered at `path`. */
  lonejson_path_value_event_fn string_begin;
  /** Delivers one decoded UTF-8 string chunk at `path`. */
  lonejson_path_value_chunk_fn string_chunk;
  /** Called after all chunks for a string value at `path`. */
  lonejson_path_value_event_fn string_end;
  /** Called before chunks for a number token are delivered at `path`. */
  lonejson_path_value_event_fn number_begin;
  /** Delivers one raw JSON number-token chunk at `path`. */
  lonejson_path_value_chunk_fn number_chunk;
  /** Called after all chunks for a number token at `path`. */
  lonejson_path_value_event_fn number_end;
  /** Delivers one JSON boolean value at `path` as non-zero for `true` and zero
   * for `false`.
   */
  lonejson_path_value_bool_fn boolean_value;
  /** Called for a JSON `null` value at `path`. */
  lonejson_path_value_event_fn null_value;
} lonejson_path_value_visitor;

struct lonejson_json_value;
typedef struct lonejson_json_value_methods {
  void (*reset)(struct lonejson_json_value *value);
  void (*cleanup)(struct lonejson_json_value *value);
  lonejson_status (*set_buffer)(struct lonejson_json_value *value,
                                const void *data, size_t len,
                                lonejson_error *error);
  lonejson_status (*set_parse_sink)(struct lonejson_json_value *value,
                                    lonejson_sink_fn sink, void *user,
                                    lonejson_error *error);
  lonejson_status (*set_parse_visitor)(struct lonejson_json_value *value,
                                       const lonejson_value_visitor *visitor,
                                       void *user, lonejson_error *error);
  lonejson_status (*set_parse_path_visitor)(
      struct lonejson_json_value *value,
      const lonejson_path_value_visitor *visitor, void *user,
      lonejson_error *error);
  lonejson_status (*enable_parse_capture)(struct lonejson_json_value *value,
                                          lonejson_error *error);
  lonejson_status (*set_reader)(struct lonejson_json_value *value,
                                lonejson_reader_fn reader, void *user,
                                lonejson_error *error);
  lonejson_status (*set_file)(struct lonejson_json_value *value, FILE *fp,
                              lonejson_error *error);
  lonejson_status (*set_fd)(struct lonejson_json_value *value, int fd,
                            lonejson_error *error);
  lonejson_status (*set_path)(struct lonejson_json_value *value,
                              const char *path, lonejson_error *error);
  lonejson_status (*write_to_sink)(const struct lonejson_json_value *value,
                                   lonejson_sink_fn sink, void *user,
                                   lonejson_error *error);
  int (*is_rewindable)(const struct lonejson_json_value *value);
} lonejson_json_value_methods;

/** Opaque JSON value handle used by embedded arbitrary JSON fields. Parsing is
 * stream-first: the caller must configure either a parse sink, a parse
 * visitor, or explicit parse capture before decoding inbound JSON into this
 * field. Serialization can source one JSON value from memory, reader
 * callbacks, files, file descriptors, or reopenable paths.
 */
typedef struct lonejson_json_value {
  /** Current configured value kind. `LONEJSON_JSON_VALUE_NULL` emits `null`. */
  lonejson_json_value_kind kind;
  /** Owned compact JSON bytes when `kind == LONEJSON_JSON_VALUE_BUFFER`. */
  char *json;
  /** Logical byte length of `json`. */
  size_t len;
  /** Caller-provided reader callback when `kind == LONEJSON_JSON_VALUE_READER`.
   */
  lonejson_reader_fn reader;
  /** Caller-provided reader callback user data. */
  void *reader_user;
  /** Caller-provided file handle when `kind == LONEJSON_JSON_VALUE_FILE`. */
  FILE *fp;
  /** Caller-provided file descriptor when `kind == LONEJSON_JSON_VALUE_FD`. */
  int fd;
  /** Owned filesystem path when `kind == LONEJSON_JSON_VALUE_PATH`. */
  char *path;
  /** Parse-time delivery mode for inbound JSON values. */
  lonejson_json_value_parse_mode parse_mode;
  /** Caller-provided sink callback when `parse_mode ==
   * LONEJSON_JSON_VALUE_PARSE_SINK`.
   */
  lonejson_sink_fn parse_sink;
  /** Caller-provided sink callback user data. */
  void *parse_sink_user;
  /** Caller-provided visitor when `parse_mode ==
   * LONEJSON_JSON_VALUE_PARSE_VISITOR`.
   */
  const lonejson_value_visitor *parse_visitor;
  /** Caller-provided visitor user data. */
  void *parse_visitor_user;
  /** Caller-provided path-aware visitor when `parse_mode ==
   * LONEJSON_JSON_VALUE_PARSE_PATH_VISITOR`.
   */
  const lonejson_path_value_visitor *parse_path_visitor;
  /** Caller-provided path-aware visitor user data. */
  void *parse_path_visitor_user;
  /** Internal runtime-selected limits applied while parsing through a parse
   * visitor. Public callers configure these through `lonejson_config` and
   * should treat this storage as opaque.
   */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
  lonejson__value_limits parse_visitor_limits;
  lonejson__value_limits runtime_parse_visitor_limits;
#else
  /** Opaque storage for lonejson-managed parse-visitor limit state. */
  size_t _reserved_parse_visitor_limits[10];
#endif
  /** Receiver-owned method table. */
  const lonejson_json_value_methods *methods;
  /** Allocator used for lonejson-owned captured JSON and path state. */
  lonejson_allocator allocator;
  /** Whether `parse_visitor_limits` still follows the runtime defaults. */
  int parse_limits_follow_runtime;
  /** Internal state marker used by lonejson to distinguish initialized
   * handles from uninitialized memory. Treat as opaque.
   */
  unsigned _lonejson_magic;
} lonejson_json_value;

/** Options controlling serialization into buffers and sinks. Use
 * `lonejson__default_write_options()` instead of manual zeroing so new fields
 * keep their intended defaults.
 */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
typedef struct lonejson__write_options {
  /** Buffer overflow handling policy for fixed-capacity output buffers. */
  lonejson_overflow_policy overflow_policy;
  /** Non-zero to emit two-space-indented pretty JSON instead of compact JSON.
   */
  int pretty;
  /** Optional allocator used for serializer-owned output buffers. */
  const lonejson_allocator *allocator;
  /** Hard cap for serializer-owned output bytes, excluding the trailing NUL
   * terminator retained for C-string convenience. Zero keeps the library
   * default `LONEJSON_WRITE_MAX_OUTPUT_BYTES`; fixed-capacity buffer and sink
   * APIs ignore this field because their caller-provided sink is the bound.
   */
  size_t max_output_bytes;
} lonejson__write_options;
#endif

/** Owned serializer output buffer returned by alloc-returning APIs.
 *
 * Read `data` and `len`, then release the handle with
 * `lonejson_owned_buffer_free()`. Treat `alloc_size` and `allocator` as
 * internal cleanup state.
 */
typedef struct lonejson_owned_buffer {
  /** NUL-terminated serialized output bytes owned by this handle. */
  char *data;
  /** Logical byte length of `data`, excluding the trailing NUL. */
  size_t len;
  /** Internal allocation size used for symmetric cleanup. */
  size_t alloc_size;
  /** Allocator that owns `data`. */
  lonejson_allocator allocator;
} lonejson_owned_buffer;

#ifdef LONEJSON_WITH_OIDC
/** Bounded materialized HTTP request used by auth provider helpers.
 *
 * Providers must not retain pointers after the callback returns. `method`,
 * `url`, and `body` are caller-owned. `max_response_bytes == 0` means the
 * helper-specific default limit.
 */
struct lonejson_http_request {
  /** HTTP method such as `GET` or `POST`. */
  const char *method;
  /** Absolute HTTPS URL for the provider to request. */
  const char *url;
  /** Optional request content type for `body`. */
  const char *content_type;
  /** Optional Authorization header value. */
  const char *authorization;
  /** Optional User-Agent header value from the installed provider. */
  const char *user_agent;
  /** Optional request body bytes. */
  const void *body;
  /** Number of bytes in `body`. */
  size_t body_len;
  /** Maximum accepted response body bytes. Zero means helper default. */
  size_t max_response_bytes;
};

/** Bounded materialized HTTP response populated by auth HTTP providers. */
struct lonejson_http_response {
  /** HTTP status code returned by the provider. */
  long status_code;
  /** Optional response content type owned by this response. */
  char *content_type;
  /** Response body bytes owned by this response. */
  lonejson_owned_buffer body;
};

/** Auth HTTP provider vtable.
 *
 * The caller owns `user_data` and must keep it alive until no runtime using
 * this provider can call OIDC/OAuth2 HTTP helpers. This is the public
 * transport boundary for OIDC discovery, JWKS refresh, and OAuth2 token
 * exchange helpers. Use `lonejson_http_provider_init_simple()` with the
 * embedding application's HTTP client. In curl-enabled builds, curl remains an
 * application-owned transport: configure libcurl in your callback and set a
 * product-specific `user_agent` so identity providers can diagnose traffic.
 * Token-flow helpers may call this provider more than once for bounded retries
 * or refresh; provider callbacks should therefore be idempotent at the HTTP
 * request boundary and enforce their own TLS/proxy/redirect policy.
 */
struct lonejson_http_provider {
  /** Caller-owned provider context passed to `request`. */
  void *user_data;
  /** Optional default User-Agent copied into provider-backed requests. */
  const char *user_agent;
  /** Caller-owned HTTP transfer callback. */
  lonejson_status (*request)(void *user_data,
                             const lonejson_http_request *request,
                             lonejson_http_response *response,
                             lonejson_error *error);
};

#define LONEJSON_M2M_AUTH_BASIC (1u << 0)
#define LONEJSON_M2M_AUTH_BEARER (1u << 1)
#define LONEJSON_M2M_AUTH_DEFAULT                                              \
  (LONEJSON_M2M_AUTH_BASIC | LONEJSON_M2M_AUTH_BEARER)

/** Generated confidential-client/API-key credential material.
 *
 * `client_secret` and `api_key` are shown once to the caller according to the
 * requested auth modes. `record_json` contains only salts and hashes plus the
 * caller-supplied claim JSON and can be placed under a credential store's
 * `credentials` array. Store persistence, locking, rotation, revocation, and
 * audit policy are caller-owned.
 */
typedef struct lonejson_m2m_credential {
  /** Generated client identifier shown to the caller. */
  char *client_id;
  /** Generated one-time client secret when Basic auth is enabled. */
  char *client_secret;
  /** Generated one-time API key when Bearer auth is enabled. */
  char *api_key;
  /** Store-ready credential record containing only salts, hashes, and claim. */
  lonejson_owned_buffer record_json;
} lonejson_m2m_credential;

/** M2M/API-key credential generation inputs.
 *
 * `claim_json` is embedded as raw JSON after validation. Use
 * `LONEJSON_M2M_AUTH_BEARER` to generate an API-key-only credential for
 * `Authorization: Bearer <api_key>`, `LONEJSON_M2M_AUTH_BASIC` for
 * client-id/client-secret Basic auth, or both. `auth_modes == 0` uses
 * `LONEJSON_M2M_AUTH_DEFAULT`.
 */
typedef struct lonejson_m2m_credential_request {
  /** Caller-supplied JSON claim embedded into the generated record. */
  const char *claim_json;
  /** Number of bytes in `claim_json`. */
  size_t claim_len;
  /** Bitmask of `LONEJSON_M2M_AUTH_*`; zero uses the default modes. */
  unsigned auth_modes;
  /** Maximum generated record bytes. Zero means the default limit. */
  size_t max_record_bytes;
} lonejson_m2m_credential_request;

/** Caller-owned credential store JSON used by M2M verification helpers.
 *
 * The expected shape is an object with optional `credentials[]` and `signups[]`
 * arrays containing records returned by lonejson generation helpers. To revoke
 * or rotate, update this caller-owned JSON store: remove a record, set its
 * `revoked` field, or insert a replacement generated credential. The helper
 * output is deliberately store-ready JSON so applications can implement those
 * mutations with their own file, database, lock, and audit model.
 */
typedef struct lonejson_m2m_store {
  /** Credential-store JSON object bytes. */
  const char *json;
  /** Number of bytes in `json`. */
  size_t len;
  /** Maximum store JSON bytes. Zero means the default limit. */
  size_t max_store_bytes;
} lonejson_m2m_store;

/** M2M/API-key Authorization header verification inputs.
 *
 * `authorization_header` is the raw HTTP `Authorization` header value. Bearer
 * mode verifies `Authorization: Bearer <api_key>`. Basic mode verifies
 * `Authorization: Basic <base64(client_id:client_secret)>`.
 * `allowed_auth_modes == 0` accepts modes present in the credential record.
 */
typedef struct lonejson_m2m_verify_request {
  /** Caller-owned credential store. Required. */
  const lonejson_m2m_store *store;
  /** Raw HTTP Authorization header value. Required. */
  const char *authorization_header;
  /** Allowed `LONEJSON_M2M_AUTH_*` modes; zero uses record modes. */
  unsigned allowed_auth_modes;
} lonejson_m2m_verify_request;

/** M2M/API-key authentication result.
 *
 * On success, `failure` is `LONEJSON_AUTH_FAILURE_NONE`, `auth_mode`
 * identifies Basic or Bearer, `client_id` owns the authenticated credential id,
 * and `claim` owns the captured caller-supplied JSON. lonejson authenticates
 * and returns facts; endpoint, method, tenant, tool, and operation
 * authorization remain application-owned decisions.
 */
typedef struct lonejson_m2m_authentication {
  /** Failure classification, or `LONEJSON_AUTH_FAILURE_NONE` on success. */
  lonejson_auth_failure failure;
  /** Auth mode that matched the credential on success. */
  unsigned auth_mode;
  /** Authenticated client identifier owned by this result. */
  char *client_id;
  /** Caller-supplied claim JSON captured from the credential record. */
  lonejson_json_value claim;
} lonejson_m2m_authentication;

/** Generated signup seed material.
 *
 * `signup_secret` is shown once to the inviter or encoded into a signup URL.
 * `record_json` contains only salted/hashed secret material and claim JSON and
 * can be placed under a credential store's `signups` array. The caller owns
 * delivery of `query`/`url`, storage mutation, expiry policy, and removal of
 * consumed or revoked signup seeds.
 */
typedef struct lonejson_m2m_signup {
  /** Generated signup identifier. */
  char *signup_id;
  /** Generated one-time signup secret. */
  char *signup_secret;
  /** URL query component carrying signup id and secret. */
  lonejson_owned_buffer query;
  /** Full signup URL when `base_url` was supplied. */
  lonejson_owned_buffer url;
  /** Store-ready signup record containing only salted/hashed secret material.
   */
  lonejson_owned_buffer record_json;
} lonejson_m2m_signup;

/** Signup seed generation inputs.
 *
 * `base_url`, when present, is combined with generated query parameters to
 * produce `url`. `secret_param` and `id_param` override the default query
 * names. `claim_json` is validated and copied into the signup record for later
 * credential generation.
 */
typedef struct lonejson_m2m_signup_request {
  /** Optional base URL used to build `url`. */
  const char *base_url;
  /** Optional query parameter name for the signup secret. */
  const char *secret_param;
  /** Optional query parameter name for the signup id. */
  const char *id_param;
  /** Caller-supplied JSON claim embedded into the signup record. */
  const char *claim_json;
  /** Number of bytes in `claim_json`. */
  size_t claim_len;
  /** Maximum generated URL bytes. Zero means the default limit. */
  size_t max_url_bytes;
  /** Maximum generated record bytes. Zero means the default limit. */
  size_t max_record_bytes;
} lonejson_m2m_signup_request;

/** Signup completion inputs.
 *
 * `email` is required by the helper so the handler can keep the first public
 * signup flow intentionally small. `credential_auth_modes` controls the
 * generated credential; use `LONEJSON_M2M_AUTH_BEARER` for API-key-only signup.
 */
typedef struct lonejson_m2m_signup_complete_request {
  /** Caller-owned credential store containing the signup seed. Required. */
  const lonejson_m2m_store *store;
  /** Signup identifier supplied by the user/link. Required. */
  const char *signup_id;
  /** Signup secret supplied by the user/link. Required. */
  const char *signup_secret;
  /** User email captured by the signup handler. Required. */
  const char *email;
  /** Auth modes for the generated credential; zero uses defaults. */
  unsigned credential_auth_modes;
} lonejson_m2m_signup_complete_request;

/** Signup completion result. Remove `signup_id` from the caller-owned store
 * after successful completion, then insert `credential.record_json`.
 */
typedef struct lonejson_m2m_signup_completion {
  /** Consumed signup identifier. */
  char *signup_id;
  /** Email copied from the completion request. */
  char *email;
  /** Generated credential to insert after removing the signup record. */
  lonejson_m2m_credential credential;
} lonejson_m2m_signup_completion;

/** Generic HTTP provider initializer config.
 *
 * The caller owns all referenced pointers and must keep them alive while the
 * initialized provider can be used. The recommended framework integration is:
 * HTTP clients fill this provider for outbound OIDC/OAuth2 requests, while web
 * frameworks such as Kore or Vectis pass each inbound Authorization header to
 * `lonejson_oidc_validate_bearer_token()` from their request handler. lonejson
 * intentionally does not own framework routing, response writing, TLS policy,
 * proxy/redirect policy, or durable credential storage. Higher-level
 * token-flow helpers may add bounded retry/refresh behavior while still using
 * this provider boundary for the actual transfer.
 */
struct lonejson_http_provider_config {
  /** Caller-owned provider context passed to `request`. */
  void *user_data;
  /** Optional default User-Agent copied into provider-backed requests. */
  const char *user_agent;
  /** Caller-owned HTTP transfer callback. */
  lonejson_status (*request)(void *user_data,
                             const lonejson_http_request *request,
                             lonejson_http_response *response,
                             lonejson_error *error);
};
#endif

/** Producer callback used by `lonejson_writer_generator_init()`.
 *
 * The callback emits zero or more writer events into `writer`. Return
 * `LONEJSON_STATUS_OK` after the complete JSON document has been emitted. A
 * writer event that returns `LONEJSON_STATUS_OK` has been accepted, even if the
 * generator still needs later reads to drain pending bytes. If a writer event
 * returns `LONEJSON_STATUS_TRUNCATED`, immediately return that status and retry
 * the same event when the producer is called again.
 */
typedef lonejson_status (*lonejson_writer_producer_fn)(lonejson_writer *writer,
                                                       void *user,
                                                       lonejson_error *error);

/** Library-owned SSE receiver, created by `lonejson_sse_open()`.
 * Parser state stays private. Do not allocate or copy this shell; release it
 * through `close()`. JSON methods take the runtime after the receiver and
 * preserve the corresponding free functions' streaming and ownership rules.
 */
struct lonejson_sse {
  /** Feeds SSE bytes and delivers event chunks incrementally. */
  lonejson_status (*push)(lonejson_sse *sse, const void *bytes, size_t len,
                          const lonejson_sse_handler *handler, void *user,
                          lonejson_error *error);
  /** Dispatches a trailing event at EOF. */
  lonejson_status (*finish)(lonejson_sse *sse,
                            const lonejson_sse_handler *handler, void *user,
                            lonejson_error *error);
  /** Feeds SSE bytes into mapped JSON event decoding. */
  lonejson_status (*push_json)(lonejson_sse *sse, lonejson *runtime,
                               const lonejson_map *map, void *dst,
                               const void *bytes, size_t len,
                               const lonejson_sse_json_options *options,
                               lonejson_sse_json_event_fn event_cb, void *user,
                               lonejson_error *error);
  /** Finishes mapped JSON event decoding. */
  lonejson_status (*finish_json)(lonejson_sse *sse, lonejson *runtime,
                                 const lonejson_map *map, void *dst,
                                 const lonejson_sse_json_options *options,
                                 lonejson_sse_json_event_fn event_cb,
                                 void *user, lonejson_error *error);
  /** Feeds SSE bytes into arbitrary JSON event decoding. */
  lonejson_status (*push_json_value)(lonejson_sse *sse, lonejson *runtime,
                                     lonejson_json_value *value,
                                     const void *bytes, size_t len,
                                     const lonejson_sse_json_options *options,
                                     lonejson_sse_json_value_event_fn event_cb,
                                     void *user, lonejson_error *error);
  /** Finishes arbitrary JSON event decoding. */
  lonejson_status (*finish_json_value)(
      lonejson_sse *sse, lonejson *runtime, lonejson_json_value *value,
      const lonejson_sse_json_options *options,
      lonejson_sse_json_value_event_fn event_cb, void *user,
      lonejson_error *error);
  /** Releases the parser and its owned resources. */
  void (*close)(lonejson_sse *sse);
};

/** Library-owned multipart receiver, created by `lonejson_multipart_open()`.
 * Parser state stays private. Do not allocate or copy this shell; release it
 * through `close()`. Part callbacks retain their existing borrowed lifetimes.
 */
struct lonejson_multipart {
  /** Feeds multipart bytes and delivers part data incrementally. */
  lonejson_status (*push)(lonejson_multipart *multipart, const void *bytes,
                          size_t len, const lonejson_multipart_handler *handler,
                          void *user, lonejson_error *error);
  /** Validates that the closing boundary arrived before EOF. */
  lonejson_status (*finish)(lonejson_multipart *multipart,
                            const lonejson_multipart_handler *handler,
                            void *user, lonejson_error *error);
  /** Releases the parser and its owned resources. */
  void (*close)(lonejson_multipart *multipart);
};

/** Instantiated lonejson runtime.
 *
 * Construct this with `lonejson_new()` and release it with `lonejson_free()`.
 * The free-function APIs and the method pointers on this object are
 * equivalent. `state` is internal runtime state owned by lonejson. Treat this
 * as an owner-only opaque object: do not copy it by value. Only the original
 * pointer returned by `lonejson_new()` may be freed. If `lonejson_free()` is
 * accidentally called on a copied wrapper while the owner is still live,
 * lonejson only clears that copy and leaves the real owner untouched. The
 * runtime itself is thread-compatible: multiple threads may call APIs through
 * the same `lonejson *` concurrently as long as those calls operate on
 * distinct mutable handles or destination values, do not share one writable
 * `fixed_string_scratch` staging buffer, and use an allocator configuration
 * whose callbacks and optional stats storage are themselves safe to share
 * across those threads. Mutable handle objects created from a runtime are not
 * concurrently callable by multiple threads unless the caller provides
 * external synchronization.
 */
struct lonejson {
  /** Internal runtime identity cookie used to validate the owning wrapper. */
  lonejson_uint64 _state_token;
  /** Opaque implementation-owned runtime state. */
  void *state;
  /** Internal self pointer used for the owner-only runtime fast path. */
  void *_runtime_owner_self;
  /** Internal owner-only fast path to runtime state. */
  void *_runtime_owner_data;
  /** Parses a NUL-terminated JSON string into a mapped struct. */
  lonejson_status (*parse_cstr)(lonejson *runtime, const lonejson_map *map,
                                void *dst, const char *json,
                                lonejson_error *error);
  /** Parses one JSON buffer into a mapped struct. */
  lonejson_status (*parse_buffer)(lonejson *runtime, const lonejson_map *map,
                                  void *dst, const void *data, size_t len,
                                  lonejson_error *error);
  /** Parses JSON from a reader callback into a mapped struct. */
  lonejson_status (*parse_reader)(lonejson *runtime, const lonejson_map *map,
                                  void *dst, lonejson_reader_fn reader,
                                  void *user, lonejson_error *error);
  /** Parses JSON from an open `FILE *` into a mapped struct. */
  lonejson_status (*parse_filep)(lonejson *runtime, const lonejson_map *map,
                                 void *dst, FILE *fp, lonejson_error *error);
  /** Parses JSON from a filesystem path into a mapped struct. */
  lonejson_status (*parse_path)(lonejson *runtime, const lonejson_map *map,
                                void *dst, const char *path,
                                lonejson_error *error);
  /** Validates one caller-owned JSON buffer. */
  lonejson_status (*validate_buffer)(lonejson *runtime, const void *data,
                                     size_t len, lonejson_error *error);
  /** Validates one NUL-terminated JSON string. */
  lonejson_status (*validate_cstr)(lonejson *runtime, const char *json,
                                   lonejson_error *error);
  /** Validates JSON from a reader callback. */
  lonejson_status (*validate_reader)(lonejson *runtime,
                                     lonejson_reader_fn reader, void *user,
                                     lonejson_error *error);
  /** Validates JSON from an open `FILE *`. */
  lonejson_status (*validate_filep)(lonejson *runtime, FILE *fp,
                                    lonejson_error *error);
  /** Validates JSON from a filesystem path. */
  lonejson_status (*validate_path)(lonejson *runtime, const char *path,
                                   lonejson_error *error);
  /** Visits exactly one JSON value from a caller-owned buffer. */
  lonejson_status (*visit_value_buffer)(lonejson *runtime, const void *data,
                                        size_t len,
                                        const lonejson_value_visitor *visitor,
                                        void *user, lonejson_error *error);
  /** Visits exactly one JSON value from a NUL-terminated string. */
  lonejson_status (*visit_value_cstr)(lonejson *runtime, const char *json,
                                      const lonejson_value_visitor *visitor,
                                      void *user, lonejson_error *error);
  /** Visits exactly one JSON value from a reader callback. */
  lonejson_status (*visit_value_reader)(lonejson *runtime,
                                        lonejson_reader_fn reader,
                                        void *reader_user,
                                        const lonejson_value_visitor *visitor,
                                        void *user, lonejson_error *error);
  /** Visits exactly one JSON value from an open `FILE *`. */
  lonejson_status (*visit_value_filep)(lonejson *runtime, FILE *fp,
                                       const lonejson_value_visitor *visitor,
                                       void *user, lonejson_error *error);
  /** Visits exactly one JSON value from a filesystem path. */
  lonejson_status (*visit_value_path)(lonejson *runtime, const char *path,
                                      const lonejson_value_visitor *visitor,
                                      void *user, lonejson_error *error);
  /** Visits exactly one JSON value from a file descriptor. */
  lonejson_status (*visit_value_fd)(lonejson *runtime, int fd,
                                    const lonejson_value_visitor *visitor,
                                    void *user, lonejson_error *error);
  /** Visits exactly one JSON value from a caller-owned buffer with path-aware
   * callbacks.
   */
  lonejson_status (*visit_path_value_buffer)(
      lonejson *runtime, const void *data, size_t len,
      const lonejson_path_value_visitor *visitor, void *user,
      lonejson_error *error);
  /** Visits exactly one JSON value from a NUL-terminated string with
   * path-aware callbacks.
   */
  lonejson_status (*visit_path_value_cstr)(
      lonejson *runtime, const char *json,
      const lonejson_path_value_visitor *visitor, void *user,
      lonejson_error *error);
  /** Visits exactly one JSON value from a reader callback with path-aware
   * callbacks.
   */
  lonejson_status (*visit_path_value_reader)(
      lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
      const lonejson_path_value_visitor *visitor, void *user,
      lonejson_error *error);
  /** Visits exactly one JSON value from an open `FILE *` with path-aware
   * callbacks.
   */
  lonejson_status (*visit_path_value_filep)(
      lonejson *runtime, FILE *fp, const lonejson_path_value_visitor *visitor,
      void *user, lonejson_error *error);
  /** Visits exactly one JSON value from a filesystem path with path-aware
   * callbacks.
   */
  lonejson_status (*visit_path_value_path)(
      lonejson *runtime, const char *path,
      const lonejson_path_value_visitor *visitor, void *user,
      lonejson_error *error);
  /** Visits exactly one JSON value from a file descriptor with path-aware
   * callbacks.
   */
  lonejson_status (*visit_path_value_fd)(
      lonejson *runtime, int fd, const lonejson_path_value_visitor *visitor,
      void *user, lonejson_error *error);
  /** Initializes previously uninitialized mapped storage.
   * Returns `LONEJSON_STATUS_OK` or an error; `error` may be `NULL`.
   * Traversal-workspace allocation failure leaves the destination untouched.
   * See lonejson_init() for initialization and ownership requirements.
   */
  lonejson_status (*init)(lonejson *runtime, const lonejson_map *map,
                          void *value, lonejson_error *error);
  /** Clears an initialized mapped value, preserving caller-owned backing.
   * Returns `LONEJSON_STATUS_OK` or an error; `error` may be `NULL`.
   * Traversal-workspace allocation failure leaves the record unchanged.
   * See lonejson_reset().
   */
  lonejson_status (*reset)(lonejson *runtime, const lonejson_map *map,
                           void *value, lonejson_error *error);
  /** Assigns null to one field in an initialized mapped record. */
  lonejson_status (*record_assign_null)(lonejson *runtime,
                                        const lonejson_map *map, void *record,
                                        const lonejson_field *field,
                                        lonejson_error *error);
  /** Assigns decoded text to one mapped string field. */
  lonejson_status (*record_assign_string)(lonejson *runtime,
                                          const lonejson_map *map, void *record,
                                          const lonejson_field *field,
                                          const char *data, size_t len,
                                          lonejson_error *error);
  /** Appends decoded text to one mapped string array. */
  lonejson_status (*record_array_append_string)(
      lonejson *runtime, const lonejson_map *map, void *record,
      const lonejson_field *field, lonejson_string_array *array,
      const char *data, size_t len, lonejson_error *error);
  /** Appends one signed integer to a mapped array. */
  lonejson_status (*record_array_append_i64)(
      lonejson *runtime, const lonejson_map *map, void *record,
      const lonejson_field *field, lonejson_i64_array *array,
      lonejson_int64 value, lonejson_error *error);
  /** Appends one unsigned integer to a mapped array. */
  lonejson_status (*record_array_append_u64)(
      lonejson *runtime, const lonejson_map *map, void *record,
      const lonejson_field *field, lonejson_u64_array *array,
      lonejson_uint64 value, lonejson_error *error);
  /** Appends one number to a mapped array. */
  lonejson_status (*record_array_append_f64)(
      lonejson *runtime, const lonejson_map *map, void *record,
      const lonejson_field *field, lonejson_f64_array *array, double value,
      lonejson_error *error);
  /** Appends one boolean to a mapped array. */
  lonejson_status (*record_array_append_bool)(lonejson *runtime,
                                              const lonejson_map *map,
                                              void *record,
                                              const lonejson_field *field,
                                              lonejson_bool_array *array,
                                              int value, lonejson_error *error);
  /** Appends one uninitialized slot to a mapped object array. */
  void *(*record_object_array_append_slot)(lonejson *runtime,
                                           const lonejson_map *map,
                                           void *record,
                                           const lonejson_field *field,
                                           lonejson_object_array *array,
                                           lonejson_error *error);
  /** Releases storage owned by an initialized mapped value.
   * Returns `LONEJSON_STATUS_OK` or an error; `error` may be `NULL`.
   * Uses the runtime allocator for traversal workspace. Workspace allocation
   * failure leaves the record unchanged; retry cleanup when memory is
   * available. Owned fields are freed through their original allocators. See
   * lonejson_cleanup().
   */
  lonejson_status (*cleanup)(lonejson *runtime, const lonejson_map *map,
                             void *value, lonejson_error *error);
  /** Opens an object-framed stream over a reader callback. */
  lonejson_stream *(*stream_open_reader)(lonejson *runtime,
                                         const lonejson_map *map,
                                         lonejson_reader_fn reader, void *user,
                                         lonejson_error *error);
  /** Opens an object-framed stream over an open `FILE *`. */
  lonejson_stream *(*stream_open_filep)(lonejson *runtime,
                                        const lonejson_map *map, FILE *fp,
                                        lonejson_error *error);
  /** Opens an object-framed stream over a filesystem path. */
  lonejson_stream *(*stream_open_path)(lonejson *runtime,
                                       const lonejson_map *map,
                                       const char *path, lonejson_error *error);
  /** Opens an object-framed stream over a file descriptor. */
  lonejson_stream *(*stream_open_fd)(lonejson *runtime, const lonejson_map *map,
                                     int fd, lonejson_error *error);
  /** Opens a selected-array stream over a reader callback. */
  lonejson_array_stream *(*array_stream_open_reader)(lonejson *runtime,
                                                     const char *path,
                                                     lonejson_reader_fn reader,
                                                     void *user,
                                                     lonejson_error *error);
  /** Opens a selected-array stream over an open `FILE *`. */
  lonejson_array_stream *(*array_stream_open_filep)(lonejson *runtime,
                                                    const char *path, FILE *fp,
                                                    lonejson_error *error);
  /** Opens a selected-array stream over a filesystem path. */
  lonejson_array_stream *(*array_stream_open_path)(lonejson *runtime,
                                                   const char *array_path,
                                                   const char *path,
                                                   lonejson_error *error);
  /** Opens a selected-array stream over a file descriptor. */
  lonejson_array_stream *(*array_stream_open_fd)(lonejson *runtime,
                                                 const char *path, int fd,
                                                 lonejson_error *error);
  /** Opens a push-fed selected-array stream. */
  lonejson_array_stream *(*array_stream_open_push)(lonejson *runtime,
                                                   const char *path,
                                                   lonejson_error *error);
  /** Pushes SSE bytes into mapped JSON event decoding. */
  lonejson_status (*sse_push_json)(lonejson *runtime, lonejson_sse *sse,
                                   const lonejson_map *map, void *dst,
                                   const void *bytes, size_t len,
                                   const lonejson_sse_json_options *options,
                                   lonejson_sse_json_event_fn event_cb,
                                   void *user, lonejson_error *error);
  /** Finishes mapped SSE JSON event decoding. */
  lonejson_status (*sse_finish_json)(lonejson *runtime, lonejson_sse *sse,
                                     const lonejson_map *map, void *dst,
                                     const lonejson_sse_json_options *options,
                                     lonejson_sse_json_event_fn event_cb,
                                     void *user, lonejson_error *error);
  /** Pushes SSE bytes into arbitrary JSON event decoding. */
  lonejson_status (*sse_push_json_value)(
      lonejson *runtime, lonejson_sse *sse, lonejson_json_value *value,
      const void *bytes, size_t len, const lonejson_sse_json_options *options,
      lonejson_sse_json_value_event_fn event_cb, void *user,
      lonejson_error *error);
  /** Finishes arbitrary SSE JSON event decoding. */
  lonejson_status (*sse_finish_json_value)(
      lonejson *runtime, lonejson_sse *sse, lonejson_json_value *value,
      const lonejson_sse_json_options *options,
      lonejson_sse_json_value_event_fn event_cb, void *user,
      lonejson_error *error);
  /** Initializes one spool handle with the runtime default spool policy. */
  void (*spooled_init)(lonejson *runtime, lonejson_spooled *value);
  /** Initializes one spool handle with one named runtime spool policy. */
  void (*spooled_init_class)(lonejson *runtime, lonejson_spooled *value,
                             lonejson_spool_class spool_class);
  /** Initializes one stack-owned arbitrary JSON value handle. */
  void (*json_value_init)(lonejson *runtime, lonejson_json_value *value);
  /** Releases one stack-owned arbitrary JSON value handle. */
  void (*json_value_cleanup)(lonejson *runtime, lonejson_json_value *value);
  /** Rewrites one selected array from a reader callback to a sink. */
  lonejson_status (*array_rewrite_reader)(
      lonejson *runtime, const char *selector, lonejson_reader_fn reader,
      void *reader_user, lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from a reader callback to an open file. */
  lonejson_status (*array_rewrite_reader_to_filep)(
      lonejson *runtime, const char *selector, lonejson_reader_fn reader,
      void *reader_user, FILE *output,
      const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from a reader callback to a file descriptor.
   */
  lonejson_status (*array_rewrite_reader_to_fd)(
      lonejson *runtime, const char *selector, lonejson_reader_fn reader,
      void *reader_user, int fd,
      const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from an open file to a sink. */
  lonejson_status (*array_rewrite_filep)(
      lonejson *runtime, const char *selector, FILE *fp, lonejson_sink_fn sink,
      void *sink_user, const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from file to file. */
  lonejson_status (*array_rewrite_filep_to_filep)(
      lonejson *runtime, const char *selector, FILE *input, FILE *output,
      const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from file descriptor to sink. */
  lonejson_status (*array_rewrite_fd)(
      lonejson *runtime, const char *selector, int fd, lonejson_sink_fn sink,
      void *sink_user, const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from file descriptor to file descriptor. */
  lonejson_status (*array_rewrite_fd_to_fd)(
      lonejson *runtime, const char *selector, int input_fd, int output_fd,
      const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one selected array from input path to output path. */
  lonejson_status (*array_rewrite_path)(
      lonejson *runtime, const char *selector, const char *input_path,
      const char *output_path,
      const struct lonejson_array_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value from a reader callback to a sink. */
  lonejson_status (*value_rewrite_reader)(
      lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
      lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_value_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value from a buffer to a sink. */
  lonejson_status (*value_rewrite_buffer)(
      lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
      void *sink_user, const struct lonejson_value_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value from an open file to a sink. */
  lonejson_status (*value_rewrite_filep)(
      lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_value_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value from a file descriptor to a sink. */
  lonejson_status (*value_rewrite_fd)(
      lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_value_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value from input path to output path. */
  lonejson_status (*value_rewrite_path)(
      lonejson *runtime, const char *input_path, const char *output_path,
      const struct lonejson_value_rewrite_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value with a selector from a reader callback.
   */
  lonejson_status (*value_rewrite_selector_reader)(
      lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
      lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_value_rewrite_selector_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value with a selector from a buffer. */
  lonejson_status (*value_rewrite_selector_buffer)(
      lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
      void *sink_user,
      const struct lonejson_value_rewrite_selector_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value with a selector from an open file. */
  lonejson_status (*value_rewrite_selector_filep)(
      lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_value_rewrite_selector_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value with a selector from a file descriptor.
   */
  lonejson_status (*value_rewrite_selector_fd)(
      lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
      const struct lonejson_value_rewrite_selector_options *options,
      lonejson_error *error);
  /** Rewrites one arbitrary JSON value with a selector from input path to
   * output path. */
  lonejson_status (*value_rewrite_selector_path)(
      lonejson *runtime, const char *input_path, const char *output_path,
      const struct lonejson_value_rewrite_selector_options *options,
      lonejson_error *error);
  /** Initializes one pull generator using runtime defaults. */
  lonejson_status (*generator_init)(lonejson *runtime,
                                    lonejson_generator *generator,
                                    const lonejson_map *map, const void *src);
  /** Measures replayable mapped output without retaining the JSON. */
  lonejson_status (*generator_measure)(lonejson *runtime,
                                       const lonejson_map *map, const void *src,
                                       size_t *out_len, lonejson_error *error);
  /** Initializes a pull generator driven by writer events. */
  lonejson_status (*writer_generator_init)(lonejson *runtime,
                                           lonejson_generator *generator,
                                           lonejson_writer_producer_fn producer,
                                           void *producer_user);
  /** Initializes one streaming writer using runtime defaults. */
  lonejson_status (*writer_init_sink)(lonejson *runtime,
                                      lonejson_writer *writer,
                                      lonejson_sink_fn sink, void *sink_user,
                                      lonejson_error *error);
  /** Writes one top-level JSON string from a reader to a sink. */
  lonejson_status (*write_json_string_sink)(
      lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
      lonejson_sink_fn sink, void *sink_user, lonejson_error *error);
  /** Writes one top-level JSON string from a buffer to a sink. */
  lonejson_status (*write_json_string_buffer_sink)(lonejson *runtime,
                                                   const void *data, size_t len,
                                                   lonejson_sink_fn sink,
                                                   void *sink_user,
                                                   lonejson_error *error);
  /** Writes one top-level JSON string from a spooled value to a sink. */
  lonejson_status (*write_json_string_spooled_sink)(
      lonejson *runtime, const lonejson_spooled *value, lonejson_sink_fn sink,
      void *sink_user, lonejson_error *error);
  /** Serializes one mapped struct to a sink. */
  lonejson_status (*serialize_sink)(lonejson *runtime, const lonejson_map *map,
                                    const void *src, lonejson_sink_fn sink,
                                    void *sink_user, lonejson_error *error);
  /** Serializes one mapped struct into a caller buffer. */
  lonejson_status (*serialize_buffer)(lonejson *runtime,
                                      const lonejson_map *map, const void *src,
                                      char *buffer, size_t capacity,
                                      size_t *needed, lonejson_error *error);
  /** Serializes one mapped struct into a newly allocated string. */
  char *(*serialize_alloc)(lonejson *runtime, const lonejson_map *map,
                           const void *src, size_t *out_len,
                           lonejson_error *error);
  /** Serializes one mapped struct into an owned output buffer. */
  lonejson_status (*serialize_owned)(lonejson *runtime, const lonejson_map *map,
                                     const void *src,
                                     lonejson_owned_buffer *out,
                                     lonejson_error *error);
  /** Serializes one mapped struct to an open file. */
  lonejson_status (*serialize_filep)(lonejson *runtime, const lonejson_map *map,
                                     const void *src, FILE *fp,
                                     lonejson_error *error);
  /** Serializes one mapped struct to a filesystem path. */
  lonejson_status (*serialize_path)(lonejson *runtime, const lonejson_map *map,
                                    const void *src, const char *path,
                                    lonejson_error *error);
  /** Serializes mapped structs as JSONL to a sink. */
  lonejson_status (*serialize_jsonl_sink)(lonejson *runtime,
                                          const lonejson_map *map,
                                          const void *items, size_t count,
                                          size_t stride, lonejson_sink_fn sink,
                                          void *user, lonejson_error *error);
  /** Serializes mapped structs as JSONL into a caller buffer. */
  lonejson_status (*serialize_jsonl_buffer)(lonejson *runtime,
                                            const lonejson_map *map,
                                            const void *items, size_t count,
                                            size_t stride, char *buffer,
                                            size_t capacity, size_t *needed,
                                            lonejson_error *error);
  /** Serializes mapped structs as JSONL into a newly allocated string. */
  char *(*serialize_jsonl_alloc)(lonejson *runtime, const lonejson_map *map,
                                 const void *items, size_t count, size_t stride,
                                 size_t *out_len, lonejson_error *error);
  /** Serializes mapped structs as JSONL into an owned output buffer. */
  lonejson_status (*serialize_jsonl_owned)(lonejson *runtime,
                                           const lonejson_map *map,
                                           const void *items, size_t count,
                                           size_t stride,
                                           lonejson_owned_buffer *out,
                                           lonejson_error *error);
  /** Serializes mapped structs as JSONL to an open file. */
  lonejson_status (*serialize_jsonl_filep)(lonejson *runtime,
                                           const lonejson_map *map,
                                           const void *items, size_t count,
                                           size_t stride, FILE *fp,
                                           lonejson_error *error);
  /** Serializes mapped structs as JSONL to a filesystem path. */
  lonejson_status (*serialize_jsonl_path)(lonejson *runtime,
                                          const lonejson_map *map,
                                          const void *items, size_t count,
                                          size_t stride, const char *path,
                                          lonejson_error *error);
#ifdef LONEJSON_WITH_JWT
  /** Parses one JWK JSON object into `out`. */
  lonejson_status (*jwk_parse_json)(lonejson *runtime, const char *json,
                                    size_t len, lonejson_jwk *out,
                                    lonejson_error *error);
  /** Parses one JWKS JSON object with a required `keys` array. */
  lonejson_status (*jwks_parse_json)(lonejson *runtime, const char *json,
                                     size_t len, lonejson_jwks *out,
                                     lonejson_error *error);
  /** Decodes and parses one compact JWT header and claims payload. */
  lonejson_status (*jwt_decode_compact)(lonejson *runtime, const char *token,
                                        size_t len,
                                        const lonejson_jwt_claim_policy *limits,
                                        lonejson_jwt_header *header,
                                        lonejson_jwt_claims *claims,
                                        lonejson_error *error);
  /** Validates a compact JWT signature through this runtime's auth provider. */
  lonejson_status (*jwt_validate_signature_with_runtime)(
      lonejson *runtime, const lonejson_jwt_compact *jwt,
      const lonejson_jwt_header *header, const lonejson_jwk *jwk,
      lonejson_error *error);
#endif
  /** Installs or clears the runtime auth provider. */
  lonejson_status (*set_auth_provider)(lonejson *runtime,
                                       const lonejson_auth_provider *provider,
                                       lonejson_error *error);
#ifdef LONEJSON_WITH_OIDC
  /** Parses a bounded OAuth2 token introspection response. */
  lonejson_status (*oauth2_introspection_response_parse_json)(
      lonejson *runtime, const char *json, size_t len,
      size_t max_response_bytes, lonejson_oauth2_introspection_response *out,
      lonejson_error *error);
  /** Parses a bounded OIDC UserInfo response. */
  lonejson_status (*oidc_userinfo_response_parse_json)(
      lonejson *runtime, const char *json, size_t len,
      size_t max_response_bytes, lonejson_oidc_userinfo_response *out,
      lonejson_error *error);
  /** Parses one OIDC discovery JSON object into `out`. */
  lonejson_status (*oidc_discovery_parse_json)(lonejson *runtime,
                                               const char *json, size_t len,
                                               lonejson_oidc_discovery *out,
                                               lonejson_error *error);
  /** Fetches, parses, and validates discovery metadata through HTTP provider.
   */
  lonejson_status (*oidc_fetch_discovery)(lonejson *runtime, const char *issuer,
                                          size_t max_response_bytes,
                                          lonejson_oidc_discovery *out,
                                          lonejson_error *error);
  /** Installs caller-provided JWKS JSON into a bounded cache. */
  lonejson_status (*oidc_jwks_cache_update_json)(
      lonejson *runtime, lonejson_oidc_jwks_cache *cache,
      const lonejson_oidc_jwks_cache_policy *policy, const char *json,
      size_t len, lonejson_error *error);
  /** Refreshes a JWKS cache through this runtime's HTTP provider. */
  lonejson_status (*oidc_jwks_cache_refresh)(
      lonejson *runtime, lonejson_oidc_jwks_cache *cache,
      const lonejson_oidc_jwks_cache_policy *policy, lonejson_error *error);
  /** Parses and validates a bounded OAuth2 token response. */
  lonejson_status (*oauth2_token_response_parse_json)(
      lonejson *runtime, const char *json, size_t len,
      size_t max_response_bytes, lonejson_oauth2_token_response *out,
      lonejson_error *error);
  /** Exchanges OAuth2 client credentials through this runtime's HTTP provider.
   */
  lonejson_status (*oauth2_client_credentials_request)(
      lonejson *runtime, const char *token_endpoint,
      const lonejson_oauth2_client_credentials *request,
      size_t max_response_bytes, lonejson_oauth2_token_response *out,
      lonejson_error *error);
  /** Exchanges an OAuth2 refresh token through this runtime's HTTP provider. */
  lonejson_status (*oauth2_refresh_token_request)(
      lonejson *runtime, const char *token_endpoint,
      const lonejson_oauth2_refresh_token *request, size_t max_response_bytes,
      lonejson_oauth2_token_response *out, lonejson_error *error);
  /** Ensures a token-flow has a usable access token, refreshing if needed. */
  lonejson_status (*oauth2_token_flow_ensure)(
      lonejson *runtime, lonejson_oauth2_token_flow *flow,
      const lonejson_oauth2_token_flow_policy *policy,
      lonejson_oauth2_token_flow_result *result, lonejson_error *error);
  /** Introspects a token through this runtime's HTTP provider. */
  lonejson_status (*oauth2_introspect_token_request)(
      lonejson *runtime, const char *introspection_endpoint,
      const lonejson_oauth2_token_introspection *request,
      size_t max_response_bytes, lonejson_oauth2_introspection_response *out,
      lonejson_error *error);
  /** Revokes a token through this runtime's HTTP provider. */
  lonejson_status (*oauth2_revoke_token_request)(
      lonejson *runtime, const char *revocation_endpoint,
      const lonejson_oauth2_token_revocation *request, lonejson_error *error);
  /** Fetches OIDC UserInfo through this runtime's HTTP provider. */
  lonejson_status (*oidc_fetch_userinfo)(
      lonejson *runtime, const char *userinfo_endpoint,
      const lonejson_oidc_userinfo_request *request,
      lonejson_oidc_userinfo_response *out, lonejson_error *error);
  /** Exchanges an authorization code through this runtime's HTTP provider. */
  lonejson_status (*oidc_authorization_code_token_request)(
      lonejson *runtime, const char *token_endpoint,
      const lonejson_oidc_authorization_code_token *request,
      size_t max_response_bytes, lonejson_oauth2_token_response *out,
      lonejson_error *error);
  /** Validates one Authorization Bearer JWT against cache and claim policy. */
  lonejson_status (*oidc_validate_bearer_token)(
      lonejson *runtime, const lonejson_oidc_bearer_validation_request *request,
      lonejson_oidc_bearer_validation *out, lonejson_error *error);
  /** Computes a PKCE S256 challenge through this runtime's auth provider.
   *
   * Requires provider `sha256`.
   */
  lonejson_status (*oidc_pkce_challenge_with_runtime)(
      lonejson *runtime, const char *code_verifier, lonejson_owned_buffer *out,
      lonejson_error *error);
  /** Generates a PKCE verifier through this runtime's auth provider.
   *
   * Requires provider `random_bytes` and `sha256`.
   */
  lonejson_status (*oidc_pkce_generate_with_runtime)(lonejson *runtime,
                                                     size_t verifier_bytes,
                                                     lonejson_oidc_pkce *out,
                                                     lonejson_error *error);
  /** Generates one M2M credential record plus one-time secrets. */
  lonejson_status (*m2m_credential_generate)(
      lonejson *runtime, const lonejson_m2m_credential_request *request,
      lonejson_m2m_credential *out, lonejson_error *error);
  /** Verifies Basic client credentials or Bearer API key against a JSON store.
   */
  lonejson_status (*m2m_verify_authorization)(
      lonejson *runtime, const lonejson_m2m_verify_request *request,
      lonejson_m2m_authentication *out, lonejson_error *error);
  /** Generates one signup seed record and optional URL/query values. */
  lonejson_status (*m2m_signup_generate)(
      lonejson *runtime, const lonejson_m2m_signup_request *request,
      lonejson_m2m_signup *out, lonejson_error *error);
  /** Completes one signup seed into a new M2M credential. */
  lonejson_status (*m2m_signup_complete)(
      lonejson *runtime, const lonejson_m2m_signup_complete_request *request,
      lonejson_m2m_signup_completion *out, lonejson_error *error);
#endif
  /** Installs or clears the runtime auth HTTP provider. */
  lonejson_status (*set_http_provider)(lonejson *runtime,
                                       const lonejson_http_provider *provider,
                                       lonejson_error *error);
  /** Initializes one curl parse adapter. */
  lonejson_status (*curl_parse_init)(lonejson *runtime,
                                     lonejson_curl_parse *ctx,
                                     const lonejson_map *map, void *dst);
  /** Initializes one curl selected-array parse adapter. */
  lonejson_status (*curl_array_parse_init)(
      lonejson *runtime, lonejson_curl_array_parse *ctx, const char *path,
      const lonejson_map *map, void *dst,
      lonejson_array_stream_item_fn callback, void *user);
  /** Initializes one curl selected string-array parse adapter. */
  lonejson_status (*curl_string_array_parse_init)(
      lonejson *runtime, lonejson_curl_string_array_parse *ctx,
      const char *path, const lonejson_array_stream_string_handler *handler,
      void *user);
  /** Initializes one curl selected string-items parse adapter. */
  lonejson_status (*curl_string_items_parse_init)(
      lonejson *runtime, lonejson_curl_string_items_parse *ctx,
      const char *path, lonejson_array_stream_string_fn callback, void *user);
  /** Initializes one curl upload adapter for a mapped struct. */
  lonejson_status (*curl_upload_init)(lonejson *runtime,
                                      lonejson_curl_upload *ctx,
                                      const lonejson_map *map, const void *src);
#if defined(LONEJSON_WITH_CURL) && defined(LONEJSON_WITH_OIDC)
  /** Initializes a curl adapter for a bounded JWKS cache refresh. */
  lonejson_status (*oidc_jwks_cache_parse_init)(
      lonejson *runtime, lonejson_oidc_jwks_cache_parse *ctx,
      lonejson_oidc_jwks_cache *cache,
      const lonejson_oidc_jwks_cache_policy *policy);
#endif
  /** Releases the runtime and all runtime-owned state. */
  void (*free)(lonejson *runtime);
};

/** Reader adapter state for an immutable caller-owned memory buffer.
 *
 * Initialize with `lonejson_buffer_reader_init()` and pass
 * `lonejson_buffer_reader_read` as the reader callback. The adapter never
 * copies or owns `data`; callers must keep the buffer alive until the consumer
 * finishes reading.
 */
typedef struct lonejson_buffer_reader {
  /** Caller-owned input bytes. */
  const unsigned char *data;
  /** Total byte length of `data`. */
  size_t len;
  /** Current read offset maintained by `lonejson_buffer_reader_read()`. */
  size_t offset;
} lonejson_buffer_reader;

/** Pull-style JSON generator for one mapped value.
 *
 * Generators are explicit, bounded serializer states meant for transport
 * adapters and other pull-based consumers. They do not materialize the full
 * payload. They support the normal mapped serializer surface plus
 * `lonejson_source`, `lonejson_spooled`, and `lonejson_json_value` fields.
 * `json_value` fields are streamed through the generator directly instead of
 * silently falling back to buffered serialization.
 */
struct lonejson_generator {
  /** Opaque generator state owned by lonejson. */
  void *state;
  /** Non-zero once the generator has reached end-of-stream. */
  int eof;
  /** Last generator error. Cleared by init and cleanup. */
  lonejson_error error;
  /** Pulls the next serialized bytes from the generator. */
  lonejson_status (*read)(lonejson_generator *generator, unsigned char *buffer,
                          size_t capacity, size_t *out_len, int *out_eof);
  /** Releases resources owned by the generator. */
  void (*cleanup)(lonejson_generator *generator);
};

/** Public state for a streaming JSON writer.
 *
 * The `state` pointer is owned by lonejson after successful initialization.
 * `error` stores the last writer error for callers that prefer handle-local
 * diagnostics. Release initialized writers with `lonejson_writer_cleanup()`.
 */
struct lonejson_writer {
  /** Opaque writer state owned by lonejson. */
  void *state;
  /** Last writer error. Cleared by init and cleanup. */
  lonejson_error error;
  /** Releases resources owned by the writer. */
  void (*cleanup)(lonejson_writer *writer);
  /** Begins a JSON object value. */
  lonejson_status (*begin_object)(lonejson_writer *writer,
                                  lonejson_error *error);
  /** Ends the current JSON object. */
  lonejson_status (*end_object)(lonejson_writer *writer, lonejson_error *error);
  /** Begins a JSON array value. */
  lonejson_status (*begin_array)(lonejson_writer *writer,
                                 lonejson_error *error);
  /** Ends the current JSON array. */
  lonejson_status (*end_array)(lonejson_writer *writer, lonejson_error *error);
  /** Emits one object member key. */
  lonejson_status (*key)(lonejson_writer *writer, const char *key,
                         size_t key_len, lonejson_error *error);
  /** Emits one JSON string value from a buffer. */
  lonejson_status (*string)(lonejson_writer *writer, const void *data,
                            size_t len, lonejson_error *error);
  /** Begins a chunked JSON string value. */
  lonejson_status (*string_begin)(lonejson_writer *writer,
                                  lonejson_error *error);
  /** Emits one raw chunk into an open chunked JSON string. */
  lonejson_status (*string_chunk)(lonejson_writer *writer, const void *data,
                                  size_t len, lonejson_error *error);
  /** Ends a chunked JSON string value. */
  lonejson_status (*string_end)(lonejson_writer *writer, lonejson_error *error);
  /** Emits one JSON string by streaming from a reader callback. */
  lonejson_status (*string_reader)(lonejson_writer *writer,
                                   lonejson_reader_fn reader, void *reader_user,
                                   lonejson_error *error);
  /** Emits one JSON string from a rewindable spooled value. */
  lonejson_status (*string_spooled)(lonejson_writer *writer,
                                    const lonejson_spooled *value,
                                    lonejson_error *error);
  /** Emits one JSON string from a rewindable outbound source. */
  lonejson_status (*source_text)(lonejson_writer *writer,
                                 const lonejson_source *value,
                                 lonejson_error *error);
  /** Emits one base64 string from a rewindable spooled value. */
  lonejson_status (*spooled_base64)(lonejson_writer *writer,
                                    const lonejson_spooled *value,
                                    lonejson_error *error);
  /** Emits one base64 string from a rewindable outbound source. */
  lonejson_status (*source_base64)(lonejson_writer *writer,
                                   const lonejson_source *value,
                                   lonejson_error *error);
  /** Emits one validated JSON number token. */
  lonejson_status (*number_text)(lonejson_writer *writer, const char *data,
                                 size_t len, lonejson_error *error);
  /** Begins a chunked JSON number value. */
  lonejson_status (*number_begin)(lonejson_writer *writer,
                                  lonejson_error *error);
  /** Appends one raw token chunk to an open chunked JSON number. */
  lonejson_status (*number_chunk)(lonejson_writer *writer, const char *data,
                                  size_t len, lonejson_error *error);
  /** Ends and validates a chunked JSON number value. */
  lonejson_status (*number_end)(lonejson_writer *writer, lonejson_error *error);
  /** Emits one signed 64-bit JSON integer. */
  lonejson_status (*i64)(lonejson_writer *writer, lonejson_int64 value,
                         lonejson_error *error);
  /** Emits one unsigned 64-bit JSON integer. */
  lonejson_status (*u64)(lonejson_writer *writer, lonejson_uint64 value,
                         lonejson_error *error);
  /** Emits one finite double value. */
  lonejson_status (*f64)(lonejson_writer *writer, double value,
                         lonejson_error *error);
  /** Emits one JSON boolean value. */
  lonejson_status (*bool_fn)(lonejson_writer *writer, int value,
                             lonejson_error *error);
  /** Emits one JSON `null`. */
  lonejson_status (*null_fn)(lonejson_writer *writer, lonejson_error *error);
  /** Emits one arbitrary JSON value from a configured handle. */
  lonejson_status (*json_value)(lonejson_writer *writer,
                                const lonejson_json_value *value,
                                lonejson_error *error);
  /** Streams one arbitrary JSON value from a reader callback. */
  lonejson_status (*json_value_reader)(lonejson_writer *writer,
                                       lonejson_reader_fn reader,
                                       void *reader_user,
                                       lonejson_error *error);
  /** Streams one arbitrary JSON value from a buffer. */
  lonejson_status (*json_value_buffer)(lonejson_writer *writer,
                                       const void *data, size_t len,
                                       lonejson_error *error);
  /** Streams one arbitrary JSON value from an open `FILE *`. */
  lonejson_status (*json_value_file)(lonejson_writer *writer, FILE *fp,
                                     lonejson_error *error);
  /** Streams one arbitrary JSON value from a file descriptor. */
  lonejson_status (*json_value_fd)(lonejson_writer *writer, int fd,
                                   lonejson_error *error);
  /** Streams one arbitrary JSON value from a filesystem path. */
  lonejson_status (*json_value_path)(lonejson_writer *writer, const char *path,
                                     lonejson_error *error);
  /** Streams one arbitrary JSON value from a rewindable spooled value. */
  lonejson_status (*json_value_spooled)(lonejson_writer *writer,
                                        const lonejson_spooled *value,
                                        lonejson_error *error);
  /** Appends selected array items from a reader callback into the current
   * array. */
  lonejson_status (*array_items_reader)(lonejson_writer *writer,
                                        const char *selector,
                                        lonejson_reader_fn reader,
                                        void *reader_user,
                                        lonejson_error *error);
  /** Appends selected array items from a buffer into the current array. */
  lonejson_status (*array_items_buffer)(lonejson_writer *writer,
                                        const char *selector, const void *data,
                                        size_t len, lonejson_error *error);
  /** Appends selected array items from an open `FILE *`. */
  lonejson_status (*array_items_filep)(lonejson_writer *writer,
                                       const char *selector, FILE *fp,
                                       lonejson_error *error);
  /** Appends selected array items from a file descriptor. */
  lonejson_status (*array_items_fd)(lonejson_writer *writer,
                                    const char *selector, int fd,
                                    lonejson_error *error);
  /** Appends selected array items from a filesystem path. */
  lonejson_status (*array_items_path)(lonejson_writer *writer,
                                      const char *selector, const char *path,
                                      lonejson_error *error);
  /** Appends selected array items from a rewindable spooled value. */
  lonejson_status (*array_items_spooled)(lonejson_writer *writer,
                                         const char *selector,
                                         const lonejson_spooled *value,
                                         lonejson_error *error);
  /** Emits one mapped struct value through the normal serializer. */
  lonejson_status (*mapped)(lonejson_writer *writer, const lonejson_map *map,
                            const void *src, lonejson_error *error);
  /** Finalizes the writer after one complete JSON document. */
  lonejson_status (*finish)(lonejson_writer *writer, lonejson_error *error);
};

/** Public state for a push-fed arbitrary JSON value inside a writer. */
struct lonejson_writer_value_stream {
  /** Opaque value-stream state owned by lonejson. */
  void *state;
  /** Last value-stream error. Cleared by open and cleanup. */
  lonejson_error error;
  /** Opens one push-fed arbitrary JSON value on the current writer position. */
  lonejson_status (*open)(lonejson_writer_value_stream *stream,
                          lonejson_writer *writer, lonejson_error *error);
  /** Feeds one byte chunk into an open writer value stream. */
  lonejson_status (*push)(lonejson_writer_value_stream *stream,
                          const void *data, size_t len, lonejson_error *error);
  /** Finalizes an open writer value stream at source EOF. */
  lonejson_status (*close)(lonejson_writer_value_stream *stream,
                           lonejson_error *error);
  /** Releases resources owned by a writer value stream. */
  void (*cleanup)(lonejson_writer_value_stream *stream);
};

/** Callback that emits one replacement JSON value through a writer.
 *
 * The callback must emit exactly one complete JSON value into `writer`. It
 * should use writer events rather than pre-serialized JSON text so lonejson
 * continues to own JSON syntax, escaping, and validation.
 */
typedef lonejson_status (*lonejson_value_rewrite_emit_fn)(
    lonejson_writer *writer, void *user, lonejson_error *error);

/** Metadata passed to old-value-dependent rewrite callbacks.
 *
 * For `LONEJSON_VALUE_NUMBER`, `number` points to the validated JSON number
 * token and remains valid only for the duration of the callback. For
 * `LONEJSON_VALUE_BOOL`, `boolean` is non-zero for `true`. Other value types
 * are reported by type only; configure `old_value_visitor` in the rewrite
 * options when structured streaming inspection is needed.
 */
typedef struct lonejson_value_rewrite_old_value {
  /** Non-zero when the target existed in the input document. */
  int present;
  /** JSON type of the matched value, or `LONEJSON_VALUE_ABSENT`. */
  lonejson_value_type type;
  /** Validated number token for numeric matches. */
  const char *number;
  /** Length of `number` in bytes. */
  size_t number_len;
  /** Boolean value for `LONEJSON_VALUE_BOOL` matches. */
  int boolean;
} lonejson_value_rewrite_old_value;

/** Callback used by old-value-dependent value rewrites.
 *
 * The callback is invoked at the selected value position after the old value
 * has been streamed to `old_value_visitor`, if one was configured. It must emit
 * exactly one complete replacement JSON value through `writer`, or return an
 * error to reject the rewrite. For absent object paths, `old_value->present` is
 * zero and the callback emits the value for the synthesized final segment.
 */
typedef lonejson_status (*lonejson_value_rewrite_replace_fn)(
    lonejson_writer *writer, const lonejson_value_rewrite_old_value *old_value,
    void *user, lonejson_error *error);

/** Defines a static mapping table for one struct type from a separately
 * declared field array.
 *
 * The resulting map has static storage duration and carries a stable cache
 * identity cookie for lonejson's internal schema-analysis fast path. Use one
 * macro expansion per distinct schema; do not rewrite a different map in place
 * at the same address.
 */
#define LONEJSON_MAP_DEFINE(name, type, fields_array)                          \
  static const lonejson_map name = {                                           \
      #type,                                                                   \
      sizeof(type),                                                            \
      fields_array,                                                            \
      sizeof(fields_array) / sizeof((fields_array)[0]),                        \
      &name,                                                                   \
      LONEJSON__MAP_STATIC_COOKIE(__LINE__, sizeof(type),                      \
                                  sizeof(fields_array) /                       \
                                      sizeof((fields_array)[0]))}

/** Maps a JSON string field into a dynamically allocated `char *` member.
 *
 * Parse-time growth is bounded by
 * `lonejson_config.max_dynamic_string_bytes`, which defaults to
 * `LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES`.
 */
#define LONEJSON_FIELD_STRING_ALLOC(type, member, key)                         \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON string field into a dynamically allocated `char *`
 * member.
 *
 * Parse-time growth is bounded by
 * `lonejson_config.max_dynamic_string_bytes`, which defaults to
 * `LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES`.
 */
#define LONEJSON_FIELD_STRING_ALLOC_REQ(type, member, key)                     \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps an optional JSON string field into a dynamically allocated `char *`
 * member and omits the field when the pointer is `NULL` during serialization.
 */
#define LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(type, member, key)               \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_NULL,                                                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps an optional dynamically allocated JSON string and omits it when the
 * pointer is `NULL` or the string is empty during serialization.
 */
#define LONEJSON_FIELD_STRING_ALLOC_OMIT_EMPTY(type, member, key)              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON string field into a fixed-size character array member.
 *
 * One-shot parses write directly into the destination. When parsing with
 * `lonejson_config.clear_destination_by_default = 0` and the destination
 * already holds a non-empty string, lonejson stages the replacement first. It
 * uses `lonejson_config.fixed_string_scratch` when that scratch is large
 * enough for the field capacity; otherwise it allocates temporary staging
 * storage that counts against `lonejson_config.max_alloc_bytes`.
 */
#define LONEJSON_FIELD_STRING_FIXED(type, member, key, policy)                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   policy,                                                                     \
   0u,                                                                         \
   sizeof(((type *)0)->member),                                                \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON string field into a fixed-size character array member.
 *
 * Reused non-empty destinations follow the same staging rules as
 * `LONEJSON_FIELD_STRING_FIXED`.
 */
#define LONEJSON_FIELD_STRING_FIXED_REQ(type, member, key, policy)             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   policy,                                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   sizeof(((type *)0)->member),                                                \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a fixed-capacity JSON string and omits it when the first byte is NUL
 * during serialization.
 *
 * Reused non-empty destinations follow the same staging rules as
 * `LONEJSON_FIELD_STRING_FIXED`.
 */
#define LONEJSON_FIELD_STRING_FIXED_OMIT_EMPTY(type, member, key, policy)      \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   sizeof(((type *)0)->member),                                                \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON string field into a `lonejson_spooled` member that keeps an
 * in-memory prefix and spills excess data to a temporary file using default
 * spool options (`LONEJSON_SPOOL_MEMORY_LIMIT` in memory and
 * `LONEJSON_SPOOL_MAX_BYTES` total bytes).
 */
#define LONEJSON_FIELD_STRING_STREAM(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON string field into a `lonejson_spooled` member using
 * default spool options (`LONEJSON_SPOOL_MEMORY_LIMIT` in memory and
 * `LONEJSON_SPOOL_MAX_BYTES` total bytes).
 */
#define LONEJSON_FIELD_STRING_STREAM_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a streamed JSON string field and omits it when the spooled value is
 * empty during serialization.
 */
#define LONEJSON_FIELD_STRING_STREAM_OMIT_EMPTY(type, member, key)             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON string field into a `lonejson_spooled` member that decodes
 * Base64 incrementally and stores the decoded bytes using default spool
 * options. */
#define LONEJSON_FIELD_BASE64_STREAM(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required Base64-decoded JSON string field into a `lonejson_spooled`
 * member using default spool options. */
#define LONEJSON_FIELD_BASE64_STREAM_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a streamed Base64 field and omits it when the decoded spooled value is
 * empty during serialization.
 */
#define LONEJSON_FIELD_BASE64_STREAM_OMIT_EMPTY(type, member, key)             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Internal-only map helper for one explicit spool-policy pointer. Public
 * callers should use `LONEJSON_FIELD_STRING_STREAM_CLASS`.
 */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
#define LONEJSON_FIELD_STRING_STREAM_OPTS(type, member, key, options_ptr)      \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   options_ptr,                                                                \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}
#endif

/** Maps a JSON string field into a `lonejson_spooled` member using one named
 * runtime spool policy class.
 */
#define LONEJSON_FIELD_STRING_STREAM_CLASS(type, member, key, spool_class)     \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   spool_class}

/** Maps a JSON array of strings into a chunked streaming field. The destination
 * member must be a `lonejson_string_array_stream` configured with
 * `lonejson_string_array_stream_set_handler` before parsing.
 */
#define LONEJSON_FIELD_STRING_ARRAY_STREAM(type, member, key)                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM,                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON array of strings into a chunked streaming field. */
#define LONEJSON_FIELD_STRING_ARRAY_STREAM_REQ(type, member, key)              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM,                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of objects into an item-by-item mapped stream. The
 * destination member must be a `lonejson_mapped_array_stream` configured with
 * `lonejson_mapped_array_stream_set_handler` before parsing.
 */
#define LONEJSON_FIELD_MAPPED_ARRAY_STREAM(type, member, key)                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM,                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON array of objects into an item-by-item mapped stream. */
#define LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(type, member, key)              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM,                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a serialize-only JSON string field into a `lonejson_source` member
 * that streams raw text bytes from a file, fd, or path at write time. */
#define LONEJSON_FIELD_STRING_SOURCE(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a serialize-only JSON string source and omits it when no source is
 * configured. */
#define LONEJSON_FIELD_STRING_SOURCE_OMIT_NULL(type, member, key)              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_NULL,                                                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required serialize-only JSON string field into a `lonejson_source`
 * member that streams raw text bytes at write time. */
#define LONEJSON_FIELD_STRING_SOURCE_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a serialize-only JSON string field into a `lonejson_source` member
 * that base64-encodes raw bytes from a file, fd, or path at write time. */
#define LONEJSON_FIELD_BASE64_SOURCE(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a serialize-only Base64 source and omits it when no source is
 * configured. */
#define LONEJSON_FIELD_BASE64_SOURCE_OMIT_NULL(type, member, key)              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_NULL,                                                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required serialize-only Base64 JSON string field into a
 * `lonejson_source` member that streams raw bytes at write time. */
#define LONEJSON_FIELD_BASE64_SOURCE_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps an arbitrary embedded JSON value into a `lonejson_json_value` member.
 * The field accepts any JSON value on parse and emits that value directly on
 * serialize without wrapping it as a JSON string. Before parsing into this
 * field, configure the destination handle for sink, visitor, or capture mode.
 */
#define LONEJSON_FIELD_JSON_VALUE(type, member, key)                           \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_JSON_VALUE,                                             \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required arbitrary embedded JSON value into a `lonejson_json_value`
 * member. Before parsing into this field, configure the destination handle
 * for sink, visitor, or capture mode.
 */
#define LONEJSON_FIELD_JSON_VALUE_REQ(type, member, key)                       \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_JSON_VALUE,                                             \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps an arbitrary embedded JSON value and omits it when the handle is in
 * the `LONEJSON_JSON_VALUE_NULL` state during serialization. */
#define LONEJSON_FIELD_JSON_VALUE_OMIT_NULL(type, member, key)                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_JSON_VALUE,                                             \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_NULL,                                                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Internal-only map helper for one explicit spool-policy pointer. Public
 * callers should use `LONEJSON_FIELD_BASE64_STREAM_CLASS`.
 */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
#define LONEJSON_FIELD_BASE64_STREAM_OPTS(type, member, key, options_ptr)      \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   options_ptr,                                                                \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}
#endif

/** Maps a Base64-decoded JSON string field into a `lonejson_spooled` member
 * using one named runtime spool policy class.
 */
#define LONEJSON_FIELD_BASE64_STREAM_CLASS(type, member, key, spool_class)     \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   spool_class}

/** Maps a JSON integer field into a `lonejson_int64` member. */
#define LONEJSON_FIELD_I64(type, member, key)                                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON integer field into a `lonejson_int64` member. */
#define LONEJSON_FIELD_I64_REQ(type, member, key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON integer field that serializes only when an `int` presence
 * member is non-zero. */
#define LONEJSON_FIELD_I64_PRESENT(type, member, present_member, key)          \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE,                                                \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a nullable JSON integer field into a `lonejson_int64` member and an
 * `int` presence member. A JSON integer sets presence to `1`; JSON `null` sets
 * presence to `0` and resets the integer member to zero. Serialization omits
 * the field when presence is `0` and emits the integer when presence is `1`. */
#define LONEJSON_FIELD_I64_PRESENT_NULLABLE(type, member, present_member, key) \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL,                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON unsigned integer field into a `lonejson_uint64` member. */
#define LONEJSON_FIELD_U64(type, member, key)                                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON unsigned integer field into a `lonejson_uint64` member.
 */
#define LONEJSON_FIELD_U64_REQ(type, member, key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON unsigned integer field that serializes only when an `int`
 * presence member is non-zero. */
#define LONEJSON_FIELD_U64_PRESENT(type, member, present_member, key)          \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE,                                                \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a nullable JSON unsigned integer field into a `lonejson_uint64` member
 * and an `int` presence member. A JSON unsigned integer sets presence to `1`;
 * JSON `null` sets presence to `0` and resets the integer member to zero.
 * Serialization omits the field when presence is `0` and emits the integer
 * when presence is `1`. */
#define LONEJSON_FIELD_U64_PRESENT_NULLABLE(type, member, present_member, key) \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL,                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON number field into a `double` member. */
#define LONEJSON_FIELD_F64(type, member, key)                                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON number field into a `double` member. */
#define LONEJSON_FIELD_F64_REQ(type, member, key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON number field that serializes only when an `int` presence member
 * is non-zero. */
#define LONEJSON_FIELD_F64_PRESENT(type, member, present_member, key)          \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE,                                                \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a nullable JSON number field into a `double` member and an `int`
 * presence member. A JSON number sets presence to `1`; JSON `null` sets
 * presence to `0` and resets the number member to zero. Serialization omits
 * the field when presence is `0` and emits the number when presence is `1`. */
#define LONEJSON_FIELD_F64_PRESENT_NULLABLE(type, member, present_member, key) \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL,                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON boolean field into a `bool` member. */
#define LONEJSON_FIELD_BOOL(type, member, key)                                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL,                                                   \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required JSON boolean field into a `bool` member. */
#define LONEJSON_FIELD_BOOL_REQ(type, member, key)                             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL,                                                   \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON boolean field that serializes only when an `int` presence
 * member is non-zero. */
#define LONEJSON_FIELD_BOOL_PRESENT(type, member, present_member, key)         \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL,                                                   \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE,                                                \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a nullable JSON boolean field into a `bool` member and an `int`
 * presence member. A JSON boolean sets presence to `1`; JSON `null` sets
 * presence to `0` and resets the boolean member to `false`. Serialization
 * omits the field when presence is `0` and emits the boolean when presence is
 * `1`. */
#define LONEJSON_FIELD_BOOL_PRESENT_NULLABLE(type, member, present_member,     \
                                             key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL,                                                   \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_HAS_PRESENCE | LONEJSON_FIELD_ACCEPT_NULL,                   \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   offsetof(type, present_member),                                             \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a nested JSON object into a nested struct member using another mapping
 * table. */
#define LONEJSON_FIELD_OBJECT(type, member, key, submap_ptr)                   \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   submap_ptr,                                                                 \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a required nested JSON object into a nested struct member using another
 * mapping table. */
#define LONEJSON_FIELD_OBJECT_REQ(type, member, key, submap_ptr)               \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   submap_ptr,                                                                 \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a nested JSON object and omits it when none of its nested fields would
 * be serialized. */
#define LONEJSON_FIELD_OBJECT_OMIT_EMPTY(type, member, key, submap_ptr)        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   0u,                                                                         \
   submap_ptr,                                                                 \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of strings into a `lonejson_string_array` member. */
#define LONEJSON_FIELD_STRING_ARRAY(type, member, key, policy)                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_ARRAY,                                           \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of integers into a `lonejson_i64_array` member. */
#define LONEJSON_FIELD_I64_ARRAY(type, member, key, policy)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(lonejson_int64),                                                     \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of unsigned integers into a `lonejson_u64_array` member.
 */
#define LONEJSON_FIELD_U64_ARRAY(type, member, key, policy)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(lonejson_uint64),                                                    \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of numbers into a `lonejson_f64_array` member. */
#define LONEJSON_FIELD_F64_ARRAY(type, member, key, policy)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(double),                                                             \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of booleans into a `lonejson_bool_array` member. */
#define LONEJSON_FIELD_BOOL_ARRAY(type, member, key, policy)                   \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL_ARRAY,                                             \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(bool),                                                               \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of nested objects into a `lonejson_object_array` member.
 */
#define LONEJSON_FIELD_OBJECT_ARRAY(type, member, key, elem_type, submap_ptr,  \
                                    policy)                                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT_ARRAY,                                           \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(elem_type),                                                          \
   submap_ptr,                                                                 \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of strings and omits it when `count == 0` during
 * serialization. */
#define LONEJSON_FIELD_STRING_ARRAY_OMIT_EMPTY(type, member, key, policy)      \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_ARRAY,                                           \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps an integer array and omits it when `count == 0` during serialization.
 */
#define LONEJSON_FIELD_I64_ARRAY_OMIT_EMPTY(type, member, key, policy)         \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   sizeof(lonejson_int64),                                                     \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps an unsigned integer array and omits it when `count == 0` during
 * serialization.
 */
#define LONEJSON_FIELD_U64_ARRAY_OMIT_EMPTY(type, member, key, policy)         \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   sizeof(lonejson_uint64),                                                    \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a number array and omits it when `count == 0` during serialization. */
#define LONEJSON_FIELD_F64_ARRAY_OMIT_EMPTY(type, member, key, policy)         \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   sizeof(double),                                                             \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a boolean array and omits it when `count == 0` during serialization.
 */
#define LONEJSON_FIELD_BOOL_ARRAY_OMIT_EMPTY(type, member, key, policy)        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL_ARRAY,                                             \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   sizeof(bool),                                                               \
   NULL,                                                                       \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Maps a JSON array of nested objects and omits it when `count == 0` during
 * serialization. */
#define LONEJSON_FIELD_OBJECT_ARRAY_OMIT_EMPTY(type, member, key, elem_type,   \
                                               submap_ptr, policy)             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT_ARRAY,                                           \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   LONEJSON_FIELD_OMIT_EMPTY,                                                  \
   0u,                                                                         \
   sizeof(elem_type),                                                          \
   submap_ptr,                                                                 \
   NULL,                                                                       \
   0u,                                                                         \
   LONEJSON_SPOOL_CLASS_DEFAULT}

/** Returns the library default lonejson runtime configuration. */
lonejson_config lonejson_default_config(void);
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
/** Internal helper retained for lonejson implementation and tests. */
lonejson__spool_options lonejson__default_spool_options(void);
/** Internal helper retained for lonejson implementation and tests. */
lonejson__parse_options lonejson__default_parse_options(void);
/** Internal helper retained for lonejson implementation and tests. */
lonejson__write_options lonejson__default_write_options(void);
/** Internal helper retained for lonejson implementation and tests. */
lonejson__value_limits lonejson__default_value_limits(void);
#endif
/** Creates one configured lonejson runtime. Passing `NULL` uses defaults.
 *
 * `lonejson_new()` copies spool temp-dir strings and any custom allocator
 * vtable into runtime-owned storage. `fixed_string_scratch` remains
 * caller-owned and must stay valid for as long as the runtime may parse with
 * that scratch configured. Custom allocator `ctx` / `stats` pointers remain
 * caller-owned and must stay valid until every runtime-derived object or value
 * that can still allocate or free through that allocator has been closed or
 * cleaned up, even if the `lonejson` runtime itself has already been freed.
 * Non-NULL configs must be initialized with `lonejson_default_config()` before
 * overriding fields. Calls that only share this runtime may execute
 * concurrently from different threads as long as they operate on distinct
 * mutable handles or destination values and the runtime does not share one
 * writable `fixed_string_scratch` buffer across those calls. When
 * `fixed_string_scratch` is configured, callers must serialize concurrent
 * parse operations that could stage through that shared scratch or use
 * separate runtimes. The same allocator callbacks and allocator `ctx` are
 * shared across calls through one runtime, so callers must provide a
 * thread-safe allocator implementation when they want to share one runtime
 * concurrently across threads. lonejson serializes updates to
 * `allocator.stats` internally, so a shared stats block may also be used
 * safely across those concurrent calls. The same mutable stateful handle
 * (stream, array stream, writer, generator, spool, `lonejson_json_value`, and
 * similar objects) may move between threads but must not be called
 * concurrently without external synchronization.
 */
lonejson *lonejson_new(const lonejson_config *config, lonejson_error *error);
/** Releases one lonejson runtime. */
void lonejson_free(lonejson *runtime);
/** Initializes an error struct to the empty `OK` state. */
void lonejson_error_init(lonejson_error *error);
/** Returns lonejson's default allocator backed by the configured allocation
 * macros.
 */
lonejson_allocator lonejson_default_allocator(void);
/** Returns the empty default read result used by reader callbacks. */
lonejson_read_result lonejson_default_read_result(void);
/** Initializes a memory-buffer reader adapter.
 *
 * The adapter reads directly from the caller-owned `data` buffer without
 * copying it. Passing `NULL` with `len == 0` is valid; passing `NULL` with a
 * non-zero length makes later reads report callback failure.
 */
void lonejson_buffer_reader_init(lonejson_buffer_reader *reader,
                                 const void *data, size_t len);
/** Reader callback for `lonejson_buffer_reader`.
 *
 * Pass this function with a `lonejson_buffer_reader *` user pointer to
 * reader-backed parse, validate, visit, and rewrite APIs.
 */
lonejson_read_result
lonejson_buffer_reader_read(void *user, unsigned char *buffer, size_t capacity);
/** Returns the empty visitor with all callbacks set to `NULL`. */
lonejson_value_visitor lonejson_default_value_visitor(void);
/** Returns a zeroed path-aware arbitrary JSON value visitor. */
lonejson_path_value_visitor lonejson_default_path_value_visitor(void);
/** Initializes a mapped string-array stream field with no handler. */
void lonejson_string_array_stream_init(lonejson_string_array_stream *stream);
/** Configures callbacks for a mapped string-array stream field.
 *
 * `stream` may point at either explicit `lonejson_string_array_stream_init()`
 * output or plain zeroed storage. This helper auto-initializes the field when
 * needed and preserves the configured callbacks across later destination
 * clearing.
 */
lonejson_status lonejson_string_array_stream_set_handler(
    lonejson_string_array_stream *stream,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error);
/** Initializes a mapped object-array stream field with no handler. */
void lonejson_mapped_array_stream_init(lonejson_mapped_array_stream *stream);
/** Configures callbacks and reusable item storage for a mapped array-stream
 * field.
 *
 * `stream` may point at either explicit `lonejson_mapped_array_stream_init()`
 * output or plain zeroed storage. This helper auto-initializes the field when
 * needed and preserves the configured handler across reset and later per-parse
 * destination clearing.
 */
lonejson_status lonejson_mapped_array_stream_set_handler(
    lonejson_mapped_array_stream *stream,
    const lonejson_mapped_array_stream_handler *handler, lonejson_error *error);
/** Initializes a spool handle with the runtime's default spool policy. */
void lonejson_spooled_init(lonejson *runtime, lonejson_spooled *value);
/** Initializes a writer value stream receiver for method-style use. */
void lonejson_writer_value_stream_init(lonejson_writer_value_stream *stream);
/** Initializes a spool handle with one named runtime spool policy. Unknown
 * classes fall back to `LONEJSON_SPOOL_CLASS_DEFAULT`.
 */
void lonejson_spooled_init_class(lonejson *runtime, lonejson_spooled *value,
                                 lonejson_spool_class spool_class);
/** Internal helper used by lonejson's implementation and tests. */
#ifdef LONEJSON_INTERNAL_BUILD
void lonejson_spooled_init_with_allocator(
    lonejson_spooled *value, const lonejson__spool_options *options,
    const lonejson_allocator *allocator);
#endif
/** Clears a spool handle while preserving its configured thresholds. */
void lonejson_spooled_reset(lonejson_spooled *value);
/** Releases all resources owned by a spool handle, including any temporary
 * file. */
void lonejson_spooled_cleanup(lonejson_spooled *value);
/** Returns the logical byte size of a spooled value. */
size_t lonejson_spooled_size(const lonejson_spooled *value);
/** Returns non-zero when a spooled value has spilled beyond memory into a
 * temporary file. */
int lonejson_spooled_spilled(const lonejson_spooled *value);
/** Appends raw bytes to an initialized spooled value, spilling according to
 * the value's configured spool options.
 *
 * `value` must have been initialized with `lonejson_spooled_init()` or
 * `lonejson_spooled_init_with_allocator()`. The call preserves the current
 * read cursor; it does not implicitly rewind. Use `lonejson_spooled_rewind()`
 * before reading from the beginning.
 */
lonejson_status lonejson_spooled_append(lonejson_spooled *value,
                                        const void *data, size_t len,
                                        lonejson_error *error);
/** Rewinds a spooled value's raw read cursor to the beginning. */
lonejson_status lonejson_spooled_rewind(lonejson_spooled *value,
                                        lonejson_error *error);
/** Reads raw bytes sequentially from a spooled value. */
lonejson_read_result lonejson_spooled_read(lonejson_spooled *value,
                                           unsigned char *buffer,
                                           size_t capacity);
/** Streams the raw bytes of a spooled value to a generic sink callback. */
lonejson_status lonejson_spooled_write_to_sink(const lonejson_spooled *value,
                                               lonejson_sink_fn sink,
                                               void *user,
                                               lonejson_error *error);
/** Initializes an outbound source handle to the empty `null` state. */
void lonejson_source_init(lonejson_source *value);
/** Resets an outbound source handle to the empty `null` state. */
void lonejson_source_reset(lonejson_source *value);
/** Releases any path owned by an outbound source handle and resets it. */
void lonejson_source_cleanup(lonejson_source *value);
/** Configures an outbound source handle to read from a caller-owned `FILE *`.
 */
lonejson_status lonejson_source_set_file(lonejson_source *value, FILE *fp,
                                         lonejson_error *error);
/** Configures an outbound source handle to read from a caller-owned file
 * descriptor. */
lonejson_status lonejson_source_set_fd(lonejson_source *value, int fd,
                                       lonejson_error *error);
/** Configures an outbound source handle to reopen and stream a filesystem
 * path. lonejson owns the duplicated path string and frees it on cleanup. */
lonejson_status lonejson_source_set_path(lonejson_source *value,
                                         const char *path,
                                         lonejson_error *error);
/** Streams the raw bytes of an outbound source handle to a generic sink. Path
 * sources are opened and closed by lonejson for the duration of the call. */
lonejson_status lonejson_source_write_to_sink(const lonejson_source *value,
                                              lonejson_sink_fn sink, void *user,
                                              lonejson_error *error);
/** Returns non-zero when a source can be reopened or rewound for another
 * serialization pass. */
int lonejson_source_is_rewindable(const lonejson_source *value);
/** Initializes an embedded JSON value handle to the empty `null` state with no
 * inbound parse destination configured, using lonejson's default allocator.
 * This is the required starting state before setting parse sink, parse
 * visitor, parse capture, or outbound source configuration.
 */
void lonejson_json_value_init(lonejson *runtime, lonejson_json_value *value);
/** Internal helper used by lonejson's implementation and tests. */
#ifdef LONEJSON_INTERNAL_BUILD
void lonejson_json_value_init_with_allocator(
    lonejson_json_value *value, const lonejson_allocator *allocator);
#endif
/** Resets an embedded JSON value handle to the empty `null` state while
 * preserving its allocator and configured parse visitor limits. */
void lonejson_json_value_reset(lonejson_json_value *value);
/** Releases any storage or path owned by an embedded JSON value handle and
 * resets it while preserving its allocator and configured parse visitor
 * limits. */
void lonejson_json_value_cleanup(lonejson_json_value *value);
/** Validates one JSON value from a memory buffer, stores a compact owned copy,
 * and configures the handle to serialize from memory. Use this when retained
 * ownership is intentional; it is not the default parse path for
 * `JSON_VALUE` fields.
 */
lonejson_status lonejson_json_value_set_buffer(lonejson_json_value *value,
                                               const void *data, size_t len,
                                               lonejson_error *error);
/** Configures an embedded JSON value handle to stream inbound parsed JSON
 * bytes to a caller sink as they are validated. Callers using parse sinks must
 * keep the handle configured across parsing, typically by initializing the
 * destination first and parsing with `clear_destination = 0`. This supported
 * reuse pattern also applies when the `JSON_VALUE` field lives inside nested
 * mapped objects or mapped-array-stream item structs.
 */
lonejson_status lonejson_json_value_set_parse_sink(lonejson_json_value *value,
                                                   lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error);
/** Configures an embedded JSON value handle to deliver inbound parsed JSON as
 * structured visitor callbacks. Callers using parse visitors must keep the
 * handle configured across parsing, typically by initializing the destination
 * first and parsing with `clear_destination = 0`. This supported reuse pattern
 * also applies when the `JSON_VALUE` field lives inside nested mapped objects
 * or mapped-array-stream item structs. Visitor limits come from the runtime
 * that initialized the handle.
 */
lonejson_status
lonejson_json_value_set_parse_visitor(lonejson_json_value *value,
                                      const lonejson_value_visitor *visitor,
                                      void *user, lonejson_error *error);
/** Configures an embedded JSON value handle to deliver inbound parsed JSON as
 * structured visitor callbacks with decoded path context relative to the
 * embedded value root. The root embedded value has zero path segments.
 */
lonejson_status lonejson_json_value_set_parse_path_visitor(
    lonejson_json_value *value, const lonejson_path_value_visitor *visitor,
    void *user, lonejson_error *error);
/** Enables explicit parse-time capture of one inbound JSON value into owned
 * compact bytes. This is the opt-in storage path for callers that need to
 * retain a parsed arbitrary JSON value after decoding, including nested
 * `JSON_VALUE` fields reused with `clear_destination = 0`.
 */
lonejson_status
lonejson_json_value_enable_parse_capture(lonejson_json_value *value,
                                         lonejson_error *error);
/** Configures an embedded JSON value handle to stream from a caller-provided
 * reader callback at serialize time. Reader-backed values are consumed in one
 * pass per serialization call. */
lonejson_status lonejson_json_value_set_reader(lonejson_json_value *value,
                                               lonejson_reader_fn reader,
                                               void *user,
                                               lonejson_error *error);
/** Configures an embedded JSON value handle to read from a caller-owned
 * `FILE *`. */
lonejson_status lonejson_json_value_set_file(lonejson_json_value *value,
                                             FILE *fp, lonejson_error *error);
/** Configures an embedded JSON value handle to read from a caller-owned file
 * descriptor. */
lonejson_status lonejson_json_value_set_fd(lonejson_json_value *value, int fd,
                                           lonejson_error *error);
/** Configures an embedded JSON value handle to reopen and stream a filesystem
 * path. lonejson owns the duplicated path string and frees it on cleanup. */
lonejson_status lonejson_json_value_set_path(lonejson_json_value *value,
                                             const char *path,
                                             lonejson_error *error);
/** Streams the compact form of one embedded JSON value to a generic sink while
 * validating that the handle contains exactly one JSON value. */
lonejson_status
lonejson_json_value_write_to_sink(const lonejson_json_value *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error);
/** Returns non-zero when an embedded JSON value can be replayed for another
 * serialization pass. Reader-backed values are not rewindable. */
int lonejson_json_value_is_rewindable(const lonejson_json_value *value);

/** Returns an empty owned-buffer handle that uses the default allocator. */
lonejson_owned_buffer lonejson_default_owned_buffer(void);
/** Initializes an owned-buffer handle to its empty default state. */
void lonejson_owned_buffer_init(lonejson_owned_buffer *buffer);
/** Returns a stable string name for a `lonejson_status` value. */
const char *lonejson_status_string(lonejson_status status);

/** Result produced by `lonejson_stream_next`. */
typedef enum lonejson_stream_result {
  /** One complete top-level object was parsed into the destination. */
  LONEJSON_STREAM_OBJECT = 0,
  /** No more objects remain in the underlying stream. */
  LONEJSON_STREAM_EOF,
  /** The underlying source would block before a full object was available. */
  LONEJSON_STREAM_WOULD_BLOCK,
  /** A parse, I/O, or argument error occurred. Inspect `lonejson_stream_error`.
   */
  LONEJSON_STREAM_ERROR
} lonejson_stream_result;

/** Result produced by `lonejson_array_stream_next*`. */
typedef enum lonejson_array_stream_result {
  /** One complete selected array item was parsed into the destination. */
  LONEJSON_ARRAY_STREAM_ITEM = 0,
  /** No more items remain in the selected array and the document is valid. */
  LONEJSON_ARRAY_STREAM_EOF,
  /** The underlying source would block before an array item was available. */
  LONEJSON_ARRAY_STREAM_WOULD_BLOCK,
  /** A parse, I/O, path, or argument error occurred. */
  LONEJSON_ARRAY_STREAM_ERROR
} lonejson_array_stream_result;

/** Public state for an object-framed JSON stream cursor.
 *
 * Stream handles emit mapped top-level objects only; they are not arbitrary
 * JSON-value iterators. Handles are created by `lonejson_stream_open_*()` and
 * must be closed with `lonejson_stream_close()` or `stream->close(stream)`.
 */
struct lonejson_stream {
  /** Last stream error. Cleared when a complete object is emitted. */
  lonejson_error error;
  /** Returns the stream's last error; named to avoid the `error` field. */
  const lonejson_error *(*error_fn)(const lonejson_stream *stream);
  /** Parses the next top-level object into `dst`. */
  lonejson_stream_result (*next)(lonejson_stream *stream, void *dst,
                                 lonejson_error *error);
  /** Closes the stream and releases its resources. */
  void (*close)(lonejson_stream *stream);
};

/** Public state for a selected-array item stream cursor.
 *
 * Array-stream handles are created by `lonejson_array_stream_open_*()` and
 * must be closed with `lonejson_array_stream_close()` or
 * `stream->close(stream)`.
 */
struct lonejson_array_stream {
  /** Last array-stream error. Cleared when an item is emitted. */
  lonejson_error error;
  /** Returns the stream's last error; named to avoid the `error` field. */
  const lonejson_error *(*error_fn)(const lonejson_array_stream *stream);
  /** Selects caller-owned mapped-record reuse before processing starts. */
  lonejson_status (*set_record_reuse)(lonejson_array_stream *stream,
                                      int enabled, lonejson_error *error);
  /** Parses the next selected item through `map` into `dst`. */
  lonejson_array_stream_result (*next)(lonejson_array_stream *stream,
                                       const lonejson_map *map, void *dst,
                                       lonejson_error *error);
  /** Captures the next selected item as one compact JSON value. */
  lonejson_array_stream_result (*next_value)(lonejson_array_stream *stream,
                                             lonejson_json_value *value,
                                             lonejson_error *error);
  /** Feeds one byte chunk into a push-fed mapped-item stream. */
  lonejson_status (*push)(lonejson_array_stream *stream,
                          const lonejson_map *map, void *dst, const void *bytes,
                          size_t len, lonejson_array_stream_item_fn callback,
                          void *user, lonejson_error *error);
  /** Finalizes a push-fed mapped-item stream after source EOF. */
  lonejson_status (*finish)(lonejson_array_stream *stream,
                            const lonejson_map *map, void *dst,
                            lonejson_array_stream_item_fn callback, void *user,
                            lonejson_error *error);
  /** Feeds one byte chunk into a push-fed selected string stream. */
  lonejson_status (*push_string)(
      lonejson_array_stream *stream, const void *bytes, size_t len,
      const lonejson_array_stream_string_handler *handler, void *user,
      lonejson_error *error);
  /** Finalizes a push-fed selected string stream after source EOF. */
  lonejson_status (*finish_string)(
      lonejson_array_stream *stream,
      const lonejson_array_stream_string_handler *handler, void *user,
      lonejson_error *error);
  /** Feeds one byte chunk into a push-fed selected string-item stream. */
  lonejson_status (*push_string_items)(lonejson_array_stream *stream,
                                       const void *bytes, size_t len,
                                       lonejson_array_stream_string_fn callback,
                                       void *user, lonejson_error *error);
  /** Finalizes a push-fed selected string-item stream after source EOF. */
  lonejson_status (*finish_string_items)(
      lonejson_array_stream *stream, lonejson_array_stream_string_fn callback,
      void *user, lonejson_error *error);
  /** Closes the stream and releases its resources. */
  void (*close)(lonejson_array_stream *stream);
};

/** Action returned by a selected-array rewrite item callback. The rewriter owns
 * array framing and comma placement for all actions.
 */
typedef enum lonejson_array_rewrite_action {
  /** Emit the original selected item. */
  LONEJSON_ARRAY_REWRITE_KEEP = 0,
  /** Omit the original selected item. */
  LONEJSON_ARRAY_REWRITE_DROP,
  /** Emit `replacement` instead of the original selected item. */
  LONEJSON_ARRAY_REWRITE_REPLACE,
  /** Emit `insert` before the original selected item, then keep the item. */
  LONEJSON_ARRAY_REWRITE_INSERT_BEFORE,
  /** Keep the original selected item, then emit `insert`. */
  LONEJSON_ARRAY_REWRITE_INSERT_AFTER,
  /** Emit `replacement`, then emit `insert`. */
  LONEJSON_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER
} lonejson_array_rewrite_action;

/** Operation applied by the generic streaming JSON value rewriter. */
typedef enum lonejson_value_rewrite_action {
  /** Emit the original value unchanged. */
  LONEJSON_VALUE_REWRITE_KEEP = 0,
  /** Omit the matched value from its containing object or array. */
  LONEJSON_VALUE_REWRITE_DROP,
  /** Emit `replacement` instead of the matched value. */
  LONEJSON_VALUE_REWRITE_REPLACE,
  /** Inspect the matched value and let `replace` emit the replacement. */
  LONEJSON_VALUE_REWRITE_REPLACE_WITH
} lonejson_value_rewrite_action;

/** One outbound item supplied by a selected-array rewrite callback. Exactly one
 * source form must be set: either `map`+`src`, or `json`.
 */
typedef struct lonejson_array_rewrite_source {
  /** Map used with `src` when emitting a mapped replacement or insertion. */
  const lonejson_map *map;
  /** Pointer to a mapped struct when `map` is non-NULL. */
  const void *src;
  /** Rewindable arbitrary JSON value used when `map` and `src` are NULL. */
  const lonejson_json_value *json;
} lonejson_array_rewrite_source;

/** Replacement value supplied to a generic value rewrite operation.
 *
 * Exactly one source form must be configured. `emit` is the most general form
 * and receives a streaming writer for one replacement JSON value. `map`+`src`
 * and `json` are convenience forms for mapped structs and rewindable arbitrary
 * JSON values.
 */
typedef struct lonejson_value_rewrite_replacement {
  /** Callback used to emit one replacement JSON value dynamically. */
  lonejson_value_rewrite_emit_fn emit;
  /** User data passed to `emit`. */
  void *emit_user;
  /** Map used with `src` when emitting a mapped replacement. */
  const lonejson_map *map;
  /** Pointer to a mapped struct when `map` is non-NULL. */
  const void *src;
  /** Rewindable arbitrary JSON value used when `map` and `src` are NULL. */
  const lonejson_json_value *json;
} lonejson_value_rewrite_replacement;

/** Options for rewriting one arbitrary JSON value by escaped selector string.
 *
 * `selector` uses dot-separated segments (`meta.request.id`). Use backslash to
 * escape literal dots or backslashes inside a segment. An empty selector or
 * `NULL` selects the root value. The selector is parsed for the duration of
 * the call; callers do not need to allocate a segment array.
 */
typedef struct lonejson_value_rewrite_selector_options {
  /** Dot-separated target selector, or empty/NULL for root. */
  const char *selector;
  /** Rewrite action to apply at the selected path. */
  lonejson_value_rewrite_action action;
  /** Replacement value used by `REPLACE`. */
  lonejson_value_rewrite_replacement replacement;
  /** Optional visitor that receives the matched old value for `REPLACE_WITH`.
   * Visitor events are streamed and balanced; the complete old value is not
   * materialized.
   */
  const lonejson_value_visitor *old_value_visitor;
  /** Opaque user pointer passed to `old_value_visitor`. */
  void *old_value_user;
  /** Callback that emits one replacement for `REPLACE_WITH`. */
  lonejson_value_rewrite_replace_fn replace;
  /** Opaque user pointer passed to `replace`. */
  void *replace_user;
} lonejson_value_rewrite_selector_options;

/** Options for rewriting one arbitrary JSON value by normalized path.
 *
 * `target_segments` names one value in the input tree. Object segments match
 * decoded object member names. Array segments are decimal zero-based indexes.
 * Empty paths select the root value. Missing descendant object chains are
 * synthesized for `REPLACE` operations when all missing segments are object
 * names rather than array indexes.
 */
typedef struct lonejson_value_rewrite_options {
  /** Normalized target path segments. */
  const char *const *target_segments;
  /** Number of entries in `target_segments`. */
  size_t target_segment_count;
  /** Rewrite action to apply at the target path. */
  lonejson_value_rewrite_action action;
  /** Replacement value used by `REPLACE`. */
  lonejson_value_rewrite_replacement replacement;
  /** Optional visitor that receives the matched old value for `REPLACE_WITH`.
   * Visitor events are streamed and balanced; the complete old value is not
   * materialized.
   */
  const lonejson_value_visitor *old_value_visitor;
  /** Opaque user pointer passed to `old_value_visitor`. */
  void *old_value_user;
  /** Callback that emits one replacement for `REPLACE_WITH`. */
  lonejson_value_rewrite_replace_fn replace;
  /** Opaque user pointer passed to `replace`. */
  void *replace_user;
} lonejson_value_rewrite_options;

/** Mutable result initialized to `KEEP` before each item callback. */
typedef struct lonejson_array_rewrite_result {
  /** Requested operation for the current selected array item. */
  lonejson_array_rewrite_action action;
  /** Replacement item used by replace actions. */
  lonejson_array_rewrite_source replacement;
  /** Additional item used by insert actions. */
  lonejson_array_rewrite_source insert;
} lonejson_array_rewrite_result;

/** Bounded parent context for a `[]` selector segment such as `boards[]`.
 * `dst` is reused for one active parent item and is updated in source JSON
 * order as fields are parsed. Fields that appear after the selected child
 * array are not visible to child item callbacks until later.
 */
typedef struct lonejson_array_rewrite_parent {
  /** Selector segment name without the trailing `[]`. */
  const char *segment;
  /** Map used to parse the active parent item. */
  const lonejson_map *map;
  /** Reusable destination for the active parent item. */
  void *dst;
} lonejson_array_rewrite_parent;

/** Context passed to rewrite callbacks. Parent entries are ordered from the
 * root toward the selected array. Pointers remain valid only for the callback.
 */
typedef struct lonejson_array_rewrite_context {
  /** Selector passed to the rewrite entry point. */
  const char *selector;
  /** Active parent contexts configured by the caller. */
  const lonejson_array_rewrite_parent *parents;
  /** Number of entries in `parents`. */
  size_t parent_count;
} lonejson_array_rewrite_context;

/** Emits one appended selected-array item. */
typedef lonejson_status (*lonejson_array_rewrite_emit_fn)(
    void *emit_user, const lonejson_array_rewrite_source *source,
    lonejson_error *error);

/** Called once for each selected array item. If `item_map` is configured in
 * the options, `item` points at the reusable mapped item destination. If
 * `item_value` is configured instead, `item` points at that
 * `lonejson_json_value`.
 */
typedef lonejson_status (*lonejson_array_rewrite_item_fn)(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error);

/** Called after all existing items in one selected array have been processed.
 * Use `emit` to append zero or more items; callers never emit array framing or
 * commas directly.
 */
typedef lonejson_status (*lonejson_array_rewrite_append_fn)(
    void *user, const lonejson_array_rewrite_context *context,
    lonejson_array_rewrite_emit_fn emit, void *emit_user,
    lonejson_error *error);

/** Options for selected-array streaming rewrite. `selector == NULL` or `""`
 * selects a root array. Object paths use dot-separated keys. Array fanout must
 * be explicit with `[]`, for example `boards[].items`; plain dotted paths do
 * not implicitly fan out through arrays.
 *
 * Output is validated input re-emitted as compact canonical JSON. Unselected
 * parts are streamed through the visitor pipeline rather than materialized.
 *
 * When `item` is configured, configure exactly one item delivery mode:
 * `item_map`+`item_dst`, or `item_value`. Append-only rewrites may omit `item`
 * and item delivery. Replacement, inserted, and appended items can be supplied
 * from mapped structs or rewindable `lonejson_json_value` handles.
 */
typedef struct lonejson_array_rewrite_options {
  /** Optional map used to parse each selected item into `item_dst`. */
  const lonejson_map *item_map;
  /** Reusable destination for mapped item delivery. */
  void *item_dst;
  /** Optional reusable JSON value handle for raw item delivery. */
  lonejson_json_value *item_value;
  /** Optional parent mappings for explicit `[]` selector segments. */
  const lonejson_array_rewrite_parent *parents;
  /** Number of entries in `parents`. */
  size_t parent_count;
  /** Optional callback invoked for each selected item. */
  lonejson_array_rewrite_item_fn item;
  /** Optional callback invoked after one selected array has been scanned. */
  lonejson_array_rewrite_append_fn append;
  /** Opaque user pointer passed to `item` and `append`. */
  void *user;
} lonejson_array_rewrite_options;

/** Opens an object-framed JSON stream over a caller-supplied reader callback.
 */
lonejson_stream *lonejson_stream_open_reader(lonejson *runtime,
                                             const lonejson_map *map,
                                             lonejson_reader_fn reader,
                                             void *user, lonejson_error *error);
/** Opens an object-framed JSON stream over an open `FILE *`. */
lonejson_stream *lonejson_stream_open_filep(lonejson *runtime,
                                            const lonejson_map *map, FILE *fp,
                                            lonejson_error *error);
/** Opens an object-framed JSON stream over a filesystem path. */
lonejson_stream *lonejson_stream_open_path(lonejson *runtime,
                                           const lonejson_map *map,
                                           const char *path,
                                           lonejson_error *error);
/** Opens an object-framed JSON stream over a file descriptor, including Unix
 * domain sockets. */
lonejson_stream *lonejson_stream_open_fd(lonejson *runtime,
                                         const lonejson_map *map, int fd,
                                         lonejson_error *error);
/** Parses the next top-level JSON object from a stream into `dst`. Whitespace
 * between objects is ignored and EOF after the last object is reported
 * separately. */
lonejson_stream_result lonejson_stream_next(lonejson_stream *stream, void *dst,
                                            lonejson_error *error);
/** Returns the stream's last error state. */
const lonejson_error *lonejson_stream_error(const lonejson_stream *stream);
/** Closes a stream and releases resources it owns. */
void lonejson_stream_close(lonejson_stream *stream);

/** Opens a streaming cursor over an array selected by a direct root object key.
 * `path == NULL` or `""` selects a root array. Non-empty v1 paths must be one
 * object-key segment such as `boards` or `items`; dotted paths are rejected
 * instead of implicitly fanning out through arrays.
 */
lonejson_array_stream *
lonejson_array_stream_open_reader(lonejson *runtime, const char *path,
                                  lonejson_reader_fn reader, void *user,
                                  lonejson_error *error);
/** Opens an array-item stream over an open `FILE *`. */
lonejson_array_stream *lonejson_array_stream_open_filep(lonejson *runtime,
                                                        const char *path,
                                                        FILE *fp,
                                                        lonejson_error *error);
/** Opens an array-item stream over a filesystem path. */
lonejson_array_stream *lonejson_array_stream_open_path(lonejson *runtime,
                                                       const char *array_path,
                                                       const char *path,
                                                       lonejson_error *error);
/** Opens an array-item stream over a file descriptor. */
lonejson_array_stream *lonejson_array_stream_open_fd(lonejson *runtime,
                                                     const char *path, int fd,
                                                     lonejson_error *error);
/** Opens a push-fed array-item stream for write-callback style sources. Feed
 * bytes with `lonejson_array_stream_push` and validate EOF with
 * `lonejson_array_stream_finish`.
 */
lonejson_array_stream *lonejson_array_stream_open_push(lonejson *runtime,
                                                       const char *path,
                                                       lonejson_error *error);
/** Selects caller-owned mapped-record reuse before processing this stream.
 *
 * `enabled == 1` suppresses destination initialization and automatic cleanup
 * after push callbacks. Supply initialized storage, reset it between items
 * with `lonejson_reset()`, and perform final cleanup with `lonejson_cleanup()`,
 * including after parse errors or callback cancellation. Closing the stream
 * does not clean the record. No automatic reset is introduced.
 *
 * `enabled == 0` is the default and inherits the runtime policy captured when
 * the stream opened, including existing push callback cleanup. Other streams
 * and the owning runtime are unaffected. Raw-value and string delivery are
 * unchanged. Works with every array-stream constructor.
 *
 * Call before the first next, push or finish operation, including raw/string
 * variants. A zero-length push or a WOULD_BLOCK result starts processing too.
 * NULL streams, values other than 0/1, and late configuration return
 * `LONEJSON_STATUS_INVALID_ARGUMENT` without changing processing state or
 * reuse policy. The supplied `error` receives the configuration diagnostic.
 */
lonejson_status
lonejson_array_stream_set_record_reuse(lonejson_array_stream *stream,
                                       int enabled, lonejson_error *error);
/** Parses the next selected array item into `dst` through `map`. */
lonejson_array_stream_result
lonejson_array_stream_next(lonejson_array_stream *stream,
                           const lonejson_map *map, void *dst,
                           lonejson_error *error);
/** Captures the next selected array item as a validated compact JSON value. */
lonejson_array_stream_result
lonejson_array_stream_next_value(lonejson_array_stream *stream,
                                 lonejson_json_value *value,
                                 lonejson_error *error);
/** Feeds response bytes into a push-fed selected-array stream. Each complete
 * item is parsed directly into `dst` through `map`, then reported to
 * `callback`. This function consumes bounded chunks; it does not materialize
 * the complete response or selected array.
 */
lonejson_status lonejson_array_stream_push(
    lonejson_array_stream *stream, const lonejson_map *map, void *dst,
    const void *bytes, size_t len, lonejson_array_stream_item_fn callback,
    void *user, lonejson_error *error);
/** Finalizes a push-fed selected-array stream after the source reaches EOF. */
lonejson_status lonejson_array_stream_finish(
    lonejson_array_stream *stream, const lonejson_map *map, void *dst,
    lonejson_array_stream_item_fn callback, void *user, lonejson_error *error);
/** Feeds bytes into a push-fed selected string-array stream. String item bytes
 * are decoded and delivered incrementally through `handler`.
 */
lonejson_status lonejson_array_stream_push_string(
    lonejson_array_stream *stream, const void *bytes, size_t len,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error);
/** Finalizes a push-fed selected string-array stream after source EOF. */
lonejson_status lonejson_array_stream_finish_string(
    lonejson_array_stream *stream,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error);
/** Feeds bytes into a push-fed selected string-array stream and invokes
 * `callback` once per decoded string item. Each item is assembled through the
 * chunked string path into bounded temporary storage; the complete response and
 * selected array are never materialized.
 */
lonejson_status lonejson_array_stream_push_string_items(
    lonejson_array_stream *stream, const void *bytes, size_t len,
    lonejson_array_stream_string_fn callback, void *user,
    lonejson_error *error);
/** Finalizes a push-fed selected string-array item stream after source EOF. */
lonejson_status lonejson_array_stream_finish_string_items(
    lonejson_array_stream *stream, lonejson_array_stream_string_fn callback,
    void *user, lonejson_error *error);
/** Returns the cursor's last error state. */
const lonejson_error *
lonejson_array_stream_error(const lonejson_array_stream *stream);
/** Closes an array-item stream and releases resources it owns. */
void lonejson_array_stream_close(lonejson_array_stream *stream);

/** Rewrites one selected JSON array from a caller-provided reader to a generic
 * sink. The document is validated and emitted incrementally; the complete
 * document and selected arrays are never materialized. The reader must behave
 * like a blocking or immediately-available source for the duration of the
 * call. `would_block` is rejected because this helper does not retain
 * resumable reader state across calls.
 */
lonejson_status lonejson_array_rewrite_reader(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, lonejson_sink_fn sink, void *sink_user,
    const lonejson_array_rewrite_options *options, lonejson_error *error);
/** Rewrites one selected array from a caller-provided reader to an open
 * `FILE *` output.
 */
lonejson_status lonejson_array_rewrite_reader_to_filep(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, FILE *output,
    const lonejson_array_rewrite_options *options, lonejson_error *error);
/** Rewrites one selected array from a caller-provided reader to a file
 * descriptor output.
 */
lonejson_status lonejson_array_rewrite_reader_to_fd(
    lonejson *runtime, const char *selector, lonejson_reader_fn reader,
    void *reader_user, int fd, const lonejson_array_rewrite_options *options,
    lonejson_error *error);
/** Rewrites one selected array from an open `FILE *` to a generic sink. */
lonejson_status
lonejson_array_rewrite_filep(lonejson *runtime, const char *selector, FILE *fp,
                             lonejson_sink_fn sink, void *sink_user,
                             const lonejson_array_rewrite_options *options,
                             lonejson_error *error);
/** Rewrites one selected array from an open `FILE *` to an open `FILE *`. */
lonejson_status lonejson_array_rewrite_filep_to_filep(
    lonejson *runtime, const char *selector, FILE *input, FILE *output,
    const lonejson_array_rewrite_options *options, lonejson_error *error);
/** Rewrites one selected array from a file descriptor to a generic sink. */
lonejson_status
lonejson_array_rewrite_fd(lonejson *runtime, const char *selector, int fd,
                          lonejson_sink_fn sink, void *sink_user,
                          const lonejson_array_rewrite_options *options,
                          lonejson_error *error);
/** Rewrites one selected array from a file descriptor to a file descriptor. */
lonejson_status lonejson_array_rewrite_fd_to_fd(
    lonejson *runtime, const char *selector, int input_fd, int output_fd,
    const lonejson_array_rewrite_options *options, lonejson_error *error);
/** Rewrites one selected array from an input path to an output path. Atomic
 * rename and transaction boundaries remain caller responsibility.
 */
lonejson_status
lonejson_array_rewrite_path(lonejson *runtime, const char *selector,
                            const char *input_path, const char *output_path,
                            const lonejson_array_rewrite_options *options,
                            lonejson_error *error);

/** Rewrites one arbitrary JSON value from a reader to a sink.
 *
 * The input is validated and re-emitted as compact canonical JSON. The complete
 * input and output documents are not materialized. Replacement values are
 * emitted through lonejson serializers, so callers do not write JSON syntax.
 * The reader must behave like a blocking or immediately-available source for
 * the duration of the call. `would_block` is rejected because this helper does
 * not retain resumable reader state across calls.
 */
lonejson_status lonejson_value_rewrite_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_options *options, lonejson_error *error);
/** Rewrites one arbitrary JSON value from a caller-owned memory buffer to a
 * sink. This is a reader adapter convenience; input and output are still
 * processed incrementally and the full document is not materialized.
 */
lonejson_status
lonejson_value_rewrite_buffer(lonejson *runtime, const void *data, size_t len,
                              lonejson_sink_fn sink, void *sink_user,
                              const lonejson_value_rewrite_options *options,
                              lonejson_error *error);
/** Rewrites one arbitrary JSON value from an open `FILE *` to a sink. */
lonejson_status lonejson_value_rewrite_filep(
    lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_options *options, lonejson_error *error);
/** Rewrites one arbitrary JSON value from a file descriptor to a sink. */
lonejson_status lonejson_value_rewrite_fd(
    lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_options *options, lonejson_error *error);
/** Rewrites one arbitrary JSON value from an input path to an output path. */
lonejson_status lonejson_value_rewrite_path(
    lonejson *runtime, const char *input_path, const char *output_path,
    const lonejson_value_rewrite_options *options, lonejson_error *error);
/** Rewrites one arbitrary JSON value using an escaped selector string.
 *
 * The reader must behave like a blocking or immediately-available source for
 * the duration of the call. `would_block` is rejected because this helper does
 * not retain resumable reader state across calls.
 */
lonejson_status lonejson_value_rewrite_selector_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
/** Rewrites one arbitrary JSON value from a caller-owned memory buffer using
 * an escaped selector string.
 */
lonejson_status lonejson_value_rewrite_selector_buffer(
    lonejson *runtime, const void *data, size_t len, lonejson_sink_fn sink,
    void *sink_user, const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
/** Rewrites from an open `FILE *` using an escaped selector string. */
lonejson_status lonejson_value_rewrite_selector_filep(
    lonejson *runtime, FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
/** Rewrites from a file descriptor using an escaped selector string. */
lonejson_status lonejson_value_rewrite_selector_fd(
    lonejson *runtime, int fd, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);
/** Rewrites from an input path to an output path using an escaped selector. */
lonejson_status lonejson_value_rewrite_selector_path(
    lonejson *runtime, const char *input_path, const char *output_path,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error);

/** Returns the default SSE parser options. */
lonejson_sse_options lonejson_default_sse_options(void);
/** Opens an incremental Server-Sent Events parser. */
lonejson_sse *lonejson_sse_open(const lonejson_sse_options *options,
                                lonejson_error *error);
/** Pushes arbitrary SSE bytes and streams zero or more event payload chunks. */
lonejson_status lonejson_sse_push(lonejson_sse *sse, const void *bytes,
                                  size_t len,
                                  const lonejson_sse_handler *handler,
                                  void *user, lonejson_error *error);
/** Finishes an SSE stream, dispatching a trailing unterminated event when one
 * is in progress.
 */
lonejson_status lonejson_sse_finish(lonejson_sse *sse,
                                    const lonejson_sse_handler *handler,
                                    void *user, lonejson_error *error);
/** Pushes SSE bytes, decodes selected event data as JSON, and calls `event_cb`
 * after each decoded JSON event.
 */
lonejson_status lonejson_sse_push_json(lonejson *runtime, lonejson_sse *sse,
                                       const lonejson_map *map, void *dst,
                                       const void *bytes, size_t len,
                                       const lonejson_sse_json_options *options,
                                       lonejson_sse_json_event_fn event_cb,
                                       void *user, lonejson_error *error);
/** Finishes an SSE JSON stream. */
lonejson_status lonejson_sse_finish_json(
    lonejson *runtime, lonejson_sse *sse, const lonejson_map *map, void *dst,
    const lonejson_sse_json_options *options,
    lonejson_sse_json_event_fn event_cb, void *user, lonejson_error *error);
/** Pushes SSE bytes, decodes selected event data as one arbitrary JSON value,
 * and calls `event_cb` after each decoded JSON event. The caller must
 * configure `value` for parse sink, parse visitor, or explicit capture before
 * starting the stream. Runtime parse defaults come from `lj`; per-event
 * runtime payload is cleared automatically while the configured parse mode
 * remains attached to `value`.
 */
lonejson_status lonejson_sse_push_json_value(
    lonejson *runtime, lonejson_sse *sse, lonejson_json_value *value,
    const void *bytes, size_t len, const lonejson_sse_json_options *options,
    lonejson_sse_json_value_event_fn event_cb, void *user,
    lonejson_error *error);
/** Finishes an SSE JSON-value stream. */
lonejson_status
lonejson_sse_finish_json_value(lonejson *runtime, lonejson_sse *sse,
                               lonejson_json_value *value,
                               const lonejson_sse_json_options *options,
                               lonejson_sse_json_value_event_fn event_cb,
                               void *user, lonejson_error *error);
/** Releases an SSE parser. */
void lonejson_sse_close(lonejson_sse *sse);

/** Returns the default multipart parser options. */
lonejson_multipart_options lonejson_default_multipart_options(void);
/** Opens an incremental MIME multipart parser from a Content-Type value that
 * contains a `boundary=` parameter.
 */
lonejson_multipart *
lonejson_multipart_open(const char *content_type,
                        const lonejson_multipart_options *options,
                        lonejson_error *error);
/** Pushes arbitrary multipart bytes and emits part lifecycle callbacks. */
lonejson_status
lonejson_multipart_push(lonejson_multipart *multipart, const void *bytes,
                        size_t len, const lonejson_multipart_handler *handler,
                        void *user, lonejson_error *error);
/** Finishes a multipart stream and fails if the closing boundary was not
 * observed.
 */
lonejson_status
lonejson_multipart_finish(lonejson_multipart *multipart,
                          const lonejson_multipart_handler *handler, void *user,
                          lonejson_error *error);
/** Releases a multipart parser. */
void lonejson_multipart_close(lonejson_multipart *multipart);

/** Parses a JSON buffer into a mapped struct. */
lonejson_status lonejson_parse_buffer(lonejson *runtime,
                                      const lonejson_map *map, void *dst,
                                      const void *data, size_t len,
                                      lonejson_error *error);
/** Parses a NUL-terminated JSON string into a mapped struct. */
lonejson_status lonejson_parse_cstr(lonejson *runtime, const lonejson_map *map,
                                    void *dst, const char *json,
                                    lonejson_error *error);
/** Parses JSON from a caller-supplied reader callback into a mapped struct.
 *
 * The reader must behave like a blocking or immediately-available source for
 * the duration of the call. `would_block` is rejected because this helper does
 * not retain resumable parse state across calls.
 */
lonejson_status lonejson_parse_reader(lonejson *runtime,
                                      const lonejson_map *map, void *dst,
                                      lonejson_reader_fn reader, void *user,
                                      lonejson_error *error);
/** Parses JSON from an open `FILE *` stream into a mapped struct. */
lonejson_status lonejson_parse_filep(lonejson *runtime, const lonejson_map *map,
                                     void *dst, FILE *fp,
                                     lonejson_error *error);
/** Parses JSON from a filesystem path into a mapped struct. */
lonejson_status lonejson_parse_path(lonejson *runtime, const lonejson_map *map,
                                    void *dst, const char *path,
                                    lonejson_error *error);

/** Validates that a buffer contains a syntactically valid JSON document. */
lonejson_status lonejson_validate_buffer(lonejson *runtime, const void *data,
                                         size_t len, lonejson_error *error);
/** Validates that a NUL-terminated string contains syntactically valid JSON. */
lonejson_status lonejson_validate_cstr(lonejson *runtime, const char *json,
                                       lonejson_error *error);
/** Validates JSON produced by a caller-supplied reader callback.
 *
 * The reader must behave like a blocking or immediately-available source for
 * the duration of the call. `would_block` is rejected because this helper does
 * not retain resumable validation state across calls.
 */
lonejson_status lonejson_validate_reader(lonejson *runtime,
                                         lonejson_reader_fn reader, void *user,
                                         lonejson_error *error);
/** Validates JSON from an open `FILE *` stream. */
lonejson_status lonejson_validate_filep(lonejson *runtime, FILE *fp,
                                        lonejson_error *error);
/** Validates JSON from a filesystem path. */
lonejson_status lonejson_validate_path(lonejson *runtime, const char *path,
                                       lonejson_error *error);

/** Visits exactly one JSON value from a caller-provided buffer. The call
 * fails on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
lonejson_status
lonejson_visit_value_buffer(lonejson *runtime, const void *data, size_t len,
                            const lonejson_value_visitor *visitor, void *user,
                            lonejson_error *error);
/** Visits exactly one JSON value from a NUL-terminated string. The call fails
 * on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
lonejson_status lonejson_visit_value_cstr(lonejson *runtime, const char *json,
                                          const lonejson_value_visitor *visitor,
                                          void *user, lonejson_error *error);
/** Visits exactly one JSON value from a caller-provided reader callback. The
 * call fails on malformed JSON or on trailing non-whitespace bytes after the
 * first complete value. The reader must behave like a blocking or
 * immediately-available source for the duration of the call. `would_block` is
 * rejected because this helper does not retain resumable visitor state across
 * calls.
 */
lonejson_status lonejson_visit_value_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_value_visitor *visitor, void *user, lonejson_error *error);
/** Visits exactly one JSON value from an open `FILE *`. The call fails on
 * malformed JSON or on trailing non-whitespace bytes after the first complete
 * value.
 */
lonejson_status
lonejson_visit_value_filep(lonejson *runtime, FILE *fp,
                           const lonejson_value_visitor *visitor, void *user,
                           lonejson_error *error);
/** Visits exactly one JSON value from a filesystem path. The call fails on
 * malformed JSON or on trailing non-whitespace bytes after the first complete
 * value.
 */
lonejson_status lonejson_visit_value_path(lonejson *runtime, const char *path,
                                          const lonejson_value_visitor *visitor,
                                          void *user, lonejson_error *error);
/** Visits exactly one JSON value from a file descriptor, including Unix domain
 * sockets. The call fails on malformed JSON or on trailing non-whitespace
 * bytes after the first complete value.
 */
lonejson_status lonejson_visit_value_fd(lonejson *runtime, int fd,
                                        const lonejson_value_visitor *visitor,
                                        void *user, lonejson_error *error);
/** Visits exactly one JSON value from a caller-provided buffer and supplies the
 * normalized current path with every path-aware visitor callback. The call
 * fails on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
lonejson_status
lonejson_visit_path_value_buffer(lonejson *runtime, const void *data,
                                 size_t len,
                                 const lonejson_path_value_visitor *visitor,
                                 void *user, lonejson_error *error);
/** Visits exactly one JSON value from a NUL-terminated string and supplies the
 * normalized current path with every path-aware visitor callback.
 */
lonejson_status
lonejson_visit_path_value_cstr(lonejson *runtime, const char *json,
                               const lonejson_path_value_visitor *visitor,
                               void *user, lonejson_error *error);
/** Visits exactly one JSON value from a caller-provided reader callback and
 * supplies the normalized current path with every path-aware visitor callback.
 */
lonejson_status
lonejson_visit_path_value_reader(lonejson *runtime, lonejson_reader_fn reader,
                                 void *reader_user,
                                 const lonejson_path_value_visitor *visitor,
                                 void *user, lonejson_error *error);
/** Visits exactly one JSON value from an open `FILE *` and supplies the
 * normalized current path with every path-aware visitor callback.
 */
lonejson_status
lonejson_visit_path_value_filep(lonejson *runtime, FILE *fp,
                                const lonejson_path_value_visitor *visitor,
                                void *user, lonejson_error *error);
/** Visits exactly one JSON value from a filesystem path and supplies the
 * normalized current path with every path-aware visitor callback.
 */
lonejson_status
lonejson_visit_path_value_path(lonejson *runtime, const char *path,
                               const lonejson_path_value_visitor *visitor,
                               void *user, lonejson_error *error);
/** Visits exactly one JSON value from a file descriptor and supplies the
 * normalized current path with every path-aware visitor callback.
 */
lonejson_status
lonejson_visit_path_value_fd(lonejson *runtime, int fd,
                             const lonejson_path_value_visitor *visitor,
                             void *user, lonejson_error *error);

/** Serializes a mapped struct to a generic output sink callback using runtime
 * write policy.
 */
lonejson_status lonejson_serialize_sink(lonejson *runtime,
                                        const lonejson_map *map,
                                        const void *src, lonejson_sink_fn sink,
                                        void *user, lonejson_error *error);
/** Initializes a pull-style JSON generator for one mapped struct.
 *
 * The generator stays bounded and never materializes the whole payload. Drain
 * it incrementally through `lonejson_generator_read()` and release it with
 * `lonejson_generator_cleanup()`. It supports the normal mapped serializer
 * surface plus `lonejson_source`, `lonejson_spooled`, and `lonejson_json_value`
 * fields without silently buffering them into one giant payload. The init call
 * fully establishes the generator state; callers do not need to pre-zero the
 * struct.
 */
lonejson_status lonejson_generator_init(lonejson *runtime,
                                        lonejson_generator *generator,
                                        const lonejson_map *map,
                                        const void *src);
/** Measures the serialized byte length of one mapped struct without retaining
 * the generated JSON.
 *
 * The document must be replayable: path, buffer, spool, and seekable file or
 * file-descriptor sources are accepted, while reader-backed JSON values are
 * rejected because measuring would consume them.
 */
lonejson_status lonejson_generator_measure(lonejson *runtime,
                                           const lonejson_map *map,
                                           const void *src, size_t *out_len,
                                           lonejson_error *error);
/** Pulls the next serialized JSON bytes from a generator.
 *
 * `out_len` receives the produced byte count and `out_eof` becomes non-zero
 * once the generator has fully drained.
 */
lonejson_status lonejson_generator_read(lonejson_generator *generator,
                                        unsigned char *buffer, size_t capacity,
                                        size_t *out_len, int *out_eof);
/** Releases resources owned by a pull-style JSON generator. */
void lonejson_generator_cleanup(lonejson_generator *generator);

/** Initializes a streaming JSON writer that emits directly to a sink.
 *
 * The writer owns JSON structure and escaping but does not own the sink. The
 * output remains streaming: bytes are forwarded to `sink` as writer events are
 * accepted, without materializing the full document.
 */
lonejson_status lonejson_writer_init_sink(lonejson *runtime,
                                          lonejson_writer *writer,
                                          lonejson_sink_fn sink,
                                          void *sink_user,
                                          lonejson_error *error);
/** Releases resources owned by an initialized streaming writer. */
void lonejson_writer_cleanup(lonejson_writer *writer);
/** Begins a JSON object value at the current writer position. */
lonejson_status lonejson_writer_begin_object(lonejson_writer *writer,
                                             lonejson_error *error);
/** Ends the current JSON object. The call fails if a key has no value. */
lonejson_status lonejson_writer_end_object(lonejson_writer *writer,
                                           lonejson_error *error);
/** Begins a JSON array value at the current writer position. */
lonejson_status lonejson_writer_begin_array(lonejson_writer *writer,
                                            lonejson_error *error);
/** Ends the current JSON array. */
lonejson_status lonejson_writer_end_array(lonejson_writer *writer,
                                          lonejson_error *error);
/** Emits an object member key for the current object.
 *
 * `key` may contain arbitrary UTF-8 bytes except embedded NULs that the caller
 * intentionally includes in `key_len`; lonejson handles required JSON string
 * escaping. A value event must follow before another key or object end.
 */
lonejson_status lonejson_writer_key(lonejson_writer *writer, const char *key,
                                    size_t key_len, lonejson_error *error);
/** Emits one JSON string value from a caller buffer. */
lonejson_status lonejson_writer_string(lonejson_writer *writer,
                                       const void *data, size_t len,
                                       lonejson_error *error);
/** Begins a JSON string value for chunked emission.
 *
 * After this succeeds, emit zero or more raw text chunks with
 * `lonejson_writer_string_chunk()` and close the value with
 * `lonejson_writer_string_end()`. This is the low-level form used when the
 * producer already has a streaming string source and must not materialize the
 * complete string.
 */
lonejson_status lonejson_writer_string_begin(lonejson_writer *writer,
                                             lonejson_error *error);
/** Emits one raw text chunk inside an open chunked JSON string value. */
lonejson_status lonejson_writer_string_chunk(lonejson_writer *writer,
                                             const void *data, size_t len,
                                             lonejson_error *error);
/** Ends a JSON string value opened by `lonejson_writer_string_begin()`. */
lonejson_status lonejson_writer_string_end(lonejson_writer *writer,
                                           lonejson_error *error);
/** Emits one JSON string value by streaming raw text bytes from a reader.
 *
 * When used from a writer generator, `LONEJSON_STATUS_TRUNCATED` means output
 * backpressure paused the active string before another reader chunk was
 * consumed. Retry this function with the same `reader` and `reader_user` to
 * resume the same string value.
 */
lonejson_status lonejson_writer_string_reader(lonejson_writer *writer,
                                              lonejson_reader_fn reader,
                                              void *reader_user,
                                              lonejson_error *error);
/** Emits one JSON string value from a rewindable spooled byte container. */
lonejson_status lonejson_writer_string_spooled(lonejson_writer *writer,
                                               const lonejson_spooled *value,
                                               lonejson_error *error);
/** Emits one JSON string value from a rewindable outbound source handle. */
lonejson_status lonejson_writer_source_text(lonejson_writer *writer,
                                            const lonejson_source *value,
                                            lonejson_error *error);
/** Emits one JSON string value containing base64-encoded spooled bytes. */
lonejson_status lonejson_writer_spooled_base64(lonejson_writer *writer,
                                               const lonejson_spooled *value,
                                               lonejson_error *error);
/** Emits one JSON string value containing base64-encoded outbound source
 * bytes.
 */
lonejson_status lonejson_writer_source_base64(lonejson_writer *writer,
                                              const lonejson_source *value,
                                              lonejson_error *error);
/** Emits one already-validated JSON number token.
 *
 * The token is validated by lonejson before emission. Non-number JSON text and
 * number text with trailing bytes are rejected.
 */
lonejson_status lonejson_writer_number_text(lonejson_writer *writer,
                                            const char *data, size_t len,
                                            lonejson_error *error);
/** Begins a JSON number value for chunked emission.
 *
 * After this succeeds, append one or more raw number token chunks with
 * `lonejson_writer_number_chunk()` and close the value with
 * `lonejson_writer_number_end()`. The complete token is validated at end and
 * emitted only when it is a valid JSON number. If any chunk append fails, the
 * writer enters a failed state and `lonejson_writer_number_end()` rejects the
 * number without emitting a truncated prefix.
 */
lonejson_status lonejson_writer_number_begin(lonejson_writer *writer,
                                             lonejson_error *error);
/** Appends one raw number token chunk inside an open chunked JSON number.
 *
 * Failure marks the writer failed. After that, callers should clean up the
 * writer; later writer operations, including `lonejson_writer_number_end()`,
 * fail without committing buffered partial number text.
 */
lonejson_status lonejson_writer_number_chunk(lonejson_writer *writer,
                                             const char *data, size_t len,
                                             lonejson_error *error);
/** Ends and validates a JSON number opened by `lonejson_writer_number_begin()`.
 *
 * This emits the complete buffered number only if no prior chunk failed and the
 * complete token is valid JSON number text.
 */
lonejson_status lonejson_writer_number_end(lonejson_writer *writer,
                                           lonejson_error *error);
/** Emits one signed 64-bit JSON integer value. */
lonejson_status lonejson_writer_i64(lonejson_writer *writer,
                                    lonejson_int64 value,
                                    lonejson_error *error);
/** Emits one unsigned 64-bit JSON integer value. */
lonejson_status lonejson_writer_u64(lonejson_writer *writer,
                                    lonejson_uint64 value,
                                    lonejson_error *error);
/** Emits one finite double-precision JSON number value. */
lonejson_status lonejson_writer_f64(lonejson_writer *writer, double value,
                                    lonejson_error *error);
/** Emits one JSON boolean value. */
lonejson_status lonejson_writer_bool(lonejson_writer *writer, int value,
                                     lonejson_error *error);
/** Emits one JSON null value. */
lonejson_status lonejson_writer_null(lonejson_writer *writer,
                                     lonejson_error *error);
/** Emits one arbitrary JSON value from a `lonejson_json_value` handle. */
lonejson_status lonejson_writer_json_value(lonejson_writer *writer,
                                           const lonejson_json_value *value,
                                           lonejson_error *error);
/** Opens one push-fed arbitrary JSON value at the current writer position.
 *
 * The writer owns the surrounding root/object/array framing and emits any
 * required comma or object-key transition before accepting chunks for the
 * nested value. While the stream is open, unrelated writer events on the same
 * writer fail. This API is true streaming in sink mode and rejects writer
 * generator mode instead of buffering a full pushed value. Value-visitor
 * limits come from the writer runtime.
 */
lonejson_status
lonejson_writer_value_stream_open(lonejson_writer_value_stream *stream,
                                  lonejson_writer *writer,
                                  lonejson_error *error);
/** Feeds one byte chunk into an open push-fed writer value stream.
 *
 * Chunks may split anywhere inside the nested JSON value. lonejson validates
 * incrementally and forwards compact JSON bytes to the writer sink as parser
 * events are accepted. Once the nested value is complete, later chunks may
 * contain only JSON whitespace until close.
 */
lonejson_status
lonejson_writer_value_stream_push(lonejson_writer_value_stream *stream,
                                  const void *data, size_t len,
                                  lonejson_error *error);
/** Finalizes an open push-fed writer value stream at source EOF.
 *
 * The call fails for empty input, incomplete values, malformed values, limit
 * violations, trailing non-whitespace bytes, or sink failures. Any failure
 * poisons the owning writer so `lonejson_writer_finish()` cannot report
 * success for a partial document.
 */
lonejson_status
lonejson_writer_value_stream_close(lonejson_writer_value_stream *stream,
                                   lonejson_error *error);
/** Releases resources owned by a writer value stream. If the stream is still
 * open, the owning writer is poisoned because the nested JSON value was not
 * finalized.
 */
void lonejson_writer_value_stream_cleanup(lonejson_writer_value_stream *stream);
/** Streams exactly one arbitrary JSON value from a reader callback.
 *
 * The writer owns the surrounding root/object/array framing. The helper
 * validates incrementally, allows trailing JSON whitespace, rejects empty
 * input, multiple top-level values, malformed JSON, limit violations, reader
 * failure, and trailing non-whitespace, and poisons the writer on failure so
 * `lonejson_writer_finish()` cannot succeed for partial output. This is a
 * sink-mode convenience over `lonejson_writer_value_stream_*`; writer
 * generator producers must use a stable `lonejson_json_value` with
 * `lonejson_writer_json_value()` or a resumable writer event.
 */
lonejson_status lonejson_writer_json_value_reader(lonejson_writer *writer,
                                                  lonejson_reader_fn reader,
                                                  void *reader_user,
                                                  lonejson_error *error);
/** Streams exactly one arbitrary JSON value from a memory buffer.
 *
 * The buffer is not retained after the call. Behavior and failure semantics
 * match `lonejson_writer_json_value_reader()`.
 */
lonejson_status lonejson_writer_json_value_buffer(lonejson_writer *writer,
                                                  const void *data, size_t len,
                                                  lonejson_error *error);
/** Streams exactly one arbitrary JSON value from the current `FILE *`
 * position. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
lonejson_status lonejson_writer_json_value_file(lonejson_writer *writer,
                                                FILE *fp,
                                                lonejson_error *error);
/** Streams exactly one arbitrary JSON value from the current file-descriptor
 * position. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
lonejson_status lonejson_writer_json_value_fd(lonejson_writer *writer, int fd,
                                              lonejson_error *error);
/** Opens `path`, streams exactly one arbitrary JSON value, and closes the
 * file before returning. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
lonejson_status lonejson_writer_json_value_path(lonejson_writer *writer,
                                                const char *path,
                                                lonejson_error *error);
/** Streams exactly one arbitrary JSON value from a rewindable spooled byte
 * container. The spooled value is copied as a read cursor, so the caller's
 * cursor is preserved. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
lonejson_status
lonejson_writer_json_value_spooled(lonejson_writer *writer,
                                   const lonejson_spooled *value,
                                   lonejson_error *error);
/** Appends all items from a selected source array into the writer's current
 * array.
 *
 * `selector == NULL` or `""` selects a root array. Non-empty selectors follow
 * `lonejson_array_stream` v1 behavior and select one direct root-object key.
 * The writer owns all output commas and array framing; callers must already be
 * inside a writer array. Source item bytes are validated and streamed directly
 * into the writer without materializing the complete source document, selected
 * array, or copied items. This helper is sink-mode only and fails cleanly for
 * writer-generator producers.
 */
lonejson_status lonejson_writer_array_items_reader(lonejson_writer *writer,
                                                   const char *selector,
                                                   lonejson_reader_fn reader,
                                                   void *reader_user,
                                                   lonejson_error *error);
/** Appends all selected source-array items from a memory buffer. Behavior and
 * failure semantics match `lonejson_writer_array_items_reader()`.
 */
lonejson_status lonejson_writer_array_items_buffer(lonejson_writer *writer,
                                                   const char *selector,
                                                   const void *data, size_t len,
                                                   lonejson_error *error);
/** Appends all selected source-array items from an open `FILE *`. Behavior and
 * failure semantics match `lonejson_writer_array_items_reader()`.
 */
lonejson_status lonejson_writer_array_items_filep(lonejson_writer *writer,
                                                  const char *selector,
                                                  FILE *fp,
                                                  lonejson_error *error);
/** Appends all selected source-array items from a file descriptor. Behavior
 * and failure semantics match `lonejson_writer_array_items_reader()`.
 */
lonejson_status lonejson_writer_array_items_fd(lonejson_writer *writer,
                                               const char *selector, int fd,
                                               lonejson_error *error);
/** Opens `path`, appends all selected source-array items, and closes the file
 * before returning. Behavior and failure semantics match
 * `lonejson_writer_array_items_reader()`.
 */
lonejson_status lonejson_writer_array_items_path(lonejson_writer *writer,
                                                 const char *selector,
                                                 const char *path,
                                                 lonejson_error *error);
/** Appends all selected source-array items from a rewindable spooled byte
 * container. The spooled value is copied as a read cursor, so the caller's
 * cursor is preserved. Behavior and failure semantics match
 * `lonejson_writer_array_items_reader()`.
 */
lonejson_status lonejson_writer_array_items_spooled(
    lonejson_writer *writer, const char *selector,
    const lonejson_spooled *value, lonejson_error *error);
/** Emits one mapped struct value through lonejson's normal serializer. */
lonejson_status lonejson_writer_mapped(lonejson_writer *writer,
                                       const lonejson_map *map, const void *src,
                                       lonejson_error *error);
/** Finishes a writer and validates that exactly one complete JSON value was
 * emitted and all containers were closed.
 */
lonejson_status lonejson_writer_finish(lonejson_writer *writer,
                                       lonejson_error *error);
/** Initializes a pull-style generator that drains writer events.
 *
 * `producer` is called by `lonejson_generator_read()` and receives a writer
 * handle. A writer event that returns `LONEJSON_STATUS_OK` has been accepted,
 * even if the generator still needs later reads to drain pending bytes. If an
 * event returns `LONEJSON_STATUS_TRUNCATED`, return that status and retry that
 * same event on the next producer call. The writer generator does not
 * materialize the full document.
 */
lonejson_status
lonejson_writer_generator_init(lonejson *runtime, lonejson_generator *generator,
                               lonejson_writer_producer_fn producer,
                               void *producer_user);
/** Writes a top-level JSON string value from a reader to a sink. */
lonejson_status
lonejson_write_json_string_sink(lonejson *runtime, lonejson_reader_fn reader,
                                void *reader_user, lonejson_sink_fn sink,
                                void *sink_user, lonejson_error *error);
/** Writes a top-level JSON string value from a caller buffer to a sink. */
lonejson_status
lonejson_write_json_string_buffer_sink(lonejson *runtime, const void *data,
                                       size_t len, lonejson_sink_fn sink,
                                       void *sink_user, lonejson_error *error);
/** Writes a top-level JSON string value from a spooled byte container to a
 * sink.
 */
lonejson_status lonejson_write_json_string_spooled_sink(
    lonejson *runtime, const lonejson_spooled *value, lonejson_sink_fn sink,
    void *sink_user, lonejson_error *error);
/** Serializes a mapped struct into a caller-provided output buffer. */
lonejson_status lonejson_serialize_buffer(lonejson *runtime,
                                          const lonejson_map *map,
                                          const void *src, char *buffer,
                                          size_t capacity, size_t *needed,
                                          lonejson_error *error);
/** Serializes a mapped struct into a newly allocated JSON string using the
 * default allocator. Release the returned buffer with `free()` /
 * `LONEJSON_FREE()`. This convenience API rejects custom allocators; use
 * `lonejson_serialize_owned()` when you need allocator-aware ownership.
 */
char *lonejson_serialize_alloc(lonejson *runtime, const lonejson_map *map,
                               const void *src, size_t *out_len,
                               lonejson_error *error);
/** Serializes a mapped struct into an owned output buffer. Initialize `out`
 * with `lonejson_owned_buffer_init()` or `lonejson_default_owned_buffer()`,
 * then release it with `lonejson_owned_buffer_free()`.
 */
lonejson_status lonejson_serialize_owned(lonejson *runtime,
                                         const lonejson_map *map,
                                         const void *src,
                                         lonejson_owned_buffer *out,
                                         lonejson_error *error);
/** Releases an owned output buffer produced by `*_owned()` serializers. */
void lonejson_owned_buffer_free(lonejson_owned_buffer *buffer);
/** Appends bytes to an initialized `lonejson_owned_buffer`.
 *
 * This is a generic sink callback for caller-owned accumulation. It grows the
 * buffer with the allocator stored in the handle and keeps `data` NUL
 * terminated for C-string convenience.
 */
lonejson_status lonejson_owned_buffer_sink(void *user, const void *data,
                                           size_t len, lonejson_error *error);
/** Serializes a mapped struct to an open `FILE *`. */
lonejson_status lonejson_serialize_filep(lonejson *runtime,
                                         const lonejson_map *map,
                                         const void *src, FILE *fp,
                                         lonejson_error *error);
/** Serializes a mapped struct to a filesystem path. */
lonejson_status lonejson_serialize_path(lonejson *runtime,
                                        const lonejson_map *map,
                                        const void *src, const char *path,
                                        lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records to a generic output
 * sink callback using runtime write policy. `stride` defaults to
 * `map->struct_size` when set to zero. */
lonejson_status lonejson_serialize_jsonl_sink(
    lonejson *runtime, const lonejson_map *map, const void *items, size_t count,
    size_t stride, lonejson_sink_fn sink, void *user, lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records into a
 * caller-provided output buffer using runtime write policy. `stride` defaults
 * to `map->struct_size` when set to zero. */
lonejson_status lonejson_serialize_jsonl_buffer(lonejson *runtime,
                                                const lonejson_map *map,
                                                const void *items, size_t count,
                                                size_t stride, char *buffer,
                                                size_t capacity, size_t *needed,
                                                lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records into a newly
 * allocated string using the default allocator. Release the returned buffer
 * with `free()` / `LONEJSON_FREE()`. This convenience API rejects custom
 * allocators; use `lonejson_serialize_jsonl_owned()` when you need
 * allocator-aware ownership.
 */
char *lonejson_serialize_jsonl_alloc(lonejson *runtime, const lonejson_map *map,
                                     const void *items, size_t count,
                                     size_t stride, size_t *out_len,
                                     lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records into an owned output
 * buffer. Release it with `lonejson_owned_buffer_free()`. Uses runtime write
 * policy. `stride` defaults to `map->struct_size` when set to zero.
 */
lonejson_status lonejson_serialize_jsonl_owned(
    lonejson *runtime, const lonejson_map *map, const void *items, size_t count,
    size_t stride, lonejson_owned_buffer *out, lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records to an open `FILE *`
 * using runtime write policy. `stride` defaults to `map->struct_size` when set
 * to zero. */
lonejson_status lonejson_serialize_jsonl_filep(lonejson *runtime,
                                               const lonejson_map *map,
                                               const void *items, size_t count,
                                               size_t stride, FILE *fp,
                                               lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records to a filesystem path
 * using runtime write policy. `stride` defaults to `map->struct_size` when set
 * to zero. */
lonejson_status lonejson_serialize_jsonl_path(lonejson *runtime,
                                              const lonejson_map *map,
                                              const void *items, size_t count,
                                              size_t stride, const char *path,
                                              lonejson_error *error);

/** Frees dynamic storage owned by a mapped value.
 *
 * Cleanup remains available as a free function because it is runtime-
 * independent: it only releases storage already owned by `value` according to
 * `map` and does not depend on any instance policy. `lonejson` also exposes a
 * method twin for API symmetry on runtime-owned entrypoints.
 *
 * Lifecycle operations return a status and optionally fill `error`. Schema
 * analysis and record traversal use a small inline workspace, growing on the
 * heap for deep maps. A traversal-workspace allocation failure leaves the
 * record unchanged; retry cleanup after restoring allocator availability.
 * Inline object cycles and cyclic record storage are invalid arguments.
 * The free function uses the default allocator for traversal workspace; the
 * receiver method uses its runtime allocator. Owned fields retain their
 * original allocation ownership in both forms.
 *
 * @param map Mapping describing the initialized record.
 * @param value Initialized mapped storage to release.
 * @param error Optional diagnostic output; may be `NULL`.
 * @return `LONEJSON_STATUS_OK`, `LONEJSON_STATUS_ALLOCATION_FAILED` for
 * workspace allocation failure, or `LONEJSON_STATUS_INVALID_ARGUMENT` for
 * invalid mapping arguments or cyclic record storage.
 */
lonejson_status lonejson_cleanup(const lonejson_map *map, void *value,
                                 lonejson_error *error);
/** Initializes previously uninitialized mapped storage according to its field
 * descriptors without inspecting its prior contents.
 *
 * Call this before the first parse into storage that contains stateful handle
 * fields such as `lonejson_spooled`, `lonejson_json_value`, mapped array-stream
 * helpers, or other runtime-managed subobjects. It overwrites every mapped
 * field and does not release earlier storage; use `lonejson_reset()` to clear
 * and reuse an already initialized value.
 *
 * This is also the right entrypoint when you plan to parse with
 * `clear_destination = 0`, or when you are configuring caller-owned
 * fixed-capacity array backing storage before parse.
 * Check the returned status before parsing or using any mapped handles.
 * On traversal-workspace allocation failure, the destination is untouched.
 * Retry initialization before using previously uninitialized storage.
 *
 * @param runtime Runtime supplying allocation and field policy.
 * @param map Mapping describing the record.
 * @param value Previously uninitialized mapped storage.
 * @param error Optional diagnostic output; may be `NULL`.
 * @return `LONEJSON_STATUS_OK`, `LONEJSON_STATUS_ALLOCATION_FAILED` for
 * workspace allocation failure, or `LONEJSON_STATUS_INVALID_ARGUMENT` for
 * invalid mapping arguments or inline object cycles.
 */
lonejson_status lonejson_init(lonejson *runtime, const lonejson_map *map,
                              void *value, lonejson_error *error);
/** Clears an initialized mapped value while preserving caller-owned
 * fixed-capacity array backing storage.
 *
 * Traversal-workspace allocation failure leaves the record unchanged. Restore
 * allocator availability and retry reset before reusing the record.
 *
 * @param runtime Runtime supplying allocation and field policy.
 * @param map Mapping describing the initialized record.
 * @param value Initialized mapped storage to reset.
 * @param error Optional diagnostic output; may be `NULL`.
 * @return `LONEJSON_STATUS_OK`, `LONEJSON_STATUS_ALLOCATION_FAILED` for
 * workspace allocation failure, or `LONEJSON_STATUS_INVALID_ARGUMENT` for
 * invalid mapping arguments or cyclic record storage.
 */
lonejson_status lonejson_reset(lonejson *runtime, const lonejson_map *map,
                               void *value, lonejson_error *error);
/** Returns non-zero when a field uses a presence flag. Dynamic language
 * bindings can use this to mirror lonejson's nullable scalar semantics without
 * including private implementation headers.
 */
int lonejson_field_has_presence(const lonejson_field *field);
/** Sets or clears a field presence flag when the field has one. No-op for
 * fields without presence storage.
 */
void lonejson_field_set_presence(void *record, const lonejson_field *field,
                                 int present);
/** Assigns JSON null semantics to one field in an initialized mapped record. */
lonejson_status lonejson_record_assign_null(lonejson *runtime,
                                            const lonejson_map *map,
                                            void *record,
                                            const lonejson_field *field,
                                            lonejson_error *error);
/** Assigns one decoded string value to a string field in an initialized mapped
 * record, applying fixed-capacity overflow and allocation limits.
 */
lonejson_status lonejson_record_assign_string(lonejson *runtime,
                                              const lonejson_map *map,
                                              void *record,
                                              const lonejson_field *field,
                                              const char *data, size_t len,
                                              lonejson_error *error);
/** Appends one decoded string item to a mapped string array field. */
lonejson_status lonejson_record_array_append_string(
    lonejson *runtime, const lonejson_map *map, void *record,
    const lonejson_field *field, lonejson_string_array *array, const char *data,
    size_t len, lonejson_error *error);
/** Appends one signed integer item to a mapped i64 array field. */
lonejson_status
lonejson_record_array_append_i64(lonejson *runtime, const lonejson_map *map,
                                 void *record, const lonejson_field *field,
                                 lonejson_i64_array *array,
                                 lonejson_int64 value, lonejson_error *error);
/** Appends one unsigned integer item to a mapped u64 array field. */
lonejson_status
lonejson_record_array_append_u64(lonejson *runtime, const lonejson_map *map,
                                 void *record, const lonejson_field *field,
                                 lonejson_u64_array *array,
                                 lonejson_uint64 value, lonejson_error *error);
/** Appends one number item to a mapped f64 array field. */
lonejson_status
lonejson_record_array_append_f64(lonejson *runtime, const lonejson_map *map,
                                 void *record, const lonejson_field *field,
                                 lonejson_f64_array *array, double value,
                                 lonejson_error *error);
/** Appends one boolean item to a mapped bool array field. */
lonejson_status
lonejson_record_array_append_bool(lonejson *runtime, const lonejson_map *map,
                                  void *record, const lonejson_field *field,
                                  lonejson_bool_array *array, int value,
                                  lonejson_error *error);
/** Appends one uninitialized slot to a mapped object array field and returns
 * the slot pointer. The caller should initialize/populate it with the field's
 * submap before serialization.
 */
void *lonejson_record_object_array_append_slot(lonejson *runtime,
                                               const lonejson_map *map,
                                               void *record,
                                               const lonejson_field *field,
                                               lonejson_object_array *array,
                                               lonejson_error *error);

/** Computes the encoded byte length for `len` raw bytes and base64 `variant`.
 *
 * Use `LONEJSON_BASE64_STANDARD` for padded RFC 4648 base64,
 * `LONEJSON_BASE64_STANDARD_RAW` for unpadded standard base64,
 * `LONEJSON_BASE64_URL` for padded base64url, and
 * `LONEJSON_BASE64_URL_RAW` for unpadded JWT/JWS-style base64url segments.
 */
lonejson_status lonejson_base64_encoded_len(size_t len,
                                            lonejson_base64_variant variant,
                                            size_t *out_len,
                                            lonejson_error *error);
/** Encodes raw bytes into caller-provided base64 storage.
 *
 * `needed` is set to the required encoded size on success or truncation. The
 * output is not NUL-terminated. If `capacity` is too small, no bytes are
 * written and `LONEJSON_STATUS_TRUNCATED` is returned.
 */
lonejson_status lonejson_base64_encode(const void *data, size_t len,
                                       lonejson_base64_variant variant,
                                       char *out, size_t capacity,
                                       size_t *needed, lonejson_error *error);
/** Encodes raw bytes as base64 and streams encoded chunks to `sink`.
 *
 * This uses bounded internal chunks and does not require the caller to
 * allocate the complete encoded output.
 */
lonejson_status lonejson_base64_encode_sink(const void *data, size_t len,
                                            lonejson_base64_variant variant,
                                            lonejson_sink_fn sink, void *user,
                                            lonejson_error *error);
/** Computes the decoded byte length for one base64 value.
 *
 * Raw variants reject `=` padding. Padded variants accept canonical padding
 * and reject non-padding data after padding.
 */
lonejson_status lonejson_base64_decoded_len(const char *data, size_t len,
                                            lonejson_base64_variant variant,
                                            size_t *out_len,
                                            lonejson_error *error);
/** Decodes one base64 value into caller-provided storage.
 *
 * `needed` is set to the required decoded size on success or truncation. If
 * `capacity` is too small, no bytes are written and
 * `LONEJSON_STATUS_TRUNCATED` is returned.
 */
lonejson_status lonejson_base64_decode(const char *data, size_t len,
                                       lonejson_base64_variant variant,
                                       unsigned char *out, size_t capacity,
                                       size_t *needed, lonejson_error *error);
/** Decodes one base64 value and streams decoded chunks to `sink`.
 *
 * The decoder validates the selected alphabet and padding policy before
 * emitting bytes. It does not materialize the complete decoded value.
 */
lonejson_status lonejson_base64_decode_sink(const char *data, size_t len,
                                            lonejson_base64_variant variant,
                                            lonejson_sink_fn sink, void *user,
                                            lonejson_error *error);

#ifdef LONEJSON_WITH_JWT
/** Installs or clears the runtime auth provider. */
lonejson_status
lonejson_set_auth_provider(lonejson *runtime,
                           const lonejson_auth_provider *provider,
                           lonejson_error *error);
#ifdef LONEJSON_WITH_OPENSSL
#ifndef LONEJSON_OPENSSL_INLINE
#if defined(_MSC_VER)
#define LONEJSON_OPENSSL_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define LONEJSON_OPENSSL_INLINE static __inline__ __attribute__((unused))
#else
#define LONEJSON_OPENSSL_INLINE static
#endif
#endif

#ifndef LONEJSON_OPENSSL_JWT_MAX_RSA_COMPONENT_BYTES
#define LONEJSON_OPENSSL_JWT_MAX_RSA_COMPONENT_BYTES (8u * 1024u)
#endif

LONEJSON_OPENSSL_INLINE void
lonejson__openssl_clear_error(lonejson_error *error) {
  if (error != NULL) {
    memset(error, 0, sizeof(*error));
  }
}

LONEJSON_OPENSSL_INLINE void
lonejson__openssl_copy_error_message(char *dst, size_t dst_size,
                                     const char *message) {
  size_t i;

  if (dst == NULL || dst_size == 0u) {
    return;
  }
  if (message == NULL) {
    dst[0] = '\0';
    return;
  }
  for (i = 0u; i + 1u < dst_size && message[i] != '\0'; ++i) {
    dst[i] = message[i];
  }
  dst[i] = '\0';
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__openssl_set_error(
    lonejson_error *error, lonejson_status status, size_t line, size_t column,
    size_t offset, const char *message) {
  if (error != NULL) {
    memset(error, 0, sizeof(*error));
    error->code = status;
    error->line = line;
    error->column = column;
    error->offset = offset;
    lonejson__openssl_copy_error_message(error->message, sizeof(error->message),
                                         message);
  }
  return status;
}

LONEJSON_OPENSSL_INLINE int lonejson__openssl_streq(const char *a,
                                                    const char *b) {
  return a != NULL && b != NULL && strcmp(a, b) == 0;
}

LONEJSON_OPENSSL_INLINE unsigned char *
lonejson__jwt_decode_base64url_alloc(const char *data, size_t len,
                                     size_t *out_len, const char *what,
                                     lonejson_error *error) {
  unsigned char *out;
  size_t decoded_len;
  size_t needed;
  lonejson_status status;

  (void)what;
  status = lonejson_base64_decoded_len(data, len, LONEJSON_BASE64_URL_RAW,
                                       &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return NULL;
  }
  out = (unsigned char *)malloc(decoded_len == 0u ? 1u : decoded_len);
  if (out == NULL) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                      0u, 0u, 0u,
                                      "failed to allocate decoded JWT segment");
    return NULL;
  }
  status = lonejson_base64_decode(data, len, LONEJSON_BASE64_URL_RAW, out,
                                  decoded_len, &needed, error);
  if (status != LONEJSON_STATUS_OK) {
    free(out);
    return NULL;
  }
  *out_len = decoded_len;
  return out;
}

LONEJSON_OPENSSL_INLINE unsigned char *
lonejson__jwt_decode_base64_alloc(const char *data, size_t len, size_t *out_len,
                                  const char *what, lonejson_error *error) {
  unsigned char *out;
  size_t decoded_len;
  size_t needed;
  lonejson_status status;

  (void)what;
  status = lonejson_base64_decoded_len(data, len, LONEJSON_BASE64_STANDARD,
                                       &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return NULL;
  }
  out = (unsigned char *)malloc(decoded_len == 0u ? 1u : decoded_len);
  if (out == NULL) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                      0u, 0u, 0u,
                                      "failed to allocate decoded JWT segment");
    return NULL;
  }
  status = lonejson_base64_decode(data, len, LONEJSON_BASE64_STANDARD, out,
                                  decoded_len, &needed, error);
  if (status != LONEJSON_STATUS_OK) {
    free(out);
    return NULL;
  }
  *out_len = decoded_len;
  return out;
}

LONEJSON_OPENSSL_INLINE int lonejson__jwt_digest_matches_b64url(
    const char *encoded, const unsigned char *digest, size_t digest_len) {
  unsigned char *decoded;
  size_t decoded_len = 0u;
  lonejson_error error;
  int ok;

  if (encoded == NULL) {
    return 1;
  }
  lonejson_error_init(&error);
  decoded = lonejson__jwt_decode_base64url_alloc(
      encoded, strlen(encoded), &decoded_len, "thumbprint", &error);
  if (decoded == NULL) {
    return 0;
  }
  ok = decoded_len == digest_len && memcmp(decoded, digest, digest_len) == 0;
  free(decoded);
  return ok;
}

LONEJSON_OPENSSL_INLINE X509 *
lonejson__jwt_x509_from_x5c_item(const char *item, lonejson_error *error) {
  unsigned char *der;
  const unsigned char *cursor;
  size_t der_len = 0u;
  X509 *cert;

  if (item == NULL || item[0] == '\0') {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                      0u, 0u,
                                      "JWK x5c certificate must not be empty");
    return NULL;
  }
  der = lonejson__jwt_decode_base64_alloc(item, strlen(item), &der_len,
                                          "x5c certificate", error);
  if (der == NULL) {
    return NULL;
  }
  if (der_len > (size_t)LONG_MAX) {
    free(der);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                      0u, "JWK x5c certificate is too large");
    return NULL;
  }
  cursor = der;
  cert = d2i_X509(NULL, &cursor, (long)der_len);
  free(der);
  if (cert == NULL || cursor == NULL) {
    X509_free(cert);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                      0u, 0u, "JWK x5c certificate is invalid");
    return NULL;
  }
  return cert;
}

LONEJSON_OPENSSL_INLINE EVP_PKEY *
lonejson__jwt_rsa_public_key_from_jwk(const lonejson_jwk *jwk,
                                      lonejson_error *error) {
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;
  OSSL_PARAM_BLD *builder = NULL;
  OSSL_PARAM *params = NULL;
  BIGNUM *n_bn = NULL;
  BIGNUM *e_bn = NULL;
  unsigned char *n = NULL;
  unsigned char *e = NULL;
  size_t n_len = 0u;
  size_t e_len = 0u;
  int ok = 0;

  if (jwk->n == NULL || jwk->e == NULL || jwk->n[0] == '\0' ||
      jwk->e[0] == '\0') {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                      0u, 0u,
                                      "RSA JWK n and e members are required");
    return NULL;
  }
  n = lonejson__jwt_decode_base64url_alloc(jwk->n, strlen(jwk->n), &n_len,
                                           "JWK modulus", error);
  e = lonejson__jwt_decode_base64url_alloc(jwk->e, strlen(jwk->e), &e_len,
                                           "JWK exponent", error);
  if (n == NULL || e == NULL || n_len == 0u || e_len == 0u) {
    if (n != NULL && n_len == 0u) {
      (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                        0u, 0u,
                                        "RSA JWK modulus must not be empty");
    } else if (e != NULL && e_len == 0u) {
      (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                        0u, 0u,
                                        "RSA JWK exponent must not be empty");
    }
    goto done;
  }
  if (n_len > LONEJSON_OPENSSL_JWT_MAX_RSA_COMPONENT_BYTES ||
      e_len > LONEJSON_OPENSSL_JWT_MAX_RSA_COMPONENT_BYTES) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
        "RSA JWK key material exceeds configured limit");
    goto done;
  }
  if (n_len > (size_t)INT_MAX || e_len > (size_t)INT_MAX) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
        "RSA JWK key material exceeds supported size");
    goto done;
  }
  n_bn = BN_bin2bn(n, (int)n_len, NULL);
  e_bn = BN_bin2bn(e, (int)e_len, NULL);
  if (n_bn == NULL || e_bn == NULL) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate RSA JWK key material");
    goto done;
  }
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
  if (builder == NULL || ctx == NULL) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate RSA public key context");
    goto done;
  }
  if (!OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_N, n_bn) ||
      !OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_E, e_bn)) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate RSA public key params");
    goto done;
  }
  params = OSSL_PARAM_BLD_to_param(builder);
  if (params == NULL) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate RSA public key params");
    goto done;
  }
  if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
      EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0 ||
      pkey == NULL) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "RSA JWK public key is invalid");
    goto done;
  }
  ok = 1;

done:
  free(n);
  free(e);
  BN_free(n_bn);
  BN_free(e_bn);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  if (!ok) {
    EVP_PKEY_free(pkey);
    pkey = NULL;
  }
  return pkey;
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__jwt_validate_rs256_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk, int pss,
    lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  EVP_PKEY_CTX *pkey_ctx = NULL;
  unsigned char *signature = NULL;
  size_t signature_len = 0u;
  int verify_result;

  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  if (signature_len == 0u) {
    free(signature);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u,
                                       "JWT signature must not be empty");
  }
  pkey = lonejson__jwt_rsa_public_key_from_jwk(jwk, error);
  if (pkey == NULL) {
    free(signature);
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    free(signature);
    EVP_PKEY_free(pkey);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                       0u, 0u, 0u,
                                       "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, &pkey_ctx, EVP_sha256(), NULL, pkey) <= 0 ||
      EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, pss ? RSA_PKCS1_PSS_PADDING
                                                 : RSA_PKCS1_PADDING) <= 0 ||
      (pss && EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, 32) <= 0)) {
    free(signature);
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u,
                                       "failed to initialize JWT verifier");
  }
  verify_result = EVP_DigestVerify(
      md, signature, signature_len,
      (const unsigned char *)jwt->signing_input.data, jwt->signing_input.len);
  free(signature);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                     0u, 0u, "JWT signature validation failed");
}

LONEJSON_OPENSSL_INLINE EVP_PKEY *
lonejson__jwt_ec_public_key_from_jwk(const lonejson_jwk *jwk,
                                     lonejson_error *error) {
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;
  OSSL_PARAM_BLD *builder = NULL;
  OSSL_PARAM *params = NULL;
  unsigned char *x = NULL;
  unsigned char *y = NULL;
  unsigned char pub[65];
  size_t x_len = 0u;
  size_t y_len = 0u;
  int ok = 0;

  if (!lonejson__openssl_streq(jwk->crv, "P-256")) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "ES256 requires a P-256 JWK");
    return NULL;
  }
  if (jwk->x == NULL || jwk->x[0] == '\0' || jwk->y == NULL ||
      jwk->y[0] == '\0') {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u,
                                      "ES256 JWK coordinates are required");
    return NULL;
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  y = lonejson__jwt_decode_base64url_alloc(jwk->y, strlen(jwk->y), &y_len,
                                           "JWK y coordinate", error);
  if (x == NULL || y == NULL) {
    goto done;
  }
  if (x_len != 32u || y_len != 32u) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u,
                                      "ES256 JWK coordinates must be 32 bytes");
    goto done;
  }
  pub[0] = 0x04u;
  memcpy(pub + 1u, x, 32u);
  memcpy(pub + 33u, y, 32u);
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
  if (builder == NULL || ctx == NULL) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate EC public key context");
    goto done;
  }
  if (!OSSL_PARAM_BLD_push_utf8_string(builder, OSSL_PKEY_PARAM_GROUP_NAME,
                                       "prime256v1", 0u) ||
      !OSSL_PARAM_BLD_push_octet_string(builder, OSSL_PKEY_PARAM_PUB_KEY, pub,
                                        sizeof(pub))) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate EC public key params");
    goto done;
  }
  params = OSSL_PARAM_BLD_to_param(builder);
  if (params == NULL) {
    (void)lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to allocate EC public key params");
    goto done;
  }
  if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
      EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0 ||
      pkey == NULL) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "EC JWK public key is invalid");
    goto done;
  }
  ok = 1;

done:
  free(x);
  free(y);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  if (!ok) {
    EVP_PKEY_free(pkey);
    pkey = NULL;
  }
  return pkey;
}

LONEJSON_OPENSSL_INLINE unsigned char *
lonejson__jwt_es256_der_signature_alloc(const unsigned char *signature,
                                        size_t signature_len, size_t *out_len,
                                        lonejson_error *error) {
  ECDSA_SIG *sig = NULL;
  BIGNUM *r = NULL;
  BIGNUM *s = NULL;
  unsigned char *der = NULL;
  unsigned char *cursor;
  int der_len;

  if (signature_len != 64u) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u,
                                      "ES256 signature must be 64 bytes");
    return NULL;
  }
  sig = ECDSA_SIG_new();
  r = BN_bin2bn(signature, 32, NULL);
  s = BN_bin2bn(signature + 32u, 32, NULL);
  if (sig == NULL || r == NULL || s == NULL) {
    ECDSA_SIG_free(sig);
    BN_free(r);
    BN_free(s);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                      0u, 0u, 0u,
                                      "failed to allocate ES256 signature");
    return NULL;
  }
  if (ECDSA_SIG_set0(sig, r, s) != 1) {
    ECDSA_SIG_free(sig);
    BN_free(r);
    BN_free(s);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                      0u, 0u, 0u,
                                      "failed to prepare ES256 signature");
    return NULL;
  }
  r = NULL;
  s = NULL;
  der_len = i2d_ECDSA_SIG(sig, NULL);
  if (der_len <= 0) {
    ECDSA_SIG_free(sig);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "ES256 signature is invalid");
    return NULL;
  }
  der = (unsigned char *)malloc((size_t)der_len);
  if (der == NULL) {
    ECDSA_SIG_free(sig);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                      0u, 0u, 0u,
                                      "failed to allocate ES256 signature");
    return NULL;
  }
  cursor = der;
  der_len = i2d_ECDSA_SIG(sig, &cursor);
  ECDSA_SIG_free(sig);
  if (der_len <= 0) {
    free(der);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "ES256 signature is invalid");
    return NULL;
  }
  *out_len = (size_t)der_len;
  return der;
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__jwt_validate_es256_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk,
    lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  unsigned char *signature = NULL;
  unsigned char *der_signature = NULL;
  size_t signature_len = 0u;
  size_t der_signature_len = 0u;
  int verify_result;

  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  der_signature = lonejson__jwt_es256_der_signature_alloc(
      signature, signature_len, &der_signature_len, error);
  free(signature);
  if (der_signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  pkey = lonejson__jwt_ec_public_key_from_jwk(jwk, error);
  if (pkey == NULL) {
    free(der_signature);
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    free(der_signature);
    EVP_PKEY_free(pkey);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                       0u, 0u, 0u,
                                       "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, NULL, EVP_sha256(), NULL, pkey) <= 0) {
    free(der_signature);
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u,
                                       "failed to initialize JWT verifier");
  }
  verify_result = EVP_DigestVerify(
      md, der_signature, der_signature_len,
      (const unsigned char *)jwt->signing_input.data, jwt->signing_input.len);
  free(der_signature);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                     0u, 0u, "JWT signature validation failed");
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__jwt_validate_eddsa_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk,
    lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  unsigned char *x = NULL;
  unsigned char *signature = NULL;
  size_t x_len = 0u;
  size_t signature_len = 0u;
  int verify_result;

  if (!lonejson__openssl_streq(jwk->crv, "Ed25519")) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u, "EdDSA requires an Ed25519 JWK");
  }
  if (jwk->x == NULL || jwk->x[0] == '\0') {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u,
                                       "Ed25519 JWK x coordinate is required");
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (x == NULL || signature == NULL) {
    free(x);
    free(signature);
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  if (x_len != 32u || signature_len != 64u) {
    free(x);
    free(signature);
    return lonejson__openssl_set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "Ed25519 JWK or signature length is invalid");
  }
  pkey = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, x, x_len);
  free(x);
  if (pkey == NULL) {
    free(signature);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u,
                                       "Ed25519 JWK public key is invalid");
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    EVP_PKEY_free(pkey);
    free(signature);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                       0u, 0u, 0u,
                                       "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, NULL, NULL, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    free(signature);
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                       0u, 0u,
                                       "failed to initialize JWT verifier");
  }
  verify_result = EVP_DigestVerify(
      md, signature, signature_len,
      (const unsigned char *)jwt->signing_input.data, jwt->signing_input.len);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  free(signature);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                     0u, 0u, "JWT signature validation failed");
}

LONEJSON_OPENSSL_INLINE EVP_PKEY *
lonejson__jwt_eddsa_public_key_from_jwk(const lonejson_jwk *jwk,
                                        lonejson_error *error) {
  EVP_PKEY *pkey;
  unsigned char *x = NULL;
  size_t x_len = 0u;

  if (!lonejson__openssl_streq(jwk->crv, "Ed25519")) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "EdDSA requires an Ed25519 JWK");
    return NULL;
  }
  if (jwk->x == NULL || jwk->x[0] == '\0') {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u,
                                      "Ed25519 JWK x coordinate is required");
    return NULL;
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  if (x == NULL) {
    return NULL;
  }
  if (x_len != 32u) {
    free(x);
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u, "Ed25519 JWK length is invalid");
    return NULL;
  }
  pkey = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, x, x_len);
  free(x);
  if (pkey == NULL) {
    (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                      0u, 0u,
                                      "Ed25519 JWK public key is invalid");
  }
  return pkey;
}

LONEJSON_OPENSSL_INLINE EVP_PKEY *
lonejson__jwt_public_key_from_jwk_for_alg(const lonejson_jwt_header *header,
                                          const lonejson_jwk *jwk,
                                          lonejson_error *error) {
  if (strcmp(header->alg, "RS256") == 0 || strcmp(header->alg, "PS256") == 0) {
    return lonejson__jwt_rsa_public_key_from_jwk(jwk, error);
  }
  if (strcmp(header->alg, "ES256") == 0) {
    return lonejson__jwt_ec_public_key_from_jwk(jwk, error);
  }
  if (strcmp(header->alg, "EdDSA") == 0) {
    return lonejson__jwt_eddsa_public_key_from_jwk(jwk, error);
  }
  (void)lonejson__openssl_set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u,
                                    0u, 0u,
                                    "JWT signature algorithm is not supported");
  return NULL;
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__jwt_validate_jwk_x5c(
    void *x509_store, const lonejson_jwt_header *header,
    const lonejson_jwk *jwk, lonejson_error *error) {
  STACK_OF(X509) *chain = NULL;
  X509 *leaf = NULL;
  X509 *cert = NULL;
  X509_STORE *store = NULL;
  X509_STORE_CTX *store_ctx = NULL;
  EVP_PKEY *cert_key = NULL;
  EVP_PKEY *jwk_key = NULL;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0u;
  size_t i;
  int owns_store = 0;
  int ok;
  lonejson_status status = LONEJSON_STATUS_OK;

  if (jwk->x5c.count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  chain = sk_X509_new_null();
  if (chain == NULL) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                       0u, 0u, 0u,
                                       "failed to allocate JWK x5c chain");
  }
  for (i = 0u; i < jwk->x5c.count; ++i) {
    cert = lonejson__jwt_x509_from_x5c_item(jwk->x5c.items[i], error);
    if (cert == NULL) {
      status = error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
      goto done;
    }
    if (i == 0u) {
      leaf = cert;
    } else if (!sk_X509_push(chain, cert)) {
      X509_free(cert);
      status = lonejson__openssl_set_error(
          error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
          "failed to append JWK x5c certificate");
      goto done;
    }
    cert = NULL;
  }
  if (leaf == NULL) {
    status = lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_JSON,
                                         0u, 0u, 0u, "JWK x5c chain is empty");
    goto done;
  }
  if (X509_digest(leaf, EVP_sha1(), digest, &digest_len) != 1 ||
      digest_len != 20u) {
    status =
        lonejson__openssl_set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                                    0u, 0u, "failed to compute JWK x5c SHA-1");
    goto done;
  }
  if (!lonejson__jwt_digest_matches_b64url(jwk->x5t, digest, digest_len) ||
      !lonejson__jwt_digest_matches_b64url(header->x5t, digest, digest_len)) {
    status = lonejson__openssl_set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "JWT x5t thumbprint does not match x5c leaf");
    goto done;
  }
  if (X509_digest(leaf, EVP_sha256(), digest, &digest_len) != 1 ||
      digest_len != 32u) {
    status = lonejson__openssl_set_error(error, LONEJSON_STATUS_INTERNAL_ERROR,
                                         0u, 0u, 0u,
                                         "failed to compute JWK x5c SHA-256");
    goto done;
  }
  if (!lonejson__jwt_digest_matches_b64url(jwk->x5t_s256, digest, digest_len) ||
      !lonejson__jwt_digest_matches_b64url(header->x5t_s256, digest,
                                           digest_len)) {
    status = lonejson__openssl_set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "JWT x5t#S256 thumbprint does not match x5c leaf");
    goto done;
  }
  cert_key = X509_get_pubkey(leaf);
  jwk_key = lonejson__jwt_public_key_from_jwk_for_alg(header, jwk, error);
  if (cert_key == NULL || jwk_key == NULL) {
    status = error != NULL && error->code != LONEJSON_STATUS_OK
                 ? error->code
                 : lonejson__openssl_set_error(
                       error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                       "JWK x5c leaf public key is invalid");
    goto done;
  }
  if (EVP_PKEY_eq(cert_key, jwk_key) != 1) {
    status = lonejson__openssl_set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "JWK public key does not match x5c leaf");
    goto done;
  }
  if (x509_store != NULL) {
    store = (X509_STORE *)x509_store;
  } else {
    store = X509_STORE_new();
    owns_store = 1;
    if (store == NULL || X509_STORE_set_default_paths(store) != 1) {
      status = lonejson__openssl_set_error(
          error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
          "failed to initialize OpenSSL trust store");
      goto done;
    }
  }
  store_ctx = X509_STORE_CTX_new();
  if (store_ctx == NULL ||
      X509_STORE_CTX_init(store_ctx, store, leaf, chain) != 1) {
    status = lonejson__openssl_set_error(
        error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
        "failed to initialize JWK x5c verifier");
    goto done;
  }
  ok = X509_verify_cert(store_ctx);
  if (ok != 1) {
    status = lonejson__openssl_set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "JWK x5c certificate chain is not trusted");
    goto done;
  }

done:
  EVP_PKEY_free(cert_key);
  EVP_PKEY_free(jwk_key);
  X509_STORE_CTX_free(store_ctx);
  if (owns_store) {
    X509_STORE_free(store);
  }
  X509_free(leaf);
  sk_X509_pop_free(chain, X509_free);
  return status;
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__openssl_auth_verify_jws(
    void *user_data, const lonejson_jws_verify_request *request,
    lonejson_error *error) {
  const lonejson_jwt_compact *jwt;
  const lonejson_jwt_header *header;
  const lonejson_jwk *jwk;
  lonejson_status status;

  if (request == NULL || request->jwt == NULL || request->header == NULL ||
      request->jwk == NULL) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                                       0u, 0u, 0u,
                                       "JWS verification request is required");
  }
  jwt = request->jwt;
  header = request->header;
  jwk = request->jwk;
  status = lonejson__jwt_validate_jwk_x5c(user_data, header, jwk, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strcmp(header->alg, "RS256") == 0 || strcmp(header->alg, "PS256") == 0) {
    if (!lonejson__openssl_streq(jwk->kty, "RSA")) {
      return lonejson__openssl_set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
          "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_rs256_signature(
        jwt, jwk, strcmp(header->alg, "PS256") == 0, error);
  }
  if (strcmp(header->alg, "ES256") == 0) {
    if (!lonejson__openssl_streq(jwk->kty, "EC")) {
      return lonejson__openssl_set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
          "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_es256_signature(jwt, jwk, error);
  }
  if (strcmp(header->alg, "EdDSA") == 0) {
    if (!lonejson__openssl_streq(jwk->kty, "OKP")) {
      return lonejson__openssl_set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
          "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_eddsa_signature(jwt, jwk, error);
  }
  {
    return lonejson__openssl_set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "JWT signature algorithm is not supported");
  }
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson__openssl_auth_random_bytes(
    void *user_data, unsigned char *dst, size_t len, lonejson_error *error) {
  (void)user_data;
  if (dst == NULL && len != 0u) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                                       0u, 0u, 0u,
                                       "random byte output is required");
  }
  if (len > (size_t)INT_MAX) {
    return lonejson__openssl_set_error(
        error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
        "random byte request exceeds OpenSSL limit");
  }
  if (len != 0u && RAND_bytes(dst, (int)len) != 1) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_INTERNAL_ERROR,
                                       0u, 0u, 0u,
                                       "failed to generate random bytes");
  }
  return LONEJSON_STATUS_OK;
}

LONEJSON_OPENSSL_INLINE lonejson_status
lonejson__openssl_auth_sha256(void *user_data, const void *data, size_t len,
                              unsigned char out[32], lonejson_error *error) {
  unsigned int digest_len = 0u;

  (void)user_data;
  if ((data == NULL && len != 0u) || out == NULL) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                                       0u, 0u, 0u,
                                       "SHA-256 input and output are required");
  }
  if (EVP_Digest(data, len, out, &digest_len, EVP_sha256(), NULL) != 1 ||
      digest_len != 32u) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_INTERNAL_ERROR,
                                       0u, 0u, 0u,
                                       "failed to compute SHA-256 digest");
  }
  return LONEJSON_STATUS_OK;
}

LONEJSON_OPENSSL_INLINE lonejson_status lonejson_auth_provider_init_openssl(
    lonejson_auth_provider *provider,
    const lonejson_openssl_auth_provider_config *config,
    lonejson_error *error) {
  lonejson__openssl_clear_error(error);
  if (provider == NULL) {
    return lonejson__openssl_set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                                       0u, 0u, 0u,
                                       "auth provider output is required");
  }
  if (config != NULL && (config->libctx != NULL || config->propq != NULL)) {
    return lonejson__openssl_set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OpenSSL provider config fields are reserved");
  }
  memset(provider, 0, sizeof(*provider));
  provider->user_data = config != NULL ? config->x509_store : NULL;
  provider->verify_jws = lonejson__openssl_auth_verify_jws;
  provider->random_bytes = lonejson__openssl_auth_random_bytes;
  provider->sha256 = lonejson__openssl_auth_sha256;
  return LONEJSON_STATUS_OK;
}
#endif

/** Parses one JWT compact serialization into caller-owned segment slices.
 *
 * This checks compact serialization and base64url segment syntax only. It does
 * not decode JSON, validate signatures, validate claims, or establish trust.
 */
lonejson_status lonejson_jwt_parse_compact(const char *token, size_t len,
                                           lonejson_jwt_compact *out,
                                           lonejson_error *error);
/** Initializes a JWK object for parsing or cleanup. */
void lonejson_jwk_init(lonejson_jwk *jwk);
/** Releases storage owned by a JWK object and resets it to empty. */
void lonejson_jwk_cleanup(lonejson_jwk *jwk);
/** Initializes a JWKS object for parsing or cleanup. */
void lonejson_jwks_init(lonejson_jwks *jwks);
/** Releases storage owned by a JWKS object and resets it to empty. */
void lonejson_jwks_cleanup(lonejson_jwks *jwks);
/** Parses one JWK JSON object into `out`.
 *
 * This validates required key material for supported `kty` values and checks
 * base64url member syntax only. It does not validate signatures or trust.
 */
lonejson_status lonejson_jwk_parse_json(lonejson *runtime, const char *json,
                                        size_t len, lonejson_jwk *out,
                                        lonejson_error *error);
/** Parses one JWKS JSON object with a required `keys` array. */
lonejson_status lonejson_jwks_parse_json(lonejson *runtime, const char *json,
                                         size_t len, lonejson_jwks *out,
                                         lonejson_error *error);
/** Selects the first JWK matching all non-NULL filters.
 *
 * Returns `LONEJSON_STATUS_OK` and sets `*out` to NULL when no key matches.
 */
lonejson_status lonejson_jwks_select(const lonejson_jwks *jwks,
                                     const lonejson_jwk_select_options *options,
                                     const lonejson_jwk **out,
                                     lonejson_error *error);
/** Initializes decoded JWT header storage. */
void lonejson_jwt_header_init(lonejson_jwt_header *header);
/** Releases storage owned by decoded JWT header storage. */
void lonejson_jwt_header_cleanup(lonejson_jwt_header *header);
/** Initializes decoded JWT claims storage. */
void lonejson_jwt_claims_init(lonejson_jwt_claims *claims);
/** Releases storage owned by decoded JWT claims storage. */
void lonejson_jwt_claims_cleanup(lonejson_jwt_claims *claims);
/** Decodes and parses the header and claims payload from one compact JWT.
 *
 * This validates compact serialization, base64url syntax, decoded-size limits,
 * JSON syntax, duplicate registered claims, and registered claim types. It
 * does not validate signatures or trust.
 */
lonejson_status
lonejson_jwt_decode_compact(lonejson *runtime, const char *token, size_t len,
                            const lonejson_jwt_claim_policy *limits,
                            lonejson_jwt_header *header,
                            lonejson_jwt_claims *claims, lonejson_error *error);
/** Validates decoded JWT header and claims against an explicit policy.
 *
 * This is a trust decision for claims only. Signature validation must be
 * composed separately once a verified signature result is available.
 */
lonejson_status lonejson_jwt_validate_claims(
    const lonejson_jwt_header *header, const lonejson_jwt_claims *claims,
    const lonejson_jwt_claim_policy *policy, lonejson_error *error);
/** Validates a compact JWT signature against a selected JWK.
 *
 * This is a trust decision for the JWS signature only. The caller must still
 * validate claims with `lonejson_jwt_validate_claims()`. The decoded header is
 * supplied explicitly so algorithm and key constraints are checked against the
 * same parsed header the caller will use for claim policy. With the OpenSSL
 * auth provider, selected JWK `x5c` chains are validated when present.
 */
lonejson_status
lonejson_jwt_validate_signature(const lonejson_jwt_compact *jwt,
                                const lonejson_jwt_header *header,
                                const lonejson_jwk *jwk, lonejson_error *error);
/** Validates a compact JWT signature through a runtime auth provider. */
lonejson_status lonejson_jwt_validate_signature_with_runtime(
    lonejson *runtime, const lonejson_jwt_compact *jwt,
    const lonejson_jwt_header *header, const lonejson_jwk *jwk,
    lonejson_error *error);
#endif
#ifdef LONEJSON_WITH_OIDC
/** Initializes a materialized HTTP response for provider use or cleanup. */
void lonejson_http_response_init(lonejson_http_response *response);
/** Releases storage owned by a materialized HTTP response. */
void lonejson_http_response_cleanup(lonejson_http_response *response);
/** Initializes an HTTP provider from caller-owned callback config. */
lonejson_status
lonejson_http_provider_init(lonejson_http_provider *provider,
                            const lonejson_http_provider_config *config,
                            lonejson_error *error);
/** Initializes an HTTP provider from direct caller-owned callback arguments. */
lonejson_status lonejson_http_provider_init_simple(
    lonejson_http_provider *provider, void *user_data, const char *user_agent,
    lonejson_status (*request)(void *user_data,
                               const lonejson_http_request *request,
                               lonejson_http_response *response,
                               lonejson_error *error),
    lonejson_error *error);
/** Installs or clears the runtime auth HTTP provider. */
lonejson_status
lonejson_set_http_provider(lonejson *runtime,
                           const lonejson_http_provider *provider,
                           lonejson_error *error);
/** Initializes OIDC discovery metadata storage. */
void lonejson_oidc_discovery_init(lonejson_oidc_discovery *discovery);
/** Releases storage owned by OIDC discovery metadata. */
void lonejson_oidc_discovery_cleanup(lonejson_oidc_discovery *discovery);
/** Builds the OIDC discovery URL for an HTTPS issuer.
 *
 * Path-based issuers follow OpenID Connect Discovery placement:
 * `https://host/path` becomes
 * `https://host/path/.well-known/openid-configuration`.
 */
lonejson_status lonejson_oidc_discovery_url(const char *issuer,
                                            lonejson_owned_buffer *out,
                                            lonejson_error *error);
/** Parses one OIDC discovery JSON object into `out`.
 *
 * This validates required metadata shape only. It does not fetch JWKS, validate
 * signatures, validate tokens, or establish issuer trust.
 */
lonejson_status lonejson_oidc_discovery_parse_json(lonejson *runtime,
                                                   const char *json, size_t len,
                                                   lonejson_oidc_discovery *out,
                                                   lonejson_error *error);
/** Validates parsed discovery metadata against the issuer configured by caller.
 */
lonejson_status lonejson_oidc_discovery_validate_issuer(
    const lonejson_oidc_discovery *discovery, const char *expected_issuer,
    lonejson_error *error);
/** Fetches, parses, and issuer-validates OIDC discovery metadata through the
 * runtime HTTP provider.
 */
lonejson_status lonejson_oidc_fetch_discovery(lonejson *runtime,
                                              const char *issuer,
                                              size_t max_response_bytes,
                                              lonejson_oidc_discovery *out,
                                              lonejson_error *error);
/** Initializes a JWKS cache for later update or cleanup. */
void lonejson_oidc_jwks_cache_init(lonejson_oidc_jwks_cache *cache);
/** Releases all storage owned by a JWKS cache. */
void lonejson_oidc_jwks_cache_cleanup(lonejson_oidc_jwks_cache *cache);
/** Installs caller-provided JWKS JSON into a bounded cache.
 *
 * This parses and validates a JWKS document, records the expected issuer and
 * JWKS URI, and sets `expires_at = policy->now + policy->ttl_seconds`. Network
 * retrieval remains caller-owned.
 */
lonejson_status lonejson_oidc_jwks_cache_update_json(
    lonejson *runtime, lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, const char *json, size_t len,
    lonejson_error *error);
/** Returns non-zero when a cache has keys for the configured issuer/URI and is
 * not expired at `policy->now`.
 */
int lonejson_oidc_jwks_cache_is_fresh(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy);
/** Selects a key from a fresh JWKS cache. */
lonejson_status
lonejson_oidc_jwks_cache_select(const lonejson_oidc_jwks_cache *cache,
                                const lonejson_oidc_jwks_cache_policy *policy,
                                const lonejson_jwk_select_options *options,
                                const lonejson_jwk **out,
                                lonejson_error *error);
/** Refreshes a JWKS cache through the runtime HTTP provider. */
lonejson_status lonejson_oidc_jwks_cache_refresh(
    lonejson *runtime, lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, lonejson_error *error);
/** Builds an `application/x-www-form-urlencoded` client-credentials body. */
lonejson_status lonejson_oauth2_client_credentials_body(
    const lonejson_oauth2_client_credentials *request,
    lonejson_owned_buffer *out, lonejson_error *error);
/** Builds an `application/x-www-form-urlencoded` refresh-token body. */
lonejson_status
lonejson_oauth2_refresh_token_body(const lonejson_oauth2_refresh_token *request,
                                   lonejson_owned_buffer *out,
                                   lonejson_error *error);
/** Builds an `application/x-www-form-urlencoded` introspection body. */
lonejson_status lonejson_oauth2_token_introspection_body(
    const lonejson_oauth2_token_introspection *request,
    lonejson_owned_buffer *out, lonejson_error *error);
/** Builds an `application/x-www-form-urlencoded` revocation body. */
lonejson_status lonejson_oauth2_token_revocation_body(
    const lonejson_oauth2_token_revocation *request, lonejson_owned_buffer *out,
    lonejson_error *error);
/** Builds an `application/x-www-form-urlencoded` authorization-code token body.
 */
lonejson_status lonejson_oidc_authorization_code_token_body(
    const lonejson_oidc_authorization_code_token *request,
    lonejson_owned_buffer *out, lonejson_error *error);
/** Initializes a token response for parsing or cleanup. */
void lonejson_oauth2_token_response_init(
    lonejson_oauth2_token_response *response);
/** Releases all storage owned by a token response. */
void lonejson_oauth2_token_response_cleanup(
    lonejson_oauth2_token_response *response);
/** Parses and validates a bounded successful OAuth2 token endpoint response.
 *
 * `max_response_bytes == 0` applies lonejson's default token-response cap.
 * Provider error responses are rejected with `LONEJSON_STATUS_TYPE_MISMATCH`.
 */
lonejson_status lonejson_oauth2_token_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oauth2_token_response *out, lonejson_error *error);
/** Initializes an introspection response for parsing or cleanup. */
void lonejson_oauth2_introspection_response_init(
    lonejson_oauth2_introspection_response *response);
/** Releases all storage owned by an introspection response. */
void lonejson_oauth2_introspection_response_cleanup(
    lonejson_oauth2_introspection_response *response);
/** Parses and validates a bounded OAuth2 token introspection response. */
lonejson_status lonejson_oauth2_introspection_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oauth2_introspection_response *out, lonejson_error *error);
/** Exchanges OAuth2 client credentials through the runtime HTTP provider and
 * parses the bounded token endpoint response.
 */
lonejson_status lonejson_oauth2_client_credentials_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oauth2_client_credentials *request,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error);
/** Exchanges an OAuth2 refresh token through the runtime HTTP provider. */
lonejson_status lonejson_oauth2_refresh_token_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oauth2_refresh_token *request, size_t max_response_bytes,
    lonejson_oauth2_token_response *out, lonejson_error *error);
/** Initializes token-flow storage for update or cleanup. */
void lonejson_oauth2_token_flow_init(lonejson_oauth2_token_flow *flow);
/** Releases all storage owned by a token flow. */
void lonejson_oauth2_token_flow_cleanup(lonejson_oauth2_token_flow *flow);
/** Copies borrowed or persisted token-flow fields into owned token-flow
 * storage.
 *
 * This is the supported import path when loading a token flow from application
 * storage, Lua tables, or other caller-owned memory. Existing storage in
 * `flow` is released only after all replacement strings have been copied.
 */
lonejson_status
lonejson_oauth2_token_flow_assign(lonejson_oauth2_token_flow *flow,
                                  const lonejson_oauth2_token_flow *source,
                                  lonejson_error *error);
/** Returns non-zero when the flow has no usable access token at `now`.
 *
 * A zero `skew_seconds` applies the default 60-second refresh skew. Negative
 * skew is treated as zero.
 */
int lonejson_oauth2_token_flow_is_expired(
    const lonejson_oauth2_token_flow *flow, lonejson_int64 now,
    lonejson_int64 skew_seconds);
/** Updates token-flow storage from a successful token endpoint response.
 *
 * Returned strings are copied. Existing refresh tokens are preserved when a
 * refresh response omits `refresh_token`, matching common OAuth2 rotation
 * behavior.
 */
lonejson_status lonejson_oauth2_token_flow_update_response(
    lonejson_oauth2_token_flow *flow,
    const lonejson_oauth2_token_response *response, lonejson_int64 now,
    lonejson_error *error);
/** Ensures a flow has a usable access token, refreshing through the runtime
 * HTTP provider when refresh is enabled and required.
 *
 * This helper never performs browser interaction and never persists state.
 * When interaction or re-authorization is needed it returns
 * `LONEJSON_STATUS_OK` with result state
 * `LONEJSON_OAUTH2_TOKEN_FLOW_NEEDS_INTERACTION`.
 */
lonejson_status lonejson_oauth2_token_flow_ensure(
    lonejson *runtime, lonejson_oauth2_token_flow *flow,
    const lonejson_oauth2_token_flow_policy *policy,
    lonejson_oauth2_token_flow_result *result, lonejson_error *error);
/** Introspects a token through the runtime HTTP provider. */
lonejson_status lonejson_oauth2_introspect_token_request(
    lonejson *runtime, const char *introspection_endpoint,
    const lonejson_oauth2_token_introspection *request,
    size_t max_response_bytes, lonejson_oauth2_introspection_response *out,
    lonejson_error *error);
/** Revokes a token through the runtime HTTP provider. */
lonejson_status lonejson_oauth2_revoke_token_request(
    lonejson *runtime, const char *revocation_endpoint,
    const lonejson_oauth2_token_revocation *request, lonejson_error *error);
/** Initializes an OIDC UserInfo response for request or cleanup. */
void lonejson_oidc_userinfo_response_init(lonejson_oidc_userinfo_response *out);
/** Releases all storage owned by an OIDC UserInfo response. */
void lonejson_oidc_userinfo_response_cleanup(
    lonejson_oidc_userinfo_response *out);
/** Parses and validates a bounded OIDC UserInfo JSON response. */
lonejson_status lonejson_oidc_userinfo_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oidc_userinfo_response *out, lonejson_error *error);
/** Fetches OIDC UserInfo through the runtime HTTP provider. */
lonejson_status
lonejson_oidc_fetch_userinfo(lonejson *runtime, const char *userinfo_endpoint,
                             const lonejson_oidc_userinfo_request *request,
                             lonejson_oidc_userinfo_response *out,
                             lonejson_error *error);
/** Exchanges an OIDC/OAuth2 authorization code through the runtime HTTP
 * provider.
 */
lonejson_status lonejson_oidc_authorization_code_token_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oidc_authorization_code_token *request,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error);
/** Initializes a PKCE pair for generation or cleanup. */
void lonejson_oidc_pkce_init(lonejson_oidc_pkce *pkce);
/** Releases all storage owned by a PKCE pair. */
void lonejson_oidc_pkce_cleanup(lonejson_oidc_pkce *pkce);
/** Computes a base64url S256 PKCE challenge for a caller-provided verifier.
 *
 * This convenience helper has no built-in crypto dependency. Use
 * `lonejson_oidc_pkce_challenge_with_runtime()` and install an auth provider
 * with `sha256` when cryptographic work is required.
 */
lonejson_status lonejson_oidc_pkce_challenge(const char *code_verifier,
                                             lonejson_owned_buffer *out,
                                             lonejson_error *error);
/** Computes a PKCE S256 challenge through the runtime auth provider.
 *
 * The provider must implement `sha256`.
 */
lonejson_status lonejson_oidc_pkce_challenge_with_runtime(
    lonejson *runtime, const char *code_verifier, lonejson_owned_buffer *out,
    lonejson_error *error);
/** Generates a random PKCE verifier and matching S256 challenge.
 *
 * `verifier_bytes == 0` uses the default 32 random bytes. Valid non-zero
 * values are 32..96, producing RFC 7636 verifier lengths of 43..128 chars.
 * This convenience helper has no built-in crypto dependency. Use
 * `lonejson_oidc_pkce_generate_with_runtime()` with an auth provider that
 * implements `random_bytes` and `sha256`.
 */
lonejson_status lonejson_oidc_pkce_generate(size_t verifier_bytes,
                                            lonejson_oidc_pkce *out,
                                            lonejson_error *error);
/** Generates a PKCE verifier and matching challenge through the runtime auth
 * provider.
 *
 * The provider must implement `random_bytes` and `sha256`.
 */
lonejson_status lonejson_oidc_pkce_generate_with_runtime(
    lonejson *runtime, size_t verifier_bytes, lonejson_oidc_pkce *out,
    lonejson_error *error);
/** Builds an authorization-code URL with PKCE S256 parameters. */
lonejson_status lonejson_oidc_authorization_url(
    const lonejson_oidc_authorization_request *request,
    lonejson_owned_buffer *out, lonejson_error *error);
/** Initializes a parsed authorization callback for parsing or cleanup. */
void lonejson_oidc_authorization_callback_init(
    lonejson_oidc_authorization_callback *callback);
/** Releases all storage owned by a parsed authorization callback. */
void lonejson_oidc_authorization_callback_cleanup(
    lonejson_oidc_authorization_callback *callback);
/** Parses and validates an authorization-code callback query string.
 *
 * `query` may start with `?`. Provider error callbacks are rejected with
 * `LONEJSON_STATUS_TYPE_MISMATCH`. `expected_state` is required.
 */
lonejson_status lonejson_oidc_authorization_callback_parse_query(
    const char *query, size_t len, const char *expected_state,
    size_t max_query_bytes, lonejson_oidc_authorization_callback *out,
    lonejson_error *error);
/** Returns a stable string for one bearer-token authentication failure class.
 */
const char *lonejson_auth_failure_string(lonejson_auth_failure failure);
/** Initializes bearer validation output storage. */
void lonejson_oidc_bearer_validation_init(
    lonejson_oidc_bearer_validation *validation);
/** Releases storage owned by a bearer validation result. */
void lonejson_oidc_bearer_validation_cleanup(
    lonejson_oidc_bearer_validation *validation);
/** Extracts the compact JWT from an HTTP Authorization Bearer header value. */
lonejson_status
lonejson_oidc_authorization_bearer_token(const char *authorization_header,
                                         lonejson_jwt_segment *out,
                                         lonejson_error *error);
/** Validates one Authorization Bearer JWT against a fresh JWKS cache and claim
 * policy.
 */
lonejson_status lonejson_oidc_validate_bearer_token(
    lonejson *runtime, const lonejson_oidc_bearer_validation_request *request,
    lonejson_oidc_bearer_validation *out, lonejson_error *error);
/** Initializes an M2M credential result for generation or cleanup. */
void lonejson_m2m_credential_init(lonejson_m2m_credential *credential);
/** Releases storage owned by an M2M credential result. */
void lonejson_m2m_credential_cleanup(lonejson_m2m_credential *credential);
/** Generates a store-ready M2M credential record and one-time clear secrets. */
lonejson_status lonejson_m2m_credential_generate(
    lonejson *runtime, const lonejson_m2m_credential_request *request,
    lonejson_m2m_credential *out, lonejson_error *error);
/** Initializes an M2M authentication result. */
void lonejson_m2m_authentication_init(lonejson_m2m_authentication *auth);
/** Releases storage owned by an M2M authentication result. */
void lonejson_m2m_authentication_cleanup(lonejson_m2m_authentication *auth);
/** Verifies an HTTP Authorization header against a JSON credential store. */
lonejson_status lonejson_m2m_verify_authorization(
    lonejson *runtime, const lonejson_m2m_verify_request *request,
    lonejson_m2m_authentication *out, lonejson_error *error);
/** Initializes an M2M signup seed result for generation or cleanup. */
void lonejson_m2m_signup_init(lonejson_m2m_signup *signup);
/** Releases storage owned by an M2M signup seed result. */
void lonejson_m2m_signup_cleanup(lonejson_m2m_signup *signup);
/** Generates a signup seed record plus optional query/URL values. */
lonejson_status
lonejson_m2m_signup_generate(lonejson *runtime,
                             const lonejson_m2m_signup_request *request,
                             lonejson_m2m_signup *out, lonejson_error *error);
/** Initializes an M2M signup completion result for completion or cleanup. */
void lonejson_m2m_signup_complete_init(
    lonejson_m2m_signup_completion *complete);
/** Releases storage owned by an M2M signup completion result. */
void lonejson_m2m_signup_complete_cleanup(
    lonejson_m2m_signup_completion *complete);
/** Verifies a signup seed and returns a generated credential for the user. */
lonejson_status lonejson_m2m_signup_complete(
    lonejson *runtime, const lonejson_m2m_signup_complete_request *request,
    lonejson_m2m_signup_completion *out, lonejson_error *error);
#endif

#ifdef LONEJSON_WITH_CURL
#include <curl/curl.h>

/** Curl response parse adapter state for incremental `CURLOPT_WRITEFUNCTION`
 * parsing. Initialize all curl adapter storage to zero before its first use
 * (for example, `lonejson_curl_parse ctx = {0}`). Init may then be called again
 * on a live adapter; it releases the previous state before replacing it.
 */
struct lonejson_curl_parse {
  /** Opaque push-parser state owned by the curl parse adapter. */
  void *parser;
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
  /** Internal runtime snapshot retained by the curl parse adapter. */
  void *runtime_snapshot;
#else
  void *_reserved_runtime_snapshot;
#endif
  /** Last parse error captured by the curl parse adapter. */
  lonejson_error error;
  /** Reserved opaque lifecycle state for the curl parse adapter. */
  unsigned char _reserved_state[8];
  /** Feeds one curl write chunk into this parse adapter. */
  size_t (*write_callback)(struct lonejson_curl_parse *ctx, char *ptr,
                           size_t size, size_t nmemb);
  /** Finalizes this parse adapter after curl EOF. */
  lonejson_status (*finish)(struct lonejson_curl_parse *ctx);
  /** Releases resources owned by this parse adapter. */
  void (*cleanup)(struct lonejson_curl_parse *ctx);
};

/** Curl response adapter state for streaming selected array items from
 * `CURLOPT_WRITEFUNCTION` chunks.
 */
struct lonejson_curl_array_parse {
  /** Push-fed selected-array stream owned by the curl adapter. */
  lonejson_array_stream *stream;
  /** Map used to parse each selected array item. */
  const lonejson_map *map;
  /** Reused destination populated before each item callback. */
  void *dst;
  /** Callback invoked for each complete selected array item. */
  lonejson_array_stream_item_fn callback;
  /** User data passed to `callback`. */
  void *user;
  /** Last parse or callback error captured by the curl adapter. */
  lonejson_error error;
  /** Reserved opaque lifecycle state for the curl array adapter. */
  unsigned char _reserved_state[8];
  /** Feeds one curl write chunk into this array parse adapter. */
  size_t (*write_callback)(struct lonejson_curl_array_parse *ctx, char *ptr,
                           size_t size, size_t nmemb);
  /** Finalizes this array parse adapter after curl EOF. */
  lonejson_status (*finish)(struct lonejson_curl_array_parse *ctx);
  /** Releases resources owned by this array parse adapter. */
  void (*cleanup)(struct lonejson_curl_array_parse *ctx);
};

/** Curl response adapter state for streaming selected string array items from
 * `CURLOPT_WRITEFUNCTION` chunks.
 */
struct lonejson_curl_string_array_parse {
  /** Push-fed selected-array stream owned by the curl adapter. */
  lonejson_array_stream *stream;
  /** Handler invoked for decoded string item chunks. */
  lonejson_array_stream_string_handler handler;
  /** User data passed to `handler`. */
  void *user;
  /** Last parse or callback error captured by the curl adapter. */
  lonejson_error error;
  /** Reserved opaque lifecycle state for the curl string-array adapter. */
  unsigned char _reserved_state[8];
  /** Feeds one curl write chunk into this string-array parse adapter. */
  size_t (*write_callback)(struct lonejson_curl_string_array_parse *ctx,
                           char *ptr, size_t size, size_t nmemb);
  /** Finalizes this string-array parse adapter after curl EOF. */
  lonejson_status (*finish)(struct lonejson_curl_string_array_parse *ctx);
  /** Releases resources owned by this string-array parse adapter. */
  void (*cleanup)(struct lonejson_curl_string_array_parse *ctx);
};

/** Curl response adapter state for selected string array items reported once
 * per decoded item from `CURLOPT_WRITEFUNCTION` chunks.
 */
struct lonejson_curl_string_items_parse {
  /** Push-fed selected-array stream owned by the curl adapter. */
  lonejson_array_stream *stream;
  /** Callback invoked for each complete decoded string item. */
  lonejson_array_stream_string_fn callback;
  /** User data passed to `callback`. */
  void *user;
  /** Last parse or callback error captured by the curl adapter. */
  lonejson_error error;
  /** Reserved opaque lifecycle state for the curl string-items adapter. */
  unsigned char _reserved_state[8];
  /** Feeds one curl write chunk into this string-items parse adapter. */
  size_t (*write_callback)(struct lonejson_curl_string_items_parse *ctx,
                           char *ptr, size_t size, size_t nmemb);
  /** Finalizes this string-items parse adapter after curl EOF. */
  lonejson_status (*finish)(struct lonejson_curl_string_items_parse *ctx);
  /** Releases resources owned by this string-items parse adapter. */
  void (*cleanup)(struct lonejson_curl_string_items_parse *ctx);
};

/** Curl upload adapter state for streaming generated JSON to libcurl. */
struct lonejson_curl_upload {
  /** Underlying pull-style JSON generator used by the curl adapter. */
  lonejson_generator generator;
  /** Reserved opaque lifecycle state for the curl upload adapter. */
  unsigned char _reserved_state[8];
  /** Reads one curl upload chunk from this adapter. */
  size_t (*read_callback)(struct lonejson_curl_upload *ctx, char *ptr,
                          size_t size, size_t nmemb);
  /** Returns the upload size this adapter reports to curl. */
  curl_off_t (*size_fn)(const struct lonejson_curl_upload *ctx);
  /** Releases resources owned by this upload adapter. */
  void (*cleanup)(struct lonejson_curl_upload *ctx);
  /** Checks whether all emitted fields support replay without reading them. */
  int (*is_rewindable)(const struct lonejson_curl_upload *ctx);
  /** Restarts serialization; see lonejson_curl_upload_rewind for failures. */
  lonejson_status (*rewind)(struct lonejson_curl_upload *ctx);
  /** Handles curl seek requests using the upload's replay policy. */
  int (*seek_callback)(struct lonejson_curl_upload *ctx, curl_off_t offset,
                       int origin);
};

#ifdef LONEJSON_WITH_OIDC
/** Curl response adapter that installs a bounded JWKS response into a cache at
 * EOF. The policy strings are copied at init; `runtime` and `cache` are
 * caller-owned and must remain valid until finish/cleanup.
 */
struct lonejson_oidc_jwks_cache_parse {
  lonejson_owned_buffer response;
  lonejson *runtime;
  lonejson_oidc_jwks_cache *cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_error error;
  unsigned char _reserved_state[8];
  size_t (*write_callback)(struct lonejson_oidc_jwks_cache_parse *ctx,
                           char *ptr, size_t size, size_t nmemb);
  lonejson_status (*finish)(struct lonejson_oidc_jwks_cache_parse *ctx);
  void (*cleanup)(struct lonejson_oidc_jwks_cache_parse *ctx);
};
#endif

/** Initializes a curl parse adapter suitable for `CURLOPT_WRITEFUNCTION`. */
lonejson_status lonejson_curl_parse_init(lonejson_curl_parse *ctx,
                                         lonejson *runtime,
                                         const lonejson_map *map, void *dst);
/** Curl write callback that incrementally feeds response bytes to lonejson. */
size_t lonejson_curl_write_callback(char *ptr, size_t size, size_t nmemb,
                                    void *userdata);
/** Finalizes a curl-backed parse adapter. */
lonejson_status lonejson_curl_parse_finish(lonejson_curl_parse *ctx);
/** Releases resources owned by a curl parse adapter. */
void lonejson_curl_parse_cleanup(lonejson_curl_parse *ctx);

/** Initializes a curl write adapter that streams selected array items directly
 * into `dst` and reports each item through `callback`.
 */
lonejson_status lonejson_curl_array_parse_init(
    lonejson_curl_array_parse *ctx, lonejson *runtime, const char *path,
    const lonejson_map *map, void *dst, lonejson_array_stream_item_fn callback,
    void *user);
/** Curl write callback that forwards response bytes into a selected-array
 * stream adapter.
 */
size_t lonejson_curl_array_write_callback(char *ptr, size_t size, size_t nmemb,
                                          void *userdata);
/** Finalizes a curl selected-array stream adapter after curl reaches EOF. */
lonejson_status
lonejson_curl_array_parse_finish(lonejson_curl_array_parse *ctx);
/** Releases resources owned by a curl selected-array stream adapter. */
void lonejson_curl_array_parse_cleanup(lonejson_curl_array_parse *ctx);

/** Initializes a curl write adapter that streams selected string-array items.
 */
lonejson_status lonejson_curl_string_array_parse_init(
    lonejson_curl_string_array_parse *ctx, lonejson *runtime, const char *path,
    const lonejson_array_stream_string_handler *handler, void *user);
/** Curl write callback for a selected string-array stream adapter. */
size_t lonejson_curl_string_array_write_callback(char *ptr, size_t size,
                                                 size_t nmemb, void *userdata);
/** Finalizes a curl selected string-array stream adapter after curl EOF. */
lonejson_status
lonejson_curl_string_array_parse_finish(lonejson_curl_string_array_parse *ctx);
/** Releases resources owned by a curl selected string-array stream adapter. */
void lonejson_curl_string_array_parse_cleanup(
    lonejson_curl_string_array_parse *ctx);

/** Initializes a curl write adapter that reports selected string-array items
 * once per decoded string. Each item is assembled through the chunked string
 * path into bounded temporary storage.
 */
lonejson_status lonejson_curl_string_items_parse_init(
    lonejson_curl_string_items_parse *ctx, lonejson *runtime, const char *path,
    lonejson_array_stream_string_fn callback, void *user);
/** Curl write callback for a selected string-array item adapter. */
size_t lonejson_curl_string_items_write_callback(char *ptr, size_t size,
                                                 size_t nmemb, void *userdata);
/** Finalizes a curl selected string-array item adapter after curl EOF. */
lonejson_status
lonejson_curl_string_items_parse_finish(lonejson_curl_string_items_parse *ctx);
/** Releases resources owned by a curl selected string-array item adapter. */
void lonejson_curl_string_items_parse_cleanup(
    lonejson_curl_string_items_parse *ctx);

/** Initializes a curl upload adapter for a mapped struct.
 *
 * This adapter is stream-first: it incrementally serializes through curl's
 * `CURLOPT_READFUNCTION` callback instead of materializing the whole JSON
 * payload up front. The reported size is currently always `-1`; lonejson does
 * not do a hidden prebuffer or pre-count pass just to compute it.
 */
lonejson_status lonejson_curl_upload_init(lonejson_curl_upload *ctx,
                                          lonejson *runtime,
                                          const lonejson_map *map,
                                          const void *src);
/** Curl read callback that streams serialized JSON bytes to libcurl. */
size_t lonejson_curl_read_callback(char *ptr, size_t size, size_t nmemb,
                                   void *userdata);
/** Checks replay capability without reading source contents. File probes may
 * seek and restore position. Omitted fields are ignored. Returns zero for an
 * inactive adapter. Capability does not guarantee subsequent source I/O.
 */
int lonejson_curl_upload_is_rewindable(const lonejson_curl_upload *ctx);
/** Restarts serialization from byte zero, including after partial reads or EOF.
 * Map, values, source contents, and allocator context must remain alive and
 * stable through cleanup. The runtime handle is not retained. No size pass or
 * full-message buffering occurs. Returns INVALID_ARGUMENT for inactive or
 * non-rewindable uploads. Failures set generator.error; allocation failure
 * preserves the previous cursor. Source I/O errors are reported on later reads.
 */
lonejson_status lonejson_curl_upload_rewind(lonejson_curl_upload *ctx);
/** Install as CURLOPT_SEEKFUNCTION with this upload as CURLOPT_SEEKDATA.
 * Only (0, SEEK_SET) is supported. Unsupported seeks or one-shot documents
 * return CURL_SEEKFUNC_CANTSEEK without consuming input or changing
 * diagnostics. Inactive adapters and restart failures return
 * CURL_SEEKFUNC_FAIL.
 */
int lonejson_curl_seek_callback(void *userdata, curl_off_t offset, int origin);
/** Returns the upload size reported by a curl upload adapter. The value is
 * `-1` when lonejson is streaming with an unknown total length.
 */
curl_off_t lonejson_curl_upload_size(const lonejson_curl_upload *ctx);
/** Releases resources owned by a curl upload adapter. */
void lonejson_curl_upload_cleanup(lonejson_curl_upload *ctx);
#ifdef LONEJSON_WITH_OIDC
/** Initializes a curl write adapter for a bounded JWKS cache refresh.
 *
 * The adapter copies the policy strings. The runtime and cache pointers remain
 * caller-owned and must outlive finish/cleanup.
 */
lonejson_status lonejson_oidc_jwks_cache_parse_init(
    lonejson_oidc_jwks_cache_parse *ctx, lonejson *runtime,
    lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy);
/** Curl write callback for a JWKS cache refresh adapter. */
size_t lonejson_oidc_jwks_cache_write_callback(char *ptr, size_t size,
                                               size_t nmemb, void *userdata);
/** Finalizes a JWKS cache refresh adapter after curl EOF. */
lonejson_status
lonejson_oidc_jwks_cache_parse_finish(lonejson_oidc_jwks_cache_parse *ctx);
/** Releases resources owned by a JWKS cache refresh adapter. */
void lonejson_oidc_jwks_cache_parse_cleanup(
    lonejson_oidc_jwks_cache_parse *ctx);
#endif
#endif

#ifndef LONEJSON_DISABLE_SHORT_NAMES
/** Major component of the lonejson header version. */
#define LJ_VERSION_MAJOR LONEJSON_VERSION_MAJOR
/** Minor component of the lonejson header version. */
#define LJ_VERSION_MINOR LONEJSON_VERSION_MINOR
/** Patch component of the lonejson header version. */
#define LJ_VERSION_PATCH LONEJSON_VERSION_PATCH
/** Shared-library ABI / SONAME version for binary compatibility tracking. */
#define LJ_ABI_VERSION LONEJSON_ABI_VERSION
/** ABI version for public borrowed schema and record view structs. */
#define LJ_VIEW_ABI_VERSION LONEJSON_VIEW_ABI_VERSION

#define LJ_UINT64_MAX LONEJSON_UINT64_MAX
/** Marks a mapping field as required during parse. */
#define LJ_FIELD_REQUIRED LONEJSON_FIELD_REQUIRED
/** Omits an optional field during serialization when its value is JSON `null`.
 */
#define LJ_FIELD_OMIT_NULL LONEJSON_FIELD_OMIT_NULL
/** Omits an optional field during serialization when its value is empty. This
 * also implies `LONEJSON_FIELD_OMIT_NULL` behavior. */
#define LJ_FIELD_OMIT_EMPTY LONEJSON_FIELD_OMIT_EMPTY
/** Internal flag used by presence-gated primitive field macros. */
#define LJ_FIELD_HAS_PRESENCE LONEJSON_FIELD_HAS_PRESENCE
/** Internal flag allowing JSON `null` to parse as an absent optional primitive
 * presence field. */
#define LJ_FIELD_ACCEPT_NULL LONEJSON_FIELD_ACCEPT_NULL
/** Internal/runtime flag indicating that an array container owns its backing
 * allocation. */
#define LJ_ARRAY_OWNS_ITEMS LONEJSON_ARRAY_OWNS_ITEMS
/** Marks an array container as using caller-owned fixed-capacity storage. */
#define LJ_ARRAY_FIXED_CAPACITY LONEJSON_ARRAY_FIXED_CAPACITY
#ifndef LJ_MALLOC
/** Overrides lonejson's internal `malloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#define LJ_MALLOC LONEJSON_MALLOC
#endif
#ifndef LJ_CALLOC
/** Overrides lonejson's internal `calloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#define LJ_CALLOC LONEJSON_CALLOC
#endif
#ifndef LJ_REALLOC
/** Overrides lonejson's internal `realloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#define LJ_REALLOC LONEJSON_REALLOC
#endif
#ifndef LJ_FREE
/** Overrides lonejson's internal `free` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#define LJ_FREE LONEJSON_FREE
#endif
#ifndef LJ_PARSER_BUFFER_SIZE
/** Total internal parser workspace budget used by one-shot parse/validate
 * entry points. */
#define LJ_PARSER_BUFFER_SIZE LONEJSON_PARSER_BUFFER_SIZE
#endif
#ifndef LJ_READER_BUFFER_SIZE
/** Private read-buffer size used by one-shot reader-based parse/validate entry
 * points. */
#define LJ_READER_BUFFER_SIZE LONEJSON_READER_BUFFER_SIZE
#endif
#ifndef LJ_PUSH_PARSER_BUFFER_SIZE
/** Total internal parser workspace budget used by the push parser.
 * Defaults to `LONEJSON_PARSER_BUFFER_SIZE`. */
#define LJ_PUSH_PARSER_BUFFER_SIZE LONEJSON_PUSH_PARSER_BUFFER_SIZE
#endif
#ifndef LJ_STREAM_BUFFER_SIZE
/** Private read-buffer size used by object-framed streaming APIs. Defaults to
 * `LONEJSON_READER_BUFFER_SIZE`. */
#define LJ_STREAM_BUFFER_SIZE LONEJSON_STREAM_BUFFER_SIZE
#endif
#ifndef LJ_SPOOL_MEMORY_LIMIT
/** Default in-memory threshold before streamed fields spill into a temporary
 * file. */
#define LJ_SPOOL_MEMORY_LIMIT LONEJSON_SPOOL_MEMORY_LIMIT
#endif
#ifndef LJ_WRITE_MAX_OUTPUT_BYTES
/** Default hard cap for serializer-owned output buffers. Set explicit
 * `lonejson_config.write_max_output_bytes` when a runtime needs a larger
 * materialized result.
 */
#define LJ_WRITE_MAX_OUTPUT_BYTES LONEJSON_WRITE_MAX_OUTPUT_BYTES
#endif
#ifndef LJ_SPOOL_TEMP_PATH_CAPACITY
/** Fixed path-buffer capacity used for named temporary spool files. */
#define LJ_SPOOL_TEMP_PATH_CAPACITY LONEJSON_SPOOL_TEMP_PATH_CAPACITY
#endif
#ifndef LJ_TRACK_WORKSPACE_USAGE
/** Enables parser workspace high-water tracking when non-zero. */
#define LJ_TRACK_WORKSPACE_USAGE LONEJSON_TRACK_WORKSPACE_USAGE
#endif

/** Operation completed successfully. */
#define LJ_STATUS_OK LONEJSON_STATUS_OK
/** Caller supplied an invalid pointer, size, or incompatible argument. */
#define LJ_STATUS_INVALID_ARGUMENT LONEJSON_STATUS_INVALID_ARGUMENT
/** Input was not syntactically valid JSON. */
#define LJ_STATUS_INVALID_JSON LONEJSON_STATUS_INVALID_JSON
/** A JSON value did not match the mapped destination field type. */
#define LJ_STATUS_TYPE_MISMATCH LONEJSON_STATUS_TYPE_MISMATCH
/** A field marked required by the map was not present in the object. */
#define LJ_STATUS_MISSING_REQUIRED_FIELD LONEJSON_STATUS_MISSING_REQUIRED_FIELD
/** Duplicate object keys were rejected by parse options. */
#define LJ_STATUS_DUPLICATE_FIELD LONEJSON_STATUS_DUPLICATE_FIELD
/** A fixed-capacity destination or configured spool limit was exceeded. */
#define LJ_STATUS_OVERFLOW LONEJSON_STATUS_OVERFLOW
/** Output was truncated under a non-failing overflow policy. */
#define LJ_STATUS_TRUNCATED LONEJSON_STATUS_TRUNCATED
/** lonejson could not allocate internal or mapped dynamic storage. */
#define LJ_STATUS_ALLOCATION_FAILED LONEJSON_STATUS_ALLOCATION_FAILED
/** A caller-provided sink or reader callback reported failure. */
#define LJ_STATUS_CALLBACK_FAILED LONEJSON_STATUS_CALLBACK_FAILED
/** An underlying file descriptor or `FILE *` operation failed. */
#define LJ_STATUS_IO_ERROR LONEJSON_STATUS_IO_ERROR
/** The operation is valid, but unsupported for the selected runtime shape. */
#define LJ_STATUS_UNSUPPORTED LONEJSON_STATUS_UNSUPPORTED
/** lonejson encountered an unexpected internal state. */
#define LJ_STATUS_INTERNAL_ERROR LONEJSON_STATUS_INTERNAL_ERROR

/** JSON string stored in a fixed buffer or allocated `char *`. */
#define LJ_FIELD_KIND_STRING LONEJSON_FIELD_KIND_STRING
/** JSON string streamed into a `lonejson_spooled` handle. */
#define LJ_FIELD_KIND_STRING_STREAM LONEJSON_FIELD_KIND_STRING_STREAM
/** Base64 JSON string decoded into a `lonejson_spooled` handle. */
#define LJ_FIELD_KIND_BASE64_STREAM LONEJSON_FIELD_KIND_BASE64_STREAM
/** Serialize-only JSON string streamed from a `lonejson_source` handle. */
#define LJ_FIELD_KIND_STRING_SOURCE LONEJSON_FIELD_KIND_STRING_SOURCE
/** Serialize-only Base64 JSON string streamed from a `lonejson_source`
 * handle. */
#define LJ_FIELD_KIND_BASE64_SOURCE LONEJSON_FIELD_KIND_BASE64_SOURCE
/** Arbitrary embedded JSON value stored or streamed through
 * `lonejson_json_value`. */
#define LJ_FIELD_KIND_JSON_VALUE LONEJSON_FIELD_KIND_JSON_VALUE
/** JSON integer stored in a `lonejson_int64`. */
#define LJ_FIELD_KIND_I64 LONEJSON_FIELD_KIND_I64
/** JSON unsigned integer stored in a `lonejson_uint64`. */
#define LJ_FIELD_KIND_U64 LONEJSON_FIELD_KIND_U64
/** JSON number stored in a `double`. */
#define LJ_FIELD_KIND_F64 LONEJSON_FIELD_KIND_F64
/** JSON boolean stored in a `bool`. */
#define LJ_FIELD_KIND_BOOL LONEJSON_FIELD_KIND_BOOL
/** Nested JSON object mapped by a nested `lonejson_map`. */
#define LJ_FIELD_KIND_OBJECT LONEJSON_FIELD_KIND_OBJECT
/** JSON array of strings stored in `lonejson_string_array`. */
#define LJ_FIELD_KIND_STRING_ARRAY LONEJSON_FIELD_KIND_STRING_ARRAY
/** JSON array of strings streamed through `lonejson_string_array_stream`. */
#define LJ_FIELD_KIND_STRING_ARRAY_STREAM                                      \
  LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM
/** JSON array of mapped objects streamed item-by-item. */
#define LJ_FIELD_KIND_MAPPED_ARRAY_STREAM                                      \
  LONEJSON_FIELD_KIND_MAPPED_ARRAY_STREAM
/** JSON array of integers stored in `lonejson_i64_array`. */
#define LJ_FIELD_KIND_I64_ARRAY LONEJSON_FIELD_KIND_I64_ARRAY
/** JSON array of unsigned integers stored in `lonejson_u64_array`. */
#define LJ_FIELD_KIND_U64_ARRAY LONEJSON_FIELD_KIND_U64_ARRAY
/** JSON array of numbers stored in `lonejson_f64_array`. */
#define LJ_FIELD_KIND_F64_ARRAY LONEJSON_FIELD_KIND_F64_ARRAY
/** JSON array of booleans stored in `lonejson_bool_array`. */
#define LJ_FIELD_KIND_BOOL_ARRAY LONEJSON_FIELD_KIND_BOOL_ARRAY
/** JSON array of nested objects stored in `lonejson_object_array`. */
#define LJ_FIELD_KIND_OBJECT_ARRAY LONEJSON_FIELD_KIND_OBJECT_ARRAY
#ifdef LONEJSON_WITH_OIDC
#define LJ_M2M_AUTH_BASIC LONEJSON_M2M_AUTH_BASIC
#define LJ_M2M_AUTH_BEARER LONEJSON_M2M_AUTH_BEARER
#define LJ_M2M_AUTH_DEFAULT LONEJSON_M2M_AUTH_DEFAULT
#endif

/** No source configured. Serializes as JSON `null`. */
#define LJ_SOURCE_NONE LONEJSON_SOURCE_NONE
/** Read raw bytes from a caller-provided `FILE *`. */
#define LJ_SOURCE_FILE LONEJSON_SOURCE_FILE
/** Read raw bytes from a caller-provided file descriptor. */
#define LJ_SOURCE_FD LONEJSON_SOURCE_FD
/** Open and read a filesystem path on each serialization. */
#define LJ_SOURCE_PATH LONEJSON_SOURCE_PATH

/** No value configured. Serializes as JSON `null`. */
#define LJ_JSON_VALUE_NULL LONEJSON_JSON_VALUE_NULL
/** Owned compact JSON bytes retained in memory. */
#define LJ_JSON_VALUE_BUFFER LONEJSON_JSON_VALUE_BUFFER
/** Read JSON bytes from a caller-provided reader callback. */
#define LJ_JSON_VALUE_READER LONEJSON_JSON_VALUE_READER
/** Read JSON bytes from a caller-provided `FILE *`. */
#define LJ_JSON_VALUE_FILE LONEJSON_JSON_VALUE_FILE
/** Read JSON bytes from a caller-provided file descriptor. */
#define LJ_JSON_VALUE_FD LONEJSON_JSON_VALUE_FD
/** Open and read JSON bytes from a filesystem path on each serialization. */
#define LJ_JSON_VALUE_PATH LONEJSON_JSON_VALUE_PATH

/** No existing selected value was present. */
#define LJ_VALUE_ABSENT LONEJSON_VALUE_ABSENT
/** Existing selected value is a JSON object. */
#define LJ_VALUE_OBJECT LONEJSON_VALUE_OBJECT
/** Existing selected value is a JSON array. */
#define LJ_VALUE_ARRAY LONEJSON_VALUE_ARRAY
/** Existing selected value is a JSON string. */
#define LJ_VALUE_STRING LONEJSON_VALUE_STRING
/** Existing selected value is a JSON number. */
#define LJ_VALUE_NUMBER LONEJSON_VALUE_NUMBER
/** Existing selected value is a JSON boolean. */
#define LJ_VALUE_BOOL LONEJSON_VALUE_BOOL
/** Existing selected value is JSON `null`. */
#define LJ_VALUE_NULL LONEJSON_VALUE_NULL

/** lonejson owns and allocates storage as needed. */
#define LJ_STORAGE_DYNAMIC LONEJSON_STORAGE_DYNAMIC
/** Caller provides fixed-capacity storage inside the mapped struct. */
#define LJ_STORAGE_FIXED LONEJSON_STORAGE_FIXED

/** Treat overflow as an error and stop. */
#define LJ_OVERFLOW_FAIL LONEJSON_OVERFLOW_FAIL
/** Truncate but report `LONEJSON_STATUS_TRUNCATED`. */
#define LJ_OVERFLOW_TRUNCATE LONEJSON_OVERFLOW_TRUNCATE
/** Truncate silently while still setting `error.truncated`. */
#define LJ_OVERFLOW_TRUNCATE_SILENT LONEJSON_OVERFLOW_TRUNCATE_SILENT

/** One complete top-level object was parsed into the destination. */
#define LJ_STREAM_OBJECT LONEJSON_STREAM_OBJECT
/** No more objects remain in the underlying stream. */
#define LJ_STREAM_EOF LONEJSON_STREAM_EOF
/** The underlying source would block before a full object was available. */
#define LJ_STREAM_WOULD_BLOCK LONEJSON_STREAM_WOULD_BLOCK
/** A parse, I/O, or argument error occurred. Inspect `lonejson_stream_error`.
 */
#define LJ_STREAM_ERROR LONEJSON_STREAM_ERROR

/** One complete selected array item was parsed into the destination. */
#define LJ_ARRAY_STREAM_ITEM LONEJSON_ARRAY_STREAM_ITEM
/** No more items remain in the selected array and the document is valid. */
#define LJ_ARRAY_STREAM_EOF LONEJSON_ARRAY_STREAM_EOF
/** The underlying source would block before an array item was available. */
#define LJ_ARRAY_STREAM_WOULD_BLOCK LONEJSON_ARRAY_STREAM_WOULD_BLOCK
/** A parse, I/O, path, or argument error occurred. */
#define LJ_ARRAY_STREAM_ERROR LONEJSON_ARRAY_STREAM_ERROR
/** Emit the original selected item. */
#define LJ_ARRAY_REWRITE_KEEP LONEJSON_ARRAY_REWRITE_KEEP
/** Omit the original selected item. */
#define LJ_ARRAY_REWRITE_DROP LONEJSON_ARRAY_REWRITE_DROP
/** Emit `replacement` instead of the original selected item. */
#define LJ_ARRAY_REWRITE_REPLACE LONEJSON_ARRAY_REWRITE_REPLACE
/** Emit `insert` before the original selected item, then keep the item. */
#define LJ_ARRAY_REWRITE_INSERT_BEFORE LONEJSON_ARRAY_REWRITE_INSERT_BEFORE
/** Keep the original selected item, then emit `insert`. */
#define LJ_ARRAY_REWRITE_INSERT_AFTER LONEJSON_ARRAY_REWRITE_INSERT_AFTER
/** Emit `replacement`, then emit `insert`. */
#define LJ_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER                              \
  LONEJSON_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER
/** Emit the original value unchanged. */
#define LJ_VALUE_REWRITE_KEEP LONEJSON_VALUE_REWRITE_KEEP
/** Omit the matched value from its containing object or array. */
#define LJ_VALUE_REWRITE_DROP LONEJSON_VALUE_REWRITE_DROP
/** Emit `replacement` instead of the matched value. */
#define LJ_VALUE_REWRITE_REPLACE LONEJSON_VALUE_REWRITE_REPLACE
/** Inspect the matched value and let `replace` emit the replacement. */
#define LJ_VALUE_REWRITE_REPLACE_WITH LONEJSON_VALUE_REWRITE_REPLACE_WITH

/** Defines a static mapping table for a struct type from a separately declared
 * field array. */
#define LJ_MAP_DEFINE LONEJSON_MAP_DEFINE
/** Maps a JSON string field into a dynamically allocated `char *` member. */
#define LJ_FIELD_STRING_ALLOC LONEJSON_FIELD_STRING_ALLOC
/** Maps a required JSON string field into a dynamically allocated `char *`
 * member. */
#define LJ_FIELD_STRING_ALLOC_REQ LONEJSON_FIELD_STRING_ALLOC_REQ
/** Maps an optional JSON string field into a dynamically allocated `char *`
 * member and omits the field when the pointer is `NULL` during serialization.
 */
#define LJ_FIELD_STRING_ALLOC_OMIT_NULL LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL
/** Maps an optional dynamically allocated JSON string and omits it when the
 * pointer is `NULL` or the string is empty during serialization.
 */
#define LJ_FIELD_STRING_ALLOC_OMIT_EMPTY LONEJSON_FIELD_STRING_ALLOC_OMIT_EMPTY
/** Maps a JSON string field into a fixed-size character array member. */
#define LJ_FIELD_STRING_FIXED LONEJSON_FIELD_STRING_FIXED
/** Maps a required JSON string field into a fixed-size character array member.
 */
#define LJ_FIELD_STRING_FIXED_REQ LONEJSON_FIELD_STRING_FIXED_REQ
/** Maps a fixed-capacity JSON string and omits it when the first byte is NUL
 * during serialization.
 */
#define LJ_FIELD_STRING_FIXED_OMIT_EMPTY LONEJSON_FIELD_STRING_FIXED_OMIT_EMPTY
/** Maps a JSON string field into a `lonejson_spooled` member that keeps an
 * in-memory prefix and spills excess data to a temporary file using default
 * spool options. */
#define LJ_FIELD_STRING_STREAM LONEJSON_FIELD_STRING_STREAM
/** Maps a required JSON string field into a `lonejson_spooled` member using
 * default spool options. */
#define LJ_FIELD_STRING_STREAM_REQ LONEJSON_FIELD_STRING_STREAM_REQ
/** Maps a JSON string field into a `lonejson_spooled` member using one named
 * runtime spool class.
 */
#define LJ_FIELD_STRING_STREAM_CLASS LONEJSON_FIELD_STRING_STREAM_CLASS
/** Maps a streamed JSON string field and omits it when the spooled value is
 * empty during serialization.
 */
#define LJ_FIELD_STRING_STREAM_OMIT_EMPTY                                      \
  LONEJSON_FIELD_STRING_STREAM_OMIT_EMPTY
/** Maps a JSON string field into a `lonejson_spooled` member that decodes
 * Base64 incrementally and stores the decoded bytes using default spool
 * options. */
#define LJ_FIELD_BASE64_STREAM LONEJSON_FIELD_BASE64_STREAM
/** Maps a required Base64-decoded JSON string field into a `lonejson_spooled`
 * member using default spool options. */
#define LJ_FIELD_BASE64_STREAM_REQ LONEJSON_FIELD_BASE64_STREAM_REQ
/** Maps a Base64-decoded JSON string field into a `lonejson_spooled` member
 * using one named runtime spool class.
 */
#define LJ_FIELD_BASE64_STREAM_CLASS LONEJSON_FIELD_BASE64_STREAM_CLASS
/** Maps a streamed Base64 field and omits it when the decoded spooled value is
 * empty during serialization.
 */
#define LJ_FIELD_BASE64_STREAM_OMIT_EMPTY                                      \
  LONEJSON_FIELD_BASE64_STREAM_OMIT_EMPTY
/** Maps a JSON string field into a `lonejson_spooled` member using explicit
 * spool options. */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
#define LJ_FIELD_STRING_STREAM_OPTS LONEJSON_FIELD_STRING_STREAM_OPTS
#endif
/** Maps a serialize-only JSON string field into a `lonejson_source` member
 * that streams raw text bytes from a file, fd, or path at write time. */
#define LJ_FIELD_STRING_SOURCE LONEJSON_FIELD_STRING_SOURCE
/** Maps a required serialize-only JSON string field into a `lonejson_source`
 * member that streams raw text bytes at write time. */
#define LJ_FIELD_STRING_SOURCE_REQ LONEJSON_FIELD_STRING_SOURCE_REQ
/** Maps a serialize-only JSON string source and omits it when no source is
 * configured. */
#define LJ_FIELD_STRING_SOURCE_OMIT_NULL LONEJSON_FIELD_STRING_SOURCE_OMIT_NULL
/** Maps a serialize-only JSON string field into a `lonejson_source` member
 * that base64-encodes raw bytes from a file, fd, or path at write time. */
#define LJ_FIELD_BASE64_SOURCE LONEJSON_FIELD_BASE64_SOURCE
/** Maps a required serialize-only Base64 JSON string field into a
 * `lonejson_source` member that streams raw bytes at write time. */
#define LJ_FIELD_BASE64_SOURCE_REQ LONEJSON_FIELD_BASE64_SOURCE_REQ
/** Maps a serialize-only Base64 source and omits it when no source is
 * configured. */
#define LJ_FIELD_BASE64_SOURCE_OMIT_NULL LONEJSON_FIELD_BASE64_SOURCE_OMIT_NULL
/** Maps an arbitrary embedded JSON value into a `lonejson_json_value` member.
 * The field accepts any JSON value on parse and emits that value directly on
 * serialize without wrapping it as a JSON string. Before parsing into this
 * field, configure the destination handle for sink, visitor, or capture mode.
 */
#define LJ_FIELD_JSON_VALUE LONEJSON_FIELD_JSON_VALUE
/** Maps a required arbitrary embedded JSON value into a `lonejson_json_value`
 * member. Before parsing into this field, configure the destination handle
 * for sink, visitor, or capture mode.
 */
#define LJ_FIELD_JSON_VALUE_REQ LONEJSON_FIELD_JSON_VALUE_REQ
/** Maps an arbitrary embedded JSON value and omits it when the handle is in
 * the `LONEJSON_JSON_VALUE_NULL` state during serialization. */
#define LJ_FIELD_JSON_VALUE_OMIT_NULL LONEJSON_FIELD_JSON_VALUE_OMIT_NULL
/** Maps a Base64-decoded JSON string field into a `lonejson_spooled` member
 * using explicit spool options. */
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
#define LJ_FIELD_BASE64_STREAM_OPTS LONEJSON_FIELD_BASE64_STREAM_OPTS
#endif
/** Maps a JSON integer field into a `lonejson_int64` member. */
#define LJ_FIELD_I64 LONEJSON_FIELD_I64
/** Maps a required JSON integer field into a `lonejson_int64` member. */
#define LJ_FIELD_I64_REQ LONEJSON_FIELD_I64_REQ
/** Maps a JSON integer field that serializes only when an `int` presence
 * member is non-zero. */
#define LJ_FIELD_I64_PRESENT LONEJSON_FIELD_I64_PRESENT
/** Maps a nullable JSON integer field into a `lonejson_int64` member and an
 * `int` presence member. A JSON integer sets presence to `1`; JSON `null` sets
 * presence to `0` and resets the integer member to zero. Serialization omits
 * the field when presence is `0` and emits the integer when presence is `1`. */
#define LJ_FIELD_I64_PRESENT_NULLABLE LONEJSON_FIELD_I64_PRESENT_NULLABLE
/** Maps a JSON unsigned integer field into a `lonejson_uint64` member. */
#define LJ_FIELD_U64 LONEJSON_FIELD_U64
/** Maps a required JSON unsigned integer field into a `lonejson_uint64` member.
 */
#define LJ_FIELD_U64_REQ LONEJSON_FIELD_U64_REQ
/** Maps a JSON unsigned integer field that serializes only when an `int`
 * presence member is non-zero. */
#define LJ_FIELD_U64_PRESENT LONEJSON_FIELD_U64_PRESENT
/** Maps a nullable JSON unsigned integer field into a `lonejson_uint64` member
 * and an `int` presence member. A JSON unsigned integer sets presence to `1`;
 * JSON `null` sets presence to `0` and resets the integer member to zero.
 * Serialization omits the field when presence is `0` and emits the integer
 * when presence is `1`. */
#define LJ_FIELD_U64_PRESENT_NULLABLE LONEJSON_FIELD_U64_PRESENT_NULLABLE
/** Maps a JSON number field into a `double` member. */
#define LJ_FIELD_F64 LONEJSON_FIELD_F64
/** Maps a required JSON number field into a `double` member. */
#define LJ_FIELD_F64_REQ LONEJSON_FIELD_F64_REQ
/** Maps a JSON number field that serializes only when an `int` presence member
 * is non-zero. */
#define LJ_FIELD_F64_PRESENT LONEJSON_FIELD_F64_PRESENT
/** Maps a nullable JSON number field into a `double` member and an `int`
 * presence member. A JSON number sets presence to `1`; JSON `null` sets
 * presence to `0` and resets the number member to zero. Serialization omits
 * the field when presence is `0` and emits the number when presence is `1`. */
#define LJ_FIELD_F64_PRESENT_NULLABLE LONEJSON_FIELD_F64_PRESENT_NULLABLE
/** Maps a JSON boolean field into a `bool` member. */
#define LJ_FIELD_BOOL LONEJSON_FIELD_BOOL
/** Maps a required JSON boolean field into a `bool` member. */
#define LJ_FIELD_BOOL_REQ LONEJSON_FIELD_BOOL_REQ
/** Maps a JSON boolean field that serializes only when an `int` presence
 * member is non-zero. */
#define LJ_FIELD_BOOL_PRESENT LONEJSON_FIELD_BOOL_PRESENT
/** Maps a nullable JSON boolean field into a `bool` member and an `int`
 * presence member. A JSON boolean sets presence to `1`; JSON `null` sets
 * presence to `0` and resets the boolean member to `false`. Serialization
 * omits the field when presence is `0` and emits the boolean when presence is
 * `1`. */
#define LJ_FIELD_BOOL_PRESENT_NULLABLE LONEJSON_FIELD_BOOL_PRESENT_NULLABLE
/** Maps a nested JSON object into a nested struct member using another mapping
 * table. */
#define LJ_FIELD_OBJECT LONEJSON_FIELD_OBJECT
/** Maps a required nested JSON object into a nested struct member using another
 * mapping table. */
#define LJ_FIELD_OBJECT_REQ LONEJSON_FIELD_OBJECT_REQ
/** Maps a nested JSON object and omits it when none of its nested fields would
 * be serialized. */
#define LJ_FIELD_OBJECT_OMIT_EMPTY LONEJSON_FIELD_OBJECT_OMIT_EMPTY
/** Maps a JSON array of strings into a `lonejson_string_array` member. */
#define LJ_FIELD_STRING_ARRAY LONEJSON_FIELD_STRING_ARRAY
/** Maps a JSON array of strings into a chunked streaming field. The destination
 * member must be a `lonejson_string_array_stream` configured with
 * `lonejson_string_array_stream_set_handler` before parsing.
 */
#define LJ_FIELD_STRING_ARRAY_STREAM LONEJSON_FIELD_STRING_ARRAY_STREAM
/** Maps a required JSON array of strings into a chunked streaming field. */
#define LJ_FIELD_STRING_ARRAY_STREAM_REQ LONEJSON_FIELD_STRING_ARRAY_STREAM_REQ
/** Maps a JSON array of objects into an item-by-item mapped stream. The
 * destination member must be a `lonejson_mapped_array_stream` configured with
 * `lonejson_mapped_array_stream_set_handler` before parsing.
 */
#define LJ_FIELD_MAPPED_ARRAY_STREAM LONEJSON_FIELD_MAPPED_ARRAY_STREAM
/** Maps a required JSON array of objects into an item-by-item mapped stream. */
#define LJ_FIELD_MAPPED_ARRAY_STREAM_REQ LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ
/** Maps a JSON array of strings and omits it when `count == 0` during
 * serialization. */
#define LJ_FIELD_STRING_ARRAY_OMIT_EMPTY LONEJSON_FIELD_STRING_ARRAY_OMIT_EMPTY
/** Maps a JSON array of integers into a `lonejson_i64_array` member. */
#define LJ_FIELD_I64_ARRAY LONEJSON_FIELD_I64_ARRAY
/** Maps an integer array and omits it when `count == 0` during serialization.
 */
#define LJ_FIELD_I64_ARRAY_OMIT_EMPTY LONEJSON_FIELD_I64_ARRAY_OMIT_EMPTY
/** Maps a JSON array of unsigned integers into a `lonejson_u64_array` member.
 */
#define LJ_FIELD_U64_ARRAY LONEJSON_FIELD_U64_ARRAY
/** Maps an unsigned integer array and omits it when `count == 0` during
 * serialization.
 */
#define LJ_FIELD_U64_ARRAY_OMIT_EMPTY LONEJSON_FIELD_U64_ARRAY_OMIT_EMPTY
/** Maps a JSON array of numbers into a `lonejson_f64_array` member. */
#define LJ_FIELD_F64_ARRAY LONEJSON_FIELD_F64_ARRAY
/** Maps a number array and omits it when `count == 0` during serialization. */
#define LJ_FIELD_F64_ARRAY_OMIT_EMPTY LONEJSON_FIELD_F64_ARRAY_OMIT_EMPTY
/** Maps a JSON array of booleans into a `lonejson_bool_array` member. */
#define LJ_FIELD_BOOL_ARRAY LONEJSON_FIELD_BOOL_ARRAY
/** Maps a boolean array and omits it when `count == 0` during serialization.
 */
#define LJ_FIELD_BOOL_ARRAY_OMIT_EMPTY LONEJSON_FIELD_BOOL_ARRAY_OMIT_EMPTY
/** Maps a JSON array of nested objects into a `lonejson_object_array` member.
 */
#define LJ_FIELD_OBJECT_ARRAY LONEJSON_FIELD_OBJECT_ARRAY
/** Maps a JSON array of nested objects and omits it when `count == 0` during
 * serialization. */
#define LJ_FIELD_OBJECT_ARRAY_OMIT_EMPTY LONEJSON_FIELD_OBJECT_ARRAY_OMIT_EMPTY

/** Unsigned 32-bit integer type used by lonejson public structs and APIs. */
typedef lonejson_uint32 lj_uint32;
/** Signed 64-bit integer type used by lonejson public structs and APIs. */
typedef lonejson_int64 lj_int64;
/** Unsigned 64-bit integer type used by lonejson public structs and APIs. */
typedef lonejson_uint64 lj_uint64;
/** Outcome codes returned by lonejson parse, stream, and serialization APIs.
 */
typedef lonejson_status lj_status;
/** Internal field categories understood by `lonejson_field` maps. Most users
 * select these indirectly through the `LONEJSON_FIELD_*` macros. */
typedef lonejson_field_kind lj_field_kind;
/** Backing source kind used by `lonejson_source` outbound field handles. */
typedef lonejson_source_kind lj_source_kind;
/** Backing mode used by `lonejson_json_value` opaque embedded JSON handles. */
typedef lonejson_json_value_kind lj_json_value_kind;
/** JSON value category reported to old-value-dependent rewrite callbacks. */
typedef lonejson_value_type lj_value_type;
/** Controls how a `lonejson_json_value` field receives inbound parsed JSON.
 * Parse is stream-first by default; callers must deliberately choose one of
 * these modes before decoding into a `JSON_VALUE` field.
 */
typedef lonejson_json_value_parse_mode lj_json_value_parse_mode;
/** Visitor callbacks for one arbitrary JSON value. String values and object
 * keys are delivered as decoded UTF-8 in chunks. Number values are delivered as
 * raw token bytes in chunks. Any callback may be `NULL` when the caller does
 * not need that event. Use `lonejson_default_value_visitor()` instead of
 * manual zeroing when you want the empty visitor state. The callback sequence
 * is balanced:
 * `object_begin/object_end`, `array_begin/array_end`, and matching
 * begin/chunk/end triplets for keys, strings, and numbers.
 */
typedef lonejson_value_visitor lj_value_visitor;
/** One decoded path segment in a parser-owned arbitrary JSON value path. */
typedef lonejson_path_segment lj_path_segment;
/** Current normalized location while visiting one arbitrary JSON value. */
typedef lonejson_value_path lj_value_path;
/** Path-aware visitor callbacks for one arbitrary JSON value. */
typedef lonejson_path_value_visitor lj_path_value_visitor;
#ifdef LONEJSON_WITH_JWT
typedef lonejson_auth_provider lj_auth_provider;
typedef lonejson_jws_verify_request lj_jws_verify_request;
#ifdef LONEJSON_WITH_OPENSSL
typedef lonejson_openssl_auth_provider_config lj_openssl_auth_provider_config;
#endif
/** Caller-owned JWT compact-serialization segment. */
typedef lonejson_jwt_segment lj_jwt_segment;
/** Parsed JWT compact serialization. */
typedef lonejson_jwt_compact lj_jwt_compact;
/** Parsed JSON Web Key. */
typedef lonejson_jwk lj_jwk;
/** Parsed JSON Web Key Set. */
typedef lonejson_jwks lj_jwks;
/** Optional JWK selection filters. */
typedef lonejson_jwk_select_options lj_jwk_select_options;
/** Decoded JWT JOSE header fields retained by lonejson. */
typedef lonejson_jwt_header lj_jwt_header;
/** Decoded JWT claims retained by lonejson. */
typedef lonejson_jwt_claims lj_jwt_claims;
/** Explicit trust policy for validating parsed JWT header and claims. */
typedef lonejson_jwt_claim_policy lj_jwt_claim_policy;
#endif
#ifdef LONEJSON_WITH_OIDC
typedef lonejson_http_request lj_http_request;
typedef lonejson_http_response lj_http_response;
typedef lonejson_http_provider lj_http_provider;
typedef lonejson_http_provider_config lj_http_provider_config;
typedef lonejson_oidc_discovery lj_oidc_discovery;
typedef lonejson_oidc_jwks_cache_policy lj_oidc_jwks_cache_policy;
typedef lonejson_oidc_jwks_cache lj_oidc_jwks_cache;
typedef lonejson_oauth2_client_credentials lj_oauth2_client_credentials;
typedef lonejson_oauth2_refresh_token lj_oauth2_refresh_token;
typedef lonejson_oauth2_token_introspection lj_oauth2_token_introspection;
typedef lonejson_oauth2_token_revocation lj_oauth2_token_revocation;
typedef lonejson_oidc_authorization_code_token lj_oidc_authorization_code_token;
typedef lonejson_oauth2_token_response lj_oauth2_token_response;
typedef lonejson_oauth2_token_flow lj_oauth2_token_flow;
typedef lonejson_oauth2_token_flow_state lj_oauth2_token_flow_state;
typedef lonejson_oauth2_token_flow_policy lj_oauth2_token_flow_policy;
typedef lonejson_oauth2_token_flow_result lj_oauth2_token_flow_result;
typedef lonejson_oauth2_introspection_response lj_oauth2_introspection_response;
typedef lonejson_oidc_userinfo_request lj_oidc_userinfo_request;
typedef lonejson_oidc_userinfo_response lj_oidc_userinfo_response;
typedef lonejson_oidc_pkce lj_oidc_pkce;
typedef lonejson_oidc_authorization_request lj_oidc_authorization_request;
typedef lonejson_oidc_authorization_callback lj_oidc_authorization_callback;
typedef lonejson_auth_failure lj_auth_failure;
typedef lonejson_oidc_bearer_validation_request
    lj_oidc_bearer_validation_request;
typedef lonejson_oidc_bearer_validation lj_oidc_bearer_validation;
typedef lonejson_m2m_credential lj_m2m_credential;
typedef lonejson_m2m_credential_request lj_m2m_credential_request;
typedef lonejson_m2m_store lj_m2m_store;
typedef lonejson_m2m_verify_request lj_m2m_verify_request;
typedef lonejson_m2m_authentication lj_m2m_authentication;
typedef lonejson_m2m_signup lj_m2m_signup;
typedef lonejson_m2m_signup_request lj_m2m_signup_request;
typedef lonejson_m2m_signup_complete_request lj_m2m_signup_complete_request;
typedef lonejson_m2m_signup_completion lj_m2m_signup_completion;
#endif
/** Handler invoked while a mapped string-array stream field is decoded.
 * `chunk` receives decoded UTF-8 string bytes and may be called more than once
 * per item. `end` is called only after the string item is complete; the JSON
 * array and enclosing document are still validated before parse completion.
 */
typedef lonejson_array_stream_string_handler lj_array_stream_string_handler;
/** Destination member for `LONEJSON_FIELD_STRING_ARRAY_STREAM*` fields.
 *
 * This is a configured parse-field helper, not a standalone receiver handle.
 * Configure it with `lj_string_array_stream_set_handler()` before parsing;
 * lonejson preserves the configured callbacks across destination clearing and
 * only resets per-parse internal state.
 */
typedef lonejson_string_array_stream lj_string_array_stream;
/** Static initializer for one short-alias mapped string-array stream field
 * helper.
 */
#define LJ_STRING_ARRAY_STREAM_INIT LONEJSON_STRING_ARRAY_STREAM_INIT
/** Callback invoked after one mapped array-stream item has been parsed into
 * the configured reusable item destination. The destination is valid only for
 * the duration of the callback; lonejson cleans and reuses it before parsing
 * the next item.
 */
typedef lonejson_mapped_array_stream_item_fn lj_mapped_array_stream_item_fn;
/** Handler configured on a `lonejson_mapped_array_stream` field before
 * parsing. `item_map` must describe one object item and `item_dst` is reused
 * for each item.
 */
typedef lonejson_mapped_array_stream_handler lj_mapped_array_stream_handler;
/** Destination member for `LONEJSON_FIELD_MAPPED_ARRAY_STREAM*` fields.
 *
 * This streams mapped object items from a JSON array field during normal mapped
 * parsing. Item callbacks are invoked in source JSON order. Sibling fields
 * that appear later in the containing object have not been parsed yet when an
 * earlier streamed array item is delivered. If callbacks need envelope or
 * parent metadata, serialize those fields before the streamed array field or
 * defer that work until the containing parse has completed.
 *
 * The selected array and enclosing document are not materialized. Each item is
 * parsed into the configured reusable destination, delivered to the callback,
 * then cleaned before the next item. Item maps may themselves contain mapped
 * array-stream fields, so nested streamed arrays compose through normal maps.
 */
typedef lonejson_mapped_array_stream lj_mapped_array_stream;
/** Static initializer for one short-alias mapped object-array stream field
 * helper.
 */
#define LJ_MAPPED_ARRAY_STREAM_INIT LONEJSON_MAPPED_ARRAY_STREAM_INIT
/** Storage model used by a mapped field. */
typedef lonejson_storage_kind lj_storage_kind;
/** Behavior used when a fixed-capacity output or mapped field is too small. */
typedef lonejson_overflow_policy lj_overflow_policy;
/** Detailed error information populated by most public APIs. Caller-provided
 * error outputs do not need prior initialization because lonejson overwrites
 * them on entry, but `lonejson_error_init` is available when you want an
 * explicit known-empty state.
 */
typedef lonejson_error lj_error;
/** Optional allocation counters used by custom allocators. lonejson updates
 * these counters only in debug builds. Release builds leave them untouched.
 */
typedef lonejson_allocator_stats lj_allocator_stats;
/** Allocator vtable used by parser, spool, stream, and lonejson-owned output
 * buffers. When all three callbacks are `NULL`, lonejson falls back to
 * `lonejson_default_allocator()`. Partial callback sets are invalid; callers
 * must provide either all callbacks or none. `malloc_fn` and `realloc_fn` must
 * return pointers aligned at least as strictly as standard `malloc`; returning
 * weaker-aligned storage is undefined behavior because lonejson may place
 * internal owned-allocation headers at those addresses. Custom allocators that
 * prepend private headers must over-align the returned payload pointer, for
 * example by using a header union containing `void *`, integer, `double`, and
 * `long double` members.
 */
typedef lonejson_allocator lj_allocator;
#if defined(LONEJSON_INTERNAL_BUILD) || defined(LONEJSON_IMPLEMENTATION)
/** Internal spool threshold layout retained for lonejson's implementation.
 * Public callers configure spool behavior through `lj_config` and named spool
 * classes.
 */
typedef lonejson__spool_options lj_spool_options;
#endif
/** Spill-backed storage used by streamed text and decoded byte fields.
 * Applications typically treat this as an opaque handle and interact through
 * the `lonejson_spooled_*` helpers. */
typedef lonejson_spooled lj_spooled;
/** Serialize-only outbound source used by streamed text/base64 source fields.
 * Unlike `lonejson_spooled`, this handle never buffers payload bytes inside
 * lonejson; it streams directly from a file, fd, or reopenable path when a
 * mapped struct is serialized. */
typedef lonejson_source lj_source;
/** Opaque JSON value handle used by embedded arbitrary JSON fields. Parsing is
 * stream-first: the caller must configure either a parse sink, a parse
 * visitor, or explicit parse capture before decoding inbound JSON into this
 * field. Serialization can source one JSON value from memory, reader
 * callbacks, files, file descriptors, or reopenable paths.
 */
typedef lonejson_json_value lj_json_value;
/** Dynamically sized array of NUL-terminated strings. */
typedef lonejson_string_array lj_string_array;
/** Dynamically sized array of `lonejson_int64` values. */
typedef lonejson_i64_array lj_i64_array;
/** Dynamically sized array of `lonejson_uint64` values. */
typedef lonejson_u64_array lj_u64_array;
/** Dynamically sized array of `double` values. */
typedef lonejson_f64_array lj_f64_array;
/** Dynamically sized array of boolean values. */
typedef lonejson_bool_array lj_bool_array;
/** Dynamically sized array of nested structs described by another map. */
typedef lonejson_object_array lj_object_array;
/** Forward declaration for one mapped JSON field descriptor. */
typedef lonejson_field lj_field;
/** Forward declaration for one schema map describing a C struct. */
typedef lonejson_map lj_map;
/** Borrowed view of a lonejson schema owned by another API layer. */
typedef lonejson_schema_view lj_schema_view;
/** Borrowed view of a mapped record owned by another API layer. */
typedef lonejson_record_view lj_record_view;
/** Streaming JSON writer state.
 *
 * A writer owns JSON syntax for dynamically shaped documents: object and array
 * delimiters, commas, keys, string escaping, scalar emission, and final-state
 * validation. Initialize it with `lonejson_writer_init_sink()` for push/sink
 * output, or use `lonejson_writer_generator_init()` when a pull-style
 * transport should drive the same writer events through
 * `lonejson_generator_read()`.
 */
typedef lonejson_writer lj_writer;
/** Public state for a push-fed arbitrary JSON value inside a writer.
 *
 * If you use the receiver-method surface (`stream.open(...)`), initialize the
 * struct first with `lonejson_writer_value_stream_init()` or
 * `LONEJSON_WRITER_VALUE_STREAM_INIT`. Plain zero-initialization remains valid
 * when you only use the free functions such as
 * `lonejson_writer_value_stream_open(...)`. The `state` pointer is owned by
 * lonejson after a successful open call. Feed chunks with `push()`, validate
 * EOF and return the writer to normal use with `close()`, and release any
 * remaining stream resources with `cleanup()`.
 */
typedef lonejson_writer_value_stream lj_writer_value_stream;
/** Short alias for `LONEJSON_WRITER_VALUE_STREAM_INIT`. */
#define LJ_WRITER_VALUE_STREAM_INIT LONEJSON_WRITER_VALUE_STREAM_INIT
/** Producer callback used by `lonejson_writer_generator_init()`.
 *
 * The callback emits zero or more writer events into `writer`. Return
 * `LONEJSON_STATUS_OK` after the complete JSON document has been emitted. A
 * writer event that returns `LONEJSON_STATUS_OK` has been accepted, even if the
 * generator still needs later reads to drain pending bytes. If a writer event
 * returns `LONEJSON_STATUS_TRUNCATED`, immediately return that status and retry
 * the same event when the producer is called again.
 */
typedef lonejson_writer_producer_fn lj_writer_producer_fn;
/** Callback that emits one replacement JSON value through a writer.
 *
 * The callback must emit exactly one complete JSON value into `writer`. It
 * should use writer events rather than pre-serialized JSON text so lonejson
 * continues to own JSON syntax, escaping, and validation.
 */
typedef lonejson_value_rewrite_emit_fn lj_value_rewrite_emit_fn;
/** Metadata passed to old-value-dependent rewrite callbacks.
 *
 * For `LONEJSON_VALUE_NUMBER`, `number` points to the validated JSON number
 * token and remains valid only for the duration of the callback. For
 * `LONEJSON_VALUE_BOOL`, `boolean` is non-zero for `true`. Other value types
 * are reported by type only; configure `old_value_visitor` in the rewrite
 * options when structured streaming inspection is needed.
 */
typedef lonejson_value_rewrite_old_value lj_value_rewrite_old_value;
/** Callback used by old-value-dependent value rewrites.
 *
 * The callback is invoked at the selected value position after the old value
 * has been streamed to `old_value_visitor`, if one was configured. It must emit
 * exactly one complete replacement JSON value through `writer`, or return an
 * error to reject the rewrite. For absent object paths, `old_value->present` is
 * zero and the callback emits the value for the synthesized final segment.
 */
typedef lonejson_value_rewrite_replace_fn lj_value_rewrite_replace_fn;
/** Opaque object-framed JSON stream parser state. */
typedef lonejson_stream lj_stream;
/** Opaque selected-array item stream parser state. */
typedef lonejson_array_stream lj_array_stream;
/** Callback invoked after one push-fed selected array item has been parsed into
 * `dst`. The push stream cleans up and reuses `dst` after the callback returns,
 * so callers that need to retain an item must copy it here.
 */
typedef lonejson_array_stream_item_fn lj_array_stream_item_fn;
/** Callback invoked after one push-fed selected string-array item has been
 * decoded into a bounded, NUL-terminated temporary buffer. The pointer is valid
 * only for the duration of the callback.
 */
typedef lonejson_array_stream_string_fn lj_array_stream_string_fn;
/** Action returned by a selected-array rewrite item callback. The rewriter owns
 * array framing and comma placement for all actions.
 */
typedef lonejson_array_rewrite_action lj_array_rewrite_action;
/** Operation applied by the generic streaming JSON value rewriter. */
typedef lonejson_value_rewrite_action lj_value_rewrite_action;
/** One outbound item supplied by a selected-array rewrite callback. Exactly one
 * source form must be set: either `map`+`src`, or `json`.
 */
typedef lonejson_array_rewrite_source lj_array_rewrite_source;
/** Replacement value supplied to a generic value rewrite operation.
 *
 * Exactly one source form must be configured. `emit` is the most general form
 * and receives a streaming writer for one replacement JSON value. `map`+`src`
 * and `json` are convenience forms for mapped structs and rewindable arbitrary
 * JSON values.
 */
typedef lonejson_value_rewrite_replacement lj_value_rewrite_replacement;
/** Options for rewriting one arbitrary JSON value by escaped selector string.
 *
 * `selector` uses dot-separated segments (`meta.request.id`). Use backslash to
 * escape literal dots or backslashes inside a segment. An empty selector or
 * `NULL` selects the root value. The selector is parsed for the duration of
 * the call; callers do not need to allocate a segment array.
 */
typedef lonejson_value_rewrite_selector_options
    lj_value_rewrite_selector_options;
/** Mutable result initialized to `KEEP` before each item callback. */
typedef lonejson_array_rewrite_result lj_array_rewrite_result;
/** Bounded parent context for a `[]` selector segment such as `boards[]`.
 * `dst` is reused for one active parent item and is updated in source JSON
 * order as fields are parsed. Fields that appear after the selected child
 * array are not visible to child item callbacks until later.
 */
typedef lonejson_array_rewrite_parent lj_array_rewrite_parent;
/** Context passed to rewrite callbacks. Parent entries are ordered from the
 * root toward the selected array. Pointers remain valid only for the callback.
 */
typedef lonejson_array_rewrite_context lj_array_rewrite_context;
/** Emits one appended selected-array item. */
typedef lonejson_array_rewrite_emit_fn lj_array_rewrite_emit_fn;
/** Called once for each selected array item. If `item_map` is configured in
 * the options, `item` points at the reusable mapped item destination. If
 * `item_value` is configured instead, `item` points at that
 * `lonejson_json_value`.
 */
typedef lonejson_array_rewrite_item_fn lj_array_rewrite_item_fn;
/** Called after all existing items in one selected array have been processed.
 * Use `emit` to append zero or more items; callers never emit array framing or
 * commas directly.
 */
typedef lonejson_array_rewrite_append_fn lj_array_rewrite_append_fn;
/** Options for selected-array streaming rewrite. `selector == NULL` or `""`
 * selects a root array. Object paths use dot-separated keys. Array fanout must
 * be explicit with `[]`, for example `boards[].items`; plain dotted paths do
 * not implicitly fan out through arrays.
 *
 * Output is validated input re-emitted as compact canonical JSON. Unselected
 * parts are streamed through the visitor pipeline rather than materialized.
 *
 * When `item` is configured, configure exactly one item delivery mode:
 * `item_map`+`item_dst`, or `item_value`. Append-only rewrites may omit `item`
 * and item delivery. Replacement, inserted, and appended items can be supplied
 * from mapped structs or rewindable `lonejson_json_value` handles.
 */
typedef lonejson_array_rewrite_options lj_array_rewrite_options;
/** Options for rewriting one arbitrary JSON value by normalized path.
 *
 * `target_segments` names one value in the input tree. Object segments match
 * decoded object member names. Array segments are decimal zero-based indexes.
 * Empty paths select the root value. Missing descendant object chains are
 * synthesized for `REPLACE` operations when all missing segments are object
 * names rather than array indexes.
 */
typedef lonejson_value_rewrite_options lj_value_rewrite_options;
/** Instantiated lonejson runtime. */
typedef lonejson lj;
/** Runtime configuration applied by `lj_new()`. */
typedef lonejson_config lj_config;
/** Opaque incremental Server-Sent Events parser state. */
typedef lonejson_sse lj_sse;
/** Opaque incremental MIME multipart parser state. */
typedef lonejson_multipart lj_multipart;
/** Result returned by reader callbacks and `lonejson_spooled_read`. Use
 * `lonejson_default_read_result()` when constructing one manually inside a
 * reader callback instead of open-coding `memset` or `{0}`.
 */
typedef lonejson_read_result lj_read_result;
/** Reader adapter state for an immutable caller-owned memory buffer.
 *
 * Initialize with `lonejson_buffer_reader_init()` and pass
 * `lonejson_buffer_reader_read` as the reader callback. The adapter never
 * copies or owns `data`; callers must keep the buffer alive until the consumer
 * finishes reading.
 */
typedef lonejson_buffer_reader lj_buffer_reader;
/** Callback type used by reader-backed parse and stream APIs. */
typedef lonejson_reader_fn lj_reader_fn;
/** Generic sink callback used by serializer APIs and raw spool writers. */
typedef lonejson_sink_fn lj_sink_fn;
/** Base64 alphabet and padding policy. */
typedef lonejson_base64_variant lj_base64_variant;
#define LJ_BASE64_STANDARD LONEJSON_BASE64_STANDARD
#define LJ_BASE64_STANDARD_RAW LONEJSON_BASE64_STANDARD_RAW
#define LJ_BASE64_URL LONEJSON_BASE64_URL
#define LJ_BASE64_URL_RAW LONEJSON_BASE64_URL_RAW
/** Header name/value pair exposed while processing multipart part headers.
 * Pointers are valid only for the duration of the callback currently using
 * them.
 */
typedef lonejson_header lj_header;
/** Limits and allocator selection for incremental SSE parsing. Zero limit
 * fields use the library defaults. `lonejson_sse_push` streams `data:` field
 * bytes to the caller as lines arrive; `lonejson_sse_push_json` streams
 * selected `data:` fields into the JSON parser as lines arrive.
 */
typedef lonejson_sse_options lj_sse_options;
/** Metadata for one Server-Sent Event. Field pointers are valid only during
 * the callback currently receiving them. `data_len` is the number of streamed
 * data bytes for the event, including SSE-inserted newlines between multiple
 * `data:` fields.
 */
typedef lonejson_sse_event lj_sse_event;
/** Streaming callbacks for `lonejson_sse_push`. `begin_event` is called before
 * the first data chunk, or at dispatch for metadata-only events; its metadata
 * reflects fields parsed so far. `end_event` receives the final metadata for
 * the dispatched event.
 */
typedef lonejson_sse_handler lj_sse_handler;
/** Controls JSON decoding for SSE events. When `event_names` is non-NULL and
 * `event_name_count` is non-zero, only matching event names are decoded.
 * Events without an explicit SSE `event:` field match the empty string. For
 * filtered streaming, the `event:` field must arrive before the first `data:`
 * field in that event.
 */
typedef lonejson_sse_json_options lj_sse_json_options;
/** Called after a selected SSE event's streamed JSON data has been decoded.
 * `event->data_len` is the number of JSON data bytes streamed to the parser,
 * including SSE-inserted newlines between multiple `data:` fields.
 */
typedef lonejson_sse_json_event_fn lj_sse_json_event_fn;
/** Called after a selected SSE event's streamed JSON data has been decoded
 * into one configured `lj_json_value` handle.
 */
typedef lonejson_sse_json_value_event_fn lj_sse_json_value_event_fn;
/** Limits and allocator selection for incremental multipart parsing. Zero
 * limit fields use the library defaults. Parts with `Content-Length` stream
 * body bytes directly to `part_data` after headers are parsed; parts without
 * `Content-Length` stream body bytes while retaining only bounded boundary
 * lookahead.
 */
typedef lonejson_multipart_options lj_multipart_options;
/** Metadata for the current multipart part. Pointers are valid only for the
 * callback currently using them.
 */
typedef lonejson_multipart_part lj_multipart_part;
/** Streaming callbacks for `lonejson_multipart_push`. Part header metadata is
 * available in `begin_part` before body bytes are delivered. Each callback may
 * return a non-OK status to stop parsing.
 */
typedef lonejson_multipart_handler lj_multipart_handler;
/** Result produced by `lonejson_stream_next`. */
typedef lonejson_stream_result lj_stream_result;
/** Result produced by `lonejson_array_stream_next*`. */
typedef lonejson_array_stream_result lj_array_stream_result;

/** Initializes an error struct to the empty `OK` state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_error_init(lj_error *error) {
  lonejson_error_init(error);
}
/** Returns lonejson's default allocator backed by the configured allocation
 * macros.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_allocator lj_default_allocator(void) {
  return lonejson_default_allocator();
}
/** Returns the empty default read result used by reader callbacks. */
LONEJSON_SHORT_ALIAS_INLINE lj_read_result lj_default_read_result(void) {
  return lonejson_default_read_result();
}
/** Initializes a memory-buffer reader adapter.
 *
 * The adapter reads directly from the caller-owned `data` buffer without
 * copying it. Passing `NULL` with `len == 0` is valid; passing `NULL` with a
 * non-zero length makes later reads report callback failure.
 */
LONEJSON_SHORT_ALIAS_INLINE void
lj_buffer_reader_init(lj_buffer_reader *reader, const void *data, size_t len) {
  lonejson_buffer_reader_init(reader, data, len);
}
/** Reader callback for `lonejson_buffer_reader`.
 *
 * Pass this function with a `lonejson_buffer_reader *` user pointer to
 * reader-backed parse, validate, visit, and rewrite APIs.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_read_result
lj_buffer_reader_read(void *user, unsigned char *buffer, size_t capacity) {
  return lonejson_buffer_reader_read(user, buffer, capacity);
}
/** Returns the empty visitor with all callbacks set to `NULL`. */
LONEJSON_SHORT_ALIAS_INLINE lj_value_visitor lj_default_value_visitor(void) {
  return lonejson_default_value_visitor();
}
/** Returns the empty path-aware visitor with all callbacks set to `NULL`. */
LONEJSON_SHORT_ALIAS_INLINE lj_path_value_visitor
lj_default_path_value_visitor(void) {
  return lonejson_default_path_value_visitor();
}
/** Initializes a mapped string-array stream field with no handler. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_string_array_stream_init(lj_string_array_stream *stream) {
  lonejson_string_array_stream_init(stream);
}
/** Configures callbacks for a mapped string-array stream field. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_string_array_stream_set_handler(
    lj_string_array_stream *stream,
    const lj_array_stream_string_handler *handler, void *user,
    lj_error *error) {
  return lonejson_string_array_stream_set_handler(stream, handler, user, error);
}
/** Initializes a mapped object-array stream field with no handler. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_mapped_array_stream_init(lj_mapped_array_stream *stream) {
  lonejson_mapped_array_stream_init(stream);
}
/** Configures callbacks and reusable item storage for a mapped array-stream
 * field.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_mapped_array_stream_set_handler(
    lj_mapped_array_stream *stream,
    const lj_mapped_array_stream_handler *handler, lj_error *error) {
  return lonejson_mapped_array_stream_set_handler(stream, handler, error);
}
/** Initializes a spool handle with the runtime's default spool policy. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_init(lonejson *runtime,
                                                 lj_spooled *value) {
  lonejson_spooled_init(runtime, value);
}
/** Initializes a writer value stream receiver for method-style use. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_writer_value_stream_init(lj_writer_value_stream *stream) {
  lonejson_writer_value_stream_init(stream);
}
/** Initializes a spool handle with one named runtime spool policy. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_spooled_init_class(lonejson *runtime, lj_spooled *value,
                      lonejson_spool_class spool_class) {
  lonejson_spooled_init_class(runtime, value, spool_class);
}
/** Internal helper used by lonejson's implementation and tests. */
#ifdef LONEJSON_INTERNAL_BUILD
LONEJSON_SHORT_ALIAS_INLINE void
lj_spooled_init_with_allocator(lj_spooled *value,
                               const lj_spool_options *options,
                               const lj_allocator *allocator) {
  lonejson_spooled_init_with_allocator(value, options, allocator);
}
#endif
/** Clears a spool handle while preserving its configured thresholds. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_reset(lj_spooled *value) {
  lonejson_spooled_reset(value);
}
/** Releases all resources owned by a spool handle, including any temporary
 * file. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_cleanup(lj_spooled *value) {
  lonejson_spooled_cleanup(value);
}
/** Returns the logical byte size of a spooled value. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_spooled_size(const lj_spooled *value) {
  return lonejson_spooled_size(value);
}
/** Returns non-zero when a spooled value has spilled beyond memory into a
 * temporary file. */
LONEJSON_SHORT_ALIAS_INLINE int lj_spooled_spilled(const lj_spooled *value) {
  return lonejson_spooled_spilled(value);
}
/** Appends raw bytes to an initialized spooled value, spilling according to
 * the value's configured spool options.
 *
 * `value` must have been initialized with `lonejson_spooled_init()` or
 * `lonejson_spooled_init_with_allocator()`. The call preserves the current
 * read cursor; it does not implicitly rewind. Use `lonejson_spooled_rewind()`
 * before reading from the beginning.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_append(lj_spooled *value,
                                                        const void *data,
                                                        size_t len,
                                                        lj_error *error) {
  return lonejson_spooled_append(value, data, len, error);
}
/** Rewinds a spooled value's raw read cursor to the beginning. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_rewind(lj_spooled *value,
                                                        lj_error *error) {
  return lonejson_spooled_rewind(value, error);
}
/** Reads raw bytes sequentially from a spooled value. */
LONEJSON_SHORT_ALIAS_INLINE lj_read_result
lj_spooled_read(lj_spooled *value, unsigned char *buffer, size_t capacity) {
  return lonejson_spooled_read(value, buffer, capacity);
}
/** Streams the raw bytes of a spooled value to a generic sink callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_write_to_sink(
    const lj_spooled *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_spooled_write_to_sink(value, sink, user, error);
}
/** Initializes an outbound source handle to the empty `null` state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_init(lj_source *value) {
  lonejson_source_init(value);
}
/** Resets an outbound source handle to the empty `null` state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_reset(lj_source *value) {
  lonejson_source_reset(value);
}
/** Releases any path owned by an outbound source handle and resets it. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_cleanup(lj_source *value) {
  lonejson_source_cleanup(value);
}
/** Configures an outbound source handle to read from a caller-owned `FILE *`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_file(lj_source *value,
                                                         FILE *fp,
                                                         lj_error *error) {
  return lonejson_source_set_file(value, fp, error);
}
/** Configures an outbound source handle to read from a caller-owned file
 * descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_fd(lj_source *value, int fd,
                                                       lj_error *error) {
  return lonejson_source_set_fd(value, fd, error);
}
/** Configures an outbound source handle to reopen and stream a filesystem
 * path. lonejson owns the duplicated path string and frees it on cleanup. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_path(lj_source *value,
                                                         const char *path,
                                                         lj_error *error) {
  return lonejson_source_set_path(value, path, error);
}
/** Streams the raw bytes of an outbound source handle to a generic sink. Path
 * sources are opened and closed by lonejson for the duration of the call. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_write_to_sink(
    const lj_source *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_source_write_to_sink(value, sink, user, error);
}
/** Returns non-zero when a source can be reopened or rewound for another
 * serialization pass. */
LONEJSON_SHORT_ALIAS_INLINE int
lj_source_is_rewindable(const lj_source *value) {
  return lonejson_source_is_rewindable(value);
}
/** Initializes an embedded JSON value handle to the empty `null` state with no
 * inbound parse destination configured, using lonejson's default allocator.
 * This is the required starting state before setting parse sink, parse
 * visitor, parse capture, or outbound source configuration.
 */
LONEJSON_SHORT_ALIAS_INLINE void lj_json_value_init(lonejson *runtime,
                                                    lj_json_value *value) {
  lonejson_json_value_init(runtime, value);
}
/** Initializes an embedded JSON value handle with an explicit allocator and no
 * inbound parse destination configured.
 */
#ifdef LONEJSON_INTERNAL_BUILD
LONEJSON_SHORT_ALIAS_INLINE void
lj_json_value_init_with_allocator(lj_json_value *value,
                                  const lj_allocator *allocator) {
  lonejson_json_value_init_with_allocator(value, allocator);
}
#endif
/** Resets an embedded JSON value handle to the empty `null` state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_json_value_reset(lj_json_value *value) {
  lonejson_json_value_reset(value);
}
/** Releases any storage or path owned by an embedded JSON value handle and
 * resets it. */
LONEJSON_SHORT_ALIAS_INLINE void lj_json_value_cleanup(lj_json_value *value) {
  lonejson_json_value_cleanup(value);
}
/** Validates one JSON value from a memory buffer, stores a compact owned copy,
 * and configures the handle to serialize from memory. Use this when retained
 * ownership is intentional; it is not the default parse path for
 * `JSON_VALUE` fields.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_buffer(
    lj_json_value *value, const void *data, size_t len, lj_error *error) {
  return lonejson_json_value_set_buffer(value, data, len, error);
}

/** Configures an embedded JSON value handle to stream inbound parsed JSON
 * bytes to a caller sink as they are validated. Callers using parse sinks must
 * keep the handle configured across parsing, typically by initializing the
 * destination first and parsing with `clear_destination = 0`. This supported
 * reuse pattern also applies when the `JSON_VALUE` field lives inside nested
 * mapped objects or mapped-array-stream item structs.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_parse_sink(
    lj_json_value *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_json_value_set_parse_sink(value, sink, user, error);
}

/** Configures an embedded JSON value handle to deliver inbound parsed JSON as
 * structured visitor callbacks. Callers using parse visitors must keep the
 * handle configured across parsing, typically by initializing the destination
 * first and parsing with `clear_destination = 0`. This supported reuse pattern
 * also applies when the `JSON_VALUE` field lives inside nested mapped objects
 * or mapped-array-stream item structs. Visitor limits come from the runtime
 * that initialized the handle.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_parse_visitor(
    lj_json_value *value, const lj_value_visitor *visitor, void *user,
    lj_error *error) {
  return lonejson_json_value_set_parse_visitor(value, visitor, user, error);
}

/** Configures an embedded JSON value handle to deliver inbound parsed JSON as
 * path-aware structured visitor callbacks.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_parse_path_visitor(
    lj_json_value *value, const lj_path_value_visitor *visitor, void *user,
    lj_error *error) {
  return lonejson_json_value_set_parse_path_visitor(value, visitor, user,
                                                    error);
}

/** Enables explicit parse-time capture of one inbound JSON value into owned
 * compact bytes. This is the opt-in storage path for callers that need to
 * retain a parsed arbitrary JSON value after decoding, including nested
 * `JSON_VALUE` fields reused with `clear_destination = 0`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_json_value_enable_parse_capture(lj_json_value *value, lj_error *error) {
  return lonejson_json_value_enable_parse_capture(value, error);
}
/** Configures an embedded JSON value handle to stream from a caller-provided
 * reader callback at serialize time. Reader-backed values are consumed in one
 * pass per serialization call. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_reader(
    lj_json_value *value, lj_reader_fn reader, void *user, lj_error *error) {
  return lonejson_json_value_set_reader(value, reader, user, error);
}
/** Configures an embedded JSON value handle to read from a caller-owned
 * `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_json_value_set_file(lj_json_value *value, FILE *fp, lj_error *error) {
  return lonejson_json_value_set_file(value, fp, error);
}
/** Configures an embedded JSON value handle to read from a caller-owned file
 * descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_fd(lj_json_value *value,
                                                           int fd,
                                                           lj_error *error) {
  return lonejson_json_value_set_fd(value, fd, error);
}
/** Configures an embedded JSON value handle to reopen and stream a filesystem
 * path. lonejson owns the duplicated path string and frees it on cleanup. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_path(
    lj_json_value *value, const char *path, lj_error *error) {
  return lonejson_json_value_set_path(value, path, error);
}
/** Streams the compact form of one embedded JSON value to a generic sink while
 * validating that the handle contains exactly one JSON value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_write_to_sink(
    const lj_json_value *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_json_value_write_to_sink(value, sink, user, error);
}
/** Returns non-zero when an embedded JSON value can be replayed for another
 * serialization pass. Reader-backed values are not rewindable. */
LONEJSON_SHORT_ALIAS_INLINE int
lj_json_value_is_rewindable(const lj_json_value *value) {
  return lonejson_json_value_is_rewindable(value);
}
/** Owned serializer output buffer returned by alloc-returning APIs.
 *
 * Read `data` and `len`, then release the handle with
 * `lonejson_owned_buffer_free()`. Treat `alloc_size` and `allocator` as
 * internal cleanup state.
 */
typedef lonejson_owned_buffer lj_owned_buffer;
/** Returns an empty owned-buffer handle that uses the default allocator. */
LONEJSON_SHORT_ALIAS_INLINE lj_owned_buffer lj_default_owned_buffer(void) {
  return lonejson_default_owned_buffer();
}
/** Initializes an owned-buffer handle to its empty default state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_owned_buffer_init(lj_owned_buffer *buffer) {
  lonejson_owned_buffer_init(buffer);
}
/** Returns a stable string name for a `lonejson_status` value. */
LONEJSON_SHORT_ALIAS_INLINE const char *lj_status_string(lj_status status) {
  return lonejson_status_string(status);
}
/** Returns the library default runtime configuration. */
LONEJSON_SHORT_ALIAS_INLINE lonejson_config lj_default_config(void) {
  return lonejson_default_config();
}
/** Creates one configured lonejson runtime. Passing `NULL` uses defaults. */
LONEJSON_SHORT_ALIAS_INLINE lonejson *lj_new(const lonejson_config *config,
                                             lj_error *error) {
  return lonejson_new(config, error);
}
/** Releases one lonejson runtime. */
LONEJSON_SHORT_ALIAS_INLINE void lj_free(lonejson *runtime) {
  lonejson_free(runtime);
}
/** Opens an object-framed JSON stream over a caller-supplied reader callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_reader(lonejson *runtime, const lj_map *map, lj_reader_fn reader,
                      void *user, lj_error *error) {
  return lonejson_stream_open_reader(runtime, map, reader, user, error);
}
/** Opens an object-framed JSON stream over an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *lj_stream_open_filep(lonejson *runtime,
                                                            const lj_map *map,
                                                            FILE *fp,
                                                            lj_error *error) {
  return lonejson_stream_open_filep(runtime, map, fp, error);
}
/** Opens an object-framed JSON stream over a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *lj_stream_open_path(lonejson *runtime,
                                                           const lj_map *map,
                                                           const char *path,
                                                           lj_error *error) {
  return lonejson_stream_open_path(runtime, map, path, error);
}
/** Opens an object-framed JSON stream over a file descriptor, including Unix
 * domain sockets. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *lj_stream_open_fd(lonejson *runtime,
                                                         const lj_map *map,
                                                         int fd,
                                                         lj_error *error) {
  return lonejson_stream_open_fd(runtime, map, fd, error);
}
/** Parses the next top-level JSON object from a stream into `dst`. Whitespace
 * between objects is ignored and EOF after the last object is reported
 * separately. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream_result lj_stream_next(lj_stream *stream,
                                                            void *dst,
                                                            lj_error *error) {
  return lonejson_stream_next(stream, dst, error);
}
/** Returns the stream's last error state. */
LONEJSON_SHORT_ALIAS_INLINE const lj_error *
lj_stream_error(const lj_stream *stream) {
  return lonejson_stream_error(stream);
}
/** Closes a stream and releases resources it owns. */
LONEJSON_SHORT_ALIAS_INLINE void lj_stream_close(lj_stream *stream) {
  lonejson_stream_close(stream);
}
/** Opens a streaming cursor over an array selected by a direct root object key.
 * `path == NULL` or `""` selects a root array. Non-empty v1 paths must be one
 * object-key segment such as `boards` or `items`; dotted paths are rejected
 * instead of implicitly fanning out through arrays.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream *
lj_array_stream_open_reader(lonejson *runtime, const char *path,
                            lj_reader_fn reader, void *user, lj_error *error) {
  return lonejson_array_stream_open_reader(runtime, path, reader, user, error);
}
/** Opens an array-item stream over an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream *
lj_array_stream_open_filep(lonejson *runtime, const char *path, FILE *fp,
                           lj_error *error) {
  return lonejson_array_stream_open_filep(runtime, path, fp, error);
}
/** Opens an array-item stream over a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream *
lj_array_stream_open_path(lonejson *runtime, const char *array_path,
                          const char *path, lj_error *error) {
  return lonejson_array_stream_open_path(runtime, array_path, path, error);
}
/** Opens an array-item stream over a file descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream *
lj_array_stream_open_fd(lonejson *runtime, const char *path, int fd,
                        lj_error *error) {
  return lonejson_array_stream_open_fd(runtime, path, fd, error);
}
/** Opens a push-fed array-item stream for write-callback style sources. Feed
 * bytes with `lonejson_array_stream_push` and validate EOF with
 * `lonejson_array_stream_finish`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream *
lj_array_stream_open_push(lonejson *runtime, const char *path,
                          lj_error *error) {
  return lonejson_array_stream_open_push(runtime, path, error);
}
/** Selects caller-owned mapped-record reuse before stream processing. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_set_record_reuse(
    lj_array_stream *stream, int enabled, lj_error *error) {
  return lonejson_array_stream_set_record_reuse(stream, enabled, error);
}
/** Parses the next selected array item into `dst` through `map`. */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream_result lj_array_stream_next(
    lj_array_stream *stream, const lj_map *map, void *dst, lj_error *error) {
  return lonejson_array_stream_next(stream, map, dst, error);
}
/** Captures the next selected array item as a validated compact JSON value. */
LONEJSON_SHORT_ALIAS_INLINE lj_array_stream_result lj_array_stream_next_value(
    lj_array_stream *stream, lj_json_value *value, lj_error *error) {
  return lonejson_array_stream_next_value(stream, value, error);
}
/** Feeds response bytes into a push-fed selected-array stream. Each complete
 * item is parsed directly into `dst` through `map`, then reported to
 * `callback`. This function consumes bounded chunks; it does not materialize
 * the complete response or selected array.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_push(
    lj_array_stream *stream, const lj_map *map, void *dst, const void *bytes,
    size_t len, lj_array_stream_item_fn callback, void *user, lj_error *error) {
  return lonejson_array_stream_push(stream, map, dst, bytes, len, callback,
                                    user, error);
}
/** Finalizes a push-fed selected-array stream after the source reaches EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_finish(
    lj_array_stream *stream, const lj_map *map, void *dst,
    lj_array_stream_item_fn callback, void *user, lj_error *error) {
  return lonejson_array_stream_finish(stream, map, dst, callback, user, error);
}
/** Feeds bytes into a push-fed selected string-array stream. String item bytes
 * are decoded and delivered incrementally through `handler`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_push_string(
    lj_array_stream *stream, const void *bytes, size_t len,
    const lj_array_stream_string_handler *handler, void *user,
    lj_error *error) {
  return lonejson_array_stream_push_string(stream, bytes, len, handler, user,
                                           error);
}
/** Finalizes a push-fed selected string-array stream after source EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_finish_string(
    lj_array_stream *stream, const lj_array_stream_string_handler *handler,
    void *user, lj_error *error) {
  return lonejson_array_stream_finish_string(stream, handler, user, error);
}
/** Feeds bytes into a push-fed selected string-array stream and invokes
 * `callback` once per decoded string item. Each item is assembled through the
 * chunked string path into bounded temporary storage; the complete response and
 * selected array are never materialized.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_push_string_items(
    lj_array_stream *stream, const void *bytes, size_t len,
    lj_array_stream_string_fn callback, void *user, lj_error *error) {
  return lonejson_array_stream_push_string_items(stream, bytes, len, callback,
                                                 user, error);
}
/** Finalizes a push-fed selected string-array item stream after source EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_stream_finish_string_items(
    lj_array_stream *stream, lj_array_stream_string_fn callback, void *user,
    lj_error *error) {
  return lonejson_array_stream_finish_string_items(stream, callback, user,
                                                   error);
}
/** Returns the cursor's last error state. */
LONEJSON_SHORT_ALIAS_INLINE const lj_error *
lj_array_stream_error(const lj_array_stream *stream) {
  return lonejson_array_stream_error(stream);
}
/** Closes an array-item stream and releases resources it owns. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_array_stream_close(lj_array_stream *stream) {
  lonejson_array_stream_close(stream);
}
/** Rewrites one selected JSON array from a caller-provided reader to a generic
 * sink. The document is validated and emitted incrementally; the complete
 * document and selected arrays are never materialized.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_reader(
    lj *runtime, const char *selector, lj_reader_fn reader, void *reader_user,
    lj_sink_fn sink, void *sink_user, const lj_array_rewrite_options *options,
    lj_error *error) {
  return lonejson_array_rewrite_reader(runtime, selector, reader, reader_user,
                                       sink, sink_user, options, error);
}
/** Rewrites one selected array from a caller-provided reader to an open
 * `FILE *` output.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_reader_to_filep(
    lj *runtime, const char *selector, lj_reader_fn reader, void *reader_user,
    FILE *output, const lj_array_rewrite_options *options, lj_error *error) {
  return lonejson_array_rewrite_reader_to_filep(
      runtime, selector, reader, reader_user, output, options, error);
}
/** Rewrites one selected array from a caller-provided reader to a file
 * descriptor output.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_reader_to_fd(
    lj *runtime, const char *selector, lj_reader_fn reader, void *reader_user,
    int fd, const lj_array_rewrite_options *options, lj_error *error) {
  return lonejson_array_rewrite_reader_to_fd(runtime, selector, reader,
                                             reader_user, fd, options, error);
}
/** Rewrites one selected array from an open `FILE *` to a generic sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_filep(
    lj *runtime, const char *selector, FILE *fp, lj_sink_fn sink,
    void *sink_user, const lj_array_rewrite_options *options, lj_error *error) {
  return lonejson_array_rewrite_filep(runtime, selector, fp, sink, sink_user,
                                      options, error);
}
/** Rewrites one selected array from an open `FILE *` to an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_filep_to_filep(
    lj *runtime, const char *selector, FILE *input, FILE *output,
    const lj_array_rewrite_options *options, lj_error *error) {
  return lonejson_array_rewrite_filep_to_filep(runtime, selector, input, output,
                                               options, error);
}
/** Rewrites one selected array from a file descriptor to a generic sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_fd(
    lj *runtime, const char *selector, int fd, lj_sink_fn sink, void *sink_user,
    const lj_array_rewrite_options *options, lj_error *error) {
  return lonejson_array_rewrite_fd(runtime, selector, fd, sink, sink_user,
                                   options, error);
}
/** Rewrites one selected array from a file descriptor to a file descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_fd_to_fd(
    lj *runtime, const char *selector, int input_fd, int output_fd,
    const lj_array_rewrite_options *options, lj_error *error) {
  return lonejson_array_rewrite_fd_to_fd(runtime, selector, input_fd, output_fd,
                                         options, error);
}
/** Rewrites one selected array from an input path to an output path. Atomic
 * rename and transaction boundaries remain caller responsibility.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_array_rewrite_path(
    lj *runtime, const char *selector, const char *input_path,
    const char *output_path, const lj_array_rewrite_options *options,
    lj_error *error) {
  return lonejson_array_rewrite_path(runtime, selector, input_path, output_path,
                                     options, error);
}
/** Rewrites one arbitrary JSON value from a reader to a sink.
 *
 * The input is validated and re-emitted as compact canonical JSON. The complete
 * input and output documents are not materialized. Replacement values are
 * emitted through lonejson serializers, so callers do not write JSON syntax.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_reader(
    lj *runtime, lj_reader_fn reader, void *reader_user, lj_sink_fn sink,
    void *sink_user, const lj_value_rewrite_options *options, lj_error *error) {
  return lonejson_value_rewrite_reader(runtime, reader, reader_user, sink,
                                       sink_user, options, error);
}
/** Rewrites one arbitrary JSON value from a caller-owned memory buffer to a
 * sink. This is a reader adapter convenience; input and output are still
 * processed incrementally and the full document is not materialized.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_buffer(
    lj *runtime, const void *data, size_t len, lj_sink_fn sink, void *sink_user,
    const lj_value_rewrite_options *options, lj_error *error) {
  return lonejson_value_rewrite_buffer(runtime, data, len, sink, sink_user,
                                       options, error);
}
/** Rewrites one arbitrary JSON value from an open `FILE *` to a sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_filep(
    lj *runtime, FILE *fp, lj_sink_fn sink, void *sink_user,
    const lj_value_rewrite_options *options, lj_error *error) {
  return lonejson_value_rewrite_filep(runtime, fp, sink, sink_user, options,
                                      error);
}
/** Rewrites one arbitrary JSON value from a file descriptor to a sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_value_rewrite_fd(lj *runtime, int fd, lj_sink_fn sink, void *sink_user,
                    const lj_value_rewrite_options *options, lj_error *error) {
  return lonejson_value_rewrite_fd(runtime, fd, sink, sink_user, options,
                                   error);
}
/** Rewrites one arbitrary JSON value from an input path to an output path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_path(
    lj *runtime, const char *input_path, const char *output_path,
    const lj_value_rewrite_options *options, lj_error *error) {
  return lonejson_value_rewrite_path(runtime, input_path, output_path, options,
                                     error);
}
/** Rewrites one arbitrary JSON value using an escaped selector string. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_selector_reader(
    lj *runtime, lj_reader_fn reader, void *reader_user, lj_sink_fn sink,
    void *sink_user, const lj_value_rewrite_selector_options *options,
    lj_error *error) {
  return lonejson_value_rewrite_selector_reader(
      runtime, reader, reader_user, sink, sink_user, options, error);
}
/** Rewrites one arbitrary JSON value from a caller-owned memory buffer using
 * an escaped selector string.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_selector_buffer(
    lj *runtime, const void *data, size_t len, lj_sink_fn sink, void *sink_user,
    const lj_value_rewrite_selector_options *options, lj_error *error) {
  return lonejson_value_rewrite_selector_buffer(runtime, data, len, sink,
                                                sink_user, options, error);
}
/** Rewrites from an open `FILE *` using an escaped selector string. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_selector_filep(
    lj *runtime, FILE *fp, lj_sink_fn sink, void *sink_user,
    const lj_value_rewrite_selector_options *options, lj_error *error) {
  return lonejson_value_rewrite_selector_filep(runtime, fp, sink, sink_user,
                                               options, error);
}
/** Rewrites from a file descriptor using an escaped selector string. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_selector_fd(
    lj *runtime, int fd, lj_sink_fn sink, void *sink_user,
    const lj_value_rewrite_selector_options *options, lj_error *error) {
  return lonejson_value_rewrite_selector_fd(runtime, fd, sink, sink_user,
                                            options, error);
}
/** Rewrites from an input path to an output path using an escaped selector. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_value_rewrite_selector_path(
    lj *runtime, const char *input_path, const char *output_path,
    const lj_value_rewrite_selector_options *options, lj_error *error) {
  return lonejson_value_rewrite_selector_path(runtime, input_path, output_path,
                                              options, error);
}
/** Returns the default SSE parser options. */
LONEJSON_SHORT_ALIAS_INLINE lj_sse_options lj_default_sse_options(void) {
  return lonejson_default_sse_options();
}
/** Opens an incremental Server-Sent Events parser. */
LONEJSON_SHORT_ALIAS_INLINE lj_sse *lj_sse_open(const lj_sse_options *options,
                                                lj_error *error) {
  return lonejson_sse_open(options, error);
}
/** Pushes arbitrary SSE bytes and streams zero or more event payload chunks. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_sse_push(lj_sse *sse,
                                                  const void *bytes, size_t len,
                                                  const lj_sse_handler *handler,
                                                  void *user, lj_error *error) {
  return lonejson_sse_push(sse, bytes, len, handler, user, error);
}
/** Finishes an SSE stream, dispatching a trailing unterminated event when one
 * is in progress.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_sse_finish(
    lj_sse *sse, const lj_sse_handler *handler, void *user, lj_error *error) {
  return lonejson_sse_finish(sse, handler, user, error);
}
/** Pushes SSE bytes, decodes selected event data as JSON, and calls `event_cb`
 * after each decoded JSON event.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_sse_push_json(
    lj *runtime, lj_sse *sse, const lj_map *map, void *dst, const void *bytes,
    size_t len, const lj_sse_json_options *options,
    lj_sse_json_event_fn event_cb, void *user, lj_error *error) {
  return lonejson_sse_push_json(runtime, sse, map, dst, bytes, len, options,
                                event_cb, user, error);
}
/** Finishes an SSE JSON stream. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_sse_finish_json(lj *runtime, lj_sse *sse, const lj_map *map, void *dst,
                   const lj_sse_json_options *options,
                   lj_sse_json_event_fn event_cb, void *user, lj_error *error) {
  return lonejson_sse_finish_json(runtime, sse, map, dst, options, event_cb,
                                  user, error);
}
/** Pushes SSE bytes, decodes selected event data as one arbitrary JSON value,
 * and calls `event_cb` after each decoded JSON event. Prepare `value` with
 * `lj_json_value_set_parse_sink()`, `lj_json_value_set_parse_visitor()`, or
 * `lj_json_value_enable_parse_capture()` before starting the stream.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_sse_push_json_value(
    lj *runtime, lj_sse *sse, lj_json_value *value, const void *bytes,
    size_t len, const lj_sse_json_options *options,
    lj_sse_json_value_event_fn event_cb, void *user, lj_error *error) {
  return lonejson_sse_push_json_value(runtime, sse, value, bytes, len, options,
                                      event_cb, user, error);
}
/** Finishes an SSE JSON-value stream. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_sse_finish_json_value(
    lj *runtime, lj_sse *sse, lj_json_value *value,
    const lj_sse_json_options *options, lj_sse_json_value_event_fn event_cb,
    void *user, lj_error *error) {
  return lonejson_sse_finish_json_value(runtime, sse, value, options, event_cb,
                                        user, error);
}
/** Releases an SSE parser. */
LONEJSON_SHORT_ALIAS_INLINE void lj_sse_close(lj_sse *sse) {
  lonejson_sse_close(sse);
}
/** Returns the default multipart parser options. */
LONEJSON_SHORT_ALIAS_INLINE lj_multipart_options
lj_default_multipart_options(void) {
  return lonejson_default_multipart_options();
}
/** Opens an incremental MIME multipart parser from a Content-Type value that
 * contains a `boundary=` parameter.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_multipart *
lj_multipart_open(const char *content_type, const lj_multipart_options *options,
                  lj_error *error) {
  return lonejson_multipart_open(content_type, options, error);
}
/** Pushes arbitrary multipart bytes and emits part lifecycle callbacks. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_multipart_push(
    lj_multipart *multipart, const void *bytes, size_t len,
    const lj_multipart_handler *handler, void *user, lj_error *error) {
  return lonejson_multipart_push(multipart, bytes, len, handler, user, error);
}
/** Finishes a multipart stream and fails if the closing boundary was not
 * observed.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_multipart_finish(
    lj_multipart *multipart, const lj_multipart_handler *handler, void *user,
    lj_error *error) {
  return lonejson_multipart_finish(multipart, handler, user, error);
}
/** Releases a multipart parser. */
LONEJSON_SHORT_ALIAS_INLINE void lj_multipart_close(lj_multipart *multipart) {
  lonejson_multipart_close(multipart);
}
/** Parses a JSON buffer into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_buffer(lonejson *runtime, const lj_map *map, void *dst,
                const void *data, size_t len, lj_error *error) {
  return lonejson_parse_buffer(runtime, map, dst, data, len, error);
}
/** Parses a NUL-terminated JSON string into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_parse_cstr(lonejson *runtime,
                                                    const lj_map *map,
                                                    void *dst, const char *json,
                                                    lj_error *error) {
  return lonejson_parse_cstr(runtime, map, dst, json, error);
}
/** Parses JSON from a caller-supplied reader callback into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_reader(lonejson *runtime, const lj_map *map, void *dst,
                lj_reader_fn reader, void *user, lj_error *error) {
  return lonejson_parse_reader(runtime, map, dst, reader, user, error);
}
/** Parses JSON from an open `FILE *` stream into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_parse_filep(lonejson *runtime,
                                                     const lj_map *map,
                                                     void *dst, FILE *fp,
                                                     lj_error *error) {
  return lonejson_parse_filep(runtime, map, dst, fp, error);
}
/** Parses JSON from a filesystem path into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_parse_path(lonejson *runtime,
                                                    const lj_map *map,
                                                    void *dst, const char *path,
                                                    lj_error *error) {
  return lonejson_parse_path(runtime, map, dst, path, error);
}
/** Validates that a buffer contains a syntactically valid JSON document. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_buffer(lonejson *runtime,
                                                         const void *data,
                                                         size_t len,
                                                         lj_error *error) {
  return lonejson_validate_buffer(runtime, data, len, error);
}
/** Validates that a NUL-terminated string contains syntactically valid JSON. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_cstr(lonejson *runtime,
                                                       const char *json,
                                                       lj_error *error) {
  return lonejson_validate_cstr(runtime, json, error);
}
/** Validates JSON produced by a caller-supplied reader callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_reader(lonejson *runtime,
                                                         lj_reader_fn reader,
                                                         void *user,
                                                         lj_error *error) {
  return lonejson_validate_reader(runtime, reader, user, error);
}
/** Validates JSON from an open `FILE *` stream. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_filep(lonejson *runtime,
                                                        FILE *fp,
                                                        lj_error *error) {
  return lonejson_validate_filep(runtime, fp, error);
}
/** Validates JSON from a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_path(lonejson *runtime,
                                                       const char *path,
                                                       lj_error *error) {
  return lonejson_validate_path(runtime, path, error);
}
/** Visits exactly one JSON value from a caller-provided buffer. The call
 * fails on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lj_value_visitor *visitor, void *user, lj_error *error) {
  return lonejson_visit_value_buffer(runtime, data, len, visitor, user, error);
}
/** Visits exactly one JSON value from a NUL-terminated string. The call fails
 * on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_cstr(
    lonejson *runtime, const char *json, const lj_value_visitor *visitor,
    void *user, lj_error *error) {
  return lonejson_visit_value_cstr(runtime, json, visitor, user, error);
}
/** Visits exactly one JSON value from a caller-provided reader callback. The
 * call fails on malformed JSON or on trailing non-whitespace bytes after the
 * first complete value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_reader(
    lonejson *runtime, lj_reader_fn reader, void *reader_user,
    const lj_value_visitor *visitor, void *user, lj_error *error) {
  return lonejson_visit_value_reader(runtime, reader, reader_user, visitor,
                                     user, error);
}
/** Visits exactly one JSON value from an open `FILE *`. The call fails on
 * malformed JSON or on trailing non-whitespace bytes after the first complete
 * value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_filep(
    lonejson *runtime, FILE *fp, const lj_value_visitor *visitor, void *user,
    lj_error *error) {
  return lonejson_visit_value_filep(runtime, fp, visitor, user, error);
}
/** Visits exactly one JSON value from a filesystem path. The call fails on
 * malformed JSON or on trailing non-whitespace bytes after the first complete
 * value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_path(
    lonejson *runtime, const char *path, const lj_value_visitor *visitor,
    void *user, lj_error *error) {
  return lonejson_visit_value_path(runtime, path, visitor, user, error);
}
/** Visits exactly one JSON value from a file descriptor, including Unix domain
 * sockets. The call fails on malformed JSON or on trailing non-whitespace
 * bytes after the first complete value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_visit_value_fd(lonejson *runtime, int fd, const lj_value_visitor *visitor,
                  void *user, lj_error *error) {
  return lonejson_visit_value_fd(runtime, fd, visitor, user, error);
}
/** Visits exactly one JSON value from a caller-provided buffer and supplies the
 * normalized current path with every path-aware visitor callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_path_value_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lj_path_value_visitor *visitor, void *user, lj_error *error) {
  return lonejson_visit_path_value_buffer(runtime, data, len, visitor, user,
                                          error);
}
/** Visits exactly one JSON value from a NUL-terminated string and supplies the
 * normalized current path with every path-aware visitor callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_path_value_cstr(
    lonejson *runtime, const char *json, const lj_path_value_visitor *visitor,
    void *user, lj_error *error) {
  return lonejson_visit_path_value_cstr(runtime, json, visitor, user, error);
}
/** Visits exactly one JSON value from a caller-provided reader callback and
 * supplies the normalized current path with every path-aware visitor callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_path_value_reader(
    lonejson *runtime, lj_reader_fn reader, void *reader_user,
    const lj_path_value_visitor *visitor, void *user, lj_error *error) {
  return lonejson_visit_path_value_reader(runtime, reader, reader_user, visitor,
                                          user, error);
}
/** Visits exactly one JSON value from an open `FILE *` and supplies the
 * normalized current path with every path-aware visitor callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_path_value_filep(
    lonejson *runtime, FILE *fp, const lj_path_value_visitor *visitor,
    void *user, lj_error *error) {
  return lonejson_visit_path_value_filep(runtime, fp, visitor, user, error);
}
/** Visits exactly one JSON value from a filesystem path and supplies the
 * normalized current path with every path-aware visitor callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_path_value_path(
    lonejson *runtime, const char *path, const lj_path_value_visitor *visitor,
    void *user, lj_error *error) {
  return lonejson_visit_path_value_path(runtime, path, visitor, user, error);
}
/** Visits exactly one JSON value from a file descriptor and supplies the
 * normalized current path with every path-aware visitor callback.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_path_value_fd(
    lonejson *runtime, int fd, const lj_path_value_visitor *visitor, void *user,
    lj_error *error) {
  return lonejson_visit_path_value_fd(runtime, fd, visitor, user, error);
}
/** Pull-style JSON generator state. */
typedef lonejson_generator lj_generator;
/** Serializes a mapped struct to a generic output sink callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_sink(lonejson *runtime, const lj_map *map, const void *src,
                  lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_serialize_sink(runtime, map, src, sink, user, error);
}
/** Initializes a pull-style JSON generator for one mapped struct.
 *
 * The generator stays bounded and never materializes the whole payload. Drain
 * it incrementally through `lonejson_generator_read()` and release it with
 * `lonejson_generator_cleanup()`. It supports the normal mapped serializer
 * surface plus `lonejson_source`, `lonejson_spooled`, and `lonejson_json_value`
 * fields without silently buffering them into one giant payload. The init call
 * fully establishes the generator state; callers do not need to pre-zero the
 * struct.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_generator_init(lonejson *runtime,
                                                        lj_generator *generator,
                                                        const lj_map *map,
                                                        const void *src) {
  return lonejson_generator_init(runtime, generator, map, src);
}
/** Measures the serialized byte length of one mapped struct without retaining
 * the generated JSON.
 *
 * The document must be replayable: path, buffer, spool, and seekable file or
 * file-descriptor sources are accepted, while reader-backed JSON values are
 * rejected because measuring would consume them.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_generator_measure(lonejson *runtime,
                                                           const lj_map *map,
                                                           const void *src,
                                                           size_t *out_len,
                                                           lj_error *error) {
  return lonejson_generator_measure(runtime, map, src, out_len, error);
}
/** Pulls the next serialized JSON bytes from a generator.
 *
 * `out_len` receives the produced byte count and `out_eof` becomes non-zero
 * once the generator has fully drained.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_generator_read(lj_generator *generator,
                                                        unsigned char *buffer,
                                                        size_t capacity,
                                                        size_t *out_len,
                                                        int *out_eof) {
  return lonejson_generator_read(generator, buffer, capacity, out_len, out_eof);
}
/** Releases resources owned by a pull-style JSON generator. */
LONEJSON_SHORT_ALIAS_INLINE void lj_generator_cleanup(lj_generator *generator) {
  lonejson_generator_cleanup(generator);
}
/** Initializes a streaming JSON writer that emits directly to a sink.
 *
 * The writer owns JSON structure and escaping but does not own the sink. The
 * output remains streaming: bytes are forwarded to `sink` as writer events are
 * accepted, without materializing the full document.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_init_sink(lonejson *runtime,
                                                          lj_writer *writer,
                                                          lj_sink_fn sink,
                                                          void *sink_user,
                                                          lj_error *error) {
  return lonejson_writer_init_sink(runtime, writer, sink, sink_user, error);
}
/** Releases resources owned by an initialized streaming writer. */
LONEJSON_SHORT_ALIAS_INLINE void lj_writer_cleanup(lj_writer *writer) {
  lonejson_writer_cleanup(writer);
}
/** Begins a JSON object value at the current writer position. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_begin_object(lj_writer *writer,
                                                             lj_error *error) {
  return lonejson_writer_begin_object(writer, error);
}
/** Ends the current JSON object. The call fails if a key has no value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_end_object(lj_writer *writer,
                                                           lj_error *error) {
  return lonejson_writer_end_object(writer, error);
}
/** Begins a JSON array value at the current writer position. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_begin_array(lj_writer *writer,
                                                            lj_error *error) {
  return lonejson_writer_begin_array(writer, error);
}
/** Ends the current JSON array. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_end_array(lj_writer *writer,
                                                          lj_error *error) {
  return lonejson_writer_end_array(writer, error);
}
/** Emits an object member key for the current object.
 *
 * `key` may contain arbitrary UTF-8 bytes except embedded NULs that the caller
 * intentionally includes in `key_len`; lonejson handles required JSON string
 * escaping. A value event must follow before another key or object end.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_key(lj_writer *writer,
                                                    const char *key,
                                                    size_t key_len,
                                                    lj_error *error) {
  return lonejson_writer_key(writer, key, key_len, error);
}
/** Emits one JSON string value from a caller buffer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_string(lj_writer *writer,
                                                       const void *data,
                                                       size_t len,
                                                       lj_error *error) {
  return lonejson_writer_string(writer, data, len, error);
}
/** Begins a JSON string value for chunked emission.
 *
 * After this succeeds, emit zero or more raw text chunks with
 * `lonejson_writer_string_chunk()` and close the value with
 * `lonejson_writer_string_end()`. This is the low-level form used when the
 * producer already has a streaming string source and must not materialize the
 * complete string.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_string_begin(lj_writer *writer,
                                                             lj_error *error) {
  return lonejson_writer_string_begin(writer, error);
}
/** Emits one raw text chunk inside an open chunked JSON string value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_string_chunk(lj_writer *writer,
                                                             const void *data,
                                                             size_t len,
                                                             lj_error *error) {
  return lonejson_writer_string_chunk(writer, data, len, error);
}
/** Ends a JSON string value opened by `lonejson_writer_string_begin()`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_string_end(lj_writer *writer,
                                                           lj_error *error) {
  return lonejson_writer_string_end(writer, error);
}
/** Emits one JSON string value by streaming raw text bytes from a reader.
 *
 * When used from a writer generator, `LONEJSON_STATUS_TRUNCATED` means output
 * backpressure paused the active string before another reader chunk was
 * consumed. Retry this function with the same `reader` and `reader_user` to
 * resume the same string value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_string_reader(lj_writer *writer, lj_reader_fn reader,
                        void *reader_user, lj_error *error) {
  return lonejson_writer_string_reader(writer, reader, reader_user, error);
}
/** Emits one JSON string value from a rewindable spooled byte container. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_string_spooled(
    lj_writer *writer, const lj_spooled *value, lj_error *error) {
  return lonejson_writer_string_spooled(writer, value, error);
}
/** Emits one JSON string value from a rewindable outbound source handle. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_source_text(
    lj_writer *writer, const lj_source *value, lj_error *error) {
  return lonejson_writer_source_text(writer, value, error);
}
/** Emits one JSON string value containing base64-encoded spooled bytes. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_spooled_base64(
    lj_writer *writer, const lj_spooled *value, lj_error *error) {
  return lonejson_writer_spooled_base64(writer, value, error);
}
/** Emits one JSON string value containing base64-encoded outbound source
 * bytes.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_source_base64(
    lj_writer *writer, const lj_source *value, lj_error *error) {
  return lonejson_writer_source_base64(writer, value, error);
}
/** Emits one already-validated JSON number token.
 *
 * The token is validated by lonejson before emission. Non-number JSON text and
 * number text with trailing bytes are rejected.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_number_text(lj_writer *writer,
                                                            const char *data,
                                                            size_t len,
                                                            lj_error *error) {
  return lonejson_writer_number_text(writer, data, len, error);
}
/** Begins a JSON number value for chunked emission. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_number_begin(lj_writer *writer,
                                                             lj_error *error) {
  return lonejson_writer_number_begin(writer, error);
}
/** Appends one raw number token chunk inside an open chunked JSON number. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_number_chunk(lj_writer *writer,
                                                             const char *data,
                                                             size_t len,
                                                             lj_error *error) {
  return lonejson_writer_number_chunk(writer, data, len, error);
}
/** Ends and validates a chunked JSON number value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_number_end(lj_writer *writer,
                                                           lj_error *error) {
  return lonejson_writer_number_end(writer, error);
}
/** Emits one signed 64-bit JSON integer value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_i64(lj_writer *writer,
                                                    lonejson_int64 value,
                                                    lj_error *error) {
  return lonejson_writer_i64(writer, value, error);
}
/** Emits one unsigned 64-bit JSON integer value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_u64(lj_writer *writer,
                                                    lonejson_uint64 value,
                                                    lj_error *error) {
  return lonejson_writer_u64(writer, value, error);
}
/** Emits one finite double-precision JSON number value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_f64(lj_writer *writer,
                                                    double value,
                                                    lj_error *error) {
  return lonejson_writer_f64(writer, value, error);
}
/** Emits one JSON boolean value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_bool(lj_writer *writer,
                                                     int value,
                                                     lj_error *error) {
  return lonejson_writer_bool(writer, value, error);
}
/** Emits one JSON null value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_null(lj_writer *writer,
                                                     lj_error *error) {
  return lonejson_writer_null(writer, error);
}
/** Emits one arbitrary JSON value from a `lonejson_json_value` handle. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_json_value(
    lj_writer *writer, const lj_json_value *value, lj_error *error) {
  return lonejson_writer_json_value(writer, value, error);
}
/** Opens one push-fed arbitrary JSON value at the current writer position.
 *
 * The writer owns the surrounding root/object/array framing and emits any
 * required comma or object-key transition before accepting chunks for the
 * nested value. While the stream is open, unrelated writer events on the same
 * writer fail. This API is true streaming in sink mode and rejects writer
 * generator mode instead of buffering a full pushed value. Depth, string, and
 * total-byte ceilings come from the runtime configured on the writer.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_value_stream_open(
    lj_writer_value_stream *stream, lj_writer *writer, lj_error *error) {
  return lonejson_writer_value_stream_open(stream, writer, error);
}
/** Feeds one byte chunk into an open push-fed writer value stream.
 *
 * Chunks may split anywhere inside the nested JSON value. lonejson validates
 * incrementally and forwards compact JSON bytes to the writer sink as parser
 * events are accepted. Once the nested value is complete, later chunks may
 * contain only JSON whitespace until close.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_value_stream_push(lj_writer_value_stream *stream, const void *data,
                            size_t len, lj_error *error) {
  return lonejson_writer_value_stream_push(stream, data, len, error);
}
/** Finalizes an open push-fed writer value stream at source EOF.
 *
 * The call fails for empty input, incomplete values, malformed values, limit
 * violations, trailing non-whitespace bytes, or sink failures. Any failure
 * poisons the owning writer so `lonejson_writer_finish()` cannot report
 * success for a partial document.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_value_stream_close(lj_writer_value_stream *stream, lj_error *error) {
  return lonejson_writer_value_stream_close(stream, error);
}
/** Releases resources owned by a writer value stream. If the stream is still
 * open, the owning writer is poisoned because the nested JSON value was not
 * finalized.
 */
LONEJSON_SHORT_ALIAS_INLINE void
lj_writer_value_stream_cleanup(lj_writer_value_stream *stream) {
  lonejson_writer_value_stream_cleanup(stream);
}
/** Streams exactly one arbitrary JSON value from a reader callback.
 *
 * The writer owns the surrounding root/object/array framing. The helper
 * validates incrementally, allows trailing JSON whitespace, rejects empty
 * input, multiple top-level values, malformed JSON, limit violations, reader
 * failure, and trailing non-whitespace, and poisons the writer on failure so
 * `lonejson_writer_finish()` cannot succeed for partial output. This is a
 * sink-mode convenience over `lonejson_writer_value_stream_*`; writer
 * generator producers must use a stable `lonejson_json_value` with
 * `lonejson_writer_json_value()` or a resumable writer event.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_json_value_reader(lj_writer *writer, lj_reader_fn reader,
                            void *reader_user, lj_error *error) {
  return lonejson_writer_json_value_reader(writer, reader, reader_user, error);
}
/** Streams exactly one arbitrary JSON value from a memory buffer.
 *
 * The buffer is not retained after the call. Behavior and failure semantics
 * match `lonejson_writer_json_value_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_json_value_buffer(
    lj_writer *writer, const void *data, size_t len, lj_error *error) {
  return lonejson_writer_json_value_buffer(writer, data, len, error);
}
/** Streams exactly one arbitrary JSON value from the current `FILE *`
 * position. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_json_value_file(lj_writer *writer, FILE *fp, lj_error *error) {
  return lonejson_writer_json_value_file(writer, fp, error);
}
/** Streams exactly one arbitrary JSON value from the current file-descriptor
 * position. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_json_value_fd(lj_writer *writer,
                                                              int fd,
                                                              lj_error *error) {
  return lonejson_writer_json_value_fd(writer, fd, error);
}
/** Opens `path`, streams exactly one arbitrary JSON value, and closes the
 * file before returning. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_json_value_path(
    lj_writer *writer, const char *path, lj_error *error) {
  return lonejson_writer_json_value_path(writer, path, error);
}
/** Streams exactly one arbitrary JSON value from a rewindable spooled byte
 * container. The spooled value is copied as a read cursor, so the caller's
 * cursor is preserved. Behavior and failure semantics match
 * `lonejson_writer_json_value_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_json_value_spooled(
    lj_writer *writer, const lj_spooled *value, lj_error *error) {
  return lonejson_writer_json_value_spooled(writer, value, error);
}
/** Appends all items from a selected source array into the writer's current
 * array.
 *
 * `selector == NULL` or `""` selects a root array. Non-empty selectors follow
 * `lonejson_array_stream` v1 behavior and select one direct root-object key.
 * The writer owns all output commas and array framing; callers must already be
 * inside a writer array. Source item bytes are validated and streamed directly
 * into the writer without materializing the complete source document, selected
 * array, or copied items. This helper is sink-mode only and fails cleanly for
 * writer-generator producers.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_array_items_reader(
    lj_writer *writer, const char *selector, lj_reader_fn reader,
    void *reader_user, lj_error *error) {
  return lonejson_writer_array_items_reader(writer, selector, reader,
                                            reader_user, error);
}
/** Appends all selected source-array items from a memory buffer. Behavior and
 * failure semantics match `lonejson_writer_array_items_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_array_items_buffer(lj_writer *writer, const char *selector,
                             const void *data, size_t len, lj_error *error) {
  return lonejson_writer_array_items_buffer(writer, selector, data, len, error);
}
/** Appends all selected source-array items from an open `FILE *`. Behavior and
 * failure semantics match `lonejson_writer_array_items_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_array_items_filep(
    lj_writer *writer, const char *selector, FILE *fp, lj_error *error) {
  return lonejson_writer_array_items_filep(writer, selector, fp, error);
}
/** Appends all selected source-array items from a file descriptor. Behavior
 * and failure semantics match `lonejson_writer_array_items_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_array_items_fd(
    lj_writer *writer, const char *selector, int fd, lj_error *error) {
  return lonejson_writer_array_items_fd(writer, selector, fd, error);
}
/** Opens `path`, appends all selected source-array items, and closes the file
 * before returning. Behavior and failure semantics match
 * `lonejson_writer_array_items_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_array_items_path(lj_writer *writer, const char *selector,
                           const char *path, lj_error *error) {
  return lonejson_writer_array_items_path(writer, selector, path, error);
}
/** Appends all selected source-array items from a rewindable spooled byte
 * container. The spooled value is copied as a read cursor, so the caller's
 * cursor is preserved. Behavior and failure semantics match
 * `lonejson_writer_array_items_reader()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_array_items_spooled(lj_writer *writer, const char *selector,
                              const lj_spooled *value, lj_error *error) {
  return lonejson_writer_array_items_spooled(writer, selector, value, error);
}
/** Emits one mapped struct value through lonejson's normal serializer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_mapped(lj_writer *writer,
                                                       const lj_map *map,
                                                       const void *src,
                                                       lj_error *error) {
  return lonejson_writer_mapped(writer, map, src, error);
}
/** Finishes a writer and validates that exactly one complete JSON value was
 * emitted and all containers were closed.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_writer_finish(lj_writer *writer,
                                                       lj_error *error) {
  return lonejson_writer_finish(writer, error);
}
/** Initializes a pull-style generator that drains writer events.
 *
 * `producer` is called by `lonejson_generator_read()` and receives a writer
 * handle. A writer event that returns `LONEJSON_STATUS_OK` has been accepted,
 * even if the generator still needs later reads to drain pending bytes. If an
 * event returns `LONEJSON_STATUS_TRUNCATED`, return that status and retry that
 * same event on the next producer call. The writer generator does not
 * materialize the full document.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_writer_generator_init(lonejson *runtime, lj_generator *generator,
                         lj_writer_producer_fn producer, void *producer_user) {
  return lonejson_writer_generator_init(runtime, generator, producer,
                                        producer_user);
}
/** Writes a top-level JSON string value from a reader to a sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_write_json_string_sink(
    lonejson *runtime, lj_reader_fn reader, void *reader_user, lj_sink_fn sink,
    void *sink_user, lj_error *error) {
  return lonejson_write_json_string_sink(runtime, reader, reader_user, sink,
                                         sink_user, error);
}
/** Writes a top-level JSON string value from a caller buffer to a sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_write_json_string_buffer_sink(
    lonejson *runtime, const void *data, size_t len, lj_sink_fn sink,
    void *sink_user, lj_error *error) {
  return lonejson_write_json_string_buffer_sink(runtime, data, len, sink,
                                                sink_user, error);
}
/** Writes a top-level JSON string value from a spooled byte container to a
 * sink.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_write_json_string_spooled_sink(
    lonejson *runtime, const lj_spooled *value, lj_sink_fn sink,
    void *sink_user, lj_error *error) {
  return lonejson_write_json_string_spooled_sink(runtime, value, sink,
                                                 sink_user, error);
}
/** Serializes a mapped struct into a caller-provided output buffer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_buffer(
    lonejson *runtime, const lj_map *map, const void *src, char *buffer,
    size_t capacity, size_t *needed, lj_error *error) {
  return lonejson_serialize_buffer(runtime, map, src, buffer, capacity, needed,
                                   error);
}
/** Serializes a mapped struct into a newly allocated JSON string using the
 * default allocator. Release the returned buffer with `free()` /
 * `LONEJSON_FREE()`. This convenience API rejects custom allocators; use
 * `lonejson_serialize_owned()` when you need allocator-aware ownership.
 */
LONEJSON_SHORT_ALIAS_INLINE char *
lj_serialize_alloc(lonejson *runtime, const lj_map *map, const void *src,
                   size_t *out_len, lj_error *error) {
  return lonejson_serialize_alloc(runtime, map, src, out_len, error);
}
/** Serializes a mapped struct into an owned output buffer. Initialize `out`
 * with `lonejson_owned_buffer_init()` or `lonejson_default_owned_buffer()`,
 * then release it with `lonejson_owned_buffer_free()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_owned(lonejson *runtime,
                                                         const lj_map *map,
                                                         const void *src,
                                                         lj_owned_buffer *out,
                                                         lj_error *error) {
  return lonejson_serialize_owned(runtime, map, src, out, error);
}
/** Releases an owned output buffer produced by `*_owned()` serializers. */
LONEJSON_SHORT_ALIAS_INLINE void lj_owned_buffer_free(lj_owned_buffer *buffer) {
  lonejson_owned_buffer_free(buffer);
}
/** Appends bytes to an initialized `lonejson_owned_buffer`.
 *
 * This is a generic sink callback for caller-owned accumulation. It grows the
 * buffer with the allocator stored in the handle and keeps `data` NUL
 * terminated for C-string convenience.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_owned_buffer_sink(void *user,
                                                           const void *data,
                                                           size_t len,
                                                           lj_error *error) {
  return lonejson_owned_buffer_sink(user, data, len, error);
}
/** Serializes a mapped struct to an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_filep(lonejson *runtime,
                                                         const lj_map *map,
                                                         const void *src,
                                                         FILE *fp,
                                                         lj_error *error) {
  return lonejson_serialize_filep(runtime, map, src, fp, error);
}
/** Serializes a mapped struct to a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_path(lonejson *runtime,
                                                        const lj_map *map,
                                                        const void *src,
                                                        const char *path,
                                                        lj_error *error) {
  return lonejson_serialize_path(runtime, map, src, path, error);
}
/** Serializes an array of mapped structs as JSONL records to a generic output
 * sink callback. Uses compact output by default, or two-space pretty printing
 * when `options->pretty` is non-zero. `stride` defaults to `map->struct_size`
 * when set to zero. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_sink(
    lonejson *runtime, const lj_map *map, const void *items, size_t count,
    size_t stride, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_serialize_jsonl_sink(runtime, map, items, count, stride, sink,
                                       user, error);
}
/** Serializes an array of mapped structs as JSONL records into a
 * caller-provided output buffer. Uses compact output by default, or two-space
 * pretty printing when `options->pretty` is non-zero. `stride` defaults to
 * `map->struct_size` when set to zero. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_buffer(
    lonejson *runtime, const lj_map *map, const void *items, size_t count,
    size_t stride, char *buffer, size_t capacity, size_t *needed,
    lj_error *error) {
  return lonejson_serialize_jsonl_buffer(runtime, map, items, count, stride,
                                         buffer, capacity, needed, error);
}
/** Serializes an array of mapped structs as JSONL records into a newly
 * allocated string using the default allocator. Release the returned buffer
 * with `free()` / `LONEJSON_FREE()`. This convenience API rejects custom
 * allocators; use `lonejson_serialize_jsonl_owned()` when you need
 * allocator-aware ownership.
 */
LONEJSON_SHORT_ALIAS_INLINE char *
lj_serialize_jsonl_alloc(lonejson *runtime, const lj_map *map,
                         const void *items, size_t count, size_t stride,
                         size_t *out_len, lj_error *error) {
  return lonejson_serialize_jsonl_alloc(runtime, map, items, count, stride,
                                        out_len, error);
}
/** Serializes an array of mapped structs as JSONL records into an owned output
 * buffer. Release it with `lonejson_owned_buffer_free()`. Uses compact output
 * by default, or two-space pretty printing when `options->pretty` is
 * non-zero. `stride` defaults to `map->struct_size` when set to zero.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_owned(
    lonejson *runtime, const lj_map *map, const void *items, size_t count,
    size_t stride, lj_owned_buffer *out, lj_error *error) {
  return lonejson_serialize_jsonl_owned(runtime, map, items, count, stride, out,
                                        error);
}
/** Serializes an array of mapped structs as JSONL records to an open `FILE *`.
 * Uses compact output by default, or two-space pretty printing when
 * `options->pretty` is non-zero. `stride` defaults to `map->struct_size` when
 * set to zero. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_filep(
    lonejson *runtime, const lj_map *map, const void *items, size_t count,
    size_t stride, FILE *fp, lj_error *error) {
  return lonejson_serialize_jsonl_filep(runtime, map, items, count, stride, fp,
                                        error);
}
/** Serializes an array of mapped structs as JSONL records to a filesystem
 * path. Uses compact output by default, or two-space pretty printing when
 * `options->pretty` is non-zero. `stride` defaults to `map->struct_size` when
 * set to zero. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_path(
    lonejson *runtime, const lj_map *map, const void *items, size_t count,
    size_t stride, const char *path, lj_error *error) {
  return lonejson_serialize_jsonl_path(runtime, map, items, count, stride, path,
                                       error);
}
/** Status-returning alias of lonejson_cleanup(); `error` may be `NULL`.
 * Workspace allocation failure leaves the record unchanged for a later retry.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_cleanup(const lj_map *map, void *value,
                                                 lj_error *error) {
  return lonejson_cleanup(map, value, error);
}
/** Initializes a mapped value according to its field descriptors. Use this
 * instead of manual `memset` when you need a known reusable starting state,
 * when you plan to parse with `clear_destination = 0`, or when you are
 * configuring caller-owned fixed-capacity array backing storage before parse.
 * Returns the same status and optional diagnostics as lonejson_init().
 * Workspace allocation failure leaves the destination untouched.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_init(lonejson *runtime,
                                              const lj_map *map, void *value,
                                              lj_error *error) {
  return lonejson_init(runtime, map, value, error);
}
/** Status-returning alias of lonejson_reset(), preserving caller-owned
 * fixed-capacity backing. `error` may be `NULL`. Workspace allocation failure
 * leaves the initialized record unchanged; retry reset before reuse.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_reset(lonejson *runtime,
                                               const lj_map *map, void *value,
                                               lj_error *error) {
  return lonejson_reset(runtime, map, value, error);
}
/** Computes the encoded byte length for raw bytes and base64 variant. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_base64_encoded_len(
    size_t len, lj_base64_variant variant, size_t *out_len, lj_error *error) {
  return lonejson_base64_encoded_len(len, variant, out_len, error);
}
/** Encodes raw bytes into caller-provided base64 storage. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_base64_encode(const void *data, size_t len, lj_base64_variant variant,
                 char *out, size_t capacity, size_t *needed, lj_error *error) {
  return lonejson_base64_encode(data, len, variant, out, capacity, needed,
                                error);
}
/** Encodes raw bytes as base64 and streams encoded chunks to a sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_base64_encode_sink(const void *data, size_t len, lj_base64_variant variant,
                      lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_base64_encode_sink(data, len, variant, sink, user, error);
}
/** Computes the decoded byte length for one base64 value. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_base64_decoded_len(const char *data, size_t len, lj_base64_variant variant,
                      size_t *out_len, lj_error *error) {
  return lonejson_base64_decoded_len(data, len, variant, out_len, error);
}
/** Decodes one base64 value into caller-provided storage. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_base64_decode(
    const char *data, size_t len, lj_base64_variant variant, unsigned char *out,
    size_t capacity, size_t *needed, lj_error *error) {
  return lonejson_base64_decode(data, len, variant, out, capacity, needed,
                                error);
}
/** Decodes one base64 value and streams decoded chunks to a sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_base64_decode_sink(const char *data, size_t len, lj_base64_variant variant,
                      lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_base64_decode_sink(data, len, variant, sink, user, error);
}
#ifdef LONEJSON_WITH_JWT
/** Installs or clears the runtime auth provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_set_auth_provider(
    lj *runtime, const lj_auth_provider *provider, lj_error *error) {
  return lonejson_set_auth_provider(runtime, provider, error);
}
#ifdef LONEJSON_WITH_OPENSSL
/** Initializes an OpenSSL-backed auth provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_auth_provider_init_openssl(
    lj_auth_provider *provider, const lj_openssl_auth_provider_config *config,
    lj_error *error) {
  return lonejson_auth_provider_init_openssl(provider, config, error);
}
#endif
/** Parses one JWT compact serialization into caller-owned segment slices. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_jwt_parse_compact(const char *token,
                                                           size_t len,
                                                           lj_jwt_compact *out,
                                                           lj_error *error) {
  return lonejson_jwt_parse_compact(token, len, out, error);
}
/** Initializes a JWK object for parsing or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwk_init(lj_jwk *jwk) {
  lonejson_jwk_init(jwk);
}
/** Releases storage owned by a JWK object and resets it to empty. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwk_cleanup(lj_jwk *jwk) {
  lonejson_jwk_cleanup(jwk);
}
/** Initializes a JWKS object for parsing or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwks_init(lj_jwks *jwks) {
  lonejson_jwks_init(jwks);
}
/** Releases storage owned by a JWKS object and resets it to empty. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwks_cleanup(lj_jwks *jwks) {
  lonejson_jwks_cleanup(jwks);
}
/** Parses one JWK JSON object into `out`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_jwk_parse_json(lj *runtime,
                                                        const char *json,
                                                        size_t len, lj_jwk *out,
                                                        lj_error *error) {
  return lonejson_jwk_parse_json(runtime, json, len, out, error);
}
/** Parses one JWKS JSON object with a required `keys` array. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_jwks_parse_json(
    lj *runtime, const char *json, size_t len, lj_jwks *out, lj_error *error) {
  return lonejson_jwks_parse_json(runtime, json, len, out, error);
}
/** Selects the first JWK matching all non-NULL filters. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_jwks_select(const lj_jwks *jwks, const lj_jwk_select_options *options,
               const lj_jwk **out, lj_error *error) {
  return lonejson_jwks_select(jwks, options, out, error);
}
/** Initializes decoded JWT header storage. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwt_header_init(lj_jwt_header *header) {
  lonejson_jwt_header_init(header);
}
/** Releases storage owned by decoded JWT header storage. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwt_header_cleanup(lj_jwt_header *header) {
  lonejson_jwt_header_cleanup(header);
}
/** Initializes decoded JWT claims storage. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwt_claims_init(lj_jwt_claims *claims) {
  lonejson_jwt_claims_init(claims);
}
/** Releases storage owned by decoded JWT claims storage. */
LONEJSON_SHORT_ALIAS_INLINE void lj_jwt_claims_cleanup(lj_jwt_claims *claims) {
  lonejson_jwt_claims_cleanup(claims);
}
/** Decodes and parses the header and claims payload from one compact JWT. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_jwt_decode_compact(lj *runtime, const char *token, size_t len,
                      const lj_jwt_claim_policy *limits, lj_jwt_header *header,
                      lj_jwt_claims *claims, lj_error *error) {
  return lonejson_jwt_decode_compact(runtime, token, len, limits, header,
                                     claims, error);
}
/** Validates decoded JWT header and claims against an explicit policy. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_jwt_validate_claims(const lj_jwt_header *header, const lj_jwt_claims *claims,
                       const lj_jwt_claim_policy *policy, lj_error *error) {
  return lonejson_jwt_validate_claims(header, claims, policy, error);
}
/** Validates a compact JWT signature against a selected JWK. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_jwt_validate_signature(
    const lj_jwt_compact *jwt, const lj_jwt_header *header, const lj_jwk *jwk,
    lj_error *error) {
  return lonejson_jwt_validate_signature(jwt, header, jwk, error);
}
/** Validates a compact JWT signature through a runtime auth provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_jwt_validate_signature_with_runtime(
    lj *runtime, const lj_jwt_compact *jwt, const lj_jwt_header *header,
    const lj_jwk *jwk, lj_error *error) {
  return lonejson_jwt_validate_signature_with_runtime(runtime, jwt, header, jwk,
                                                      error);
}
#endif
#ifdef LONEJSON_WITH_OIDC
/** Initializes a materialized HTTP response for provider use or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_http_response_init(lj_http_response *response) {
  lonejson_http_response_init(response);
}
/** Releases storage owned by a materialized HTTP response. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_http_response_cleanup(lj_http_response *response) {
  lonejson_http_response_cleanup(response);
}
/** Initializes an HTTP provider from caller-owned callback config. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_http_provider_init(lj_http_provider *provider,
                      const lj_http_provider_config *config, lj_error *error) {
  return lonejson_http_provider_init(provider, config, error);
}
/** Initializes an HTTP provider from direct caller-owned callback arguments. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_http_provider_init_simple(
    lj_http_provider *provider, void *user_data, const char *user_agent,
    lj_status (*request)(void *user_data, const lj_http_request *request,
                         lj_http_response *response, lj_error *error),
    lj_error *error) {
  return lonejson_http_provider_init_simple(
      provider, user_data, user_agent,
      (lonejson_status (*)(void *, const lonejson_http_request *,
                           lonejson_http_response *, lonejson_error *))request,
      error);
}
/** Installs or clears the runtime auth HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_set_http_provider(
    lj *runtime, const lj_http_provider *provider, lj_error *error) {
  return lonejson_set_http_provider(runtime, provider, error);
}
/** Initializes OIDC discovery metadata storage. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_discovery_init(lj_oidc_discovery *discovery) {
  lonejson_oidc_discovery_init(discovery);
}
/** Releases storage owned by OIDC discovery metadata. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_discovery_cleanup(lj_oidc_discovery *discovery) {
  lonejson_oidc_discovery_cleanup(discovery);
}
/** Builds the OIDC discovery URL for an HTTPS issuer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_discovery_url(
    const char *issuer, lj_owned_buffer *out, lj_error *error) {
  return lonejson_oidc_discovery_url(issuer, out, error);
}
/** Parses one OIDC discovery JSON object into `out`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_discovery_parse_json(lj *runtime, const char *json, size_t len,
                             lj_oidc_discovery *out, lj_error *error) {
  return lonejson_oidc_discovery_parse_json(runtime, json, len, out, error);
}
/** Validates parsed discovery metadata against the caller-configured issuer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_discovery_validate_issuer(
    const lj_oidc_discovery *discovery, const char *expected_issuer,
    lj_error *error) {
  return lonejson_oidc_discovery_validate_issuer(discovery, expected_issuer,
                                                 error);
}
/** Fetches, parses, and validates discovery metadata through the HTTP provider.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_fetch_discovery(
    lj *runtime, const char *issuer, size_t max_response_bytes,
    lj_oidc_discovery *out, lj_error *error) {
  return lonejson_oidc_fetch_discovery(runtime, issuer, max_response_bytes, out,
                                       error);
}
/** Initializes a JWKS cache for later update or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_jwks_cache_init(lj_oidc_jwks_cache *cache) {
  lonejson_oidc_jwks_cache_init(cache);
}
/** Releases all storage owned by a JWKS cache. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_jwks_cache_cleanup(lj_oidc_jwks_cache *cache) {
  lonejson_oidc_jwks_cache_cleanup(cache);
}
/** Installs caller-provided JWKS JSON into a bounded cache. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_jwks_cache_update_json(lj *runtime, lj_oidc_jwks_cache *cache,
                               const lj_oidc_jwks_cache_policy *policy,
                               const char *json, size_t len, lj_error *error) {
  return lonejson_oidc_jwks_cache_update_json(runtime, cache, policy, json, len,
                                              error);
}
/** Returns non-zero when a cache has fresh keys for the configured issuer/URI.
 */
LONEJSON_SHORT_ALIAS_INLINE int
lj_oidc_jwks_cache_is_fresh(const lj_oidc_jwks_cache *cache,
                            const lj_oidc_jwks_cache_policy *policy) {
  return lonejson_oidc_jwks_cache_is_fresh(cache, policy);
}
/** Selects a key from a fresh JWKS cache. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_jwks_cache_select(
    const lj_oidc_jwks_cache *cache, const lj_oidc_jwks_cache_policy *policy,
    const lj_jwk_select_options *options, const lj_jwk **out, lj_error *error) {
  return lonejson_oidc_jwks_cache_select(cache, policy, options, out, error);
}
/** Refreshes a JWKS cache through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_jwks_cache_refresh(
    lj *runtime, lj_oidc_jwks_cache *cache,
    const lj_oidc_jwks_cache_policy *policy, lj_error *error) {
  return lonejson_oidc_jwks_cache_refresh(runtime, cache, policy, error);
}
/** Builds an `application/x-www-form-urlencoded` client-credentials body. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oauth2_client_credentials_body(const lj_oauth2_client_credentials *request,
                                  lj_owned_buffer *out, lj_error *error) {
  return lonejson_oauth2_client_credentials_body(request, out, error);
}
/** Builds an `application/x-www-form-urlencoded` refresh-token body. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oauth2_refresh_token_body(const lj_oauth2_refresh_token *request,
                             lj_owned_buffer *out, lj_error *error) {
  return lonejson_oauth2_refresh_token_body(request, out, error);
}
/** Builds an introspection request body. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oauth2_token_introspection_body(const lj_oauth2_token_introspection *request,
                                   lj_owned_buffer *out, lj_error *error) {
  return lonejson_oauth2_token_introspection_body(request, out, error);
}
/** Builds a revocation request body. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oauth2_token_revocation_body(const lj_oauth2_token_revocation *request,
                                lj_owned_buffer *out, lj_error *error) {
  return lonejson_oauth2_token_revocation_body(request, out, error);
}
/** Builds an authorization-code token body. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_authorization_code_token_body(
    const lj_oidc_authorization_code_token *request, lj_owned_buffer *out,
    lj_error *error) {
  return lonejson_oidc_authorization_code_token_body(request, out, error);
}
/** Initializes a token response for parsing or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oauth2_token_response_init(lj_oauth2_token_response *response) {
  lonejson_oauth2_token_response_init(response);
}
/** Releases all storage owned by a token response. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oauth2_token_response_cleanup(lj_oauth2_token_response *response) {
  lonejson_oauth2_token_response_cleanup(response);
}
/** Parses and validates a bounded successful OAuth2 token endpoint response. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_token_response_parse_json(
    lj *runtime, const char *json, size_t len, size_t max_response_bytes,
    lj_oauth2_token_response *out, lj_error *error) {
  return lonejson_oauth2_token_response_parse_json(
      runtime, json, len, max_response_bytes, out, error);
}
/** Initializes an introspection response for parsing or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void lj_oauth2_introspection_response_init(
    lj_oauth2_introspection_response *response) {
  lonejson_oauth2_introspection_response_init(response);
}
/** Releases all storage owned by an introspection response. */
LONEJSON_SHORT_ALIAS_INLINE void lj_oauth2_introspection_response_cleanup(
    lj_oauth2_introspection_response *response) {
  lonejson_oauth2_introspection_response_cleanup(response);
}
/** Parses and validates a bounded OAuth2 introspection response. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oauth2_introspection_response_parse_json(
    lj *runtime, const char *json, size_t len, size_t max_response_bytes,
    lj_oauth2_introspection_response *out, lj_error *error) {
  return lonejson_oauth2_introspection_response_parse_json(
      runtime, json, len, max_response_bytes, out, error);
}
/** Exchanges OAuth2 client credentials through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_client_credentials_request(
    lj *runtime, const char *token_endpoint,
    const lj_oauth2_client_credentials *request, size_t max_response_bytes,
    lj_oauth2_token_response *out, lj_error *error) {
  return lonejson_oauth2_client_credentials_request(
      runtime, token_endpoint, request, max_response_bytes, out, error);
}
/** Exchanges an OAuth2 refresh token through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_refresh_token_request(
    lj *runtime, const char *token_endpoint,
    const lj_oauth2_refresh_token *request, size_t max_response_bytes,
    lj_oauth2_token_response *out, lj_error *error) {
  return lonejson_oauth2_refresh_token_request(runtime, token_endpoint, request,
                                               max_response_bytes, out, error);
}
/** Initializes token-flow storage for update or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oauth2_token_flow_init(lj_oauth2_token_flow *flow) {
  lonejson_oauth2_token_flow_init(flow);
}
/** Releases all storage owned by a token flow. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oauth2_token_flow_cleanup(lj_oauth2_token_flow *flow) {
  lonejson_oauth2_token_flow_cleanup(flow);
}
/** Copies borrowed or persisted token-flow fields into owned storage. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_token_flow_assign(
    lj_oauth2_token_flow *flow, const lj_oauth2_token_flow *source,
    lj_error *error) {
  return lonejson_oauth2_token_flow_assign(flow, source, error);
}
/** Returns non-zero when the flow has no usable access token at `now`. */
LONEJSON_SHORT_ALIAS_INLINE int
lj_oauth2_token_flow_is_expired(const lj_oauth2_token_flow *flow, lj_int64 now,
                                lj_int64 skew_seconds) {
  return lonejson_oauth2_token_flow_is_expired(flow, now, skew_seconds);
}
/** Updates token-flow storage from a successful token endpoint response. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_token_flow_update_response(
    lj_oauth2_token_flow *flow, const lj_oauth2_token_response *response,
    lj_int64 now, lj_error *error) {
  return lonejson_oauth2_token_flow_update_response(flow, response, now, error);
}
/** Ensures a token-flow has a usable access token, refreshing if needed. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_token_flow_ensure(
    lj *runtime, lj_oauth2_token_flow *flow,
    const lj_oauth2_token_flow_policy *policy,
    lj_oauth2_token_flow_result *result, lj_error *error) {
  return lonejson_oauth2_token_flow_ensure(runtime, flow, policy, result,
                                           error);
}
/** Introspects a token through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_introspect_token_request(
    lj *runtime, const char *introspection_endpoint,
    const lj_oauth2_token_introspection *request, size_t max_response_bytes,
    lj_oauth2_introspection_response *out, lj_error *error) {
  return lonejson_oauth2_introspect_token_request(
      runtime, introspection_endpoint, request, max_response_bytes, out, error);
}
/** Revokes a token through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oauth2_revoke_token_request(
    lj *runtime, const char *revocation_endpoint,
    const lj_oauth2_token_revocation *request, lj_error *error) {
  return lonejson_oauth2_revoke_token_request(runtime, revocation_endpoint,
                                              request, error);
}
/** Initializes an OIDC UserInfo response for parsing or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_userinfo_response_init(lj_oidc_userinfo_response *out) {
  lonejson_oidc_userinfo_response_init(out);
}
/** Releases all storage owned by an OIDC UserInfo response. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_userinfo_response_cleanup(lj_oidc_userinfo_response *out) {
  lonejson_oidc_userinfo_response_cleanup(out);
}
/** Parses and validates a bounded OIDC UserInfo response. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_userinfo_response_parse_json(
    lj *runtime, const char *json, size_t len, size_t max_response_bytes,
    lj_oidc_userinfo_response *out, lj_error *error) {
  return lonejson_oidc_userinfo_response_parse_json(
      runtime, json, len, max_response_bytes, out, error);
}
/** Fetches OIDC UserInfo through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_fetch_userinfo(lj *runtime, const char *userinfo_endpoint,
                       const lj_oidc_userinfo_request *request,
                       lj_oidc_userinfo_response *out, lj_error *error) {
  return lonejson_oidc_fetch_userinfo(runtime, userinfo_endpoint, request, out,
                                      error);
}
/** Exchanges an authorization code through the runtime HTTP provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_authorization_code_token_request(
    lj *runtime, const char *token_endpoint,
    const lj_oidc_authorization_code_token *request, size_t max_response_bytes,
    lj_oauth2_token_response *out, lj_error *error) {
  return lonejson_oidc_authorization_code_token_request(
      runtime, token_endpoint, request, max_response_bytes, out, error);
}
/** Initializes a PKCE pair for generation or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void lj_oidc_pkce_init(lj_oidc_pkce *pkce) {
  lonejson_oidc_pkce_init(pkce);
}
/** Releases all storage owned by a PKCE pair. */
LONEJSON_SHORT_ALIAS_INLINE void lj_oidc_pkce_cleanup(lj_oidc_pkce *pkce) {
  lonejson_oidc_pkce_cleanup(pkce);
}
/** Computes a base64url S256 PKCE challenge for a verifier. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_pkce_challenge(
    const char *code_verifier, lj_owned_buffer *out, lj_error *error) {
  return lonejson_oidc_pkce_challenge(code_verifier, out, error);
}
/** Computes a PKCE S256 challenge through a runtime auth provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_pkce_challenge_with_runtime(lj *runtime, const char *code_verifier,
                                    lj_owned_buffer *out, lj_error *error) {
  return lonejson_oidc_pkce_challenge_with_runtime(runtime, code_verifier, out,
                                                   error);
}
/** Generates a random PKCE verifier and matching S256 challenge. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_pkce_generate(
    size_t verifier_bytes, lj_oidc_pkce *out, lj_error *error) {
  return lonejson_oidc_pkce_generate(verifier_bytes, out, error);
}
/** Generates a PKCE verifier through a runtime auth provider. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_pkce_generate_with_runtime(
    lj *runtime, size_t verifier_bytes, lj_oidc_pkce *out, lj_error *error) {
  return lonejson_oidc_pkce_generate_with_runtime(runtime, verifier_bytes, out,
                                                  error);
}
/** Builds an authorization-code URL with PKCE S256 parameters. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_authorization_url(const lj_oidc_authorization_request *request,
                          lj_owned_buffer *out, lj_error *error) {
  return lonejson_oidc_authorization_url(request, out, error);
}
/** Initializes a parsed authorization callback for parsing or cleanup. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_authorization_callback_init(lj_oidc_authorization_callback *callback) {
  lonejson_oidc_authorization_callback_init(callback);
}
/** Releases all storage owned by a parsed authorization callback. */
LONEJSON_SHORT_ALIAS_INLINE void lj_oidc_authorization_callback_cleanup(
    lj_oidc_authorization_callback *callback) {
  lonejson_oidc_authorization_callback_cleanup(callback);
}
/** Parses and validates an authorization-code callback query string. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_authorization_callback_parse_query(const char *query, size_t len,
                                           const char *expected_state,
                                           size_t max_query_bytes,
                                           lj_oidc_authorization_callback *out,
                                           lj_error *error) {
  return lonejson_oidc_authorization_callback_parse_query(
      query, len, expected_state, max_query_bytes, out, error);
}

LONEJSON_SHORT_ALIAS_INLINE const char *
lj_auth_failure_string(lj_auth_failure failure) {
  return lonejson_auth_failure_string(failure);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_bearer_validation_init(lj_oidc_bearer_validation *validation) {
  lonejson_oidc_bearer_validation_init(validation);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_bearer_validation_cleanup(lj_oidc_bearer_validation *validation) {
  lonejson_oidc_bearer_validation_cleanup(validation);
}

LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_authorization_bearer_token(
    const char *authorization_header, lj_jwt_segment *out, lj_error *error) {
  return lonejson_oidc_authorization_bearer_token(authorization_header, out,
                                                  error);
}

LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_validate_bearer_token(
    lj *runtime, const lj_oidc_bearer_validation_request *request,
    lj_oidc_bearer_validation *out, lj_error *error) {
  return lonejson_oidc_validate_bearer_token(runtime, request, out, error);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_m2m_credential_init(lj_m2m_credential *credential) {
  lonejson_m2m_credential_init(credential);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_m2m_credential_cleanup(lj_m2m_credential *credential) {
  lonejson_m2m_credential_cleanup(credential);
}

LONEJSON_SHORT_ALIAS_INLINE lj_status lj_m2m_credential_generate(
    lj *runtime, const lj_m2m_credential_request *request,
    lj_m2m_credential *out, lj_error *error) {
  return lonejson_m2m_credential_generate(runtime, request, out, error);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_m2m_authentication_init(lj_m2m_authentication *auth) {
  lonejson_m2m_authentication_init(auth);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_m2m_authentication_cleanup(lj_m2m_authentication *auth) {
  lonejson_m2m_authentication_cleanup(auth);
}

LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_m2m_verify_authorization(lj *runtime, const lj_m2m_verify_request *request,
                            lj_m2m_authentication *out, lj_error *error) {
  return lonejson_m2m_verify_authorization(runtime, request, out, error);
}

LONEJSON_SHORT_ALIAS_INLINE void lj_m2m_signup_init(lj_m2m_signup *signup) {
  lonejson_m2m_signup_init(signup);
}

LONEJSON_SHORT_ALIAS_INLINE void lj_m2m_signup_cleanup(lj_m2m_signup *signup) {
  lonejson_m2m_signup_cleanup(signup);
}

LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_m2m_signup_generate(lj *runtime, const lj_m2m_signup_request *request,
                       lj_m2m_signup *out, lj_error *error) {
  return lonejson_m2m_signup_generate(runtime, request, out, error);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_m2m_signup_complete_init(lj_m2m_signup_completion *complete) {
  lonejson_m2m_signup_complete_init(complete);
}

LONEJSON_SHORT_ALIAS_INLINE void
lj_m2m_signup_complete_cleanup(lj_m2m_signup_completion *complete) {
  lonejson_m2m_signup_complete_cleanup(complete);
}

LONEJSON_SHORT_ALIAS_INLINE lj_status lj_m2m_signup_complete(
    lj *runtime, const lj_m2m_signup_complete_request *request,
    lj_m2m_signup_completion *out, lj_error *error) {
  return lonejson_m2m_signup_complete(runtime, request, out, error);
}
#endif
#ifdef LONEJSON_WITH_CURL
/** Curl response parse adapter state for incremental `CURLOPT_WRITEFUNCTION`
 * parsing. Initialize all curl adapter storage to zero before its first use
 * (for example, `lonejson_curl_parse ctx = {0}`). Init may then be called again
 * on a live adapter; it releases the previous state before replacing it.
 */
typedef lonejson_curl_parse lj_curl_parse;
/** Curl response adapter state for streaming selected array items from
 * `CURLOPT_WRITEFUNCTION` chunks.
 */
typedef lonejson_curl_array_parse lj_curl_array_parse;
/** Curl response adapter state for streaming selected string array items from
 * `CURLOPT_WRITEFUNCTION` chunks.
 */
typedef lonejson_curl_string_array_parse lj_curl_string_array_parse;
/** Curl response adapter state for selected string array items reported once
 * per decoded item from `CURLOPT_WRITEFUNCTION` chunks.
 */
typedef lonejson_curl_string_items_parse lj_curl_string_items_parse;
/** Curl upload adapter state for streaming generated JSON to libcurl. */
typedef lonejson_curl_upload lj_curl_upload;
#ifdef LONEJSON_WITH_OIDC
/** Curl response adapter that installs a bounded JWKS response into a cache. */
typedef lonejson_oidc_jwks_cache_parse lj_oidc_jwks_cache_parse;
#endif
/** Initializes a curl parse adapter suitable for `CURLOPT_WRITEFUNCTION`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_parse_init(lj_curl_parse *ctx,
                                                         lj *runtime,
                                                         const lj_map *map,
                                                         void *dst) {
  return lonejson_curl_parse_init(ctx, runtime, map, dst);
}
/** Curl write callback that incrementally feeds response bytes to lonejson. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_write_callback(char *ptr,
                                                          size_t size,
                                                          size_t nmemb,
                                                          void *userdata) {
  return lonejson_curl_write_callback(ptr, size, nmemb, userdata);
}
/** Finalizes a curl-backed parse adapter. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_parse_finish(lj_curl_parse *ctx) {
  return lonejson_curl_parse_finish(ctx);
}
/** Releases resources owned by a curl parse adapter. */
LONEJSON_SHORT_ALIAS_INLINE void lj_curl_parse_cleanup(lj_curl_parse *ctx) {
  lonejson_curl_parse_cleanup(ctx);
}
/** Initializes a curl write adapter that streams selected array items directly
 * into `dst` and reports each item through `callback`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_array_parse_init(
    lj_curl_array_parse *ctx, lj *runtime, const char *path, const lj_map *map,
    void *dst, lj_array_stream_item_fn callback, void *user) {
  return lonejson_curl_array_parse_init(ctx, runtime, path, map, dst, callback,
                                        user);
}
/** Curl write callback that forwards response bytes into a selected-array
 * stream adapter.
 */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_array_write_callback(
    char *ptr, size_t size, size_t nmemb, void *userdata) {
  return lonejson_curl_array_write_callback(ptr, size, nmemb, userdata);
}
/** Finalizes a curl selected-array stream adapter after curl reaches EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_array_parse_finish(lj_curl_array_parse *ctx) {
  return lonejson_curl_array_parse_finish(ctx);
}
/** Releases resources owned by a curl selected-array stream adapter. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_curl_array_parse_cleanup(lj_curl_array_parse *ctx) {
  lonejson_curl_array_parse_cleanup(ctx);
}
/** Initializes a curl write adapter that streams selected string-array items.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_string_array_parse_init(
    lj_curl_string_array_parse *ctx, lj *runtime, const char *path,
    const lj_array_stream_string_handler *handler, void *user) {
  return lonejson_curl_string_array_parse_init(ctx, runtime, path, handler,
                                               user);
}
/** Curl write callback for a selected string-array stream adapter. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_string_array_write_callback(
    char *ptr, size_t size, size_t nmemb, void *userdata) {
  return lonejson_curl_string_array_write_callback(ptr, size, nmemb, userdata);
}
/** Finalizes a curl selected string-array stream adapter after curl EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_string_array_parse_finish(lj_curl_string_array_parse *ctx) {
  return lonejson_curl_string_array_parse_finish(ctx);
}
/** Releases resources owned by a curl selected string-array stream adapter. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_curl_string_array_parse_cleanup(lj_curl_string_array_parse *ctx) {
  lonejson_curl_string_array_parse_cleanup(ctx);
}
/** Initializes a curl write adapter that reports selected string-array items
 * once per decoded string. Each item is assembled through the chunked string
 * path into bounded temporary storage.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_string_items_parse_init(
    lj_curl_string_items_parse *ctx, lj *runtime, const char *path,
    lj_array_stream_string_fn callback, void *user) {
  return lonejson_curl_string_items_parse_init(ctx, runtime, path, callback,
                                               user);
}
/** Curl write callback for a selected string-array item adapter. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_string_items_write_callback(
    char *ptr, size_t size, size_t nmemb, void *userdata) {
  return lonejson_curl_string_items_write_callback(ptr, size, nmemb, userdata);
}
/** Finalizes a curl selected string-array item adapter after curl EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_string_items_parse_finish(lj_curl_string_items_parse *ctx) {
  return lonejson_curl_string_items_parse_finish(ctx);
}
/** Releases resources owned by a curl selected string-array item adapter. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_curl_string_items_parse_cleanup(lj_curl_string_items_parse *ctx) {
  lonejson_curl_string_items_parse_cleanup(ctx);
}
/** Initializes a curl upload adapter for a mapped struct.
 *
 * This adapter is stream-first: it incrementally serializes through curl's
 * `CURLOPT_READFUNCTION` callback instead of materializing the whole JSON
 * payload up front. The reported size is currently always `-1`; lonejson does
 * not do a hidden prebuffer or pre-count pass just to compute it.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_upload_init(lj_curl_upload *ctx,
                                                          lj *runtime,
                                                          const lj_map *map,
                                                          const void *src) {
  return lonejson_curl_upload_init(ctx, runtime, map, src);
}
/** Curl read callback that streams serialized JSON bytes to libcurl. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_read_callback(char *ptr, size_t size,
                                                         size_t nmemb,
                                                         void *userdata) {
  return lonejson_curl_read_callback(ptr, size, nmemb, userdata);
}
/** Returns the upload size reported by a curl upload adapter. The value is
 * `-1` when lonejson is streaming with an unknown total length.
 */
LONEJSON_SHORT_ALIAS_INLINE curl_off_t
lj_curl_upload_size(const lj_curl_upload *ctx) {
  return lonejson_curl_upload_size(ctx);
}
/** Checks replay capability without consuming source contents. */
LONEJSON_SHORT_ALIAS_INLINE int
lj_curl_upload_is_rewindable(const lj_curl_upload *ctx) {
  return lonejson_curl_upload_is_rewindable(ctx);
}
/** Restarts a mapped upload without buffering or measuring it. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_upload_rewind(lj_curl_upload *ctx) {
  return lonejson_curl_upload_rewind(ctx);
}
/** Curl seek callback; pass the upload adapter as userdata. */
LONEJSON_SHORT_ALIAS_INLINE int
lj_curl_seek_callback(void *userdata, curl_off_t offset, int origin) {
  return lonejson_curl_seek_callback(userdata, offset, origin);
}
/** Releases resources owned by a curl upload adapter. */
LONEJSON_SHORT_ALIAS_INLINE void lj_curl_upload_cleanup(lj_curl_upload *ctx) {
  lonejson_curl_upload_cleanup(ctx);
}
#ifdef LONEJSON_WITH_OIDC
/** Initializes a curl write adapter for a bounded JWKS cache refresh. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_oidc_jwks_cache_parse_init(
    lj_oidc_jwks_cache_parse *ctx, lj *runtime, lj_oidc_jwks_cache *cache,
    const lj_oidc_jwks_cache_policy *policy) {
  return lonejson_oidc_jwks_cache_parse_init(ctx, runtime, cache, policy);
}
/** Curl write callback for a JWKS cache refresh adapter. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_oidc_jwks_cache_write_callback(
    char *ptr, size_t size, size_t nmemb, void *userdata) {
  return lonejson_oidc_jwks_cache_write_callback(ptr, size, nmemb, userdata);
}
/** Finalizes a JWKS cache refresh adapter after curl EOF. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_oidc_jwks_cache_parse_finish(lj_oidc_jwks_cache_parse *ctx) {
  return lonejson_oidc_jwks_cache_parse_finish(ctx);
}
/** Releases resources owned by a JWKS cache refresh adapter. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_oidc_jwks_cache_parse_cleanup(lj_oidc_jwks_cache_parse *ctx) {
  lonejson_oidc_jwks_cache_parse_cleanup(ctx);
}
#endif
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
