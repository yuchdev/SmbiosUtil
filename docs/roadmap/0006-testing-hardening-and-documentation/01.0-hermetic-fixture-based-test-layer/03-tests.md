# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Wire the migrated hermetic suite into CI as a real, unprivileged,
assertion-running job (R3), closing the gap milestone 0001's Story 02.0
Task 03 left as build-only, and close out Story 01.0.

## Files

- **Modify** `.github/workflows/ci.yml` (milestone 0001): add (or convert
  an existing build-only job into) a `hermetic-tests` job running the full
  migrated unit-test suite on all three OSes, with no elevated privileges
  requested.

## Success criteria

- [ ] CI runs the full hermetic suite, unprivileged, on Windows, Linux
      and macOS, on every push.
- [ ] The real-hardware functional suite's CI job (if any, per milestone
      0001's original build-only compromise) is now clearly labeled as
      the privileged, real-hardware-dependent complement to this
      unprivileged suite - not a redundant duplicate.
- [ ] Milestone 0006's `status.md` Story 01.0 row is updated to ✅.
