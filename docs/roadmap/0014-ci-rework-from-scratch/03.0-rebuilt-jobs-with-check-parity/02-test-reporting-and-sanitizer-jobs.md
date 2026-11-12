# 02 - Test reporting and the `sanitizers` job

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
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
  - **Run:** `ctest --test-dir <dir> [-C <config>] <set args> --output-junit <RUNNER_TEMP>/junit-<label>.xml --timeout 60 --no-tests=error`, through `ci_step`. Only the `unprivileged` and `gui_ci` sets add `--output-on-failure`. Live-table sets never do (RC1 data hygiene).
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

- [ ] C06, C10, C12, C13 and C14 present, with the inventory's gates. Best-effort steps are explicit (RC5).
- [ ] Test counts in every `ok:` line come from JUnit. No step parses ctest's summary wording.
- [ ] D2 outcome recorded: fatal UBSan added, plus any findings handed off, or the maintainer declined.
