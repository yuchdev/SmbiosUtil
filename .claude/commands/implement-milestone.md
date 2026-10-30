---
name: implement-milestone
description: Drives an entire milestone to completion. Cold-starts with a deep research phase - reads the milestone plan.md and status.md, reconciles the spec against as-built code, and authors specs for any decomposition gaps - then executes every story in dependency order through the implement-story iteration algorithm, one task per iteration. Updates status.md after every completed task and appends a per-story detail record at each story close. Self-terminates when every story row in status.md shows ✅ Complete (or a ratified deferral) and the milestone exit gates pass.
invoke: /loop implement-milestone <milestone>
terminates-when: Every story row in the milestone status.md shows ✅ Complete (or a ratified deferral) AND the milestone exit gates have passed
---

# implement-milestone - milestone-level execution loop

This loop drives **one whole milestone** (e.g. `docs/roadmap/0002-notification-delivery/`) to
completion. It is the level above [implement-story.md](implement-story.md): that
loop builds one story; this one researches the milestone, sequences its stories by their
dependency graph, and executes each story *through* the implement-story algorithm -
still exactly **one task per iteration**, because the task is the atomic unit of
verified progress at every level.

The navigation chain, one level up from implement-story:

```
docs/roadmap/{NNNN}-{milestone-slug}/plan.md      → stories, dependency graph, shared contracts
        └─ status.md                               → per-story state gate + decisions record
              └─ {TT.t}-{story-slug}/README.md      → the story's task queue
                    └─ {NN}-{task-slug}.md           → the spec for THIS iteration
```

---

## Argument

`<milestone>` identifies which milestone to drive. Accepted forms (most specific wins):

- `0002` - milestone number; resolves `docs/roadmap/0002-*/`.
- `0002-notification-delivery` or `notification-delivery` - the folder slug (full or suffix).
- `"Notification Delivery Pipeline"` - the milestone title, matched (case-insensitive,
  substring) against each `plan.md`'s H1.

These are illustrative - a fictional example milestone, not one this preset ships. The
shapes below (multi-story dependency graph, a security-sensitive story, shared contracts)
are what a real milestone looks like; substitute your own project's actual milestone
argument forms once one exists.

If the argument matches zero or more than one milestone, **stop** and ask the user to
disambiguate - do not guess. If the folder exists but has no `plan.md`, **stop**: this
loop executes a specified milestone; writing the specification is `app-architect`'s
job, not an execution loop's.

---

## Composition contract with implement-story

**This loop embeds the implement-story algorithm; it never spawns it.** A loop owns
its own `ScheduleWakeup`: if this loop rescheduled with `/loop implement-story
<story>`, control would pass to the story loop permanently - implement-story
terminates by *not* rescheduling, so there is no wakeup left to return to the
milestone. Instead, each iteration of this loop executes **Steps 2-6 of
[implement-story.md](implement-story.md) verbatim, by reference** (pick task →
delegate to the fleet → stop-and-ask on forks → verification gate → `/verify-task` +
quality gates), and overrides only the boundary steps that implement-story defines
for a *single-story* run:

| implement-story step     | Milestone-run override                                                                                       |
|---------------------------|----------------------------------------------------------------------------------------------------------------|
| Step 1 (cursor)          | The cursor is milestone-level (below); the story-level fields implement-story needs are embedded in it       |
| Step 7, "story complete" | Do **not** stop: run the story-close gate, record the story, advance the story queue (Step M4 below)          |
| Step 7.1-7.2 (recording) | Additionally update `status.md` after **every** task, not only at story close (Step M3 below)                |
| Step 8 (reschedule)      | Reschedule with `/loop implement-milestone <milestone>` - always the milestone prompt, never the story prompt |

Everything else in implement-story - the fleet routing table, briefing discipline
(paths and anchors, never pasted bodies), return discipline, stop-and-ask rules, the
verification and spec-compliance gates - applies unchanged and is **not** duplicated
here. If the two files ever disagree about a per-task mechanic, implement-story
wins; if they disagree about story sequencing or milestone state, this file wins.

---

## Milestone cursor (token-economy core)

Resolve once, then never re-read `plan.md`/`status.md` wholesale in the main loop:

