# 0005 - Unified build, package and publish pipeline

> **Status:** Proposed
>
> **Date:** 2026-11-12
>
> **Supersedes:** _(none)_. Amends the delivery mechanics of [ADR 0004](0004-package-manager-distribution.md); its packaging decisions are unchanged.
>
> **Superseded by:** _(none)_
>
> **Amended:** 2026-11-13 by the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) (CR-1, CR-2, CR-3): D6/D9 staging tier reconciled with P8; S3 confinement made canonical-path based and create-only; P8/P10-P13 amended and P14/P15 added; new section [Repository settings required](#repository-settings-required).

## Context

Three milestones planned CI work separately, and none of it has been implemented yet:

- [Milestone 0014](/docs/roadmap/0014-ci-rework-from-scratch/plan.md) rebuilds `ci.yml` around a report contract (RC1-RC5), one step runner (`scripts/ci/ci_step.py`), pinned runners and tools, and a policy gate. Its Non-goals leave the release workflow and the binding jobs out, and say they must "adopt RC1-RC5 when they are built".
- [Milestone 0012](/docs/roadmap/0012-package-manager-distribution/plan.md) adds a separate `release.yml` (Story 03.0). After the [packaging-automation review](/docs/reviews/milestone-0012-packaging-automation-2026-11-12.md) it also adds Story 04.0: version consistency, a gated draft release, registry update and post-publish verification. That design has its own job graph, its own scripts directory (`scripts/release/`), its own permission rules and its own runner choices.
- [Milestone 0013](/docs/roadmap/0013-language-bindings/plan.md) adds binding CI jobs to `ci.yml` and plans crates.io/PyPI publication.

Read together, these plans conflict:

1. **Two workflow designs.** 0012 Story 03.0 Task 01's illustrative `release.yml` uses `ubuntu-latest`/`macos-latest`, `${{ }}` expressions inside `run:` bodies, and multi-line shell. Those break 0014 policy rules P3, P6 and P7 before a line of it is written.
2. **Five version values, none of them authoritative** (review M1): the tag, `SMBIOS_C_VERSION` (`smbios_c/CMakeLists.txt`), the planned `LIBSMBIOS_VERSION`, the port's `vcpkg.json` `"version"`, and the registry baseline. 0012 Story 03.0 Task 02 states that drift between them is acceptable, while 0012 Story 04.0 Task 01 states it must fail CI.
3. **What gets tested is not what gets shipped.** `release.yml` would rebuild from scratch, separately from the `ci.yml` run that tested the commit. Nothing ties the two builds together, and Story 04.0 has to bolt on a "verify the tagged commit has a green `ci.yml` run" check to make up for it.
4. **The build configuration is unspecified.** 0014's Windows test step runs `ctest -C Debug` (inventory C06), and 0012 never says which configuration it ships. The two milestones could test one configuration and ship another.
5. **Security rules conflict.** 0014 P8 bans every `secrets.` reference. crates.io/PyPI publication (0013) needs either a credential or OIDC trusted publishing (`id-token: write`).

The maintainer ruled on 2026-11-12: "CI, packaging and publishing should be designed as unified subsystem." A second requirement followed on the same day: the actions performed inside pipelines must be separated from the pipeline configuration, so that migrating from GitHub Actions to JetBrains TeamCity (or any other CI system) is simple.

### Decision drivers

- **DD1 - Tested equals shipped.** The bytes a consumer downloads must be the bytes the pipeline built and tested, and then verified as installed artifacts. A green CI run on the same commit is not enough on its own.
- **DD2 - One contract for every job.** The RC1-RC5 report contract, the step runner, the pins and the policy gate apply to every stage, including release and the 0013 binding jobs. There is no second dialect for release jobs.
- **DD3 - One authoritative version.** Exactly one value is edited by hand. Every other version site is derived from it or checked against it, and a mismatch fails CI before anything is built.
- **DD4 - Least privilege by stage.** Write permissions and credentials exist only in publish jobs, and only on events that can publish.
- **DD5 - CI-platform portability.** Pipeline actions live in platform-neutral, in-repo commands that run identically on a developer machine. The CI system's configuration is a thin adapter around them. Moving to TeamCity (or elsewhere) should touch only the adapter, the reporter backend and some glue.
- **DD6 - Run economy.** The maintainer's Q1 ruling on trigger tiering (0014 `status.md`) stands. Packaging and publishing must not add a per-push cost.

## Decision

### D1 - One pipeline with five ordered stages

Every CI activity is a stage of one pipeline, in this order:

```text
lint -> build/test -> package -> verify -> publish
```

| Stage | Input | Output | Runs on |
|-------|-------|--------|---------|
| `lint` | Source tree | RC3 records only | Every event (always first) |
| `build/test` | Source tree | Test results. On a packaging leg it also writes an installed tree (`cmake --install`) to `$CI_ARTIFACT_DIR/install/<leg>/`, exported as a stage artifact | Per the tier table (D7) |
| `package` | The build stage's installed tree, **not a rebuild** | Archives, `.sha256` files and `manifest.json` (D4) | Full-tier events (D7) |
| `verify` | The package stage's exact archives, matched by digest | Pass/fail of the clean-room consumer tests | Full-tier events (D7) |
| `publish` | The verified archives plus the manifest | GitHub Release, registry PR, and later crates.io/PyPI | Release tags only (D7) |

- **Stage ordering is structural.** On GitHub this is `needs:`. A stage cannot start unless the previous stage succeeded in the **same run**, which makes 0012 Story 04.0 Task 02's "verify the tagged commit has a green `ci.yml` run" check unnecessary.
- **Sanitizer and GUI legs never feed `package`.** They stay `build/test`-only legs.
- **Packaging legs are named `<os>-<arch>`.** For example: `linux-x64`, `windows-x64`, `macos-arm64`. This settles review M6's arch question by naming the leg. The glibc baseline is recorded in the manifest.
- **Packaging legs build in the shipped configuration, and tests run against that same build.** Which configuration ships (`Release` or `RelWithDebInfo`) is decided by 0012 Story 03.0. Whatever it is, the packaging leg builds it once, tests it, installs it and hands it on. This resolves contradiction 4 above.

### D2 - One step runner, report contract and policy gate for every stage

- **Every project-owned step in every stage is one invocation of a stage command (D8).** Stage commands run their steps through the `ci_step` library (0014 RC2), print RC1 output and append RC3 records. This covers package, verify, publish and the 0013 binding jobs.
- **The policy gate (0014 Story 02.0 Task 03) checks every workflow file in the family.** No file is exempt, including publish jobs. The P8 exception for publish credentials is defined in D6.

### D3 - One pinned toolchain

The runner labels and the CMake/CTest/Ninja/vcpkg versions pinned by 0014 Story 02.0 Task 01 come from **one data file**. Every stage reads that file, including package, verify, publish and binding jobs. No stage declares its own runner label or tool version. The verify stage may choose a *clean* runner (an empty workspace, no vcpkg, no checkout) with the same pinned label. It still never chooses a different label.

### D4 - Artifact layout and manifest (platform-neutral contract)

```text
$CI_ARTIFACT_DIR/
  install/<leg>/                         # build stage: cmake --install output, untouched
  dist/libbios-<version>-<leg>.<ext>     # package stage: .tar.gz (posix) / .zip (windows)
  dist/libbios-<version>-<leg>.<ext>.sha256
  dist/manifest.json                     # {version, commit, legs:[{leg, file, sha256, bytes, config, glibc|macos_min|msvc}]}
```

- **The package stage only archives an existing tree.** It writes files in a deterministic order with normalised permissions, computes SHA-256 with a stdlib-Python script (review L1: no `shasum`), and writes the manifest.
- **`verify` and `publish` refuse any file whose digest differs from `manifest.json`.** This is how DD1 is enforced mechanically.
- **`<version>` is the D5 version.** It is not the raw tag. This supersedes 0012 Story 03.0 Task 02's `libbios-<tag>-<preset>` naming.

### D5 - Single version source of truth: an in-tree CMake value, with a CI consistency check

**Authoritative:** the root `CMakeLists.txt` `project(... VERSION X.Y.Z)` value.

- **Derived** at configure time, never hand-edited: `SMBIOS_C_VERSION`, `LIBSMBIOS_VERSION` and the generated `smbios_c.pc` / `libsmbiosConfigVersion.cmake`.
- **Checked** against the authoritative value: the port's `ports/smbios-c/vcpkg.json` `"version"`, the registry's `versions/baseline.json` entry, and, on tag events only, the tag. The tag must equal `v<X.Y.Z>` (optionally `-<prerelease>`).
- **Where the check lives:** `scripts/ci/check_versions.py` reads a data file (`scripts/ci/version_sites.json`) that lists every version site and its rule (`derived` or `equal`). It runs in `lint` on every event, plus in the tag check of the publish stage. Mismatches are printed in RC1 discrepancy form (`<site>: expected <x> -> actual <y>`). 0014 ships the script with the sites that exist today, and 0012 adds a row per new site.
- **Out of scope for this value:** binding packages (0013) keep their own versions, by 0013's own Key constraints. Their manifests are checked only for the `smbios_c` version range they declare.
- **The legacy tag scheme never triggers a release.** The `-DGIT_TAG=ON` scheme (`<n>.<n>.<n>.<n>` tags) never matches the `v*` release trigger. The policy gate forbids a release trigger pattern that could match it.

**Why the CMake value and not the tag** (the review's suggestion): a vcpkg port build (`vcpkg_from_git` at a pinned `REF`) and a source-tarball build have no tag metadata at configure time, so a tag-derived version is empty exactly where packaging needs it. The tag is a *release request* that must agree with the tree. It is not the source of the number.

### D6 - Publish gating and least privilege

- **Workflow-level permissions are `contents: read`** (0014 P1, unchanged). Only publish-stage jobs widen them, each with a `# permissions:` reason comment:

  | Publish job | Permissions |
  |-------------|-------------|
  | `release-draft` | `contents: write` |
  | `release-promote` | `contents: write` |
  | `registry-pr` | `contents: write` + `pull-requests: write` |
  | Binding publish (later) | `id-token: write` for OIDC trusted publishing where the registry supports it |

- **Publish to a `release`-tier destination runs only on a release-tag event, and only after `verify` succeeded in the same run.** For bindings, publish runs only on that binding's own publish trigger (the trigger is 0013's decision) and only after that binding's verify job succeeded in the same run.
- **Publish to a `staging`-tier destination (D9) runs only on a `push` to `refs/heads/master`, and only after `verify` succeeded in the same run** (amended 2026-11-13; the earlier text also allowed dispatch, which contradicted both this bullet's own "dispatch cannot reach publish" rule and P8).
- **Dispatch, PR (same-repo or fork), non-`master` branch push and nightly events cannot reach any publish job, for either tier.** The policy gate checks this per tier (P8, P12): a publish-stage job must match exactly one tier clause - `environment: release` + `v*` tag guard, or `environment: staging` + `master`-push guard - and `needs:` the same run's `verify`. Anything else is a violation.
- **GitHub Release sequence:**
  1. `release-draft` creates the draft once (fixing the review M2 race) and uploads the manifest's files.
  2. It re-reads the draft's asset digests and compares them with `manifest.json`.
  3. `release-promote` publishes the draft.
- **Post-publish check.** A `post-publish` check then downloads the public assets by URL and re-checks their digests (0012 Story 04.0 Task 04). Its failure is a hard, loud failure that names the release. It is not a rollback.
- **Registry PR timing.** A PR opened with `GITHUB_TOKEN` does not trigger workflows. The registry validation (filesystem-registry consumer test, review M3/H2) therefore runs **inside the publish run, before** the PR is opened. Merging is reviewed by a human.
- **Credentials.** P8 (no `secrets.`) stays for every non-publish job. In a publish job, a credential is allowed only through the GitHub *environment* of its tier (`release`, with required reviewers; or `staging`, deployable from `master` only), and the job is listed in the policy gate's allowlist. OIDC trusted publishing is preferred over a stored token wherever the registry supports it. Within the publish job, child processes receive a credential only when the step passes it by name (0014 Story 02.0 Task 04).
- **The policy gate is not the boundary.** For `push`, `pull_request`, `workflow_dispatch` and tag events, GitHub runs the YAML of the triggering ref, so the guards above are enforced by code an actor with write access controls. The boundary is the repository and environment settings in [Repository settings required](#repository-settings-required). In the publish stage (0012), before any write, a defence-in-depth check confirms that the tagged commit is an ancestor of `origin/master`.

### D7 - Trigger tiering (extends the Q1 ruling)

| Event | lint | build/test | package | verify | publish |
|-------|------|------------|---------|--------|---------|
| Push, docs-only changes | yes | - | - | - | - |
| Push to a non-`master` branch | yes | Linux legs | - | - | - |
| Same-repo PR to `master` | yes | Legs not run by the push tier | yes (all packaging legs) | yes | - |
| Fork PR, nightly, dispatch `all` | yes | full matrix | yes | yes | - |
| Push to `master` | yes | full matrix | yes | yes | `staging` tier only (D9) |
| Release tag `v*` | yes | full matrix | yes | yes | `release` tier |

Notes:

- **Packaging breakage shows up before release day.** It surfaces on every PR to `master` and nightly, and a tag only adds `publish`.
- **On a same-repo PR**, `package` needs all packaging legs. The PR run therefore builds the Linux packaging leg again rather than depending on another run's artifacts. This is a deliberate cost (one Linux leg per PR) traded for DD1.
- **Concurrency.** Tag runs and `master` runs are never cancelled.

### D8 - Workflow family shape

The GitHub adapter consists of the following files, which are the only workflow files the P9 allowlist permits:

- **Entry workflow: `ci.yml`.** It owns triggers, the `plan` (tiering) job, concurrency and the run-level report.
- **Reusable stage workflows: `stage-lint.yml`, `stage-build.yml`, `stage-package.yml`, `stage-verify.yml`, `stage-publish.yml`** (`on: workflow_call`). Each holds jobs and legs only, and each of their `run:` steps is one stage-command invocation (see Platform portability).
- **Probe workflows** (0014 Story 04.0 Task 03) and single-stage dispatch call the same reusable stage workflows.
- **There is no separate `release.yml`.** A release is the same pipeline on a tag event.

### D9 - Publish destinations are plug-ins (adding one needs no workflow redesign)

GitHub Releases, the vcpkg registry PR and later crates.io/PyPI are not special cases in the publish stage. Each is a **destination**: a small neutral module behind one interface, listed in one data file. A new destination (the worked example is an **S3 staging area**) is added by writing a module, one registry entry, tests and a doc line, and nothing in the stage logic changes.

- **Interface** (`scripts/ci/destinations/<name>.py`, Python stdlib only; the contract is 0014 SC5):

  | Member | Meaning |
  |--------|---------|
  | `NAME` | Registry key, e.g. `github-release`, `s3-staging` |
  | `REQUIRED_ENV` | Neutral variable names the destination reads (`CI_PUBLISH_*`). The adapter maps credentials onto them. |
  | `validate(manifest, env)` | Checks inputs and prints nothing secret. Runs in `--dry-run` and before any write. |
  | `publish(manifest, artifact_dir, env, dry_run)` | Uploads exactly the files `manifest.json` lists, and returns a result record (RC3) with the remote locations. |
  | `check_published(manifest, env)` | Re-reads the remote copy and compares digests with the manifest. This is the post-publish check, per destination. |

  A destination never builds, re-archives or recomputes digests. The manifest is its only source of truth (D4).
- **Registry** (`scripts/ci/publish_destinations.json`): one entry per destination with `name`, `enabled`, `tier` (`release` or `staging`), `permission_class`, `environment`, and the credential names it needs. `plan.py` turns the enabled entries for the current event into the publish matrix, so the adapter holds **no per-destination job**.
- **Tiers** (amended 2026-11-13, threat model CR-1). `release` destinations run only on a release tag (D6). `staging` destinations run only on a `push` to `refs/heads/master`, **only after `verify` passed in the same run**, through the `staging` environment, and never write to a `release` location. They never run on pull requests, forks, tags, nightly or dispatch. Manual dispatch was removed from the staging tier: it made the tier contradict P8 and D6's "dispatch cannot reach publish", and dispatch can be run on any branch's YAML. Re-adding it is open question 6. A staging copy is disposable and is not a release.
- **Binary cache** (CR-2). A `release`-tier destination refuses any manifest leg whose `binary_cache` is `restored`; tag-event packaging legs build with `VCPKG_BINARY_SOURCES=clear` (0014 Story 02.0 Task 02). A `staging`-tier destination accepts `restored`, because the copy is disposable.
- **Permission classes.** GitHub fixes `permissions:` per job, so destinations are grouped: `repo-write` (`contents: write`, `pull-requests: write`), `oidc` (`id-token: write`, used by S3 via an assumed role, and by PyPI/crates.io trusted publishing), `token-only` (no workflow permission, a credential from the environment). `stage-publish.yml` has one matrix job per class. Adding a destination to an existing class needs **no workflow edit**. A new class is an adapter change, reviewed through the policy gate (P12 reads `publish_destinations.json` and `publish_jobs.json` together).
- **Credentials.** Always through a GitHub environment named by the entry (`release` for release tier, `staging` for staging tier), never from the repository or a PR. OIDC role assumption is preferred over stored keys.
- **Worked example, S3 staging** (full walkthrough in [docs/dev/adding-a-publish-destination.md](/docs/dev/adding-a-publish-destination.md); amended 2026-11-13, threat model CR-3). `s3_staging.py` uploads the manifest's files to `s3://<bucket>/staging/<version>/<run-id>-<attempt>/<file>` with `aws s3api put-object` (called through `ci_step`, no shell), recording each object's sha256 as metadata, and `check_published` reads the objects back and compares. The bucket, prefix and role come from `CI_PUBLISH_S3_BUCKET`, `CI_PUBLISH_S3_PREFIX` and `CI_PUBLISH_S3_ROLE`; the run id and attempt from `CI_RUN_ID` and `CI_RUN_ATTEMPT`. The `aws` CLI is a documented tool requirement in the pins file (D3), which makes it a TeamCity agent requirement on migration; its version is printed in RC4 and a mismatch with the pin fails.
  - **Confinement is by canonical path, not by string prefix.** `validate` builds every key from validated parts only and checks the result, before any write:
    - the prefix is exactly `staging` or starts with `staging/`; every `/`-separated component matches `^[A-Za-z0-9._-]+$` and is neither `.` nor `..` (so `staging-release`, `stagingX`, `staging/../release` and `a/../../b` are all rejected);
    - `<version>` matches `^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$`; `<run-id>` and `<attempt>` are decimal digits;
    - `<file>` is a manifest entry whose name matches the D4 basename regex `^libbios-<version>-<leg>\.(tar\.gz|zip)(\.sha256)?$` (or `manifest.json`), contains no separator, and resolves (after `realpath`) to a regular, non-symlink file directly under `$CI_ARTIFACT_DIR/dist/`;
    - the assembled key equals its own `posixpath.normpath` and starts with `staging/`.
  - **Create-only, no overwrite.** Every upload is a conditional write (`put-object --if-none-match '*'`). An existing key, including one from an earlier attempt of the same run, makes S3 return `PreconditionFailed`, which is a hard failure, never a retry-with-overwrite. The attempt number in the key lets a legitimate re-run write a fresh location. The destination never issues a delete.
  - **The client-side check is defence in depth.** The boundary is the cloud-side trust and role policy in [Repository settings required](#repository-settings-required): a write outside `staging/*`, a delete, or an assumption from another repository or environment must fail at AWS even if `s3_staging.py` were edited.
  - The log shows counts and the base location only, never the role ARN, a bucket policy, a presigned URL or a credential.
- **Portability.** A destination reads only neutral names and calls only argv commands, so it moves to TeamCity unchanged. Only the credential mapping and the permission-class job layout live in the adapter.

## Platform portability

This section implements DD5.

### Three layers

| Layer | What it holds | GitHub Actions today | TeamCity tomorrow | Changes on migration? |
|-------|---------------|----------------------|-------------------|-----------------------|
| **(a) Pipeline definition (adapter)** | Triggers, matrix/agents, ordering, caching, artifact hand-off, secrets/permissions, and one stage-command call per step. Nothing else. | `.github/workflows/ci.yml` + `stage-*.yml` | Kotlin DSL versioned settings (`.teamcity/settings.kts`): build configurations, snapshot/artifact dependencies, triggers | **Rewritten** |
| **(b) Stage logic** | Platform-neutral entrypoints in `scripts/ci/` (stdlib Python): `python3 scripts/ci/stage.py <lint\|build\|test\|package\|verify\|publish> [--leg <leg>] [--only <step>]`. They run identically on a developer machine. | Same | Same | **Unchanged** |
| **(c) Contracts** | RC1 grammar, RC3 records, RC4 header, RC5 semantics, exit codes, D4 artifact layout + manifest, D5 version sites, neutral env names (below) | Same | Same | **Unchanged** |

Between (a) and (b) sit two small, swappable pieces:

- **Reporter backends** (`scripts/ci/reporters/{github,teamcity,plain}.py`, selected by `CI_REPORTER`). This is the only code that emits platform workflow commands or service messages, or writes a platform summary file.
- **Adapter glue.** On GitHub this is the `env:` mapping in the workflow files. On TeamCity it is the parameter mapping in the Kotlin DSL.

**Stage-command exit codes:**

| Code | Meaning |
|------|---------|
| 0 | Success (best-effort warnings included) |
| 1 | A gate failed |
| 2 | Usage or environment error (missing neutral variable, bad leg name) |
| 128+n | Killed by signal n (0014 RC2) |

### Neutral environment names

Scripts read **only** these names, never `GITHUB_*`, `RUNNER_*` or `teamcity.*`. The adapter sets them. Running locally, `stage.py` fills defaults (`CI_PLATFORM=local`, `CI_REPORTER=plain`, temp directories).

| Neutral name | Meaning | GitHub Actions source | TeamCity source |
|--------------|---------|-----------------------|-----------------|
| `CI_PLATFORM` | `github` / `teamcity` / `local` | literal in workflow `env:` | literal parameter |
| `CI_REPORTER` | Reporter backend | literal `github` | literal `teamcity` |
| `CI_RUN_ID` | Unique run id | `github.run_id` | `%teamcity.build.id%` |
| `CI_RUN_ATTEMPT` | Attempt number of a re-run (added 2026-11-13; S3 staging keys) | `github.run_attempt` | literal `1` (`teamcity.build.id` is already unique per attempt) |
| `CI_OUTPUT_FILE` | Neutral job-output channel, written only through `Reporter.set_output()` (added 2026-11-13; stage digests, plan matrix) | `GITHUB_OUTPUT`, given to the `stage.py` process only, never inherited by its children | empty; the reporter emits `##teamcity[setParameter ...]` |
| `CI_RUN_URL` | Link to the run | `github.server_url`/`github.repository`/`actions/runs/`/`github.run_id` | `%teamcity.serverUrl%/viewLog.html?buildId=%teamcity.build.id%` |
| `CI_EVENT` | `push` / `pr` / `schedule` / `manual` / `tag` | mapped from `github.event_name` + `github.ref_type` | set per trigger in the DSL (VCS / schedule / manual; tag branch filter) |
| `CI_REF` / `CI_REF_NAME` | Full ref / short name | `github.ref` / `github.ref_name` | `%teamcity.build.vcs.branch.<root>%` / `%teamcity.build.branch%` |
| `CI_COMMIT` | Commit under test | `github.sha` | `%build.vcs.number%` |
| `CI_BASE_COMMIT` | Diff base for tiering (empty means full) | PR base sha / `github.event.before` | last-built revision, or empty |
| `CI_JOB` / `CI_LEG` | Stage job and leg (`linux-x64`, ...) | job id / matrix value | build configuration id / DSL parameter |
| `CI_WORKSPACE` | Checkout root | `github.workspace` | `%teamcity.build.checkoutDir%` |
| `CI_TEMP_DIR` | Per-run scratch | `runner.temp` | `%teamcity.build.tempDir%` |
| `CI_ARTIFACT_DIR` | D4 root | `${runner.temp}/artifacts` | checkout-relative directory published via artifact paths |
| `CI_LOG_DIR` / `CI_RESULTS_FILE` | RC2 logs / RC3 JSONL | under `CI_TEMP_DIR` | under `CI_TEMP_DIR` |
| `CI_SUMMARY_FILE` | Markdown job summary target (may be empty) | `GITHUB_STEP_SUMMARY` | empty; the reporter publishes `ci-summary.md` as an artifact instead |
| `CI_IMAGE` | RC4 `image=` value | `ImageOS` + `ImageVersion` | agent name + OS from `%teamcity.agent.name%` / `%teamcity.agent.jvm.os.name%` |
| `CI_PUBLISH_TOKEN` | Publish-only credential (D6) | `GITHUB_TOKEN` passed to publish jobs only | password parameter on the publish configuration only |

The parameter names in the TeamCity column are the documented TeamCity predefined parameters as best known when this ADR was written. Confirm each one during migration; this is checklist item 2.

### Platform-specific pieces of RC1-RC4 that move behind the reporter

As written in 0014 today, these are GitHub-only:

- `::error title=...::` and `::warning ...::` annotations;
- the `%`/CR/LF annotation escaping;
- `::stop-commands::<token>` / `::<token>::` injection fencing;
- `$GITHUB_STEP_SUMMARY`;
- `$RUNNER_TEMP`;
- RC4's `ImageOS`/`ImageVersion`.

After this ADR, RC1 defines **neutral events**:

- `step_ok`
- `step_fail(first_diagnostic, excerpt)`
- `step_warn`
- `step_skip`
- `begin_untrusted_block` / `end_untrusted_block`
- `job_summary(markdown)`

Each backend renders them:

| Neutral event | `github` backend | `teamcity` backend | `plain` backend (local) |
|---------------|------------------|--------------------|-------------------------|
| `step_ok` | `ok: ...` line | `ok: ...` line, optionally in `##teamcity[blockOpened/blockClosed]` | `ok: ...` line |
| `step_fail` | `::error title=<label>::...` | `##teamcity[buildProblem description='...' identity='<label>']` | `error: ...` line |
| `step_warn` (RC5) | `::warning title=best-effort: ...::` | `##teamcity[message text='...' status='WARNING']` | `warning: ...` |
| Escaping | `%25` / `%0D` / `%0A` | `\|\|`, `\|'`, `\|n`, `\|r`, `\|[`, `\|]` | none |
| Untrusted-excerpt fence (T-3) | `::stop-commands::<token>` ... `::<token>::` | `##teamcity[disableServiceMessages]` ... `##teamcity[enableServiceMessages]` | none needed |
| `job_summary` | Write to `CI_SUMMARY_FILE` | Publish `ci-summary.md` as an artifact, plus `##teamcity[buildStatus text='...']` one-liner | Print to stdout |

The line-count and byte ceilings (64 lines, 16 KiB), the discrepancy form and the data-hygiene rules stay platform-neutral and are enforced before the backend renders anything.

### Platform-capability seams

| Capability | Neutral side | GitHub Actions | TeamCity |
|------------|--------------|----------------|----------|
| Annotations / summaries | Reporter backend (above) | Workflow commands, `GITHUB_STEP_SUMMARY` | Service messages, artifact + build status |
| Test results | `ctest --output-junit` + RC3 counts (`ctest_report.py`) | Summary from RC3 | Same, plus the optional native XML Report Processing build feature on the JUnit file |
| Dependency cache | vcpkg binary cache configured by `VCPKG_BINARY_SOURCES` (vcpkg's own, neutral) with keys computed by `vcpkg_cache_report.py` | `actions/cache` saves/restores the `files` provider directory | TeamCity Build Cache feature, or an HTTP/NuGet binary source. Only the backing store changes. |
| Artifact hand-off between stages | D4 layout under `CI_ARTIFACT_DIR` | `actions/upload-artifact` / `download-artifact` | Artifact paths + artifact dependencies |
| Stage ordering / same-run gating | Stage order D1 | `needs:` | Snapshot dependencies in a build chain (same revision guaranteed) |
| Matrix / legs | Leg list in `scripts/ci/legs.json`; `plan.py` emits per-leg run/skip | `strategy.matrix` from the `plan` job via `fromJSON` | One build configuration per leg, generated by a Kotlin DSL loop over the same `legs.json` (or a matrix build feature where available); agent requirements select the OS |
| Trigger tiering (Q1) | `plan.py` decides; a leg that is not selected prints `skip: tier` itself | `on:` + `plan` job outputs | VCS trigger with branch filters + the same `plan.py`. Every configuration starts and self-skips, which costs agent time but keeps the decision in one place. |
| Concurrency cancel | Not neutral | `concurrency:` group + `cancel-in-progress` | Queue optimisation replaces superseded *queued* builds. Cancelling *running* builds needs a REST call or a server setting (needs reimplementation; verify on migration). |
| Nightly | `CI_EVENT=schedule` | `schedule: cron` | Schedule trigger |
| Release creation | `scripts/ci/publish_github_release.py` calling the GitHub REST API with `CI_PUBLISH_TOKEN` (no marketplace action) | `GITHUB_TOKEN`, `contents: write` on the publish job | Fine-grained token / GitHub App token as a password parameter on the publish configuration (there is no `GITHUB_TOKEN` outside Actions) |
| Registry PR | `scripts/ci/open_registry_pr.py` (REST) | `GITHUB_TOKEN` | Same token as above |
| Binding publish credentials | Stage command reads token or OIDC | `id-token: write` trusted publishing | **Needs reimplementation:** PyPI/crates.io trusted publishing does not list TeamCity as an identity provider, so a scoped API token is needed. This is a real security downgrade that must be recorded on migration. |
| Least privilege | D6 | Job `permissions:` + `release` environment reviewers | Credentials only on the publish configuration + its approval feature / project permissions |

### Rules the policy gate enforces

These rules are specified in full in [0014 Story 02.0 Task 03](/docs/roadmap/0014-ci-rework-from-scratch/02.0-pinned-toolchain-and-hardened-environment/03-workflow-lint-and-policy-gate.md). This ADR amends P7, P8 and P9 and adds P10-P13; the 2026-11-13 threat-model amendment further amends P2, P8 and P10-P13 and adds P14-P15:

- **P2 (amended 2026-11-13).** SHA pin plus version comment; `docker://` rejected.
- **P7 (tightened).** A `run:` body is exactly one line invoking `python3 scripts/ci/stage.py ...` (or, inside 0014's bootstrap/lint, `scripts/ci/ci_step.py` / `scripts/ci/lint.py`). It has no shell control flow, no pipes and no redirection.
- **P8 (amended; amended again 2026-11-13).** Triggers are limited to `push`, `pull_request`, `schedule`, `workflow_dispatch` and `workflow_call` (so `pull_request_target`, `workflow_run`, `issue_comment`, review and discussion triggers are violations), and `secrets: inherit` is banned. `secrets.` and `id-token: write` are allowed only in publish-stage jobs listed in `publish_jobs.json` that `needs:` the same run's `verify` and match one tier clause: `environment: release` + `v*` tag guard, or `environment: staging` + `refs/heads/master` push guard (D6, D9).
- **P9 (amended).** Allowed files: `ci.yml`, `stage-*.yml` and, until cutover, `ci-next.yml`. There is no `release.yml`.
- **P10 (amended 2026-11-13).** A step that calls a stage command receives inputs only through `env:` keys from the neutral-name table (`CI_*`, plus documented tool variables such as `VCPKG_BINARY_SOURCES`) or through argv. `scripts/ci/env_names.py` holds the table, and the gate reads it from there. The right-hand side of `env:` may read only an allowlist of contexts (`github.run_id`, `run_attempt`, `server_url`, `repository`, `event_name`, `ref`, `ref_name`, `ref_type`, `sha`, `workspace`, `github.event.before`, `github.event.pull_request.base.sha`, `runner.temp`, `matrix.*`, `needs.plan.outputs.*`, declared choice `inputs.*`); `head_ref`, titles, bodies, messages, labels and names of PR/issue/comment objects are violations.
- **P11 (amended 2026-11-13).** No platform syntax in scripts. `scripts/ci/**` (apart from `reporters/`, `env_names.py` and the test harness `tests/`) must not contain `${{`, `%teamcity.`, `##teamcity[`, `::error`, `::warning`, `::stop-commands`, or any read of `GITHUB_*` / `RUNNER_*` / `TEAMCITY_*` variables. No `print(`/`sys.stdout.write` outside `ci_step.py` and `reporters/` (single emitter).
- **P12 (amended 2026-11-13).** Publish-capable permissions appear only on jobs listed in `scripts/ci/publish_jobs.json`, one per permission class of D9, and the caller job of every `stage-publish.yml` call is checked against the same tier clause. Every `enabled` entry in `scripts/ci/publish_destinations.json` must name an existing module, a known permission class and an environment equal to its tier; a `release`-tier entry is reachable only from a `v*` tag event and a `staging`-tier entry only from a `master` push. A release trigger pattern must not match the legacy `-DGIT_TAG` tag format.
- **P13 (amended 2026-11-13).** Every `uses:` is listed in `scripts/ci/actions_allowlist.json` with a one-line justification, either its TeamCity equivalent or the words `needs reimplementation`, and `tag_commit_verified_on`: the date a reviewer confirmed with `gh api .../git/ref/tags/<version>` (dereferencing annotated tags) that the pinned SHA is the upstream tag's commit, not an impostor commit from a fork network. An action whose job a neutral script can do (release creation, checksums, archive upload to a release, vcpkg bootstrap) is rejected, as are `actions/github-script` and local or composite actions.
- **P14 (new 2026-11-13).** Every `actions/checkout` sets `persist-credentials: false`.
- **P15 (new 2026-11-13).** No `actions/cache` in verify or publish jobs; cache saves only on `master` push or `schedule`; `logs-*` artifact uploads only in `stage-build.yml`, with literal file paths (no directory, no glob) and `include-hidden-files: false`.

**Current marketplace actions and their status:**

| Action | Justification | TeamCity equivalent |
|--------|---------------|---------------------|
| `actions/checkout` | Native checkout | VCS root (native) |
| `actions/cache` | Cache backing store | Build Cache feature |
| `actions/upload-artifact` / `download-artifact` | Stage hand-off | Artifact paths / artifact dependencies |
| `lukka/run-vcpkg` | - | **Needs reimplementation.** A neutral `scripts/ci/bootstrap_vcpkg.py` (clone at the pinned commit + `bootstrap-vcpkg`) can do this job, so P13 rejects the action. 0014 Story 02.0 Task 02 replaces it, which reverses that story's Non-goal. `bootstrap_vcpkg.py` fetches the pinned baseline commit by full SHA, asserts `HEAD`, verifies the vcpkg tool binary's SHA-512 and never uses the image's own vcpkg (threat model CR-7). |

### Migration checklist (GitHub Actions to TeamCity)

1. Write `.teamcity/settings.kts`. Generate one build configuration per stage and leg from `scripts/ci/legs.json`, and chain them with snapshot and artifact dependencies in D1 order.
2. Map every neutral name in the table to a TeamCity parameter, and verify each predefined parameter name against the TeamCity version in use.
3. Set `CI_REPORTER=teamcity`. Confirm that the backend's unit tests pass and that a failing step renders `buildProblem` inside the 64-line ceiling.
4. Point `VCPKG_BINARY_SOURCES` at a TeamCity-reachable store (Build Cache feature or an HTTP provider).
5. Recreate the tier table with VCS/schedule triggers, branch filters and `plan.py`. Decide whether to accept self-skipping builds or to use a REST-based scheduler.
6. Choose an equivalent for concurrency cancel, or accept that it is lost (it costs agent time, not correctness).
7. Move publish credentials to password parameters on the publish configuration only. Record the trusted-publishing downgrade for binding registries.
8. Re-run the 0014 negative proofs (failing build, failing test, injected `##teamcity[` line in tool output, oversize output) on TeamCity agents.
9. Run `stage.py` for every stage locally and on TeamCity, and diff the RC3 records. They must match apart from timings.
10. Retire the GitHub adapter. Remove `.github/workflows/` and the `github` reporter only after one green release-tag run on TeamCity.

## Alternatives Considered

| Alternative | Pros | Cons | Reason rejected |
|-------------|------|------|-----------------|
| **A - Keep separate designs** (0014's `ci.yml`, 0012's `release.yml`, 0013's jobs added to `ci.yml`) and only add a "use `ci_step.py`" amendment, as 0014 `plan.md` originally proposed | Smallest planning change, and each milestone stays self-contained | Two build paths (tested is not shipped, DD1); separate version and permission handling; the release workflow is written in a dialect the policy gate already rejects; three places to port on a CI migration | Fails DD1 and DD2, and contradicts the maintainer's "unified subsystem" ruling |
| **B - One monolithic `ci.yml`** holding every stage as inline jobs, gated with `if:` on the event | One file; same-run gating comes for free; no `workflow_call` plumbing | A very large file; probes and single-stage dispatch cannot reuse a stage; publish-job permissions sit next to every test job, which makes review harder | Same-run gating is kept in D8 through reusable stages, without the size and review cost |
| **C - Entry workflow + reusable stage workflows over neutral stage commands** (adopted) | Same-run gating (DD1); one contract (DD2); stage YAML is small and per-concern; probes reuse stages; the adapter is thin (DD5) | `workflow_call` adds GitHub-specific indirection (permissions cap at the caller, outputs plumbing); more files under the P9 allowlist | **Adopted** |
| **D - Stage logic directly in YAML** (inline shell / marketplace actions per step, the common GitHub pattern) | Familiar; the GitHub UI shows each step natively; no Python runner to maintain | Untestable locally (E5 in 0014); unbounded output; full rewrite on any CI migration; logic split between YAML and scripts | Fails DD5 and repeats 0014's evidence E1/E5/E8 |
| **Version source: the tag** (review suggestion) | No file to bump; a tag is the natural release request | Empty at configure time inside vcpkg port builds and source-tarball builds; existing `-DGIT_TAG` tags complicate parsing | Rejected for D5's in-tree value plus a tag check |
| **Version source: port `vcpkg.json`** | Already the registry's own value | Makes a packaging file authoritative over the product build; the CLI/`libsmbios` would read a port file | Rejected |

## Consequences

### Positive

- **Tested equals shipped by construction.** Archives come from the tested build, and `verify` and `publish` check digests against one manifest (DD1).
- **One contract, one runner, one pin set and one policy gate** cover lint through publish, and future binding jobs inherit them without negotiation (DD2, DD3).
- **A version mismatch fails `lint` in about a minute** on every event, not on release day (DD3).
- **Write permissions exist only in publish jobs** behind a tag guard and an environment, which the policy gate checks mechanically (DD4).
- **Migrating CI touches only the adapter, the reporter backend and env-name glue.** Every stage command can be run and debugged locally, which also addresses 0014's E6 debugging-cost problem (DD5).

### Negative

- **Extra indirection.** A reader follows YAML -> `stage.py` -> `ci_step` to see what a step does. The GitHub UI shows one step per stage command rather than one per sub-action.
- **Three reporter backends to maintain.** The TeamCity backend is unused until a migration happens, so it is kept honest only by unit tests on rendered strings.
- **More CI minutes on PRs.** Package and verify now run on every PR to `master` and nightly, and a same-repo PR rebuilds the Linux packaging leg (D7).
- **Sequencing.** 0012's workflow work cannot start until 0014's foundation (Stories 01.0-03.0) lands. A root CMake version change (D5 derivation) becomes a 0012 task, which counts as a product-build change.
- **`lukka/run-vcpkg` must be replaced** with a project-owned bootstrap script, which is new code to keep correct.
- **Destination plug-ins add a small interface to maintain** (D9), and a destination needing a new permission class still needs one reviewed adapter change.
- **The TeamCity migration loses OIDC trusted publishing** for binding registries and loses free concurrency cancel. Both are recorded, not solved.
- **Security depends on settings outside the repository** (2026-11-13). The policy gate cannot protect against an actor who can edit it; the [Repository settings required](#repository-settings-required) checklist must be applied and re-checked by hand, and drift there is invisible to CI.
- **Release builds are always cold** (CR-2). Tag-event packaging legs ignore the vcpkg binary cache, so a release run takes a full dependency build on every packaging leg.
- **Staging publishes cannot be triggered on demand** (D9 as amended); a fresh staging copy needs a `master` push.

## Open questions for the maintainer

These are recorded only. This ADR does not decide them.

1. **Licence** (review H3). It blocks the port's copyright step, the licence field of every artifact, and crates.io/PyPI.
2. **`.pc` relocatability** (review H1). Should `smbios_c.pc.in` use a `pcfiledir`-relative prefix, which needs an exception to 0012's "pc unchanged" constraint, or should each artifact be configured with its own prefix?
3. **CLI in the tarball** (review I2). Does the `smbios` CLI gain an `install()` rule and ship in the archive?
4. **Registry hosting** (review H2 / unknown 6). Should the registry live on this repo's `master` (consumers pin a SHA) or in a dedicated `vcpkg-registry` repo (which breaks the circularity)?
5. Also still open from the review: whether Windows static triplets are supported (M4), and which build configuration ships (D1; 0012 Story 03.0 decides).
6. **Dispatch to the staging tier** (RESOLVED 2026-11-13: maintainer kept it removed) (added 2026-11-13). D9 now limits `staging` publishes to `master` pushes. If manual dispatch of a staging publish is wanted back, it must be restricted to `refs/heads/master` and the `staging` environment then needs a required reviewer - which also gates every `master`-push staging publish. Keep it removed (recommended), or re-add it with a reviewer?
7. **Repository settings** (added 2026-11-13, threat model CR-1). The real boundary for every in-repo rule is the checklist in [Repository settings required](#repository-settings-required). Applying it is a maintainer action and is not done by any milestone task. In particular: whether `master` gets a ruleset at all (plan Q5 recorded none), whether "Allow GitHub Actions to create and approve pull requests" stays enabled for 0012's registry PR, and who the `release` reviewers are.

## Repository settings required

Added 2026-11-13 from the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) CR-1 and CR-3. For `push`, `pull_request`, `workflow_dispatch` and tag events, GitHub takes the workflow YAML from the triggering ref, so the policy gate (P1-P15) is enforced by code that an actor with write access, or a stolen token with `workflows` scope, can change in the same commit. Only the settings below sit outside that ref. They are **maintainer actions**, applied in repository and cloud settings, not in code. Each is read back with `gh api` (or the AWS equivalent) and the read-back recorded in `docs/dev/ci.md` and milestone 0014's `status.md` at the 0014 Story 03.0 Task 04 cutover. The publish items must be in place before the first workflow that can reach `package` on a tag or use a `staging`/`release` environment (milestone 0012).

**GitHub repository**

- [ ] Default `GITHUB_TOKEN` workflow permissions: read-only (`gh api repos/{owner}/{repo}/actions/permissions/workflow`).
- [ ] "Allow GitHub Actions to create and approve pull requests": off, unless 0012's registry PR needs it; the decision is recorded.
- [ ] Fork-PR workflows require approval for outside contributors.
- [ ] **Tag ruleset:** creating, updating or deleting `v*` tags is restricted to maintainers (`gh api repos/{owner}/{repo}/rulesets`).
- [ ] **Branch ruleset on `master`:** per the maintainer's decision (Q5 records no protection today). If enabled: code-owner review required, no force-push, no deletion.
- [ ] **CODEOWNERS** covering `/scripts/ci/` and `/.github/` (and `/scripts/ci/publish_destinations.json`, `/scripts/ci/publish_jobs.json`, `/scripts/ci/actions_allowlist.json` explicitly), owned by maintainers. CODEOWNERS only binds when the `master` ruleset requires code-owner review.

**GitHub environments** (`gh api repos/{owner}/{repo}/environments`)

- [ ] `release`: deployment rule "selected tags" `v*` only; required reviewer(s); "prevent self-review" on; no admin bypass.
- [ ] `staging`: deployment rule branch `master` only. A required reviewer is needed only if open question 6 re-adds dispatch.

**Cloud side, S3 staging (D9)**

- [ ] OIDC trust policy: `StringEquals` on `token.actions.githubusercontent.com:aud = sts.amazonaws.com` and `:sub = repo:<owner>/<repo>:environment:staging`. No `StringLike` wildcard.
- [ ] Role policy: `s3:PutObject`, `s3:PutObjectTagging` and `s3:GetObject` only on `arn:aws:s3:::<bucket>/staging/*`. No `DeleteObject`, ACL or bucket-policy actions.
- [ ] Release content lives in a different bucket, or an explicit bucket-policy `Deny` for the staging role covers every key outside `staging/`; the release prefix has versioning (or Object Lock).
- [ ] Session duration at most 15 minutes; a lifecycle expiry on `staging/`.
- [ ] A recorded `aws iam simulate-principal-policy` (or a dry run) shows `PutObject` on `release/x` denied and `DeleteObject` on `staging/x` denied. The record names no role ARN, account id or bucket policy body in public logs.

## Validation / Rollout

Rollout follows existing milestone boundaries:

- **0014 (foundation).** It delivers:
  - neutral env names and reporter backends (Story 01.0 Task 02);
  - pins as one data file, `bootstrap_vcpkg.py`, and policy rules P7-P13 (Story 02.0);
  - `stage.py` with `lint`/`build`/`test`, `check_versions.py`, the reusable `stage-lint.yml`/`stage-build.yml`, and the install-tree export on packaging legs (Story 03.0);
  - the D7 tier table rows (Story 04.0).

  `package`/`verify`/`publish` rows report `not run: stage not defined` until 0012 adds them.
- **0012 (stage definitions).** It adds `stage.py package|verify|publish`, `stage-package.yml`/`stage-verify.yml`/`stage-publish.yml`, the version sites it introduces, and the D5 CMake derivation.
- **0013.** Its binding build/test/publish jobs are legs of these stages on the same contract.

Each step is validated in these ways:

- A scratch pre-release tag (`v0.0.0-rc.1`, deleted afterwards) produces a draft promoted only after `verify`. A tag whose version mismatches fails in `lint` and publishes nothing. A corrupted archive (digest mismatch) fails `verify`.
- `stage.py <stage>` run locally with `CI_REPORTER=plain` produces RC3 records equal to those of the CI run, apart from timings.
- Reporter-backend unit tests render every neutral event for `github`, `teamcity` and `plain`, including a hostile `::error::` / `##teamcity[` line inside an untrusted excerpt.

## Links

- **Roadmap:** [milestone 0014 plan](/docs/roadmap/0014-ci-rework-from-scratch/plan.md) (foundation), [milestone 0012 plan](/docs/roadmap/0012-package-manager-distribution/plan.md) (stage definitions), [milestone 0013 plan](/docs/roadmap/0013-language-bindings/plan.md) (binding legs)
- **How-to:** [adding a publish destination (S3 staging example)](/docs/dev/adding-a-publish-destination.md)
- **Supporting specs:** [packaging-automation review](/docs/reviews/milestone-0012-packaging-automation-2026-11-12.md), [ADR 0004](0004-package-manager-distribution.md)
- **Threat model:** [CI rework and pipeline foundation, 2026-11-13](/docs/security/2026-11-13-ci-rework.md) (source of the 2026-11-13 amendments)
- **Diagrams:** [docs/diagrams/07-build-package-publish-pipeline.md](/docs/diagrams/07-build-package-publish-pipeline.md), [docs/diagrams/06-test-and-ci-flow.md](/docs/diagrams/06-test-and-ci-flow.md)
