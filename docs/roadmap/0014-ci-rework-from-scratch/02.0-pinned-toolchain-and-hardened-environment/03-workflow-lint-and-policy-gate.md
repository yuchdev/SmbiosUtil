# 03 - Workflow lint and policy gate

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01, Story 01.0 Task 03

## Objective

Turn this milestone's structural rules into a fast, mechanical check that runs first in CI and locally before any push. A workflow that would fail for a syntax, shell-flag, permission, pinning or timeout reason is then rejected in seconds, not after a 30-minute cycle (plan E5, E6, E7).

## Files

- **Create** `scripts/ci/check_workflow_policy.py`. It parses each workflow under test (a list passed on the command line; until cutover, only `ci-next.yml`). The YAML loading is limited to the GitHub Actions subset the workflow uses, so no PyYAML dependency is needed. The alternative is to run against `actionlint -format '{{json .}}'` output, decided during implementation and recorded. It prints RC1 discrepancy lines, `<file>:<job>/<step>: <rule>: expected <x> -> actual <y>`. Rules:
  - **P1** Top-level `permissions:` exists and is `contents: read`. A job-level widening needs a `# permissions:` comment with a reason.
  - **P2** Every `uses:` is pinned to a 40-hex SHA followed by a `# vX.Y.Z` comment (C21). Local `./` actions are exempt.
  - **P3** No `runs-on` (or matrix `os`) value ends in `-latest` (E2).
  - **P4** Every job has `timeout-minutes`, and every step that has a `run:` or network-dependent `uses:` has `timeout-minutes`. Budgets are 2x the Task 01 baseline medians (Story 01.0 Task 01), rounded up, and recorded in a table in `docs/dev/ci.md`.
  - **P5** No `continue-on-error: true` (RC5: best-effort goes through `ci_step --best-effort`).
  - **P6** No `${{ ... }}` expression inside a `run:` body. Values reach scripts through `env:` or argv (expression-injection hygiene).
  - **P7** A `run:` body has at most 3 non-empty lines and no `if`/`for`/`while`/`case`/`&&`/`||` control flow. Logic belongs in tested scripts.
  - **P8** No `pull_request_target`, no `secrets.` reference, no `ACTIONS_STEP_DEBUG`/`ACTIONS_RUNNER_DEBUG` (C22; 0011 R-6).
  - **P9** Only allowlisted workflow files exist under `.github/workflows/`: `ci.yml`, plus `ci-next.yml` until cutover, plus the 0012 `release.yml` once it exists. This catches a leftover probe workflow (Story 04.0 Task 03).
- **Create** `scripts/ci/install_actionlint.py`. It downloads one pinned actionlint release asset for the host, verifies its SHA-256 against a value in the script, and unpacks it to `$RUNNER_TEMP` (or a temp dir locally). actionlint runs `shellcheck` on `run:` blocks when shellcheck is on `PATH`, and the script records whether it was (`info:` line).
- **Create** `scripts/ci/lint.py`. The one local and CI entry point: `run_tests.py` (Story 01.0 Task 03) + actionlint + `check_workflow_policy.py`. Each goes through `ci_step`, so the output is 3 `ok:` lines on success.

## Functions and Classes

```python
@dataclass(frozen=True)
class Violation:
    file: str; location: str; rule: str; expected: str; actual: str

def check_workflow(path: Path, allowed_files: Sequence[str]) -> list[Violation]: ...
def main(argv: Sequence[str]) -> int: ...   # exit 1 if any violation; prints <= 20 lines + "(+k more)"
```

## Constraints

- The gate itself follows RC1: one `ok:` line, or discrepancy lines.
- Budgets in P4 are data (a table the script reads from `scripts/ci/step_budgets.json`), so a change to a budget is a reviewed one-line diff.
- The `security-auditor` reviews P1-P9 before Story 03.0 starts, and the threat model `docs/security/<date>-ci-rework.md` records the result.

## Tests

- `scripts/ci/tests/test_check_workflow_policy.py`: one minimal violating workflow fixture per rule P1-P9 -> exactly that violation; one fully compliant fixture -> no violations. A fixture combining all violations prints at most 20 lines plus `(+k more)`.
- `scripts/ci/tests/test_shell_flags.py` (Story 01.0 Task 03) now extracts `run:` bodies from the workflow fixtures and runs them under `bash --noprofile --norc -e -o pipefail`.
- Run against today's `ci.yml` once, as evidence only: every P-rule it breaks is listed in this task's as-built notes. The expected list is P1, P3, P4, P5, P6 and P7.

## Success criteria

- [ ] `python3 scripts/ci/lint.py` runs locally in under 60 s (after the first actionlint download) and prints 3 lines on success.
- [ ] Each rule has a passing and a failing fixture.
- [ ] Today's `ci.yml` violations are recorded as the "before" evidence.
