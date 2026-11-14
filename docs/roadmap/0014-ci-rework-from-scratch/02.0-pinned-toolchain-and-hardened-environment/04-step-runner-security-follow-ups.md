# 04 - Step-runner security follow-ups (environment scrub, data hygiene, output channel)

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13; on-runner checks pending, see As-built notes)
**Depends on:** Story 01.0 Task 02 (shipped code). Story 01.0 stays closed; this task amends its code without reopening it.
**Blocks:** Task 02, Task 03 (P11 single-emitter rule), Story 03.0 Task 01 (must merge before it)
**Source:** [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md), "Per-task requirements" items 1-5 (CR-4, CR-5, CR-9, CR-11), plus the CR-6/CR-8 job-output channel.

## Objective

Close the gaps the threat model found around the shipped Story 01.0 runner (`scripts/ci/ci_step.py`, `scripts/ci/reporters/`, `scripts/ci/env_names.py`). The runner's core (no shell, bounded output, per-fence random token, escaped annotations) is unchanged. This task adds what surrounds it:

- child processes no longer inherit platform side channels or credentials;
- every unfenced line is sanitised, on TeamCity too;
- live-table steps get a status-lines-only mode (RC1 data hygiene);
- runner-written files are private and do not follow symlinks;
- one neutral channel for job outputs, used by stage-to-stage digest checks.

## Files

- **Modify** `scripts/ci/reporters/__init__.py`:
  - `CHILD_ENV_DENYLIST` becomes a name-and-prefix denylist (CR-5). A child process never inherits:
    - the GitHub file-command channels `GITHUB_ENV`, `GITHUB_OUTPUT`, `GITHUB_PATH`, `GITHUB_STATE`, `GITHUB_STEP_SUMMARY`;
    - `ACTIONS_ID_TOKEN_REQUEST_TOKEN`, `ACTIONS_ID_TOKEN_REQUEST_URL` (the OIDC request token), `ACTIONS_RUNTIME_TOKEN`, `ACTIONS_RUNTIME_URL`, `ACTIONS_CACHE_URL`, `ACTIONS_RESULTS_URL`;
    - every `CI_PUBLISH_*` and `AWS_*` name;
    - TeamCity password-type parameters exported to the environment (`TEAMCITY_*`/`BUILD_*` names that the TeamCity adapter marks as secure (a project-defined convention: only `TEAMCITY_BUILD_PROPERTIES_FILE` is a real TeamCity name; the `TEAMCITY_SECURE_`/`BUILD_SECURE_` prefixes are invented here and unverified until a TeamCity adapter exists); the list lives in `env_names.py`);
    - the existing three debug switches.
  - `child_environment(environ, pass_env=())` keeps a denied name only when the caller lists it in `pass_env`. A publish destination exchanges the OIDC token itself, writes it to a 0600 file under `CI_TEMP_DIR`, and passes only that file's path (for example `AWS_WEB_IDENTITY_TOKEN_FILE`) by name.
  - **New** `Reporter.info(line)`, and every backend's `step_ok`/`step_skip`/`info` pass the summary through `safe_line`.
- **Modify** `scripts/ci/reporters/teamcity.py` (CR-4). The TeamCity fence (`disableServiceMessages` ... `enableServiceMessages`) covers only the tool excerpt. Every other line the backend prints and did not author itself (`ok:`, `skip:`, `info:`, RC4, `first_diagnostic` in `buildProblem`) is passed through `_neutralise`, so `##teamcity[` never appears in it unless the backend wrote the message. The GitHub backend needs no equivalent (it interprets commands only at line start, and labels start alphanumeric), but its `info()` still applies `safe_line`.
- **Modify** `scripts/ci/ci_step.py`:
  - **`--hygiene counts-only`** (the `run` sub-command) and `run_step(..., hygiene="counts-only")` (CR-9, RC1 data hygiene). Under it:
    - the failure excerpt is only lines matching the anchored CTest status-line regex `^\s*\d+/\d+ Test +#\d+: \S+ \.+\s*(\*{3}\w+|Passed|Failed|Not Run)` plus the fixed CTest summary lines (`tests passed`, `Total Test time`);
    - `first_diagnostic` (RC1 and the RC3 record) is chosen from the same filter, or is the literal `see status lines`;
    - nothing else from the log is printed, annotated or recorded;
    - the step's log is written under `CI_LOG_DIR/live/`, a directory no upload path names.
    The mode is opt-in per step; callers that run a live-table step must pass it (enforced by tests in Story 03.0 Tasks 01-02, and merge-blocking per the threat model).
  - `run_step(..., pass_env=())` forwards to `child_environment`.
  - **File safety** (CR-11): `os.umask(0o077)` for files the runner creates; logs opened with `O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW` on POSIX; the RC3 results file created 0600; a `CI_LOG_DIR` that is group- or world-writable and was not created by this runner is refused (exit 2). Local `plain` runs print one `info: logs kept in <dir>` line.
