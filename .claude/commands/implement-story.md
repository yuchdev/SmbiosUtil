---
name: implement-story
description: Drives one story to completion, implementing exactly one task per iteration. Resolves the story from a milestone's plan.md, gates on status.md, then walks the story README's task queue - implementing, verifying, and running quality gates per task. Updates the story README after each task and status.md when the story lands. Self-terminates when the story reaches ✅ Complete.
invoke: /loop implement-story <story>
terminates-when: The target story's row in the milestone status.md shows ✅ Complete
---

# implement-story - per-task implementation loop for one story

This loop takes **one story** (e.g. "Hello World Endpoint", story `1.0`) and drives it to
completion. Each iteration implements **exactly one** pending task, verifies it against
its spec, runs the relevant quality-gate skills, and records progress. The loop
re-schedules itself until the whole story is ✅ Complete.

The navigation chain it follows every iteration:

```
docs/roadmap/{NNNN}-{milestone-slug}/plan.md          → find the story in ## Stories
        └─ status.md                                   → gate on ## Current status
              └─ {TT.t}-{story-slug}/README.md          → read ## Tasks queue
                    └─ {NN}-{task-slug}.md               → the spec for THIS iteration
```

---

## Argument

`<story>` identifies which story to drive. Accepted forms (most specific wins):

- `0001/1.0` or `0001 1.0` - explicit milestone + story number.
- `1.0` - story number only; resolve the milestone by scanning every
  `docs/roadmap/{NNNN}-*/plan.md` `## Stories` table for that number.
- `"Hello World Endpoint"` - story name; matched (case-insensitive, substring) against the
  `Name` column of the same tables.

If the argument matches zero or more than one story, **stop** and ask the user to
disambiguate - do not guess.

**Number → folder mapping:** a story number `T.t` maps to the folder prefix `{TT.t}`
zero-padded to two digits before the dot: `3.2 → 03.2`, `6.1 → 06.1`, `8 → 08.0`. The
matching story folder is `docs/roadmap/{NNNN}-{milestone-slug}/{TT.t}-{story-slug}/`.

---

## Per-run state cursor (token-economy core)

This loop re-fires into the **same growing conversation**, so anything the main loop reads
directly stays in context for *every* subsequent iteration. The milestone path, story folder,
and task queue do **not** change within a run - re-parsing the ~500-line `plan.md` and
~150-line `status.md` each wakeup would add ~10k tokens of pure rediscovery per iteration.
Instead, resolve once and persist a small cursor:

```
.claude/state/implement-story-{story-slug}.json
```
```json
{
  "milestone_path": "docs/roadmap/0001-working-implementation",
  "story_number": "1.0",
  "story_name": "Hello World Endpoint",
  "story_folder": "docs/roadmap/0001-working-implementation/01.0-hello-world-endpoint",
  "task_heading": "## Tasks",
  "task_queue": [
    {"nn": "01", "slug": "config-model", "status": "complete"},
    {"nn": "02", "slug": "...", "status": "not_started"}
  ],
  "next_index": 1
}
```

The cursor is a **derived cache**, not the source of truth: the story `README.md` and
`status.md` remain authoritative. Refresh the cursor only when a task closes (Step 7).
`{story-slug}` is a filesystem-safe slug of the `<story>` argument.

## Iteration algorithm

### Step 1 - load or build the run cursor

Check for the cursor file.

- **Warm path - cursor exists:** read it (~300 tokens) and go straight to Step 3. Do **not**
  re-read `plan.md`, `status.md`, or the README - they are already distilled into the cursor.
