#include "support/local_only_guard.hpp"

/// @file hidden_window_smoke_test.cpp
/// @brief Local-only smoke of the real GLFW + OpenGL3 + Dear ImGui stack over
/// the live table (milestone 0011, Story 04.0 Task 04).
///
/// - Type: integration (local-only tier, labels `gui;gui-local`). Needs a
///   display and an OpenGL 3 context; nothing is shown on screen.
/// - Scenario: GlfwTestWindow opens a hidden window with `smbios_gui`'s
///   context hints, a Dear ImGui context and both backends; the live table
///   is read as `smbios_gui` reads it; kFrames frames then render the real
///   render_entry_browser() inside a full-viewport window (the shape of
///   `main.cpp`'s render_main_window()), selecting node `frame % size` so
///   every node's field pane is drawn several times through real GL.
/// - Asserts: setup reached Ready; the ID/style/window stacks are the same
///   size before and after render_entry_browser() on every frame (Task 02's
///   S3 oracle, stack_sizes_equal()); the selection index is left as set;
///   kFrames frames completed; teardown made all five shutdown calls in
///   reverse init order; no Dear ImGui context is current afterwards; GLFW
///   reported no error.
/// - Output: counts and status names only (no live data).
/// - Skips: as root (R-19); no display/GL (hidden-window probe).
/// - On failure, first check: GLFW error count (a driver/context problem),
///   then whether a recent ImGui bump changed backend shutdown order.

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <smbios/gui/entry_tree_view.hpp>

#include "support/display_probe.hpp"
#include "support/glfw_test_window.hpp"
#include "support/headless_imgui.hpp"
#include "support/live_table_support.hpp"
#include "support/local_skip.hpp"

namespace utf = boost::unit_test;
using smbios::test::gui::display_and_gl_available;
using smbios::test::gui::GlfwTestWindow;
using smbios::test::gui::LiveGuiModel;
using smbios::test::gui::read_live_gui_model;
using smbios::test::gui::skip_if_root;
using smbios::test::gui::skip_unless;
using smbios::test::gui::stack_size_mismatches;
using smbios::test::gui::stack_sizes_equal;
using smbios::test::gui::TeardownStep;

namespace {

constexpr const char* kCaseName = "HiddenWindowSmoke";

/// Frames to render (task spec).
constexpr int kFrames = 300;

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Normal case: 300 real frames of the entry browser over the live table in
/// a hidden window, then a complete, ordered teardown.
BOOST_AUTO_TEST_CASE(HiddenWindowSmoke,
    *utf::precondition([](utf::test_unit_id) {
        auto not_root = skip_if_root(kCaseName);
        if (!not_root) {
            return not_root;
        }
        return skip_unless(display_and_gl_available(), kCaseName,
                           "no display or OpenGL context (hidden GLFW window creation failed)");
    }))
{
    LiveGuiModel model = read_live_gui_model();
    BOOST_REQUIRE_MESSAGE(!model.read_failed, "GUI model table read: ok -> threw");

    std::vector<TeardownStep> teardown;
    int frames_rendered = 0;
    int unbalanced_frames = 0;
    int max_stack_mismatches = 0;
    int selection_changed_frames = 0;
    int glfw_errors = 0;
    {
        GlfwTestWindow window(GlfwTestWindow::Layers::WithImGui, &teardown);
        BOOST_REQUIRE_MESSAGE(window.ready(), "hidden window setup: Ready -> "
                                                  << smbios::test::gui::status_name(window.status()));
        for (int frame = 0; frame < kFrames; ++frame) {
            const int expected_selection =
                model.nodes.empty() ? 0 : static_cast<int>(static_cast<std::size_t>(frame) % model.nodes.size());
            int selected_index = expected_selection;

            window.begin_frame();
            const ImGuiViewport* const viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                              | ImGuiWindowFlags_NoSavedSettings
                                              | ImGuiWindowFlags_NoBringToFrontOnFocus;
            if (ImGui::Begin("smbios_gui_local_test", nullptr, kFlags)) {
                ImGuiErrorRecoveryState before;
                ImGuiErrorRecoveryState after;
                ImGui::ErrorRecoveryStoreState(&before);
                smbios::gui::render_entry_browser(*model.bios, model.nodes, selected_index);
                ImGui::ErrorRecoveryStoreState(&after);
                if (!stack_sizes_equal(before, after)) {
                    ++unbalanced_frames;
                    const int mismatches = stack_size_mismatches(before, after);
                    max_stack_mismatches = mismatches > max_stack_mismatches ? mismatches : max_stack_mismatches;
                }
            }
            ImGui::End();
            window.end_frame();

            selection_changed_frames += selected_index != expected_selection ? 1 : 0;
            ++frames_rendered;
        }
        glfw_errors = GlfwTestWindow::glfw_error_count();
    }

    BOOST_TEST((frames_rendered == kFrames), "frames rendered: " << kFrames << " -> " << frames_rendered);
    BOOST_TEST((unbalanced_frames == 0), "frames with unbalanced ImGui stacks: 0 -> " << unbalanced_frames
                                             << " (max stacks differing: " << max_stack_mismatches << ")");
    BOOST_TEST((selection_changed_frames == 0),
               "frames whose selection index changed without input: 0 -> " << selection_changed_frames);
    BOOST_TEST((glfw_errors == 0), "GLFW errors: 0 -> " << glfw_errors);

    const std::vector<TeardownStep> expected_teardown = {
        TeardownStep::OpenGl3BackendShutdown, TeardownStep::GlfwBackendShutdown,
        TeardownStep::ImGuiContextDestroyed, TeardownStep::WindowDestroyed, TeardownStep::GlfwTerminated};
    BOOST_TEST((teardown == expected_teardown),
               "teardown calls in reverse init order: " << expected_teardown.size() << " -> " << teardown.size()
                                                        << (teardown.size() == expected_teardown.size()
                                                                ? " (order differs)"
                                                                : ""));
    BOOST_TEST((ImGui::GetCurrentContext() == nullptr), "current ImGui context after teardown: none -> one");
}

BOOST_AUTO_TEST_SUITE_END()