- **Modify** `scripts/ci/env_names.py`:
  - **New** `CI_RUN_ATTEMPT` (GitHub `github.run_attempt`; TeamCity: no equivalent, the adapter sets `1` because `teamcity.build.id` is already unique per attempt). Used by S3 staging keys (ADR 0005 D9).
  - **New** `CI_OUTPUT_FILE`: the neutral job-output channel (GitHub: `GITHUB_OUTPUT`, set by the adapter only for the one `stage.py` process, never inherited by its children; TeamCity: `##teamcity[setParameter ...]` through the reporter).
  - The TeamCity secure-parameter names used by the denylist above.
- **Modify** `scripts/ci/reporters/` (all backends): **new** `Reporter.set_output(name, value)` (CR-6, CR-8). `name` must match `^[a-z][a-z0-9_]{0,63}$`; `value` is written as single-line JSON with no CR/LF, using a heredoc delimiter drawn from `secrets.token_hex(16)` per call. This is the only way any script sets a job output; Story 03.0 Task 01 uses it for the `install/<leg>/` tree digest and Story 04.0 Task 02 for the `plan` matrix.
- **Modify** `scripts/ci/tests/fixtures/hostile.log` (CR-4): add `##[set-output name=x;]y`, `##[error]x`, and a mid-line `ok: x ##teamcity[buildStatus text='green']`.

## Functions and Classes

```python
# reporters/__init__.py
CHILD_ENV_DENY_NAMES: Final[frozenset[str]]
CHILD_ENV_DENY_PREFIXES: Final[tuple[str, ...]]   # ("CI_PUBLISH_", "AWS_", ...)
def child_environment(environ: Mapping[str, str], pass_env: Sequence[str] = ()) -> dict[str, str]: ...

class Reporter(Protocol):
    def info(self, line: str) -> list[str]: ...                 # sanitised, never interpretable
    def set_output(self, name: str, value: object) -> None: ...  # single-line JSON, random delimiter

# ci_step.py
Hygiene = Literal["full", "counts-only"]
def run_step(label: str, argv: Sequence[str], *, ..., hygiene: Hygiene = "full",
             pass_env: Sequence[str] = ()) -> StepResult: ...
```

The existing `run_step` signature keeps every current parameter; the two new keyword parameters default to today's behaviour.

## Constraints

- No change to RC1-RC5 wording or ceilings. `counts-only` only narrows what an excerpt may contain.
- P11 still holds for the new code: only `reporters/` and `env_names.py` name `GITHUB_*`/`TEAMCITY_*`.
- The denylist is a security control: removing a name needs a threat-model note.
- `sudo` children (Q4) already get `env_reset`; the scrub still runs first.

## Tests

- `test_ChildDoesNotInheritDebugSwitches` is extended to every denied name and prefix above, plus a positive `pass_env=("AWS_WEB_IDENTITY_TOKEN_FILE",)` case.
- A failing step under `--hygiene counts-only` whose log contains the fake serial `TEST-SERIAL-0000` among CTest status lines prints the status lines and never the serial, in the excerpt or in the RC3 record; its log is under `CI_LOG_DIR/live/`.
- For every backend, `step_ok`/`step_skip`/`info` given a hostile summary print no `##teamcity[` the backend did not author, and no line starting with `::` or `##[`.
- `set_output` with a value containing `\n`, `\r` or a fixed delimiter string produces exactly one output entry; an invalid name raises.
- A symlinked `<label>.log` is refused and its target is unchanged; created files are mode 0600; a world-writable `CI_LOG_DIR` the runner did not create -> exit 2.
- `python3 scripts/ci/run_tests.py` stays green (37 existing tests plus these).

## Success criteria

- [x] No child process of `ci_step` sees a denied name unless passed by name.
- [x] Live-table steps can run under `counts-only`, and the fake-serial test proves nothing else leaks.
- [x] Every unfenced line is sanitised on all three backends.
- [x] `set_output` is the only job-output writer; the threat-model per-task requirements 1-5 are each covered by a test.

## As-built notes (2026-11-13)

