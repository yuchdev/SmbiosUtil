# 01 - Version sites and release preparation

**Status:** ⬜ Not started

## Objective

Implement [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md) D5 for this milestone's version sites, and resolve the blocking decisions (licence, `.pc` prefix). The consistency *check* itself (`scripts/ci/check_versions.py`, run in `lint` and on tags) is owned by milestone 0014 (SC3). This task only adds rows and derivations.

## Files

- **Modify** root `CMakeLists.txt`. Add `project(... VERSION X.Y.Z)` as the one authoritative value, and derive `SMBIOS_C_VERSION` (`smbios_c/CMakeLists.txt`) and `LIBSMBIOS_VERSION` (Story 01.0) from `PROJECT_VERSION`. This is a product-build change handed to `cpp-expert`.
- **Modify** `scripts/ci/version_sites.json` (owned by 0014). Add `equal` rows for `ports/smbios-c/vcpkg.json` `"version"` and the registry `versions/baseline.json` entry, and `derived` rows for the two CMake variables.
- **Create** `scripts/release/prepare_release.py`. It bumps the authoritative value and every `equal` site from one argument, then runs `check_versions.py`. The registry update itself is Task 03.
- **Create** `LICENSE` (the maintainer's chosen licence), per review finding H3.
- **Modify** `smbios_c/smbios_c.pc.in` (or the install step) so that the installed `.pc` is relocatable, per finding H1. This needs an explicit exception to plan.md's "pc unchanged" constraint, recorded in `status.md`.

## Success criteria

- [ ] `check_versions.py` (0014) passes on the tree, and fails with a discrepancy line when any one site is edited alone.
- [ ] An extracted archive's `pkg-config --cflags --libs smbios_c` works from any extraction directory with no configure-time prefix present.
- [ ] `LICENSE` exists and the port's copyright step resolves it.
