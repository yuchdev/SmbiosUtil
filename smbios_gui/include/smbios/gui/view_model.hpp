#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <libsmbios/field_value.hpp>

/// @file view_model.hpp
/// @brief smbios_gui's renderer-neutral view model (milestone 0011 contract
/// GC1, Story 02.0 Task 01).
///
/// The pure-data layer between libsmbios's entry model and the Dear ImGui
/// rendering code: build_entry_tree() turns one live smbios::SMBios into a
/// flat list of EntryNode values, and display_text() is the on-screen
/// projection of a firmware string. Neither this header nor its .cpp
/// includes any Dear ImGui header, so the whole layer is unit-testable with
/// no window, GPU context or GUI library linked in (it builds into the
/// GUI-free `libsmbios_gui_model` library, see smbios_gui/CMakeLists.txt).
///
/// Ownership (threat model `docs/security/2026-11-11-gui-browsing-ui.md`,
/// G-7): every value in the view model is a copy - an EntryNode borrows
/// nothing from the SMBios it was built from, so the nodes may outlive it.
/// That does *not* hold for AbstractSMBiosEntry objects: an entry created by
/// SMBiosEntryFactory::create() points into SMBios's own table buffer, so any
/// such object (e.g. one kept for "Copy as...", Task 03) must be destroyed
/// before its SMBios is - declare the SMBios before every entry container.
///
/// Raw vs. displayed text (G-4): FieldRow keeps the raw bytes get_fields()
/// returned, unmodified, so the clipboard and any formatter see exactly the
/// firmware's data. Renderers must pass every firmware-derived string through
/// display_text() before it reaches the screen.

namespace smbios {
class SMBios;
} // namespace smbios

namespace smbios::gui {

/// @brief One row of an entry's field table - a direct copy of one
/// smbios::NamedField, plus nothing else.
///
/// No GUI-specific decoding: which FieldValue alternative is shown how is the
/// renderer's (Task 02's) decision, via std::visit.
struct FieldRow {
    /// Field name, copied verbatim from NamedField::name.
    std::string name;
    /// Field value, copied verbatim (raw, unsanitized) from NamedField::value.
    smbios::FieldValue value;
};

/// @brief One tree node: one DMIHeader, labeled, with its decoded fields.
///
/// A structure with no registered entry class (SMBiosEntryFactory::create()
/// returned nullptr) or whose decoding threw still becomes a node, with an
/// empty `fields` list - never silently dropped.
struct EntryNode {
    /// DMIHeader::type, the raw SMBIOS structure type.
    size_t type = 0;
    /// DMIHeader::handle. Not unique on a hostile table - never use it as a
    /// widget ID (threat model G-2).
    uint16_t handle = 0;
    /// entry_type_label(type): a fixed string, never firmware-derived.
    std::string label;
    /// The entry's get_fields() result, copied in the same order; empty if
    /// the type is unregistered or decode_failed is true.
    std::vector<FieldRow> fields;
    /// True iff a registered entry class threw (anything but std::bad_alloc)
    /// while being created or while producing its fields; `fields` is then
    /// empty. The exception's message is deliberately not kept (G-6).
    bool decode_failed = false;
};

/// @brief Builds one EntryNode per structure in `bios`, in SMBios iteration
/// order.
///
/// Pure function: no Dear ImGui call, no I/O beyond what SMBios/
/// SMBiosEntryFactory/get_fields() already do. Each structure's create() +
/// get_fields() is exception-isolated: if either throws anything other than
/// std::bad_alloc, that structure still yields exactly one node, with empty
/// `fields` and `decode_failed` set (threat model G-6).
/// @param bios the table to walk; it is only read during this call. Every
/// node is a copy, so the result does not borrow from `bios` and may outlive
/// it. Entry objects created internally are destroyed before this returns.
/// @return one node per structure - always exactly
/// `std::distance(bios.begin(), bios.end())` nodes, in the same order;
/// empty for an empty table.
/// @throws std::bad_alloc only.
std::vector<EntryNode> build_entry_tree(smbios::SMBios& bios);

/// @brief Looks up the human-readable label for an SMBios::SMBiosHandler
/// type value (e.g. 0 -> "BIOS Information"), or "Unknown (0xNN)" for a
/// type with no name in that enum.
///
/// Unnamed types include OEM-specific types 0x80-0xFF, obsolete spec types
/// (5, 6, 10) and any future spec type this project has not named yet. The
/// named labels match the corresponding entry classes' get_type() text.
/// @param type the raw DMIHeader::type value.
/// @return the label text, never empty; `NN` is uppercase hex, at least two
/// digits.
std::string entry_type_label(size_t type);

/// @brief Maximum size in bytes of display_text()'s result, truncation
/// suffix included (threat model G-3).
inline constexpr size_t kDisplayTextMaxBytes = 4096;

/// @brief The on-screen projection of one firmware-derived string (threat
/// model G-3 and G-4). ImGui-free; the view model itself never calls it.
///
/// Steps, in order:
/// 1. smbios::sanitize_utf8(): every invalid UTF-8 sequence becomes U+FFFD.
/// 2. Each code point in C0 (U+0000-U+001F, including TAB, LF and CR), DEL
///    (U+007F), C1 (U+0080-U+009F), U+2028/U+2029 (line/paragraph
///    separators), U+202A-U+202E and U+2066-U+2069 (bidi embeddings,
///    overrides and isolates) is replaced by U+FFFD.
/// 3. If the result exceeds kDisplayTextMaxBytes, it is cut at a code-point
///    boundary and ` ... [N bytes truncated]` is appended, where N is the
///    number of bytes of `raw` (the *input*, before steps 1-2) whose
///    projection was dropped. For input that steps 1-2 leave unchanged
///    (valid UTF-8 with no replaced code point) that equals the number of
///    step-2 bytes dropped; when replacement changed the size it does not,
///    by design - see "Bounded cost". The cut leaves room for the suffix, so
///    the final result never exceeds kDisplayTextMaxBytes.
///
/// Bounded cost (security review P-1, CWE-400): this runs for every visible
/// row every frame, so steps 1-3 are applied incrementally and stop as soon
/// as the cap is exceeded - at most about kDisplayTextMaxBytes bytes of
/// `raw` are ever read, whatever its size. The input is consumed in chunks
/// (one byte plus up to three following UTF-8 continuation bytes) at which
/// sanitize_utf8() always restarts decoding, so the kept text is byte for
/// byte what whole-string processing would produce; the cut falls on a chunk
/// boundary, which for invalid UTF-8 may keep a few bytes less than the
/// last code-point boundary would. Counting N in step-2 bytes would require
/// reading the whole input, hence N counts input bytes.
///
/// Because the result is valid UTF-8, contains no replaced code point and
/// fits the cap, display_text(display_text(s)) == display_text(s).
/// @param raw arbitrary input bytes (e.g. a FieldRow's std::string value).
/// @return valid UTF-8, at most kDisplayTextMaxBytes bytes, with no C0, DEL,
/// C1, line/paragraph separator or bidi control code point.
/// @throws std::bad_alloc only.
std::string display_text(const std::string& raw);

} // namespace smbios::gui
