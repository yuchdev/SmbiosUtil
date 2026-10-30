# Story 01.0 - C ABI Core: Handles and Error Model

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Milestone 0004 (stable entry model/factory)

## Scope

Define the opaque handle types and error enum (contract **C1**), the
exception-to-error-code boundary pattern (contract **C2**), and the
handle-lifecycle functions (`smbios_open`/`smbios_close`) that establish
both for every later function in Story 02.0 to follow.

## Tasks

| #  | Document                                                  | Status         | Blocks |
|----|----------------------------------------------------------------|----------------|--------|
| 01 | [Error code and handle types](01-error-code-and-handle-types.md) | ⬜ Not started | 02     |
| 02 | [smbios_open/close lifecycle](02-smbios-open-close-lifecycle.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                           | ⬜ Not started | -      |

## Key constraints

- No `std::string`, `std::vector`, or any C++ standard-library type
  crosses the boundary in either direction - every exported function's
  signature uses only C-ABI-safe types (`const char*` + length, raw
  pointers, fixed-width integers, the opaque handle typedefs).
- `smbios_c/include/smbios_c/smbios_c.h` is a pure C header - it must
  compile under a C compiler (`gcc -std=c11 -c smbios_c.h`-style check),
  not just under a C++ compiler with `extern "C"` ignored.

## Files or directories created or modified by this story

- `smbios_c/include/smbios_c/smbios_c.h` (new) - the public C header.
- `smbios_c/src/smbios_c.cpp` (new) - the C++ implementation behind it.
