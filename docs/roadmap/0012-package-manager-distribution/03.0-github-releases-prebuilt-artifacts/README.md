# Story 03.0 - GitHub Releases Prebuilt Artifacts (Windows/Linux/macOS)

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`libsmbios` must be `install()`-able for the
tarball to be complete - see `plan.md`'s Dependency graph note)

## Scope

Implement ADR 0004's adopted **parallel baseline** (the ADR's own
Alternative 3, "GitHub Releases of prebuilt tarballs/zips + `cmake
--install`"): a CI job that, on a tagged release, builds and installs
this project on all three platforms and attaches a per-platform
tarball/zip to the corresponding GitHub Release - serving any consumer
unwilling to add a vcpkg registry at all.

## Tasks

| #  | Document                                                             | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Release workflow and packaging](01-release-workflow-and-packaging.md) | ⬜ Not started | 02     |
| 02 | [Artifact naming, checksums, and versioning](02-artifact-naming-checksums-and-versioning.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                  | ⬜ Not started | -      |

## Key constraints

- **Reuses `cmake --install` output only - no second packaging
  implementation.** Per ADR 0004's own framing of this option, the
  release job is a thin wrapper around the exact same `cmake --install`
  path Story 01.0 (`libsmbios`) and milestone 0007 (`smbios_c`) already
  produce and verify; it does not hand-assemble a tarball from
  individually-copied files.
- **Not a substitute for the vcpkg port (Story 02.0).** Per the ADR's own
  Consequences, this baseline gives no dependency-graph integration (a
  consumer's build system does not learn about `smbios_c`'s own
  dependencies automatically) - it exists alongside Story 02.0, not
  instead of it.
- **No semantic-versioning policy is introduced.** Per the milestone's
  Key constraints, this task needs just enough of a naming/tagging
  convention to produce a reproducible, collision-free filename per
  release - not a full semver policy for either `libsmbios` or
  `smbios_c`.
- **The per-platform artifact matrix (Windows/Linux/macOS x this
  project's own compiler) is this project's own CI burden**, per the
  ADR's own named Negative consequence - record what triggers a rebuild
  (a new tag only, not every push) so this does not become an
  every-push cost.

## Files or directories created or modified by this story

- `.github/workflows/release.yml` (new) or a new job added to the
  existing `.github/workflows/ci.yml`, gated on a version-tag push
  (implementation's choice - a separate workflow file is likely cleaner
  given the existing file's own size and its unrelated `on: [push,
  pull_request]` trigger).
- Documentation (exact location decided during implementation, e.g. a
  new `docs/dev/release-artifacts.md` or a section in
  `docs/cli-usage.md`) describing the artifact naming convention and how
  to consume a release tarball without any package manager.
