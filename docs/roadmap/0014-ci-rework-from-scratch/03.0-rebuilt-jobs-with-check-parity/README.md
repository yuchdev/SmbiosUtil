# Story 03.0 - Rebuilt Jobs with Check Parity

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (inventory, step runner, harness), Story 02.0 (pinning, install, policy gate; threat model written)
**Flag:** `security_sensitive`. The R-7/R-8 proofs (C18, C20) are merge-blocking under [docs/security/2026-11-11-gui-test-tiers.md](/docs/security/2026-11-11-gui-test-tiers.md).

## Scope

This story writes the new workflow from scratch as `.github/workflows/ci-next.yml`. Every inventory row C01-C22 is carried over with the same or a stronger gate. Parity is proven row by row and the failure paths are proven on real runners. Then the new workflow replaces `ci.yml`. The old `ci.yml` keeps running, unchanged, until the cutover in Task 04.

Target job topology (names are part of the contract, because branch protection may use them, see plan Q5):

| Job | Legs | Inventory rows |
|-----|------|----------------|
| `lint` | Linux only | C01, C02, C21, C22 + script tests, actionlint, policy gate |
| `build` | linux, windows, macos | C03, C04, C06, C07, C08, C09, C10, C11 (C05 dropped) |
| `sanitizers` | linux, macos | C03, C11, C12, C13, C14 |
| `gui` | linux-gui, windows-gui, macos-gui | C03, C11, C15, C16, C17, C18, C19, C20 |

`build`, `sanitizers` and `gui` declare `needs: lint`, so a broken workflow, script or policy fails within about a minute and the expensive legs never start.

## Tasks

| #  | Document | Status | Blocks |
|----|----------|--------|--------|
| 01 | [`lint` and `build` jobs](01-lint-and-build-jobs.md) | ⬜ Not started | 02, 03, 04 |
| 02 | [Test reporting and the `sanitizers` job](02-test-reporting-and-sanitizer-jobs.md) | ⬜ Not started | 03, 04 |
| 03 | [`gui` job and the R-7/R-8 guard port](03-gui-job-and-guard-port.md) | ⬜ Not started | 04 |
| 04 | [Parity gate, negative proofs and cutover](04-parity-gate-negative-proofs-and-cutover.md) | ⬜ Not started | Story 04.0 |

## Key constraints

- **The inventory is the contract.** A row is dropped only by its `plan.md` disposition (today only C05). Anything else that is found unnecessary while writing goes to the maintainer first.
- **Every project-owned step is one `stage.py` invocation** whose sub-steps are `ci_step` calls (RC1-RC3, SC1), and it passes the policy gate (P1-P13, [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md)). No inline shell logic, and no platform syntax in scripts.
- **Exit codes decide; structured output counts** (plan E4). Tests are counted from `ctest --output-junit`, never from the `% tests passed` line.
- **Data hygiene for live-table steps** (C07, C08, C10, C14): counts only, no `--output-on-failure`, no artifact upload of their logs or JUnit files (RC1).
- **ctest regexes are unchanged in meaning.** The C06/C13 `-R` regex and the C10/C14 regex are copied verbatim into one shared place (`scripts/ci/test_sets.json`), so `build` and `sanitizers` cannot drift apart.

## Non-goals

- New test coverage or new checks beyond the inventory (plan Non-goals), apart from fatal UBSan if the maintainer approves D2.
- Summaries and triggers (Story 04.0). Until Story 04.0, `ci-next.yml` uses the same triggers as `ci.yml`, with `branches-ignore: ['probe/**']` added.

## Open questions

- Plan Q2 (fatal UBSan), Q3 (failure-log artifacts), Q4 (Linux integration under `sudo`) and Q5 (branch protection) are needed by Tasks 02, 01, 02 and 04 respectively.

## Files or directories created or modified by this story

- `.github/workflows/ci-next.yml` (new; renamed to `ci.yml` at cutover), `.github/workflows/ci.yml` (deleted at cutover).
- `scripts/ci/ctest_report.py`, `scripts/ci/gui_guards.py`, `scripts/ci/cli_format_check.py`, `scripts/ci/test_sets.json` (new) + tests.
- `docs/security/<date>-ci-rework.md` (threat model, before Task 01 code).
