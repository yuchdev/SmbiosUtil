# Story 02.0 - Submodule Update and Pinning Workflow

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (submodules must exist)

## Scope

Document exactly how a submodule's pinned commit gets bumped (R3) -
who/what proposes it, who approves it, what must be re-verified - and add
a CI check that catches pin/working-tree drift, so the pinning this
milestone establishes doesn't silently rot.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Pinning policy and bump procedure](01-pinning-policy-and-bump-procedure.md) | ⬜ Not started | 02     |
| 02 | [CI submodule checkout verification](02-ci-submodule-checkout-verification.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                | ⬜ Not started | -      |

## Key constraints

- **No automated bump ever merges on its own** - every pin change is a
  normal, human-reviewed PR, even when Story 03.0's agent is the one that
  proposed it (R3).
- The bump procedure must explicitly name which of this project's own
  files (`unix_bios.cpp`'s TODOs, milestone 0003's field-table
  implementations, milestone 0005's source implementations) are the
  "downstream" of each submodule, so a bump's PR description can point a
  reviewer at exactly what to re-check - not leave them to rediscover it.

## Files or directories created or modified by this story

- `docs/dev/submodule_bump_procedure.md` (new).
- `.github/workflows/ci.yml` (milestone 0001) - submodule-pin
  verification step.
