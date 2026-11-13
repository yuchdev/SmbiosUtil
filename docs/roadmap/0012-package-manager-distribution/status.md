# Milestone 0012 - Package Manager Distribution for `libsmbios` and `smbios_c` - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                        | Status         | Tests |
|-------|------------------------------------------------------------------|----------------|-------|
| 01.0  | `libsmbios` install rules and whole-archive-safe package export | ⬜ Not started | - |
| 02.0  | Self-hosted vcpkg overlay/registry port for `smbios_c`           | ⬜ Not started | - |
| 03.0  | GitHub Releases prebuilt artifacts - pipeline `package` stage (ADR 0005); blocked on 0014 | ⬜ Not started | - |
| 04.0  | Release and publish automation - pipeline `verify`/`publish` stages (ADR 0005); blocked on 0014 | ⬜ Not started | - |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- 2026-11-12 Story 04.0 added after the [packaging-automation review](/docs/reviews/milestone-0012-packaging-automation-2026-11-12.md) (H1-H3, M1-M6).
- 2026-11-12 **Amended by [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md)** (Proposed). This follows the maintainer's ruling: "CI, packaging and publishing should be designed as unified subsystem."

  **What changed in this milestone:**
  - **Story 03.0** is now the pipeline's `package` stage. It archives the build stage's tested `cmake --install` tree and does not rebuild. The standalone `release.yml` is withdrawn, because its sketch broke 0014 policy rules P3/P6/P7/P13 and shipped an untested build.
  - **Story 04.0** is now the `verify` and `publish` stages. Its duplicated workflow conventions (permissions, pinning, report contract) were removed in favour of pointers to ADR 0005 and milestone 0014.
  - **Version source decided (D5):** the root CMake `project()` VERSION, checked against the port, the registry and the tag by 0014's `check_versions.py`. This reverses Story 03.0 Task 02's "drift is acceptable".
  - **Artifact name** is now `libbios-<version>-<os>-<arch>.<ext>`, with a `manifest.json` of digests.

  **Dependency fixed:** this milestone's workflow work (Stories 03.0 and 04.0) depends on milestone 0014 Stories 01.0-03.0 plus Story 04.0 Task 02. Stories 01.0 and 02.0 do not.

  **New requirement:** Story 03.0 must name the shipped build configuration, because 0014 C06 tests `-C Debug` today.

  **Still open for the maintainer:** licence (H3), `.pc` relocatability (H1), CLI in the tarball (I2), registry hosting (H2), Windows static triplet (M4).
