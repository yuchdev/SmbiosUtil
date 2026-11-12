# Story 01.0 - App Shell: Windowing, GPU Backend and Build Target

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete (2026-11-11)
**Depends on:** Milestone 0009 (`get_fields()`/`IOutputFormatter`, already shipped); [docs/adr/0003-gui-toolkit-for-smbios-visualization.md](/docs/adr/0003-gui-toolkit-for-smbios-visualization.md)

## Scope

This story is the ADR's own "minimal spike" bar, made real as a buildable
CMake target: a new `smbios_gui` executable that opens one GLFW window with
an OpenGL3-backed Dear ImGui context, links `libsmbios` as a whole archive,
and renders one placeholder window - proving the whole-archive link, the
vcpkg-resolved toolkit dependencies, and the window/frame-loop lifecycle all
work on this platform before Story 02.0 writes any real browsing UI against
it. No SMBIOS-specific rendering happens in this story: Story 02.0 owns the
tree/field-table content.

## Tasks

| #  | Document                                                                    | Status         | Blocks |
|----|--------------------------------------------------------------------------------|----------------|--------|
| 01 | [vcpkg dependencies: Dear ImGui and GLFW](01-vcpkg-dependencies-imgui-and-glfw.md) | ✅ Complete | 02     |
| 02 | [`smbios_gui` CMake target and window skeleton](02-smbios-gui-cmake-target-and-window-skeleton.md) | ✅ Complete | 03     |
| 03 | [Tests](03-tests.md)                                                           | ✅ Complete | -      |

## Key constraints

- **Mirrors `smbios`/`smbios_c`'s whole-archive link, exactly.** Task 02's
  CMake target uses the identical three-way `-force_load`/`/WHOLEARCHIVE`/
  `--whole-archive` block `smbios/src/CMakeLists.txt`,
  `smbios_c/CMakeLists.txt` and `examples/CMakeLists.txt` already carry
  (each with the same comment explaining why - `REGISTER_SMBIOS_ENTRY`'s
  and `REGISTER_OUTPUT_FORMATTER`'s static initializers only run for object
  files actually linked in). Do not write a fourth, slightly different
  version of this block; copy the existing one and adjust only the target
  name.
- **One backend pair, not three.** GLFW + OpenGL3 on every platform (see
  `plan.md`'s Key constraints) - no `#ifdef` branch selecting Win32+D3D11 on
  Windows or a Metal backend on macOS. This is a deliberate scope
  reduction, not an oversight: revisit only if a real, encountered
  platform problem (not a theoretical one) requires it.
- **No SMBIOS content in this story.** The window Task 02 produces renders
  a fixed placeholder (e.g. its own title, a "Hello, SMBIOS" label) and
  nothing read from `SMBios`/`get_fields()` - Story 02.0 owns that. This
  keeps Story 01.0's own exit gate ("it builds, links, opens a window, and
  closes cleanly on all three presets") independently verifiable before
  any SMBIOS-specific logic exists to confound it.
- **Follows the `include/smbios/<segment>/` convention.** New headers go
  under `smbios_gui/include/smbios/gui/`, sources flat under
  `smbios_gui/src/` - see `plan.md`'s Key constraints and root `CLAUDE.md`'s
  Architecture section for why the directory name (`smbios_gui`) and the
  header segment name (`smbios/gui/`) differ.
- **`vcpkg.json` is the only place new dependencies are declared.** No
  hand-rolled `FetchContent`/`ExternalProject` fetch of Dear ImGui or GLFW
  source - this project resolves every dependency through the vcpkg
  manifest + `CMakePresets.json`'s toolchain file (root `CLAUDE.md`'s Build
  and test section), and this milestone does not introduce a second
  mechanism.

## Files or directories created or modified by this story

- `vcpkg.json` (gains `imgui` with its `glfw-binding`/`opengl3-binding`
  features, and `glfw3`).
- `smbios_gui/CMakeLists.txt` (new).
- `smbios_gui/include/smbios/gui/` (new directory; empty or near-empty
  after this story - Task 02's window/frame-loop code is simple enough to
  live in `main.cpp` alone, per its own task spec).
- `smbios_gui/src/main.cpp` (new).
- Root `CMakeLists.txt` gains one `add_subdirectory(smbios_gui)` line.
