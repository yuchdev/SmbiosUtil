# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 03

## Objective

Close Story 02.0's exit gate with FFI-boundary-specific tests for the
Rust crates themselves (unit-level, within `bindings/rust/`), run in
CI - distinct from Story 04.0's cross-language, cross-binding parity
suite, which depends on this story and Story 03.0 both existing first.

## Scope

- `smbios-sys`: the raw-symbol linkage and sync-check tests from Task
  01, wired into `cargo test` and CI.
- `smbios`: the `Drop`/lifetime/`Result`-mapping tests from Task 02,
  wired into `cargo test` and CI, run on real hardware where available
  (Linux/macOS CI runners, per Story 01.0 Task 03's platform scope) and
  tolerant of a `NoTableFound` result on a runner with no accessible
  SMBIOS table (mirroring how this project's own C++ integration tests
  already fail cleanly, not crash, without privilege - see root
  `CLAUDE.md`'s Build and test section).
- A `cargo clippy --all-targets` and `cargo fmt --check` gate, matching
  this project's own `-Wall -Wextra -Werror` C++ hygiene bar applied to
  the Rust side.

## Success criteria

- [ ] `cargo test` passes for both crates in CI on every platform Story
      01.0 Task 03 scoped as pass/fail-gating.
- [ ] The `compile_fail` lifetime test (Task 02) runs as part of this
      gate, not just locally.
- [ ] `cargo clippy`/`cargo fmt --check` are clean.
- [ ] A missing/inaccessible real SMBIOS table on a CI runner produces a
      clean `NoTableFound` result, not a test crash or hang.
