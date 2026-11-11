# 04 - Docs and milestone exit gate

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Tasks 01-03

## Objective

Document how CI works now, how to read it and how to debug it, then close the milestone against its evidence list (plan E1-E11).

## Files

- **Create** `docs/dev/ci.md`, at most about 200 lines:
  - the job topology and tier table;
  - RC1-RC5 in short form, linking to this milestone's `plan.md` for the full contract;
  - how to read a run (run table -> job block -> step excerpt -> failure-log artifact if Q3);
  - the local commands `python3 scripts/ci/lint.py` and `python3 scripts/ci/run_tests.py`;
  - single-job dispatch and the probe pattern;
  - the step budget table (`scripts/ci/step_budgets.json`);
  - how to add a check: write a script, call it through `ci_step`, add an inventory-style row to the table in `ci.md`, and pass the policy gate;
  - the pinned versions, and how to bump them (labels, `requirements-tools.txt` hashes, vcpkg commit);
  - a note for milestones 0012/0013: new jobs and `release.yml` use RC1-RC5 and pass P1-P9.
- **Modify** root `CLAUDE.md`:
  - replace the description of `.github/workflows/ci.yml`'s jobs ("`build` job on Windows, `sanitizers` job on Linux/macOS") with the new topology;
  - add a one-line pointer to `docs/dev/ci.md`;
  - keep the GUI bullet's CI facts (gui-ci runs; gui-local never; hook-string and local-tier checks), with the job name updated to `gui`.
- **Modify** `docs/test/test-subsystem.md`: job names in the CI sentences (`gui-build` -> `gui`), and the C06/C13 vs C10/C14 must-pass/best-effort split.
- **Modify** `docs/dev/cpp_coding_standard.md`, Build conventions: list the CI gates and what enforces each (policy gate, sanitizers with fatal UBSan if D2, R-7/R-8 guards), following that file's "name what enforces it" rule.
- **Modify** this milestone's `status.md`: story rows, the exit gate below.

## Functions and Classes

None.

## Constraints

- Docs state facts only as measured or as configured. Numbers come from `status.md` records, not from expectations.
- `python3 scripts/check_doc_links.py docs/` is clean.

## Tests (the milestone exit gate)

Report discrepancies only (`expected -> actual`). A clean gate is one line per item.

1. **Parity:** Story 03.0 Task 04's result still holds after Story 04.0's changes (21 kept, C05 dropped, 0 discrepancies).
2. **E1 (bounded output):** on a green full run, each project-owned step prints at most 1 line plus RC4. Each failing step in a negative proof stays within 64 lines / 16 KiB. Job log bytes are below the Story 01.0 baseline.
3. **E2 (pinning):** RC4 shows identical cmake/ctest/ninja versions on all legs, no `-latest` label exists (P3), and the vcpkg commit equals the baseline.
4. **E3 (installs):** the apt step prints its mirror line, and the script tests cover the fallback and the silent-update case.
5. **E4 (parsing):** no script parses ctest summary wording (test `CtestBothWordings`); test counts come from JUnit.
6. **E5 (shell flags):** `ShellFlagsHarness` passes; actionlint + shellcheck run in `lint`; P7 holds.
7. **E6 (debug cost):** one probe and one single-job dispatch recorded with wall-clock times.
8. **E7 (privilege/economy):** P1 and P4 hold; concurrency cancel and duplicate-run suppression observed once each.
9. **E8 (best-effort):** no `continue-on-error` (P5); every best-effort outcome is visible in the run table; C05 is gone.
10. **E9 (summaries):** a green run and a failing run each show one bounded run table; a multi-leg failure is shown once.
11. **E10 (GUI proofs):** the `gui` legs on all three OSes show the R-7 and R-8 `ok:` lines.
12. **E11 (UBSan):** D2's outcome recorded.
13. **Docs:** `check_doc_links.py docs/` is clean.

## Success criteria

- [ ] Exit gate items 1-13 recorded in `status.md`, counts and run IDs only.
- [ ] `docs/dev/ci.md` exists. `CLAUDE.md`, `test-subsystem.md` and `cpp_coding_standard.md` describe the new CI.
- [ ] Every story row in `status.md` is ✅. The milestone row is updated in [docs/roadmap/README.md](/docs/roadmap/README.md) if that table ever gains a status column.
