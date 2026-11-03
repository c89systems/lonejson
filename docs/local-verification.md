# Local Verification

`make prerelease` runs ordinary proof and the binary release matrix incrementally.
`make release` first validates a clean owning checkout attached to its default
branch, a verified commit signature, exactly one lightweight `vX.Y.Z` tag on
HEAD, and no candidate override, cleans generated
state, and runs that proof graph. It then reconstructs the all-component source
archive and verifies the complete checksum manifest. Source reconstruction stays
out of the incremental binary matrix. The extracted archive runs host C and Lua
tests and builds its Lua release artifacts. Ordinary source-package checks use
its `VERSION` and `RELEASE_MANIFEST`, even inside a Git checkout, without changing
the parent checkout's tags.

Make indexes commands, Bash sequences preparation, CMake owns build/install
dependencies, and CTest owns test execution and results. Python is confined to
small fixture servers and data preparation. Run one operation per mutable
workspace with stable sources, configuration, and prepared tool collections.
The terminal or job runner cancels the whole job/process group. Copied or
restored inputs with preserved timestamps require explicit invalidation.

The release pipeline runs native debug/Lua tests, sanitizers, Valgrind,
deterministic e2e, fuzz smoke, and benchmark checks before the binary matrix.
The matrix owns host release and cross-target testing once per configuration;
it does not repeat a preceding `test-cross` run. `test-all` remains a standalone
broad confidence gate. `prerelease-hardening` is an alias for `prerelease`, since
that graph already includes the native hardening and benchmark gates.

For routine edits, `make finalize-slice` formats and builds only the core,
map-cache, short-name and link-test targets, runs core/header/ABI checks and the
Lua suite, then checks formatting. It reuses configured builds and does not
stage examples or run the repository-wide policy fixtures. Run the matching
lifecycle or packaging fixtures separately when changing those surfaces.

`make lifecycle-version-contract` is read-only. It checks final release identity
before any cleanup or producer runs. CTest uses disposable Git command fixtures
to test tag identity and failures without changing checkout refs. Candidate
version overrides remain available to preparation commands.

Implement and verify on one development branch. For an actual release, squash
the verified change onto the local default branch before requesting final
amendment/signing. Set `origin/HEAD` to the authoritative default branch. Sign
that commit, add its exact local lightweight tag, and run the clean release
gate. Keep remote branch and tag refs unchanged until all required local tests
and final artifact checks pass. Hosted CI never supplies release proof or
artifacts; Darwin proof uses local osxcross. A failed release stops for repair
in a separately authorized iteration.

Temporary test and verifier workspaces live under `build/`, resolved from each
script's physical repository location. `make format-check` checks all maintained
C sources and headers without changing them. Shared libraries and Lua modules
use explicit export allowlists; build and extracted-SDK checks compare exact
symbol tables with target tools and reject private downstream linkage.

## Compiler and Cross Toolchains

On native Apple Silicon macOS, plain CMake, native presets, the Darwin release
preset, curl examples, and LuaRocks use the active Apple compiler and macOS SDK
reported by `xcrun`. Install Xcode or Command Line Tools first. Native builds
do not require osxcross. Linux hosts use osxcross for Darwin artifacts and
Bootlin for Linux targets; Apple SDKs are never downloaded by the project.
`make build-release` prepares dependencies only for `RELEASE_BUILD_PRESETS`.
Without a prepared osxcross collection, the default selection builds the six
Linux targets without attempting Darwin preparation.
The native preset toolchain is now `cmake/toolchains/native.cmake`; refresh an
existing build cache with `cmake --fresh --preset <preset>` after updating.

Linux collections are pinned in `scripts/cpkt-toolchains.sh` to Bootlin stable
`2026.08-1` (GCC 15.3.0), including archive SHA-256 checksums. Updates replace
the complete collection, not just the compiler. AFL++ cache identities include
the Bootlin collection so upgrades cannot reuse an incompatible GCC plugin.
Standalone curl examples, LuaRocks bindings (including source rocks), and
compiler-using test fixtures resolve the same pinned collection. Linux LuaRocks
builds override its compiler setting; a host compiler is never a fallback.
After a Bootlin upgrade, configure existing build directories with
`cmake --fresh --preset <preset>` before building. Configuration rejects an old
collection cache because CMake's automatic compiler-change restart can discard
preset feature settings and silently reduce test coverage.

