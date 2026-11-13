# Reviews

Asynchronous review outputs from the `background-reviewer` agent: dependency
audits, secret scans, performance-regression findings, and license checks.
Also upstream-reference triage reports from the `dmidecode-upstream-watcher`
agent (milestone 0008).

## Naming convention

`<type>-<scope>-<YYYY-MM-DD>.md`. Examples: `dep-audit-2025-09-01.md`,
`license-check-2025-10-15.md`, `upstream-watch-2026-11-09.md`.

| Report type                      | Produced by                  | Invocation                                         |
|----------------------------------|------------------------------|----------------------------------------------------|
| `dep-audit-<YYYY-MM-DD>.md`      | `background-reviewer`        | On demand                                          |
| `license-check-<YYYY-MM-DD>.md`  | `background-reviewer`        | On demand                                          |
| `upstream-watch-<YYYY-MM-DD>.md` | `dmidecode-upstream-watcher` | On demand only (see [below](#upstream-watch-reports)) |

## Contents

| Document                                                     | Description                                                         |
|--------------------------------------------------------------|---------------------------------------------------------------------|
| [example-report.md](example-report.md)                       | Example efficiency audit.                                           |
| [upstream-watch-2026-11-09.md](upstream-watch-2026-11-09.md) | First upstream watch: empty ranges; Windows doc baseline recorded. |
| [milestone-0012-packaging-automation-2026-11-12.md](milestone-0012-packaging-automation-2026-11-12.md) | Milestone 0012 automation review: gaps, unknowns, new Story 04.0. |

## What a review report must contain

1. **Scope** - what was reviewed (files, deps, date range).
2. **Findings** - each as `CRITICAL / HIGH / MEDIUM / LOW / INFO` with evidence.
3. **Verdict** - CRITICAL findings block merge until resolved.
4. **Recommended actions** - linked to GitHub issues where applicable.

## Upstream watch reports

`upstream-watch-*` reports follow the same four-part shape, with two
differences:

- Findings are labelled **Cosmetic / New information / Correctness**
  (milestone 0008 plan, R5) instead of the severity scale.
- They add **Process issues** and **Windows documentation digest** sections.

They never block a merge. A report may cite only SHAs, upstream file/function
names and short paraphrases, never upstream (GPL) code or diffs. No automated
check scans reports for GPL text; the PR reviewer is the control.

**Invocation is on demand only.** There is no scheduled workflow, no CI job
and no Claude-in-CI credential (user decision, 2026-11-09). A developer asks
Claude Code to run the `dmidecode-upstream-watcher` agent
(`.claude/agents/dmidecode-upstream-watcher.md`), or another agent delegates
to it per `agent-orchestrator.md`'s cross-agent pattern. A natural trigger is
before starting new work on milestone 0003/0005 parsing logic. The run needs
a developer machine with network access, because it runs
`scripts/fetch_upstream_references.py`. The agent writes the report and
nothing else. A human commits it through a normal PR and, if the report
recommends it, advances the manifest's `baseline_sha` per
[upstream_reference_review.md](../dev/upstream_reference_review.md).

Each report is advisory output built from untrusted input. Before advancing a
`baseline_sha`, the human re-runs `git rev-list --count <old>..<new>` in the
clone, compares it with the report, and reads every Correctness and
New-information commit.
