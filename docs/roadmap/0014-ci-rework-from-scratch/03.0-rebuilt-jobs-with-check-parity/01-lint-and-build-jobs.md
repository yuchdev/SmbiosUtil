# 01 - `lint` and `build` jobs

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Create `ci-next.yml` with the `lint` job and the 3-OS `build` job, carrying C01-C11 over (C05 dropped as dead), with every step under RC1.

## Files

- **Create** `.github/workflows/ci-next.yml`:
  - Top level: `name: CI (next)`; the same triggers as `ci.yml` plus `branches-ignore: ['probe/**']`; `permissions: contents: read`.
  - **`lint`** (pinned Linux label, `timeout-minutes` from the budgets table): checkout -> `print_env.py` (RC4) -> `ci_step.py self-test` -> `lint.py` (script tests, actionlint, policy gate on `ci-next.yml`) -> C01 `check_cversions_isolation.py` -> C02 `check_doc_samples.py --check`.
  - **`build`** (`needs: lint`, matrix `linux`/`windows`/`macos` on the Story 02.0 labels, `fail-fast: false`): checkout -> RC4 -> self-test -> pinned tools (`requirements-tools.txt`) -> `run-vcpkg` (pinned `vcpkgGitCommitId`) -> `actions/cache` (Story 02.0 key scheme, flavor `default`) -> configure (`ci_step`, patterns `cmake,vcpkg`) -> cache-health line -> build (`ci_step`, patterns `cxx`) -> C06 on Windows (Task 02's `ctest_report.py`) -> C09 info line on Linux -> C07/C08 (`cli_format_check.py`) -> C10 on Windows (best-effort, Task 02).
  - The matrix carries `py` (`python3`/`python`), `preset` and `config` (`Debug` on Windows, empty elsewhere). All of them reach scripts through `env:` (P6).
- **Create** `scripts/ci/cli_format_check.py`, which replaces the C07/C08 inline block with identical semantics:
  - find the `smbios` binary under `build/<preset>` (`*/smbios/bin/smbios[.exe]`); none found is a hard failure;
  - Linux only: one `info:` line with the sysfs table modes (C09), as `<name> <mode> <owner>` triples, at most 3;
  - for each of `text xml json yaml csv`: run `examples/scripts/check_structured_output.py --format <f> -- [sudo] <bin> --format <f>` without a shell. Collect its `ok: <fmt>, <N> ...` line, and turn a `0 structures`/`0 rows` result into one `::warning title=empty table::` per format, as today;
  - byte-compare no-flag output with `--format text` in memory, never printed or written to the workspace. A mismatch prints `no-flag output: expected == --format text -> actual differs (<a> vs <b> bytes)`;
  - success: one line, `ok: cli formats: text <n>, xml <n>, json <n>, yaml <n>, csv <n>; no-flag == text`.
- **Failure-log artifacts:** only if plan Q3 is answered yes. Then a failing configure/build step's log goes to `actions/upload-artifact` (SHA-pinned, `if: failure()`, `retention-days: 3`, name `logs-<job>-<leg>`). Live-table and test logs are never uploaded.

## Functions and Classes

```python
# cli_format_check.py
FORMATS: Final = ("text", "xml", "json", "yaml", "csv")
def find_cli(build_dir: Path) -> Path | None: ...
def check_format(py: str, cli: Path, fmt: str, sudo: bool) -> tuple[bool, int, str]: ...  # ok, count, unit
def noflag_equals_text(cli: Path, sudo: bool) -> tuple[bool, int, int]: ...
def main(argv: Sequence[str]) -> int: ...
```

## Constraints

- C01 and C02 run **once** (in `lint`), not per leg (plan inventory disposition). They no longer run in `build`/`sanitizers`.
- The `allow_build_failure` matrix knob is not carried over (dead, C04).
- Step budgets come from `scripts/ci/step_budgets.json` (Story 02.0 Task 03).
- `cli_format_check.py` never prints CLI stdout, `check_structured_output.py` stderr beyond its own fixed `FAIL: <reason>` line, or any field value.

## Tests

- `scripts/ci/tests/test_cli_format_check.py`, with a stub CLI:
  - the five formats with counts -> one `ok:` line;
  - an empty table -> five warnings and still `ok:`;
  - no-flag mismatch -> a hard failure with the byte counts;
  - missing binary -> a hard failure;
  - stub output containing `::error::` is never echoed.
- Push the branch. `ci-next` `lint` + `build` are green on all three legs, side by side with the old `ci.yml`.

## Success criteria

- [ ] `lint` finishes in under 3 minutes and blocks the other jobs when it fails.
- [ ] On a green run, every project-owned step in `build` prints exactly one line (`ok:`/`info:`/`skip:`), plus the RC4 line.
- [ ] C01, C02, C04, C06-C11 are present, with the gates the inventory names.
