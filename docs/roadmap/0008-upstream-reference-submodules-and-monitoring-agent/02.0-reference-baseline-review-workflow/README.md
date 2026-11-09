# Story 02.0 - Reference Baseline Review Workflow

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete
**Depends on:** Story 01.0 (the manifest and fetch script must exist)

## Scope

This story documents exactly how an upstream's `baseline_sha` in
`scripts/upstream_references.json` gets advanced (R3):
- **when**: only after a watcher report has triaged the range;
- **how**: a human-reviewed manifest-edit PR;
- **what** must be re-checked: named files in this tree.

Without this procedure the "last triaged" marker either never moves or
moves without review.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Baseline-advance procedure](01-pinning-policy-and-bump-procedure.md) | ✅ Complete    | 03     |
| 02 | [CI submodule checkout verification](02-ci-submodule-checkout-verification.md) (superseded by [01.0/02](../01.0-upstream-reference-fetch-script-and-license-isolation/02-build-isolation-guard.md)) | ⛔ Superseded | -      |
| 03 | [Tests](03-tests.md)                                                | ✅ Complete    | -      |

## Key constraints

- **No automated baseline advance ever merges on its own.** Every
  manifest change is a normal, human-reviewed PR, even when the Story
  03.0 agent's report recommended it (R3). The agent never edits the
  manifest.
- The procedure names, per upstream, the real current files in this tree
  that are its "downstream". A baseline-advance PR can then point a
  reviewer at exactly what to re-check, instead of leaving them to
  rediscover it.

## Files or directories created or modified by this story

- `docs/dev/upstream_reference_review.md` (new).
- No CI change. The former pin-drift CI step (Task 02) is superseded
  because there are no pins to drift from.
