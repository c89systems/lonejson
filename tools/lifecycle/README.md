These five scripts are an exact vendored copy of the
`pkt-systems-cmake-lifecycle` skill helper collection read on 2026-10-28.
Keep them together and update them as one collection. Project entrypoints in
`scripts/` validate cache ownership and add native Apple discovery; CMake passes
this directory as `CPKT_LIFECYCLE_SCRIPTS_DIR` through compiler probes.
