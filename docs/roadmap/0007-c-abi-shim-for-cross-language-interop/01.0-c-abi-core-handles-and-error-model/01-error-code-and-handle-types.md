# 01 - Error code and handle types

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C1** exactly: `smbios_handle_t`, `smbios_entry_handle_t`,
and `smbios_error_t`, in a header that compiles clean under both a C and a
C++ compiler.

## Files

- **Create** `smbios_c/include/smbios_c/smbios_c.h` (the opaque types and
  error enum from contract C1, plus an `#include <stddef.h>` for `size_t`
  and `#include <stdint.h>` for fixed-width integers - no other includes).

## Functions and Classes

- `smbios_handle_t`, `smbios_entry_handle_t` - new opaque pointer typedefs.
- `smbios_error_t` - new enum.

## Constraints

- The incomplete-struct-pointer idiom (`typedef struct smbios_handle_s*
  smbios_handle_t;` with `smbios_handle_s` never defined in the public
  header) is used specifically so callers cannot dereference, copy by
  value, or inspect the handle's layout - only this library's own
  functions (Story 01.0 Task 02 onward) ever see the real type behind it
  (a `reinterpret_cast` to/from `smbios::SMBios*` inside `smbios_c.cpp`).
- `smbios_error_t` is a plain C `enum` (not `enum class`, since this
  header must compile as C) with explicit values - never renumber an
  existing value once this ships (ABI stability for the values
  themselves, even though this milestone defers a stability *guarantee*
  for the API as a whole per its Non-goals).

## Tests

- `tests/c_abi/compile_as_c_test.c` (new): a trivial `.c` file that
  `#include <smbios_c/smbios_c.h>` and compiles under a C compiler as part
  of the build, proving the header is genuinely C-compatible and not
  merely "C++ with `extern "C"`" that happens to also parse.

## Success criteria

- [ ] `smbios_c.h` compiles under `gcc -std=c11`/`clang -std=c11` with no
      warnings.
- [ ] `smbios_c.h` compiles under a C++ compiler with no warnings.
- [ ] No struct field of `smbios_handle_s`/`smbios_entry_handle_s` is
      visible in the public header.
