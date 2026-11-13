# 04 - Verify stage and post-publish check

**Status:** ⬜ Not started

## Objective

Verify, automatically and on all three OSes, the exact archives that `publish` will upload ([ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md) D1, D4). Then check what users actually download.

## Design

- **`verify` stage** (`stage.py verify --leg <leg>`, `stage-verify.yml`; runs on PRs to `master`, nightly and tags per D7). The stage:
  1. downloads `dist/` from the package stage of the same run;
  2. checks every digest against `manifest.json`;
  3. extracts each archive into an empty directory (no source checkout, no vcpkg);
  4. runs Story 03.0 Task 03's clean-room `pkg-config` and `find_package(libsmbios CONFIG)` smoke tests and the `nm`/`dumpbin` export-allowlist check.
- **Post-publish check** (`stage.py publish --only post-publish`, after `release-promote`). It downloads the public assets by URL and re-checks them against the manifest. A failure is loud and names the release. It is not a rollback.
- **Registry consumer check.** After the registry PR merges, a nightly `verify` step runs the Story 02.0 Task 03 vcpkg consumer test against the real registry baseline.

## Success criteria

- [ ] Each OS verifies its own archive, and a corrupted asset (digest mismatch) fails the pipeline before `publish`.
- [ ] The vcpkg consumer test passes against the published registry with no overlay flag.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems.
