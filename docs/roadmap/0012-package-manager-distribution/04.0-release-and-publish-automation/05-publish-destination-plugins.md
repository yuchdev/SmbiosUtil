# 05 - Publish destination plug-ins and the S3 staging example

**Status:** ⬜ Not started
**Depends on:** Tasks 02-03; 0014 Stories 02.0-03.0 (SC5, policy gate P12)

## Objective
Implement ADR 0005 D9: the destination interface, the registry file, and the existing GitHub Release and registry-PR publishing as the first two destinations, plus `s3-staging` as the worked example of adding a third.

## Files
- **Create** `scripts/ci/destinations/{__init__,github_release,registry_pr,s3_staging}.py`: each implements `NAME`, `REQUIRED_ENV`, `validate`, `publish`, `check_published`. Tasks 02 and 03's scripts become the first two modules rather than standalone scripts.
- **Create** `scripts/ci/publish_destinations.json` (`github-release` and `registry-pr` enabled, tier `release`; `s3-staging` present, `enabled: false` until the maintainer provides a bucket and role).
- **Modify** `stage.py publish --destination <name> [--dry-run]`; `plan.py` emits the publish matrix per permission class.
- **Modify** `stage-publish.yml`: one matrix job per permission class, each `needs: verify`, with the tier and tag/branch guards from D6/D9.
- **Create** tests under `scripts/ci/tests/` as listed in `docs/dev/adding-a-publish-destination.md`.

## Constraints
- A destination reads the manifest only; it never rebuilds or recomputes digests.
- `staging` tier never runs on pull requests or forks and never writes a `release` location (the S3 prefix check).
- Enabling `s3-staging` is a maintainer action (bucket, OIDC trust, `staging` environment); it ships disabled.

## Success criteria
- [ ] Adding a fourth destination in an existing class needs no workflow edit, shown by a unit test that registers a fake destination.
- [ ] `--dry-run` of every destination lists exact files and keys with no network or secret access.
- [ ] Policy gate P12 fails on a registry entry with an unknown class, a missing module, or a `release` tier reachable from a non-tag event.
- [ ] `python3 scripts/check_doc_links.py docs/` reports zero problems.
