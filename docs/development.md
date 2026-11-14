# Building and verification

Run commands from the source checkout. Make indexes commands, Bash sequences
preparation, CMake owns build/install dependencies, and CTest owns test results.
Python is confined to small fixture servers and data preparation. The
[release guide](releasing.md) owns release identity and packaging policy.

## Build and check

Use CMake 3.25+, Ninja and Bash 4.4+. Make prepares tools and dependencies;
direct CMake users first run `make deps-host` or `make deps-<target>`.

| Command | Purpose |
| --- | --- |
| `make build` | Prepare debug artifacts and the `.clangd` compilation database after checkout or cleanup. |
| `make test` | Configure/build debug artifacts, stage standalone examples and run CTest plus full-feature Lua integration. |
| `make finalize-slice` | Format/build the core and run focused core, header, ABI and Lua checks before committing. |
| `make test-host-curl` | Native C tests with curl, OpenSSL, JWT and OIDC enabled. |
| `make test-all` | Debug, host release, curl/auth, cross, sanitizers, Valgrind, e2e, fuzz and available benchmark gates. |
| `make format-check` | Check maintained C sources and headers without changing them. |
| `make abi-check` | Check header/SONAME agreement and report coordinated downstream upgrades. |
| `make prerelease` | Run native hardening, then build and verify the binary release matrix incrementally. |

`finalize-slice` formats maintained sources before building the core, resumable
visitor, map-cache, short-name and link-test targets. Commit those formatted
sources normally. It omits examples and repository-wide policy fixtures; run
matching lifecycle and packaging fixtures when changing those surfaces.

Run operations serially with stable sources, configuration and prepared tool
collections. The terminal or job runner owns whole-job cancellation. Make defaults
to four compiler jobs; override `LONEJSON_BUILD_JOBS` or
`CMAKE_BUILD_PARALLEL_LEVEL`. Tests remain serial. Copied/restored inputs with
preserved timestamps need explicit invalidation.

Temporary workspaces live under the owning checkout's `build/`. `make clean`
stops its Podman pod and removes generated build, artifact and local cache state,
including hidden entries; it preserves installed prerequisites and shared cpkt
caches. Never borrow another checkout's sources, generated tools, build state or
local caches through overrides or symlinks.

## Compiler and cross toolchains

Native Linux workflows require an x86_64 glibc host. All Linux targets use the
pinned Bootlin stable `2026.08-1` collection (GCC 15.3.0): compiler, linker,
binutils, libc sysroot and target runtime. Checksums and identities are in
`tools/lifecycle/scripts/cpkt-toolchains.sh`; replace the complete collection
when upgrading. Configuration checks the compiler triple and rejects stale
collection caches. After an upgrade use `cmake --fresh --preset <preset>`.

Native Apple Silicon macOS uses the active Apple compiler and SDK from `xcrun`;
install Xcode or Command Line Tools. Native builds do not need osxcross. Linux
uses local osxcross for Darwin artifacts; the project never downloads Apple SDKs.
The resolver chooses the newest complete `arm64-apple-darwin25.x` prefix;
`CPKT_OSXCROSS_HOST` pins one. Compiler, linker, inspection tools and smoke
consumers use that same prefix. `ensure arm64-apple-darwin` also prepares the
checksum-pinned Bootlin host MIG helper, which is never shipped.

`make toolchains-all` warms the Linux collections. The read-only
`scripts/cpkt-toolchains.sh discover` reports all targets without acquisition,
including native Apple or local osxcross/MIG readiness. Direct target toolchain
files inspect prepared tools without downloading, including during probes.
The vendored helper collection under `tools/lifecycle/scripts` is updated as a
unit and passed through compiler probes as `CPKT_LIFECYCLE_SCRIPTS_DIR`.

Toolchains default to `${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/toolchains`.
`CPKT_TOOLCHAIN_CACHE` overrides must select owned `build/` or `.cache/` state
or the declared global cpkt cache. SDK archives use
`${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/deps}`.
Acquisition verifies SHA-256 before extracting into checkout-local `.cache/`.