CMake selects the pinned Bootlin collection matching the native Linux host's
processor and libc for debug, host, and Lua workflows. Explicit Linux target
toolchain files inspect prepared tools without acquisition, including compiler
probes. Make entrypoints prepare their dependencies before configuring; direct
CMake users must first run `make deps-host` or the selected `make deps-<target>`.
To warm every Linux
collection before a matrix build:

```sh
make toolchains-all
```

The default cache is
`${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/toolchains`; set
`CPKT_TOOLCHAIN_CACHE` to use an owned `build/` or `.cache/` directory or the
declared global cpkt cache. Overrides and symlinks must not borrow another
checkout's generated state. The exact lifecycle helpers are vendored together
under `tools/lifecycle/scripts`; CMake propagates that location through
`CPKT_LIFECYCLE_SCRIPTS_DIR`. Host Bash 4.4 or newer is required.

`scripts/cpkt-toolchains.sh discover` reports every lifecycle target without
downloading it. On macOS, `discover arm64-apple-darwin` reports native Apple
tools; on Linux it reports local osxcross and pinned host MIG readiness.

Use `make cross-build` to configure/build the Linux cross presets and `make
cross-test` (or the compatibility name `make test-cross`) to execute their
QEMU-backed test coverage. `make package-single-header` creates the separate
version-stamped single-header artifact without running a full release.

`make release-matrix` repeats the runnable QEMU-backed cross coverage while
building and verifying binary SDKs and Lua artifacts; host-only LuaRocks/tooling
checks remain native. A missing required runner is a failure, not a skip.

