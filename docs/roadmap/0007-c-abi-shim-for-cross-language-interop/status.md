# Milestone 0007 - C ABI Shim for Cross-Language Interop - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                     | Status         | Tests |
|-------|---------------------------------------------|----------------|-------|
| 01.0  | C ABI core: handles and error model        | ✅ Complete    | manual (pre-CMake) |
| 02.0  | Entry and field accessors C API            | ⬜ Not started | -     |
| 03.0  | Build integration and packaging            | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Added at the user's request after a design discussion on how to offer
  both a C and a C++ interface. The architectural decision made then,
  carried into `plan.md`: a thin, separately-versioned C ABI shim over
  opaque handles, wrapping the existing C++ core - not a second C
  implementation, and not an attempt to make the C++ headers themselves
  C-compatible.
- Depends on milestone 0004 (entry model and factory finalized) landing
  first, so this shim wraps a settled surface rather than one still being
  redesigned by milestones 0002/0003.
- The decision to wrap only a curated subset of typed getters (not every
  C++ getter 1:1) is deliberate scope control, recorded in `plan.md`'s Key
  constraints - revisit and expand the curated set in a future milestone
  once real external consumers (a Python/Rust/C# binding) identify which
  fields they actually need.
- Story 01.0 Task 02: `smbios::SMBios()` never throws when every
  `ISmbiosSource` in the chain fails - per its own doc comment in
  `libsmbios/include/libsmbios/smbios.h`, it leaves itself with an empty
  table instead. `smbios_open()` therefore detects "no table found" via
  `get_structures_count() == 0` after construction, not via a caught
  exception for that specific case; the try/catch still exists for any
  other exception type (e.g. `std::bad_alloc`), mapped to
  `SMBIOS_ERR_UNKNOWN`.
- Story 01.0 Task 02's second success criterion asked to force every
  `ISmbiosSource` to fail via a test double "if milestone 0005's chain is
  injectable for testing." It is not: `make_default_source_chain()` is a
  CMake-time link seam (one definition per per-OS library), not a
  runtime-swappable dependency, so no C++ test can substitute a fake
  chain without relinking the binary. Verified instead by: (1) code
  reading `smbios::SMBios()`'s constructor and confirming the "every
  source failed" path never throws, so `smbios_open()`'s non-exception
  detection path is exercised by construction, not luck; and (2) a real
  run against this host's live SMBIOS table confirming the success path
  (`SMBIOS_OK` + non-null handle) and the `smbios_close()` safety on both
  a real handle and `nullptr`.
- Story 01.0 Task 03's third success criterion ("sanitizer CI job green
  against the new `smbios_c` target") is deferred to Story 03.0's close:
  the root `CMakeLists.txt` has no `add_subdirectory(smbios_c)` yet
  (that is Story 03.0 Task 01's own deliverable), so no CI job can
  exercise a target that does not exist in the build graph. Story 01.0
  closes on its other two criteria (both manual compile tests pass) plus
  a clean full local-suite run (4/4 `ctest`, unaffected since this
  story's changes are purely additive with no CMake wiring). Re-verify
  this specific criterion once Story 03.0 lands the `smbios_c` CMake
  target.

### Story 01.0 - C ABI Core: Handles and Error Model (✅ 2026-11-09)

**Delivered**
- `smbios_c/include/smbios_c/smbios_c.h`: opaque `smbios_handle_t`/
  `smbios_entry_handle_t` (incomplete-struct-pointer idiom, no struct body
  exposed) and the `smbios_error_t` enum (contract C1), plus
  `smbios_open()`/`smbios_close()` declarations.
- `smbios_c/src/smbios_c.cpp`: `smbios_open()`/`smbios_close()`
  implementing the exception-to-error-code boundary pattern (contract
  C2) - `smbios_open()` detects the "every source failed" case via
  `get_structures_count() == 0` (since `smbios::SMBios()` never throws
  for that case) and catches any other exception as `SMBIOS_ERR_UNKNOWN`;
  `smbios_close()` is a defensive no-throw no-op on `nullptr`.
- `tests/c_abi/compile_as_c_test.c` and `tests/c_abi/smbios_open_close_test.cpp`
  (not yet wired into CMake - Story 03.0's job).

**Tests / gate**
- `compile_as_c_test.c`: compiles clean (zero warnings) under both
  `clang -std=c11` and `clang++ -std=c++20`.
- `smbios_open_close_test.cpp`: compiled and linked manually against the
  real `libsmbios`/`libsmbios_macos`/`libsmbios_posix_legacy` archives;
  run against this host's live SMBIOS table - `smbios_open()` returns
  `SMBIOS_OK` with a non-null handle, `smbios_close()` is safe on both
  that handle and `nullptr`.
- Existing full suite (`ctest --test-dir build/macos`): 4/4 passed, no
  regression (this story's changes are purely additive and not yet
  wired into any CMake target).
- Sanitizer-CI criterion deferred to Story 03.0 - see note above.
