# 03 - Registry update in the publish stage

**Status:** ⬜ Not started

## Objective

Automate the circular steps of review finding H2 as the publish stage's `registry-pr` job ([ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md) D6).

## Design

This is `python3 scripts/ci/stage.py publish --only registry-pr`, run after `release-promote`. The job:

- checks out the registry branch/repo;
- sets the port `REF` to the released tag's commit;
- runs `vcpkg x-add-version smbios-c`, which computes `git-tree` and updates `versions/baseline.json` and `versions/s-/smbios-c.json`;
- **validates the result in the same run**, with the Story 02.0 Task 03 consumer test against a `filesystem` registry pointing at that checkout;
- only then opens a PR through `scripts/ci/open_registry_pr.py` (REST, `CI_PUBLISH_TOKEN`).

The validation must run before the PR because a PR opened with `GITHUB_TOKEN` triggers no workflow, so no PR-side check would ever run (ADR 0005 D6).

Two items carry over from the review and stay in this task:

- Registry hosting (this repo's `master` vs a dedicated registry repo) is an open maintainer decision (ADR 0005 Open questions 4), recorded before implementation.
- The portfile fixes from review finding M4 (`vcpkg_from_git` arguments, the real repo URL, `vcpkg_check_linkage`), and a decision on M5 (root CMake options for the CLI and tests).

## Success criteria

- [ ] Publishing a scratch release produces a registry PR with a consistent `git-tree`, which passes `vcpkg x-add-version --verbose` without edits.
- [ ] No manual step remains in the "bump baseline" procedure doc. That doc becomes a description of the automation.