```
.claude/state/implement-milestone-{milestone-slug}.json
```
```json
{
  "milestone_path": "docs/roadmap/0002-notification-delivery",
  "milestone_title": "Notification Delivery Pipeline",
  "phase": "execute",
  "story_queue": [
    {"tt": "01.0", "slug": "delivery-queue-foundation", "status": "complete",
     "depends_on": [], "security_sensitive": false, "tasks_done": 4, "tasks_total": 4},
    {"tt": "02.0", "slug": "retry-and-backoff-policy", "status": "in_progress",
     "depends_on": ["01.0"], "security_sensitive": false, "tasks_done": 1, "tasks_total": 3},
    {"tt": "03.0", "slug": "webhook-signing", "status": "not_started",
     "depends_on": ["01.0"], "security_sensitive": true, "tasks_done": 0, "tasks_total": 4},
    {"tt": "04.0", "slug": "delivery-dashboard", "status": "not_started",
     "depends_on": ["02.0", "03.0"], "security_sensitive": false, "tasks_done": 0, "tasks_total": 3}
  ],
  "current_story": {
    "story_folder": "docs/roadmap/0002-notification-delivery/02.0-retry-and-backoff-policy",
    "task_queue": [
      {"nn": "01", "slug": "backoff-schedule-model", "status": "complete"},
      {"nn": "02", "slug": "...", "status": "not_started"}
    ],
    "next_index": 1
  },
  "research_digest": {
    "contracts_anchor": "plan.md#shared-contracts-authoritative",
    "contracts": ["C1 delivery-attempt schema", "C2 idempotency-key format"],
    "exit_gates": ["full regression", "security-auditor pass (Story 03.0 surface)"],
    "gap_dispositions": ["Story 04.0 implied a dashboard-auth task the README lacked -> authored 04.0/03-dashboard-auth.md"]
  },
  "decisions": ["03.0 signing algorithm ratified as HMAC-SHA256 2026-02-03 (see status.md Notes & decisions)"]
}
```

The cursor is a **derived cache**; `plan.md`, `status.md`, and the story READMEs remain
the source of truth. `current_story` is the embedded implement-story cursor for the
in-flight story. `research_digest` holds one-line pointers (anchor and label), never
copied contract text - agents are briefed with the anchors and read the plan
themselves. Refresh the cursor only at task close (M3) and story close (M4).

## Phase R - cold-start research (once per run)

Runs only when no cursor exists. This phase is what makes the loop safe on a large
piece of functionality: it establishes what the plan promises, what already exists,
and what is missing - **before** any implementation. All heavy reading happens
**inside subagents** that return structured summaries; the file bodies never enter
the persistent loop context.

### R1 - digest the plan

