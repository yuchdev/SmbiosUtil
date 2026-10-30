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
  reports coverage/risk deltas (unit vs. mock vs. integration vs. e2e), and
  which test framework (GoogleTest/Catch2) and build system (CMake/CTest)
  the project uses.
- **Build conventions** - any clang-tidy/cppcheck/sanitizer gates CI enforces,
  and the minimum supported compiler/platform matrix.

## Project-specific overrides

Add this project's mandatory rules here: concrete, enforced-by-a-hook-or-CI-gate
rules, not general advice. Each entry should name what enforces it (a CMake
target, a clang-tidy check, a CI gate, a review checklist item) so the rule is
auditable rather than aspirational.
