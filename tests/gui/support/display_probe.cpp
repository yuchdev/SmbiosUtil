#include "support/local_only_guard.hpp"

/// @file display_probe.cpp
/// @brief Implementation of display_and_gl_available() (local-only GUI tier).

#include "support/display_probe.hpp"

#include <GLFW/glfw3.h>

namespace smbios::test::gui {

bool display_and_gl_available() {
    const GLFWerrorfun previous_callback = glfwSetErrorCallback(nullptr);
    bool available = false;
    if (glfwInit() == GLFW_TRUE) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        // Same context hints as smbios_gui/src/main.cpp's
        // request_gl_context_hints().
#if defined(__APPLE__)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
        GLFWwindow* const window = glfwCreateWindow(64, 64, "smbios_gui display probe", nullptr, nullptr);
        if (window != nullptr) {
            glfwMakeContextCurrent(window);
            available = glfwGetCurrentContext() == window;
            glfwMakeContextCurrent(nullptr);
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }
    glfwSetErrorCallback(previous_callback);
    return available;
}

} // namespace smbios::test::gui
