# Story 02.0 - CMake Presets, Target Hygiene & CI

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** 01.0 (vcpkg manifest must exist before a preset can point at it)

## Scope

Add `CMakePresets.json` so "configure for Windows/Linux/macOS" is a named
preset instead of a remembered command line, tidy the existing per-compiler
flag blocks so they compose cleanly with the new presets, and stand up a CI
workflow (GitHub Actions) that builds and runs the test suite on all three
OSes on every push - the first time this project has had any CI at all.

## Tasks

| #  | Document                                                    | Status         | Blocks |
|----|----------------------------------------------------------------|----------------|--------|
| 01 | [CMake presets](01-cmake-presets.md)                            | ⬜ Not started | 02     |
| 02 | [Target hygiene and warnings](02-target-hygiene-and-warnings.md) | ⬜ Not started | 03     |
| 03 | [CI workflow matrix](03-ci-workflow-matrix.md)                  | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                            | ⬜ Not started | -      |

## Key constraints

- Presets must reference the vcpkg toolchain file from Story 01.0 via
  `CMAKE_TOOLCHAIN_FILE`, not duplicate vcpkg path logic inline.
- CI is additive only in this milestone: it runs the existing
  `smbios_functional_test` suite as-is (still root/admin-gated on
  Linux/Windows where applicable) - making that suite CI-friendly without
  elevated privileges is milestone 0006's job, not this one. Where a test
  genuinely cannot run unprivileged in CI, the workflow may build-only and
  skip execution for that job, with a comment explaining why.

## Files or directories created or modified by this story

- `CMakePresets.json` - new, repo root.
- Root `CMakeLists.txt`, `smbios/CMakeLists.txt`,
  `smbios_util/CMakeLists.txt` - flag/target cleanup only, no new targets.
- `.github/workflows/ci.yml` - new CI workflow.
