# CI developer notes

This page grows with milestone 0014. Story 04.0 Task 04 owns its final shape. So far it holds the stage command, the lint entry point, the workflow policy gate and the time budgets that gate enforces.

## Stage command

```
python3 scripts/ci/stage.py <lint|build|test> [--leg <leg>] [--only <step>]   # python on Windows
```

Every project-owned workflow step is one such line (plan SC1, policy P7); `.github/workflows/ci.yml` calls the reusable `stage-lint.yml`, `stage-build.yml` and `stage-sanitizers.yml`. Locally the same command runs with the plain reporter and temporary directories; the leg defaults to the host OS (`linux`, `macos`, `windows`), and the build stage uses the host cmake/ninja instead of the pinned venv.

| Stage | Steps (`--only`) | Inventory rows |
|-------|------------------|----------------|
| `lint` | `env`, `self-test`, `lint`, `versions`, `cversions`, `doc-samples` | C01, C02, C21, C22 + SC3 |
| `build` | `prepare` (pinned tools, RC4 header, self-test, vcpkg cache key), `compile` (vcpkg bootstrap, configure, cache health, build), `install` (packaging legs only) | C03, C04, C11 |
| `test` | `unit` (C06, C13), `cli-formats` (C07, C08, C09), `live-table` (C10, C14, Q4; best-effort, counts-only) | C06-C10, C13, C14 |

