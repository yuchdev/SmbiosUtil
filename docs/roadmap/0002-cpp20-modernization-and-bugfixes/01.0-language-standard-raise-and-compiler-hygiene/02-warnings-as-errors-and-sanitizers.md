# 02 - Warnings-as-errors and sanitizers

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (C++20 must already compile clean before promoting warnings to errors)

## Objective

Promote the `-Wall -Wextra` baseline (added build-system-only in milestone
0001) to `-Werror` (`/WX` on MSVC), explicitly enabling `-Wreturn-type`
(already implied by `-Wall`, called out here because it is the exact
warning that would have caught Story 02.0's `get_table_size()` bug) and add
an AddressSanitizer+UndefinedBehaviorSanitizer CI job on Linux/macOS.

## Files

- **Modify** root `CMakeLists.txt` / `smbios/CMakeLists.txt` /
  `smbios_util/CMakeLists.txt`: `-Werror`/`/WX` on the `smbios` and
  `smbios_util` targets.
- **Modify** `.github/workflows/ci.yml` (milestone 0001): add a
  `sanitizers` job building with `-fsanitize=address,undefined` and running
  the test suite's unprivileged subset.
- **Modify** any source file that is currently relying on a warning staying
  non-fatal - fixed in place.

## Functions and Classes

N/A.

## Constraints

- If a third-party header (Boost) emits a warning under `-Wextra` that this
  project cannot fix, isolate it with `#pragma GCC/clang diagnostic
  push/ignored` + `pop` scoped tightly around the `#include`, not a
  blanket `-Wno-*` on the whole target.
- The sanitizer job's unprivileged subset is whatever milestone 0001's
  Story 02.0 Task 03 already identified as runnable without root/admin; do
  not attempt to grant the CI runner elevated privileges just to satisfy
  this task - that is milestone 0006's scope.

## Success criteria

- [ ] `smbios` and `smbios_util` build with zero warnings under
      `-Wall -Wextra -Werror` (`/W4 /WX`) on GCC, Clang and MSVC.
- [ ] A new sanitizer CI job exists and is green.
- [ ] Re-introducing the Story 02.0 missing-`return` bug locally and
      rebuilding under this task's flags reproduces a build failure
      (manually verified once, as proof the gate actually catches it).
