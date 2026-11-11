# 04 - CI wiring and cross-platform gate

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 03

## Objective

Wire Tasks 01-03 into CI as this milestone's own exit gate, on every
platform Story 01.0 Task 03 scoped as pass/fail-gating, running after
both bindings (Stories 02.0/03.0) and `smbios_c` itself (Story 01.0)
have successfully built in the same workflow run.

## Files

- **Modify** `.github/workflows/ci.yml`: a job depending on the
  `smbios_c` build step, the Rust crate build, and the Python wheel
  build (all added by earlier stories), running:
  1. Story 01.0 Task 02's sync check (already gating earlier, re-run
     here as a final confirmation against the actual built artifacts,
     not only the static declaration files).
  2. This story's Tasks 01-03 (handle lifecycle, error mapping,
     round-trip parity).
  3. A clear, named failure message per task on failure, so a CI
     failure in this job immediately identifies which of the three
     concerns (lifecycle, error mapping, parity) broke, rather than a
     single opaque "tests failed."

## Functions and Classes

N/A - CI wiring only.

## Constraints

- This job must not be allowed to silently skip on a platform where a
  real SMBIOS table is unavailable - per Task 01/02/03's own
  constraints, the degraded "no table" case is itself a tested,
  asserted outcome, not a skip condition.
- Mirror this milestone's own platform floor (Story 01.0 Task 03's
  pass/fail-gating set) exactly - do not silently add a new platform
  here that was not already in scope for either binding's own build.

## Tests

(This task's own deliverable is itself the final CI wiring - its
"test" is a green CI run exercising Tasks 01-03 for real.)

## Success criteria

- [ ] The gate job runs after `smbios_c`, the Rust crate, and the Python
      wheel have all built successfully in the same CI run.
- [ ] All three of Tasks 01-03 run and pass on every pass/fail-gating
      platform.
- [ ] A failure in any one of the three sub-areas is identifiable from
      the CI failure message alone, without needing to read raw test
      output first.
- [ ] This completes milestone 0013's exit criteria, per plan.md's
      Dependency graph.
