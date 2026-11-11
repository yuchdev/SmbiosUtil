#include "support/local_only_guard.hpp"

/// @file glfw_test_window.cpp
/// @brief Implementation of GlfwTestWindow (local-only GUI tier).

#include "support/glfw_test_window.hpp"

#include <atomic>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace smbios::test::gui {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

/// GLFW errors reported while the current GlfwTestWindow exists; reset by
/// its constructor (only one instance exists at a time).
std::atomic<int> g_glfw_errors{0};

/// @brief Counts a GLFW error; deliberately prints nothing.
void count_glfw_error(int /*error*/, const char* /*description*/) {
    g_glfw_errors.fetch_add(1);
}

/// @brief Same context hints as smbios_gui/src/main.cpp's
/// request_gl_context_hints().
/// @return the matching GLSL version string for ImGui_ImplOpenGL3_Init().
const char* request_gl_context_hints() {
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    return "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    return "#version 130";
#endif
}

} // namespace

GlfwTestWindow::GlfwTestWindow(Layers layers, std::vector<TeardownStep>* teardown_log)
    : teardown_log_(teardown_log) {
    g_glfw_errors.store(0);
    previous_error_callback_ = glfwSetErrorCallback(count_glfw_error);

    if (glfwInit() != GLFW_TRUE) {
        status_ = Status::GlfwInitFailed;
        return;
    }
    glfw_initialized_ = true;

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    const char* const glsl_version = request_gl_context_hints();
    window_ = glfwCreateWindow(kWindowWidth, kWindowHeight, "smbios_gui_local_test", nullptr, nullptr);
    if (window_ == nullptr) {
        status_ = Status::WindowCreationFailed;
        return;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(0);

    if (layers == Layers::WithImGui) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imgui_context_ = true;
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
            status_ = Status::ImGuiGlfwBackendInitFailed;
            return;
        }
        glfw_backend_ = true;
        if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
            status_ = Status::ImGuiOpenGl3BackendInitFailed;
            return;
        }
        opengl3_backend_ = true;
    }
    status_ = Status::Ready;
}

GlfwTestWindow::~GlfwTestWindow() {
    const auto record = [this](TeardownStep step) {
        if (teardown_log_ != nullptr) {
            teardown_log_->push_back(step);
        }
    };
    if (opengl3_backend_) {
        ImGui_ImplOpenGL3_Shutdown();
        record(TeardownStep::OpenGl3BackendShutdown);
    }
    if (glfw_backend_) {
        ImGui_ImplGlfw_Shutdown();
        record(TeardownStep::GlfwBackendShutdown);
    }
    if (imgui_context_) {
        ImGui::DestroyContext();
        record(TeardownStep::ImGuiContextDestroyed);
    }
    if (window_ != nullptr) {
        glfwMakeContextCurrent(nullptr);
        glfwDestroyWindow(window_);
        record(TeardownStep::WindowDestroyed);
    }
    if (glfw_initialized_) {
        glfwTerminate();
        record(TeardownStep::GlfwTerminated);
    }
    glfwSetErrorCallback(previous_error_callback_);
}

void GlfwTestWindow::begin_frame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GlfwTestWindow::end_frame() {
    ImGui::Render();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.10F, 0.10F, 0.12F, 1.00F);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

int GlfwTestWindow::glfw_error_count() noexcept {
    return g_glfw_errors.load();
}

std::string_view status_name(GlfwTestWindow::Status status) noexcept {
    switch (status) {
    case GlfwTestWindow::Status::Ready:
        return "Ready";
    case GlfwTestWindow::Status::GlfwInitFailed:
        return "GlfwInitFailed";
    case GlfwTestWindow::Status::WindowCreationFailed:
        return "WindowCreationFailed";
    case GlfwTestWindow::Status::ImGuiGlfwBackendInitFailed:
        return "ImGuiGlfwBackendInitFailed";
    case GlfwTestWindow::Status::ImGuiOpenGl3BackendInitFailed:
        return "ImGuiOpenGl3BackendInitFailed";
    }
    return "Unknown";
}

} // namespace smbios::test::gui
