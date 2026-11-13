# 02 - Test reporting and the `sanitizers` job

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete locally (runner checks pending)
**Depends on:** Task 01

## Objective

Report every ctest run by exit code plus JUnit counts in a form that cannot break when CTest's wording changes (plan E4). Make the best-effort test steps explicit (E8, RC5). Rebuild the `sanitizers` job (C12-C14) so that a sanitizer finding actually fails a test (E11, subject to D2).

## Files

- **Create** `scripts/ci/test_sets.json`. Named ctest selections, copied verbatim from today's workflow:
  - `unprivileged`: `-R "smbios_unit_test|smbios_mock_test|smbios_c_|smbios_cli_unknown_format|smbios_example_"` (C06, C13);
  - `live_table`: `-R "smbios_integration_test|smbios_perf_test|smbios_cli_format_csv"` (C10, C14);
  - `gui_ci`: `-L gui-ci` + `--no-tests=error` (C19).

  Every set carries `--timeout 60` (C11).
- **Create** `scripts/ci/ctest_report.py`:
  - **Run:** `ctest --test-dir <dir> [-C <config>] <set args> --output-junit $CI_TEMP_DIR/junit-<label>.xml --timeout 60 --no-tests=error`, through `ci_step` (amended 2026-11-13: `$CI_TEMP_DIR`, not the platform name, per P11 and threat model CR-13). Only the `unprivileged` and `gui_ci` sets add `--output-on-failure`. Live-table sets never do, and they run under `ci_step`'s `hygiene="counts-only"` with logs under `CI_LOG_DIR/live/` (Story 02.0 Task 04; RC1 data hygiene, CR-9). Before parsing, `parse_junit` rejects any input whose first 4 KiB contains `<!DOCTYPE` or `<!ENTITY` (`counts unavailable`), and test names in discrepancy lines must match `^[A-Za-z0-9_./:-]{1,120}$`, else `<test #n>`.
  - **Report:** parse the JUnit XML with `xml.etree.ElementTree` (no external entities, size capped at 32 MiB). Success: `ok: tests <set>: <p> passed, <s> skipped (<n> total, <t>s)`. Failure: RC1 excerpt + one discrepancy line per failing test, `<test name>: expected passed -> <failed|timeout|notrun>`, at most 20.
  - If the XML is missing or unreadable, the exit code alone decides and the summary says `counts unavailable`. Counts go into the RC3 record.
- **Modify** `ci-next.yml` - add the **`sanitizers`** job (`needs: lint`, linux + macos):
  - pinned tools -> vcpkg (cache flavor `asan`) -> configure into `build/<preset>-asan` with today's flags (C12) -> build -> `unprivileged` set (hard, C13) -> `live_table` set (best-effort, C14, or `skip:` per Q4).
  - `ASAN_OPTIONS` is kept exactly as today: `detect_container_overflow=0` everywhere, plus `detect_leaks=0` on macOS. Both reasons are kept as comments: the libc++ `vector<string>` container-overflow false positive, and LeakSanitizer being unsupported on macOS.
  - **If D2 = yes:** add `-fno-sanitize-recover=undefined` to the configure flags and set `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. If this turns up existing undefined behaviour, the findings are recorded (counts + file:line, no data) and handed to `cpp-expert`. The gate is not weakened to get a green run.
- **Modify** `ci-next.yml` `build` job: wire C06 (`unprivileged`, Windows, hard) and C10 (`live_table`, Windows, best-effort with the reason "a virtualized runner's firmware may not expose an SMBIOS table") through `ctest_report.py`. Per Q4, the Linux `build` leg may run `live_table` under `sudo` (hard or best-effort, per the maintainer's answer).

## Functions and Classes

```python
@dataclass(frozen=True)
class TestCounts:
    passed: int; failed: int; skipped: int; not_run: int; total: int

def parse_junit(path: Path, max_bytes: int = 32 << 20) -> tuple[TestCounts, list[tuple[str, str]]] | None: ...
def run_set(set_name: str, test_dir: Path, config: str | None, label: str,
            best_effort_reason: str | None) -> int: ...
