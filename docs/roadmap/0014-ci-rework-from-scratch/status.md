# Milestone 0014 - CI Rework From Scratch - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                          | Status         | Tests |
|-------|---------------------------------------------------------------|----------------|-------|
| 01.0  | Report contract, step runner and workflow-script test harness (+ reporter backends, neutral env names - ADR 0005) | ⬜ Not started | -     |
| 02.0  | Pinned toolchain, hardened environment and least privilege (+ policy rules P10-P13, `bootstrap_vcpkg.py` - ADR 0005) | ⬜ Not started | -     |
| 03.0  | Rebuilt jobs with check parity (+ `stage.py`, reusable stage workflows, version check SC3, install-tree export SC2 - ADR 0005) | ⬜ Not started | -     |
| 04.0  | Summaries, triggers and the probe pattern (+ package/verify/publish tier rows - ADR 0005) | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- 2026-11-12 Milestone specified (specification only, no workflow code) on the maintainer's 2026-11-11 request, quoted verbatim in `plan.md`. Inputs: milestone 0011's CI debugging findings and the "CI-rework milestone inputs" recorded in its `status.md`. Open maintainer decisions Q1-Q5 are listed in `plan.md`, and none has been ratified yet.
- 2026-11-12 Maintainer rulings (implement-milestone Phase R): **Q1** push to any branch runs `lint` + the three Linux legs; PR/`master`/nightly/dispatch run the full matrix. **Q2** UBSan made fatal (`-fno-sanitize-recover=undefined`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`); product UB it exposes goes to `cpp-expert` as a separate item. **Q3** upload full configure/build logs as artifacts on failure only (3-day retention); test/live-table logs never uploaded. **Q4** integration/perf run under `sudo` in the non-sanitized Linux leg, `skip:` under sanitizers. **Q5** verified via `gh api`: `master` has no branch protection, so there are no required checks to update at cutover.
- 2026-11-12 R4 decomposition audit: all four stories have a README and one spec per task row; no missing specs. Drift note: CLAUDE.md now says `enable_testing()` is called and `ctest` works (the loop skill's text saying otherwise is stale); `.claude/state/` did not exist before this run.
- 2026-11-12 Story 01.0 Task 01 (partial, baseline measured; inventory cross-check not yet closed). **C05 confirmed**: no `smbios_functional_test` target in any `CMakeLists.txt`. **E11 confirmed**: the sanitizer configure passes `-fsanitize=address,undefined` with no `-fno-sanitize-recover` and no `UBSAN_OPTIONS`. Baseline, green `master` run 43169714193 (total log bytes / lines / duration): sanitizers ubuntu 433751 / 2611 / 332s; sanitizers macos 287263 / 1624 / 287s; build ubuntu 413675 / 2595 / 277s; build macos 241968 / 1603 / 194s; build windows 297879 / 1917 / 531s; gui-build ubuntu-24.04 89454 / 717 / 351s; gui-build macos 53700 / 380 / 292s; gui-build windows 54717 / 383 / 475s. Median duration of the last 5 green runs (s): build macos 230, ubuntu 261, windows 534; sanitizers macos 306, ubuntu 255; gui-build macos 346, ubuntu-24.04 310, windows 654. Not yet split into project-owned vs third-party lines, and no failing run measured.
- 2026-11-12 **PAUSED** on the maintainer's direction: "CI, packaging and publishing should be designed as unified subsystem." No story code written. Milestones 0012 and 0014 are being reconciled into one design before Story 01.0 continues.
- 2026-11-12 **Reconciled by [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md)** (Proposed). The pipeline is now `lint -> build/test -> package -> verify -> publish`. A release is the same pipeline on a `v*` tag, so there is no separate `release.yml`.

  **This milestone owns the shared foundation:**
  - RC1-RC5 with `github`/`teamcity`/`plain` reporter backends;
  - the neutral `CI_*` env-name table;
  - one pin data file;
  - policy gate P1-P13 (P7 tightened to a single stage-command line; P8 publish exception; P9 allowlist `ci.yml` + `stage-*.yml`; new P10-P13);
  - `stage.py`;
  - `check_versions.py`;
  - the reusable `stage-lint.yml`/`stage-build.yml`;
  - new plan section "Pipeline stage contracts" SC1-SC4.

  Milestone 0012 defines `package`/`verify`/`publish` on it.

  **Decisions:**
  - The version source of truth is the root CMake `project()` VERSION, with CI checks against the port, the registry and the tag.
  - The package stage archives the build stage's `cmake --install` tree and never rebuilds.
  - `verify`/`publish` gate on manifest digests.
  - Publish-only `contents: write`/`pull-requests: write`/`id-token: write` sit behind the `release` environment.
  - The maintainer's CI-platform-portability requirement (TeamCity migration) is a first-class driver: YAML is a thin adapter, and stage logic lives in neutral scripts.

  **Changes to existing stories:**
  - 02.0's Non-goal "keep `lukka/run-vcpkg`" is **reversed** (P13 rejects it).
  - No new story was added, and story numbering is unchanged. The additions are scoped into 01.0 Task 02, 02.0 Tasks 02/03, 03.0 Task 01 and 04.0 Task 02.

  **Contradictions found and resolved in the ADR:**
  - 0012's `release.yml` sketch broke P3/P6/P7.
  - 0012 Story 03.0 Task 02 accepted version drift, while Story 04.0 forbade it.
  - C06 tests `-C Debug`, while 0012's shipped configuration was unspecified (SC2 now requires tested == shipped).
  - P8 "no secrets" conflicted with crates.io/PyPI publication.

  **Still open for the maintainer** (ADR "Open questions"): licence, `.pc` relocatability, CLI in the tarball, registry hosting. **Pause remains** until the maintainer accepts or amends ADR 0005.

