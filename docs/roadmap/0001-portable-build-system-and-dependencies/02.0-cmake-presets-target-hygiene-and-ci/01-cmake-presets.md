# 01 - CMake presets

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`vcpkg.json`)

## Objective

Add `CMakePresets.json` with one configure preset per target platform
(`windows`, `linux`, `macos`) plus a platform-agnostic `default` that
inherits the host's toolchain file, each wired to vcpkg via
`CMAKE_TOOLCHAIN_FILE`, and a matching build preset for each.

## Files

- **Create** `CMakePresets.json`:
  ```json
  {
    "version": 6,
    "configurePresets": [
      {
        "name": "default",
        "hidden": true,
        "binaryDir": "${sourceDir}/build/${presetName}",
        "cacheVariables": {
          "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        }
      },
      { "name": "windows", "inherits": "default", "generator": "Visual Studio 17 2022" },
      { "name": "linux",   "inherits": "default", "generator": "Ninja" },
      { "name": "macos",   "inherits": "default", "generator": "Ninja" }
    ],
    "buildPresets": [
      { "name": "windows", "configurePreset": "windows" },
      { "name": "linux",   "configurePreset": "linux" },
      { "name": "macos",   "configurePreset": "macos" }
    ]
  }
  ```
  (Exact generator names/options to be confirmed against each CI runner
  image during Task 03.)

## Functions and Classes

N/A.

## Constraints

- `VCPKG_ROOT` is read from the environment, not hardcoded - CI sets it
  after checking out vcpkg (or using a cached install); local developers set
  it once in their shell profile.
- `-DGIT_TAG=ON` stays an explicit opt-in flag passed on top of a preset
  (`cmake --preset linux -DGIT_TAG=ON`), not baked into any preset's
  `cacheVariables` - presets must not change its documented off-by-default
  behavior.

## Success criteria

- [ ] `cmake --preset linux && cmake --build --preset linux` succeeds from a
      clean checkout with only `VCPKG_ROOT` set in the environment.
- [ ] The same is true for `windows` and `macos` on their respective hosts.
- [ ] `docs/dev/building.md` (or CLAUDE.md's "Build and test" section) is
      updated to document presets as the primary build path, with the raw
      `cmake -D...` invocation kept only as a fallback note.
