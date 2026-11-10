# libbios C++ Style Guide

Referenced by `cpp-expert` and `testing-expert` (`.claude/agents/`). This is a
starting skeleton, not a finished standard - fill in the project-specific
rules below before relying on it.

## Contents

- **Style** - formatting, naming, namespace structure, and the project's
  C++ standard version (e.g., C++17/20/23) and compiler flag baseline
  (`-Wall -Wextra -Wpedantic` or the MSVC equivalent).
- **Doxygen** - what requires a doc comment (every public class, function,
  template, and non-obvious data member).
- **Ownership conventions** - `std::unique_ptr` vs. `std::shared_ptr` vs.
  non-owning raw pointer/reference; when (if ever) raw `new`/`delete` is
  permitted.
- **Testing** - the test classification scheme `testing-expert` uses when it
  reports coverage/risk deltas: **unit, mock, perf, integration** (this
  project's actual four categories - no `e2e`; a hardware-inventory CLI has
  no end-to-end user journey distinct from "integration," and "perf" is a
  first-class category here because the project already ships a
  performance-timing suite). Framework: **Boost.Test** (`BOOST_AUTO_TEST_SUITE`/
  `BOOST_AUTO_TEST_CASE`), not GoogleTest/Catch2 - this project's two original
  suites were already Boost.Test, and the vcpkg manifest declares `boost-test`
  specifically for it. Build system: CMake + CTest (`enable_testing()` in the
  root `CMakeLists.txt`; each category is its own executable registered via
  `add_test(...)`).
- **Build conventions** - any clang-tidy/cppcheck/sanitizer gates CI enforces,
  and the minimum supported compiler/platform matrix.

## Project-specific overrides

Add this project's mandatory rules here: concrete, enforced-by-a-hook-or-CI-gate
rules, not general advice. Each entry should name what enforces it (a CMake
target, a clang-tidy check, a CI gate, a review checklist item) so the rule is
auditable rather than aspirational.

### Test classification and layout (enforced by `tests/CMakeLists.txt`)

Four categories, one top-level directory and one Boost.Test executable each,
all discovered by `ctest` (root `CMakeLists.txt` calls `enable_testing()`):

| Category      | Directory           | Target                 | Needs root/admin? | Always expected green? |
|---------------|----------------------|-------------------------|--------------------|--------------------------|
| Unit          | `tests/unit/`        | `smbios_unit_test`      | No                 | Yes                      |
| Mock          | `tests/mock/`        | `smbios_mock_test`      | No                 | Yes (currently a documented placeholder - no injectable seam exists until milestone 0005's `ISmbiosSource`) |
| Perf          | `tests/perf/`        | `smbios_perf_test`      | Yes (real hardware) | No - fails cleanly without privileges, not a regression |
| Integration   | `tests/integration/` | `smbios_integration_test` | Yes (real hardware) | No - same as Perf |

CTest-level smoke tests (`smbios_cli_*`, `smbios_example_*`) are classified
**Integration**: they exercise the built `smbios` binary or an example program
rather than a Boost.Test case. `smbios_example_*` and the hardware-free
`smbios_cli_unknown_format*` tests are the must-pass exception - they degrade by
design and are in CI's must-pass `ctest -R` regex - whereas `smbios_cli_format_csv`
reads real hardware and is grouped with `smbios_integration_test`/`smbios_perf_test`
in CI's warning-only step.

A new structure/entry type/parsing rule gets a **unit** test first (a
hand-built `DMIHeader`/raw buffer, no real `SMBios`/hardware) - see
`tests/unit/unit_tests.cpp`'s `build_port_connection_buffer()` helper for the
pattern. Escalate a bug a unit test surfaces to `cpp-expert` for a fix in the
same pass, per `testing-expert`'s own rule - never leave a known-wrong
behavior as a permanently-red test.
