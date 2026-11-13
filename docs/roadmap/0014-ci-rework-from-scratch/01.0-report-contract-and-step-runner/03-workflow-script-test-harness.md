# 03 - Workflow-script test harness

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13)
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

- [x] `python3 scripts/ci/run_tests.py` passes locally on macOS and prints one line.
- [x] `ShellFlagsHarness` fails when the corrected block is reverted to the 0011 bug form (shown once, then restored).
- [ ] Documented in `docs/dev/ci.md` (created by Story 04.0 Task 04). Until then, the command is in this task's as-built notes.

## As-built notes (2026-11-13)

- **The command:** `python3 scripts/ci/run_tests.py` (`python` on Windows), from any working directory. Success prints `ok: scripts/ci tests: <n> tests (<t>s)`; failure prints the RC1 excerpt (`python` pattern set). The count comes from `unittest.TestLoader().countTestCases()`, not from unittest's summary text (E4). An unknown `$CI_REPORTER` exits 2. Until `docs/dev/ci.md` exists (Story 04.0 Task 04), this note is the documentation.
- **Result:** 37 tests, all pass on macOS (Python 3.12), 2 skipped by design (`ShellScriptsUnderRunnerFlags`: no `*.sh` under `scripts/ci/`; the workflow `run:` extractor does not exist yet). `run_tests.py` takes about 18 s wall, dominated by `HugeLogBounded` (about 7 s). Every other case is under 1 s.
- **Files:** `scripts/ci/tests/test_ci_step.py` (the ten Task 03 cases plus the ADR 0005 additions: hostile `::error::` and `##teamcity[` lines against every backend, every neutral event rendered by every backend, `env_names` exit 2, the P11 scan with a positive control, usage errors, internal error 70, `ACTIONS_*_DEBUG` not inherited, `self-test`, `--help` under `-I`), `scripts/ci/tests/test_shell_flags.py`, `scripts/ci/tests/ci_test_support.py` (shared helpers, not collected), `scripts/ci/tests/fixtures/*.log`, `scripts/ci/run_tests.py`. `huge.log` (50 MB, including one 4 MB line) is generated in a per-test temp directory.
- **Repository plumbing:** the root `.gitignore` ignores `*.log`, so it now un-ignores `/scripts/ci/tests/fixtures/*.log`. `.gitattributes` marks `scripts/ci/tests/fixtures/** -text`, because `hostile.log` has CR-only line endings and invalid UTF-8 that a CRLF checkout would corrupt.
- **`ShellFlagsHarness` negative proof:** `CORRECTED_FORM` was temporarily replaced with `BUG_FORM_0011`. Two cases failed: `CorrectedFormPrintsRc1BeforeExit` ("no RC1 annotation before exit") and `CorrectedFormPassingIsOneOkLine`. The file was then restored, and the cases pass again.
- **`*.sh` convention:** any future `scripts/ci/**/*.sh` must carry `# ci-expected-exit: <n>`. Without it, the test fails.
- **Hand-off to Story 02.0 Task 03:**
  1. `test_WorkflowRunBlocks` skips until `scripts/ci/workflow_run_blocks.py` exists. That file name is this harness's assumption. Once the file exists, the test fails on purpose, so the wiring is not forgotten.
  2. P11 as written covers `scripts/ci/**`, which includes this harness. The harness must contain hostile `::error::`/`##teamcity[` input and must assert on rendered platform syntax. The P11 test here exempts `tests/` in addition to `reporters/` and `env_names.py`, and the policy gate needs the same exemption.
- **Task 02 code:** no defect was found, and no change was made to `ci_step.py`, `ci_patterns.py`, `env_names.py` or `reporters/`. The runner (`ci_step.py`, `ci_patterns.py`) passes P11 as-is.
- **Observed, not a defect (ADR 0005 "none needed"):** the `plain` backend prints hostile `::error::`/`##teamcity[` lines unfenced. It is safe only because it is local-only. A CI adapter that leaves `CI_REPORTER` unset would get `plain`, and the policy gate should require `CI_REPORTER` in every workflow `env:`.
