# Performance Logs

Generated benchmark artifacts live here.

One bounded command runs C and Lua. Both use the same C aggregation and
regression gates. Workload definitions and correctness checks remain separate
from measurement orchestration.
`bench-check` remeasures the frozen implementation and candidate together; it
does not compare current execution against an old machine-speed snapshot.

Benchmark output is scoped by a privacy-preserving host id:

```sh
uname -n | md5sum
```

The first field of that command becomes `perflogs/hosts/<host-id>/`. Public
benchmark files store that host id in the JSON `host` field rather than the raw
hostname.

Frozen references live at `hosts/<host-id>/baseline.json` and
`hosts/<host-id>/lua/baseline.json`. Raw rounds, final JSON reports, history JSONL
and archives live under the command's printed `build/.../run.*` directory.
Each reference records immutable production Git tree IDs for `src/`, `include/`
and `lua/`. These define the accepted implementation even after its working
branch has been squashed. Keep those objects available in the owning checkout.
The reference build uses its own generated files and dependencies and the current
harness/build inputs. All performance samples are fresh on every check.
`make bench-freeze-baseline` validates both reports before replacing the pair;
previous bytes are preserved in the ignored per-host `runs/` directories.
Historical `latest.json`, `history.jsonl` and `runs/` files remain local evidence.
The report engine reads and writes JSON through lonejson itself.

The C benchmark harness measures lonejson only. The Lua harness may also report
optional `lua-cjson` one-shot and line-by-line JSONL decode lanes from
`workloadbench.jsonl`. These bounded-memory reference lanes are informational,
excluded from gates, and do not model lonejson streaming or large generated fixtures.

Methodology:

* The complete command has one five-minute deadline, covering preparation,
  both implementations when comparing, both runtimes, pilot, workers and reports.
  GNU coreutils `timeout` (`gtimeout` on macOS) owns process-group cancellation.
* At least twenty seconds of excluded whole-suite warmup follow preparation; their raw
  reports remain separate from the recorded samples. One excluded pilot then
  calibrates each case using `bench/workloads.tsv`. Each of 399 fresh
  whole-suite workers reuses those work counts, executes one excluded warmup operation,
  then records one batch per case. Inputs are loaded once per worker.
  Case order rotates and reverses deterministically between rounds.
