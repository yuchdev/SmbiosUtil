#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <imgui.h>

/// @file headless_imgui.hpp
/// @brief CI-safe headless Dear ImGui harness for the `gui-ci` test tier
/// (milestone 0011, Story 04.0, Task 02). Test code only: never included by
/// smbios_gui/ or libsmbios/ production sources.
///
/// One HeadlessImGuiContext owns one Dear ImGui context with no window, no
/// GL and no OS access: every OS hook is replaced before the first frame
/// (threat model docs/security/2026-11-11-gui-test-tiers.md, R-9) and the
/// renderer side is a "null renderer" that only acknowledges texture
/// requests (spike item S1). Nothing here calls a windowing library or
/// includes an ImGui platform/renderer backend header.
///
/// **Internal-API coupling (accepted for test code only, story README).**
/// headless_imgui.cpp and the tests using this header rely on these
/// `imgui_internal.h` symbols of the pinned Dear ImGui 1.92.8:
/// - `GImGui`, `ImGuiContext::{Windows, FrameCount, LogBuffer,
///   OpenPopupStack, DebugDrawIdConflictsId}`;
/// - `ImGuiWindow::{Name, ID, LastFrameActive, StateStorage}`;
/// - `ImGuiPopupData::PopupId`;
/// - `ImGui::{LogToBuffer, ErrorRecoveryStoreState,
///   ErrorRecoveryTryToRecoverState, ActivateItemByID, GetIDWithSeed}`;
/// - `ImGuiErrorRecoveryState` (all eleven `SizeOf*Stack` members);
/// - `ImHashStr` (the combo popup ID, as `BeginCombo()` derives it).
///
/// A vcpkg `builtin-baseline` bump that changes the imgui version MUST
/// re-run this suite (`ctest -L gui-ci`) on all three OSes before merging:
/// any of the symbols above may change shape between ImGui releases.

struct ImGuiContext;
struct ImGuiWindow;
struct ImGuiErrorRecoveryState;

namespace smbios::test::gui {

/// @brief One element of a widget's ID path below a child window: either a
/// string ID (`PushID("##copy_as")`, a widget label such as `"Hex"`) or an
/// integer scope (`PushID(index)`). Hashed exactly as Dear ImGui hashes it.
using ItemIdPart = std::variant<std::string_view, int>;

/// @brief True iff the two snapshots of Dear ImGui's stack sizes (window,
/// ID, tree, color, style-var, font, focus-scope, group, item-flags,
/// begin-popup and disabled stacks) are identical (spike item S3).
/// @param a snapshot taken by ImGui::ErrorRecoveryStoreState().
/// @param b snapshot taken by ImGui::ErrorRecoveryStoreState().
/// @return whether every stack has the same size in both.
bool stack_sizes_equal(const ImGuiErrorRecoveryState& a, const ImGuiErrorRecoveryState& b);

/// @brief Counts the stacks whose size differs between two snapshots, for a
/// counts-only failure message.
/// @return 0 iff stack_sizes_equal(a, b).
int stack_size_mismatches(const ImGuiErrorRecoveryState& a, const ImGuiErrorRecoveryState& b);

/// @brief Counts non-overlapping occurrences of `needle` in `haystack`.
/// For counts-only assertions on captured text (R-5); returns 0 for an
/// empty needle.
size_t count_occurrences(std::string_view haystack, std::string_view needle);

/// @brief RAII owner of one headless Dear ImGui context (spike items S1/S2).
///
/// Construction (immediately after `ImGui::CreateContext()`, before any
/// frame - threat model R-9):
/// - `io.IniFilename = nullptr`, `io.LogFilename = nullptr`;
/// - `Platform_SetClipboardTextFn`/`Platform_GetClipboardTextFn` -> an
///   in-memory capture owned by this object (never the OS clipboard);
/// - `Platform_OpenInShellFn` -> records the call and returns false (never
///   launches anything);
/// - `Platform_SetImeDataFn = nullptr`; the legacy
///   `io.{Get,Set}ClipboardTextFn` stay null;
/// - `ImGuiBackendFlags_RendererHasTextures`, with every texture request
///   acknowledged after each `Render()` (see run_frame());
/// - a fixed 1280x720 display and a fixed 1/60 s frame time.
///
/// Only one instance may exist at a time: it becomes the current context
/// and its destructor calls `ImGui::DestroyContext()`, leaving
/// `ImGui::GetCurrentContext() == nullptr`. Not copyable or movable.
///
/// Logging is only ever to Dear ImGui's in-memory buffer (`LogToBuffer`).
/// Captured text and clipboard bytes are for in-memory comparison only and
/// must never be printed, logged or written to a file (threat model R-5).
class HeadlessImGuiContext {
public:
    /// Display width in pixels, fixed so layouts are deterministic.
    static constexpr float kDisplayWidth = 1280.0F;
    /// Display height in pixels, fixed so layouts are deterministic.
    static constexpr float kDisplayHeight = 720.0F;
    /// Name (and so ID) of the full-viewport test window every frame opens.
    static constexpr const char* kTestWindowName = "smbios_gui_ci_test_window";

    /// @brief Creates and configures the context (see the class comment).
    /// @throws std::logic_error if a Dear ImGui context is already current
    /// (nesting would let two harnesses share hooks).
    HeadlessImGuiContext();

    /// @brief Destroys the context (`ImGui::DestroyContext()`).
    ~HeadlessImGuiContext();

