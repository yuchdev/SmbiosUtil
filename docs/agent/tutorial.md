# Agentic Tutorial

How to work with the agents and skills in libbios. This page holds the rules that
hold for every stack; the last section is the project-specific part. For what is actually
installed, see the generated references: [agents](agents.md), [skills](skills.md),
[hooks](hooks.md).

## The mental model

- **Agent** - a specialist with its own role, tool allowlist, and model. It runs in a separate
  context and returns a summary. Use it to *delegate* a bounded job.
- **Skill** - a packaged procedure (`/name`) that runs in your current session, often by
  spawning agents. Use it for a *repeatable workflow*.
- **Hook** - a script wired in `.claude/settings.json` that fires automatically on an event and
  can block it. You never invoke a hook; you only meet its verdict.

If you are unsure which to reach for: one-off judgment call -> agent; recurring checklist ->
skill; "this must never happen" -> hook.

## Pick the agent by role, not by language

Every preset ships the same role set; only the implementer is stack-specific
(`*-expert`, e.g. `python-expert`, `cpp-expert`, `swift-expert`).

| You need to...                                   | Use                   |
|--------------------------------------------------|-----------------------|
| Decide how something should be built             | `app-architect`       |
| Write or change product code                     | the stack's `*-expert`|
| Write tests, find coverage gaps                  | `testing-expert`      |
| Explain existing tests in a standard format      | `test-documenter`     |
| Review a change before merge                     | `feature-reviewer`    |
| Threat-model anything touching auth, secrets, untrusted input | `security-auditor` |
| Audit dependencies, licenses, performance off the hot path | `background-reviewer` |
| Check a lifecycle has a failure exit and a human escalation | `incident-analyst` |
| Document something new / keep docs in sync       | `docs-writer` / `docs-updater` |
| Check an implementation against its task spec    | `task-verifier`       |
| Coordinate several of the above in sequence      | `agent-orchestrator`  |

## The default flow

For anything larger than a one-file fix, work in this order and stop at each human gate:

1. **Design** - `app-architect` drafts an ADR (`/adr-write <title>`, stored in `docs/adr/`).
   *Gate: you approve the ADR.*
2. **Implement** - the `*-expert` works on a feature branch against the ADR.
3. **Test** - `testing-expert` adds tests and reports the coverage delta.
4. **Verify** - `/verify-task <path>` checks the result against the task spec in `docs/roadmap/`.
5. **Review** - `/pr-review` runs `feature-reviewer` and `security-auditor` in parallel and
   returns one verdict. A CRITICAL security finding blocks the merge.
6. **Document** - `docs-writer` for new material, `docs-updater` for existing; then
   `/link-check`.

For a multi-step request ("ticket -> merged PR") hand the whole chain to `agent-orchestrator`
instead of driving each step yourself. It plans and delegates; it does not write product code.

## Working rules

1. **One job per agent call.** A narrow, well-specified brief beats "fix everything". Name the
   files, the goal, and what is already ruled out.
2. **Give context, not conclusions.** Point an agent at the ADR, the task spec, or the failing
   test. Asking it to confirm your hypothesis tends to return the hypothesis confirmed.
3. **Respect the boundaries.** Reviewers and auditors are read-only by design; architects do not
   write implementation code. Do not ask an agent to step outside its role - switch agents.
4. **Independent work runs in parallel; dependent work runs in sequence.** Review and security
   audit are independent. Implement and test are not.
5. **Read the result, then verify.** An agent's report is a summary, not proof. Run the tests
   and read the diff before accepting it.
6. **Keep humans at the gates.** ADR approval, merge, and anything outward-facing (push,
   publish, delete) stay yours.
7. **Never push to the default branch.** Work on a branch and open a PR.
8. **Do not fight the hooks.** A blocked secret, doc link, or shell command is information. Fix
   the cause; see [hooks](hooks.md) for what each one checks and why.
9. **Record decisions where they live.** Design rationale goes in `docs/adr/`, milestones and
   tasks in `docs/roadmap/`, review reports in `docs/reviews/`, threat models in `docs/security/`.

## Common mistakes

- Using the orchestrator for a one-line change - the overhead exceeds the work.
- Skipping the ADR for a non-trivial change, then asking the reviewer to judge the design.
- Treating `feature-reviewer` as a security gate. It is not; `security-auditor` is.
- Letting two agents edit the same files at once. Parallelize only across disjoint files.
- Editing generated reference pages by hand; `agents.md`, `skills.md` and `hooks.md` are
  regenerated from what is installed.

## Using the agents on this project

libbios is a single-language C++11 code base built with CMake. It has two targets: the `smbios` static library (`smbios/src/`, `smbios/include/smbios/`) and the `smbios_util` CLI (`smbios_util/src/`). Ownership splits like this:

- **`cpp-expert`** owns nearly all product code: the table walk in `smbios.cpp`, the typed entries (`*_entry.cpp`) and `SMBiosEntryFactory`.
- **`boost-expert`** is the specialist for the Boost-specific parts: the Boost.Iostreams `/dev/mem` mapping in `posix_physical_memory.cpp`, the Boost.Program_options CLI in `command_line_parser.cpp`, the `boost::factory`/`boost::bind` registry, and Boost-vs-`std::` choices under the C++11 constraint. The project uses no Asio or Beast, so `/boost-asio-review` rarely applies.
- **`app-architect`** coordinates work that crosses the platform boundary. The real boundary is not a language but the compile-time `#if` split between Windows (`win_bios.cpp`, `win_physical_memory.cpp`, NT native API) and Unix (`unix_bios.cpp`, `posix_physical_memory.cpp`). Both sides define identically named `SMBiosImpl`/`NativePhysicalMemory` classes, so a change on one side must keep the other compiling and behaviourally aligned. Examples are finishing the stubbed Linux sysfs/EFI readers or adding the planned `--read-file` dump import. Record such decisions with `/adr-write` and hand implementation to `cpp-expert`.

The agents and skills that matter most here follow from the domain: libbios decodes privileged, firmware-supplied bytes through packed-struct overlays.

- **`testing-expert`** should build deterministic Boost.Test cases over captured or hand-made table buffers. Today's `smbios/test/` suites only exercise the live host and need root.
- **`security-auditor`** must see any change to table walking, string parsing or physical-memory mapping.
- **`feature-reviewer`**, run via `/pr-review`, checks the version-gating and bounds invariants.
- **`background-reviewer`** watches per-entry allocation and Boost version drift.
- For documentation, use **`cpp-code-documenter`** with `/doxygen-documentation` on public headers in `smbios/include/smbios/`, **`test-documenter`** with `/document-tests` for the Boost.Test suites, and `/link-check` after doc edits.
- For roadmap-driven work, use `/implement-story`, `/implement-milestone` and `/verify-task`. Note that `ctest` is not wired up (no `enable_testing()`), so agents must run `smbios_functional_test` from `bin/` directly.