The core JSON library has no cpkt API dependency. Optional curl/auth features
and Lua use [`c89systems/cpkt` 0.8.0](https://github.com/c89systems/cpkt/releases/tag/v0.8.0),
including curl 8.22.0, OpenSSL 3.6.5 and Lua 5.5.1. Acquisition verifies package
group, version and target. `LONEJSON_CPKT_ROOT` selects its independently
extracted core SDK; do not merge provider prefixes. Neither `cpktdb` nor
`cpktmisc` is required.

`make cross-build` builds the Linux cross presets; `make cross-test` (alias
`test-cross`) executes their QEMU coverage. `make build-release` prepares only
`RELEASE_BUILD_PRESETS`, defaulting to six Linux targets plus Darwin when
osxcross is ready. `make release-matrix` tests runnable targets, packages SDKs
and Lua products, then verifies those bytes. Missing required runners fail.
Darwin cross proof covers build, linking and metadata, not macOS execution.

### Optional native macOS diagnostics

`.github/workflows/macos-native.yml` runs native Apple Silicon diagnostics on the
standard `macos-15` GitHub runner. It is manual (`workflow_dispatch`) only: there
are no push, pull-request, tag, or release triggers. Repository access is read-only,
checkout credentials are not persisted, and no artifacts are uploaded or published.

The job installs its Homebrew prerequisites, selects Bash 4.4+ through PATH, and
uses the active Xcode/Apple tools and SDK with a macOS 15 deployment target. It
records the exact checked-out commit and toolchain, then runs `make test` and
`make test-host-curl` serially. These exercise native Debug and full-feature Release
C/Lua tests; failures remain visible in the Actions logs. Native macOS does not
run the Linux Valgrind or AFL++ gates, or the full performance comparison.

GitHub requires the workflow file on the default branch before manual dispatch
is available. Once it is registered and the selected development ref is already
available remotely, an explicitly authorized run can be started with:

```sh
gh workflow run macos-native.yml --repo github.com/c89systems/lonejson --ref <existing-development-branch>
```

Adding this workflow does not authorize publishing refs to obtain CI feedback.
Track the run's exact source commit and each test step's result; workflow linting
or a queued run is not native execution proof. Hosted results are optional
development diagnostics. The local lifecycle still owns required release tests,
osxcross Darwin artifacts, signing, tagging, and publication.

Run `make build` after checkout or cleanup to publish
`build/debug/compile_commands.json` for `.clangd`, including generated headers
and the Lua module. Check ordinary discovery with
`clangd --check=src/lonejson.c` and `clangd --check=src/lua/lonejson_lua.c`,
without database or driver overrides. In the editor, check public-header
diagnostics and API discovery. Compiler and supported linker warnings are errors
in Debug and Release; editor warnings remain warnings.

## Test scopes and runtime

Use `make test` as the debug completion gate. Raw CTest does not configure or
build and can execute stale artifacts. Focused diagnostics require the matching
configure/build first; report that scope and its preparation commands. On Linux
x86_64, prepare `make toolchains-aflpp` before directly selecting AFL policy
fixtures. The native test Make targets prepare it automatically.

CTest labels host fixtures `host-policy`; matrix and hardening presets exclude
them while retaining target runtime, exports, strict headers, linking, versions
and metadata checks. Linux compiler, SDK/ABI and resolver fixtures register only
on their supported x86_64 host. Portable policy and native Darwin Lua checks
remain available on Darwin. Inventory tests check these boundaries.
Ordinary presets exclude `e2e`. Preserve preset label exclusions: adding another
`-LE` intersects exclusions and can accidentally reselect service tests.

Every non-shipped Linux executable records the selected Bootlin loader and
private transitive RPATH. CTest and e2e execute directly without
`LD_LIBRARY_PATH`. Shipped libraries and Lua modules have explicit export
allowlists; build and extracted-SDK checks reject private linkage, instrumentation
and collection/cache paths. See [artifact verification](releasing.md#artifact-verification).

Linux Lua tests use the target-built embedding runner. Benchmarks load a separate
production module; uservalue, integer and statistics instrumentation stays in
test modules. Native Darwin uses LuaRocks and its native interpreter. Its CTest
setup fixture installs the selected SDK and builds the rock before any selected
Lua test; schema-cache and encode-stat checks build separate instrumented modules.
`LONEJSON_LUA_BUILD_DIR` preserves the selected configuration. Preparation always
installs that SDK and uses current flags; a shared installation stamp cannot
prove a different configuration is installed. ASan/TSan omit Darwin Lua modules
and their fixture because the ordinary interpreter cannot load their runtimes;
compiled sanitizer tests remain selected. Fresh extracted-source checks prepare
their own runtime. See [Lua setup](lua.md) for loader selection and `make lua-env`.

Debug glibc executables set default pthread stacks to 128 KiB, including executed
Debug e2e children. CTest supplies `glibc.pthread.stack_cache_size=0` in
`GLIBC_TUNABLES` so cached larger stacks cannot hide failures; supply that tunable
for direct Debug runs too. Main-thread limits and explicit stack requests stay
unchanged. Tests verify default/nested sizes, explicit stacks, guards that
actually fault, executed children and an unconstrained control. Native musl
default-worker tests remain required. Nested-record tests cover 4,096 maps,
recursive arrays, workspace boundaries, cycles and allocation failure/retry;
Lua interop covers depth 128, limit rejection and callback/allocator errors.
GCC/Clang reject production C/Lua frames over 8 KiB, VLAs and `alloca()`.
Heap-backed test buffers and bounded serializer frames preserve the complete
worker call-chain budget; Debug uses ordinary rather than forced recursive
serializer inlining, while optimized builds retain forced inlining.

`make valgrind` uses host Valgrind with curl, OpenSSL, JWT and OIDC enabled. CTest
Memcheck serially runs main, curl rewind, Lua and fixed-record benchmark ownership
tests with a 30-minute timeout. `make tsan` probes support after resolving the
native collection; preparation cannot silently turn a required gate into a skip.

`make fuzz-smoke` uses pinned Bootlin/AFL++ GCC-plugin instrumentation, a fixed
seed and 1,000 executions per target; AFL calibration/budget boundaries can
increase the final count. The one-second input timeout detects hangs. `make fuzz`
and `make fuzz-long` retain duration budgets. The fuzz build selects the static
library and all 16 fuzz targets and seed corpora. Its 18-case CTest suite includes
instrumentation checks and real engine success/crash/hang controls.
Saved crashes or hangs fail even if AFL exits successfully; inspect
`build/fuzz/afl/<target>/output/` and replay retained inputs with the same binary.
Drivers disable only their own Linux dumpability to avoid core-collector delays;
signals still identify crashes. No root/sysctl changes are needed. AFL cache
identity includes the Bootlin collection. Portable Make-surface assertions run
everywhere; real AFL checks run on Linux x86_64. Lua correctness and isolated
fuzz checks use CTest and the selected runtime.

## Local Podman Kube e2e

Install rootless Podman and `pasta`. `make test-e2e` starts one checkout-specific
pod, waits for HTTPS, OIDC, API-fixture and upload-sink readiness, prepares native
fixture executables once and runs four CTest cases. The five containers come
from `devenv.yaml.in` and `devenv/`; service traffic uses pod-local loopback,
published ports bind to host loopback, and ownership checks verify container
writers map to the invoking host user.

`make dev-up`, `dev-ps`, `dev-logs`, `dev-down` and `dev-reset` use
`scripts/devenv.sh`. The generated manifest, state, scratch, logs and credentials
live under `build/devenv/`; its CA is `credentials/server.crt`. `dev-reset` stops
the pod with its saved manifest before deleting state. E2e success removes pod
and state; failure stops the pod and retains diagnostics.
`LONEJSON_E2E_KEEP_DEVSERVICES=1` retains both for debugging; reset when finished.

Override ports for independent checkouts with `LONEJSON_OAUTH2_E2E_PORT` (8090),
`LONEJSON_OIDC_E2E_PORT` (18443), `LONEJSON_API_FIXTURE_E2E_PORT` (18080),
`LONEJSON_NGINX_HTTP_E2E_PORT` (8080) and `LONEJSON_NGINX_HTTPS_E2E_PORT` (8443).
Use distinct ports from 1024–65535 and reset before changing them.

M2M reserves a kernel-assigned loopback port, publishes readiness only after
listening and stops through an explicit shutdown request. Its private state is
under `build/devenv/`. `LONEJSON_M2M_E2E_SERVER_PORT` requests a fixed debug port;
occupation fails with a diagnostic. Tests cover isolation, startup failure,
extra health probes, fragmented requests and orderly shutdown.

## Downstream ABI coordination

`make abi-check` and `finalize-slice` compare header/SONAME ABI with the latest
local stable release and report downstream rebuild order. The advisory report
does not fetch, inspect downstream checkouts or participate in release/matrix
gates. Missing history reports an unknown comparison and the known upgrade order.

Upgrade liblockdc and librotmos first, libcai after liblockdc, libailo after
librotmos, and vectis last. All five directly consume LoneJSON; vectis also
depends on the other four. Keep direct/transitive users on one ABI. Maintain
the curated graph in `cmake/report_downstream_abi.cmake`.

## Benchmarks

`make bench` measures all C and Lua workloads together. `make bench-check` rebuilds
and remeasures the frozen implementation alongside the candidate, using the same
current harness. Saved timing values remain historical evidence; the source trees
define the performance reference. The Lua-named targets use this same
invocation. Normal `prerelease` and `release` do not run performance measurements;
`prerelease-hardening` and `test-all` explicitly run `bench-check`.
Keep sources and build inputs unchanged throughout the command. Source identity
is resolved once before workers so both implementations have the same launch path.

The complete command has a five-minute deadline, including preparation, pilot,
all workers, aggregation and gates. It requires GNU coreutils `timeout` (`gtimeout`
on macOS). A deadline failure never publishes a complete report or freezes a baseline.
At least twenty seconds of excluded whole-suite warmup follow preparation,
settling the transition from compilation to sustained benchmark work. Its reports are
retained separately. An excluded pilot then warms each case before calibrating its work count with process
CPU time and the fixed budgets in `bench/workloads.tsv`. Scheduling pauses during
calibration therefore cannot shrink later batches. Recorded samples still measure
wall time, including scheduling pauses. Each of 399 fresh
whole-suite workers reuses that fixed work count, warms each case with one excluded operation, then
records one batch. All observations and CPU/wall times are retained. Lua uses
normal automatic GC during measurement; a full collection before each case settles
fixture allocations and preceding-case debt. C performs the shared aggregation,
validation and gates. Case reversal and implementation order use separate cycles.
The flat case table assigns 1.5–8 ms according to measured workload variability.
It reserves about 155 seconds of recorded work for a full two-implementation
comparison, leaving more than 144 seconds for preparation, warmup, startup and reporting.
Every report records each case budget; a budget change requires remeasured references.

Each metric reports arithmetic mean wall time per operation over its 399 workers,
with approximate 95% Student-t confidence intervals. No different workloads are
averaged together. See [NIST's confidence limits for the mean](https://www.itl.nist.gov/div898/handbook/eda/section3/eda352.htm)
for the standard-error calculation and its assumptions. Intervals wider than 5%,
confirmed drift beyond 5%, and comparisons whose bounds cross the existing 10%
regression limit are inconclusive. There are no automatic measurement retries.

Reports and raw rounds are retained in unique `build/benchmark/run.*` or
`build/bench-check.*/run.*` directories. `make bench-freeze-baseline` measures and
validates both reports before replacing either host baseline, preserving the old
bytes under the host's ignored `runs/` directories.

To compare a prepared accepted source checkout with the current implementation:

```sh
LONEJSON_BENCH_REFERENCE_REVISION=<accepted-commit> ./scripts/benchmark.sh compare "$PWD" "$PWD/build/benchmark-comparison" <reference-source>
```

Both checkouts must contain the same harness and their own dependency/build state.
Reference and candidate whole-suite workers alternate order between rounds. The
single five-minute deadline covers both implementations, C and Lua together.
The standalone C worker embeds only the selected toolchain's runtime directories.
Unused checkout/cache search paths change loader allocations and can bias allocating
workloads even when the compiled instructions are identical.
Normal `bench-check` reconstructs the reference in an owned `build/` directory
from recorded Git trees for `src/`, `include/` and `lua/`, then applies the current
harness and build inputs. It requires those Git objects in the owning checkout.
The frozen source trees survive a squash when the accepted implementation is
landed; an unavailable tree fails explicitly. Build artifacts are incremental;
all 399 measurements of every metric are collected anew. Reports record whether
one or two implementations ran, and gates reject comparisons between those modes.
Baseline freezing requires committed, versioned sources.

See [performance logs](https://github.com/c89systems/lonejson/blob/trunk/perflogs/README.md) for methodology, paths and baseline
provenance. After a measurement-method change, remeasure the implementation behind
the accepted baseline. After a compiler change, remeasure a comparable prior
release with the current harness and compiler before freezing a replacement.
Do not freeze the candidate to erase a regression. Candidate version overrides
and the release pipeline are described in the [release guide](releasing.md).
