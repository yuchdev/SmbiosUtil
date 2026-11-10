# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the GitHub Releases baseline works end to end from a genuinely
clean machine, closing out Story 03.0 and milestone 0012 - directly
re-running ADR 0004's own Validation/Rollout item: "The GitHub Releases
baseline (option 3) is validated by downloading a tagged release's
artifact on a clean machine with no vcpkg/Conan/this repo's source
present and confirming `pkg-config --cflags --libs smbios_c` plus a
compile/link/run smoke test succeeds from the extracted tarball alone."

This is a **packaging-correctness verification task, not a unit test** -
no Boost.Test case is added; verification happens against a downloaded
artifact in a disposable environment (a fresh CI container/VM, or a
local scratch machine during implementation), never against this
repo's own build tree.

## Files

No new test source beyond a verification script (location decided during
implementation, e.g. `scripts/verify_release_artifact.sh`/`.ps1`) that:
1. Downloads (or accepts a path to) a release tarball/zip.
2. Extracts it into an empty directory.
3. Runs `pkg-config --cflags --libs smbios_c` against the extracted
   `lib/pkgconfig/smbios_c.pc` and compiles/links/runs a trivial C
   program against the result (reusing milestone 0007's own verification
   shape).
4. Runs a second smoke test against the extracted `libsmbios` package
   config (Story 01.0) - a trivial C++ program using
   `find_package(libsmbios CONFIG)` against the extracted
   `lib/cmake/libsmbios/` directory via `CMAKE_PREFIX_PATH`.
5. Verifies the `.sha256` checksum (Task 02) matches the downloaded
   artifact.

## Success criteria

- [ ] On each of Windows, Linux, and macOS, a released artifact
      extracted on a clean machine (no vcpkg, no Conan, no checkout of
      this repo's source) passes all four checks above.
- [ ] The `smbios_c` smoke test produces correct output against real
      SMBIOS data (or degrades cleanly to "no table found," matching
      `smbios_open()`'s documented behavior).
- [ ] The `libsmbios` smoke test links successfully with **no**
      whole-archive flag of its own (re-confirming Story 01.0's Task 03
      result, this time from a release tarball rather than a local
      `cmake --install`).
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems.
- [ ] This story's, and milestone 0012's overall, `status.md` are
      updated to reflect every story's final state.
