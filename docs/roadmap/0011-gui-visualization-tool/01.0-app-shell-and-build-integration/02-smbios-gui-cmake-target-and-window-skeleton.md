# 02 - `smbios_gui` CMake target and window skeleton

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Create the `smbios_gui` executable target: a GLFW window with an
OpenGL3-backed Dear ImGui context, a frame loop that renders one
placeholder window and exits cleanly on window-close, and the same
whole-archive link of `libsmbios` that `smbios`/`smbios_c`/`examples/`
already carry (needed from this task onward, even though no concrete entry
or formatter class is named until Story 02.0, so that Story 02.0 does not
have to also discover and fix a missing-whole-archive bug the way milestone
0007's own verification work did - see that milestone's `status.md`).

## Files

- **Create** `smbios_gui/CMakeLists.txt`:
  ```cmake
  set(TARGET smbios_gui)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

  find_package(imgui CONFIG REQUIRED)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(OpenGL REQUIRED)

  file(GLOB SOURCES src/*.cpp include/smbios/gui/*.hpp)

  add_executable(${TARGET} ${SOURCES})

  target_include_directories(${TARGET} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/libsmbios/include
  )

  target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
  target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

  # Whole-archive link of libsmbios - the identical three-way block
  # smbios/src/CMakeLists.txt, smbios_c/CMakeLists.txt and
  # examples/CMakeLists.txt already carry, and for the identical reason
  # (REGISTER_SMBIOS_ENTRY's and REGISTER_OUTPUT_FORMATTER's static
  # initializers only run for object files actually linked in). See any of
  # those three files' own comment for the full rationale.
  if(APPLE)
      target_link_options(${TARGET} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
  elseif(MSVC)
      target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
  else()
      target_link_options(${TARGET} PRIVATE
          "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
  endif()

  target_link_libraries(${TARGET} PRIVATE
      libsmbios
      imgui::imgui
      glfw
      OpenGL::GL)

  add_dependencies(${TARGET} libsmbios)
  ```
  The exact `find_package`/target names (`imgui::imgui`, `glfw`) must match
  whatever the resolved vcpkg ports actually export - confirm against a
  real configure, and correct this task's spec in `status.md` if they
  differ (vcpkg's own per-port usage documentation, printed at the end of
  `vcpkg install`, is the authoritative source).
- **Create** `smbios_gui/src/main.cpp`: GLFW init, window creation, GL
  context, `ImGui::CreateContext()`, `ImGui_ImplGlfw_InitForOpenGL()`,
  `ImGui_ImplOpenGL3_Init()`, a frame loop (`glfwPollEvents()` ->
  `ImGui_ImplOpenGL3_NewFrame()`/`ImGui_ImplGlfw_NewFrame()`/
  `ImGui::NewFrame()` -> one placeholder `ImGui::Begin("smbios_gui")`/
  `ImGui::Text(...)`/`ImGui::End()` -> `ImGui::Render()` -> swap buffers)
  until `glfwWindowShouldClose()`, then teardown in exact reverse order
  (`ImGui_ImplOpenGL3_Shutdown()`, `ImGui_ImplGlfw_Shutdown()`,
  `ImGui::DestroyContext()`, `glfwDestroyWindow()`, `glfwTerminate()`).
- **Modify** root `CMakeLists.txt`: add `add_subdirectory(smbios_gui)`
  alongside the existing `add_subdirectory(smbios)`/`add_subdirectory(smbios_c)`
  lines - unconditional, built by default on every platform (unlike
  `SMBIOS_BUILD_FUZZERS`/`SMBIOS_BUILD_EXAMPLES`, this target has no opt-out
  flag of its own; Story 03.0 is where any platform-specific gating, if it
  turns out to be needed, would be added).

## Functions and Classes

- `main()` in `smbios_gui/src/main.cpp` - new; this task's only function.
  No SMBIOS-specific type is referenced (Story 02.0 adds those).

## Constraints

- **No SMBIOS content.** Per the story README's Key constraints, this
  task's window renders a fixed placeholder only - no `smbios::SMBios`
  construction, no `#include <libsmbios/smbios.hpp>`. This keeps the
  window/frame-loop lifecycle verifiable in isolation from Story 02.0's
  logic.
- **Clean, deterministic shutdown.** `glfwWindowShouldClose()` becomes true
  on the user clicking the window's close control or (for an unattended/CI
  build-check context) `Escape`; teardown order is exactly the reverse of
  initialization order, with every `ImGui_Impl*_Shutdown()`/
  `ImGui::DestroyContext()`/`glfwDestroyWindow()`/`glfwTerminate()` call
  made exactly once, matching the upstream Dear ImGui GLFW+OpenGL3 example
  this task's frame loop is modeled on.
- **No persistent widget state to manage.** Per `plan.md`'s Key
  constraints, Dear ImGui's immediate-mode model means this task's only
  real lifecycle concern is the window/GL-context/ImGui-context triple
  above - there is no separate "widget tree" to also construct or tear
  down.
- **Headers under `include/smbios/gui/`, matching the segment
  convention.** Even though this task's own code may fit entirely in
  `main.cpp`, create the `smbios_gui/include/smbios/gui/` directory now so
  Story 02.0's view-model/rendering headers have an established home from
  their first commit, matching milestone 0010's own "land the directory
  structure first" reasoning for its `tables/` reorg.

## Tests

- No new automated test in this task - window/frame-loop code that needs a
  real display/GPU context is exactly the kind of thing this milestone's
  testing-scope call (see Task 03 and `plan.md`'s Non-goals) puts outside
  automated CI coverage. Verified by a local, manual run on each
  developer's own machine: the window opens, shows the placeholder text,
  and closes cleanly with no crash or leaked handle (checked with a
  debugger/Instruments/Valgrind at the developer's discretion - not a CI
  gate).

## Success criteria

- [ ] `cmake --build --preset <os>` produces a runnable `smbios_gui`
      binary under `smbios_gui/bin/` on Windows, Linux and macOS.
- [ ] Running it manually opens one window with the placeholder text
      visible, and closing the window exits the process with status 0 and
      no sanitizer/leak-checker complaint.
- [ ] `nm`/`dumpbin`/equivalent confirms at least one `libsmbios` object
      file that is not referenced by `main.cpp` directly (e.g. a concrete
      `AbstractSMBiosEntry` subclass's object) is present in the linked
      binary - the same whole-archive verification milestone 0007's
      `status.md` describes performing for `smbios_c`.
