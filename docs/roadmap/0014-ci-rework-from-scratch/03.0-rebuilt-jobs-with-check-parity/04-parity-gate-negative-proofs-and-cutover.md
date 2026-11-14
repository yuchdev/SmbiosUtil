# 04 - Parity gate, negative proofs and cutover

**Parent story:** [README.md](README.md)
**Status:** 🔶 In progress: proofs and cutover done on runners; settings checklist and first post-cutover run open
**Depends on:** Tasks 01-03

## Objective

Prove three things on real runners before replacing `ci.yml`:

- nothing was dropped (inventory parity);
- every failure path produces bounded, useful output (negative proofs);
- the logs are actually smaller (measured against the Story 01.0 Task 01 baseline).

Then cut over.

## Files

- **Modify** this milestone's `status.md`: parity table result, negative-proof run IDs, log measurements.
- **Cutover commit:** delete `.github/workflows/ci.yml`; rename `ci-next.yml` -> `ci.yml` (`name: CI`); update the policy gate's P9 allowlist; update the job names in branch protection if plan Q5 says required checks exist. That last step is done by the maintainer, because it is a repository setting.
- **Repository-settings checklist** (added 2026-11-13, [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) CR-1). The in-repo policy gate runs from the triggering ref, so it is not the security boundary; these settings are. The maintainer applies them before cutover and before any workflow that can reach `package` on a tag or any `staging`/`release` environment. Each is read back with `gh api` and the read-back recorded (these endpoints return settings, not secrets) in `status.md` and `docs/dev/ci.md`. The canonical list is [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md) "Repository settings required"; in short:
  - default workflow permissions read-only; "Allow GitHub Actions to create and approve pull requests" off unless 0012's registry PR needs it (decision recorded);
  - branch and tag rulesets: `v*` tag create/update/delete restricted to maintainers; `master` protection per the maintainer's decision (Q5 records none today);
  - environment `release`: deployment rule "selected tags" `v*`, required reviewer(s), prevent self-review, no admin bypass;
  - environment `staging`: deployment rule branch `master` only;
  - CODEOWNERS covering `scripts/ci/` and `.github/`, with code-owner review required by the `master` ruleset;
  - fork-PR workflows require approval for outside contributors.
- **Checkout credentials.** Every `actions/checkout` in `ci.yml` and `stage-*.yml` has `persist-credentials: false` (P14); verified as part of the parity check.

## Functions and Classes

None.

## Constraints

