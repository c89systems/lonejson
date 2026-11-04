/*
 * MIT License
 *
 * lonejson is Copyright (c) 2026 Michel Blomgren <mike@c89.systems>
 * https://c89.systems
 */

#ifndef LONEJSON_LUA_H
#define LONEJSON_LUA_H

#include "lonejson.h"

#include <lua.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Lua C module embedder interop boundary.
 *
 * This header belongs to the Lua facade source surface, not the installed core
 * C SDK. The generic `lonejson_schema_view` and `lonejson_record_view` structs
 * are Lua-agnostic core views from `lonejson.h`; the functions below adapt
 * Lua-owned userdata on one `lua_State` into those borrowed views.
 *
 * Views and record pointers returned by these functions remain owned by Lua and
 * are valid only while the backing userdata remains alive and unchanged.
 * Embedders that retain Lua-owned objects beyond the current stack frame must
 * keep their own Lua registry reference to that userdata.
 */

/** Checks that a Lua stack value is a lonejson schema userdata and fills a
 * generic borrowed schema view. This API returns a status instead of raising a
 * Lua argument error for type or ABI mismatches.
 */
lonejson_status lonejson_lua_check_schema(lua_State *L, int index,
                                          lonejson_schema_view *out,
                                          lonejson_error *error);
/** Creates a Lua-owned record userdata for the schema userdata at
 * `schema_index` and pushes it on the Lua stack. `record_index`, when non-NULL,
 * receives the absolute stack index of the new userdata.
 */
lonejson_status lonejson_lua_new_record(lua_State *L, int schema_index,
                                        int *record_index,
                                        lonejson_record_view *out,
                                        lonejson_error *error);
/** Checks that a Lua stack value is a lonejson record userdata, optionally
 * validating it against `schema`, and fills a generic borrowed record view.
 */
lonejson_status lonejson_lua_check_record(lua_State *L, int index,
                                          const lonejson_schema_view *schema,
                                          lonejson_record_view *out,
                                          lonejson_error *error);
/** Clears and reinitializes a Lua-owned record userdata in place. */
lonejson_status lonejson_lua_clear_record(lua_State *L, int record_index,
                                          lonejson_error *error);
/** Converts a Lua-owned record userdata to a newly pushed Lua table and returns
 * the table's absolute stack index, or 0 when `record_index` is not a lonejson
 * record userdata.
 */
int lonejson_lua_record_to_table(lua_State *L, int record_index);
/** Parses JSON from a callback-backed reader directly into a Lua-owned record
 * userdata created from the schema at `schema_index`.
 */
lonejson_status
lonejson_lua_parse_reader_to_record(lua_State *L, int schema_index,
                                    lonejson_reader_fn reader, void *userdata,
                                    int *record_index, lonejson_error *error);
/** Parses JSON from a callback-backed reader into a Lua-created schema and
 * pushes the resulting Lua table.
 */
lonejson_status
lonejson_lua_parse_reader_to_table(lua_State *L, int schema_index,
                                   lonejson_reader_fn reader, void *userdata,
                                   int *table_index, lonejson_error *error);

#ifdef __cplusplus
}
#endif

#endif
