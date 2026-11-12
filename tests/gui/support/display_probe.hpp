#pragma once

#include "support/local_only_guard.hpp"

/// @file display_probe.hpp
/// @brief Display + OpenGL availability probe for the local-only GUI tier
/// (milestone 0011, Story 04.0 Task 04). Local-only: guarded by
/// local_only_guard.hpp.

namespace smbios::test::gui {

/// @brief Reports whether this session can open a GLFW window with an
/// OpenGL context of the kind `smbios_gui` asks for.
///
/// Calls `glfwInit()`, sets the same context hints as
/// `smbios_gui/src/main.cpp` (3.2 core forward-compatible on macOS, 3.0
/// elsewhere), creates a small **hidden** window, makes its context
/// current, then destroys it and calls `glfwTerminate()`. GLFW errors are
/// not printed (the error callback is cleared for the probe and restored
/// afterwards). Use it in a case's precondition with skip_unless() before
/// launching anything that needs a display, such as the
/// `smbios_gui_autoexit` twin.
/// @return true if the window and its context could be created.
[[nodiscard]] bool display_and_gl_available();

} // namespace smbios::test::gui