- **Parity check, row by row.** For each C01-C22, record the job/step in `ci-next.yml` and its gate. The report is discrepancies only, `<ID>: expected <gate> -> actual <gate>`; a clean result is one line, `inventory parity: 21 of 21 kept, 1 dropped (C05: dead)`. Old and new workflows must agree on the same commit: same legs green, and the same ctest totals per set. Compare the JUnit counts against the old run's log for the same commit.
- **Negative proofs,** each a throwaway commit on its own branch `negproof/<n>`, deleted afterwards. These must run the *real* workflow, so they cannot use the `probe/**` branches, which `ci-next.yml` ignores. The old `ci.yml` also runs on them, and its result is ignored. Each must fail the right job on every leg where it applies, with output inside RC1 bounds and no fixture or firmware text. Record `expected -> actual` per leg:
  1. a compile error in a unit-test source (`build`, `sanitizers`, `gui`; the `cxx` patterns show the file:line);
  2. one failing Boost assertion in `smbios_unit_test` (`build` on Windows, `sanitizers`);
  3. one failing `gui-ci` assertion (`gui`, all three OSes; repeats 0011's run 43067987979 under the new contract);
  4. a policy violation (an unpinned `uses:`) -> `lint` fails in under 3 minutes and no other job starts;
  5. if D2 = yes: a UBSan-only defect in a throwaway test -> `sanitizers` fails;
  6. (threat model, 2026-11-13) a **failing** unit test that prints `##[set-output name=x;]y` and `::error::x` -> both appear only inside the RC1 fence, no step output `x` is set, and no extra annotation is raised.
- **Q3 upload proof.** Proof 1's run shows a `logs-<job>-<leg>` artifact containing exactly `configure.log`/`build.log` (literal paths, no other file, no `.git`).

  The R-7/R-8 failure branches are proven by the script tests (Task 03), not by planting artifacts on CI.
- **Log budget, measured.** With `gh run view --log` per job, record for one green run and for proofs 1-3: the lines produced by project-owned steps, and the total log bytes. Targets:
  - green: each project-owned step prints at most 1 line, plus RC4;
  - failing step: at most 64 lines / 16 KiB;
  - total bytes per green job: lower than the baseline. Third-party action output is reported separately, not counted against the target.
- The old `ci.yml` is deleted only after a green parity run **and** the maintainer's go-ahead. Until then both workflows run.

## Tests

The gate above is the test.

## Success criteria

- [ ] Parity: 21 kept, C05 dropped with a reason, 0 discrepancies.
- [ ] Negative proofs 1-4 and 6 (and 5 if D2) fail as expected on every applicable leg, within RC1 bounds; run IDs recorded.
- [ ] Repository-settings checklist applied by the maintainer and its `gh api` read-back recorded in `status.md`.
- [ ] Log measurements recorded next to the Story 01.0 baseline.
- [ ] Cutover done; `.github/workflows/` holds only `ci.yml`; the old file is gone; the first post-cutover run is green.

## As-built notes (local part)

Done 2026-11-13, locally only. Nothing was pushed, no branch was created, no workflow was triggered, and `ci.yml` is untouched.

**Done**

- **Inventory data file.** `scripts/ci/inventory.json` holds C01-C22: the plan's Gate column verbatim (`plan_gate`), its normalised gate and disposition, and where each row lives in the new workflow (job, legs, the sub-steps that must be observed). `scripts/ci/tests/test_parity_check.py` keeps the IDs, gates and dispositions equal to `plan.md`.
- **Parity gate.** `scripts/ci/parity_check.py` (`--table` prints the row-to-job/step mapping).
  - It reads the entry workflow (`ci-next.yml`, or `ci.yml` after cutover) and the stage workflows it calls.
  - For each leg it dry-runs `stage.py` in-process, once per stage-command line that the workflow runs on that leg. Every side effect is replaced by a recorder.
  - It **measures** each gate by forcing the check to fail. A non-zero stage exit counts as `hard`; a failure that a best-effort step swallows counts as `best-effort`; any other swallowed failure counts as `ignored`.
  - It also covers C03 (vcpkg bootstrap/cache sub-steps and actions), C11 (job ceilings, step budgets, `ctest --timeout 60`), C21/C22 (a firing policy fixture, a clean tree for that rule, and a hard policy step in `lint`), C05 (no reference left in the new workflows, `scripts/ci` or CMake) and P14 (`persist-credentials: false` on every checkout).
  - **Result on today's tree:** `inventory parity: 21 of 21 kept, 1 dropped (C05: dead)`, exit 0. C14 shows best-effort on `macos-asan` and the `skip:` line from the Q4 ruling on `linux-asan`.
- **Log budget tool.** `scripts/ci/log_budget.py <run-id>` calls `gh run view <id> --json jobs` and then `gh run view <id> --log --job <job-id>` for each job (argv only; `--gh` substitutes the command). It prints counts only, per job:
  - total bytes and lines;
  - the project-owned / runner-echo / third-party split, taken from the step-name column (`split unavailable` when the log has no usable step column);
  - the longest failing-step block.

  It checks the targets: a failing project step must stay at or under 64 lines and 16 KiB; a green project step must print no non-RC line; a green job must log fewer bytes than its baseline job. The baseline numbers from `status.md` are in `scripts/ci/log_baseline.json`, which also maps each new leg to the old job it replaces. A test keeps those numbers equal to `status.md`.
- **Tests.** `test_parity_check.py` has 18 tests: the contract, today's tree, and nine deliberately broken copies, each asserting its exact discrepancy line. `test_log_budget.py` has 17 tests and uses fixtures in `tests/fixtures/log_budget/` (named `.txt` because the repository ignores `*.log` below the fixtures top level).

**Remains (real runners or maintainer action)**

1. Negative proofs 1-6, each on a throwaway branch `negproof/<n>` running the real `ci-next.yml` (not `probe/**`), with the run IDs and per-leg `expected -> actual` recorded in `status.md`. Proof 5 applies, because D2 = yes.
2. The Q3 upload proof: proof 1's run has a `logs-build-<leg>` artifact that contains exactly `configure.log`/`build.log`.
3. The measured log budget. Run `log_budget.py` on one green `ci-next.yml` run and on proofs 1-3, and record the results next to the Story 01.0 baseline.
4. A green parity run: old and new workflows green on the same commit, with the same ctest totals per set (JUnit counts against the old log). `parity_check.py` proves the structure only.
5. The repository-settings checklist above, applied by the maintainer, with the `gh api` read-back recorded in `status.md` and `docs/dev/ci.md`.
6. The branch-protection check. Q5 = none (status.md 2026-11-12), so no required checks need renaming; re-read it with `gh api` at cutover.
7. The cutover commit, after the maintainer's go-ahead: delete `ci.yml`; rename `ci-next.yml` to `ci.yml` (`name: CI`); update the P9 allowlist and `lint.py`'s entry workflow. `parity_check.py` already falls back to `ci.yml`. The first post-cutover run must be green.
