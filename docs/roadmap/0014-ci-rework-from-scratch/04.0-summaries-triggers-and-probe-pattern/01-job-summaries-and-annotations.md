# 01 - Job summaries and annotations

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

A reader opening a run sees, without opening a single log:

- which jobs and legs failed;
- the first failure of each, said once;
- key counts;
- the versions involved;
- which best-effort steps warned.

Each block is bounded (plan E9).

## Files

- **Create** `scripts/ci/job_summary.py`. It runs as the last step of every job with `if: always()`. It reads `$RUNNER_TEMP/ci-results.jsonl` (RC3) and `CI_ENV_LINE` (RC4), and appends one block of at most 30 lines and 4 KiB to `$GITHUB_STEP_SUMMARY`:

  ```text
  ### <job> (<leg>) - PASS | FAIL | PASS with <k> best-effort warnings
  <RC4 line>
  | step | result | detail |          <- one row per RC3 record; detail <= 120 chars
  first failure: <label>: <first_diagnostic>   <- only when FAIL
  ```

  It also writes the same records as a JSON artifact, `summary-<job>-<leg>` (`actions/upload-artifact`, SHA-pinned, `retention-days: 3`; at most 8 KiB, no log content), for the run-level report. This is needed because matrix job `outputs` collapse to one leg.
- **Create** `scripts/ci/run_report.py`, used by a new **`report`** job (`needs: [lint, build, sanitizers, gui]`, `if: always()`, Linux, `timeout-minutes: 5`, `permissions: contents: read`). It downloads the `summary-*` artifacts (`actions/download-artifact`, SHA-pinned) and writes one table of at most 40 lines:

  ```text
  ## CI: <PASS|FAIL> - <n> legs passed, <f> failed, <w> with warnings, <s> not run (tier)
  | job | linux | windows | macos | first failure (once) |
  versions: <one line per distinct cmake/ctest/compiler set, deduplicated>
  ```

  When several legs share the same first diagnostic, it is printed once with the legs listed. The job fails only if a needed job failed, and then it adds one `::error title=CI::<n> legs failed: <list>` annotation. That annotation is the one line a reader sees at the top of the run page.
- **Modify** `ci.yml`: the summary step in every job; the `report` job.

## Functions and Classes

```python
def render_job_block(records: Sequence[dict], env_line: str, job: str, leg: str,
                     max_lines: int = 30, max_bytes: int = 4096) -> str: ...
def render_run_table(job_records: Mapping[tuple[str, str], Sequence[dict]],
                     expected_legs: Sequence[tuple[str, str]], max_lines: int = 40) -> str: ...
def dedupe_failures(firsts: Mapping[tuple[str, str], str]) -> list[tuple[str, list[str]]]: ...
```

## Constraints

- **Annotations budget.** Per run: the RC1 `::error` lines (one per failing step) plus at most one run-level `::error`. Best-effort `::warning` lines are per step. Nothing else emits annotations. GitHub shows at most 10 error and 10 warning annotations per step, so the RC1 limit of one per step keeps every one visible.
- **No log content in summaries or artifacts.** `first_diagnostic` is already escaped, cut to 160 characters and free of live-table data (RC1). Summary text that comes from tool output is rendered in a code span, so Markdown or HTML in it is inert.
- A missing `summary-*` artifact (a job cancelled or never started) shows as `no record`, never as PASS.

## Tests

- `scripts/ci/tests/test_job_summary.py` and `test_run_report.py`:
  - 0, 1 and 50 records stay within the line and byte caps, with a `(+k more)` row;
  - three legs with the same first diagnostic produce one row listing three legs;
  - a missing artifact shows `no record`;
  - a best-effort `warn` gives `PASS with 1 best-effort warning`;
  - Markdown or `<script>` in a diagnostic is rendered inert.
- On a real run, a green run shows one `## CI: PASS` table, and negative proof 3 (Story 03.0 Task 04, re-run) shows `FAIL` with the gui-ci failure on one row listing all three legs.

## Success criteria

- [ ] Every job writes a block of at most 30 lines; the run writes one table of at most 40 lines.
- [ ] A single failing step produces exactly: its RC1 `::error`, its row in the job block, and one row in the run table. It never appears twice in any of them.
