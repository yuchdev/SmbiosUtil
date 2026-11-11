/// @file main.cpp
/// @brief smbios_gui entry point (milestone 0011, ADR 0003): reads the live
/// SMBIOS table once, then browses it in a GLFW + OpenGL3 + Dear ImGui window.
///
/// Startup reads the table and builds the view model (build_entry_tree())
/// exactly once, before any window exists and outside the frame loop, inside
/// a try/catch (threat model `docs/security/2026-11-11-gui-browsing-ui.md`,
/// G-3/G-6). Each frame then only renders that prebuilt model through
/// render_entry_browser() (Story 02.0 Task 02) until the user closes the
/// window or presses Escape. If no source could read a table, the window
/// shows a fixed "no SMBIOS table could be read" view instead of an empty
/// tree (G-9). On POSIX, running as root shows a persistent warning banner
/// (G-8); the program never advises elevating privileges.
///
/// Lifecycle: each initialization step is owned by a small RAII guard
/// declared in initialization order, so the guards' destructors run the
/// matching shutdown calls exactly once, in exact reverse order
/// (ImGui_ImplOpenGL3_Shutdown, ImGui_ImplGlfw_Shutdown,
/// ImGui::DestroyContext, glfwDestroyWindow, glfwTerminate) - on the normal
/// exit path and on every early-failure path alike.
///
/// Exit status: 0 on a normal close, 1 if reading the table or any
/// initialization step fails (a fixed message naming the failing step goes
/// to stderr; no firmware data or exception text is ever printed).

#include <cstdio>
#include <memory>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <libsmbios/smbios.hpp>
#include <smbios/gui/entry_tree_view.hpp>
#include <smbios/gui/view_model.hpp>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 720;

/// @brief GLFW error callback: reports the error code and description on
/// stderr. Registered before glfwInit() so init failures are reported too.
void report_glfw_error(int error, const char* description) {
    std::fprintf(stderr, "smbios_gui: GLFW error %d: %s\n", error,
                 description != nullptr ? description : "(no description)");
}

/// @brief Owns a successful glfwInit(); calls glfwTerminate() exactly once.
struct GlfwLibrary {
    GlfwLibrary() = default;
    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;
    ~GlfwLibrary() { glfwTerminate(); }
};

/// @brief Deleter giving std::unique_ptr ownership of a GLFWwindow.
struct GlfwWindowDeleter {
    void operator()(GLFWwindow* window) const noexcept { glfwDestroyWindow(window); }
};

/// @brief Exclusive owner of the application window.
using GlfwWindowPtr = std::unique_ptr<GLFWwindow, GlfwWindowDeleter>;

/// @brief Owns the Dear ImGui context; calls ImGui::DestroyContext() once.
struct ImGuiContextGuard {
    ImGuiContextGuard() { ImGui::CreateContext(); }
    ImGuiContextGuard(const ImGuiContextGuard&) = delete;
    ImGuiContextGuard& operator=(const ImGuiContextGuard&) = delete;
    ~ImGuiContextGuard() { ImGui::DestroyContext(); }
};

/// @brief Owns a successful ImGui_ImplGlfw_InitForOpenGL(); calls
/// ImGui_ImplGlfw_Shutdown() once.
struct ImGuiGlfwBackendGuard {
    ImGuiGlfwBackendGuard() = default;
    ImGuiGlfwBackendGuard(const ImGuiGlfwBackendGuard&) = delete;
    ImGuiGlfwBackendGuard& operator=(const ImGuiGlfwBackendGuard&) = delete;
    ~ImGuiGlfwBackendGuard() { ImGui_ImplGlfw_Shutdown(); }
};

/// @brief Owns a successful ImGui_ImplOpenGL3_Init(); calls
/// ImGui_ImplOpenGL3_Shutdown() once.
struct ImGuiOpenGl3BackendGuard {
    ImGuiOpenGl3BackendGuard() = default;
    ImGuiOpenGl3BackendGuard(const ImGuiOpenGl3BackendGuard&) = delete;
    ImGuiOpenGl3BackendGuard& operator=(const ImGuiOpenGl3BackendGuard&) = delete;
    ~ImGuiOpenGl3BackendGuard() { ImGui_ImplOpenGL3_Shutdown(); }
};

/// @brief Requests the OpenGL context version and returns the matching
/// GLSL version string for ImGui_ImplOpenGL3_Init().
///
/// macOS only provides a 3.2+ context as a forward-compatible core profile
/// (GLSL 150); other platforms use GL 3.0 / GLSL 130, matching the upstream
/// Dear ImGui GLFW+OpenGL3 example. This selects context hints only - the
/// backend pair is GLFW + OpenGL3 everywhere.
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

/// Fixed text of the running-as-root warning banner (threat model G-8). It
/// deliberately suggests no privilege change of any kind.
constexpr const char* kRootWarningText =
    "Warning: smbios_gui is running as root. Running a desktop GUI with root "
    "privileges is not recommended.";

