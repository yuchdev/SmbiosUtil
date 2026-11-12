/// @file headless_imgui.cpp
/// @brief HeadlessImGuiContext and helpers (milestone 0011, Story 04.0,
/// Task 02). See headless_imgui.hpp for the contract and for the list of
/// `imgui_internal.h` symbols this file depends on.

#include "support/headless_imgui.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <boost/test/unit_test.hpp>

namespace smbios::test::gui {

namespace {

/// Window flags of the fixed test window: no decoration, no move/resize, and
/// never part of any settings file (none is configured anyway).
constexpr ImGuiWindowFlags kTestWindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
                                            | ImGuiWindowFlags_NoBringToFrontOnFocus;

/// Arbitrary non-zero texture ID the null renderer hands out; never
/// dereferenced (nothing is drawn).
constexpr ImTextureID kDummyTextureId = 1;

/// @brief The harness that owns `ctx`, via the clipboard user-data slot the
/// constructor set.
HeadlessImGuiContext* owner_of(ImGuiContext* ctx)
{
    return ctx == nullptr ? nullptr : static_cast<HeadlessImGuiContext*>(ctx->PlatformIO.Platform_ClipboardUserData);
}

/// @brief True iff `name`'s last '/'-separated segment is
/// `<fragment>_XXXXXXXX` (8 hex digits) - BeginChild()'s naming scheme
/// `"%s/%s_%08X"` (imgui.cpp, BeginChildEx).
bool is_child_named(std::string_view name, std::string_view fragment)
{
    const size_t slash = name.rfind('/');
    if (slash == std::string_view::npos) {
        return false;
    }
    const std::string_view segment = name.substr(slash + 1);
    constexpr size_t kHexDigits = 8;
    if (segment.size() != fragment.size() + 1 + kHexDigits || segment.substr(0, fragment.size()) != fragment
        || segment[fragment.size()] != '_') {
        return false;
    }
    for (const char c : segment.substr(fragment.size() + 1)) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
        if (!hex) {
            return false;
        }
    }
    return true;
}

} // namespace

bool stack_sizes_equal(const ImGuiErrorRecoveryState& a, const ImGuiErrorRecoveryState& b)
{
    return stack_size_mismatches(a, b) == 0;
}

int stack_size_mismatches(const ImGuiErrorRecoveryState& a, const ImGuiErrorRecoveryState& b)
{
    int mismatches = 0;
    mismatches += a.SizeOfWindowStack != b.SizeOfWindowStack ? 1 : 0;
    mismatches += a.SizeOfIDStack != b.SizeOfIDStack ? 1 : 0;
    mismatches += a.SizeOfTreeStack != b.SizeOfTreeStack ? 1 : 0;
    mismatches += a.SizeOfColorStack != b.SizeOfColorStack ? 1 : 0;
    mismatches += a.SizeOfStyleVarStack != b.SizeOfStyleVarStack ? 1 : 0;
    mismatches += a.SizeOfFontStack != b.SizeOfFontStack ? 1 : 0;
    mismatches += a.SizeOfFocusScopeStack != b.SizeOfFocusScopeStack ? 1 : 0;
    mismatches += a.SizeOfGroupStack != b.SizeOfGroupStack ? 1 : 0;
    mismatches += a.SizeOfItemFlagsStack != b.SizeOfItemFlagsStack ? 1 : 0;
    mismatches += a.SizeOfBeginPopupStack != b.SizeOfBeginPopupStack ? 1 : 0;
    mismatches += a.SizeOfDisabledStack != b.SizeOfDisabledStack ? 1 : 0;
    return mismatches;
}

