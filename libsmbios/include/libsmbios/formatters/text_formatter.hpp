#pragma once
#include <string>

#include <libsmbios/output_formatter.hpp>

namespace smbios {

/// @brief The `text` output format: the legacy human-readable description,
/// byte-identical to AbstractSMBiosEntry::render_to_description()
/// (milestone 0009 R3).
///
/// This is the *only* formatter permitted to bypass get_fields() and call
/// render_to_description() directly (R4's stated exception), so every
/// existing `smbios` CLI invocation keeps its exact output. It adds nothing
/// of its own: the CLI's `DMI version`/`Table size` preamble, its
/// `Header ID = N` lines and the trailing `'\n'` after each entry stay in
/// `smbios/src/main.cpp`. Accordingly, the framing hooks
/// (begin_document()/entry_separator()/end_document()) keep their empty
/// defaults.
///
/// Security note: the output is **not** sanitised. Firmware strings,
/// including ESC/C1 terminal control sequences (CWE-150), reach the output
/// unchanged, because R3 requires byte-identity with render_to_description().
/// This is a pre-existing gap tracked as a post-0009 follow-up. Switching to
/// a structured format does not by itself make output terminal-safe: `xml`
/// and `json` guarantee document well-formedness for any firmware bytes, and
/// `xml` still passes DEL and the C1 controls (e.g. U+009B CSI) through as
/// legal character data. Use `json` when untrusted output may be displayed
/// directly in a terminal: it escapes every C0, DEL and C1 code point
/// (threat model SR-1), leaving only display concerns such as bidi
/// overrides to the consumer.
///
/// Registered as `"text"` via REGISTER_OUTPUT_FORMATTER in
/// text_formatter.cpp. Both members are defined out of line there, so a
/// translation unit that names this class (e.g. constructs it) also links
/// that object file - and with it the registration - into a binary that
/// does not force-load libsmbios (such as `smbios_unit_test`).
class TextFormatter : public IOutputFormatter {
public:
    /// @brief Renders `entry` as its legacy text description.
    /// @param entry the entry to render; only borrowed for the call.
    /// @return exactly `entry.render_to_description()`.
    std::string render(const AbstractSMBiosEntry& entry) const override;

    /// @brief The registry key of this format.
    /// @return `"text"`.
    std::string format_name() const override;
};

} // namespace smbios
