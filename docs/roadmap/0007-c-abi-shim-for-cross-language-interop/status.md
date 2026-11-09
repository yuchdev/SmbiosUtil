# Milestone 0007 - C ABI Shim for Cross-Language Interop - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                     | Status         | Tests |
|-------|---------------------------------------------|----------------|-------|
| 01.0  | C ABI core: handles and error model        | ✅ Complete    | manual (pre-CMake) |
| 02.0  | Entry and field accessors C API            | ✅ Complete    | manual (pre-CMake) |
| 03.0  | Build integration and packaging            | 🔶 In progress (2/3 tasks) | - |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Story 03.0 Task 01: the task spec's own CMake example links against a
  target named `smbios` - that named the core library before this
  session's earlier rename (`smbios_util`->`smbios`,
  `libsmbios/smbios`->`libsmbios/libsmbios`, landed on `master` before
  this milestone started). `smbios` now names the CLI executable; the
  core library is `libsmbios`. Linked against `libsmbios` (the correct
  current name), not `smbios`, and not against the CLI target (linking a
  library against an executable is not meaningful for this purpose
  anyway). Also built `smbios_c` as `SHARED`, not the plain static
  archive the spec's literal `add_library(smbios_c ...)` example
  produces: a static `smbios_c` only records a link-time dependency on
  `libsmbios`, it does not merge its objects, so the "a consumer links
  `smbios_c` only" contract (this story's Key constraints) would not
  hold without every downstream consumer separately re-solving the
  whole-archive registration problem. Added an explicit linker export
  allowlist (`smbios_c.exported_symbols` for macOS,
  `smbios_c.version` for a GNU-ld-style `--version-script` elsewhere)
  since `CXX_VISIBILITY_PRESET hidden` alone cannot retroactively hide
  symbols already baked into `libsmbios.a`'s own objects (visibility is
  set at each object's own compile time; `libsmbios`'s own
  `CMakeLists.txt` sets no visibility option). Verified via `nm -gU`:
  exactly the 13 intended `smbios_*` functions are exported, nothing
  else leaks.
- Story 03.0 Task 02's vcpkg-port-vs-manifest decision: deferred full
  vcpkg port publication (`portfile.cmake` + its own `vcpkg.json` under
  a `ports/smbios-c/` layout, registry submission, independent
  versioning) to a future milestone - the task's own Constraints section
  explicitly allows this when the immediate goal is "usable via
  `pkg-config` today," which Task 02 already delivers and verified
  end-to-end (a trivial external C program built from `pkg-config
  --cflags --libs smbios_c` alone, no reference to this repo's source
  tree, ran correctly against the real SMBIOS table). Revisit once a
  real external consumer (the Python/Rust/C# binding this milestone's
  own motivation names) actually needs `vcpkg install smbios-c` to work.
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
- Story 02.0 Task 01: `smbios_entry_handle_t`'s real type
  (`smbios_entry_handle_s`, defined only in `smbios_c.cpp`) carries both
  the entry's `DMIHeader*` and a pointer back to its parent `SMBios*` -
  not just the header - because `smbios_entry_render()`'s fixed signature
  (per plan.md's contract) takes no parent-handle parameter, yet decoding
  an entry correctly needs the parent's SMBIOS version and table bounds
  for version-gated fields. This required replacing Story 01.0 Task 02's
  original bare `reinterpret_cast<smbios::SMBios*>` handle representation
  with a small internal wrapper struct (`smbios_handle_s`) that owns both
  the real `SMBios` and every entry handle ever given out through it -
  freed together at `smbios_close()`, never separately. No change to
  `smbios_open()`/`smbios_close()`'s public signatures or behavior;
  contract C1 explicitly allows the opaque layout to change freely.
- Story 02.0 Task 03's cross-platform criterion ("all C API tests pass on
  real hardware across Windows/Linux/macOS") is deferred to Story 03.0's
  close for the same reason as Story 01.0's CI criterion: there is no
  CMake target yet for any CI job to build or run these tests against.
  Verified on macOS only for now; re-verify on Windows/Linux once Story
  03.0 lands the CMake target and CI exercises it.
- Story 02.0 Task 03's end-to-end C program criterion: `tests/c_abi/smbios_c_example.c`,
  compiled with `clang -std=c11 -Wall -Wextra` (zero warnings) and linked
  against `smbios_c`/`libsmbios` with a C++ driver (required since both
  are implemented in C++ internally - the *compilation* of the consumer
  is what had to be, and was, done with a plain C compiler). Ran
  successfully against this host's real SMBIOS table: printed all 32
  structures' type/length/handle and rendered descriptions, plus the BIOS
  vendor via `smbios_bios_vendor()`.
- While verifying Task 01/02's output byte-for-byte against the C++ API,
  discovered that the manual verification binaries initially omitted the
  whole-archive link flag (`-force_load`) the real `smbios` CLI target
  uses - without it, entry types never directly named in `smbios_c.cpp`
  (e.g. `CacheInformationEntry`) never got their `REGISTER_SMBIOS_ENTRY`
  static initializer linked in, so `SMBiosEntryFactory::create()`
  silently returned `nullptr` for them in both the C API path and the
  test's own reference walk - both sides agreeing on the wrong answer and
  masking the gap. Fixed the verification script, not any shipped code.
  Separately, this also surfaced a real pre-existing bug unrelated to
  this milestone - `PortConnectionEntry::render_to_description()`
  (`libsmbios/src/port_connection_entry.cpp:122`) streams a raw `uint8_t`
  through `operator<<`, which inserts it as a literal character (a NUL
  byte when the value is 0) instead of a number - flagged as a separate
  background task, not fixed here (out of this milestone's scope).

### Story 02.0 - Entry and Field Accessors C API (✅ 2026-11-09)

**Delivered**
- `smbios_entry_count()`, `smbios_entry_at()`, `smbios_entry_type()`/
  `length()`/`handle_id()`, `smbios_entry_render()` (Task 01).
- `smbios_bios_vendor()`/`smbios_bios_version()`, `smbios_system_uuid()`,
  `smbios_memory_device_size()`/`smbios_memory_device_speed()` - the
  exact curated set from `plan.md`'s Key constraints (Task 02).
- `tests/c_abi/smbios_entry_iteration_test.cpp`,
  `tests/c_abi/smbios_typed_getters_test.cpp`,
  `tests/c_abi/smbios_c_example.c` (Tasks 01-03).

**Tests / gate**
- All 32 real structures on this host render byte-for-byte identical to
  the direct C++ `SMBiosEntryFactory::create()`/`render_to_description()`
  calls (verified with the correct whole-archive link - see note above).
- All 5 curated getters match their C++ equivalents exactly; each
  correctly rejects a mismatched entry type with
  `SMBIOS_ERR_INVALID_ARGUMENT`.
- `smbios_c_example.c`: a genuine end-to-end C11 program, compiled and
  run successfully against real hardware.
- Existing full suite (`ctest --test-dir build/macos`): 4/4 passed, no
  regression.
- Cross-platform (Windows/Linux) criterion deferred to Story 03.0 - see
  note above.

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
