# Story 04.0 - FFI Boundary Test Suite (Both Languages)

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (Rust crates exist), Story 03.0 (Python
package exists)

## Scope

Where Story 02.0 Task 04 and Story 03.0 Task 04 each close out their own
language's *unit-level* wrapper correctness, this story is the
cross-cutting, both-languages-together suite that specifically exercises
the **FFI boundary itself** - handle lifecycle under stress, every
`smbios_error_t` value's mapping to each language's error convention,
and a three-way round-trip comparison (raw `smbios_c` C calls vs. the
Rust wrapper vs. the Python wrapper, against the same real-or-synthetic
SMBIOS source) - and wires all of it into CI as this milestone's own
exit gate. This is the "not just compiles" requirement plan.md's parent
task calls for, applied once at the point where both bindings exist to
be compared against each other and against the ground truth
(`smbios_c` itself).

## Tasks

| #  | Document                                                              | Status         | Blocks |
|----|----------------------------------------------------------------------------|----------------|--------|
| 01 | [Handle lifecycle tests](01-handle-lifecycle-tests.md)                    | ⬜ Not started | 02     |
| 02 | [Error-mapping tests](02-error-mapping-tests.md)                          | ⬜ Not started | 03     |
| 03 | [Cross-language round-trip parity](03-cross-language-round-trip-parity.md) | ⬜ Not started | 04     |
| 04 | [CI wiring and cross-platform gate](04-ci-wiring-and-cross-platform-gate.md) | ⬜ Not started | -      |

## Key constraints

- Every test in this story must exercise **real FFI calls through the
  real `smbios_c` shared library** - no mocking of the native layer in
  either language, since the entire point of this story is proving the
  boundary itself (marshaling, handle lifetime, error-code translation)
  behaves correctly, not that either wrapper's own Rust/Python-side
  logic is internally consistent (that is each binding story's own Task
  04's job).
- Where real hardware exposes no SMBIOS table (e.g. a sandboxed CI
  runner with no privilege, mirroring the exact situation this
  project's own `tests/integration`/`tests/perf` C++ suites already
  handle per root `CLAUDE.md`), this story's tests fall back to a
  synthetic source if one is available to `smbios_c` itself, or assert
  the shared "clean, no-table" result consistently across all three
  code paths (raw C, Rust, Python) rather than skipping silently - a
  `NoTableFound`-equivalent result must be the *same* across all three,
  not just individually clean.
- This story does **not** modify `smbios_c`'s own C++ test suite
  (`tests/c_abi/`, milestone 0007) - it adds new tests in the binding
  languages themselves, exercising the same contract from the other
  side of the FFI call.

## Files or directories created or modified by this story

- `bindings/rust/smbios/tests/ffi_boundary/` (new) - Rust-side cases for
  Tasks 01-03.
- `bindings/python/tests/ffi_boundary/` (new) - Python-side cases for
  Tasks 01-03.
- `scripts/compare_binding_round_trip.py` (or equivalent) - the
  three-way comparison harness for Task 03, if a standalone script is
  simpler than embedding the comparison logic in either language's own
  test runner.
- `.github/workflows/ci.yml` - the cross-language gate job (Task 04).
