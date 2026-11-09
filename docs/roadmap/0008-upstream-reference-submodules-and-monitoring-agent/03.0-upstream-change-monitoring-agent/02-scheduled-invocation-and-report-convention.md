# 02 - Invocation and report convention

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

> File name kept as `02-scheduled-invocation-and-report-convention.md` so
> existing links resolve. The scheduled option is **rejected**: the user
> decided on 2026-11-09 that this agent runs on demand only (see
> [status.md](../status.md)).

## Objective

Document how `dmidecode-upstream-watcher` runs (**on demand only**), and
register its report type in `docs/reviews/README.md`'s naming-convention
table.

## Files

- **Modify** `docs/reviews/README.md`: add a row for
  `upstream-watch-<YYYY-MM-DD>.md` alongside the existing `dep-audit`/
  `license-check` examples. In that row, or in the agent definition's
  description, state that the agent is invoked on demand.
- **Document the invocation (on demand).** A developer, or another agent
  following `agent-orchestrator.md`'s cross-agent coordination pattern,
  invokes the agent manually, for example before starting new work on
  milestone 0003/0005 logic. The run happens on a developer machine with
  network access, because it runs the fetch script. It requires no
  standing automation, no CI workflow, and no Claude-in-CI credentials.
  The resulting report is committed through a normal PR.

## Functions and Classes

N/A.

## Constraints

- There is no `schedule:` cron, no `workflow_dispatch` workflow, and no
  other GitHub Actions job for this agent (user decision, 2026-11-09).
  Adding one later would be a new decision, not part of this milestone.
- The agent's report goes through this project's normal PR review before
  anything it recommends is acted on (milestone Key constraints). That
  includes any recommended `baseline_sha` advance, which a human applies
  under `docs/dev/upstream_reference_review.md`.

## Tests

- **One manual end-to-end invocation.** The agent runs the fetch script,
  reviews each `<baseline_sha>..HEAD` range, and writes a correctly-shaped
  report to `docs/reviews/`.

## Success criteria

- [ ] `docs/reviews/README.md` lists this agent's report type.
- [ ] A manual (on-demand) invocation works end-to-end at least once.
