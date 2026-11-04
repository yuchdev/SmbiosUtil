# 01 - Fix compiler-ID branch and standard

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Fix the root `CMakeLists.txt`'s compiler-selection branch so it actually
matches on macOS, and bump the Clang branch's C++ standard flag just
enough to compile - confirmed by direct local testing to be `gnu++14`,
not the current `gnu++11` (which predates `std::make_unique`, a C++14
library feature libc++ gates behind the active standard regardless of
which branch sets it).

## Files

- **Modify** root `CMakeLists.txt`:
  ```cmake
  if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")          # was: STREQUAL "Clang"
      message("UNIX congiguration, Clang")
      set(SMBIOS_COMPILE_OPTIONS -stdlib=libc++)
      set(SMBIOS_CXX_STANDARD_FLAG -std=gnu++14)        # was: -std=gnu++11
      set(SMBIOS_LINK_OPTIONS -stdlib=libc++)
      set(SMBIOS_WARNING_OPTIONS -Wall -Wextra)
  elseif(${CMAKE_COMPILER_IS_GNUCXX})
      ...
  ```
  Everything else in the `if`/`elseif`/`elseif` chain (the GCC and WIN32
  branches, the `message(...)` calls) stays exactly as Story 02.0 Task 02
  left it.

## Functions and Classes

N/A - CMake script only.

## Constraints

- `MATCHES "Clang"` (regex, unanchored) is chosen specifically because it
  matches both `"Clang"` (vanilla LLVM Clang) and `"AppleClang"` (Xcode's
  fork) - confirmed this is the actual, real value
  (`CMAKE_CXX_COMPILER_ID` reports `AppleClang` on this project's own
  macOS toolchain, verified via `CMakeCache.txt` during milestone 0001's
  earlier verification pass). It would also match a hypothetical
  `ARMClang`, which is correct, not a side effect to guard against.
- **Do not bump to C++20 or any standard beyond what's needed to
  compile.** `gnu++14` was confirmed (via a standalone `clang++
  -std=gnu++14 -stdlib=libc++ -fsyntax-only` test against a
  `std::make_unique` call) to be sufficient; milestone 0002's Story 01.0
  is where the deliberate, full C++20 raise (with `-Werror`, sanitizers,
  and a from-scratch audit of every warning) happens. Going further here
  would quietly do part of that milestone's job out of order.
- Do not touch the GCC or WIN32 branches - neither is affected by this
  defect (GCC's `CMAKE_COMPILER_IS_GNUCXX` and CMake's built-in `WIN32`
  variable both already evaluate correctly, confirmed by code reading;
  only the literal string-equality check against `"Clang"` was wrong).

## Tests

- Configure on macOS (`cmake --preset macos`) and inspect
  `CMakeCache.txt`/the configure log's `message(...)` output - "UNIX
  congiguration, Clang" must now print on an AppleClang host, where it
  silently printed nothing before.
- A standalone compile of a `std::make_unique`-using translation unit
  with the resulting flags (already proven in principle via
  `clang++ -std=gnu++14 -stdlib=libc++ -fsyntax-only`) should be
  re-confirmed by this task's own implementer, not merely cited by
  reference to this document.

## Success criteria

- [ ] `cmake --preset macos` prints "UNIX congiguration, Clang" (confirms
      the branch is taken).
- [ ] `smbios/src/abstract_smbios_entry.cpp` (the file that first failed
      to compile with `std::make_unique` undefined) compiles cleanly with
      the new flags.
- [ ] GCC and Windows branches are byte-identical to before this task.
