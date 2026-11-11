#pragma once

#include "support/local_only_guard.hpp"

#include <string_view>
#include <vector>

/// @file glfw_test_window.hpp
/// @brief RAII hidden GLFW window (+ optional Dear ImGui context and the
/// GLFW/OpenGL3 backends) for the local-only GUI tier (milestone 0011,
/// Story 04.0 Task 04). Local-only: guarded by local_only_guard.hpp.
///
/// Callers decide whether a display exists first, with
/// display_and_gl_available() (support/display_probe.hpp) in the case's
/// precondition; this class then owns one real window for the case body:
/// `HiddenWindowSmoke` renders the live table through it, and
/// `ClipboardRoundTripMatchesCli` uses it as the owner of the OS clipboard
/// (X11 needs a window to own a selection).

struct GLFWwindow;

namespace smbios::test::gui {

/// @brief One teardown call made by ~GlfwTestWindow(), recorded in order so
/// a test can assert that every call was reached, in reverse init order.
enum class TeardownStep {
    /// `ImGui_ImplOpenGL3_Shutdown()`.
    OpenGl3BackendShutdown,
    /// `ImGui_ImplGlfw_Shutdown()`.
    GlfwBackendShutdown,
    /// `ImGui::DestroyContext()`.
    ImGuiContextDestroyed,
    /// `glfwDestroyWindow()`.
    WindowDestroyed,
    /// `glfwTerminate()`.
    GlfwTerminated,
};

/// @brief Owns `glfwInit()`, one **hidden** window with a current OpenGL
/// context (the context hints of `smbios_gui/src/main.cpp`), and, with
/// Layers::WithImGui, a Dear ImGui context plus the GLFW and OpenGL3
/// backends. The destructor undoes exactly the steps that succeeded, in
/// reverse order, and records each call in the optional teardown log.
///
/// Settings that differ from `smbios_gui` on purpose:
/// - `GLFW_VISIBLE` false: nothing is shown on screen (live data stays
///   off-screen, threat model T-11);
/// - `glfwSwapInterval(0)`: a hidden window with vsync can stall on macOS;
/// - GLFW errors are counted (glfw_error_count()), never printed; the
///   previous error callback is restored on destruction;
/// - `io.IniFilename` and `io.LogFilename` are null: no `imgui.ini` or log
///   file in the CTest working directory.
///
/// Not copyable or movable. Only one instance may exist at a time (GLFW
/// and Dear ImGui are process-global).
class GlfwTestWindow {
public:
    /// What to set up on top of the window.
    enum class Layers {
        /// GLFW + hidden window + current context only (clipboard use).
        WindowOnly,
        /// Also a Dear ImGui context and the GLFW + OpenGL3 backends.
        WithImGui,
    };

    /// Outcome of construction: Ready, or the first step that failed.
    enum class Status {
        Ready,
        GlfwInitFailed,
        WindowCreationFailed,
        ImGuiGlfwBackendInitFailed,
        ImGuiOpenGl3BackendInitFailed,
    };

    /// @brief Runs the setup steps in order, stopping at the first failure.
    /// @param layers what to create on top of the window.
    /// @param teardown_log if non-null, every teardown call the destructor
    ///        makes is appended to it, in call order; must outlive this object.
    explicit GlfwTestWindow(Layers layers, std::vector<TeardownStep>* teardown_log = nullptr);
    /// @brief Tears down every step that succeeded, in reverse order.
    ~GlfwTestWindow();
    GlfwTestWindow(const GlfwTestWindow&) = delete;
    GlfwTestWindow& operator=(const GlfwTestWindow&) = delete;
    GlfwTestWindow(GlfwTestWindow&&) = delete;
    GlfwTestWindow& operator=(GlfwTestWindow&&) = delete;

    /// @return Status::Ready if every requested step succeeded.
    [[nodiscard]] Status status() const noexcept { return status_; }
    /// @return true iff status() is Status::Ready.
    [[nodiscard]] bool ready() const noexcept { return status_ == Status::Ready; }
    /// @return the window; null unless window creation succeeded.
    [[nodiscard]] GLFWwindow* window() const noexcept { return window_; }

    /// @brief Starts one frame: `glfwPollEvents()`, both backends'
    /// `NewFrame()`, then `ImGui::NewFrame()`. Requires Layers::WithImGui
    /// and ready().
    void begin_frame();
    /// @brief Ends the frame: `ImGui::Render()`, clears the framebuffer,
    /// `ImGui_ImplOpenGL3_RenderDrawData()`, `glfwSwapBuffers()`.
    void end_frame();

    /// @return how many GLFW errors were reported while this object existed.
    [[nodiscard]] static int glfw_error_count() noexcept;

private:
    std::vector<TeardownStep>* teardown_log_ = nullptr;
    Status status_ = Status::GlfwInitFailed;
    GLFWwindow* window_ = nullptr;
    bool glfw_initialized_ = false;
    bool imgui_context_ = false;
    bool glfw_backend_ = false;
    bool opengl3_backend_ = false;
    void (*previous_error_callback_)(int, const char*) = nullptr;
};

/// @brief Literal name of a GlfwTestWindow::Status, for failure messages.
/// @param status the status to name.
/// @return a static string such as `"GlfwInitFailed"`.
[[nodiscard]] std::string_view status_name(GlfwTestWindow::Status status) noexcept;

} // namespace smbios::test::gui