Spawn an `Explore` agent to read `{milestone_path}/plan.md` in full and return JSON
only: the `## Stories` table rows; the dependency graph (explicit section if present,
else numeric order); the shared-contracts section's anchor plus a one-line label per
contract; any per-story security/risk markers (e.g. "security-sensitive - requires a
`security-auditor` pass"); and the milestone's own exit gates if the plan or its
closing story defines them.

### R2 - classify milestone state from status.md

Read `status.md` (or note its absence). Classify:

- **NEW** - no status.md, or every row `⬜ Not started` → full run ahead. If the file
  is missing, create it now from the skeleton shape (`## Current status` table with
  one row per story, all `⬜`, plus the legend) so every later update is a row edit.
- **IN PROGRESS** - a mix of `✅`/`🔶`/`⬜` → resume. Trust `✅` rows *provisionally*,
  pending R3.
- **DIVERGED** - the `## Notes & decisions` section records supersession or
  ratified redesigns → read those notes into `decisions` before anything else; they
  override the plan's per-story specs where they conflict.
- **COMPLETE** - every row `✅` → do **not** re-implement. Spot-verify (R3 probes on a
  sample), report the milestone's standing, and **stop** without rescheduling.

### R3 - as-built reconnaissance (code is the truth, status is a cache)

For every story not marked `✅`, spawn a cheap probe: do the artifacts named in the
story's `Output` column already exist in the codebase? Three outcomes per story:

- **absent** → normal pending story.
- **present and matching the spec** → status.md is stale; mark the story for a
  `task-verifier` pass instead of implementation (verify, record, don't rebuild).
- **present but shaped differently** → a live divergence (someone implemented the
  idea another way - the most dangerous state, because blind execution would build a
  duplicate). Route to the divergence protocol below **before** the story is queued.

### R4 - decomposition audit and gap planning

Cross-check the plan against the decomposition on disk: every story in the `## Stories`
table has a folder, a `README.md` with a task table, and one spec file per task
row; every shared contract and exit gate is exercised by at least one task. For
each gap:

- **Missing decomposition for promised scope** (a story folder or task spec that
  the plan clearly implies) → delegate `app-architect` to author the missing spec
  file(s) into the story folder, matching the sibling specs' format. This is the
  "detailed plan for facts not covered in stories and tasks" - it is written down
  as real spec files, never held only in loop memory.
- **Contract-level ambiguity or genuinely new scope** → `AskUserQuestion`. An
  execution loop fills in missing *decomposition*; it never quietly extends the
  milestone's *scope*.

Record every gap and its disposition in `status.md` under `## Notes & decisions`
(create the section if absent) - one line each, so the audit trail survives the run.

### R5 - write the cursor and enter the execute phase

Build `story_queue` (dependency-ordered), `research_digest`, and `decisions`; write
the cursor; set `phase: "execute"`; proceed to Step M1 in the same iteration if
budget allows, else reschedule.

libbios defines no milestone exit gates of its own beyond "full suite green +
`/pr-review`". There is no CI configuration, no coverage floor, and no benchmark
threshold in the repository, so R1 must not invent any. Two project facts shape what
"full suite" means:

- The root `CMakeLists.txt` never calls `enable_testing()`, so the `add_test`
  entries in `smbios/test/*/CMakeLists.txt` are inert and `ctest` finds nothing.
  Run the Boost.Test binaries directly from the `bin/` output directory:
  `smbios_functional_test --log_level=message`, and `smbios_performance_test` for
  timing.
- Both binaries read the host's live SMBIOS table, so they need root/admin. A green
  run proves only that the host table parses.

The one gate the kit itself adds is the M2 `security-auditor` pass for stories flagged
`security_sensitive`.

## Iteration algorithm (execute phase)

### Step M1 - load the cursor

Warm path only reads the cursor (~400 tokens). Never re-read `plan.md` or `status.md`
wholesale; a single necessary field is a `grep` of one row.

### Step M2 - select the active story

If `current_story` is in flight, continue it. Otherwise, pick the first `story_queue`
entry whose status is pending and whose `depends_on` are all `complete` - plan order
within a parallel-eligible group (this loop is one conversation, so "parallel" stories
still execute serially; parallelism lives *inside* a task, at the agent level, per
implement-story Step 3). If no story is eligible but pending stories remain, the
dependency graph is cyclic or blocked on a deferral - **stop** and surface it.

On first entering a story: build `current_story` from the story README (via a cheap
subagent, as implement-story Step 1 does); and if the story is flagged
`security_sensitive`, spawn `security-auditor` on the story's spec **before the first
task** - its threat model becomes a standing input for every coder briefing in
this story (implement-story's per-task trigger still applies on top).

### Step M3 - execute ONE task

Run implement-story **Steps 2-6** against `current_story`, with one milestone-run
addition to the briefing: include the plan's shared-contracts anchor
(`research_digest.contracts_anchor`) plus the names of the contracts this story
touches, so every agent reads the authoritative contract instead of re-deriving it.

Then close the task with implement-story Step 7.1-7.2 (story README row edit and
cursor refresh) **plus the milestone addendum - a `status.md` update after every
completed task** (this is a hard rule of this loop, not an option): a targeted
edit of the story's row in `## Current status`, e.g. Status cell
`🔶 In progress (3/6 tasks)`. One row, one edit; never rewrite the table.

### Step M4 - story close

When the story's completion condition holds (implement-story Step 7's rule: core
feature delivered and green; 1-2 *minor* tasks may be explicitly deferred), run
that step's story-complete actions - full suite once, `/pr-review` to LGTM, targeted
`status.md` row flip to `✅ Complete` - but **do not stop**. Additionally:

1. Append a per-story detail section to `status.md` (`### Story {TT.t} - {Name} (✅
   {date})`): a **Delivered** list and a **Tests / gate** summary, written from the
   story cursor and agent return summaries - never by re-reading diffs.
2. If the as-built shape diverged from the story's spec (superset, redesign, dropped
   task), append a **Reconciliation note** recording the disposition of each
   affected task spec: *consumed* / *superseded by <what>* / *not applicable*.
   Future readers of the decomposition must be able to tell which spec files still
   describe reality.
3. Mark the story `complete` in `story_queue`, clear `current_story`, refresh the cursor.

### Step M5 - milestone close

When every `story_queue` entry is `complete` (or carries a ratified deferral recorded
in `## Notes & decisions`):

1. Run the milestone exit gates from `research_digest.exit_gates`. If the plan
   defines a closing/integration story, its tasks *are* the gates and have already
   run as Step M3/M4 - do not run them twice; run only whatever the digest lists
   beyond them.
2. Run `/link-check docs/roadmap/` - the run has edited status.md, READMEs, and
   possibly authored new spec files.
3. Final `status.md` pass: gate-status line under the table (which gates ran, verdict,
   date), and flip the roadmap index (`docs/roadmap/README.md`) if it tracks milestone
   state.
4. **Delete the cursor** and **stop** (do NOT reschedule). Print a closing summary:
   stories delivered, gates passed, deferrals, and decisions ratified during the run.

### Step M6 - reschedule

Call `ScheduleWakeup` with:

- `prompt`: the literal `/loop implement-milestone <milestone>` (the **same** argument).
- `delaySeconds`: `270`.
- `reason`: "milestone {NNNN}: advancing to task {NN} of story {TT.t} ({done}/{total}
  stories complete)".

