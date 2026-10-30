# 02 - Remove manual Boost paths

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Delete the root `CMakeLists.txt` logic that branches on
`WINDOWS_BOOST_DIR` / `MY_BOOST_DIR` and sets `BOOST_ROOT`/`BOOST_LIBRARYDIR`
by hand, now that vcpkg's toolchain file makes `find_package(Boost ...
COMPONENTS iostreams program_options unit_test_framework date_time)` resolve
correctly on its own.

## Files

- **Modify** root `CMakeLists.txt`: remove the `if(WIN32) ... WINDOWS_BOOST_DIR
  ... else() ... MY_BOOST_DIR ...` block; keep the single
  `find_package(Boost ... REQUIRED COMPONENTS ...)` call unconditional
  across platforms.
- **Modify** `smbios/CMakeLists.txt` and `smbios_util/CMakeLists.txt` only
  if either duplicates any Boost-path logic rather than inheriting it from
  the root scope.

## Functions and Classes

N/A.

## Constraints

- Do not remove or weaken the per-compiler flag blocks (Clang
  `-stdlib=libc++`, GCC `-pthread -fPIC`, Windows `/EHsc`) - those are
  unrelated to dependency resolution and stay as-is until Story 02.0's
  target-hygiene task revisits them.
- If a CI machine or a developer's machine still has a system Boost that
  `find_package` could pick up ahead of vcpkg's, the toolchain file (not an
  `if(EXISTS ...)` fallback in this project's own `CMakeLists.txt`) is the
  mechanism that must take precedence - do not add a manual search-path
  fallback that reintroduces the problem this task removes.

## Success criteria

- [ ] `grep -r "WINDOWS_BOOST_DIR\|MY_BOOST_DIR" CMakeLists.txt smbios/
      smbios_util/` returns no matches.
- [ ] A clean checkout builds on Windows, Linux and macOS using only the
      vcpkg toolchain file - no `-DBOOST_ROOT` or similar override needed.
