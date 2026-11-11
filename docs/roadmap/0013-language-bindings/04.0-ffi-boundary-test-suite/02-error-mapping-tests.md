# 02 - Error-mapping tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Prove every one of `smbios_error_t`'s five non-`SMBIOS_OK` values maps
to the correct, documented result in both languages - `SmbiosError`
variant in Rust, the correct `SmbiosError` subclass in Python - not just
the one or two codes a happy-path test happens to trigger naturally
(e.g. `SMBIOS_ERR_INVALID_ARGUMENT` on a type-mismatched typed getter is
easy to trigger for real; `SMBIOS_ERR_NO_TABLE_FOUND` and
`SMBIOS_ERR_UNKNOWN` are harder to force without a real failing source
or a fault-injection seam).

## Files

- **Create** `bindings/rust/smbios/tests/ffi_boundary/error_mapping.rs`
  and **Create**
  `bindings/python/tests/ffi_boundary/test_error_mapping.py`, each
  covering:
  - `SMBIOS_ERR_INVALID_ARGUMENT`: call each of the five curated typed
    getters on a deliberately mismatched entry type, real hardware,
    confirming the correct error variant/exception in both languages
    (this much is already covered by each binding's own Story 02.0/03.0
    Task 02 tests - re-asserted here as part of the shared gate, not
    newly invented).
  - `SMBIOS_ERR_NO_TABLE_FOUND`: on a CI runner with no accessible
    SMBIOS table (per this story's README's Key constraints), confirm
    `Smbios::open()`/`Smbios()` in both languages produce the matching
    error/exception consistently with each other and with a direct
    `smbios_open()` C call made from the same test run (the three-way
    comparison this story exists for).
  - `SMBIOS_ERR_BUFFER_TOO_SMALL`: confirm this is **never** observable
    through either public wrapper API (per both wrapper stories' own
    Constraints) - a test that would fail if a future change in either
    wrapper accidentally started leaking this internal-retry-only code
    to a caller.
  - `SMBIOS_ERR_INVALID_HANDLE` and `SMBIOS_ERR_UNKNOWN`: document
    whether either is reachable through the public wrapper APIs at all
    given each language's own defensive design (Python's use-after-close
    check converts what would otherwise reach the C layer as
    `SMBIOS_ERR_INVALID_HANDLE` into an early `InvalidHandleError` before
    the native call even happens) - if genuinely unreachable through
    normal use, state this explicitly rather than contriving an
    artificial path just to exercise the code, and note it as a
    documentation fact in both wrappers' own error-type docstrings.

## Functions and Classes

N/A - test code only.

## Constraints

- Every error-mapping assertion must be checked against the *same* code
  path a real caller would use (the public wrapper API), not against a
  raw, internal FFI call bypassing the wrapper - this story is about the
  boundary a real consumer crosses, not an implementation-detail check.

## Tests

(This task's "Files" section is itself the test content.)

## Success criteria

- [ ] All five `smbios_error_t` values are either demonstrated reachable
      with the correct mapped result in both languages, or explicitly
      documented as unreachable through the public API with the reason
      why.
- [ ] `SMBIOS_ERR_BUFFER_TOO_SMALL` is confirmed never observable through
      either public wrapper, with a regression test that would fail if
      this changed.
