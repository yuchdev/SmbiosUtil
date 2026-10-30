# 02 - smbios_open/close lifecycle

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Implement `smbios_open()`/`smbios_close()` establishing contract **C2**'s
exception-boundary pattern, so Story 02.0's accessor functions have a
working handle to operate on and a proven pattern to copy.

## Files

- **Modify** `smbios_c/include/smbios_c/smbios_c.h`:
  ```c
  smbios_error_t smbios_open(smbios_handle_t* out_handle);
  void smbios_close(smbios_handle_t handle); /* no-op on NULL; never fails */
  ```
- **Modify** `smbios_c/src/smbios_c.cpp`: implement both per contract C2 -
  `smbios_open()` constructs a `smbios::SMBios` (which may itself throw,
  per milestone 0005's `ISmbiosSource` chain exhausting every source) and
  `reinterpret_cast`s the result into the opaque handle; `smbios_close()`
  `delete`s it and is safe to call on a `nullptr` handle.

## Functions and Classes

- `smbios_open(smbios_handle_t*) -> smbios_error_t` - new.
- `smbios_close(smbios_handle_t) -> void` - new.

## Constraints

- `smbios_close()` itself must never throw or propagate an exception -
  not even via contract C2's try/catch pattern, since a `void`-returning
  cleanup function has no error channel to report through; if the
  underlying `~SMBios()` destructor could theoretically throw (it
  currently cannot - `SMBios::~SMBios()` is an empty, non-throwing
  destructor), `smbios_close()` must still `catch (...) {}` defensively as
  a last resort, per the project's own C++ coding standard's general rule
  against letting destructors throw.
- `smbios_open()` on failure sets `*out_handle = nullptr` and returns a
  specific, non-`SMBIOS_OK` `smbios_error_t` - a caller must be able to
  rely on a null handle whenever the return code is not `SMBIOS_OK`.

## Tests

- `tests/c_abi/smbios_open_close_test.cpp` (new, C++ test file calling
  the C API - acceptable since the test itself doesn't need to be C, only
  the API under test does): `smbios_open()` on real hardware returns
  `SMBIOS_OK` with a non-null handle; `smbios_close()` on that handle and
  then on `nullptr` both succeed without crashing.

## Success criteria

- [ ] `smbios_open()`/`smbios_close()` work correctly on real hardware.
- [ ] No C++ exception escapes `smbios_open()` under any failure mode
      (verified by forcing every `ISmbiosSource` to fail in a test
      double, if milestone 0005's chain is injectable for testing - record
      in `status.md` if it is not, and what was done instead).
