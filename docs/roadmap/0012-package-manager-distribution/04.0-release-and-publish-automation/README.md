# Story 04.0 - Release and Publish Automation

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Stories 01.0, 02.0 and 03.0 (it automates their manual steps); milestone 0014 Stories 01.0-03.0 and Story 04.0 Task 02 (the pipeline foundation and the tier rows, per [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md))

## Scope

Stories 01.0-03.0 produce the packaging pieces. On their own, they leave the path from "decide to release" to "consumer can install it" partly manual and unverified. This story closes that gap by defining the pipeline's **`verify` and `publish` stages** (ADR 0005 D1, D6) and the release preparation they need. One action, pushing a `v*` tag, then yields:

- a consistent version everywhere;
- a published GitHub Release whose archives are the verified bytes;
- a registry PR;
- a post-publish check.

Origin: [docs/reviews/milestone-0012-packaging-automation-2026-11-12.md](/docs/reviews/milestone-0012-packaging-automation-2026-11-12.md) (findings H1, H2, M1-M3, L1, L2).

Workflow conventions are not restated here. Pins, permissions, step shape, reporting, neutral env names, trigger tiering and the publish-privilege rules are all defined by ADR 0005 and milestone 0014 (RC1-RC5, SC1-SC4, P1-P13). The policy gate enforces them on this story's `stage-verify.yml`/`stage-publish.yml`.

## Tasks

| #  | Document | Status | Blocks |
|----|----------|--------|--------|
| 01 | [Version sites and release preparation](01-version-source-and-release-prep.md) | ⬜ Not started | 02 |
| 02 | [Publish stage: gated draft, upload, promote](02-release-pipeline-draft-upload-publish.md) | ⬜ Not started | 03 |
| 03 | [Registry update in the publish stage](03-registry-update-automation.md) | ⬜ Not started | 04 |
| 04 | [Verify stage and post-publish check](04-post-publish-verification.md) | ⬜ Not started | 05 |
| 05 | [Publish destination plug-ins and the S3 staging example](05-publish-destination-plugins.md) | ⬜ Not started | - |

## Key constraints

- **Blocking maintainer decisions** (ADR 0005 "Open questions"; the review's "Unknowns"): the licence, `.pc` relocatability, whether the CLI ships in the tarball, and registry hosting. Task 01 records each one in `status.md` before any stage is implemented. The version source is **already decided** (ADR 0005 D5).
- **Tested equals shipped.** `verify` and `publish` act only on the files listed in the package stage's `manifest.json`, and refuse any digest mismatch.
- **Publish runs only on a `v*` tag, after `verify` in the same run.** Its permissions and credential rules are ADR 0005 D6 (enforced by P8/P12).
- **Registry changes land through a PR that a human merges**, never through an unreviewed push.
