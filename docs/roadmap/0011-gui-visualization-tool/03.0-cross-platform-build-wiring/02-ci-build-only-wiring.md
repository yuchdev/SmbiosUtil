# 02 - CI build-only wiring

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete as written (2026-11-11); behaviour on real runners UNVERIFIED until a push

> **Amended 2026-11-11 (maintainer ruling after /pr-review, see `status.md` Notes & decisions):** the GUI build is **opt-in**. `smbios_gui` is no longer built by the existing `build` job's default
presets. This task must instead **add a dedicated GUI build job** (or matrix
entries) that configures/builds the `<os>-gui` presets (or passes
`-DSMBIOS_BUILD_GUI=ON -DVCPKG_MANIFEST_FEATURES=gui`) on all three OSes, with
the Linux X11/GL development packages (`libxinerama-dev`, `libxcursor-dev`,
`xorg-dev`, `libglu1-mesa-dev`, `pkg-config` - confirm the real set by a
reproduced configure failure) installed ahead of `Configure`. The "no other
change to CI" bullet below and the "existing `Build` step already covers it"
reasoning no longer hold; the build-only/no-run constraints and the
`allow_build_failure: false` posture still apply to the new job. The vcpkg
binary-cache key (`hashFiles('vcpkg.json')`) already covers the new feature.

## Objective

Make sure `.github/workflows/ci.yml`'s existing `build` job's `Build` step
(`cmake --build --preset ${{ matrix.preset }}`) actually fails the job if
`smbios_gui` fails to compile or link, on all three OS matrix entries - and
add nothing that tries to *run* `smbios_gui`, per this story's own
testing-scope call.

## Files

- **Modify** `.github/workflows/ci.yml` only if Task 01 found a
  platform-specific system package `glfw3`'s vcpkg port needs that the CI
  image does not already have (e.g. an `apt-get install` step for X11/
  Wayland development headers on `ubuntu-latest`, ahead of the `Configure`
  step) - vcpkg's own binary-cache/port build step would otherwise fail
  the `Configure`/`Build` step anyway, so this is "does the existing job
  fail correctly and informatively," not new CI surface.
- No other change: the `Build` step already runs unconditionally for
  every target `add_subdirectory()`'d from the root `CMakeLists.txt`, and
  Story 01.0 added `smbios_gui` to that list - so no new CI step, matrix
  entry, or job is needed to make CI build it.

## Functions and Classes

None.

## Constraints

- **Build-only. No run step.** Do not add a step that executes
  `smbios_gui` (not even with a short timeout, not even just to check the
  exit code) - see the story README's Key constraints for the display/GPU
  reasoning. If a future milestone wants a headless run check, that is a
  new, separately-scoped CI investigation (see the story README's Open
  questions), not a one-line addition here.
- **Do not add `smbios_gui` to any existing `ctest -R` filter.** It is not
  a `ctest`-registered target (it is a plain executable, like `smbios`
  itself) - the `gui_view_model_test.cpp` cases from Story 02.0 are
  already covered by the existing `smbios_unit_test` filter entries with
  no edit needed.
- **Mirror the existing job's `allow_build_failure: false` posture.**
  `smbios_gui` failing to build is treated exactly like any other target
  failing to build (`matrix.allow_build_failure` is already `false` for
  all three OSes in the `build` job) - this task does not carve out a
  softer failure mode for it.

## Tests

None of this task's own; [Task 03](03-tests.md) is the story's exit gate.

## Success criteria

- [ ] A CI run on a real push shows `smbios_gui`'s object files/link step
      in the `Build` step's own log output, on all three OSes.
- [ ] Deliberately breaking `smbios_gui/src/main.cpp` (e.g. a syntax
      error) in a throwaway branch/PR, pushed once during this task's own
      verification and then reverted, fails the `build` job on all three
      OSes - proving this is a genuine hard gate, not a step CI silently
      skips.
- [ ] No CI step attempts to execute `smbios_gui`.

## As-built notes (2026-11-11)

- **Delivered:** a dedicated build-only `gui-build` job appended to `.github/workflows/ci.yml` (91 added lines, no existing step or `ctest -R` filter changed): ubuntu/windows/macos matrix building `linux-gui`/`windows-gui`/`macos-gui`; Linux apt packages from the vcpkg glfw3 port's own message (`libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config`) installed before configure; same pinned action SHAs as the existing jobs; distinct cache key `vcpkg-binarycache-gui-...`; `timeout-minutes: 30`; `allow_build_failure: false`; no step runs `smbios_gui`. Root `CLAUDE.md` GUI bullet corrected.
- **Bounded output (maintainer priority, preview of the CI-rework milestone):** full configure/build logs go to a temp file; success prints one line (`ok: <path> (<bytes> bytes)`); failure prints one `::error::` line, <=20 lines matching real diagnostic shapes (`error:`, `error C####`, `FAILED:`, `fatal error`, `CMake Error`, `undefined reference`, `linker command failed`, `LNK####`) and the last 40 lines, preserving the exit code. The orchestrator tightened the coder's bare-word `error|Error|FAILED|fatal` pattern because it matched benign file names (`memory_error_information_*.cpp`: 4 false matches on a real clean log -> 0).
- **Criterion 1 reworded:** "smbios_gui object files/link step visible in the log" now reads "the job found and reported the built `smbios_gui` binary (one `ok:` line)" - object files are deliberately not printed.
- **Unverified (needs a real push):** the job on GitHub runners; Windows/Linux `-gui` presets configuring; apt behaviour; a deliberately broken `main.cpp` failing all three OSes.

### CI debugging findings (2026-11-11) - what the first real Linux runs taught

The `gui-build` Linux job failed for four independent reasons, found one at a time (each with a probe or the job's own bounded output), none in the GUI code:

1. **Ubuntu mirror DNS.** On the runners, DNS for `azure.archive` / `archive` / `security.ubuntu.com` failed (24.04 and 26.04 images alike; `mirrors.edge.kernel.org` resolved), so `apt-get update` fetched nothing and `apt-get install` stalled until the timeout. Fix: use the default mirrors when `azure.archive.ubuntu.com` resolves, else the kernel.org Ubuntu mirror (verified: update 6 s, whole install 39 s). Two earlier guesses (a debconf prompt / apt lock, then the 26.04 image) were wrong; the runner is pinned to `ubuntu-24.04` only because the fallback was verified there.
2. **A bug in the bounded-failure script.** GitHub starts `shell: bash` with `-e`; the bare `"$@" > "$log"` exited the step before any diagnostics were printed. Fixed with `|| rc=$?` and re-tested under the exact CI flags (`bash --noprofile --norc -e -o pipefail`); the earlier local test had not used `-e`. Rule: test every workflow `run:` block under the runner's real shell flags.
3. **Missing autotools.** `glfw3` depends on `libx11` on Linux, which vcpkg builds from source with `vcpkg_make`; that needs `autoconf autoconf-archive automake libtool` from the system (see `ports/vcpkg-make/vcpkg_make.cmake`). The glfw3 port's own message - the source of the original five-package list - does not list them. Final Linux package list: `libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config autoconf autoconf-archive automake libtool`.
4. **Step timeouts.** Every apt stall had burned the 30-minute job ceiling; the apt step now has its own 10-minute timeout, short fetch timeouts and one retry.

Verified on real runners: a Linux-only probe ran the same steps and `cmake --preset linux-gui` + `cmake --build --preset linux-gui` both exited 0 and produced `build/linux-gui/smbios_gui/bin/smbios_gui` (6.5 MB); `gui-build` on macOS and Windows passed in two full runs. The probe workflow was deleted afterwards.

