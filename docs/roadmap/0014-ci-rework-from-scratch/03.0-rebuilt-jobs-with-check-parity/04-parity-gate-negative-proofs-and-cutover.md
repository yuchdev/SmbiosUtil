# 04 - Parity gate, negative proofs and cutover

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
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

## Functions and Classes

None.

## Constraints

- **Parity check, row by row.** For each C01-C22, record the job/step in `ci-next.yml` and its gate. The report is discrepancies only, `<ID>: expected <gate> -> actual <gate>`; a clean result is one line, `inventory parity: 21 of 21 kept, 1 dropped (C05: dead)`. Old and new workflows must agree on the same commit: same legs green, and the same ctest totals per set. Compare the JUnit counts against the old run's log for the same commit.
- **Negative proofs,** each a throwaway commit on its own branch `negproof/<n>`, deleted afterwards. These must run the *real* workflow, so they cannot use the `probe/**` branches, which `ci-next.yml` ignores. The old `ci.yml` also runs on them, and its result is ignored. Each must fail the right job on every leg where it applies, with output inside RC1 bounds and no fixture or firmware text. Record `expected -> actual` per leg:
  1. a compile error in a unit-test source (`build`, `sanitizers`, `gui`; the `cxx` patterns show the file:line);
  2. one failing Boost assertion in `smbios_unit_test` (`build` on Windows, `sanitizers`);
  3. one failing `gui-ci` assertion (`gui`, all three OSes; repeats 0011's run 43067987979 under the new contract);
  4. a policy violation (an unpinned `uses:`) -> `lint` fails in under 3 minutes and no other job starts;
  5. if D2 = yes: a UBSan-only defect in a throwaway test -> `sanitizers` fails.

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
- [ ] Negative proofs 1-4 (and 5 if D2) fail as expected on every applicable leg, within RC1 bounds; run IDs recorded.
- [ ] Log measurements recorded next to the Story 01.0 baseline.
- [ ] Cutover done; `.github/workflows/` holds only `ci.yml`; the old file is gone; the first post-cutover run is green.
