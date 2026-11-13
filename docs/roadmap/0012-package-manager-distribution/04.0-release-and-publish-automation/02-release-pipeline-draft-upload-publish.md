# 02 - Publish stage: gated draft, upload, promote

**Status:** ⬜ Not started

## Objective

Define the `publish` stage of the [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md) pipeline for the GitHub Release. It replaces Story 03.0's former three-way parallel upload (review M2).

## Design

This is `python3 scripts/ci/stage.py publish --only github-release`, run as jobs in `stage-publish.yml`. Ordering, the tag guard, `needs: verify`, the `release` environment and permissions are ADR 0005 D6 and 0014 SC4, enforced by the policy gate.

1. **`release-draft`:**
   - run `check_versions.py --tag "$CI_REF_NAME"`;
   - create **one** draft release with generated notes;
   - upload exactly the files listed in `dist/manifest.json`, plus the manifest itself;
   - re-read the draft's assets and compare their digests with the manifest.
2. **`release-promote`:** promote the draft to a published release.
3. **No marketplace action** (P13). `scripts/ci/publish_github_release.py` calls the GitHub REST API with `CI_PUBLISH_TOKEN`.

## Success criteria

- [ ] A scratch pre-release tag yields one draft with three archives, three checksums and the manifest. The draft is promoted only after `verify` passed in the same run.
- [ ] A tag whose version mismatches fails in `lint` and publishes nothing.
- [ ] No upload race: only `release-draft` creates the release.
