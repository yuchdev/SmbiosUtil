# 02 - Extend physical-memory guards to __APPLE__

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (needs a working C++14 standard to actually compile against)

## Objective

Add `__APPLE__` to the three `#if` guards that currently list
`__linux__`/`__FreeBSD__`/`__NetBSD__`/`__OpenBSD__`/`__sun` explicitly
but never macOS - confirmed by direct local compilation to be the second,
independent cause of `smbios` not compiling on macOS (every symbol in
`unix_bios.h`'s `SMBiosImpl` and `posix_physical_memory.h`'s
`NativePhysicalMemory` vanishes entirely on macOS, while
`smbios.cpp`/`physical_memory.cpp` still construct them through
`std::make_unique`, which requires a complete type).

## Files

- **Modify** `smbios/include/smbios/unix_bios.h` (both the opening guard
  and its matching closing `#endif` comment):
  ```cpp
  #if defined(__APPLE__) || defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
  ```
- **Modify** `smbios/src/unix_bios.cpp` - same guard, both places it
  appears (opening and closing).
- **Modify** `smbios/include/smbios/posix_physical_memory.h` - same
  guard, both places.
- **Modify** `smbios/src/posix_physical_memory.cpp` - same guard, the one
  place it appears.
- **Modify** `smbios/src/physical_memory.cpp`'s `#elif` (the one that
  chooses which header to `#include`):
  ```cpp
  #elif defined(__APPLE__) || defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
  #include <smbios/posix_physical_memory.h>
  ```

## Functions and Classes

N/A - preprocessor guard conditions only; no function body, class member,
or logic changes anywhere in any of these five files.

## Constraints

- **Do not change anything else in these files.** In particular, do not
  "fix" `unix_bios.cpp`'s `sysfs_table_exists()` (hardcoded `false`) or
  `efi_table_exists()`/`reading_from_efi()` to do anything smarter on
  macOS - those stay exactly as they are; on macOS they will now compile
  and simply always report "not found," which is correct, honest
  behavior for this story's scope (see the parent README's Key
  constraints: "compiles" is the mandate, "works" is milestone 0005's).
- Do not change `posix_physical_memory.cpp`'s hardcoded `"/dev/mem"` path
  or its `sysconf(_SC_PAGESIZE)`/`getpagesize()` logic - both are
  POSIX-standard and already confirmed (via a real local `-fsyntax-only`
  compile against this exact file, through the exact Boost.Iostreams
  header this project uses) to need no change to compile on macOS; `/dev/mem`
  opening is expected to fail at *runtime* there (SIP), which is outside
  this task's scope to address.
- `unix_bios.cpp`'s pre-existing `-Wreturn-type` warning in
  `get_table_size()` (milestone 0002's defect #2) will now actually
  surface in a macOS build for the first time (it was previously inside
  dead code that never compiled there at all) - do **not** fix it in this
  task; it is milestone 0002's Story 02.0 to fix, and fixing it here
  would blur which milestone's diff actually resolved it.

## Tests

- `tests/unit/` has no existing fixture for this (confirmed: this
  codebase's only tests today are the real-hardware functional/
  performance suites, pre-milestone-0006) - this task's own verification
  is the real local compile described in Task 03, not a new unit test.

## Success criteria

- [ ] `grep -c "__APPLE__"` on each of the five files shows the guard was
      added exactly where specified, nowhere else.
- [ ] `unix_bios.h`'s `SMBiosImpl` and `posix_physical_memory.h`'s
      `NativePhysicalMemory` are concrete (non-empty) types when compiled
      with `__APPLE__` defined.
- [ ] No other guard condition anywhere in `smbios/` (e.g. `win_bios.hpp`,
      `win_physical_memory.h`) is touched.