- **Cold path - no cursor (first iteration, or it was deleted):** do the discovery **inside a
  cheap subagent**, not in the main loop, so the large file bodies never enter the persistent
  conversation context. Spawn an `Explore` agent (Haiku-tier) with this instruction:

  > Resolve story `<story>` for the implement-story loop. (1) In each
  > `docs/roadmap/{NNNN}-*/plan.md` `## Stories` table, find the row matching the story number or
  > name; record milestone path, story number `{TT.t}`, and name. Number→folder: pad to two
  > digits before the dot (`3.2 → 03.2`); resolve `…/{TT.t}-{story-slug}/`. (2) In that
  > milestone's `status.md` `## Current status` table, read the story row's Status cell. (3) In
  > the story `README.md`, find the task table (heading `## Tasks`, or legacy
  > `## Task overview`) and list each task's `{NN}`, slug, and status.
  > **Return ONLY** a JSON object: `{milestone_path, story_number, story_name, story_folder,
  > task_heading, status_gate, task_queue:[{nn,slug,status}]}`. No prose, no file bodies.

  Branch on the returned `status_gate`:
  - `⬜ Not started` / `🔶 In progress` → write the cursor (`next_index` = first pending
    task) and continue to Step 3.
  - `✅ Complete` → do **not** re-implement. Spawn `task-verifier` against the story folder,
    report its verdict, and **stop** (do NOT reschedule). A completed story is verified, not rebuilt.
  - Argument matched zero or >1 story → **stop** and ask the user to disambiguate.
  - No decomposed folder / no README → **stop**: this loop drives a decomposed story.
  - Missing `status.md` (milestones 0002-0004) → the agent returns `status_gate: "absent"`;
    treat as `⬜ Not started` and proceed (the table is created in Step 7).

If the main loop ever needs a single field it doesn't have, prefer a narrow `grep` of one row
(e.g. `grep -n "^| 3.2 " <status.md>`) over a full `Read` of the file.

### Step 2 - pick the next pending task

From the cursor's `task_queue`, choose the **first** task whose status is `🔶`/Partial,
else the first `⬜`/Not started. Partial is prioritised: unfinished work carries higher
regression risk than new work. Record its `{NN}`, slug, and spec path
`{story_folder}/{NN}-{task-slug}.md`.

If every task is `✅ Complete` (or deferred per the Step 7 completion rule), jump straight
to Step 7 to close the story.

Read **only the chosen task spec** in full - it is the authoritative brief for this
iteration (Files, symbols/fields, validators, tests, success criteria, constraints). This is
a *different* file each iteration, so it is necessary cost, not repeated cost.

### Step 3 - implement via the dev-fleet agents

Delegate the task spec to the appropriate fleet agent. Independent units within one
task may run concurrently; units with stated ordering dependencies run sequentially. This
preset's fleet is intentionally minimal today (see `agent-orchestrator.md`'s roster) - add
rows here only once a matching agent actually exists in this preset:

| Spec role keyword  | Fleet agent                          | Agentic action          |
|---------------------|------------------------------------------|------------------------------|
| `C++ Expert`       | `cpp-expert`                           | task implementation      |
| `Testing Expert`   | `testing-expert` then `cpp-expert`     | QA then fix regressions     |

Brief each coder/QA agent with **paths and section references, not pasted file bodies** -
pasting a spec bills it twice (once in your context, once in theirs):

- The task spec **path** (`{story_folder}/{NN}-{task-slug}.md`) - the agent reads it
  itself. Add a one-line scope note, not the spec text.
- The cross-cutting rules in `@docs/dev/cpp_coding_standard.md` - Doxygen comment coverage,
  ownership/lifetime convention, no bare `catch (...) {}`, unit + mocked-integration tests.
  Also point the agent at the repository-root `CLAUDE.md`:
  - `## Build and test` covers test binaries that need root/admin, keeping `-DGIT_TAG` OFF
    so the build never rewrites git tags, and re-running `cmake` after adding files because
    sources are collected with `file(GLOB ...)`.
  - `## Architecture` covers compile-time `#if` platform selection, the
    add-an-entry-type-to-`SMBiosEntryFactory` procedure, version-gated field reads, and the
    known `SMBiosVersion` comparison defect.

  The project has no logging or redaction utility. If the milestone's `plan.md` carries its own cross-cutting
  guidelines section, name it for the agent to read.

**Return discipline (keeps the parent context lean):** instruct every spawned agent to return
a **compact structured summary** - files changed, symbols added, pass/fail, coverage delta -
and to **never echo diffs, file contents, or full test logs**. A subagent's internal reads and
reasoning are discarded with its context; only its final message persists in the loop, so that
message is the only thing you pay to carry forward.

**Security-sensitive tasks.** Spawn `security-auditor` on the task spec path before
delegating to `cpp-expert` if the task touches any of the following:

- parsing of firmware-supplied bytes: the table walk and anchor scan in
  `smbios/src/smbios.cpp`, `AbstractSMBiosEntry::parse_dmi_strings()`/`dmi_string()`, any
  `reinterpret_cast` overlay in an `*_entry.cpp`, or the packed structs in
  `smbios/include/smbios/smbios.h`;
- physical-memory mapping: `posix_physical_memory.cpp` (`/dev/mem`) or
  `win_physical_memory.cpp` (`NtOpenSection`/`NtMapViewOfSection`);
