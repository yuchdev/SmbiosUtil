# 02 - Step runner implementing the report contract

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13, proven by Task 03's 37 tests on macOS; the three-runner `self-test` check is due in the first Story 03.0 run)
**Depends on:** Task 01

## Objective

Provide `scripts/ci/ci_step.py`, the single implementation of `plan.md`'s RC1 (output grammar), RC2 (interface) and RC3 (result record). Every project-owned CI step will call it. This generalises milestone 0011's `run_quiet` bash function: full output goes to a file, success prints one line, and failure prints a bounded excerpt. It also fixes that function's known limits: GNU/BSD tool differences, `-e` fragility, no workflow-command protection, and a tail that repeats lines already shown.

## Files

- **Create** `scripts/ci/ci_step.py`. Subcommands `run` and `self-test`, exactly as in RC2. A module docstring states the contract and points to this milestone's `plan.md`.
- **Create** `scripts/ci/ci_patterns.py`. Named diagnostic pattern sets, each a list of compiled regexes plus a one-line comment saying which tool output it was taken from:
  - `cxx`: the 0011 set - `error:`, `error C[0-9]+`, `fatal error`, `undefined reference`, `linker command failed`, `LNK[0-9]+`. It deliberately matches diagnostic *shapes*, not the bare word `error`, which matched `memory_error_information_*.cpp`.
  - `cmake`: `CMake Error`.
  - `vcpkg`: `error:`, `BUILD_FAILED`, `-- Building .* failed`, `See logs for more information`. Confirm these against a real vcpkg failure log before freezing the set.
  - `ctest`: `The following tests FAILED`, `\*\*\*Failed`, `\*\*\*Timeout`, `\*\*\*Exception`, `Not Run`.
  - `boost`: `error: in "`, `fatal error: in "`.
  - `sanitizer`: `ERROR: AddressSanitizer`, `ERROR: LeakSanitizer`, `runtime error:`, `SUMMARY: (Address|Undefined|Leak)Sanitizer`.
  - `python`: `Traceback`, `^FAIL:`, `^error:`.
  - `apt`: `^E: `, `Could not resolve`, `Temporary failure resolving`.

- **Create** `scripts/ci/env_names.py` (ADR 0005). This is the neutral environment-name table (`CI_PLATFORM`, `CI_REPORTER`, `CI_RUN_ID`, ... `CI_PUBLISH_TOKEN`), with one accessor that applies local defaults and exits 2 on a missing required name. It is the only module other than the reporters that may know a platform name, and it uses that knowledge only in documentation strings for the mapping table.
- **Create** `scripts/ci/reporters/{__init__,github,teamcity,plain}.py` (ADR 0005). `ci_step` emits neutral events (`step_ok`, `step_fail`, `step_warn`, `step_skip`, `begin/end_untrusted_block`, `job_summary`). Each backend renders them:
  - `github` renders `::error`, `%` escaping, `stop-commands` and `CI_SUMMARY_FILE`;
  - `teamcity` renders `##teamcity[buildProblem]`/`message status='WARNING'`, `|` escaping, `disableServiceMessages`/`enableServiceMessages`, and publishes the summary as an artifact plus a `buildStatus` line;
  - `plain` renders `error:`/`warning:` lines.

  `--reporter`/`CI_REPORTER` selects the backend, and the default is `plain`. Truncation, ceilings and data hygiene happen before rendering, so they are identical for every backend.

## Functions and Classes

Signatures are fixed here. Bodies belong to the implementer. Every public function has a docstring.

```python
@dataclass(frozen=True)
class StepResult:
    label: str
    mode: Literal["hard", "best-effort", "info"]
    outcome: Literal["ok", "fail", "warn", "skip"]
    exit: int
    elapsed_s: float
    summary: str
    first_diagnostic: str | None
    counts: dict[str, int] | None

def run_step(label: str, argv: Sequence[str], *, patterns: Sequence[str],
             best_effort_reason: str | None, summary: str | None,
             max_diag: int = 20, tail: int = 40, log_dir: Path) -> StepResult: ...

def render_failure(label: str, log_path: Path, patterns: Sequence[str],
                   max_diag: int, tail: int) -> list[str]: ...
    # RC1 failure block; never more than 64 lines / 16 KiB; tail excludes
    # lines already in the diagnostic block

class Reporter(Protocol):                         # ADR 0005; one per backend
    def step_ok(self, label: str, summary: str, elapsed_s: float) -> list[str]: ...
    def step_fail(self, label: str, rc: int, elapsed_s: float, first_diag: str, excerpt: list[str]) -> list[str]: ...
    def step_warn(self, label: str, reason: str, rc: int, excerpt: list[str]) -> list[str]: ...
    def step_skip(self, label: str, reason: str) -> list[str]: ...
    def job_summary(self, markdown: str) -> None: ...
def get_reporter(name: str | None) -> Reporter: ...   # None -> $CI_REPORTER -> "plain"

def escape_annotation(text: str) -> str: ...      # github backend: %, CR, LF; cut to 160 chars
def safe_line(raw: bytes, width: int = 200) -> str: ...  # decode errors="replace", strip ESC/C0 except TAB, cut
def append_record(result: StepResult, path: Path) -> None: ...  # RC3 JSON line, atomic append
def main(argv: Sequence[str]) -> int: ...
```

## Constraints

- **No shell.** `subprocess.run(argv, shell=False, stdout=log, stderr=STDOUT, stdin=DEVNULL)`. On Windows the executable is resolved the same way `subprocess` normally resolves it. The runner never builds a command string.
- **Bounded memory.** The log is streamed to disk. The diagnostic scan reads it line by line, and the tail uses a fixed-size `deque`. A multi-gigabyte log must not load into memory.
- **Untrusted-excerpt fence around every excerpt.** In the `github` backend this is `stop-commands`: the token comes from `secrets.token_hex(16)` and is generated per call, and it is never derived from the output. In the `teamcity` backend it is `disableServiceMessages`/`enableServiceMessages`. Task 03 tests both backends against a hostile `::error::` line and a hostile `##teamcity[` line.
- **Exit code fidelity.** Hard mode returns the child's code unchanged (1-255), and a signal maps to 128+n. If the child cannot be started (`FileNotFoundError` and similar), the result is exit 127 with a one-line reason. A Python traceback from the runner itself is a runner bug: it is caught at `main`, reported as `::error title=ci_step::internal error (<exception type>)` and returns 70. It never dumps a traceback.
- **Do not pass `ACTIONS_STEP_DEBUG`/verbose flags through** (0011 threat model R-6).
- **`self-test`** runs a tiny built-in child (`python -c` that prints 3 lines and exits 3), checks that the RC1 failure output is produced and the record written, and prints `ok: ci_step self-test (<python version>, <platform>)`. Every job's first project-owned step runs it, so a runner-specific breakage shows within seconds.

## Tests

None in this task. All of them are in [Task 03](03-workflow-script-test-harness.md), which must land before any workflow uses the runner.

## Success criteria

- [x] `run` and `self-test` behave as RC1-RC3 specify, proven by Task 03's tests.
- [ ] Locally (macOS) and on all three runners in the first Story 03.0 run, `self-test` prints one `ok:` line. *(macOS: done, `SelfTestIsOneOkLine`; three runners: pending the first Story 03.0 run.)*
- [x] The file has no third-party import, and `python3 -I scripts/ci/ci_step.py --help` works from any working directory (`HelpWorksIsolatedFromAnyDirectory`).
