#pragma once
#include <memory>
#include <string>
#include <vector>

#include <libsmbios/registration_name.hpp>

namespace smbios {

class AbstractSMBiosEntry;

/// @brief Output format strategy (milestone 0009 contract C2): renders one
/// typed SMBIOS entry into a named textual format (`text`, `xml`, `json`,
/// ...). Concrete formatters self-register at static-init time via
/// REGISTER_OUTPUT_FORMATTER and are looked up by name through
/// get_output_formatter() - the same shape as the entry registry in
/// smbios_entry_factory.hpp.
///
/// A complete multi-entry document is assembled by the caller as
/// begin_document(), then each render(entry) with entry_separator() between
/// consecutive entries, then end_document(). It must be one well-formed
/// document for zero, one and many entries.
///
/// render_document() does exactly that and is the recommended way to build
/// a document.
///
/// Every member is `const` and one registered instance is shared by every
/// caller. Implementations must never log or persist field values.
///
/// Contract for implementations (including third-party formatters):
/// - render() and the three framing hooks must be deterministic: the same
///   entry (the same get_type()/get_fields() result) always yields the same
///   bytes - no timestamps, host, locale or platform data, and no platform
///   line endings;
/// - render() and the hooks must not throw for any entry, whatever bytes its
///   firmware strings or byte fields hold. Treat every string as untrusted
///   (any byte 0x01-0xFF, not guaranteed UTF-8) and escape it with the
///   shared helpers in libsmbios/formatters/formatter_escaping.hpp
///   (xml_escape(), json_escape(), yaml_double_quoted_escape(),
///   hex_encode()) rather than a hand-written escaper; field_value_to_string()
///   (libsmbios/field_value.hpp) stringifies a FieldValue for formats that
///   carry no type information.
class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;

    /// @brief Renders one entry in this formatter's format.
    /// @param entry the entry to render; only borrowed for the call.
    /// @return the rendered text for `entry` alone, without any document
    /// framing (see begin_document()/entry_separator()/end_document()).
    virtual std::string render(const AbstractSMBiosEntry& entry) const = 0;

    /// @brief The case-sensitive, lowercase registry key of this format
    /// (e.g. `"json"`). Must be stable for the formatter's lifetime.
    /// @return this formatter's unique registered name.
    virtual std::string format_name() const = 0;

    /// @brief Fixed, formatter-owned text emitted once before the first
    /// entry (e.g. an XML declaration and root element, or JSON `[`).
    /// Takes no entry, so it can never carry firmware data.
    /// @return the document prologue; empty by default.
    virtual std::string begin_document() const { return {}; }

    /// @brief Fixed, formatter-owned text emitted between two consecutive
    /// rendered entries (e.g. JSON `,`). Never emitted before the first or
    /// after the last entry. Takes no entry, so it can never carry firmware
    /// data.
    /// @return the inter-entry separator; empty by default.
    virtual std::string entry_separator() const { return {}; }

    /// @brief Fixed, formatter-owned text emitted once after the last entry
    /// (e.g. a closing XML root element, or JSON `]`). Takes no entry, so it
    /// can never carry firmware data.
    /// @return the document epilogue; empty by default.
    virtual std::string end_document() const { return {}; }
};

/// @brief Registers `formatter` under its format_name() in the module-scope
/// formatter registry, which takes ownership of it. Intended to be called
/// once at static-init time, via the REGISTER_OUTPUT_FORMATTER macro, from
/// each concrete formatter's own translation unit - never call this
/// directly from library code outside that macro.
///
/// @warning Static-init only and not thread-safe: the registry is an
/// unsynchronised map, safe only because static initialisation within a
/// module is single-threaded and every later access (get_output_formatter(),
/// registered_output_formatter_names()) only reads. Calling this after
/// `main()` starts while another thread looks up a formatter is a data race
/// (threat model SR-3). A duplicate registration made at static-init time
/// (i.e. through the macro) throws out of a static initialiser, which calls
/// `std::terminate` - intended fail-fast behaviour for a programming error,
/// but in a shared library it aborts the host process at load time.
/// @param formatter the formatter to register; must not be null.
/// @throws std::invalid_argument if `formatter` is null, or if its
/// format_name() is empty.
/// @throws std::logic_error if formatter->format_name() is already
/// registered; a duplicate registration is a programming error (e.g. two
/// formatter classes both claiming the same name), never a silent
/// overwrite. The message names only the format name.
void register_output_formatter(std::unique_ptr<IOutputFormatter> formatter);