* Throughput uses the arithmetic mean of normalized wall time per operation
  across the 399 workers of that individual metric. Approximate 95% Student-t
  confidence intervals use the sample standard deviation and 398 degrees of
  freedom; the first and last 133-worker thirds use 132 degrees of freedom.
  The critical values are rounded upward to 1.967 and 1.979 respectively.
  See [NIST's confidence limits for the mean](https://www.itl.nist.gov/div898/handbook/eda/section3/eda352.htm)
  for the calculation and assumptions. Throughput bounds invert these time bounds.
  CPU time remains diagnostic. Different metrics are never averaged together.
  Raw samples, process indices, fixed work counts and correctness results are kept.
  Repeated operations inside a batch are not counted as independent samples.
* Lua settles fixture and preceding-case allocations with a full collection
  before each case, then uses normal automatic GC throughout calibration,
  warmup and measurement. Timed allocating operations retain their normal GC
  costs. GC is never stopped for a recorded batch.
* Linux runs pin workers to the last allowed CPU using `taskset`, avoiding the
  boot CPU's disproportionate interrupt work when the mask permits; set
  `LONEJSON_BENCH_CPU` to select another allowed CPU. Darwin records `unbound`.
  The source revision, compiler, toolchain, host and affinity are recorded.
  Filtered `round` commands use the same worker path as full runs.
  Reports also distinguish single-implementation runs from paired experiments;
  those modes are not directly comparable.
* The C benchmark target compiles lonejson inline from the packaged single-header artifact (`LONEJSON_IMPLEMENTATION`) so the frozen C baseline stays comparable across single-header-focused releases.
* Fixed parse and stream lanes are reported in two modes:
  * default lanes use lonejson's normal `clear_destination=1` behavior
  * `*_prepared/*` lanes use `clear_destination=0` with caller-prepared fixed destinations
* Many lonejson lanes are schema-guided benchmarks:
  * lonejson parses directly into a known mapped layout instead of building a generic DOM
  * that removes generic key/value allocation and lookup work when the document shape is known up front
  * comparisons against generic decoders should therefore be read as "schema-guided mapped decode" versus "generic decode", not as proof that all parsing modes are equivalent
* Fixed-record default-clear lanes release arrays allocated by parsing on every
  iteration. Prepared lanes retain their caller-owned backing storage. Earlier
  fixed-record measurements leaked the default-clear allocations; those archived
  measurements remain unchanged, and current measurements include cleanup.
  After building the benchmark, verify these cases with
  `ctest --test-dir build/host -T MemCheck -L benchmark-memory --output-on-failure`.
* The wide multilingual document lanes exist to make that schema-guided advantage visible on a less toy-like shape than the original single-field UTF-8 fixtures.
* `bench-compare` labels deltas under `3%` as `noise`, deltas under `10%` as `small`, and larger deltas as `material`.
* `bench-gate` is the hard check:
  * it requires recorded toolchain provenance and rejects different host,
    compiler, or toolchain identities before comparing throughput
  * it fails if the baseline schema version differs from the latest run
  * it fails if the benchmark configuration differs from the baseline
  * it fails if the baseline is missing any current benchmark lanes
  * it fails on any material negative throughput regression
* Gates reject incomplete samples, inconsistent summaries, missing lanes,
  nonzero correctness failures and incompatible measurement configurations.
  Mean relative error must be at most 5%. Confidence bounds on the first and
  last thirds also reject a confirmed mean timing drift beyond 5%.
  Every current lower throughput bound must be at least 90% of its reference's
  upper bound. An imprecise estimate, confirmed drift, or bounds crossing that
  regression limit produces an **inconclusive** gate (exit 2).
  Raw coefficient of variation and mean drift are diagnostic values and remain
  in every report; a precise mean does not imply identical individual timings.
  Every sample contributes to these checks; the harness never retries to obtain
  a favorable passing value.
* `bench-check` measures both implementations in C and Lua once, evaluates both gates, and retains the
  original JSONs and sample distributions on success and failure under
  `build/bench-check.*`. It prints that directory instead of changing frozen references. Historical baseline reports identify measurement
  methods and do not compare throughput across method changes.
  A read-only preflight rejects incompatible or imprecise references
  before preparing builds or collecting candidate samples.
* Baseline freezing validates the final history entry, including its samples,
  correctness, precision and drift. It refuses an invalid or imprecise final entry
  instead of selecting an older successful run.
  Uncommitted or unversioned implementations cannot become reproducible references.
* Freeze a new host-specific `baseline.json` whenever the benchmark schema, benchmark case set, measurement method, or benchmark host changes.

After a measurement-method change, remeasure the implementation that established
the accepted performance target with the new harness. After a compiler collection
change, measure the prior released implementation
with the same current harness, Bootlin collection, and host as the candidate.
Use that reference measurement as the new baseline; do not relabel historical
measurements or freeze the candidate to erase a regression. Historical records
without toolchain provenance cannot establish a comparable baseline.

The active baselines for host `4a8bcd8b7084137ee9c5b8ffae2e333d` use the
whole-suite mean method with all 399 samples for each of 50 C and 27 Lua metrics.
They were frozen on 2026-11-12 after a complete paired comparison against the
accepted implementation `794f9855db99507b593e13dfc7ea062e8fdadd88` passed every
correctness, precision, drift and regression gate. Their recorded production
trees define the new performance reference. The old reports remain in Git
history and the host's ignored `runs/` directories; they were not relabeled.
The final comparison used the standalone worker's toolchain-only runtime paths
on CPU 7 and completed in 227 seconds. Of the C metrics, 25 improved by at least
3%, 23 remained within 3%, and corpus validation and raw array pull measured
3.36% and 3.29% slower respectively, passing the unchanged confidence-bound gate.
Of the Lua metrics, 26 improved by at least 3% and one remained within 3%.
These counts describe individual metrics, not an aggregate score.
Only this C/Lua baseline pair is retained in the current tree. Superseded host
pairs remain in Git history. Other hosts must establish their own compatible
measurements before they can support a benchmark or release check.
