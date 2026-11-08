# Performance Logs

Generated benchmark artifacts live here.

The standalone Lua benchmark harness writes its artifacts under
the same per-host archive as the C benchmark. That harness is intentionally
separate because it depends on Lua/LuaRocks. Its gates compare Lua lanes with
the frozen Lua baseline; ratios against lonejson's C sibling lanes are informational.

Benchmark output is scoped by a privacy-preserving host id:

```sh
uname -n | md5sum
```

The first field of that command becomes `perflogs/hosts/<host-id>/`. Public
benchmark files store that host id in the JSON `host` field rather than the raw
hostname.

Files:

* `hosts/<host-id>/latest.json` stores the most recent benchmark run as one JSON object and is ignored because it is local run output.
* `hosts/<host-id>/history.jsonl` appends every benchmark run as compact JSONL.
* `hosts/<host-id>/baseline.json` stores a frozen run copied from the last history entry and is committed as that host's comparison baseline.
* `hosts/<host-id>/runs/<timestamp_epoch_ns>.json` stores archived benchmark reports.
* `hosts/<host-id>/lua/latest.json`, `history.jsonl`, `baseline.json`, and `runs/` store the equivalent Lua benchmark artifacts for the same host id.

The benchmark tool reads and writes these files using `lonejson` itself.

The C benchmark harness measures lonejson only. The Lua harness may also report
optional `lua-cjson` one-shot and line-by-line JSONL decode lanes from
`workloadbench.jsonl`. These bounded-memory reference lanes are informational,
excluded from gates, and do not model lonejson streaming or large generated fixtures.

Methodology:

* Each C and Lua benchmark lane records the best of `5` samples. Real code
  regressions affect every sample; taking the best sample keeps intermittent
  scheduler, browser, and CPU-frequency noise from dominating release gates.
* Each C benchmark sample repeats the lane until at least `500 ms` of wall time
  has elapsed.
* Each Lua benchmark sample repeats the lane until at least `500 ms` of wall
  time has elapsed, with longer lanes where the Lua harness configures them.
* The C benchmark target compiles lonejson inline from the packaged single-header artifact (`LONEJSON_IMPLEMENTATION`) so the frozen C baseline stays comparable across single-header-focused releases.
* Fixed parse and stream lanes are reported in two modes:
  * default lanes use lonejson's normal `clear_destination=1` behavior
  * `*_prepared/*` lanes use `clear_destination=0` with caller-prepared fixed destinations
* Many lonejson lanes are schema-guided benchmarks:
  * lonejson parses directly into a known mapped layout instead of building a generic DOM
  * that removes generic key/value allocation and lookup work when the document shape is known up front
  * comparisons against generic decoders should therefore be read as "schema-guided mapped decode" versus "generic decode", not as proof that all parsing modes are equivalent
* The wide multilingual document lanes exist to make that schema-guided advantage visible on a less toy-like shape than the original single-field UTF-8 fixtures.
* `bench-compare` labels deltas under `3%` as `noise`, deltas under `10%` as `small`, and larger deltas as `material`.
* `bench-gate` is the hard check:
  * it requires recorded toolchain provenance and rejects different host,
    compiler, or toolchain identities before comparing throughput
  * it fails if the baseline schema version differs from the latest run
  * it fails if the benchmark configuration differs from the baseline
  * it fails if the baseline is missing any current benchmark lanes
  * it fails on any material negative throughput regression
* `bench-check` writes temporary result files instead
  of updating `latest.json`, then reruns a failed C or Lua benchmark gate
  once into a separate result file before failing. This keeps transient host
  scheduling noise from breaking the full test suite while still requiring
  reproducible material regressions to fail.
* Freeze a new host-specific `baseline.json` whenever the benchmark schema, benchmark case set, measurement method, or benchmark host changes.

After a compiler collection change, measure the prior released implementation
with the same current harness, Bootlin collection, and host as the candidate.
Use that reference measurement as the new baseline; do not relabel historical
measurements or freeze the candidate to erase a regression. Historical records
without toolchain provenance cannot establish a comparable baseline.

The baseline for host `242867651d905848bc891b3cac236c45` was remeasured on
2026-09-07 from the released `v0.42.0` single header, using the current benchmark
harness and its CMake compile flags with Bootlin `stable-2026.08-1` / GCC 15.3.0.
The reference build changed only the harness's lonejson include directory to
the released header; it retained the candidate's compiler, options, and
toolchain metadata. Both runs used 40 iterations and the same fixture corpus.
All 50 cases matched, with no material candidate regressions (one small
regression, 3.7%). The old host-compiler baseline remains in Git history; its
measurements were not relabeled. Other hosts must establish their own measured
Bootlin baseline before comparison.