**Files.**
- `scripts/ci/reporters/__init__.py`: `CHILD_ENV_DENY_NAMES`, `CHILD_ENV_DENY_PREFIXES`, `is_denied_child_name()`, `child_environment(environ, pass_env=())`; `ok_line`/`skip_line`/`info_text` pass label and text through `safe_line` and `neutralise_service_messages()`; `output_json()`/`output_delimiter()` for `set_output`; `Reporter.set_output` in the protocol. `CHILD_ENV_DENYLIST` (the original three names) is kept for Story 01.0 callers.
- `scripts/ci/reporters/{github,teamcity,plain}.py`: `set_output` on every backend (GitHub: `name<<ci_output_<token_hex(16)>` heredoc appended to `CI_OUTPUT_FILE` in one write, `O_NOFOLLOW`, 0600; TeamCity: one `setParameter` line, value `|`-escaped and not cut; plain: one `output: <name>=<json>` line). TeamCity neutralises `ok:`/`skip:`/`info`, the `buildProblem` first diagnostic, the best-effort reason and the summary `buildStatus`/`publishArtifacts` values.
- `scripts/ci/ci_step.py`: `Hygiene`, `--hygiene full|counts-only`, `counts_only_line()`, `CTEST_STATUS_LINE`, `SEE_STATUS_LINES`, `LIVE_LOG_SUBDIR`; `run_step(..., hygiene=, pass_env=)`; `UnsafePathError`, `prepare_log_dir()`, `_open_log()`, scoped `_private_umask()`; RC3 file created 0600 with `O_NOFOLLOW`; public `emit()`/`emit_error()` (`_emit` kept as an alias).
- `scripts/ci/env_names.py`: `CI_RUN_ATTEMPT` (local default `1`), `CI_OUTPUT_FILE`, `TEAMCITY_SECURE_NAMES`, `TEAMCITY_SECURE_PREFIXES`.
- `scripts/ci/tests/fixtures/hostile.log`: the three CR-4 lines.
- P11 single-emitter fix (Task 03 blocker): the 7 direct writes in `apt_install.py`, `bootstrap_vcpkg.py` (2), `print_env.py` and `run_tests.py` (3) now go through `ci_step.emit_error()` (plain reporter `info`, stderr) or `ci_step.emit()`.
- Tests: `scripts/ci/tests/test_step_runner_security.py` (new, 23) and `test_ci_step.py` (`test_ChildDoesNotInheritDebugSwitches` extended to every denied name and prefix, new `test_PassEnvKeepsOnlyTheNamedDeniedName`).

**Decisions and deviations.**
- **Umask is scoped, not process-wide.** `os.umask(0o077)` wraps only the runner's own directory and file creation. A process-wide umask would be inherited by every child, so build and `cmake --install` output (and later the packaged archives) would become 0600. A test pins that a child keeps the caller's umask.
- **"Not created by this runner".** A log directory the runner creates is made 0700, so it always passes the group/world-writable check; an existing one is checked by mode bits (POSIX only; Windows has no such bits). The CLI returns exit 2 with one stderr line; library callers get `UnsafePathError` (an `OSError`) before anything runs or is recorded.
- **`info: logs kept in <dir>`** is printed only by local (`CI_PLATFORM=local`) `plain` runs, and only with a failing or best-effort-warning step whose child started, inside the RC1 ceiling. Printing it on success would break RC1's one-line success and `lint.py`'s three-line success (RC1 wording is frozen by this task's Constraints).
- **`counts-only` re-renders, it does not relay.** Only the regex-matched part of a status line (plus an optional `<n> sec`) is kept, so text after the status word (for example after `***Exception`) never leaves the log. The fixed summary lines are matched in full. `first_diagnostic` is the first status line that matches the caller's pattern sets, else `see status lines` (runner-authored timeout/not-started texts are kept).
- **`CI_OUTPUT_FILE` is denied to children** as well as `GITHUB_OUTPUT`, because it is the same channel under its neutral name ("never inherited by its children").
- **TeamCity secure names.** TeamCity has no fixed list of password-parameter environment names, so `env_names.py` denies `TEAMCITY_BUILD_PROPERTIES_FILE` (the file that holds every parameter value) and reserves the prefixes `TEAMCITY_SECURE_`/`BUILD_SECURE_` for a TeamCity adapter that must export a password-type parameter; publish credentials map onto the already-denied `CI_PUBLISH_*`.
- **Neutralisation on every backend.** The test contract ("for every backend ... no `##teamcity[` the backend did not author") is met by applying `neutralise_service_messages()` in the shared `ok_line`/`skip_line`/`info_text`, not only in the TeamCity backend.
- **Job-output names** are checked with `fullmatch`, because `$` in `^[a-z][a-z0-9_]{0,63}$` also matches before a trailing newline.

**Pending on a runner.** `set_output` against a real `GITHUB_OUTPUT` file (first used by Story 03.0 Task 01's `stage.py`), the scrub seen from a real hosted job (OIDC/runtime variables present), and the TeamCity backend (unused until a migration).

