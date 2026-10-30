# Story 01.0 - Language Standard Raise and Compiler Hygiene

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Milestone 0001 (build must already be portable before raising the standard across three toolchains)

## Scope

Move the project's language standard from `gnu++1y`/C++11 to C++20 across
every `CMakeLists.txt`, turn the `-Wall -Wextra` baseline from milestone
0001's Story 02.0 into `-Werror` (`/WX` on MSVC), and add an
AddressSanitizer+UBSanitizer CI job on Linux/macOS so undefined behavior
(like Task 02's missing-`return` bug) is caught by tooling going forward,
not just by a human reviewer.

## Tasks

| #  | Document                                                          | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Raise to C++20](01-raise-to-cpp20.md)                                 | ⬜ Not started | 02     |
| 02 | [Warnings-as-errors and sanitizers](02-warnings-as-errors-and-sanitizers.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                   | ⬜ Not started | -      |

## Key constraints

- Raising the standard must not, by itself, change any observable behavior
  - this task is a compiler-flag change, not a rewrite. Where C++20 makes a
  cleaner spelling available (`std::span`, designated initializers,
  `[[nodiscard]]`, concepts) and the task author wants to use it inline
  while touching a line anyway, that is acceptable *only* if it does not
  change behavior; broad modernization sweeps are explicitly deferred to
  the milestones that already touch each file for a design reason (0003,
  0004, 0005).
- MSVC, GCC and Clang (the three compilers CLAUDE.md's per-compiler flag
  blocks already target) must all build clean with `-Werror`/`/WX` - if one
  compiler has a warning the others don't, fix the code, not the flag.

## Files or directories created or modified by this story

- Root `CMakeLists.txt`, `smbios/CMakeLists.txt`, `smbios_util/CMakeLists.txt`
  - `CXX_STANDARD 20`, `-Werror`/`/WX`.
- `.github/workflows/ci.yml` (from milestone 0001) - adds a sanitizer job.
- Any source file that fails to compile clean under C++20 or under
  `-Werror` - fixed in place, scoped to the compiler's actual complaint.
