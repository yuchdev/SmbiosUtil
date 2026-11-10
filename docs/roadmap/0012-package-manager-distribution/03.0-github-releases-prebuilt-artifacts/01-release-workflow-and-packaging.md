# 01 - Release workflow and packaging

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add a CI workflow that, triggered by a version-tag push, builds this
project on Windows/Linux/macOS via the existing `CMakePresets.json`
presets, runs `cmake --install` into a scratch prefix, archives that
prefix per platform, and uploads each archive to the GitHub Release
corresponding to the pushed tag.

## Files

- **Create** `.github/workflows/release.yml`:
  ```yaml
  name: Release artifacts

  on:
    push:
      tags: ["v*"] # exact pattern decided in Task 02's naming/versioning task

  jobs:
    package:
      strategy:
        matrix:
          include:
            - os: ubuntu-latest
              preset: linux
            - os: windows-latest
              preset: windows
            - os: macos-latest
              preset: macos
      runs-on: ${{ matrix.os }}
      steps:
        - uses: actions/checkout@<pinned-sha>
        - name: Setup vcpkg
          uses: lukka/run-vcpkg@<pinned-sha>
          with:
            vcpkgDirectory: ${{ github.workspace }}/vcpkg
        - name: Configure
          run: cmake --preset ${{ matrix.preset }}
          env:
            VCPKG_ROOT: ${{ github.workspace }}/vcpkg
        - name: Build
          run: cmake --build --preset ${{ matrix.preset }}
        - name: Install
          run: cmake --install build/${{ matrix.preset }} --prefix dist
        - name: Archive
          run: |
            # exact archive tool/name is Task 02's job - illustrative only
            tar -C dist -czf libbios-${{ github.ref_name }}-${{ matrix.preset }}.tar.gz .
        - name: Upload to release
          uses: softprops/action-gh-release@<pinned-sha>
          with:
            files: libbios-${{ github.ref_name }}-${{ matrix.preset }}.*
  ```
  (illustrative - archive format per OS, the release-upload action
  choice, and exact step names are implementation decisions; pin every
  third-party action to a commit SHA, matching this repo's existing
  `ci.yml` convention.)

## Functions and Classes

N/A - CI workflow only.

## Constraints

- **Triggered only by a version-tag push**, never by an ordinary push or
  pull request - this is a release artifact, not a CI-matrix build
  check; `ci.yml`'s own existing jobs remain the per-push gate.
- **Depends on Story 01.0's `libsmbios` `install()` rules** - without
  them, `cmake --install` only produces `smbios_c`'s artifacts (and the
  `smbios` CLI binary, which has no `install()` rule of its own either -
  confirm during implementation whether the CLI binary should also gain
  one so it is included in the release tarball; if so, record that as a
  deliberate scope addition to Story 01.0's Task 01, not a silent extra
  here).
- **Must not require any secret beyond the default `GITHUB_TOKEN`** - no
  new credential/signing-key management is introduced by this task.
- Reuses the exact same preset names (`windows`/`linux`/`macos`) and
  `VCPKG_ROOT`-driven toolchain this project's own `ci.yml` already uses
  - no parallel build configuration invented for "release mode."

## Tests

- Covered by Task 03 - the real test is downloading a produced artifact
  on a clean machine and confirming it works, not a local dry run of the
  workflow file's syntax alone.

## Success criteria

- [ ] Pushing a version tag (e.g. a scratch `v0.0.0-test` tag during
      implementation, deleted afterward) triggers the workflow and
      produces three artifacts (one per OS) attached to the
      corresponding GitHub Release (a draft/pre-release during testing).
- [ ] Each artifact contains `smbios_c`'s library/headers/`.pc` file and,
      per Story 01.0, `libsmbios`'s own installed library/headers/
      package config - confirmed by inspecting the archive's contents,
      not just its existence.
- [ ] The workflow does not run on an ordinary push or pull request (no
      change to `ci.yml`'s own existing trigger/behavior).
