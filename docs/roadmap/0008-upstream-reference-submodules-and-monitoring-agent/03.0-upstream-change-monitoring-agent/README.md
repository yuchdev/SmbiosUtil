# Story 03.0 - Upstream Change-Monitoring Agent

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (bump procedure the agent's reports point back to)

## Scope

Define `.claude/agents/dmidecode-upstream-watcher.md`, following this
project's established agent convention
(`.claude/agents/background-reviewer.md` is the closest existing
precedent: an asynchronous agent producing a dated, non-blocking report),
scoped to the two confirmed submodules plus the Windows API documentation
page, and wire up how/when it runs.

## Tasks

| #  | Document                                                                | Status         | Blocks |
|----|------------------------------------------------------------------------------|----------------|--------|
| 01 | [Agent definition and evaluation criteria](01-agent-definition-and-evaluation-criteria.md) | ⬜ Not started | 02     |
| 02 | [Scheduled invocation and report convention](02-scheduled-invocation-and-report-convention.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                        | ⬜ Not started | -      |

## Key constraints

- This agent **never edits `smbios`/`smbios_util`/`smbios_c` source
  itself** - its tool access is read/report only (matching
  `background-reviewer`'s own `Read, Grep, Glob, Bash, Write, WebFetch,
  WebSearch` pattern, where `Write` is scoped to `docs/reviews/` by
  convention, not by tool restriction alone - the agent's own
  instructions must say so explicitly, the same way `background-reviewer`
  does).
- The triage categories from `plan.md`'s R5 (cosmetic / new information /
  correctness) are not optional color commentary - every finding in a
  report must be labeled with one of them, so a human skimming the report
  can prioritize without reading every diff themselves.

## Files or directories created or modified by this story

- `.claude/agents/dmidecode-upstream-watcher.md` (new).
- `docs/reviews/README.md` - add this agent's report type to the existing
  naming-convention table (alongside `dep-audit`, `license-check`:
  `upstream-watch-<YYYY-MM-DD>.md`).
