# 01 - Raise to C++20

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Change the configured C++ standard from `gnu++1y` (GCC/Clang) / whatever
MSVC defaults to today, to C++20 uniformly across both library and CLI
targets, and fix whatever stops compiling as a result (expected to be
minimal - this codebase already uses `unique_ptr`, `override`, scoped
enums, and other C++11/14 idioms cleanly).

## Files

- **Modify** root `CMakeLists.txt`: replace the `gnu++1y` compiler-flag
  spelling with `set_target_properties(<target> PROPERTIES CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)` (or the per-target
  equivalent) for both `smbios` and `smbios_util`.
- **Modify** any source file that fails to compile under C++20 strict mode
  (`CXX_EXTENSIONS OFF` removes GNU extensions the old `gnu++1y` spelling
  implicitly allowed) - fix forward, do not reintroduce `CXX_EXTENSIONS ON`
  to paper over it.

## Functions and Classes

N/A - no new types; existing ones must keep compiling.

## Constraints

- `CXX_EXTENSIONS OFF` is deliberate: the project's own CLAUDE.md already
  distinguishes "Clang uses `-stdlib=libc++`, GCC `-pthread -fPIC`" as
  separate, intentional per-compiler choices from language-extension usage;
  this task should not depend on any GNU extension going forward.
- Boost headers (via vcpkg, from milestone 0001) must be confirmed compatible
  with C++20 at the pinned baseline version - if not, bump the pinned vcpkg
  baseline (not downgrade the language standard) to a version where they are.

## Success criteria

- [ ] `smbios` and `smbios_util` both build under C++20 on GCC, Clang and
      MSVC with no `CXX_EXTENSIONS`.
- [ ] No test behavior changes - the existing functional test suite's
      assertions are unchanged (this is a pure toolchain change).
- [ ] CLAUDE.md's "Build and test" section is updated to say C++20 instead
      of "C++11/`gnu++1y`".
