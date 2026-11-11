# 01 - Handle lifecycle tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Prove, in both languages, that the opaque handle contract (milestone
0007 contract C1/C2: `smbios_open`/`smbios_close`, non-owning entry
views tied to their parent's lifetime) is respected under conditions a
simple happy-path test would not exercise: repeated open/close cycles,
many entries retained simultaneously, and - specifically for Python,
since it lacks Rust's compile-time enforcement - deliberate
use-after-close attempts.

## Files

- **Create** `bindings/rust/smbios/tests/ffi_boundary/handle_lifecycle.rs`:
  - Open/close `SmbiosHandle` N times in a loop (no leak under a debug
    allocator or Miri where FFI allows).
  - Hold every `Entry` from one `SmbiosHandle` simultaneously via
    `entries().collect()`, confirm all remain valid until the parent is
    dropped.
  - Confirm (via the existing `compile_fail` test from Story 02.0 Task
    02, re-asserted here as this story's own regression anchor) that an
    `Entry` cannot outlive its parent.
- **Create** `bindings/python/tests/ffi_boundary/test_handle_lifecycle.py`:
  - Open/close `Smbios` N times in a loop (no leak, confirmed via a
    reference-count/`gc` check or an external memory-growth check over
    many iterations).
  - Hold every `Entry` from one `Smbios` simultaneously via
    `list(table)`, confirm all remain valid until `close()`.
  - **Deliberate use-after-close**: call `close()`, then call a method
    on a previously obtained `Entry` - must raise `InvalidHandleError`
    (Story 03.0 Task 02's runtime check), never crash the interpreter,
    confirmed with a process-level test (not just an in-process
    exception catch) so a native crash would actually be observed as a
    test failure, not swallowed.
  - Confirm `close()` called twice, and `__exit__` after an explicit
    `close()`, are both safe no-ops.

## Functions and Classes

N/A - test code only; no new production API.

## Constraints

- The Python use-after-close test must run the actual risky call in a
  subprocess (or equivalent process-isolation mechanism) at least once
  in CI, so that if the defensive check in Story 03.0 Task 02 is ever
  removed or bypassed, the resulting native crash fails the test suite
  visibly instead of taking down the entire test runner silently or
  (worse) passing by accident.
- Rust's equivalent guarantee is compile-time (the `compile_fail` test),
  so no analogous runtime use-after-free test is meaningful there -
  state this asymmetry explicitly in the test file's own comments
  rather than attempting to force a runtime test Rust's type system
  already makes impossible to write.

## Tests

(This task's "Files" section is itself the test content - there is no
separate test-of-the-tests here, consistent with this story's own
nature.)

## Success criteria

- [ ] Repeated open/close cycles show no leak in either language, over
      at least 100 iterations in CI.
- [ ] Python's use-after-close test observes a clean `InvalidHandleError`
      in a process-isolated run, not a crash.
- [ ] Rust's `compile_fail` lifetime test is re-confirmed as part of this
      story's own CI run (not only Story 02.0's).
