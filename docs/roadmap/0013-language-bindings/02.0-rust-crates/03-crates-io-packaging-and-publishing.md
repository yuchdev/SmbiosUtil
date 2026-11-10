# 03 - crates.io packaging and publishing

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Package both `smbios-sys` and `smbios` for `crates.io` publication,
each with correct, independent `Cargo.toml` metadata and a documented,
repeatable release process, so `cargo add smbios` (or `cargo add
smbios-sys` for the raw-FFI-only consumer) works for an external user
with no knowledge of this repository's own build system beyond what
`Cargo.toml`'s own `build-dependencies`/`links` keys already declare.

## Files

- **Modify** `bindings/rust/smbios-sys/Cargo.toml` /
  `bindings/rust/smbios/Cargo.toml`: `license` (matching this project's
  own, confirm which - `libsmbios` itself does not currently declare one
  in `vcpkg.json`/root `CMakeLists.txt`; resolve before publishing,
  since crates.io requires a `license`/`license-file` field),
  `repository`, `description`, `documentation` (docs.rs, generated
  automatically from `Cargo.toml` metadata), `keywords`, `categories`.
- **Create** `bindings/rust/RELEASING.md` (or fold into
  `docs/dev/language-bindings.md`): the manual (or CI-automated, if
  Task's own scope extends there) steps to cut a release - version bump
  in both `Cargo.toml`s (independently, per plan.md's Key constraints),
  `cargo publish -p smbios-sys` before `-p smbios` (dependency order),
  and the `smbios_c` version range each release was built/tested
  against, recorded in the crate's own changelog.
- **Modify** `.github/workflows/` (new or existing workflow): a
  `cargo publish --dry-run` CI gate on every PR touching
  `bindings/rust/`, catching a packaging-metadata regression before an
  actual publish attempt - actual publish itself is a manual,
  maintainer-triggered step (no automatic publish-on-merge), consistent
  with this being a single-maintainer-scale project with no existing
  release-automation precedent (per ADR 0004's DD2 reasoning, applied
  here by analogy).

## Functions and Classes

N/A - packaging/process only.

## Constraints

- `smbios-sys`'s `links = "smbios_c"` key must be set so Cargo enforces
  "at most one crate in a dependency graph links against `smbios_c`" -
  preventing a downstream consumer's own crate graph from accidentally
  linking two different versions of the native library.
- docs.rs builds crates in a network-sandboxed environment with no
  access to a locally built `smbios_c` - `smbios-sys`'s `build.rs` must
  either detect this (`DOCS_RS` environment variable, docs.rs's own
  documented convention) and skip the native link step, emitting
  pre-generated/cached bindings for documentation purposes only, or the
  crate must ship a `[package.metadata.docs.rs]` configuration that
  avoids invoking the native build at all - without this, the published
  crate's own docs.rs page fails to build, which is a real, common
  failure mode for `-sys` crates that this task must not reproduce.
- Confirm (and record, if unresolved) this project's own license before
  publishing either crate - crates.io requires a `license` or
  `license-file` field, and no top-level `LICENSE` file's existence/
  content was verified while writing this task; flag as an open item if
  still missing when this task starts.

## Tests

- `cargo publish --dry-run -p smbios-sys` and `-p smbios` both succeed
  locally and in CI with no warnings about missing required metadata.
- A docs.rs build simulation (`cargo doc --no-deps` with `DOCS_RS=1` set
  locally, mirroring docs.rs's own build environment as closely as
  feasible) succeeds without a native `smbios_c` present.

## Success criteria

- [ ] Both crates have complete, independent `Cargo.toml` metadata
      sufficient for a real `cargo publish` (verified via `--dry-run`).
- [ ] The release process and the `smbios_c` version-range-tracking
      convention are documented.
- [ ] A docs.rs-style build succeeds without a native `smbios_c` present.
- [ ] This project's license status for crates.io publication is
      confirmed (or explicitly flagged as a blocking open item).
