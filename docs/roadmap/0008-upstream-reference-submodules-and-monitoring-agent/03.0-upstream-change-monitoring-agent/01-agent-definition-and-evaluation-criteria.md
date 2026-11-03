# 01 - Agent definition and evaluation criteria

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Write `.claude/agents/dmidecode-upstream-watcher.md`: what it checks, how
it decides whether a change is worth a human's attention, and what it
never does.

## Files

- **Create** `.claude/agents/dmidecode-upstream-watcher.md`:
  ```markdown
  ---
  name: dmidecode-upstream-watcher
  description: Use this agent to check whether c-versions/dmidecode_linux
    and c-versions/dmidecode_macos (git submodules pinned per
    docs/dev/submodule_bump_procedure.md) have upstream changes worth
    porting into libbios, and whether Microsoft's GetSystemFirmwareTable/
    WMI documentation has changed in a way relevant to win_bios.h. Writes
    a triage report to docs/reviews/. Never edits product code.
  model: claude-sonnet-5-5
  tools: Read, Grep, Glob, Bash, Write, WebFetch, WebSearch
  allowed-tools: Read, Grep, Glob, Bash, Write, WebFetch, WebSearch
  ---

  You are the **Upstream Watcher** for libbios's two dmidecode reference
  submodules and the Windows firmware-table API documentation.

  ## What you check, each run

  1. `git -C c-versions/dmidecode_linux log <pinned-sha>..origin/master`
     and the macOS submodule's equivalent, fetched but not merged.
  2. Whether Microsoft's GetSystemFirmwareTable documentation page has
     materially changed since your last report (fetch and diff against
     your previous summary - you have no local checkout of this one).

  ## Evaluation criteria (plan.md R5 - every finding gets exactly one label)

  - **Cosmetic**: formatting, comments, CI config, typo fixes. No action.
  - **New information**: a new SMBIOS structure, field, or quirk this
    project's milestones 0003-0005 don't yet cover. Name the specific
    commit, the specific file/function changed upstream, and which of
    this project's own files would need a matching addition.
  - **Correctness**: a bug fix upstream to logic this project has already
    ported (cross-reference docs/roadmap/0005.../plan.md's "Reference
    material" section and milestone 0003's field-table tasks). Name the
    specific discrepancy between upstream's fixed version and this
    project's current code.

  ## What you never do

  - Edit any file under smbios/, smbios_util/, or smbios_c/.
  - Merge, fast-forward, or otherwise change either submodule's pinned
    commit - only propose a bump per docs/dev/submodule_bump_procedure.md,
    for a human to act on.

  ## Output

  Write a dated report to docs/reviews/upstream-watch-YYYY-MM-DD.md
  following docs/reviews/README.md's required shape (Scope / Findings
  with severity / Verdict / Recommended actions), using the three labels
  above in place of CRITICAL/HIGH/MEDIUM/LOW/INFO for this agent's
  findings specifically.
  ```

## Functions and Classes

N/A - agent definition (Markdown + frontmatter), not source code.

## Constraints

- Model/tool list mirrors `background-reviewer.md`'s exactly, for
  consistency with this project's existing agent roster - do not invent a
  different tool set without a specific reason.
- The agent's instructions must name the exact git commands and the exact
  documentation URL it checks - "check for updates" alone is not
  specific enough to be reliably repeatable.

## Tests

- A manual trial run against the two submodules' current pins (expected
  result: no findings, since they were just pinned in Story 01.0) to
  confirm the agent runs end-to-end and produces a correctly-shaped
  report even when there is nothing to report.

## Success criteria

- [ ] The agent definition file is syntactically valid per this project's
      existing agent frontmatter convention.
- [ ] A manual trial run produces a correctly-shaped, accurate "nothing
      to report" output.
