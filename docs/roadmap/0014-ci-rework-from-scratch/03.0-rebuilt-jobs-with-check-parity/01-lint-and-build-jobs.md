# 01 - `lint` and `build` jobs

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete locally (2026-11-13); runner-only checks pending (first real run, Task 04)
**Depends on:** Story 02.0 Tasks 01-04 (Task 04 and Task 03's 2026-11-13 policy amendments must merge first, per the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md))

## Objective

Create `ci-next.yml` with the `lint` job and the 3-OS `build` job, carrying C01-C11 over (C05 dropped as dead), with every step under RC1.

## Files

- **Create** `.github/workflows/ci-next.yml`:
  - Top level: `name: CI (next)`; the same triggers as `ci.yml` plus `branches-ignore: ['probe/**']`; `permissions: contents: read`.
  - **`lint`** (pinned Linux label, `timeout-minutes` from the budgets table): checkout -> `print_env.py` (RC4) -> `ci_step.py self-test` -> `lint.py` (script tests, actionlint, policy gate on `ci-next.yml`) -> C01 `check_cversions_isolation.py` -> C02 `check_doc_samples.py --check`.
  - **`build`** (`needs: lint`, matrix `linux`/`windows`/`macos` on the Story 02.0 labels, `fail-fast: false`): checkout (`persist-credentials: false`, P14) -> RC4 -> self-test -> pinned tools (`requirements-tools.txt`) -> `actions/cache/restore` (Story 02.0 key scheme, flavor `default`; skipped on a tag-event packaging leg) -> `bootstrap_vcpkg.py` (Story 02.0 Task 02: pinned commit, `HEAD` check, tool SHA-512, `VCPKG_BINARY_SOURCES` starting `clear;`) -> configure (`ci_step`, patterns `cmake,vcpkg`) -> cache-health line -> build (`ci_step`, patterns `cxx`) -> C06 on Windows (Task 02's `ctest_report.py`) -> C09 info line on Linux -> C07/C08 (`cli_format_check.py`) -> C10 on Windows (best-effort, Task 02) -> `actions/cache/save` (only on a `master` push or `schedule`, Story 02.0 Task 02).
  - `lint` checkout also sets `persist-credentials: false`. No job in this file holds a secret or `id-token: write`.
  - The matrix carries `py` (`python3`/`python`), `preset` and `config` (`Debug` on Windows, empty elsewhere). All of them reach scripts through `env:` (P6).
- **Create** `scripts/ci/cli_format_check.py`, which replaces the C07/C08 inline block with identical semantics:
  - find the `smbios` binary under `build/<preset>` (`*/smbios/bin/smbios[.exe]`); none found is a hard failure;
  - Linux only: one `info:` line with the sysfs table modes (C09), as `<name> <mode> <owner>` triples, at most 3;
  - for each of `text xml json yaml csv`: run `examples/scripts/check_structured_output.py --format <f> -- [sudo] <bin> --format <f>` without a shell. Collect its `ok: <fmt>, <N> ...` line, and turn a `0 structures`/`0 rows` result into one `::warning title=empty table::` per format, as today;
  - byte-compare no-flag output with `--format text` in memory, never printed or written to the workspace. A mismatch prints `no-flag output: expected == --format text -> actual differs (<a> vs <b> bytes)`;
  - success: one line, `ok: cli formats: text <n>, xml <n>, json <n>, yaml <n>, csv <n>; no-flag == text`.
  - **Data hygiene** (2026-11-13, threat model CR-4/CR-9): every child run here reads the live table, so each runs through `ci_step` with `hygiene="counts-only"` (Story 02.0 Task 04) and logs under `CI_LOG_DIR/live/`. The child's `ok: <fmt>, <N> ...` line is never relayed: `<N>` is parsed with an anchored regex (shaped like `^ok: (text|xml|json|yaml|csv), (\d+) (structures|rows)\b`, fixed against `check_structured_output.py`'s actual line during implementation) and the line is re-rendered from the parsed integer. An unparseable line is a hard failure with a fixed reason, not an echo.
- **ADR 0005 amendment (2026-11-12). Stage shape:**
  - `ci-next.yml` is the entry workflow. It holds the triggers, the workflow-level `env:` mapping of `github.*` values to the neutral `CI_*` names, and `permissions`. Its jobs call the reusable `stage-lint.yml`/`stage-build.yml` (`on: workflow_call`).
  - Every project-owned step is one `python3 scripts/ci/stage.py <stage> --leg <leg> [--only <step>]` line (plan SC1, policy P7/P10), and the step list above becomes `stage.py`'s internal sequence.
  - **Create** `scripts/ci/stage.py` (`lint`, `build`, `test`), `scripts/ci/legs.json`, `scripts/ci/check_versions.py` + `scripts/ci/version_sites.json` (plan SC3; runs in `lint`), and the call site of `scripts/ci/bootstrap_vcpkg.py` (Story 02.0 Task 02), the only vcpkg source. No marketplace vcpkg action is used.
  - `stage.py` re-validates `--leg`, `--only` and every dispatch input against its own allowlists (`legs.json`, the stage's step names, the declared choice values) and exits 2 on an unknown value. It never relies on the platform's choice validation (CR-6).
  - On packaging legs, `build` also runs `cmake --install` into `$CI_ARTIFACT_DIR/install/<leg>/` and uploads it as a stage artifact (plan SC2). The configuration built is the one 0012 Story 03.0 names as shipped.
  - **Digest hand-off through job outputs** (2026-11-13, CR-8). After `cmake --install`, `build` computes a tree digest of `install/<leg>/`: SHA-256 over the sorted relative POSIX paths, each followed by its file's SHA-256. A symlink, a non-regular file, or any path component named `Testing` is a hard failure (CR-9: the build tree's `Testing/` must never ship). The digest and the uploaded artifact ID are exported as **job outputs** through `Reporter.set_output()` (Story 02.0 Task 04), never as a file inside the artifact. A downstream stage (0012 `package`, then `verify`/`publish`) downloads by artifact ID and compares against `needs.<build job>.outputs`, because a job output can be written only by the job that owns it, while a sibling job holding the runtime token can replace an artifact.
  - **Tag-event packaging legs** build with `VCPKG_BINARY_SOURCES=clear`, restore no cache and save no cache (Story 02.0 Task 02, CR-2), and record `binary_cache: none` for the 0012 manifest.
  - `package`/`verify`/`publish` are not defined here (milestone 0012).
- **Failure-log artifacts** (Q3 ruled yes; amended 2026-11-13 by threat model CR-10). A failing configure/build step's log goes to `actions/upload-artifact` (SHA-pinned, `if: failure()`, `retention-days: 3`, name `logs-<job>-<leg>`, a prefix no stage hand-off uses). The `with: path:` lists **literal file paths only**, for example `${{ runner.temp }}/ci-logs/configure.log` and `${{ runner.temp }}/ci-logs/build.log`, never a directory, the workspace, or a glob (`*`, `?`, `**`). `include-hidden-files: false` and `if-no-files-found: ignore` are stated explicitly. A log above 50 MiB is not uploaded; `stage.py` prints one `info:` line instead. Live-table and test logs are never uploaded; they live under `CI_LOG_DIR/live/` and other names no path lists. Policy rule P15 enforces the path shape.

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
  - stub output containing `::error::` is never echoed;
  - stub CLI stdout containing the fake serial `TEST-SERIAL-0000`, and a stub `ok:` line carrying `##teamcity[` after the count, are never echoed (the count is re-rendered);
  - every live-table child is run with `hygiene="counts-only"`.
- `scripts/ci/tests/test_stage.py` (tree digest and input validation): a file added, changed or symlinked under `install/<leg>/` changes or fails the digest; a `Testing/` path fails; `--leg 'linux\nx=y'` and an unknown `--only` exit 2.
- `scripts/ci/tests/test_bootstrap_vcpkg.py` (Story 02.0 Task 02) covers `binary_sources()` for the tag-event packaging leg.
- Push the branch. `ci-next` `lint` + `build` are green on all three legs, side by side with the old `ci.yml`.

## Success criteria

- [ ] `lint` finishes in under 3 minutes and blocks the other jobs when it fails.
- [ ] On a green run, every project-owned step in `build` prints exactly one line (`ok:`/`info:`/`skip:`), plus the RC4 line.
- [ ] C01, C02, C04, C06-C11 are present, with the gates the inventory names.

## As-built notes (2026-11-13)

**Files.** Created: `.github/workflows/ci-next.yml`, `.github/workflows/stage-lint.yml`, `.github/workflows/stage-build.yml`, `scripts/ci/stage.py`, `scripts/ci/legs.json`, `scripts/ci/cli_format_check.py`, `scripts/ci/check_versions.py`, `scripts/ci/version_sites.json`; tests `scripts/ci/tests/test_stage.py` (47), `test_cli_format_check.py` (21), `test_check_versions.py` (16). Modified: `scripts/ci/reporters/{__init__,github,plain,teamcity}.py` (new `warning(title, text)` event; GitHub output/summary fallback, below), `scripts/ci/lint.py` (gates `ci-next.yml` + every `stage-*.yml`), `scripts/ci/step_budgets.json` (per-step budgets), `docs/dev/ci.md`. `ci.yml` and all product C++/CMake files are unchanged.

**Local results.** `check_workflow_policy.py .github/workflows/ci-next.yml .github/workflows/stage-*.yml`: `ok` (3 workflows, P1-P15, scripts/ci scanned). actionlint 1.7.12 via `lint.py`: `ok`, 3 workflows. `stage.py test --leg macos` against an existing local `build/macos` tree (real CLI, real checker): `ok: cli formats: text non-empty, xml 26, json 26, yaml 26, csv 239; no-flag == text`; the C06 and C10 ctest invocations, forced on the same tree, passed. `stage.py lint --only versions|cversions|doc-samples|env|self-test` each printed one line. `run_tests.py`: `ok: scripts/ci tests: 264 tests` (180 existing + 84 new). Full `lint.py` exited 1 twice. Both times the only failure was the pre-existing `test_ci_step.test_HugeLogBounded` 30-second timing assertion (92.6 s and 77.8 s) at load averages of 55-264 from unrelated local linker jobs; the test passes alone in 20 s and passed inside the green `run_tests.py` run. actionlint and the policy gate were `ok` in both runs. The assertion was not weakened. Rerun `python3 scripts/ci/lint.py` on an idle machine before ticking this task.

**Decisions and deviations.**
- **Matrix carries only `leg`.** P10 lets a stage step receive only neutral `CI_*` (or documented tool) names through `env:`, so `preset`/`config`/`sudo` come from `legs.json` by `CI_LEG`, and the runner label from `pins.json` through `legs.json` `runner`. `py` cannot reach a literal `run:` line (P6/P7), so each stage step exists twice, `python3 ...` (`if: matrix.leg != 'windows'`) and `python ...` (Windows). `test_stage.py` checks the `runs-on` labels against `pins.json`.
- **Neutral env mapping lives in each stage workflow**, not in `ci-next.yml`: a caller's `env:` does not reach a called workflow. `ci-next.yml` holds triggers, permissions and job order only.
- **Tiering (Q1).** The `build` matrix is `fromJSON(<branch push, not master> && '["linux"]' || '["linux", "windows", "macos"]')`; `legs.json` `tiers` holds the same rule and `stage.py` self-skips a leg outside it. `test_stage.py` evaluates the workflow expression (with the policy gate's evaluator) for every gate scenario and compares it with `stage.tier_legs()`. Every pull request runs the full matrix (no neutral name carries the PR base ref, and `ci.yml` triggers on PRs to any base).
- **Triggers.** `push` with `branches-ignore: ['probe/**']` **and** `tags: ['v*']` (with `branches-ignore` alone GitHub would stop running tag pushes at all; `v*` never matches the legacy `-DGIT_TAG` tags, P12), `pull_request`, nightly `schedule`. No `workflow_dispatch`.
- **Cache output.** `prepare` exports `cache` (`{"key","restore_prefix"}`) and a separate boolean `cache_enabled` (false on a tag-event packaging leg). The guards compare `cache_enabled`, because the policy gate cannot evaluate `fromJSON(...).key` and fails closed (P15).
- **RC4 order.** In `build`, the pinned tools are installed into `$CI_TEMP_DIR/ci-tools` (venv, `--require-hashes --only-binary :all:`) first and silently, so the RC4 header shows the pinned cmake/ninja; the printed order is still RC4 header, self-test, `ok: pinned tools`. Later workflow steps put the venv first on `PATH` (a real directory only, never a symlink). Locally the tools step prints `skip:`.
- **C06/C10** run through `ci_step` directly with the old regexes copied verbatim into `stage.py` (C06 hard with `--output-on-failure`; C10 best-effort, `counts-only`, no `--output-on-failure`). Task 02 moves them onto `ctest_report.py`/`test_sets.json`.
- **`cli_format_check.py`.** The checker prints `ok: text, non-empty` (no count), so the success line says `text non-empty`, and the `text` empty-table warning fires when every counted format is empty (five warnings in total). The C08 compare runs in a `--compare-child` process (under `ci_step`, `counts-only`) that holds both outputs in memory and prints only their sizes. A `FAIL:` reason is shown only if it matches one of the checker's own fixed templates. Linux uses `sudo -n` (never prompts; `ci_step` closes stdin); local runs never use `sudo`.
- **GitHub job outputs and summary.** `GITHUB_OUTPUT`/`GITHUB_STEP_SUMMARY`/`ImageOS`/`ImageVersion` are runner environment variables, not expression contexts, and P7 bans `$VAR` in a `run:` line, so the YAML adapter cannot map them onto `CI_OUTPUT_FILE`/`CI_SUMMARY_FILE`/`CI_IMAGE`. The `github` reporter (the platform-specific piece, exempt from P11) now falls back to the runner's two file variables when the neutral name is empty; children still never inherit either (CR-5). `CI_IMAGE` stays unmapped, so RC4 prints `image=-` on GitHub (maintainer decision, below).
- **SC2 install-tree export: stage-side only.** `build --only install` runs `cmake --install` into `$CI_ARTIFACT_DIR/install/<leg>/`, refuses symlinks, non-regular files, any `Testing` component and an empty tree, and exports `install_digest` (`{leg, sha256, files}`) through `Reporter.set_output()`; tag-event packaging legs export `binary_cache: none`. Unit-tested with a stub `cmake`. The packaging legs `linux-x64`/`windows-x64`/`macos-arm64` are declared in `legs.json` as `pending` (exit 2) because **milestone 0012** has not named the shipped configuration (Story 03.0) and `libsmbios` has no `install()` rules (Story 01.0). No packaging leg is in the workflow yet, so the YAML artifact upload and the `artifact-id`/digest job-output plumbing are deferred to 0012 with the first packaging leg.
- **`check_versions.py`.** Extractors `cmake_project_version`, `cmake_set:<VAR>`, `json_key:<path>`; comments are stripped first. Today `SMBIOS_C_VERSION` is the interim authority (`ok: versions: 1.0.0 (SMBIOS_C_VERSION, interim), 1 sites`). Once a root `project()` VERSION exists, a `derived` site must reference `${PROJECT_VERSION}`; a hand-edited literal fails.
- **Failure-log upload** is named `logs-build-<leg>` and runs only when a compile step failed. A log above 50 MiB is renamed `<name>.log.over-cap` (the literal path then does not exist, `if-no-files-found: ignore`) with one `info:` line.
- **Step budgets** in `step_budgets.json` are first estimates (no per-step baseline); Task 04 replaces them with measured values.

**Pending (runner only; nothing was pushed or dispatched).** `ci-next` `lint` + `build` green on all three legs beside `ci.yml`; the one-line-per-step output on a green run; `windows-2022` having `python` but no `python3`; the pip CMake finding VS 2022; `sudo -n` on `ubuntu-24.04`; the `GITHUB_OUTPUT` fallback, `fromJSON` step outputs and cache restore/save keys; the `runs-on` expression; the failure-log artifact containing exactly `configure.log`/`build.log` (Task 04 negative proof 1); a cold-cache build fitting the 18-minute `build` budget (the new key scheme makes the first run cold on every leg); `lint` under 3 minutes (the full `lint.py` now runs 264 tests: 2-3 minutes on an idle machine, 6 minutes under the heavy local load of this run).

**Open for the maintainer.**
1. `CI_IMAGE` cannot be set by the GitHub adapter under P7/P10 (and `CI_OUTPUT_FILE`/`CI_SUMMARY_FILE` only through the reporter fallback above). Accept the reporter fallback and add an `image` fallback in the same place, or relax P10 for a documented runner variable?
2. The spec's matrix `py`/`preset`/`config` "through `env:`" conflicts with P10's neutral-names-only rule; resolved here via `legs.json` (ratify).
3. The "lint under 3 minutes" criterion versus the growing `scripts/ci` suite (264 tests).
