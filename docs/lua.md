# Lua binding

The [full Lua manual](https://github.com/c89systems/lonejson/blob/trunk/lua/README.md)
documents the schema DSL and API. The binding supports schema-guided decoding,
reusable records, object-framed and selected-array streams, spool-backed fields,
and arbitrary `json_value` fields. It links against the public `liblonejson`
boundary rather than embedding the C implementation.

## Build and test

From the source checkout, install into `build/luarocks` and run integration tests:

```sh
make lua-rock
make lua-test
```

For out-of-tree LuaRocks builds, set `LONEJSON_LIBDIR` to the directory containing
`liblonejson` and make it available to the platform dynamic loader.

## Run the examples

After `make lua-rock`, set the LuaRocks search paths and use the installed-rock
runner. Linux uses a target-built embedding runner with the pinned Bootlin
runtime; Darwin uses the resolved native Lua interpreter and installed SDK.

```sh
eval "$(luarocks path --tree build/luarocks)"
scripts/run_installed_lua.sh "$PWD/build/luarocks" \
  "$PWD/build/lua-sdk/lib" "$(scripts/resolve_lua55.sh)" \
  examples/lua_binding.lua examples/lua_nullable.lua \
  examples/lua_json_value_nested.lua examples/lua_json_value_object_array.lua
```

Pass just one script path to run an individual example. On Linux, search paths
alone do not select the target loader; use the runner rather than invoking the
host `lua` directly.

For reusable records with `clear_destination_by_default = false`, omitted
fields remain as they were, while present arrays replace previous contents.
`json_value` fields work at top level, in nested objects and in object-array
element schemas.
