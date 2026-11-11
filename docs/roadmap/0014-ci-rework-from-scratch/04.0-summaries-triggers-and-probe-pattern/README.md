# Story 04.0 - Summaries, Triggers and the Probe Pattern

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 03.0 (cutover done), Story 01.0 (RC3 records)

## Scope

This story delivers the reader-facing half of the maintainer's request, "maximum useful information on CI problems in a reasonable amount of text":

- a bounded per-job summary and one run-level table (plan E9);
- run economy: triggers, concurrency and tiering (E7);
- a fast path for debugging CI itself (E6);
- the documentation and the milestone exit gate.

## Tasks

| #  | Document | Status | Blocks |
|----|----------|--------|--------|
| 01 | [Job summaries and annotations](01-job-summaries-and-annotations.md) | ⬜ Not started | 04 |
| 02 | [Triggers, concurrency and matrix tiering](02-triggers-concurrency-and-tiering.md) | ⬜ Not started | 04 |
| 03 | [Probe-workflow pattern and single-job runs](03-probe-workflow-pattern-and-single-job-runs.md) | ⬜ Not started | 04 |
| 04 | [Docs and milestone exit gate](04-docs-and-milestone-exit-gate.md) | ⬜ Not started | - |

## Key constraints

- **Summaries are built from RC3/RC4 only,** never by re-reading logs. That keeps them bounded and makes it impossible for them to repeat a log.
- **One failure is said once.** If several legs fail with the same first diagnostic, the run table shows it once with the list of legs.
- **No change to what is checked.** Triggers and tiering change *when* jobs run, never *what* a job checks. A tier that skips a leg says so in the run table (`not run: tier`), so a skipped leg never looks like a passed one.

## Non-goals

- External notifications (chat, e-mail), dashboards, or third-party summary/report actions.
- Retrying failed jobs automatically. A flaky job is a finding to fix, not to hide.

## Open questions

- Plan Q1 (per-push matrix) decides Task 02's tier table.

## Files or directories created or modified by this story

- `scripts/ci/job_summary.py`, `scripts/ci/run_report.py`, `scripts/ci/classify_changes.py` (new) + tests.
- `.github/workflows/ci.yml` (summary steps, `report` job, triggers, concurrency, dispatch inputs).
- `docs/dev/ci-probe-template.yml` (new; a template, deliberately not under `.github/workflows/`).
- `docs/dev/ci.md` (new), root `CLAUDE.md`, `docs/test/test-subsystem.md`, `docs/dev/cpp_coding_standard.md` (Build conventions).
