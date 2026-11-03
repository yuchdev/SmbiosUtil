# 02 - Scheduled invocation and report convention

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Decide and document how `dmidecode-upstream-watcher` actually runs
(on-demand only vs. a real schedule), and register its report type in
`docs/reviews/README.md`'s naming-convention table.

## Files

- **Modify** `docs/reviews/README.md`: add a row for
  `upstream-watch-<YYYY-MM-DD>.md` alongside the existing `dep-audit`/
  `license-check` examples.
- **Decide and document** the invocation mechanism - two realistic
  options, to be chosen during implementation rather than prescribed
  here:
  - **On-demand**: a developer (or another agent, per
    `agent-orchestrator.md`'s cross-agent coordination pattern) invokes
    it manually or via a periodic reminder, no infrastructure change.
  - **Scheduled**: a GitHub Actions workflow on a `schedule:` cron
    trigger that invokes Claude Code non-interactively with this agent,
    committing its report via a PR - a real, standing automation, not
    just documentation of intent.
  Record the choice and rationale in `status.md`, not just in this task
  file, since it's a decision with ongoing infrastructure cost either way.

## Functions and Classes

N/A.

## Constraints

- Whichever mechanism is chosen, the agent's own report still goes
  through this project's normal PR review before anything it recommends
  is acted on (milestone's Key constraints) - "scheduled" describes when
  the *agent runs*, never when a *change merges*.

## Tests

- If the scheduled option is chosen: a manual trigger of the workflow
  (GitHub Actions supports `workflow_dispatch` for exactly this) to
  confirm it runs correctly before relying on its cron schedule.

## Success criteria

- [ ] `docs/reviews/README.md` lists this agent's report type.
- [ ] The chosen invocation mechanism works end-to-end at least once.
