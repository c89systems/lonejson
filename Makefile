.NOTPARALLEL:

SHELL := bash
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules
LONEJSON_BUILD_JOBS ?= 2
export CMAKE_BUILD_PARALLEL_LEVEL ?= $(LONEJSON_BUILD_JOBS)
export CTEST_PARALLEL_LEVEL := 1

DEBUG_PRESET := debug
HOST_PRESET := host
ASAN_PRESET := asan
TSAN_PRESET := tsan
VALGRIND_PRESET := valgrind
FUZZ_PRESET := fuzz
TIME_STEP := ./scripts/time_step.sh
LONEJSON_HAVE_TSAN ?=
LONEJSON_E2E_SERVICES_READY ?= 0
RELEASE_BUILD_PRESETS := \
	x86_64-linux-gnu-release \
	x86_64-linux-musl-release \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release \
	$(shell if ./scripts/osxcross_available.sh >/dev/null 2>&1; then printf '%s' arm64-apple-darwin-release; fi)
empty :=
space := $(empty) $(empty)
export LONEJSON_RELEASE_TARGETS := $(subst $(space),;,$(patsubst %-release,%,$(strip $(RELEASE_BUILD_PRESETS))))
LONEJSON_CMAKE_RELEASE_ARGS := "-DLONEJSON_RELEASE_TARGETS=$(LONEJSON_RELEASE_TARGETS)"
CROSS_RELEASE_PRESETS := \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release
LUA ?= $(shell ./scripts/resolve_lua55.sh 2>/dev/null)
LUAROCKS ?= luarocks
LONEJSON_HOST_OS ?= $(shell uname -s)
ifeq ($(LONEJSON_HOST_OS),Linux)
LUA_BENCH_BUILD_DIR := $(CURDIR)/build/host-curl
LUA_BENCH_PREPARE := lua-target-runner
else
LUA_BENCH_BUILD_DIR := $(CURDIR)/build/$(HOST_PRESET)
LUA_BENCH_PREPARE := lua-host-runtime
endif
LUA_BENCH_RUN = LUA="$(LUA)" LUAROCKS="$(LUAROCKS)" LUA_ROCK_TREE="$(CURDIR)/$(LUA_ROCK_TREE)" LONEJSON_LUA_BENCH_LIBDIR="$(LONEJSON_LUA_BENCH_LIBDIR)" "$(CURDIR)/scripts/run_lua_benchmark.sh" "$(CURDIR)" "$(LUA_BENCH_BUILD_DIR)"
GENERATED_FIXTURE_DIR := $(CURDIR)/build/generated/fixtures
ifneq ($(LONEJSON_VERSION_OVERRIDE),)
export LONEJSON_VERSION_OVERRIDE
endif
RELEASE_VERSION := $(shell ./scripts/release_version.sh)
ifeq ($(strip $(RELEASE_VERSION)),)
ifneq ($(LONEJSON_VERSION_OVERRIDE),)
$(error invalid LONEJSON_VERSION_OVERRIDE value: $(LONEJSON_VERSION_OVERRIDE))
else
$(error failed to resolve release version)
endif
endif
DIST_DIR := $(CURDIR)/dist
RELEASE_SOURCE_TARBALL := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz
RELEASE_HEADER_GZ := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).h.gz
RELEASE_ROCKSPEC := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_PACK_DIR := $(CURDIR)/build/packaging/lua
RELEASE_PACK_STAGE_DIR := $(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION)
RELEASE_LUA_SOURCE_TARBALL := $(DIST_DIR)/lonejson-lua-$(RELEASE_VERSION).tar.gz
RELEASE_LUA_SOURCE_URL := https://github.com/c89systems/lonejson/releases/download/v$(RELEASE_VERSION)/$(notdir $(RELEASE_LUA_SOURCE_TARBALL))
RELEASE_ROCK := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.src.rock
RELEASE_CHECKSUMS := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-CHECKSUMS
PERF_CORPUS := tests/fixtures/vendor/json_test_suite/test_parsing
PERF_HOST_ID ?= $(shell ./scripts/bench_host_id.sh 2>/dev/null || printf unknown)
PERF_HOST_DIR := $(CURDIR)/perflogs/hosts/$(PERF_HOST_ID)
PERF_LATEST := $(PERF_HOST_DIR)/latest.json
PERF_HISTORY := $(PERF_HOST_DIR)/history.jsonl
PERF_BASELINE := $(PERF_HOST_DIR)/baseline.json
PERF_ARCHIVE_DIR := $(PERF_HOST_DIR)/runs
LUA_PERF_LATEST := $(PERF_HOST_DIR)/lua/latest.json
LUA_PERF_HISTORY := $(PERF_HOST_DIR)/lua/history.jsonl
LUA_PERF_BASELINE := $(PERF_HOST_DIR)/lua/baseline.json
LUA_PERF_ARCHIVE_DIR := $(PERF_HOST_DIR)/lua/runs
PERF_ITERATIONS ?= 40
LUA_PERF_ITERATIONS ?= 30
FUZZ_TIME ?= 30
FUZZ_LONG_TIME ?= 300
LUA_ROCK_TREE := build/luarocks
LUA_ROCKSPEC := $(LUA_ROCK_TREE)/lonejson-$(RELEASE_VERSION)-1.rockspec
LUA_ROCK_EXTRA_CFLAGS ?= -std=gnu89 -O3 -DNDEBUG -D_FILE_OFFSET_BITS=64 -fno-semantic-interposition
LONEJSON_LUA_LIBDIR ?= $(CURDIR)/build/lua-sdk/lib
LONEJSON_LUA_BENCH_LIBDIR ?= $(CURDIR)/build/$(HOST_PRESET)
LUA_ROCK_BUILD_BYPRODUCTS := \
	$(CURDIR)/lonejson \
	$(CURDIR)/src/lua/lonejson_lua.o
