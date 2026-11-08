# Building and verification

Commands run from the source checkout. The [local verification contract](local-verification.md)
owns detailed toolchain, test-selection and packaging policy; [release artifacts](releasing.md)
describes the maintainer release workflow.

## Build and check

Use an x86_64 glibc Linux host with the pinned Bootlin collection, or native
Apple Silicon macOS with Xcode or Command Line Tools. Make prepares the selected
tools and dependencies; direct CMake users must first run `make deps-host`.
See [compiler and cross toolchains](local-verification.md#compiler-and-cross-toolchains)
for prerequisites, cache ownership and cross-target support.

| Command | Purpose |
| --- | --- |
| `make build` | Prepare the debug build and `.clangd` compilation database after checkout or cleanup. |
| `make test` | Configure/build current debug artifacts, run debug CTest and the Lua integration suite. |
| `make finalize-slice` | Format/build the core and run focused core, header, ABI and Lua checks. |
| `make test-host-curl` | Native C tests with curl, OpenSSL, JWT and OIDC enabled. |
| `make test-all` | Broad local confidence: native and cross tests, sanitizers, Valgrind, e2e, fuzz and available benchmark gates. |
| `make format-check` | Check maintained C source/header formatting. |
| `make abi-check` | Check header/SONAME agreement and report coordinated downstream upgrades. |

Raw CTest is diagnostic after a matching configure/build; use `make test` as the
[debug lifecycle gate](local-verification.md#debug-gate). Run the matching policy
or packaging fixtures when changing those surfaces: `finalize-slice` deliberately
omits them. Run lifecycle commands serially with stable inputs. Make defaults
to two build jobs, configurable through `LONEJSON_BUILD_JOBS` or
`CMAKE_BUILD_PARALLEL_LEVEL`.

## Local Podman Kube e2e

Install rootless Podman with its `pasta` network helper on the host.
`make test-e2e` starts one checkout-specific pod, waits for HTTPS, OIDC, API-fixture,
and upload-sink readiness, then runs the curl, OIDC, and M2M workflows. The
five containers are declared in `devenv.yaml.in`; static configuration lives
under `devenv/`. Services communicate through pod-local loopback connections.
All published ports bind to host loopback.

`make dev-up`, `dev-ps`, `dev-logs`, `dev-down`, and `dev-reset` use
`scripts/devenv.sh`. The generated manifest is `build/devenv/devenv.yaml`;
state, temporary files, logs, and local credentials are under that same
`build/devenv/` root. The TLS CA is
`build/devenv/credentials/server.crt`. Each image's writer maps to the invoking
host user, and the e2e gate checks actual container-written file ownership.
`dev-reset` stops the pod with the saved manifest before deleting generated
state as that user. `make clean` also stops this pod before removing `build/`.

Published service ports can be overridden for parallel checkouts:
`LONEJSON_OAUTH2_E2E_PORT` (8090), `LONEJSON_OIDC_E2E_PORT` (18443),
`LONEJSON_API_FIXTURE_E2E_PORT` (18080), `LONEJSON_NGINX_HTTP_E2E_PORT` (8080),
and `LONEJSON_NGINX_HTTPS_E2E_PORT` (8443). Overrides must be distinct ports
between 1024 and 65535. Reset the pod before changing ports. On success,
`test-e2e` removes its pod and generated service state. On failure it stops
the pod and retains state for diagnosis. Set
`LONEJSON_E2E_KEEP_DEVSERVICES=1` to retain the pod and state for debugging.
Run `make dev-reset` when finished.

## Downstream ABI coordination

`make abi-check` reports coordinated downstream upgrades when the working ABI
differs from the latest local stable release tag. `make finalize-slice` runs it
automatically. The report is advisory, uses local history without fetching, and
does not inspect or modify downstream checkouts. It is outside `make release`
and `make release-matrix`. Without release history it reports an unknown
comparison and the known upgrade order for manual review.

Upgrade liblockdc and librotmos first; libcai after liblockdc and libailo after
its librotmos dependency; vectis last. All five consume
LoneJSON directly, and vectis also depends on all four libraries. Keep direct
and transitive users on the same LoneJSON ABI. Update the curated report in
`cmake/report_downstream_abi.cmake` when this dependency graph changes.

## Benchmarks

`make bench` and `make lua-bench` measure C and Lua lanes. `make bench-gate` and
`make lua-bench-gate` compare them with frozen baselines for the current host.
Broad gates run `bench-check`, which explicitly skips hosts without baselines.
See [performance logs](https://github.com/c89systems/lonejson/blob/trunk/perflogs/README.md) for methodology, artifact paths
and baseline provenance. When the harness, host or compiler changes, establish
a comparable released reference before freezing a new baseline; do not freeze
the candidate to erase a regression.