/// @brief Looks up a registered formatter by its exact, case-sensitive name.
/// @param name the format name to look up (e.g. `"json"`).
/// @return a non-owning pointer to the registered formatter, valid until
/// static destruction (the registry owns it), or nullptr if `name` is
/// unregistered. Never throws for an unknown name, so a caller (e.g. the
/// `smbios` CLI) can report it alongside registered_output_formatter_names().
const IOutputFormatter* get_output_formatter(const std::string& name);

/// @brief Enumerates every registered format name.
/// @return every registered format name, sorted ascending.
std::vector<std::string> registered_output_formatter_names();

/// @brief Renders a complete document from a range of pointer-like entry
/// handles: begin_document(), then render() of each entry with
/// entry_separator() between two consecutive rendered entries, then
/// end_document().
///
/// Each element of [first, last) is anything that is contextually
/// convertible to `bool` and dereferences to an AbstractSMBiosEntry (or a
/// subclass): a raw `const AbstractSMBiosEntry*`, or a
/// `std::unique_ptr<AbstractSMBiosEntry>` as SMBiosEntryFactory::create()
/// returns, or a `std::shared_ptr`. Null elements (the factory's result for
/// an unregistered structure type) are skipped and never produce a
/// separator, so the separator only ever sits between two rendered entries.
/// Zero (or only null) elements yield begin_document() + end_document().
/// @param formatter the formatter to render with.
/// @param first the first entry handle.
/// @param last one past the last entry handle.
/// @return the complete document. Entries are only borrowed for the call.
template <typename InputIt>
std::string render_document(const IOutputFormatter& formatter, InputIt first, InputIt last)
{
    std::string out = formatter.begin_document();
    bool first_rendered = true;
    for (; first != last; ++first) {
        if (!*first) {
            continue;
        }
        if (!first_rendered) {
            out += formatter.entry_separator();
        }
        out += formatter.render(**first);
        first_rendered = false;
    }
    out += formatter.end_document();
    return out;
}

/// @brief Renders a complete document from borrowed entry pointers; see the
/// iterator overload for the exact framing and the null-skipping rule. For
/// owned entries (a `std::vector<std::unique_ptr<AbstractSMBiosEntry>>`),
/// call the iterator overload with `entries.begin(), entries.end()`.
/// @param formatter the formatter to render with.
/// @param entries the entries, in document order; null elements are skipped.
/// @return the complete document.
std::string render_document(const IOutputFormatter& formatter,
                            const std::vector<const AbstractSMBiosEntry*>& entries);

} // namespace smbios

/// @brief Registers a default-constructed `class_name` (a concrete
/// smbios::IOutputFormatter) under its format_name() at static-init time,
/// via a function-local static map (see output_formatter.cpp) - no edit to
/// output_formatter.{hpp,cpp} is required to add a new format.
/// Place exactly one invocation per formatter at namespace scope in the
/// formatter's own .cpp (never its header - a header-placed invocation
/// would register once per translation unit that includes it, tripping the
/// duplicate-registration guard, same reasoning as REGISTER_SMBIOS_ENTRY).
/// `class_name` may be namespace-qualified (e.g. `vendor::CsvFormatter`):
/// the registration variable's name comes from SMBIOS_DETAIL_UNIQUE_NAME,
/// not from pasting `class_name`.
///
/// Static-init only and not thread-safe; a duplicate name terminates the
/// program during static initialisation (see register_output_formatter()).
#define REGISTER_OUTPUT_FORMATTER(class_name)                                      \
    namespace {                                                                    \
    const bool SMBIOS_DETAIL_UNIQUE_NAME(smbios_output_formatter_registered_) = [] { \
        smbios::register_output_formatter(std::make_unique<class_name>());         \
        return true;                                                               \
    }();                                                                           \
    }