- a native table source: `win_bios.cpp` or `unix_bios.cpp`;
- a CLI path that reads or writes files with the process's root/admin privileges:
  `--dump-file`, `--read-file`.

The auditor writes its threat model to `docs/security/YYYY-MM-DD-<feature>.md`, and the
model becomes a standing input to the coder briefing. A CRITICAL finding (verdict
`BLOCK`) stops the loop.

### Step 4 - stop-and-ask when implementation needs a decision

During implementation the loop **may pause to ask the user** whenever the spec is ambiguous,
a design choice materially shapes the final implementation, or a deviation from the spec
looks warranted. Use `AskUserQuestion` with concrete options, apply the answer, and
continue the same iteration. Prefer asking over guessing for anything that is expensive to
reverse. (Routine, unambiguous implementation does **not** pause - only genuine forks.)

### Step 5 - verification gate

Run **only this task's** verification, not the whole suite: scope the test run to the
task's own test target(s) (e.g. `ctest --test-dir build -R "TaskFooTest"`), plus the
project's lint/static-check task (`clang-tidy`, or the narrower target the repo
actually uses - see `cpp-expert.md`). Do **not** pipe full test output into context - a green
run is one summary line; on failure, tail only the failing assertions. The full suite runs
once at story completion (Step 7), and the Stop hook gates the session end regardless.

On failure:

- Diagnose, delegate the fix to `cpp-expert`, re-run. Repeat until green or until the
  failure is clearly a spec ambiguity needing human input - in that case stop the loop and
  surface the blocker.

### Step 6 - spec-compliance gate (`/verify-task`)

With verification green, run `/verify-task {task-path}` where `{task-path}` is the
spec path from Step 2 (relative to `docs/roadmap/`, without the `.md` extension). It spawns
`task-verifier` for a compliance matrix + verdict:

- **PASS** → record a **single line** (`✓ verify-task PASS`), not the full matrix - the
  matrix only earns its tokens when there is something to act on. Continue to Step 7.
- **PARTIAL** → log the deviation list (this is worth carrying); continue, and feed those
  deviations into the `/pr-review` context so the reviewer sees them.
- **FAIL** → delegate the blocking gaps back to `cpp-expert` (max 1 retry), re-run
  Step 5 then this step. If still FAIL after one retry, surface the gaps and stop the loop.

Then run the applicable quality-gate skills. **Skip any gate whose trigger condition is false -
do not spawn an agent that can only find nothing.** Each gate is module-scoped, never tree-wide:

| Condition                                                                    | Skill                                                              |
|-------------------------------------------------------------------------------|---------------------------------------------------------------------|
| Any file under an API route, auth, or middleware path changed, or anywhere handling credentials/tokens | `/secret-scan <changed-module>/` |
| Task touches docs / public API                                              | `/link-check docs/` - inbound references still resolve.           |

This table has no coverage or dependency-audit row because `.claude/skills/` has no such
skill and the repository has no coverage tooling or CI. The only third-party dependency
is Boost (>= 1.61, static, pinned only by `BOOST_MIN_VERSION` in the root
`CMakeLists.txt`). Repo-wide dependency review is `background-reviewer`'s dependency-audit
task, not a per-task gate.

### Step 7 - record the task, then check the story

**After each task completes:**

1. Edit the story README `{story_folder}/README.md` - set **only this task's** row Status
   cell to `✅ Complete` (or `🔶`/deferred with a one-line note if only partially done). A
   targeted row edit, not a rewrite.
2. **Refresh the cursor** (`.claude/state/implement-story-{story-slug}.json`): update
   this task's `status` and advance `next_index`. This keeps the next iteration on the warm
   path - it never has to re-read the README.

**Then test the story-completion condition** over the cursor's `task_queue`:

> The story is **✅ Complete** when the feature is overall created and working - even if 1-2
> **minor** tasks are explicitly deferred. Do not block completion on optional or
> enhancement tasks (e.g. a caching optimisation) once the core feature is delivered and
> green. A *major* task still `⬜`/`🔶` means the story is **🔶 In progress**, not complete.