LUA_ROCK_SOURCES := \
	README.md \
	$(wildcard docs/*.md) \
	lonejson.rockspec.in \
	scripts/build_lua_rock.sh \
	scripts/cpkt-toolchains.sh \
	scripts/cpkt-native-apple.sh \
	scripts/require_cache_path.sh \
	tools/lifecycle/scripts/require-host-bash.sh \
	tools/lifecycle/scripts/cpkt-archive-cache.sh \
	tools/lifecycle/scripts/cpkt-toolchains.sh \
	tools/lifecycle/scripts/cpkt-aflpp.sh \
	tools/lifecycle/scripts/cpkt-afl-runtime.sh \
	scripts/deps.sh \
	cmake/fetch_cpkt.cmake \
	cmake/CpktDependencyCache.cmake \
	cmake/cpkt_metadata.cmake \
	cmake/lonejson_lua.exports \
	cmake/lonejson.exports \
	scripts/check_library_exports.sh \
	scripts/detect_native_bootlin_target.sh \
	cmake/toolchains/print_native_bootlin_target.cmake \
	cmake/toolchains/lonejson_native_bootlin_target.cmake \
	scripts/package_lua_src_rock.sh \
	scripts/render_release_rockspec.sh \
	scripts/release_version.sh \
	scripts/stage_lua_rock_sources.sh \
	include/lonejson.h \
	src/lua/lonejson_lua.c \
	src/lua/lonejson_lua.h \
	$(wildcard src/lua/*.inc.h) \
	lua/lonejson/init.lua

SANITIZER_CTEST_EXCLUDE := ^lonejson_lua_(target|fuzz|interop|target_runner_argument|benchmark_configuration)_tests$$
SLICE_BUILD_TARGETS := lonejson_tests lonejson_value_visitor_stream_tests lonejson_map_cache_no_tls_tests lonejson_short_names_tests lonejson_short_names_disabled_tests lonejson_static_link_tests lonejson_shared_link_tests
SLICE_CTEST_INCLUDE := ^lonejson_(tests|value_visitor_stream_tests|map_cache_no_tls_tests|short_names_tests|short_names_disabled_tests|static_link_tests|shared_link_tests|single_header_version_tests|single_header_release_version_tests|library_exports_tests)$$

.PHONY: \
	help \
	finalize-slice \
	abi-check \
	build \
	build-debug \
	build-host \
	build-release \
	cross-build \
	release-lua-artifacts \
	package \
	prerelease-artifacts \
	package-source \
	package-source-smoke \
	package-checksums \
	package-single-header \
	release-source-artifact \
	release-source-smoke \
	release-darwin-smoke-bundle \
	package-verify \
	package-verify-binary \
	verify-release-archives \
	verify-release-privacy \
	lifecycle-version-contract \
	prerelease \
	prerelease-live \
	prerelease-hardening \
	release-pipeline \
	release-matrix \
	release \
	print-release-version \
	lua-rock \
	lua-env \
	lua-target-runner \
	lua-host-runtime \
	lua-bench-prerequisite \
	lua-test \
	lua-fuzz \
	lua-bench \
	lua-bench-freeze-baseline \
	lua-bench-compare \
	lua-bench-gate \
	bench \
	bench-check \
	bench-freeze-baseline \
	bench-compare \
	bench-baseline-history \
	bench-gate \
	test \
	test-debug \
	test-host \
	test-host-curl \
	test-cross \
	cross-test \
	cross-sanitizers \
	test-all \
	test-all-bindings \
	test-install-tree \
	example-smoke-local \
	asan \
	tsan \
	valgrind \
	fuzz-smoke \
	fuzz \
	fuzz-long \
	stack-usage \
	format \
	format-check \
	deps-debug \
	deps-release \
	deps-host \
	deps-x86_64-linux-gnu \
	deps-x86_64-linux-musl \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl \
	deps-arm64-apple-darwin \
	toolchains-x86_64-linux-gnu \
	toolchains-x86_64-linux-musl \
	toolchains-aarch64-linux-gnu \
	toolchains-aarch64-linux-musl \
	toolchains-armhf-linux-gnu \
	toolchains-armhf-linux-musl \
	toolchains-aflpp \
	toolchains-all \
	deps-cross \
	deps-all \
	certs \
	dev-up \
	dev-down \
	dev-reset \
	dev-ps \
	dev-logs \
	curl-examples \
	test-e2e \
	e2e-build \
	test-curl-e2e \
	test-oidc-e2e \
	test-m2m-e2e \
	clean \
	clean-dist

help:
	@printf '%s\n' \
		'make finalize-slice         Format and run incremental core, ABI/header, and Lua checks before committing a slice.' \
		'make abi-check              Check header/SONAME ABI and report coordinated downstream upgrades when the ABI changes.' \
		'make build                  Configure and build the full debug tree (tests and standalone examples, excluding lua-* and curl-examples).' \
		'make build-debug            Alias for make build.' \
		'make build-host             Configure and build the host-native release preset.' \
		'make build-release          Configure and build the full shipped release test matrix.' \
		'make cross-build            Configure and build every supported Linux cross-release preset.' \
		'make package                Build and verify release packages incrementally.' \
		'make prerelease-artifacts   Compatibility alias for make release-matrix.' \
		'make package-source         Build the source-only release tarball in dist/.' \
		'make package-source-smoke   Unpack the source release tarball into a temp tree, then run host C/Lua tests and Lua artifact packaging there.' \
		'make package-checksums      Generate release checksums for existing dist artifacts.' \
		'make package-single-header  Build the version-stamped standalone-header artifact in dist/.' \
		'make release-lua-artifacts  Build the standalone Lua source package, release rockspec, and source rock in dist/.' \
		'make package-verify         Verify checksum-listed release artifacts for privacy, relocatability, and instrumentation leaks.' \
		'make package-verify-binary  Verify the explicit binary preparation inventory under build/, preserving other dist artifacts.' \
		'make verify-release-archives Alias for make package-verify.' \
		'make verify-release-privacy Alias for make package-verify.' \
		'make lifecycle-version-contract Verify clean committed checkout and exact lightweight release tag without changing state.' \
		'make prerelease             Run the deterministic release pipeline without cleaning generated state first.' \
		'make prerelease-live        Report that no live external-provider release checks are defined.' \
		'make prerelease-hardening   Alias for prerelease; native hardening and benchmark checks are already included.' \
		'make release-pipeline       Internal shared release proof used by prerelease and release.' \
		'make release-matrix         Build host release tests, then package, checksum, and verify every release target without cleaning first.' \
		'make release                Clean generated state, then run the same pipeline as prerelease.' \
		'make print-release-version  Print the exact version used by package and release targets.' \
		'make release-source-smoke   Unpack the source release tarball into a temp tree, then run host C/Lua tests and Lua artifact packaging there.' \
		'make release-darwin-smoke-bundle Build the Darwin smoke ZIP with example and link-smoke binaries.' \
		'make lua-rock               Generate a local rockspec in build/luarocks and install the Lua module there.' \
		'make lua-env                Print shell exports for using the repo-local Lua rock and debug C library.' \
		'make lua-target-runner      Build the native Bootlin Lua runner used by Linux tests and benchmarks.' \
		'make lua-host-runtime       Build the native library used by Darwin Lua benchmarks.' \
		'make lua-bench-prerequisite Prepare the platform-native Lua benchmark runtime.' \
		'make lua-test               Build the Lua module and run the Lua integration test.' \
		'make lua-fuzz               Build the Lua module and run the Lua randomized binding fuzz smoke.' \
		'make lua-bench              Run the standalone Lua benchmark harness, compare it, and enforce the Lua benchmark gate.' \
		'make lua-bench-freeze-baseline Run fresh C and Lua measurements, then freeze the Lua baseline.' \
		'make lua-bench-compare      Run a fresh Lua benchmark, then compare against the committed Lua baseline.' \
		'make lua-bench-gate         Run a fresh Lua benchmark, then enforce the Lua benchmark gate against the committed Lua baseline.' \
		'make bench                  Build and run the host benchmark against the vendored JSON corpus, then compare/gate against the working baseline file if present.' \
		'make bench-check            Run C and Lua benchmark gates using temporary result files, leaving perflogs/ clean.' \
		'make bench-freeze-baseline  Run a fresh C measurement, then freeze it as the current benchmark baseline.' \
		'make bench-compare          Run a fresh C benchmark, then compare against the committed C baseline.' \
		'make bench-baseline-history Compare frozen C/Lua benchmark baselines across git history.' \
		'make bench-gate             Run a fresh C benchmark, then enforce the C benchmark gate against the committed C baseline.' \
		'make test                   Build debug artifacts, run CTest, then Lua tests; this is the debug lifecycle gate.' \
		'make test-debug             Alias for make test.' \
		'make test-host              Build and run the host-native test preset.' \
		'make test-host-curl         Build and run the host-native curl-enabled test preset.' \
		'make test-cross             Configure, build, and run all cross release test presets serially.' \
		'make cross-test             Standard alias for make test-cross.' \
		'make cross-sanitizers       Extra hardening: build and run the supported armhf-linux-gnu ASan/UBSan target under QEMU.' \
		'make toolchains-aflpp       Build the pinned native AFL++ GCC-plugin toolchain in the shared lifecycle cache.' \
		'make test-all               Run debug, host, host-curl, cross, host sanitizers, Valgrind, e2e, and fuzz-smoke serially.' \
		'make test-all-bindings      Compatibility alias for make lua-test; binding coverage is no longer a full world gate.' \
		'make test-install-tree      Verify checksum-listed SDK archives through installed CMake and pkg-config consumers.' \
		'make example-smoke-local    Build and stage standalone local examples.' \
		'make asan                   Build and run the ASan/UBSan preset.' \
		'make tsan                   Build the TSan preset and run the pure-C CTest subset that does not depend on external unsanitized runtimes.' \
		'make valgrind               Run the native x86_64 debug CTest suite under Valgrind Memcheck.' \
		'make fuzz-smoke             Build all AFL++ targets, run a fixed-seed 1000-execution pass for each, and run Lua binding fuzz smoke.' \
		'make fuzz                   Build all AFL++ targets, run a seeded 30s pass for each and run Lua binding fuzz smoke; missing large synthetic seeds are regenerated automatically.' \
		'make fuzz-long              Run the same fuzz targets with a several-minute soak per target.' \
		'make stack-usage            Build with compiler stack-usage reporting and print the report.' \
		'make format                 Run clang-format over the C sources.' \
		'make format-check           Check formatting without modifying sources.' \
		'make deps-debug             Alias for make deps-host.' \
		'make deps-release           Alias for make deps-all.' \
		'make deps-host              Cache the verified host cpkt archive globally, then extract it under .cache/.' \
		'make toolchains-host        Prepare the selected native compiler collection.' \
		'make toolchains-x86_64-linux-gnu Install the pinned x86_64 glibc Bootlin collection.' \
		'make toolchains-x86_64-linux-musl Install the pinned x86_64 musl Bootlin collection.' \
		'make toolchains-aarch64-linux-gnu Install the pinned aarch64 glibc Bootlin collection.' \
		'make toolchains-aarch64-linux-musl Install the pinned aarch64 musl Bootlin collection.' \
		'make toolchains-armhf-linux-gnu Install the pinned armhf glibc Bootlin collection.' \
		'make toolchains-armhf-linux-musl Install the pinned armhf musl Bootlin collection.' \
		'make toolchains-all         Install pinned Bootlin Linux toolchains in the shared lifecycle cache.' \
		'make deps-x86_64-linux-gnu  Download and extract the x86_64 glibc cpkt bundle.' \
		'make deps-x86_64-linux-musl Download and extract the x86_64 musl cpkt bundle.' \
		'make deps-aarch64-linux-gnu Download and extract the aarch64 glibc cpkt bundle.' \
		'make deps-aarch64-linux-musl Download and extract the aarch64 musl cpkt bundle.' \
		'make deps-armhf-linux-gnu   Download and extract the armhf glibc cpkt bundle.' \
		'make deps-armhf-linux-musl  Download and extract the armhf musl cpkt bundle.' \
		'make deps-arm64-apple-darwin Download and extract the arm64 Darwin cpkt bundle.' \
		'make deps-cross             Download and extract bundles required by make test-cross.' \
		'make deps-all               Download and extract every supported cpkt bundle.' \
		'make certs                  Generate the local self-signed localhost TLS cert for nginx.' \
		'make dev-up                 Start the rootless Podman Kube e2e pod through scripts/devenv.sh.' \
		'make dev-down               Stop the checkout-specific Podman Kube pod using its saved manifest.' \
		'make dev-reset              Stop the Podman pod, then remove build/devenv/ as the host user.' \
		'make dev-ps                 Show the checkout-specific Podman pod status.' \
		'make dev-logs               Tail logs from the checkout-specific Podman pod.' \
		'make curl-examples          Build the curl examples against the host cpkt dependency bundle.' \
		'make test-e2e               Run all deterministic local e2e gates serially; set LONEJSON_*_E2E_PORT to avoid host-port conflicts.' \
		'make e2e-build              Prepare the native curl and auth fixture executables for the e2e CTest preset.' \
		'  Ports: LONEJSON_OAUTH2_E2E_PORT=8090, LONEJSON_OIDC_E2E_PORT=18443, LONEJSON_API_FIXTURE_E2E_PORT=18080,' \
		'         LONEJSON_NGINX_HTTP_E2E_PORT=8080, LONEJSON_NGINX_HTTPS_E2E_PORT=8443; distinct loopback ports 1024..65535.' \
		'  Set LONEJSON_E2E_KEEP_DEVSERVICES=1 to retain the pod and generated state for debugging; use dev-reset afterward.' \
		'make test-curl-e2e          Run curl examples and the 72-case upload rewind matrix against the local HTTPS rig.' \
		'make test-oidc-e2e          Build and run OIDC/OAuth2/JWKS e2e against the local Podman pod.' \
		'make test-m2m-e2e           Build and run M2M Basic/Bearer auth e2e with curl as the client.' \
		'make release-source-artifact Build the source-only release tarball in dist/.' \
		'make clean                  Remove build/, dist/, .cache/, examples/bin/, and generated Lua module artifacts; preserve shared caches.' \
		'make clean-dist             Remove dist/ release artifacts only.'

finalize-slice:
	$(MAKE) format
	cmake --build --preset $(DEBUG_PRESET) --target $(SLICE_BUILD_TARGETS)
	ctest --no-tests=error --preset $(DEBUG_PRESET) --output-on-failure -R '$(SLICE_CTEST_INCLUDE)'
	$(MAKE) abi-check
	$(MAKE) lua-test
	$(MAKE) format-check

abi-check: toolchains-host
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared
	ctest --no-tests=error --preset $(DEBUG_PRESET) --output-on-failure -R '^lonejson_(shared_soversion_tests|header_abi_version_tests)$$'
	cmake -DLONEJSON_ROOT="$(CURDIR)" -P cmake/report_downstream_abi.cmake

build: deps-host
	./scripts/build.sh $(DEBUG_PRESET) --stage-examples

build-debug: build

build-host: deps-host
	./scripts/build.sh $(HOST_PRESET)

build-release: $(patsubst %-release,deps-%,$(RELEASE_BUILD_PRESETS))
	@set -e; for preset in $(RELEASE_BUILD_PRESETS); do \
		./scripts/build.sh "$$preset"; \
	done

cross-build: deps-cross
	./scripts/cross_build.sh $(CROSS_RELEASE_PRESETS)

$(DIST_DIR):
	mkdir -p "$(DIST_DIR)"

$(RELEASE_PACK_DIR):
	mkdir -p "$(RELEASE_PACK_DIR)"

$(RELEASE_PACK_STAGE_DIR): Makefile $(LUA_ROCK_SOURCES) | $(RELEASE_PACK_DIR)
	./scripts/stage_lua_rock_sources.sh "$(CURDIR)" "$(RELEASE_PACK_STAGE_DIR)" "$(RELEASE_VERSION)"

$(RELEASE_LUA_SOURCE_TARBALL): $(RELEASE_PACK_STAGE_DIR) scripts/archive_release_tree.sh | $(DIST_DIR)
	./scripts/archive_release_tree.sh "$(RELEASE_PACK_STAGE_DIR)" "$(RELEASE_PACK_DIR)/lonejson-lua-$(RELEASE_VERSION).tar.gz"
	mv "$(RELEASE_PACK_DIR)/lonejson-lua-$(RELEASE_VERSION).tar.gz" "$(RELEASE_LUA_SOURCE_TARBALL)"

$(RELEASE_ROCKSPEC): Makefile lonejson.rockspec.in scripts/render_release_rockspec.sh | $(DIST_DIR)
	lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(RELEASE_ROCKSPEC)" "$(RELEASE_LUA_SOURCE_URL)" "" "$$lib_ext" "lonejson-$(RELEASE_VERSION)"

$(RELEASE_ROCK): $(RELEASE_LUA_SOURCE_TARBALL) $(RELEASE_ROCKSPEC) scripts/package_lua_src_rock.sh
	./scripts/package_lua_src_rock.sh "$(RELEASE_ROCK)" "$(RELEASE_ROCKSPEC)" "$(RELEASE_LUA_SOURCE_TARBALL)"

release-lua-artifacts: $(RELEASE_ROCKSPEC) $(RELEASE_LUA_SOURCE_TARBALL) $(RELEASE_ROCK) toolchains-host
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared lonejson_static
	cmake --install "build/$(DEBUG_PRESET)" --prefix "$(CURDIR)/build/lua-sdk"
	./scripts/smoke_lua_src_rock.sh "$(RELEASE_ROCK)" "$(LONEJSON_LUA_LIBDIR)" "$(LUA)" "$(LUAROCKS)"

package:
	./scripts/package.sh

package-source: release-source-artifact

release-source-artifact: toolchains-host
	cmake --preset $(HOST_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset package-source

$(RELEASE_SOURCE_TARBALL):
	$(MAKE) release-source-artifact

release-source-smoke: release-source-artifact
	./scripts/test_release_from_source.sh "$(CURDIR)" "$(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz"

package-source-smoke: release-source-smoke

package-checksums: toolchains-host
	cmake --preset $(HOST_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset package-checksums

package-single-header: toolchains-host
	cmake --preset $(HOST_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset package-single-header

release-darwin-smoke-bundle: deps-arm64-apple-darwin
	cmake --preset arm64-apple-darwin-release $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset arm64-apple-darwin-release --target package-darwin-smoke-bundle

prerelease-artifacts: release-matrix

package-verify:
	./scripts/package-verify.sh "$(RELEASE_CHECKSUMS)"

package-verify-binary:
	LONEJSON_DIST_DIR="$(DIST_DIR)" ./scripts/package-verify.sh --binary

verify-release-archives: package-verify

verify-release-privacy: package-verify

lifecycle-version-contract:
	bash ./scripts/check_release_identity.sh

prerelease: release-pipeline

prerelease-live:
	@printf '%s\n' 'No live prerelease checks are defined; use make prerelease for local proof.' >&2; exit 1

prerelease-hardening: prerelease

release-pipeline:
	+$(TIME_STEP) prerelease/format $(MAKE) format-check
	+$(TIME_STEP) prerelease/test $(MAKE) test
	+$(TIME_STEP) prerelease/asan $(MAKE) asan
	+$(TIME_STEP) prerelease/tsan $(MAKE) tsan
	+$(TIME_STEP) prerelease/valgrind $(MAKE) valgrind
	+$(TIME_STEP) prerelease/test-e2e $(MAKE) test-e2e
	+$(TIME_STEP) prerelease/fuzz-smoke $(MAKE) fuzz-smoke
	+$(TIME_STEP) prerelease/bench-check $(MAKE) bench-check
	# The matrix owns host release and cross-target tests, once per configuration.
	+$(TIME_STEP) prerelease/release-matrix $(MAKE) release-matrix

release-matrix:
	./scripts/run_release_matrix.sh

release:
	+$(TIME_STEP) release/version-contract $(MAKE) lifecycle-version-contract
	+$(TIME_STEP) release/clean $(MAKE) clean
	+$(TIME_STEP) release/pipeline $(MAKE) release-pipeline
	+$(TIME_STEP) release/source-smoke $(MAKE) package-source-smoke
	+$(TIME_STEP) release/package-checksums $(MAKE) package-checksums
	+$(TIME_STEP) release/package-verify $(MAKE) package-verify

print-release-version:
	@printf '%s\n' "$(RELEASE_VERSION)"

bench: toolchains-host
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON $(LONEJSON_CMAKE_RELEASE_ARGS) && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	if [ -f "$(PERF_BASELINE)" ]; then \
		./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)" && \
		./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"; \
	fi

bench-check: toolchains-host
ifeq ($(and $(wildcard $(PERF_BASELINE)),$(wildcard $(LUA_PERF_BASELINE))),)
	@if [ ! -f "$(PERF_BASELINE)" ] || [ ! -f "$(LUA_PERF_BASELINE)" ]; then \
		printf '%s\n' "bench-check skipped: missing frozen benchmark baseline for host $(PERF_HOST_ID)" >&2; \
		printf '%s\n' "  C baseline: $(PERF_BASELINE)" >&2; \
		printf '%s\n' "  Lua baseline: $(LUA_PERF_BASELINE)" >&2; \
		printf '%s\n' "run 'make bench-freeze-baseline lua-bench-freeze-baseline' on this host to create one" >&2; \
		exit 0; \
	fi
else
	@$(MAKE) lua-bench-prerequisite
	@mkdir -p "$(CURDIR)/build"; tmp_dir="$$(mktemp -d "$(CURDIR)/build/bench-check.XXXXXX")"; \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	c_latest="$$tmp_dir/c-latest.json"; \
	c_history="$$tmp_dir/c-history.jsonl"; \
	c_runs="$$tmp_dir/c-runs"; \
	lua_latest="$$tmp_dir/lua-latest.json"; \
	lua_history="$$tmp_dir/lua-history.jsonl"; \
	lua_runs="$$tmp_dir/lua-runs"; \
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON $(LONEJSON_CMAKE_RELEASE_ARGS) && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench lonejson_shared && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$$c_latest" "$$c_history" "$$c_runs" "$(PERF_ITERATIONS)" && \
	if ! ./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$$c_latest"; then \
		./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$$c_latest"; \
		printf '%s\n' 'C benchmark gate failed once; rerunning only failing cases once to confirm.' >&2; \
		$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua confirm-c "./build/$(HOST_PRESET)/lonejson_bench" "$(PERF_BASELINE)" "$$c_latest" "$(PERF_ITERATIONS)"; \
	fi && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua run "$$c_latest" "$$lua_latest" "$$lua_history" "$$lua_runs" "$(LUA_PERF_ITERATIONS)" && \
	if ! $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$$lua_latest"; then \
		$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$$lua_latest"; \
		printf '%s\n' 'Lua benchmark gate failed once; rerunning only failing cases once to confirm.' >&2; \
		$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua confirm-lua "$$c_latest" "$(LUA_PERF_BASELINE)" "$$lua_latest" "$(LUA_PERF_ITERATIONS)"; \
	fi
endif

bench-freeze-baseline: toolchains-host
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON $(LONEJSON_CMAKE_RELEASE_ARGS) && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench freeze-baseline "$(PERF_HISTORY)" "$(PERF_BASELINE)"

bench-compare: toolchains-host
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON $(LONEJSON_CMAKE_RELEASE_ARGS) && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)"

bench-baseline-history:
	@$(MAKE) --no-print-directory lua-bench-prerequisite >/dev/null
	$(LUA_BENCH_RUN) scripts/bench_baseline_history.lua --repo "$(CURDIR)" --host-id "$(PERF_HOST_ID)"

bench-gate: toolchains-host
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON $(LONEJSON_CMAKE_RELEASE_ARGS) && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"

ifeq ($(LONEJSON_HOST_OS):$(shell uname -m),Linux:x86_64)
test test-host test-host-curl: toolchains-aflpp
endif

test: build
	./scripts/test.sh $(DEBUG_PRESET)
ifeq ($(LONEJSON_HOST_OS),Linux)
	$(MAKE) lua-test
endif

test-debug: test

test-host: build-host
	./scripts/test.sh $(HOST_PRESET)

test-host-curl: deps-host
	bundle_root="$$(./scripts/detect_cpkt_bundle.sh)" && cmake --preset host-curl -D LONEJSON_CPKT_ROOT="$$bundle_root" $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset host-curl
	ctest --no-tests=error --preset host-curl

cross-test: deps-cross
	./scripts/cross_test.sh $(CROSS_RELEASE_PRESETS)

test-cross: cross-test

cross-sanitizers: deps-cross
	./scripts/run_cross_sanitizer_matrix.sh

test-all:
	+$(TIME_STEP) test $(MAKE) test
	+$(TIME_STEP) test-host $(MAKE) test-host
	+$(TIME_STEP) test-host-curl $(MAKE) test-host-curl
	+$(TIME_STEP) test-cross $(MAKE) test-cross
	+$(TIME_STEP) asan $(MAKE) asan
	+$(TIME_STEP) tsan $(MAKE) tsan
	+$(TIME_STEP) valgrind $(MAKE) valgrind
	+$(TIME_STEP) test-e2e $(MAKE) test-e2e
	+$(TIME_STEP) fuzz-smoke $(MAKE) fuzz-smoke
	+$(TIME_STEP) bench-check $(MAKE) bench-check

test-all-bindings:
	$(MAKE) lua-test

test-install-tree: package-verify

example-smoke-local: build

lua-env: lua-rock
	@$(LUAROCKS) path --tree "$(LUA_ROCK_TREE)"
	@printf 'export LONEJSON_LIBDIR=%q\n' "$(LONEJSON_LUA_LIBDIR)"

$(LUA_ROCKSPEC): $(LUA_ROCK_SOURCES)
	mkdir -p "$(LUA_ROCK_TREE)"
	lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(LUA_ROCKSPEC)" "git+file://$(CURDIR)" "" "$$lib_ext"

lua-rock: $(LUA_ROCKSPEC) toolchains-host
ifeq ($(strip $(LONEJSON_LUA_BUILD_DIR)),)
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared lonejson_static
	cmake --install "build/$(DEBUG_PRESET)" --prefix "$(CURDIR)/build/lua-sdk"
else
	cmake --build "$(LONEJSON_LUA_BUILD_DIR)" --target lonejson_shared lonejson_static
	cmake --install "$(LONEJSON_LUA_BUILD_DIR)" --prefix "$(CURDIR)/build/lua-sdk"
endif
	mkdir -p "$(CURDIR)/build/luarocks-native/tool-tmp"
	set -e; export TMPDIR="$(CURDIR)/build/luarocks-native/tool-tmp"; CFLAGS="$${CFLAGS:+$$CFLAGS }$(LUA_ROCK_EXTRA_CFLAGS)" LONEJSON_LIBDIR="$(LONEJSON_LUA_LIBDIR)" "$(LUAROCKS)" make --tree "$(LUA_ROCK_TREE)" "$(LUA_ROCKSPEC)"; rm -rf $(LUA_ROCK_BUILD_BYPRODUCTS)

lua-target-runner: deps-host
	bundle_root="$$(./scripts/detect_cpkt_bundle.sh)" && cmake --preset host-curl -D LONEJSON_CPKT_ROOT="$$bundle_root" $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset host-curl --target lonejson_lua_target_runner

lua-host-runtime: lua-rock
	cmake --preset $(HOST_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(HOST_PRESET) --target lonejson_shared

ifeq ($(LONEJSON_HOST_OS),Linux)
lua-test: lua-target-runner
	ctest --preset host-curl -R '^lonejson_lua_(target|fuzz)_tests$$'

lua-fuzz: lua-target-runner
	ctest --preset host-curl -R '^lonejson_lua_fuzz_tests$$'
else
lua-test: toolchains-host
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	LUA="$(LUA)" LUAROCKS="$(LUAROCKS)" LONEJSON_LIBDIR="$(LONEJSON_LUA_LIBDIR)" ctest --preset $(DEBUG_PRESET) -R '^lonejson_lua_(native|schema_cache|encode_stats)_tests$$'

lua-fuzz: toolchains-host
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	LUA="$(LUA)" LUAROCKS="$(LUAROCKS)" LONEJSON_LIBDIR="$(LONEJSON_LUA_LIBDIR)" ctest --preset $(DEBUG_PRESET) -R '^lonejson_lua_native_fuzz_tests$$'
endif

lua-bench-prerequisite: $(LUA_BENCH_PREPARE)

lua-bench:
	@$(MAKE) lua-bench-prerequisite
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	if [ -f "$(LUA_PERF_BASELINE)" ]; then \
		$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)" && \
		$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"; \
	fi

lua-bench-freeze-baseline: lua-bench-prerequisite
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON $(LONEJSON_CMAKE_RELEASE_ARGS) && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua freeze-baseline "$(LUA_PERF_HISTORY)" "$(LUA_PERF_BASELINE)"

lua-bench-compare:
	@$(MAKE) lua-bench-prerequisite
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

lua-bench-gate:
	@$(MAKE) lua-bench-prerequisite
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" $(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	$(LUA_BENCH_RUN) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

asan: toolchains-host
	cmake --preset $(ASAN_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(ASAN_PRESET)
	ctest --no-tests=error --preset $(ASAN_PRESET) -E "$(SANITIZER_CTEST_EXCLUDE)"

tsan:
	@set -e; support="$(LONEJSON_HAVE_TSAN)"; \
	if [ -z "$$support" ]; then \
		support="$$(./scripts/check_bootlin_tsan_support.sh)"; \
	fi; \
	case "$$support" in 0|1) ;; *) printf '%s\n' 'LONEJSON_HAVE_TSAN must be 0, 1, or empty for automatic detection' >&2; exit 1 ;; esac; \
	if [ "$$support" != "1" ]; then \
		printf '%s\n' 'Skipping tsan: unsupported toolchain'; \
		exit 0; \
	fi; \
	target_id="$$(./scripts/detect_native_target.sh)"; \
	./scripts/cpkt-toolchains.sh ensure "$$target_id" >/dev/null; \
	cmake --preset $(TSAN_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS); \
	cmake --build --preset $(TSAN_PRESET); \
	ctest --no-tests=error --preset $(TSAN_PRESET) -E "$(SANITIZER_CTEST_EXCLUDE)"

valgrind: deps-host
	@command -v valgrind >/dev/null 2>&1 || { printf '%s\n' 'Valgrind is required for the native memory-check gate' >&2; exit 1; }
	bundle_root="$$(./scripts/detect_cpkt_bundle.sh)" && cmake --preset $(VALGRIND_PRESET) -D LONEJSON_CPKT_ROOT="$$bundle_root" $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(VALGRIND_PRESET) --target lonejson_tests lonejson_curl_rewind_tests lonejson_lua_target_runner
	cd build/$(VALGRIND_PRESET) && ctest --no-tests=error -j 1 -T MemCheck --overwrite "MemoryCheckCommandOptions=--leak-check=full --track-origins=yes --keep-debuginfo=yes --num-callers=30 --error-exitcode=86 --quiet" -R '^lonejson_(tests|curl_rewind_tests|lua_target_tests)$$' --output-on-failure

fuzz: deps-host toolchains-aflpp
	bundle_root="$$(./scripts/detect_cpkt_bundle.sh)" && cmake --preset $(FUZZ_PRESET) -D LONEJSON_CPKT_ROOT="$$bundle_root" -D LONEJSON_FUZZ_SECONDS="$(FUZZ_TIME)" $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(FUZZ_PRESET) --target lonejson_fuzz_targets
	ctest --preset fuzz
	$(MAKE) lua-fuzz

fuzz-long:
	$(MAKE) fuzz FUZZ_TIME=$(FUZZ_LONG_TIME)

fuzz-smoke:
	LONEJSON_FUZZ_EXECUTIONS=1000 $(MAKE) fuzz

stack-usage: toolchains-host
	cmake --preset stack-usage $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset stack-usage-report

format: toolchains-host
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset format

format-check: toolchains-host
	cmake --preset $(DEBUG_PRESET) $(LONEJSON_CMAKE_RELEASE_ARGS)
	cmake --build --preset $(DEBUG_PRESET) --target format-check

deps-debug: deps-host

toolchains-aflpp:
	./scripts/cpkt-aflpp.sh ensure

deps-release: deps-all

deps-host:
	target_id="$$(./scripts/detect_native_target.sh)" && ./scripts/deps.sh "$$target_id"

.PHONY: toolchains-host
toolchains-host:
	target_id="$$(./scripts/detect_native_target.sh)" && ./scripts/cpkt-toolchains.sh ensure "$$target_id" >/dev/null

toolchains-x86_64-linux-gnu:
	./scripts/cpkt-toolchains.sh ensure x86_64-linux-gnu

toolchains-x86_64-linux-musl:
	./scripts/cpkt-toolchains.sh ensure x86_64-linux-musl

toolchains-aarch64-linux-gnu:
	./scripts/cpkt-toolchains.sh ensure aarch64-linux-gnu

toolchains-aarch64-linux-musl:
	./scripts/cpkt-toolchains.sh ensure aarch64-linux-musl

toolchains-armhf-linux-gnu:
	./scripts/cpkt-toolchains.sh ensure armhf-linux-gnu

toolchains-armhf-linux-musl:
	./scripts/cpkt-toolchains.sh ensure armhf-linux-musl

toolchains-all:
	./scripts/cpkt-toolchains.sh ensure all

deps-x86_64-linux-gnu: toolchains-x86_64-linux-gnu
	./scripts/deps.sh x86_64-linux-gnu

deps-x86_64-linux-musl: toolchains-x86_64-linux-musl
	./scripts/deps.sh x86_64-linux-musl

deps-aarch64-linux-gnu: toolchains-aarch64-linux-gnu
	./scripts/deps.sh aarch64-linux-gnu

deps-aarch64-linux-musl: toolchains-aarch64-linux-musl
	./scripts/deps.sh aarch64-linux-musl

deps-armhf-linux-gnu: toolchains-armhf-linux-gnu
	./scripts/deps.sh armhf-linux-gnu

deps-armhf-linux-musl: toolchains-armhf-linux-musl
	./scripts/deps.sh armhf-linux-musl

deps-arm64-apple-darwin:
	./scripts/deps.sh arm64-apple-darwin

deps-cross: \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl

deps-all: \
	deps-x86_64-linux-gnu \
	deps-x86_64-linux-musl \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl \
	deps-arm64-apple-darwin

certs:
	./scripts/ensure_test_certs.sh

dev-up:
	LUA="$(LUA)" ./scripts/devenv.sh up

dev-down:
	./scripts/devenv.sh down

dev-reset:
	./scripts/devenv.sh reset

dev-ps:
	./scripts/devenv.sh ps

dev-logs:
	./scripts/devenv.sh logs -f

curl-examples: deps-host
	./scripts/build_curl_examples.sh

e2e-build: curl-examples
	cmake --build --preset host-curl --target lonejson_oidc_fixture_server lonejson_m2m_fixture_server lonejson_curl_rewind_e2e

ifeq ($(LONEJSON_E2E_SERVICES_READY),1)
test-curl-e2e: e2e-build
else
test-curl-e2e: dev-up e2e-build
endif
	ctest --preset e2e -R '^lonejson_curl_e2e_tests$$'

test-e2e:
	./scripts/test-e2e.sh

ifeq ($(LONEJSON_E2E_SERVICES_READY),1)
test-oidc-e2e: e2e-build
else
test-oidc-e2e: dev-up e2e-build
endif
	ctest --preset e2e -R '^lonejson_oidc_e2e_tests$$'

test-m2m-e2e: e2e-build
	ctest --preset e2e -R '^lonejson_m2m_e2e_tests$$'

clean:
	./scripts/clean.sh

clean-dist:
	./scripts/clean.sh --dist-only
