# 01 - Pinning policy and bump procedure

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Write the procedure a human (or the Story 03.0 agent, proposing a PR for
a human to approve) follows to bump either submodule's pinned commit.

## Files

- **Create** `docs/dev/submodule_bump_procedure.md`:
  - **When to bump:** only in response to a specific, named reason (a
    Story 03.0 agent report flagging something worth porting; a CVE in
    the upstream code even though it's never built, worth knowing about;
    a deliberate decision to re-sync before starting new work on
    milestone 0003/0005's logic). Never a routine/scheduled bump for its
    own sake.
  - **How to bump:** `cd c-versions/dmidecode_linux && git fetch && git
    checkout <new-sha>`, then `git add c-versions/dmidecode_linux` at the
    repo root to record the new pin, in a commit whose message names the
    upstream commit range and why.
  - **What to re-check afterward, per submodule:**
    - `dmidecode_linux` bump → re-read `unix_bios.cpp`'s TODOs (milestone
      0002), the Linux sysfs/EFI sources (milestone 0005 Story 02.0), and
      the version-fixup table (milestone 0005 Story 01.0 Task 02) for
      drift.
    - `dmidecode_macos` bump → re-read the macOS UEFI source (milestone
      0005 Story 03.0) for drift, especially the IORegistry
      path/property names and the Apple-Silicon-gating deviation already
      recorded there.
  - **Approval:** a normal PR, reviewed like any other change - no
    exception for a bump proposed by an agent.

## Functions and Classes

N/A - documentation only.

## Constraints

- This procedure is written once here and linked from (not duplicated
  into) Story 03.0's agent definition - the agent's job is to propose a
  bump following this procedure, not to restate it.

## Tests

- A dry-run: manually follow this procedure once for one submodule
  (bumping to its own current HEAD, a no-op bump) to confirm every step
  works as written, before Story 03.0's agent is built to follow it
  automatically.

## Success criteria

- [ ] The procedure's dry-run succeeds exactly as written.
- [ ] Every "what to re-check" entry names specific files/milestones, not
      a vague "review the changes."