The core JSON library has no cpkt API dependency. The optional curl/auth
features and Lua binding/tests use the upstream libraries supplied by
[`c89systems/cpkt` 0.6.1](https://github.com/c89systems/cpkt/releases/tag/v0.6.1),
including curl 8.22.0, OpenSSL 3.6.5, and Lua 5.5.1. No `cpktdb` or `cpktmisc`
provider is needed. `LONEJSON_CPKT_ROOT` selects the independently extracted core
SDK; do not merge provider prefixes. Acquisition checks the archive SHA-256 and
the core package manifest's group, version and target.

Pinned cpkt SDK archives are separate immutable cache entries under
`${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/deps}`.
`make deps-host` and the target-specific dependency targets verify each archive
by SHA-256 before reuse, then extract it into the checkout-local `.cache/`
tree. `make clean` removes only that extracted local state and preserves the
shared archive and toolchain caches.

Run `make build` once after checkout or full cleanup to prepare dependency and
generated headers and publish `build/debug/compile_commands.json`, the location
used by `.clangd`. It includes the target Lua module as well as C sources. Clangd
uses the owning compilation commands' standards and include paths. For a local
discovery check, run `clangd --check=src/lonejson.c` and
`clangd --check=src/lua/lonejson_lua.c` without database or driver overrides.
Open `include/lonejson.h` in the ordinary clangd editor and verify empty
diagnostics and API symbol discovery. Editor warnings remain warnings;
builds still enforce `-Werror`. Refresh
old configurations with `cmake --fresh --preset <preset>` after this migration:
the provider and global cache paths changed.

Every Linux native debug, host, and release build uses a single pinned Bootlin collection:
its GCC driver, GNU linker, binutils/debug tools, libc sysroot, and target
runtime. Configuration verifies the compiler triple; package inspection reads
the configured target tools from the same collection. Native memory checking
uses host-installed Valgrind; fuzzing uses a cached, pinned AFL++ GCC-plugin
build tied to the Bootlin x86_64 collection. Valgrind is not a
compiler-based uninitialized-memory analysis, but it provides the native leak
and invalid-memory gate without a non-portable LLVM distribution.
ThreadSanitizer support is probed by `make tsan` after resolving the selected
native Bootlin target, so broad gates do not make a parse-time skip decision
before the toolchain exists.

Every non-shipped Linux executable built by CMake—tests, fixture servers,
examples, benchmarks, and fuzzers—also records that collection's ELF loader
and private transitive `RPATH`. CTest and e2e scripts execute them directly;
they do not set `LD_LIBRARY_PATH`. Linux Lua behavior runs through the
embedded Lua runner linked against the selected bundle. Benchmarks load a
separate production-configured module; test-only uservalue, integer, and
statistics definitions remain confined to the test module. Native Darwin Lua
tests and benchmarks use its native LuaRocks runtime and the host build;
source-rock packaging remains host-side tooling.
On native Darwin, CTest prepares the installed SDK and Lua rock with a fixture
before any selected Lua or Lua benchmark test loads it. This also applies to
direct CTest selections. Schema-cache and encode-stat checks run as separate
tests that build their instrumented Lua modules against that installed SDK.
The fixture reuses its configured build through `LONEJSON_LUA_BUILD_DIR`;
it does not reconfigure the active test build with the debug preset.
Lua preparation always builds and installs that selected library and runs
LuaRocks with the current flags. A shared installation stamp cannot establish
that a different configuration is installed. CMake still reuses unchanged
library outputs through its normal dependency graph. CTest owns Darwin Lua
preparation; the Make Lua test targets do not prepare it a second time.
The benchmark tests also consume that prepared SDK without rebuilding it or
selecting the debug library. Native Darwin ASan and TSan configurations omit
these Lua module tests and their setup fixture: the ordinary Lua interpreter
cannot load their sanitizer runtime safely. Compiled C sanitizer tests remain
selected.
The source-archive check invokes the benchmark script directly in a fresh
extracted tree. On Darwin that caller prepares its own Lua SDK and rock before
the benchmark; it does not depend on the parent checkout's CTest fixture.

`make package-verify` rejects every checksum-listed artifact if a packaged
library, archive, or consumer metadata contains a Bootlin cache or stable
collection path. Development interpreter and RPATH settings are therefore
confined to local executables and cannot enter a release upload.

Fuzzing requires no root privileges or host sysctl changes. The native input
driver disables its own Linux dumpability before processing each input, which
prevents Apport or other piped core collectors from delaying crash reporting.
The runner checks that driver contract before bypassing AFL++'s global
core-pattern warning. Signals still identify crashes; AFL++ retains the input
instead of producing a core dump. Replay a retained input by passing its path
to the same fuzz executable. This protection applies to input execution, not
faults in the loader before the driver starts.

`make fuzz-smoke` uses a fixed RNG seed and a 1000-execution AFL++ budget per
target instead of a one-second race against host speed. AFL++ performs seed
calibration and checks the budget at its own boundaries, so the final execution
count may exceed the budget. A fixed one-second per-input timeout distinguishes
hangs; this is a failure bound, not a required completion delay. Standard
`make fuzz` and `make fuzz-long` retain their duration budgets. These are fuzz
campaigns, not a promise of bit-identical mutations across hosts.

Every fuzz gate first tests the real AFL++ engine against prescribed successful,
SIGSEGV, SIGABRT, and indefinitely blocked inputs. A prescribed mutation proves
that a discovered crash is retained unchanged and causes the runner to fail.
Crashing or hanging initial seeds also fail and remain in the staged seed set.
Saved crashes and hangs fail the gate even when AFL++ itself exits successfully; findings remain
under `build/fuzz/afl/<target>/output/` for investigation.

## Debug Gate

Use `make test` as the debug lifecycle gate.

The native runtime suite includes `lonejson_worker_stack_tests`, which serializes
nested records on a worker created with the platform's default thread attributes.
Its document and output buffers live on the heap. The musl run checks the default
128 KiB worker stack; unoptimized builds use ordinary inlining to avoid expanding
recursive serializer frames. Optimized builds retain the existing forced inlining.

`make test` is intentionally broader than raw CTest: it configures and builds
the debug tree, stages standalone examples, runs the debug CTest preset, and
then runs the full-feature Lua integration test through `make lua-test` on
Linux. Darwin Lua coverage is already part of the debug CTest selection.
On Linux x86_64, `make test`, `make test-host` and `make test-host-curl`
prepare the pinned AFL++ collection before CTest, because their policy suites
configure the real fuzz inventory and compiler checks. Preparation failures stop
the gate. Darwin and other Linux architectures do not acquire AFL++ for these
test targets; ordinary build targets require only their normal dependencies.

Do not use `ctest --preset debug` as a completion, pre-release, release, or
status gate. Raw CTest does not configure or build the tree, so it can report
results from stale or manually reconfigured artifacts. That is diagnostic
output, not lifecycle evidence.

Raw CTest is acceptable only for focused diagnosis after the matching debug
configuration and build have already been run in the same flow. On Linux x86_64,
run `make toolchains-aflpp` before selecting AFL policy checks directly.
Reports that use raw CTest as evidence must include the configure/build command
that made the artifacts current, for example:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

For normal local confidence and completion reports, use:

```sh
make test
```

## Broader Confidence

`make test-all` is the deterministic broad local confidence gate. It runs the
debug gate, host release tests, curl/auth host tests, cross target tests,
host sanitizers where supported, Valgrind, deterministic local e2e, and fuzz
smoke.

For release-candidate rehearsals, `LONEJSON_VERSION_OVERRIDE=X.Y.Z` is accepted
by `scripts/release_version.sh`, Make, and CMake. A CMake environment override
is intentionally one-shot and is not written into `CMakeCache.txt`; use
`-D LONEJSON_VERSION_OVERRIDE=X.Y.Z` only when the build directory itself should
retain that override.

`test-all`, `prerelease`, and `release` run `bench-check`
against frozen C and Lua baselines for the current host. Missing host baselines
produce an explicit skip rather than an invented comparison. Use `bench-gate`
or `lua-bench-gate` for focused performance verification.

For a focused test while iterating on a narrow change, build the required
targets first and state that the result is focused diagnostic coverage, not the
debug lifecycle gate:

```sh
cmake --build --preset debug --target lonejson_tests
ctest --test-dir build/debug -R '<test-regex>' --output-on-failure
```

Linux source-rock validation installs the just-built public C SDK under
`build/lua-sdk`, compiles the module against its installed headers, and runs
its Lua correctness/fuzz tests through a target-built embedding runner. Host
Lua and LuaRocks provide tooling. Compiler and supported linker warnings are
errors in Debug and Release configurations.

`make lua-env` prints LuaRocks search paths and the installed SDK's
`LONEJSON_LIBDIR`; it leaves loader selection to the target runner. To execute
a Lua script against the installed rock on Linux, use:

```sh
scripts/run_installed_lua.sh "$PWD/build/luarocks" "$PWD/build/lua-sdk/lib" lua path/to/script.lua
```

LuaRocks builds keep their tool scratch beneath `build/` too.


Darwin cross builds select the newest complete installed `arm64-apple-darwin25.x`
prefix through the shared resolver. Set `CPKT_OSXCROSS_HOST` to pin an exact
installed prefix. Configure, packaging, smoke consumers, and verification use
the selected compiler and sibling tools. The lifecycle's Linux-to-Darwin
collection also requires its checksum-pinned, Bootlin-built host MIG helper;
`scripts/cpkt-toolchains.sh ensure arm64-apple-darwin` prepares it. The helper
is build machinery and is never shipped with lonejson.

`make package` builds and verifies packages incrementally. `make release` owns
clean reconstruction and the full final gate. `make package-source-smoke` (also
`make release-source-smoke`) validates reconstruction from the source archive
with only the source artifact in the parent checkout. `make release` generates
the complete checksum manifest after source smoke and before final package
verification. Native `make valgrind` enables
curl, OpenSSL, JWT and OIDC and runs the main, curl rewind, and Lua tests through
CTest Memcheck serially with a 30 minute timeout. Binary SDK manifests record
curl and OpenSSL as external optional facade requirements; core libraries do
not embed either dependency or require them for ordinary linking.

Lua source products share one public rockspec whose HTTPS source URL names the
release's `lonejson-lua-<version>.tar.gz` asset and its extracted directory.
The source rock embeds that same rockspec and archive, allowing offline
installation. Source staging remains under `build/` for incremental reuse;
`make release-lua-artifacts` verifies the rock against a freshly prepared SDK
on each invocation. Empty and local source URLs fail verification.

CMake packaging validates physical ownership of each staging workspace before
cleanup. The default `dist/` must be a real directory; a symlink is rejected.
Explicit custom artifact destinations retain their ownership-marker policy.
Completed artifacts are copied to their final destinations, including those
on another filesystem; packaging intermediates remain under `build/`.
Tree archives use sorted entries, fixed 1980 UTC timestamps, normalized modes
and numeric root ownership in tar headers. ZIP products omit workstation extra
fields. Executable files, symlinks and empty directories retain their intended
behavior. Normalization uses a disposable staging copy so unchanged Make
prerequisites remain reusable. Linux static-library stripping also normalizes
internal archive headers. Source rocks normalize their two embedded files.
For a full upload in a custom directory, pass its checksum manifest to
`scripts/package-verify.sh`; all verification stages use the manifest's directory
as the payload directory. Binary preparation manifests remain under `build/`
and refer to the checkout's `dist/` payload by default. For a custom binary
payload, use `LONEJSON_DIST_DIR=/absolute/artifacts scripts/package-verify.sh --binary`
or `make package-verify-binary DIST_DIR=/absolute/artifacts` with the matching
binary manifest already prepared. Full cleanup removes the checkout's default
`dist/` as well as an explicitly selected custom directory; `--dist-only` removes
only its selected directory.

## Artifact inventories and selected test scopes

Binary preparation preserves existing `dist/` files and writes its explicit
required inventory to `build/packaging/checksums/lonejson-<version>-binary-CHECKSUMS`.
`make package-verify-binary` verifies those bytes, including extracted consumers,
without claiming source-archive or full upload proof. `make package-checksums`
requires all selected SDKs, Lua products, the standalone header, and the source
archive before replacing the complete upload manifest in `dist/`.
Make supplies its current `LONEJSON_RELEASE_TARGETS` as an explicit CMake
configuration argument, so a warm build cannot retain a previous Make selection
or stale optional Darwin availability. Direct CMake callers can select their
inventory with `-DLONEJSON_RELEASE_TARGETS=...`.
Direct `scripts/package-verify.sh` calls default to all six Linux targets plus
Darwin when the same read-only toolchain readiness check used by Make succeeds.
Set `LONEJSON_RELEASE_TARGETS` explicitly to verify a different selected inventory;
an explicitly empty selection is rejected.
Verification rejects missing entries, unexpected entries, and changed bytes.
Final full verification also rejects unlisted release-looking files.

CTest labels host policy fixtures `host-policy`; matrix and hardening selections
exclude that label while retaining target runtime, exports, strict headers,
linking, versions, and metadata checks. Native Lua workflow fixtures require the
prepared target runtime and fail if it is missing. They are not counted as skips.
The Bootlin compiler, Linux SDK/ABI and Linux resolver fixtures are registered
only on their supported Linux x86_64 provisioning host. Portable policy and
native Darwin Lua checks remain available on Darwin; a CTest inventory fixture
checks these platform boundaries without starting an incompatible compiler.

Ordinary test presets exclude the `e2e` label. `make test-e2e` starts and probes
the owned Podman pod, prepares the native fixture executables once, and runs the
four cases in `ctest --preset e2e`; it tears down resources it started unless
retained-service debugging was selected. Individual e2e Make targets select the
corresponding registered case. Preset selection regression tests exercise real
CTest filtering through the matrix, cross-test and native sanitizer commands,
including separation of host policy and target runtime checks. Those commands
use the presets' label exclusions without adding a narrower `-LE` expression:
CTest combines repeated label exclusions by intersection, which would allow
service tests back into the selection after service teardown.

The portable fuzz Make-surface assertions run on every supported host. Real
AFL configuration, inventory and compiler-policy checks run on Linux x86_64,
where the declared AFL collection is supported.
The fuzz CMake graph declares every maintained executable and seed corpus.
`lonejson_fuzz_targets` builds the required static library and fuzz executables
without compiling an unused instrumented shared library. The fuzz CTest preset
owns all 16 cases and its instrumentation/crash-handling prerequisites. Smoke
uses a fixed seed and 1000 executions; longer commands retain the declared
duration per case. Lua integration and isolated fuzz checks also run through
CTest, using the Bootlin embedding runner on Linux and prepared LuaRocks tooling
on native Darwin.
