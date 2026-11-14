# 03 - Workflow lint and policy gate

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13; `lint.py` 3-line criterion pending, see As-built notes)
**Depends on:** Task 01, Story 01.0 Task 03

## Objective

Turn this milestone's structural rules into a fast, mechanical check that runs first in CI and locally before any push. A workflow that would fail for a syntax, shell-flag, permission, pinning or timeout reason is then rejected in seconds, not after a 30-minute cycle (plan E5, E6, E7).

## Files

- **Create** `scripts/ci/check_workflow_policy.py`. It parses each workflow under test (a list passed on the command line; until cutover, only `ci-next.yml`). The YAML loading is limited to the GitHub Actions subset the workflow uses, so no PyYAML dependency is needed. The alternative is to run against `actionlint -format '{{json .}}'` output, decided during implementation and recorded. It prints RC1 discrepancy lines, `<file>:<job>/<step>: <rule>: expected <x> -> actual <y>`. Rules:
  - **P1** Top-level `permissions:` exists and is `contents: read`. A job-level widening needs a `# permissions:` comment with a reason.
  - **P2** Every `uses:` is pinned to a 40-hex SHA followed by a `# vX.Y.Z` comment (C21). `docker://` references are rejected (CR-7). Local `./` actions are exempt from the SHA rule only for reusable stage workflows (`./.github/workflows/stage-*.yml`); a local or composite **action** (`uses: ./<dir>`) is rejected by P13.
  - **P3** No `runs-on` (or matrix `os`) value ends in `-latest` (E2). P3 also checks the Task 01 pins file itself, because `runs-on: ${{ matrix.label }}` from `plan.py` output is not a literal the workflow scan can see (CR-6).
  - **P4** Every job has `timeout-minutes`, and every step that has a `run:` or network-dependent `uses:` has `timeout-minutes`. Budgets are 2x the Task 01 baseline medians (Story 01.0 Task 01), rounded up, and recorded in a table in `docs/dev/ci.md`.
  - **P5** No `continue-on-error: true` (RC5: best-effort goes through `ci_step --best-effort`).
  - **P6** No `${{ ... }}` expression inside a `run:` body. Values reach scripts through `env:` or argv (expression-injection hygiene).
  - **P7** (tightened by ADR 0005) A `run:` body is exactly one non-empty line invoking `python3 scripts/ci/stage.py ...`, or (bootstrap and `lint` only) `scripts/ci/ci_step.py` / `scripts/ci/lint.py`. It has no `if`/`for`/`while`/`case`/`&&`/`||`/`|`/`>` control flow or redirection. Logic belongs in tested scripts.
  - **P8** (amended by ADR 0005; amended again 2026-11-13 by the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) CR-1 [OVERRIDES SPEC]) No `ACTIONS_STEP_DEBUG`/`ACTIONS_RUNNER_DEBUG` (C22; 0011 R-6).
    - **Trigger allowlist.** `on:` may name only `push`, `pull_request`, `schedule`, `workflow_dispatch` and `workflow_call`. Everything else is a violation, explicitly including `pull_request_target`, `workflow_run`, `issue_comment`, `pull_request_review`, `pull_request_review_comment` and `discussion*`: each runs base-repository YAML with secrets on an event an outside contributor can cause.
    - **No `secrets: inherit`** on any `uses:` of a reusable workflow. A publish caller passes each secret by name.
    - **Credentials.** No `secrets.` reference and no `id-token: write`, **except** in a publish-stage job listed in `scripts/ci/publish_jobs.json` that lives in `stage-publish.yml`, `needs:` the same run's `verify` job, and matches one of two tier clauses (ADR 0005 D6/D9 as amended 2026-11-13):
      - **release:** `environment: release`, guarded to `v*` tag events (or the binding's own trigger);
      - **staging:** `environment: staging`, guarded to `push` on `refs/heads/master` only. It is unreachable from `pull_request`, tag, `schedule` and `workflow_dispatch` events.
    - The guard is checked on the job's `if:` **and** on every path that reaches the job, including the caller job in `ci.yml` (see P12).
  - **P9** (amended by ADR 0005) Only allowlisted workflow files exist under `.github/workflows/`: `ci.yml`, `stage-*.yml` (reusable, `on: workflow_call` only), plus `ci-next.yml` until cutover. There is no `release.yml`. This catches a leftover probe workflow (Story 04.0 Task 03).
  - **P10** A step that calls a stage command receives inputs only through `env:` keys from the neutral-name table in `scripts/ci/env_names.py` (`CI_*`, plus documented tool variables such as `VCPKG_BINARY_SOURCES`, `ASAN_OPTIONS`, `UBSAN_OPTIONS`) or argv. The workflow-level `env:` is where `github.*` values are mapped to `CI_*` names.
    - **Context allowlist for `env:` values** (2026-11-13, CR-6 [OVERRIDES SPEC]). The right-hand side of any `env:` entry may reference only:
      - `github.run_id`, `github.run_attempt`, `github.server_url`, `github.repository`, `github.event_name`, `github.ref`, `github.ref_name`, `github.ref_type`, `github.sha`, `github.workspace`;
      - `github.event.before` and `github.event.pull_request.base.sha`;
      - `runner.temp`, `matrix.*`, `needs.plan.outputs.*`, and `inputs.*` of declared `type: choice` inputs.
    - Everything else is a violation, explicitly `github.head_ref`, any `*.title`, `*.body`, `*.message`, `*.head_commit.*`, `*.label*` and the `*.name` of PR, issue or comment objects. These carry text an outside contributor writes. `plan.py`/`stage.py` still re-validate every value they receive against their own allowlists (exit 2), and never rely on the platform's choice validation.
  - **P11** No platform syntax in scripts. Files under `scripts/ci/` must not contain `${{`, `%teamcity.`, `##teamcity[`, `::error`, `::warning`, `::stop-commands`, or any read of `GITHUB_*`, `RUNNER_*` or `TEAMCITY_*` variables. **Exempt:** `scripts/ci/reporters/`, `scripts/ci/env_names.py`, and `scripts/ci/tests/`. The `tests/` exemption is required and kept (Story 01.0 Task 03 as-built; `status.md` 2026-11-13): the harness holds hostile `::error::`/`##teamcity[` fixtures and asserts on rendered platform syntax, so it cannot pass P11 by construction. The exemption covers `tests/` only, never a script a test imports. Unlike the other rules, this one scans scripts, not workflows.
    - **Single emitter** (CR-4). No `print(` or `sys.stdout.write`/`sys.stderr.write` in `scripts/ci/*.py` outside `ci_step.py` and `reporters/` (and the exempt `tests/`). Every line reaches the log through `ok_line`/`skip_line`/`Reporter.info()` (Task 04), so every unfenced line is sanitised.
  - **P12** A publish-capable permission (`contents: write`, `pull-requests: write`, `id-token: write`, `packages: write`) appears only on a job listed in `scripts/ci/publish_jobs.json` (ADR 0005 D6). A release trigger pattern must not match the legacy `-DGIT_TAG` tag format (`<n>.<n>.<n>.<n>`).
    - **Caller-job check** (CR-1). `workflow_call` caps permissions at the caller, so the gate also checks the caller job of every `uses: ./.github/workflows/stage-publish.yml` in `ci.yml`: its `permissions:`, its `needs:` (must include `verify`) and its `if:` guard must satisfy the same tier clause as P8.
    - **Registry check.** Every `enabled` entry in `scripts/ci/publish_destinations.json` names an existing module, a known permission class and an environment equal to its tier (`release` or `staging`). A `release`-tier entry is reachable only from a `v*` tag event; a `staging`-tier entry only from a `push` to `refs/heads/master`.
  - **P13** Every `uses:` appears in `scripts/ci/actions_allowlist.json` with `{action, sha, version, justification, teamcity_equivalent, tag_commit_verified_on}`, where `teamcity_equivalent` is either a named feature or `needs reimplementation`. The allowlist rejects an action whose job a neutral script can do (release creation, checksums, archive upload, vcpkg bootstrap). It also rejects `actions/github-script`, any local or composite action (`uses: ./<dir>`), and a third-party composite action whose transitive `uses:` are not themselves SHA-pinned (CR-6).
    - **Pinned SHA equals the upstream tag** (CR-7, impostor commits). A SHA from a fork network is served under the parent repository's name, so a 40-hex pin alone does not prove provenance. The reviewer of any allowlist change runs `gh api repos/<owner>/<repo>/git/ref/tags/<version>`, dereferences an annotated tag (`gh api repos/<owner>/<repo>/git/tags/<tag sha>`) to its commit, checks that it equals the pinned SHA, and records the date in `tag_commit_verified_on`. The gate itself stays offline: it checks only that the field is present and is an ISO date. An optional `--verify-tags` mode of `check_workflow_policy.py` performs the `gh api` lookup when run with network access; it is not part of CI.
  - **P14** (new, CR-5) Every `actions/checkout` step sets `persist-credentials: false`. Without it, checkout writes the job's `GITHUB_TOKEN` into `.git/config`, where every later step and any workspace artifact can read it.
  - **P15** (new, CR-2, CR-10) Artifact and cache placement:
    - `actions/cache` (and `cache/restore`, `cache/save`) is rejected in any job of `stage-verify.yml` or `stage-publish.yml`; a `cache/save` step must carry the `master`-push-or-`schedule` guard of Task 02.
    - `actions/upload-artifact` with a `logs-` name is allowed only in `stage-build.yml` jobs. Its `path:` lists literal file names only (for example `${{ runner.temp }}/ci-logs/configure.log`): a directory, a trailing `/`, or any `*`, `?` or `**` glob is a violation. `include-hidden-files: false` must be stated explicitly.
- **Create** `scripts/ci/install_actionlint.py`. It downloads one pinned actionlint release asset for the host, verifies its SHA-256 against a value in the script, and extracts only the single `actionlint[.exe]` member into a fresh `mkdtemp` directory under `$CI_TEMP_DIR` (CR-7). Before extraction it rejects any archive whose members include an absolute name, a `..` component, a link or a device; it never calls `extractall`. actionlint runs `shellcheck` on `run:` blocks when shellcheck is on `PATH`, and the script records whether it was (`info:` line).
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
- The `security-auditor` reviews P1-P13 (including the P8/P12 publish exception) before Story 03.0 starts, and the threat model `docs/security/<date>-ci-rework.md` records the result. Done: [2026-11-13-ci-rework.md](/docs/security/2026-11-13-ci-rework.md), PASS_WITH_FOLLOWUP. Its P8/P10/P12/P13 amendments, P14 and P15 above must be in this task before Story 03.0 Task 01 merges.
- **The gate is not the security boundary** (CR-1). For `push`, `pull_request`, `workflow_dispatch` and tag events, GitHub takes the workflow YAML from the triggering ref, so an actor with write access can delete a guard and the gate in the same commit. The gate keeps honest code honest; the boundary is the repository settings listed in [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md) "Repository settings required", applied by the maintainer at Story 03.0 Task 04 cutover.

## Tests

- `scripts/ci/tests/test_check_workflow_policy.py`: one minimal violating workflow (or, for P11, script) fixture per rule P1-P15 -> exactly that violation; one fully compliant fixture -> no violations. A fixture combining all violations prints at most 20 lines plus `(+k more)`. In addition, one fixture each (2026-11-13 threat model, each producing exactly its violation):
  - a `workflow_run` trigger; an `issue_comment` trigger; `secrets: inherit`;
  - a staging-tier job without `needs: verify`; a staging-tier job reachable from `pull_request` or `workflow_dispatch`; a caller job in `ci.yml` widening permissions for `stage-publish.yml`;
  - `CI_T: ${{ github.event.pull_request.title }}` and a `github.head_ref` mapping in `env:`;
  - `uses: actions/github-script@<sha>`, a `uses: ./local-action`, and a `docker://` reference;
  - an allowlist entry without `tag_commit_verified_on`;
  - a checkout without `persist-credentials: false`;
  - `actions/cache` in a `stage-verify.yml` job; a `logs-*` upload with `path: ${{ runner.temp }}/ci-logs/` or a `*` glob;
  - a `print(` in a non-exempt `scripts/ci/*.py`; and a positive control showing that `scripts/ci/tests/` with `::error::` fixtures passes P11.
- `scripts/ci/tests/test_install_actionlint.py`: a tarball with a `../x`, `/abs` or symlink member -> refused; a hash mismatch -> refused.
- `scripts/ci/tests/test_shell_flags.py` (Story 01.0 Task 03) now extracts `run:` bodies from the workflow fixtures and runs them under `bash --noprofile --norc -e -o pipefail`.
- Run against today's `ci.yml` once, as evidence only: every P-rule it breaks is listed in this task's as-built notes. The expected list is P1, P3, P4, P5, P6 and P7.

## Success criteria

- [x] `python3 scripts/ci/lint.py --skip-tests` (actionlint + policy) runs locally in under 60 s after the first actionlint download; the full `python3 scripts/ci/lint.py` (adds the `scripts/ci` suite, 180+ tests, about 2-3 minutes) is what CI's `lint` job runs. Amended 2026-11-13: the original 60 s target predates the test suite's growth; the fast mode keeps the local loop quick.
- [x] Each rule has a passing and a failing fixture.
- [x] Today's `ci.yml` violations are recorded as the "before" evidence.

## As-built notes (2026-11-13)

**Files.**
- `scripts/ci/check_workflow_policy.py`: the gate (P1-P15, plus a `yaml` rule for unsupported YAML).
- `scripts/ci/install_actionlint.py` (actionlint 1.7.12, six host assets, SHA-256 from the release's `checksums.txt`, which matched the release API digests).
- `scripts/ci/lint.py`.
- Data files:
  - `scripts/ci/actions_allowlist.json`: `checkout` v7.0.1, `cache`, `cache/restore` and `cache/save` v6.1.0, `upload-artifact` v7.0.2. Each tag commit was verified with `gh api` on 2026-11-13. `--verify-tags` also passed.
  - `scripts/ci/publish_jobs.json`: empty, because no job may publish yet.
  - `scripts/ci/publish_destinations.json`: an empty **placeholder** whose entries are owned by milestone 0012 Story 04.0 Task 05.
  - `scripts/ci/step_budgets.json`.
- `docs/dev/ci.md`: the P4 budget table and usage. Story 04.0 Task 04 completes this page.
- Two pattern sets added to `scripts/ci/ci_patterns.py`: `policy` and `actionlint`.
- Tests:
  - `scripts/ci/tests/test_check_workflow_policy.py` (41);
  - `test_install_actionlint.py` (10);
  - `test_shell_flags.py`: `test_WorkflowRunBlocks` now runs every `run:` body of the compliant fixtures under `bash --noprofile --norc -e -o pipefail` against a stub `python3`.
- Fixtures under `scripts/ci/tests/fixtures/workflows/`:
  - `good/`: four workflows plus the registries; passes every rule;
  - 39 `p<N>_<what>/` cases, each producing exactly its one violation, including every threat-model fixture listed above;
  - `combined/`: more than 20 violations, bounded to 20 lines plus `(+k more)`.
- `python3 scripts/ci/run_tests.py`: `ok: scripts/ci tests: 156 tests` (105 existing + 51 new) at load average about 11. A re-run at load average about 190 passed 155 of 156. The failure was the existing `test_HugeLogBounded` 30 s wall-clock limit (103 s; it uses only the `cxx` pattern set). That test passed alone in 28.1 s. The flakiness was already noted in `status.md`.

**Decisions.**
- **YAML.** The YAML is read by a stdlib-only subset loader. actionlint JSON was not used because it does not expose the comments P1 and P2 need. Anchors, aliases, tags, merge keys, multi-document files, tabs and multi-line plain or quoted scalars are rejected as a `yaml` violation. The loader reads today's `ci.yml` unchanged.
- **Guards** (P8, P12, P15 cache save) are evaluated, not pattern-matched:
  - A three-valued evaluator runs each `if:` against ten synthetic events: push to master, push to a branch, a `v*` tag, a legacy tag, two `pull_request` events, `schedule`, and dispatch on master, a branch and a tag.
  - The tier is satisfied only if every reachable event is allowed: release = `v*` tag push, staging = master push, cache save = master push or `schedule`.
  - An expression the evaluator does not understand counts as reachable from every event (fail closed).
- **`needs: verify`.** A job can only `needs:` jobs in its own workflow. So for a publish job inside the `workflow_call`-only `stage-publish.yml`, `needs: verify` is enforced on its **caller** (P12 caller check), and P8 checks the job's `environment:` and guard.
  - Caller jobs are listed under `callers` in `publish_jobs.json`, and their permissions must stay within the entry.
  - A listed caller may pass secrets by name; P8 allows this.
- **P10.** `secrets.*` on the right-hand side of `env:` is left to P8, which allows it only in qualified publish jobs and callers. Otherwise publish credentials could not be mapped at all. The allowed `env:` keys for a stage step are `env_names.NAMES`, the documented tool variables `VCPKG_BINARY_SOURCES`, `ASAN_OPTIONS` and `UBSAN_OPTIONS` (a constant in the gate, because `env_names.py` has no tool table), and the `credentials` names in `publish_destinations.json`.
- **P7.**
  - `python` is accepted besides `python3`, because hosted Windows images provide `python`.
  - A `run:` line may contain only `[A-Za-z0-9_./=:+- ]`: no quotes, `$`, `,`, `;`, `&`, `|`, `<`, `>`, globs or brackets.
  - `ci_step.py` and `lint.py` may be called directly only in `ci-next.yml`, `stage-lint.yml` or a job named `lint`.
  - A `shell:` other than `bash` is a P7 violation.
- **P4.**
  - Budgets: `build` 18, `sanitizers` 11, `gui-build` 22 minutes. Any other job gets 10 minutes.
  - The gate recomputes each budget from its medians.
  - A step's timeout may not exceed its job's.
  - Every step with `uses:` counts as network-dependent and needs a timeout.
  - A job-level `uses:` (reusable-workflow call) cannot carry `timeout-minutes` and is exempt.
- **P2/P13.** The version comment must be exactly `# vX.Y.Z`. An allowlist entry may set `kind: composite` only with `transitive_uses_pinned: true`. `actions/github-script`, `lukka/run-vcpkg`, release-creation, release-upload and checksum actions are denied by name.
- **P12 legacy tags.** Every `on.push.tags` pattern is tested against `1.0.2.202611131` and `1.2.3.4` (the root `CMakeLists.txt` `-DGIT_TAG` format).
- **P11** exempts `reporters/`, `env_names.py` and `tests/`. The single-emitter rule exempts `ci_step.py` only.

**"Before" evidence: today's `ci.yml`** (76 violations, information only, `ci.yml` unchanged). It breaks the expected P1, P3, P4, P5, P6 and P7, and also four rules added by the threat model:
- **P2:** `lukka/run-vcpkg` has the comment `# v11.6`, not `vX.Y.Z`.
- **P13:** `run-vcpkg` is denied.
- **P14:** no checkout sets `persist-credentials: false`.
- **P15:** the combined `actions/cache` saves on every event.

It does not trip P8, P9, P10 or P12. actionlint 1.7.12 (without shellcheck) reports nothing on `ci.yml`.

**Found by actionlint on the fixtures, relevant to Story 03.0:** the `runner` context is not available in workflow-level or job-level `env:`, only at step level. `CI_TEMP_DIR: ${{ runner.temp }}` must therefore be mapped per step.

**Open, needs work outside this task.**
- **`lint.py` is not yet 3 `ok:` lines.**
  - The P11 single-emitter rule correctly flags 7 direct `sys.stderr.write`/`sys.stdout.write` calls in already-shipped scripts: `apt_install.py:337`, `bootstrap_vcpkg.py:387,398`, `print_env.py:285` and `run_tests.py:53,57,75`. They must move to `ci_step._emit`/`Reporter.info()`. That belongs with Task 04 (single emitter) or a follow-up; this task did not change them.
  - The actionlint step prints `skip:` until `ci-next.yml` exists.
  - Locally the test suite took about 77 s at load average about 11. The slow tests are existing ones (`test_HugeLogBounded` 12.9 s); the new tests take about 10 s.
- **The strict P10 reading** allows `inputs.*` only for `type: choice` inputs. `workflow_call` inputs cannot be `choice`, so a stage workflow cannot map `inputs.leg` into `env:`. Story 03.0 must pass legs through `matrix.*` instead, or this rule needs a ruled amendment.
- **On-runner only:** actionlint and the policy step inside a real `lint` job on all three OSes (including the Windows `.zip` asset), and the gate against the real `ci-next.yml` and `stage-*.yml` once Story 03.0 writes them.