    HeadlessImGuiContext(const HeadlessImGuiContext&) = delete;
    HeadlessImGuiContext& operator=(const HeadlessImGuiContext&) = delete;
    HeadlessImGuiContext(HeadlessImGuiContext&&) = delete;
    HeadlessImGuiContext& operator=(HeadlessImGuiContext&&) = delete;

    /// @brief Runs one complete frame: `NewFrame()`, the fixed full-viewport
    /// test window (kTestWindowName), `body`, `End()`, `Render()`, then the
    /// null renderer's texture-request handling. Afterwards asserts (Boost.Test,
    /// fixed message) that every OS hook is still the one installed by the
    /// constructor and that `OpenInShell` was never called.
    ///
    /// If `body` throws, the ImGui stacks are unwound to their state before
    /// the test window (error recovery with asserts temporarily off), the
    /// frame is ended, and the exception is rethrown.
    /// @param body widget calls to submit inside the test window.
    void run_frame(const std::function<void()>& body);

    /// @brief Like run_frame(), with `body` wrapped in
    /// `ImGui::LogToBuffer()`/`LogFinish()`.
    ///
    /// Note (spike S4): while logging, Dear ImGui disables clipping - both
    /// `ImGuiListClipper` (every index is stepped) and `ItemAdd()`'s clip
    /// test - so the result contains the text of *every* submitted row,
    /// including rows a normal frame would clip away.
    /// @param body widget calls to submit inside the test window.
    /// @return the captured text. Never print it (R-5).
    std::string run_frame_capturing_text(const std::function<void()>& body);

    /// @brief The bytes most recently given to the clipboard hook (empty if
    /// none). Never print them (R-5).
    const std::string& clipboard() const noexcept { return clipboard_; }

    /// @brief How many times the clipboard set hook was called.
    int clipboard_set_count() const noexcept { return clipboard_set_count_; }

    /// @brief How many times the `OpenInShell` hook was called; must stay 0.
    int open_in_shell_count() const noexcept { return open_in_shell_count_; }

    /// @brief How many frames run_frame()/run_frame_capturing_text() completed.
    int frames_run() const noexcept { return frames_run_; }

    /// @brief True iff every OS hook is still the one the constructor
    /// installed, the legacy clipboard hooks and both file names are still
    /// null, and `OpenInShell` was never called.
    bool hooks_intact() const;

    /// @brief Number of textures Dear ImGui currently tracks
    /// (`ImGuiPlatformIO::Textures`); must stay bounded (S1).
    int texture_count() const;

    /// @brief Number of tracked textures whose status is not `OK` or
    /// `Destroyed` after the last frame's request handling (S1); must be 0.
    int pending_texture_requests() const;

    /// @brief Total number of texture requests acknowledged so far (S1).
    int texture_requests_handled() const noexcept { return texture_requests_handled_; }

    /// @brief Finds the child window, submitted in the last frame, whose own
    /// name segment is `<fragment>_<8 hex digits>` (how `BeginChild()` names
    /// a child, e.g. `##entry_list`). Nested children of that child do not
    /// match.
    /// @param fragment the `BeginChild()` string ID, e.g. `"##entry_fields"`.
    /// @return the window, or nullptr if none or more than one matches.
    ImGuiWindow* find_child_window(std::string_view fragment) const;

    /// @brief Computes the ID of a widget inside a child window, from the
    /// child's own ID seed and the widget's ID path (spike item S5).
    ///
    /// Example: node 3 of the entry list is `{3, "##node"}` under
    /// `"##entry_list"`; the Copy button is `{"##copy_as", "Copy"}` under
    /// `"##entry_fields"`.
    /// @param child_window_fragment see find_child_window().
    /// @param path the PushID scopes and the widget's own label, in order.
    /// @return the ID, or nullopt if the child window is not found or the
    /// path is empty.
    std::optional<ImGuiID> find_item_id(std::string_view child_window_fragment,
                                        std::initializer_list<ItemIdPart> path) const;

    /// @brief The S5 locator: queues an activation of the widget named by
    /// `path` inside `child_window_fragment` (`ImGui::ActivateItemByID()`),
    /// as if it had been clicked; Dear ImGui applies it in the next frame
    /// that submits the widget. Uses no pixel coordinates.
    /// @return false if the widget's ID could not be computed (nothing queued).
    bool activate_item(std::string_view child_window_fragment, std::initializer_list<ItemIdPart> path);

private:
    /// @brief Shared body of run_frame()/run_frame_capturing_text().
    void run_frame_impl(const std::function<void()>& body, std::string* capture);

    /// @brief Acknowledges every pending texture request (the null renderer).
    void handle_texture_requests();

    /// @brief Clipboard set hook: copies `text` into clipboard_.
    static void set_clipboard_hook(ImGuiContext* ctx, const char* text);
    /// @brief Clipboard get hook: returns clipboard_.
    static const char* get_clipboard_hook(ImGuiContext* ctx);
    /// @brief OpenInShell hook: counts the call, returns false.
    static bool open_in_shell_hook(ImGuiContext* ctx, const char* path);

    /// The owned context; never null between construction and destruction.
    ImGuiContext* context_ = nullptr;
    /// In-memory clipboard contents.
    std::string clipboard_;
    /// Calls of set_clipboard_hook().
    int clipboard_set_count_ = 0;
    /// Calls of open_in_shell_hook().
    int open_in_shell_count_ = 0;
    /// Completed frames.
    int frames_run_ = 0;
    /// Texture requests acknowledged.
    int texture_requests_handled_ = 0;
};

} // namespace smbios::test::gui
