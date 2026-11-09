# Story 03.0 - Upstream Change-Monitoring Agent

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete
**Depends on:** Story 02.0 (the baseline-advance procedure the agent's reports point back to)

## Scope

This story defines `.claude/agents/dmidecode-upstream-watcher.md`,
following this project's established agent convention.
`.claude/agents/background-reviewer.md` is the closest existing
precedent: an asynchronous agent producing a dated, non-blocking report.

The agent's scope is the two upstreams listed in
`scripts/upstream_references.json` plus the Windows API documentation
page. It refreshes its local clones with
`python scripts/fetch_upstream_references.py`. It then reviews each
`<baseline_sha>..HEAD` range, reading `baseline_sha` from the manifest.
The story also documents how the agent is invoked: on demand only (user
decision, 2026-11-09).

## Tasks

| #  | Document                                                                | Status         | Blocks |
|----|------------------------------------------------------------------------------|----------------|--------|
| 01 | [Agent definition and evaluation criteria](01-agent-definition-and-evaluation-criteria.md) | ✅ Complete    | 02     |
| 02 | [Invocation and report convention](02-scheduled-invocation-and-report-convention.md) | ✅ Complete    | 03     |
| 03 | [Tests](03-tests.md)                                                        | ✅ Complete    | -      |

## Key constraints

- **The agent never edits this project's own build-product source.**
  That covers `libsmbios`, `smbios`, `sys_interface`, `libsmbios_win`,
  `libsmbios_macos`, `libsmbios_linux`, `libsmbios_win_legacy`,
  `libsmbios_posix_legacy`, and `smbios_c` (see CLAUDE.md).
- **The agent never edits `scripts/upstream_references.json`.** It
  recommends a `baseline_sha` advance for a human to apply under
  `docs/dev/upstream_reference_review.md` (Story 02.0).
- **Its tool access is read/report only.** It matches
  `background-reviewer`'s own `Read, Grep, Glob, Bash, Write, WebFetch,
  WebSearch` pattern. `Write` is scoped to `docs/reviews/` by convention,
  not by tool restriction alone, so the agent's own instructions must say
  so explicitly, the same way `background-reviewer` does. `Bash` is used
  for the fetch script and read-only `git log`/`git show`.
- **Every finding gets a triage label.** The categories from `plan.md`'s
  R5 (cosmetic / new information / correctness) are not optional color
  commentary. Each finding in a report must carry one of them, so a human
  skimming the report can prioritize without reading every diff.
- **Reports cite upstream commits by SHA and file/function name only.**
  They never paste GPL code or diffs.

## Files or directories created or modified by this story

- `.claude/agents/dmidecode-upstream-watcher.md` (new).
- `docs/reviews/README.md`: add this agent's report type to the existing
  naming-convention table, alongside `dep-audit` and `license-check`
  (`upstream-watch-<YYYY-MM-DD>.md`).
