# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Prove the vcpkg-based configure/build/test cycle actually reproduces
today's build output on all three target platforms, with no manual Boost
path set.

## Files

- No new test source files - this task is a build/CI verification gate, not
  a unit test.

## Functions and Classes

N/A.

## Constraints

- Must be verified on a machine (or CI runner image) with no pre-existing
  Boost installation on the default search path, to prove vcpkg - not a
  system package - is what's actually being linked.

## Success criteria

- [ ] On Windows, Linux and macOS, from a clean checkout: `cmake
      -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
      -B build && cmake --build build` succeeds with zero manual
      `-D*_BOOST_DIR`/`-DBOOST_ROOT` flags.
- [ ] `smbios_functional_test` and `smbios/test/performance_test` binaries
      produced by the vcpkg-based build are run (with the existing
      root/admin requirement) and behave identically to a build from before
      this story.
- [ ] `-DGIT_TAG=ON` (still opt-in, still off by default) is spot-checked to
      confirm this story did not change its behavior.