---

## Divergence & decision protocol

The hardest real-world state (drawn from a production milestone run): a story's design
already exists in the codebase **in a different shape** than its spec - a superset, a
refactor, a rename. Blind execution builds a duplicate; blind acceptance silently
abandons the plan. When R3 or an in-flight task discovers a material departure:

1. **Pause the affected work** (finish nothing against the stale spec).
2. Present the fork with `AskUserQuestion`: follow the spec as written (rework the
   as-built code) vs. ratify the as-built shape (rework the spec's remaining
   promises onto it), with one line on the consequence of each.
3. Record the ruling in `status.md` `## Notes & decisions` - dated, with the
   per-task disposition list (consumed / superseded / n-a) - and add it to the
   cursor's `decisions` so later stories brief against the ratified reality.
4. Re-audit downstream `story_queue` entries whose specs referenced the superseded
   design; author spec amendments via `app-architect` where the ruling changed them.

Minor deviations (a field default, a file path) stay at the implement-story level:
its `/verify-task` PARTIAL flow already logs them. This protocol is for
*contract-level* departures only.

libbios records ratified divergence decisions in the default location, the milestone
`status.md` `## Notes & decisions` section, and step 3 stands as written. The
repository has no ADR history and names no design authority beyond the maintainer
answering `AskUserQuestion`. A contract-level ADR via `/adr-write` is therefore
optional, not required.

---

## Token-economy invariants

All five implement-story invariants hold per task. At milestone level, three
more compound across the (much longer) run:

1. **Research once, digest forever.** Phase R is the only wholesale read of the plan,
   and it happens inside subagents. After R5 the main loop touches only the cursor,
   single grepped rows, and the *current* task spec.
2. **Write records from the state you already hold.** Per-story detail sections (M4) come
   from the story cursor and agent summaries; never re-read diffs or re-run gates to
   write prose.
3. **`/compact` at story boundaries.** A milestone run spans dozens of iterations; the
   accumulated narration tail is the dominant compounding cost. Story close (M4) is
   the natural compact point - everything worth keeping is already in status.md and
   the cursor.

## Termination conditions

| Condition                                            | Action                                                                           |
|--------------------------------------------------------|------------------------------------------------------------------------------------|
| All stories `✅` + exit gates pass (M5)              | Final status.md pass, delete cursor, do NOT reschedule                           |
| Milestone already COMPLETE at Phase R                | Spot-verify, report standing, do NOT reschedule                                  |
| Argument matches zero or >1 milestone / no plan.md   | Stop, ask the user                                                               |
| Dependency cycle, or pending stories all blocked     | Stop, surface the graph                                                          |
| Divergence fork the user declines to resolve         | Stop; record the open fork in Notes & decisions                                  |
| Any implement-story stop condition fires mid-story   | Stop the whole loop, surface it - never skip to the next story over a failed gate |
| Exit gate fails at M5                                | Stop, surface the failing gate; do not flip status.md to complete                |

An implement-story-level failure (verification red after retries, `/verify-task`
FAIL, CRITICAL security finding, `/pr-review` unresolved) stops the **milestone**
loop, not just the story: a milestone must never advance past a story that could not
pass its own gates.

libbios uses the `implement-story.md` defaults unchanged: full suite green plus `/pr-review` LGTM, with no coverage minimum, integration suite or deployment step. "Full suite" means building with `cmake --build .` and then running the Boost.Test executables `smbios_functional_test` and `smbios_performance_test` directly from `bin/` with root/admin privileges. `ctest` cannot be used because `enable_testing()` is never called.

---

## Loop ↔ loop ↔ skill relationship

Three levels, same vocabulary: this loop *sequences stories*;
[implement-story.md](implement-story.md) *builds one story* (and remains
independently invocable for single-story work - this loop reuses its algorithm rather
than wrapping its invocation); the skills - `/verify-task`, `/secret-scan`,
`/link-check`, `/pr-review` - are the per-checkpoint gates both loops call.
Implementation is delegated to the dev-fleet agents in `agent-orchestrator.md`'s
roster; `app-architect` additionally serves this loop in Phase R (gap-spec authoring) and
the divergence protocol (spec amendments).