def main(argv: Sequence[str]) -> int: ...
```

## Constraints

- No regex in `ctest_report.py` matches ctest's human summary line. A unit test asserts this by feeding both ctest 3.31 and 4.4.5 summary wordings with identical JUnit and getting identical output.
- JUnit `system-out` content (test stdout) is never printed and never copied into RC3.
- Best-effort steps never use `continue-on-error` (P5). Their outcome is visible as `warn` in RC3 and Story 04.0's summary.

## Tests

- `scripts/ci/tests/test_ctest_report.py`:
  - JUnit fixtures for all-pass, one-fail, one-timeout, all-skipped and zero tests (must fail for `gui_ci`, via `--no-tests=error`);
  - a malformed or oversized XML gives `counts unavailable` and the exit code decides;
  - the two ctest wordings give identical output;
  - `system-out` text containing `::error::` never reaches stdout.
- On runners: the `sanitizers` legs are green and show `ok: tests unprivileged: <p> passed ...`.
- With D2 = yes, one throwaway local check: a deliberately UB-triggering snippet compiled with the CI flags exits non-zero. Recorded, not committed.

## Success criteria

- [x] C06, C10, C12, C13 and C14 present, with the inventory's gates. Best-effort steps are explicit (RC5).
- [x] Test counts in every `ok:` line come from JUnit. No step parses ctest's summary wording.
- [x] D2 outcome recorded: fatal UBSan added, plus any findings handed off, or the maintainer declined. (Added; product findings can only come from the first runner run, see below.)

## As-built notes (2026-11-13)

**Files.** Created: `scripts/ci/test_sets.json`, `scripts/ci/ctest_report.py`, `.github/workflows/stage-sanitizers.yml`, `scripts/ci/tests/test_ctest_report.py` (32 tests), `scripts/ci/tests/fixtures/junit/` (8 JUnit fixtures shaped on real ctest 3.31 output, plus a README). Modified: `scripts/ci/ci_step.py` (`run_step(finish=...)` hook and `StepFinish`), `scripts/ci/stage.py` (`unit`/`live-table` through `ctest_report`; sanitizer legs; `asan` cache flavor; `sanitizer_env`), `scripts/ci/legs.json` (legs `linux-asan`/`macos-asan` with `"job": "sanitizers"`; Linux `live_table_tests: true`; tiers), `scripts/ci/step_budgets.json` (`sanitizers/*` steps), `.github/workflows/ci-next.yml` (`sanitizers` job, `needs: lint`), `.github/workflows/stage-build.yml` (comments only), `scripts/ci/tests/test_stage.py` (+8 tests; tier/runner/matrix checks now per job), `docs/dev/ci.md`. `ci.yml` and all product C++/CMake files are unchanged.

**Verification (local).** `python3 scripts/ci/run_tests.py`: `ok: scripts/ci tests: 304 tests`. `python3 scripts/ci/lint.py --skip-tests`: actionlint 1.7.12 ok on 4 workflows, policy ok. `check_workflow_policy.py` on `ci-next.yml` + the three `stage-*.yml`: ok (P1-P15). A real run of the `unprivileged` set against the local CLion tree (`cmake-build-debug`, ctest 3.31, no rebuild): `ok: tests unprivileged: 18 passed, 0 skipped, 18 total (5.2s)`, with the counts in the RC3 record.

**D2 throwaway check (recorded, not committed).** A signed-overflow snippet built with Apple clang 14 and the C12 flags: without `-fno-sanitize-recover=undefined` and without `UBSAN_OPTIONS`, it exits 0 after the UBSan report (E11 reproduced). With the new flag it exits 134 before the next statement, with or without `UBSAN_OPTIONS`.

**Decisions and deviations.**
- **One RC1 line, counts from JUnit.** `ci_step.run_step` gained an optional `finish(rc) -> StepFinish` hook. It runs after the child exits and before rendering, so the summary, the RC3 `counts`, `first_diagnostic` and the leading discrepancy lines come from `ctest_report` while truncation, fencing and hygiene stay in `ci_step` (RC2). The exit code alone still decides.
- **`ok:` shape.** This reads `ok: tests <label>: <p> passed, <s> skipped, <n> total (<elapsed>s)`, not `(<n> total, <t>s)`, because the parenthesised elapsed time is `ci_step`'s own RC1 suffix. Labels are `tests unprivileged`, `tests live-table` and `tests gui-ci`.
- **Classification** uses ctest's JUnit attributes, not summary text. `status="run"` is passed. `fail` with `<failure message="Timeout">` is a timeout (counted under `failed`). `notrun` with a `SKIP_*` message, and `disabled`, are skipped. Any other `notrun` is not run. An unknown status counts as failed (fail closed).
- **Hardening beyond the spec.** `<!DOCTYPE`/`<!ENTITY` is refused anywhere in the capped file, not only in the first 4 KiB (a DTD can follow 4 KiB of comments). Symlinked or non-regular JUnit files are refused. A stale `junit-<label>.xml` is deleted before each run, so old counts can never be reported. A symlink at that path exits 2.
- **`--no-tests=error` on every set**, as the Run line says. This is stronger than the old C06/C13 (an empty selection now fails).
- **Sanitizer legs are legs, not a stage.** SC1's stage names are fixed, so `linux-asan`/`macos-asan` run the `build` and `test` stages with `"job": "sanitizers"`. They configure into `build/<preset>-asan` and use the `asan` cache flavor. `ASAN_OPTIONS`/`UBSAN_OPTIONS` are set by `stage.py` (`sanitizer_env`, with both reasons kept as comments) and scoped to the ctest child. `cli-formats` prints `skip:` there, because C07 runs in the build job.
- **Q4.** On `linux` (build job) the live-table set runs under `sudo -n <absolute ctest>`, because `sudo`'s `secure_path` would miss the tools venv. It is **best-effort** (C10 reason), and never uses sudo locally. On `linux-asan` it prints `skip:` (Q4). On `macos-asan`, C14 stays best-effort and counts-only, because IOKit needs no root and Q4 concerned Linux only.
- **No failure-log upload in `stage-sanitizers.yml`.** Policy P15 (CR-10) allows `logs-*` uploads only in `stage-build.yml`, and the old sanitizers job had none.
- **Tiers** list the legs of every job. Each job's workflow matrix is checked against its own legs (`test_stage.py`). A branch push runs `linux` + `linux-asan`.

**Runner-only (pending).**
- Green `sanitizers` legs that show `ok: tests unprivileged: <p> passed ...`.
- Any UB that fatal UBSan exposes in product code. It goes to `cpp-expert` with counts and file:line only, and the gate is not weakened.
- `sudo -n` ctest on ubuntu-24.04.
- The `asan` cache key on both legs.
- Whether a cold sanitizer build fits the 11-minute P4 job budget (the old job had 30 minutes; the budget comes from warm-cache medians).
- Windows C06/C10 through `ctest_report` with `-C Debug`.
- ctest 4.4.x JUnit attributes, which the fixtures assume match 3.31's.