/// Colour of the running-as-root warning banner.
constexpr ImVec4 kRootWarningColor(1.00F, 0.80F, 0.20F, 1.00F);

/// @brief Reports whether the process runs with root's effective user ID.
/// @return true iff geteuid() == 0 on POSIX; always false on Windows.
bool running_as_root() {
#if !defined(_WIN32)
    return geteuid() == 0;
#else
    return false;
#endif
}

/// @brief Renders one frame's content: a window filling the main viewport,
/// holding the optional root banner and the entry browser.
/// @param bios the table `nodes` was built from (read by the "Copy as..."
/// action on click only).
/// @param nodes the prebuilt view model; empty if no table could be read.
/// @param[in,out] selected_index the browser's selection, see
/// smbios::gui::render_entry_browser().
/// @param show_root_warning true to show the running-as-root banner.
void render_main_window(smbios::SMBios& bios, const std::vector<smbios::gui::EntryNode>& nodes,
                        int& selected_index, bool show_root_warning) {
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                      | ImGuiWindowFlags_NoSavedSettings
                                      | ImGuiWindowFlags_NoBringToFrontOnFocus;
    // Begin() must be paired with End() whatever it returns.
    if (ImGui::Begin("smbios_gui", nullptr, kFlags)) {
        if (show_root_warning) {
            ImGui::PushStyleColor(ImGuiCol_Text, kRootWarningColor);
            ImGui::TextUnformatted(kRootWarningText);
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
        // Empty view decided on nodes.empty() inside (threat model G-9).
        smbios::gui::render_entry_browser(bios, nodes, selected_index);
    }
    ImGui::End();
}

} // namespace

/// @brief Initializes GLFW/OpenGL/Dear ImGui, runs the frame loop until the
/// window should close, then tears everything down in reverse order.
/// @return 0 on a normal close, 1 if any initialization step fails.
int main() {
    // Read the table and build the view model once, before any window and
    // outside the frame loop (G-3, G-6). `bios` outlives the frame loop so a
    // later "Copy as..." action can create entries from it (G-7); SMBios's
    // constructor yields an empty table, not an exception, when no source
    // succeeds - that case is the nodes.empty() view, not an error. `bios`
    // is declared first, so it is destroyed last: the only entry objects
    // created from it after this block are scoped to a Copy click handler.
    std::unique_ptr<smbios::SMBios> bios;
    std::vector<smbios::gui::EntryNode> nodes;
    try {
        bios = std::make_unique<smbios::SMBios>();
        nodes = smbios::gui::build_entry_tree(*bios);
    } catch (...) {
        std::fprintf(stderr, "smbios_gui: failed to read the SMBIOS table\n");
        return 1;
    }
    const bool show_root_warning = running_as_root();
    int selected_index = 0;

    glfwSetErrorCallback(report_glfw_error);
    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "smbios_gui: glfwInit() failed\n");
        return 1;
    }
    const GlfwLibrary glfw_library;

    const char* const glsl_version = request_gl_context_hints();

    const GlfwWindowPtr window(glfwCreateWindow(kInitialWindowWidth, kInitialWindowHeight,
                                                "smbios_gui", nullptr, nullptr));
    if (!window) {
        std::fprintf(stderr, "smbios_gui: glfwCreateWindow() failed\n");
        return 1;
    }
    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(1); // vsync: avoid spinning a CPU core on an idle window

    IMGUI_CHECKVERSION();
    const ImGuiContextGuard imgui_context;
    // No imgui.ini: this tool has no layout worth persisting yet, and it
    // must not drop a stray file into whatever directory it is run from.
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window.get(), true)) {
        std::fprintf(stderr, "smbios_gui: ImGui_ImplGlfw_InitForOpenGL() failed\n");
        return 1;
    }
    const ImGuiGlfwBackendGuard imgui_glfw_backend;

    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::fprintf(stderr, "smbios_gui: ImGui_ImplOpenGL3_Init() failed\n");
        return 1;
    }
    const ImGuiOpenGl3BackendGuard imgui_opengl3_backend;

    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
        glfwPollEvents();
        // Minimized (zero-size framebuffer): nothing to render, so block
        // until the next event instead of spinning the frame loop.
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window.get(), &framebuffer_width, &framebuffer_height);
        if (framebuffer_width <= 0 || framebuffer_height <= 0) {
            glfwWaitEvents();
            continue;
        }
        if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_main_window(*bios, nodes, selected_index, show_root_warning);

        ImGui::Render();
        glfwGetFramebufferSize(window.get(), &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.10F, 0.10F, 0.12F, 1.00F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }

    // Guards are destroyed here in reverse declaration order:
    // OpenGL3 backend, GLFW backend, ImGui context, window, GLFW library;
    // then the view model and, last, the SMBios it was built from.
    return 0;
}