- **Story not yet complete** → go to Step 8 (reschedule for the next task).
- **Story complete** → this is the **one** place the whole suite runs: execute
  `ctest --test-dir build` (plus the project's coverage task, if it has one) once (summary line
  only), then run `/pr-review` (must reach LGTM or have REQUEST_CHANGES resolved). Then update
  `{milestone_path}/status.md` `## Current status` with a **targeted row edit**:
  - Set the story row's Status cell to `✅ Complete` (note any deferred tasks inline,
    e.g. "✅ Complete (task 11 deferred)").
  - Update the Tests cell with the test files introduced.
  - Append/extend the `## Story N - <Name>` summary paragraph: what was delivered, key
    implementation decisions, coverage numbers, and any deferred items.
  - Run `/link-check docs/roadmap/` to confirm the edits keep every link/anchor resolving.
  - **Delete the cursor** `.claude/state/implement-story-{story-slug}.json` (the run
    is over; a stale cursor would mislead a future invocation).
  - **Stop** (do NOT reschedule) - the story is done.

### Step 8 - reschedule

Call `ScheduleWakeup` with:

- `prompt`: the literal `/loop implement-story <story>` (the **same** `<story>` argument).
- `delaySeconds`: `270` - under the 300 s prompt-cache TTL, so the cached prefix (system
  prompt + this loop file + `CLAUDE.md`) is reused at ~10× lower cost on the next wakeup.
- `reason`: "advancing to next pending task of story {TT.t} <Name> after completing
  task {NN}".

> Cache caveat: a heavy `cpp-expert` step that runs longer than ~5 min blows the TTL
> regardless, so the warm cache mainly benefits the fast gate/verify iterations. The cursor
> (not the cache) is what guarantees cheap rediscovery; the cache is a bonus on top.

---

## Story-specific notes

Most stories need nothing here - Steps 3-6 already cover the general case. Add a
`### Story {TT.t} - <Name>` subsection only when a specific story carries a real gotcha future
iterations must not rediscover from scratch: a non-obvious ordering dependency between its
tasks, a security-sensitive subsystem that needs extra review before coding starts, an
artifact category (generated code, vendored fixtures, build output) that should skip the
usual lint/style gate, or a dependency that must land before a task can pass. Keep each
note to what the next iteration needs to act correctly on - not a running commentary on the
story.

---

## Token-economy invariants

Because the loop re-fires into one growing conversation, the cost that compounds is whatever
**permanently lands in the main-loop context**. Hold these every iteration:

1. **Resolve once, cache forever (per run).** Steps 1-2 read the cursor (~300 tokens), never
   re-parse `plan.md`/`status.md`/README. Cold-start discovery happens *inside a subagent* so
   the file bodies never enter the persistent context.
2. **Read narrow.** Grep single rows / `offset`+`limit` to a table; never full-`Read` a large
   doc from the main loop. Full reads of the *current task spec* are the only exception
   (necessary, and a different file each time).
3. **Delegate, then demand terse returns.** Push reads, analysis, and reasoning into subagents
   (discarded with their context); require compact structured summaries back - no diffs, file
   bodies, or full test logs. `verify-task` PASS = one line.
4. **Run the minimum gate.** Task-scoped tests with `-q`; full suite once at story close;
   skip any quality gate whose trigger is false; module-scope the ones that fire.
5. **Keep narration terse and lean on `/compact`.** A status line, not a recap - the main
   loop's "thinking out loud" persists; a subagent's does not. On a long story, `/compact`
   between iterations resets the accumulated tail.

## Termination conditions

| Condition                                           | Action                                       |
|-------------------------------------------------------|--------------------------------------------------|
| Target story reaches `✅ Complete` (Step 7)         | Update status.md, print summary, do NOT reschedule |
| Story already `✅ Complete` at Step 1               | Run `task-verifier`, report, do NOT reschedule  |
| Argument matches zero or >1 story                   | Stop, ask the user to disambiguate           |
| Verification fails after the fix retries (Step 5)   | Surface the blocker, stop the loop           |
| `task-verifier` FAIL after one retry (Step 6)       | Surface blocking gaps, stop the loop         |
| `/pr-review` returns REQUEST_CHANGES after 2 rounds | Stop the loop, surface the diff              |

---

## Loop ↔ skill relationship

This loop is the **sentence**; the skills are its **vocabulary**. It calls `/verify-task`
(spec compliance), `/secret-scan`, `/link-check`, and `/pr-review` at the right checkpoints,
and delegates implementation to the dev-fleet agents listed in `agent-orchestrator.md`'s
roster. For one-off spec-compliance audits of an already-built story, invoke the
`/verify-task` skill directly against each task spec - no separate audit loop exists
in this preset.