size_t count_occurrences(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) {
        return 0;
    }
    size_t count = 0;
    for (size_t pos = haystack.find(needle); pos != std::string_view::npos;
         pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

HeadlessImGuiContext::HeadlessImGuiContext()
{
    if (ImGui::GetCurrentContext() != nullptr) {
        throw std::logic_error("HeadlessImGuiContext: a Dear ImGui context is already current");
    }
    context_ = ImGui::CreateContext();

    // Threat model R-9 / T-7: replace every OS hook before the first frame.
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.GetClipboardTextFn = nullptr;
    io.SetClipboardTextFn = nullptr;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_ClipboardUserData = this;
    platform_io.Platform_SetClipboardTextFn = &HeadlessImGuiContext::set_clipboard_hook;
    platform_io.Platform_GetClipboardTextFn = &HeadlessImGuiContext::get_clipboard_hook;
    platform_io.Platform_OpenInShellFn = &HeadlessImGuiContext::open_in_shell_hook;
    platform_io.Platform_OpenInShellUserData = nullptr;
    platform_io.Platform_SetImeDataFn = nullptr;

    // Spike S1: the null renderer. Dear ImGui 1.92 asks the renderer to
    // create/update/destroy the font atlas texture through ImTextureData
    // requests; handle_texture_requests() acknowledges each one.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.BackendRendererName = "smbios_gui_ci_test_null_renderer";
    io.DisplaySize = ImVec2(kDisplayWidth, kDisplayHeight);
    io.DeltaTime = 1.0F / 60.0F;
}

HeadlessImGuiContext::~HeadlessImGuiContext()
{
    ImGui::DestroyContext(context_);
}

void HeadlessImGuiContext::run_frame(const std::function<void()>& body)
{
    run_frame_impl(body, nullptr);
}

std::string HeadlessImGuiContext::run_frame_capturing_text(const std::function<void()>& body)
{
    std::string captured;
    run_frame_impl(body, &captured);
    return captured;
}

void HeadlessImGuiContext::run_frame_impl(const std::function<void()>& body, std::string* capture)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(kDisplayWidth, kDisplayHeight);
    io.DeltaTime = 1.0F / 60.0F;

    ImGui::NewFrame();
    ImGuiErrorRecoveryState before_window;
    ImGui::ErrorRecoveryStoreState(&before_window);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kDisplayWidth, kDisplayHeight));
    ImGui::Begin(kTestWindowName, nullptr, kTestWindowFlags);
    try {
        if (capture != nullptr) {
            ImGui::LogToBuffer();
            body();
            // LogFinish() clears LogBuffer, so copy it first.
            capture->assign(GImGui->LogBuffer.c_str(), static_cast<size_t>(GImGui->LogBuffer.size()));
            ImGui::LogFinish();
        } else {
            body();
        }
    } catch (...) {
        // Leave the context consistent for the destructor: stop logging,
        // unwind every stack to its state before the test window (this also
        // ends the window), and finish the frame. Asserts are off for the
        // recovery only - it reports each unwound stack as a user error.
        ImGui::LogFinish();
        const bool assert_was_enabled = io.ConfigErrorRecoveryEnableAssert;
        io.ConfigErrorRecoveryEnableAssert = false;
        ImGui::ErrorRecoveryTryToRecoverState(&before_window);
        io.ConfigErrorRecoveryEnableAssert = assert_was_enabled;
        ImGui::EndFrame();
        throw;
    }
    ImGui::End();
    ImGui::Render();
    handle_texture_requests();
    ++frames_run_;

    BOOST_TEST(hooks_intact(),
               "HeadlessImGuiContext: an OS hook was replaced or OpenInShell was called (frame "
                   << frames_run_ << ", OpenInShell calls " << open_in_shell_count_ << ")");
}

void HeadlessImGuiContext::handle_texture_requests()
{
    // The same state machine as a real 1.92 renderer backend (e.g.
    // imgui_impl_opengl3's ImGui_ImplOpenGL3_UpdateTexture), minus the GPU.
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
        switch (tex->Status) {
        case ImTextureStatus_WantCreate:
            tex->SetTexID(kDummyTextureId);
            tex->SetStatus(ImTextureStatus_OK);
            ++texture_requests_handled_;
            break;
        case ImTextureStatus_WantUpdates:
            tex->SetStatus(ImTextureStatus_OK);
            ++texture_requests_handled_;
            break;
        case ImTextureStatus_WantDestroy:
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
            ++texture_requests_handled_;
            break;
        case ImTextureStatus_OK:
        case ImTextureStatus_Destroyed:
            break;
        }
    }
}