Legs, their preset, build configuration, runner and job, and the trigger tiers live in `scripts/ci/legs.json`. The sanitizer legs `linux-asan`/`macos-asan` (job `sanitizers`, inventory C12-C14) configure into `build/<preset>-asan` with ASan + fatal UBSan (`-fno-sanitize-recover=undefined`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`) and keep the old `ASAN_OPTIONS`.

ctest runs go through `scripts/ci/ctest_report.py` and the named sets in `scripts/ci/test_sets.json` (`unprivileged`, `live_table`, `gui_ci`). The exit code decides; the `ok:` counts come from `ctest --output-junit` (`$CI_TEMP_DIR/junit-<label>.xml`), never from ctest's summary text, and an unreadable JUnit file prints `counts unavailable`. Live-table sets run counts-only and their logs and JUnit files are never uploaded. A leg outside the tier of `CI_EVENT`/`CI_REF` prints one `skip:` line. `--leg`, `--only`, `CI_EVENT` and `CI_REF` are re-validated; an unknown value exits 2 without being echoed. Packaging legs (`linux-x64`, `windows-x64`, `macos-arm64`) are declared but pending until milestone 0012 names the shipped configuration and adds the `libsmbios` install rules.

Version consistency (`lint` step `versions`, plan SC3): `python3 scripts/ci/check_versions.py [--tag <name>]` checks every site in `scripts/ci/version_sites.json` against the root `project()` VERSION; until milestone 0012 adds that VERSION, `SMBIOS_C_VERSION` stands in for it.

## Lint: one command, locally and in CI

```
python3 scripts/ci/lint.py        # python on Windows
```

It runs three steps through the step runner. On success it prints one `ok:` line per step:

1. `scripts/ci/run_tests.py`: every `scripts/ci` test.
2. actionlint 1.7.12, through `scripts/ci/install_actionlint.py --run`. The first run downloads the pinned release asset for the host into `build/ci-tools/`. Its SHA-256 is checked against the value in the script. Only the single `actionlint` member is extracted, into a fresh temporary directory. Later runs re-hash and reuse the cached archive. If `shellcheck` is on `PATH`, actionlint also checks `run:` blocks; the `ok:` line says whether it was found.
3. `scripts/ci/check_workflow_policy.py`: the policy gate below.

The workflows under test are `.github/workflows/ci.yml` (since the Story 03.0 cutover) and the reusable `stage-*.yml` files it calls. While none exists, step 2 prints one `skip:` line.

## Workflow policy gate (P1-P15)

```
python3 scripts/ci/check_workflow_policy.py [WORKFLOW...]
python3 scripts/ci/check_workflow_policy.py --verify-tags   # reviewer only: needs network and gh
```

The rules are specified in [0014 Story 02.0 Task 03](/docs/roadmap/0014-ci-rework-from-scratch/02.0-pinned-toolchain-and-hardened-environment/03-workflow-lint-and-policy-gate.md). Output is one `ok:` line, or at most 20 lines of the form `<file>:<job>/<step>: <rule>: expected <x> -> actual <y>`, plus `(+k more)`. The exit code is 1 on any violation.

The gate reads these data files. A change to any of them is a reviewed one-line diff:

| File | Rule | Holds |
|------|------|-------|
| `scripts/ci/actions_allowlist.json` | P13 | Every allowed `uses:` with SHA, version, justification, TeamCity equivalent and the date its tag commit was verified |
| `scripts/ci/publish_jobs.json` | P8, P12 | The only jobs that may hold a publish permission or read a secret, and the callers of `stage-publish.yml`. Empty until milestone 0012. |
| `scripts/ci/publish_destinations.json` | P12 | The publish destination registry (ADR 0005 D9). A placeholder owned by milestone 0012. |
| `scripts/ci/step_budgets.json` | P4 | Job time budgets (table below) |
| `scripts/ci/pins.json` | P3 | Runner labels and tool versions (Story 02.0 Task 01) |

To change the allowlist, check that the pinned SHA is the upstream tag's commit before you record `tag_commit_verified_on`. Run `gh api repos/<owner>/<repo>/git/ref/tags/<version>`, and dereference an annotated tag with `gh api repos/<owner>/<repo>/git/tags/<sha>`. `--verify-tags` does this lookup for every entry.

The gate keeps honest code honest. It is not the security boundary, because whoever can edit a workflow can also edit the gate in the same commit. The boundary is the repository settings in [ADR 0005 "Repository settings required"](/docs/adr/0005-unified-build-package-publish-pipeline.md#repository-settings-required).

## Time budgets (P4)

Every job has `timeout-minutes`. So does every step that has `run:` or `uses:`. A job's budget is 2x the median duration of its slowest leg, rounded up to whole minutes. The medians are from the Story 01.0 Task 01 baseline: the median of the last 5 green `master` runs of `ci.yml`, recorded in milestone 0014's `status.md` on 2026-11-12. The gate recomputes each budget from its medians, so the table and the data cannot drift apart.

| Job | Median per leg (s) | Budget (min) |
|-----|--------------------|--------------|
| `build` | linux 261, macos 230, windows 534 | 18 |
| `sanitizers` | linux 255, macos 306 | 11 |
| `gui` (baseline: the old `gui-build` job) | linux 310, macos 346, windows 654 | 22 |
| any other job | no baseline | 10 (`default_job_minutes`) |

A step's timeout may not exceed its job's timeout. `step_budgets.json` `steps` sets a tighter per-step budget, keyed `<job>/<step id>`. The values set by Story 03.0 Task 01 are first estimates (no per-step baseline exists yet); Story 03.0 Task 04 replaces them with measured values:

| Step | Budget (min) |
|------|--------------|
| `lint/checkout`, `build/checkout`, `build/upload-logs` | 3 |
| `lint/stage` | 8 |
| `build/prepare`, `build/prepare-win` | 6 |
| `build/cache-restore`, `build/cache-save` | 5 |
| `build/compile`, `build/compile-win` | 15 |
| `build/test`, `build/test-win` | 8 |
| `sanitizers/checkout` | 3 |
| `sanitizers/cache-restore`, `sanitizers/cache-save` | 4 |
| `sanitizers/prepare` | 5 |
| `sanitizers/test` | 6 |
| `sanitizers/compile` | 10 |
| `gui/checkout` | 3 |
| `gui/cache-restore`, `gui/cache-save` | 5 |
| `gui/prepare`, `gui/prepare-win` | 6 |
| `gui/test`, `gui/test-win` | 6 |
| `gui/packages` (C15, Linux apt) | 10 |
| `gui/compile`, `gui/compile-win` | 18 |

A job that calls a reusable workflow (`uses:` at job level) cannot carry `timeout-minutes`, so the budget applies to the jobs inside the reusable workflow instead.

The medians were measured with a warm vcpkg cache. Story 03.0 should check that a cache miss still fits in the budget.
