# 7. Process: unified build, package and publish pipeline (planned)

Sources: [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md)
(D1, D4 to D9, "Platform portability" and "Repository settings required") and the "Pipeline stage
contracts (ADR 0005)" section of
[`../roadmap/0014-ci-rework-from-scratch/plan.md`](../roadmap/0014-ci-rework-from-scratch/plan.md).
This diagram describes the **planned** design (milestones 0012 and 0014); the
as-is `ci.yml` is in [diagram 6](06-test-and-ci-flow.md). Mermaid, not
rendered locally.

## 7a. Stage flow and artifact hand-off

```mermaid
flowchart LR
    lint["lint<br/>RC3 records, version check"] --> bt["build/test<br/>per leg"]
    bt -->|"install/leg/ artifact;<br/>tree digest + artifact ID<br/>as job outputs"| pkg["package<br/>download by artifact ID,<br/>compare needs.build.outputs,<br/>archive only"]
    pkg -->|"dist/ archives, .sha256,<br/>manifest.json"| ver["verify<br/>clean-room consumer tests"]
    ver -->|"push to master"| stg["publish: staging tier<br/>environment staging"]

    subgraph rel["publish: release tier, tag v* only, environment release"]
        direction LR
        rd["release-draft<br/>upload, compare digests"] --> rp["release-promote"]
        rp --> pp["post-publish<br/>re-check public assets"]
        rv["registry-pr<br/>validated in-run, then PR opened"]
    end

    ver -->|"tag v*"| rel
    man(["manifest.json digest gate"]) -.-> ver
    man -.-> stg
    man -.-> rd
    man -.-> rv
```

Reading the diagram:

- `$CI_ARTIFACT_DIR/install/<leg>/` is the `cmake --install` output of the
  tested build. `build` computes a tree digest of it and exports the digest
  and the uploaded artifact ID as **job outputs** (not a file inside the
  artifact); `package` and later stages download by artifact ID and compare
  against `needs.<build job>.outputs`, because only the owning job can write
  its outputs while a sibling job could replace an artifact.
- `package` only archives that tree into
  `$CI_ARTIFACT_DIR/dist/libbios-<version>-<leg>.<ext>` plus `.sha256` files
  and `dist/manifest.json` (D4). `<version>` is the D5 version, not the raw
  tag.
- `verify` and `publish` refuse any file whose digest differs from
  `manifest.json`.
- Staging tier (D9, for example S3 staging) runs only on a push to `master`,
  after `verify`, in environment `staging`. Release tier runs only on a `v*`
  tag, after `verify`, in environment `release` (D6). Dispatch, PR (same-repo
  or fork), non-`master` push and nightly never reach any publish job.
- `release-draft` creates the draft once, uploads the files and compares the
  draft's asset digests with the manifest; `release-promote` publishes it;
  `post-publish` downloads the public assets and re-checks them (a loud
  failure, not a rollback).
- `registry-pr` runs the registry validation inside the publish run before
  the PR is opened, because a PR opened with `GITHUB_TOKEN` triggers no
  workflow.
- Sanitizer and GUI legs are `build/test` only and never feed `package`.

### Which events reach which stage (D7)

| Event | lint | build/test | package | verify | publish |
|-------|------|------------|---------|--------|---------|
| Push, docs-only changes | yes | - | - | - | - |
| Push to a non-`master` branch | yes | Linux legs | - | - | - |
| Same-repo PR to `master` | yes | legs not run by the push tier | yes (all packaging legs) | yes | - |
| Fork PR, nightly, dispatch `all` | yes | full matrix | yes | yes | - |
| Push to `master` | yes | full matrix | yes | yes | `staging` tier only |
| Release tag `v*` | yes | full matrix | yes | yes | `release` tier |

There is no separate `release.yml`: a release is the same pipeline on a tag
event (D8).

## 7c. vcpkg bootstrap and binary cache

```mermaid
flowchart TD
    boot["bootstrap_vcpkg.py<br/>pinned baseline commit,<br/>replaces lukka/run-vcpkg"] --> ev{"CI_EVENT"}
    ev -->|"master push, schedule"| save["clear;files,dir,readwrite<br/>cache saved"]
    ev -->|"PR, fork, other branch"| ro["clear;files,dir,read<br/>restore only"]
    ev -->|"tag, packaging leg"| none["VCPKG_BINARY_SOURCES=clear<br/>no restore, no save<br/>manifest binary_cache none"]
    save --> man["manifest binary_cache per leg"]
    ro --> man
    man --> gate{"publish tier"}
    gate -->|"release"| refuse["refuses a leg with<br/>binary_cache restored"]
    gate -->|"staging"| accept["accepts restored,<br/>copy is disposable"]
```

- Every job's `VCPKG_BINARY_SOURCES` starts with `clear`, so a runner's own
  vcpkg cache is never consulted. No `actions/cache` runs in verify or
  publish jobs (P15).
- The cache never feeds a release artifact (ADR 0005 D9, CR-2).

## 7d. Trust boundary

```mermaid
flowchart LR
    subgraph inref["Inside the triggering ref: editable by a writer"]
        yaml["workflow YAML and scripts/ci"] --> gate["policy gate P1-P15"]
    end
    subgraph outref["Outside the ref: repository and cloud settings"]
        rs["rulesets: v* tags, master"]
        co["CODEOWNERS on scripts/ci and .github"]
        envp["environment protection:<br/>release tags v*, reviewers;<br/>staging master only"]
        role["S3 role policy and OIDC trust<br/>staging/* put only, no delete"]
    end
    gate -.->|"cannot enforce itself"| outref
    rs --> co
    envp --> role
```

- For push, PR, dispatch and tag events GitHub runs the YAML of the
  triggering ref, so the in-repo policy gate is not the boundary; the
  settings on the right are. They are maintainer actions listed in
  [ADR 0005, Repository settings required](/docs/adr/0005-unified-build-package-publish-pipeline.md#repository-settings-required).

## 7b. Layering for CI-platform portability

```mermaid
flowchart TD
    subgraph adapter["(a) adapter: rewritten on migration"]
        gha["GitHub Actions today<br/>ci.yml and stage-*.yml"]
        tc["TeamCity tomorrow<br/>Kotlin DSL settings.kts"]
    end

    env["neutral CI_* environment names<br/>CI_EVENT, CI_LEG, CI_ARTIFACT_DIR, ..."]

    subgraph logic["(b) stage logic: unchanged"]
        stage["scripts/ci/stage.py<br/>lint, build, test, package, verify, publish"]
        step["ci_step library"]
        rep["reporter backends<br/>github, teamcity, plain<br/>selected by CI_REPORTER"]
    end

    subgraph contracts["(c) neutral contracts: unchanged"]
        rc["RC1-RC5 report contract<br/>SC1-SC4 stage contracts"]
        d4["D4 artifact layout and manifest.json"]
        d5["D5 version sites"]
    end

    gha --> env
    tc --> env
    env --> stage
    stage --> step
    step --> rep
    stage -.-> contracts
    step -.-> contracts
    rep --> out["platform annotations,<br/>summaries, service messages"]
```

Reading the diagram:

- Only the adapter layer is rewritten on a migration; each `run:` step in
  the adapter is one `stage.py` invocation.
- Scripts read only the neutral `CI_*` names (policy rules P10, P11). Only
  `scripts/ci/reporters/` emits platform workflow commands or service
  messages.
- `stage.py` runs the same way on a developer machine with
  `CI_PLATFORM=local` and `CI_REPORTER=plain`.
- Exit codes: 0 success, 1 a gate failed, 2 usage or environment error,
  128+n killed by signal n.
