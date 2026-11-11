# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Prove, end to end and outside this repo's own build tree, that
`find_package(libsmbios CONFIG)` + `target_link_libraries(consumer PRIVATE
libsmbios::libsmbios)` is correct by construction - the ADR 0004 DD1
footgun (an entry type never named directly by the consumer silently
failing to decode) is closed - closing out Story 01.0.

This is a **packaging-correctness verification task, not a unit test.**
It does not add a Boost.Test case under `tests/unit|mock|perf|integration/`
(`docs/dev/cpp_coding_standard.md`'s four categories); it is a clean-room
consumer smoke test run as its own CI step, with no reference to this
repo's source tree from the consumer side.

## Files

- **Create** a scratch consumer fixture (exact location decided during
  implementation - e.g. `tests/packaging/libsmbios_consumer/`, built and
  run only from a separate CI step, never wired into the main `ctest`
  suite): a minimal `CMakeLists.txt` + one `.cpp` that does:
  ```cpp
  #include <libsmbios/smbios.hpp>
  #include <libsmbios/smbios_entry_factory.hpp>
  // Deliberately constructs an SMBios and walks its headers through
  // SMBiosEntryFactory::create() WITHOUT naming any concrete entry class
  // (e.g. BiosInformationEntry) directly anywhere in this file - the
  // exact shape of bug this task exists to prove is closed. A type whose
  // class is never named here (e.g. type 17, MemoryDeviceEntry) must
  // still render successfully, proving its REGISTER_SMBIOS_ENTRY
  // initializer ran via the exported target's baked-in whole-archive
  // link alone.
  ```
  with **zero** whole-archive/force_load/WHOLEARCHIVE lines of its own -
  only `find_package(libsmbios CONFIG REQUIRED)` +
  `target_link_libraries(consumer PRIVATE libsmbios::libsmbios)`.
- **Modify** `.github/workflows/ci.yml` (or a new workflow file,
  implementation's choice): a step that runs `cmake --install` into a
  scratch prefix, then configures/builds/runs the fixture above against
  that prefix via `CMAKE_PREFIX_PATH`, on all three OSes.

## Success criteria

- [ ] The fixture configures, builds, links, and runs successfully on
      Windows, Linux, and macOS, against a real installed `libsmbios`
      package - with no whole-archive flag of its own.
- [ ] The fixture's run output confirms at least one entry type whose
      concrete class is never named in the fixture's own source renders
      correctly (not `nullptr`/empty) - the actual regression test for
      the DD1 footgun, not just "it linked."
- [ ] A deliberate negative check during implementation (temporarily
      removing the baked-in `INTERFACE_LINK_OPTIONS` and re-running the
      fixture) reproduces the original bug (that unnamed entry type comes
      back `nullptr`) - confirming the fixture would actually catch a
      regression, not just a fixture that happens to pass either way.
      Recorded as a one-time implementation check in `status.md`, not a
      permanent CI step.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems.
- [ ] This story's and the milestone's `status.md` are updated.
