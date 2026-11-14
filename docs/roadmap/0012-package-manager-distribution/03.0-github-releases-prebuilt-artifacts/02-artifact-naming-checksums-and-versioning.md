# 02 - Artifact naming, checksums, and versioning

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

> **Amended 2026-11-12 by [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md).**
> Three things in this task are superseded:
>
> - **The checksum step.** The `shasum` YAML step below is withdrawn.
>   Checksums come from `scripts/ci/package_archive.py` (Task 01; stdlib,
>   so it also works on Windows, review L1).
> - **The artifact name.** It is now `libbios-<version>-<leg>.<ext>`, where
>   `<version>` is the D5 version (the tag without `v`) and `<leg>` is
>   `<os>-<arch>`. It is no longer `libbios-<tag>-<preset>`.
> - **Version drift.** The statement that the tag "may drift" from
>   `SMBIOS_C_VERSION`/`LIBSMBIOS_VERSION` is **reversed**: one
>   authoritative CMake version, checked in CI (D5; Story 04.0 Task 01).
>
> What remains of this task is the documentation deliverable
> (`docs/dev/release-artifacts.md`: naming, verifying a download against
> `.sha256`/`manifest.json`, and the supported arch/glibc baseline from
> review M6). The illustrative YAML below is kept only as history.

## Objective

Define a minimal, reproducible artifact naming convention and add
checksums, without inventing a semantic-versioning policy this project
has not actually adopted (per the milestone's own Key constraints and
ADR 0004 DD2).

## Files

- **Modify** `.github/workflows/release.yml` (Task 01): add a checksum
  step per artifact:
  ```yaml
  - name: Checksum
    run: shasum -a 256 libbios-${{ github.ref_name }}-${{ matrix.preset }}.* > libbios-${{ github.ref_name }}-${{ matrix.preset }}.sha256
  ```
  (Windows runner: `sha256sum`/`Get-FileHash` equivalent - confirm the
  cross-platform form during implementation.)
- **Create** documentation (same location Story 03.0's README names,
  e.g. `docs/dev/release-artifacts.md`) recording:
  - The artifact filename convention: `libbios-<tag>-<preset>.{tar.gz,zip}`
    (`<tag>` is the pushed git tag verbatim, `<preset>` one of
    `windows`/`linux`/`macos` - **not** a separately-maintained product
    version number, since none exists yet for the combined
    `libsmbios`+`smbios_c` artifact).
  - That `<tag>` currently has **no enforced relationship** to
    `smbios_c/CMakeLists.txt`'s own `SMBIOS_C_VERSION` or Story 01.0's
    `LIBSMBIOS_VERSION` - they may drift; reconciling them into one
    policy is explicitly deferred (same deferral as milestone 0007's own
    Non-goals), not resolved by this task.
  - A short note on how to verify a downloaded artifact against its
    `.sha256` file.

## Functions and Classes

N/A - documentation and CI metadata only.

## Constraints

- **Does not introduce a semver policy.** The tag format
  (`on: push: tags: ["v*"]` from Task 01) is a release-trigger
  convention only, not a promise about compatibility between tags.
- **Checksums are published alongside, not instead of, GitHub's own
  built-in artifact integrity** (GitHub Releases already serves
  artifacts over HTTPS with its own hosting integrity) - this is purely
  for a consumer who wants to verify a downloaded file against a
  separately-obtained hash (e.g. one pasted into an announcement),
  matching common OSS release practice.

## Tests

- Covered by Task 03.

## Success criteria

- [ ] Every artifact from Task 01's workflow has a matching `.sha256`
      file attached to the same release.
- [ ] The naming/versioning documentation is written down and linked
      from `docs/cli-usage.md` or an equivalent discoverable doc.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems.
