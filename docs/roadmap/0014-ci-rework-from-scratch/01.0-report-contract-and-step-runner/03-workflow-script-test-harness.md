# 03 - Workflow-script test harness

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Make "tested under the runner's real shell flags" (plan E5) a repeatable command, not a lesson to remember. One local command runs every `scripts/ci/` test. The same command runs in CI's `lint` job, so the CI scripts are tested before any job relies on them.

## Files

- **Create** `scripts/ci/tests/test_ci_step.py` (`unittest`, stdlib only) with the cases listed under Tests.
- **Create** `scripts/ci/tests/fixtures/`, holding small synthetic logs (a few KiB each, no firmware data):
  - `cxx_error.log`
  - `ctest_31_pass.log` and `ctest_44_pass.log` (the two real summary-line shapes from plan E4)
  - `boost_fail.log`
  - `asan_report.log`
  - `hostile.log`: lines starting `::error::`, `::add-mask::`, `::stop-commands::x`, with ESC sequences, invalid UTF-8 and a CR-only line ending
  - `huge.log`, generated at test time, 50 MB, never committed
- **Create** `scripts/ci/run_tests.py`: runs `python -m unittest discover -s scripts/ci/tests` and prints one RC1 line. This is the one command for both local use and the `lint` job.
- **Create** `scripts/ci/tests/test_shell_flags.py`. For every `*.sh` file under `scripts/ci/`, if any are ever added, and for every `run:` block of the workflow files under test (Story 02.0 Task 03 supplies the extractor), it runs `bash --noprofile --norc -e -o pipefail <file>` against stub commands on a temporary `PATH`. It asserts the documented exit code and that the RC1 lines are printed before exit. It skips with one line when no `bash` is found, so it still runs on Windows Git Bash and on macOS.

## Functions and Classes

None beyond the test classes.

## Constraints

- **Every test runs in under 10 s, apart from the 50 MB case (under 30 s).** No network, and no test depends on the host's cmake/ctest.
- **The tests assert on RC1 *shape* and *bounds*,** not on exact wording beyond the fixed prefixes (`ok: `, `::error title=`, `--- `, `::warning title=best-effort: `), so a wording polish does not break them.
- Stubs for `cmake`/`ctest`/`apt-get`/`getent` are tiny Python scripts written into a temporary directory at test time.

## Tests

| Case | Asserts |
|------|---------|
| `SuccessIsOneLine` | Exit 0, one `ok:` line, the record has `outcome=ok`. |
| `FailureIsBounded` | `cxx_error.log` child: annotation + at most 20 diagnostic lines + at most 40 tail lines, total at most 64 lines and 16 KiB, exit code preserved. |
| `TailDoesNotRepeat` | No line appears both in the diagnostic block and in the tail. |
| `HugeLogBounded` | 50 MB child output: bounded output, peak memory below 64 MiB (`tracemalloc`). |
| `HostileOutputCannotInject` | Every excerpt line is between `::stop-commands::<token>` and `::<token>::`; the token is not in the child's output; the annotation is escaped. |
| `BestEffortNeverFails` | Child exits 2: runner exits 0, prints the `::warning title=best-effort:` line, record `outcome=warn`. |
| `MissingExecutable` | Exit 127, one-line reason, no traceback. |
| `SignalExit` (POSIX only) | Child killed with SIGTERM -> exit 143. |
| `CtestBothWordings` | Both ctest fixture shapes produce `outcome=ok` with exit 0 (the runner never parses the summary; this guards against regressions toward E4). |
| `ShellFlagsHarness` | A deliberately wrong block (bare `cmd > log` followed by diagnostics) **fails** under `-e -o pipefail` without printing them; the corrected form passes. This is the E5 bug reproduced as a test. |

## Success criteria

- [ ] `python3 scripts/ci/run_tests.py` passes locally on macOS and prints one line.
- [ ] `ShellFlagsHarness` fails when the corrected block is reverted to the 0011 bug form (shown once, then restored).
- [ ] Documented in `docs/dev/ci.md` (created by Story 04.0 Task 04). Until then, the command is in this task's as-built notes.
