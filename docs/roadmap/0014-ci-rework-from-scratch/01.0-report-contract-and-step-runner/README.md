# Story 01.0 - Report Contract, Step Runner and Workflow-Script Test Harness

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** nothing (first story)

## Scope

This story builds the foundation every later story uses, before any workflow is rewritten:

- the frozen inventory of today's checks, plus a measured log-size baseline;
- one step runner that enforces the report contract (`plan.md`, RC1-RC3);
- a test harness that proves the runner, and any remaining shell, behave correctly under the runner's real shell flags.

No `.github/workflows/` file is changed in this story.

## Tasks

| #  | Document | Status | Blocks |
|----|----------|--------|--------|
| 01 | [Freeze the check inventory and measure the log baseline](01-freeze-check-inventory-and-log-baseline.md) | ⬜ Not started | 02, Story 03.0 |
| 02 | [Step runner implementing the report contract](02-step-runner-and-report-contract.md) | ⬜ Not started | 03, Stories 02.0-04.0 |
| 03 | [Workflow-script test harness](03-workflow-script-test-harness.md) | ⬜ Not started | Story 02.0 Task 03, Story 03.0 |

## Key constraints

- **Stdlib-only Python 3.** This follows the precedent of `scripts/check_cversions_isolation.py`, `scripts/check_doc_samples.py` and `examples/scripts/check_structured_output.py`. Scripts must run on all three hosted runners: `python3` on Linux/macOS, `python` on Windows. They must not depend on `bash`, `grep`, `tail` or `cut` behaving the same across GNU, BSD and Git-for-Windows.
- **No shell inside the runner.** Commands run as argv lists (`subprocess` with `shell=False`). This removes the `-e`/`pipefail` failure class (plan E5) from the runner itself.
- **The contract is the spec.** Thresholds (20 diagnostic lines, 40 tail lines, 200 characters per line, 64 lines / 16 KiB per failing step) are the RC1 values. A change to them is a `plan.md` amendment, not a code tweak.
- **Hostile output is assumed.** Tool output can contain `::` workflow commands, ESC sequences, invalid UTF-8 and CR-only lines. The runner must neither crash on it nor let it through as a command (RC1 workflow-command safety).

## Non-goals

- Changing what any check checks. That is Story 03.0, and it follows the inventory.
- Job summaries. Story 04.0 consumes the RC3 records written here.

## Open questions

- Does `$RUNNER_TEMP` behave identically on the Windows runner when it is reached from `python` (the `D:\a\_temp` style path)? Task 02 verifies this in the first real run, through the `self-test` subcommand.

## Files or directories created or modified by this story

- `scripts/ci/ci_step.py`, `scripts/ci/ci_patterns.py` (diagnostic pattern sets), `scripts/ci/tests/` (new).
- This milestone's `plan.md` (inventory drift, if any) and `status.md` (baseline numbers).