bool HeadlessImGuiContext::hooks_intact() const
{
    const ImGuiIO& io = context_->IO;
    const ImGuiPlatformIO& platform_io = context_->PlatformIO;
    return platform_io.Platform_SetClipboardTextFn == &HeadlessImGuiContext::set_clipboard_hook
        && platform_io.Platform_GetClipboardTextFn == &HeadlessImGuiContext::get_clipboard_hook
        && platform_io.Platform_OpenInShellFn == &HeadlessImGuiContext::open_in_shell_hook
        && platform_io.Platform_SetImeDataFn == nullptr
        && platform_io.Platform_ClipboardUserData == this
        && io.GetClipboardTextFn == nullptr && io.SetClipboardTextFn == nullptr
        && io.IniFilename == nullptr && io.LogFilename == nullptr
        && open_in_shell_count_ == 0;
}

int HeadlessImGuiContext::texture_count() const
{
    return context_->PlatformIO.Textures.Size;
}

int HeadlessImGuiContext::pending_texture_requests() const
{
    int pending = 0;
    for (const ImTextureData* tex : context_->PlatformIO.Textures) {
        if (tex->Status != ImTextureStatus_OK && tex->Status != ImTextureStatus_Destroyed) {
            ++pending;
        }
    }
    return pending;
}

ImGuiWindow* HeadlessImGuiContext::find_child_window(std::string_view fragment) const
{
    ImGuiWindow* found = nullptr;
    int matches = 0;
    for (ImGuiWindow* window : context_->Windows) {
        if (window->LastFrameActive == context_->FrameCount && window->Name != nullptr
            && is_child_named(window->Name, fragment)) {
            found = window;
            ++matches;
        }
    }
    return matches == 1 ? found : nullptr;
}

std::optional<ImGuiID> HeadlessImGuiContext::find_item_id(std::string_view child_window_fragment,
                                                          std::initializer_list<ItemIdPart> path) const
{
    const ImGuiWindow* window = find_child_window(child_window_fragment);
    if (window == nullptr || path.size() == 0) {
        return std::nullopt;
    }
    // A child window's ID stack starts at its own ID (Begin() resets it);
    // PushID(const char*)/PushID(int) and widget labels then hash exactly
    // as GetIDWithSeed() does.
    ImGuiID id = window->ID;
    for (const ItemIdPart& part : path) {
        if (const auto* text = std::get_if<std::string_view>(&part)) {
            id = ImGui::GetIDWithSeed(text->data(), text->data() + text->size(), id);
        } else {
            id = ImGui::GetIDWithSeed(std::get<int>(part), id);
        }
    }
    return id;
}

bool HeadlessImGuiContext::activate_item(std::string_view child_window_fragment,
                                         std::initializer_list<ItemIdPart> path)
{
    const std::optional<ImGuiID> id = find_item_id(child_window_fragment, path);
    if (!id) {
        return false;
    }
    ImGui::ActivateItemByID(*id);
    return true;
}

void HeadlessImGuiContext::set_clipboard_hook(ImGuiContext* ctx, const char* text)
{
    HeadlessImGuiContext* self = owner_of(ctx);
    if (self == nullptr) {
        return;
    }
    self->clipboard_.assign(text != nullptr ? text : "");
    ++self->clipboard_set_count_;
}

const char* HeadlessImGuiContext::get_clipboard_hook(ImGuiContext* ctx)
{
    const HeadlessImGuiContext* self = owner_of(ctx);
    return self == nullptr ? "" : self->clipboard_.c_str();
}

bool HeadlessImGuiContext::open_in_shell_hook(ImGuiContext* ctx, const char* /*path*/)
{
    // Records the call and refuses: the CI tier never launches anything.
    // The path is deliberately not kept (it could be firmware-derived).
    HeadlessImGuiContext* self = owner_of(ctx);
    if (self != nullptr) {
        ++self->open_in_shell_count_;
    }
    return false;
}

} // namespace smbios::test::gui
