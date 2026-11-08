# Release artifacts

Use the [latest published release](https://github.com/c89systems/lonejson/releases)
for prebuilt SDKs and the generated standalone header. Commands below are for
maintainers; the [local verification contract](local-verification.md) describes
detailed test, toolchain and package checks.

## Maintainer workflow

Implement and verify on one development branch. `make prerelease` runs the
native debug/Lua, sanitizer, Valgrind, deterministic e2e, fuzz and benchmark
gates, then builds and verifies the binary release matrix incrementally.
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
exactly one lightweight release tag and no candidate override. `release` then
cleans generated state, runs the proof graph, reconstructs the source archive
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
See [artifact inventories](local-verification.md#artifact-inventories-and-selected-test-scopes).
`make package-source-smoke` validates source reconstruction independently of
binary products and does not generate the complete upload manifest.

## Version identity

Archives and the standalone header share one version source: `VERSION` in a
source package, otherwise an exact lightweight `vX.Y.Z` tag on HEAD, otherwise
`0.0.0`. Source-package checks use `VERSION` and `RELEASE_MANIFEST` even inside
a Git checkout and do not change the parent checkout's tags.

`LONEJSON_VERSION_OVERRIDE=X.Y.Z` is for candidate rehearsals only. Make exports
an explicit command-line override; CMake honors an environment override for the
current configure without caching it. See [broader confidence](local-verification.md#broader-confidence)
for persistent CMake overrides and ordinary validation.

The Lua source package prefers a curl-enabled native build when curl is
available and otherwise falls back to a curl-free build. See the
[Lua manual](https://github.com/c89systems/lonejson/blob/trunk/lua/README.md)
for explicit feature selection.
