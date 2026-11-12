# Release artifacts

Use the [latest published release](https://github.com/c89systems/lonejson/releases)
for prebuilt SDKs and the generated standalone header. Commands below are for
maintainers; [building and verification](development.md) describes local tests
and toolchains.

## Maintainer workflow

Implement and verify on one development branch. `make prerelease` runs the
native debug/Lua, sanitizer, Valgrind, deterministic e2e and fuzz
gates, then builds and verifies the binary release matrix incrementally.
Use `make prerelease-hardening` to add the full C and Lua benchmark comparison
against the frozen host references. Normal prerelease and release do not run
these long performance benchmarks; release retains its quick baseline preflight.
Source reconstruction is reserved for the final release gate.

Squash the verified change onto the local default branch before final
amendment/signing. Set `origin/HEAD` to the authoritative default branch. Sign
the commit, create its exact lightweight `vX.Y.Z` tag, then run:

```sh
make release
```

Before cleanup or artifact production, its read-only
`make lifecycle-version-contract` prerequisite requires a clean owning checkout
attached to the authoritative default branch, a verified HEAD commit signature,
exactly one lightweight release tag and no candidate override.
`make release-baseline-check` then requires this host's frozen C and Lua
benchmark files to exist and be recorded in HEAD. Missing or uncommitted
baselines stop the release before cleanup. A read-only compatibility check also
rejects references from an older measurement method or incompatible sampling
policy before any cleanup or build. `release` then cleans generated
state, runs the proof graph, reconstructs the source archive
and verifies the complete upload inventory. The extracted source runs host C
and Lua tests and builds its Lua release artifacts.

Keep remote branch/tag refs unchanged until the clean local gate passes. Hosted
CI is diagnostic, never release proof or a source of release artifacts; Darwin
release proof uses local osxcross. A failed release stops for repair in a
separately authorized iteration.

## Products and verification scope

The release produces:

- Per-target C SDK archives, `liblonejson-<version>-<target-id>.tar.gz`.
- A source archive, `lonejson-<version>.tar.gz`, excluding generated output and
  local stash material.
- A compressed standalone header, `lonejson-<version>.h.gz`.
- A Lua source archive, `lonejson-lua-<version>.tar.gz`, a versioned rockspec,
  `lonejson-<version>-1.rockspec`, and `lonejson-<version>-1.src.rock`.
- A SHA-256 manifest, `dist/lonejson-<version>-CHECKSUMS`.

Binary preparation preserves unrelated `dist/` files and records its selected
inventory under `build/packaging/checksums/`; `make package-verify-binary` checks
that scope. The full manifest requires every selected SDK, Lua product, header
and source archive; final verification also rejects unlisted release artifacts.
See [artifact verification](#artifact-verification).
`make package-source-smoke` validates source reconstruction independently of
binary products and does not generate the complete upload manifest.

## Version identity

Archives and the standalone header share one version source: `VERSION` in a
source package, otherwise an exact lightweight `vX.Y.Z` tag on HEAD, otherwise
`0.0.0`. Source-package checks use `VERSION` and `RELEASE_MANIFEST` even inside
a Git checkout and do not change the parent checkout's tags.

`LONEJSON_VERSION_OVERRIDE=X.Y.Z` is for candidate rehearsals only. Make exports
an explicit command-line override; CMake honors an environment override for the
current configure without caching it. Use CMake's
`-D LONEJSON_VERSION_OVERRIDE=X.Y.Z` only when that build directory should retain
the override. `prerelease-hardening` adds performance benchmarks to `prerelease`;
`prerelease-live` reports that no live external-provider gate is defined.

The Lua source package prefers a curl-enabled native build when curl is
available and otherwise falls back to a curl-free build. See the
[Lua manual](https://github.com/c89systems/lonejson/blob/trunk/lua/README.md)
for explicit feature selection.

## Artifact verification

`make package` prepares and verifies packages incrementally. The binary matrix
owns host release and cross testing once per configuration, with host-only
LuaRocks/tooling checks kept native; it does not repeat a preceding `test-cross`.
Each target configure/build/test/package stage emits `PKT_TIMING_BEGIN/END`
records, as do final header, Lua, checksum and verification stages.

Make passes the current `RELEASE_BUILD_PRESETS` selection explicitly to CMake as
`LONEJSON_RELEASE_TARGETS`, preventing warm caches from retaining stale targets.
Direct CMake callers use `-DLONEJSON_RELEASE_TARGETS=...`. Direct verifier calls
default to six Linux targets plus Darwin when the same read-only readiness check
used by Make succeeds; set `LONEJSON_RELEASE_TARGETS` for another inventory.
An explicitly empty selection fails.

Binary preparation records
`build/packaging/checksums/lonejson-<version>-binary-CHECKSUMS` and preserves
unrelated `dist/` files. Its verifier checks the listed bytes and extracted
consumers. Full checksum generation requires every selected SDK, Lua product,
header and source archive. Verification rejects missing/unexpected entries and
changed bytes; final verification also rejects unlisted release-looking files.
SDKs declare curl/OpenSSL as external optional facade requirements; ordinary
core linking needs neither. Export allowlists, installed consumers and target
tools check actual shared/static libraries and Lua modules. Collection/cache
paths, instrumentation and private downstream linkage cannot enter uploads.

`make package-source-smoke` (alias `release-source-smoke`) reconstructs source
independently of binary products and does not generate a full upload manifest.
Linux source-rock validation installs the current public SDK under
`build/lua-sdk`, compiles against installed headers and uses the target Lua runner.
Lua products share one rockspec with the release's HTTPS source URL and extracted
directory; the source rock embeds that rockspec and archive for offline installs.
Empty/local URLs fail verification. Staging is reused under `build/`, but each
`release-lua-artifacts` invocation verifies against a freshly prepared SDK.

Packaging validates physical workspace ownership before cleanup and rejects a
symlinked default `dist/`. Explicit destinations retain ownership markers.
Intermediates stay under `build/`; final copying supports another filesystem.
Archives use sorted entries, fixed 1980 UTC timestamps, normalized modes and
numeric root tar ownership, preserving executables, links and empty directories.
Normalization uses disposable staging so maintained inputs stay reusable. Static
archive headers are normalized after stripping; ZIP omits workstation extra
fields, and source rocks normalize both embedded files. Standalone headers are
assembled from formatted maintained fragments; build aliases match, while the
release header stamps six version macros without another formatting pass.

For a custom upload destination pass its checksum manifest to
`scripts/package-verify.sh`; all verification uses that manifest's directory.
Binary verification accepts `LONEJSON_DIST_DIR=/absolute/artifacts` or
`make package-verify-binary DIST_DIR=/absolute/artifacts` with its matching manifest
already prepared. Full clean removes default `dist/` and an explicitly selected
custom destination; `--dist-only` removes only the selected directory.
