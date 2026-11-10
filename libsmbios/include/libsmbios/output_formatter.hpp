#pragma once
#include <memory>
#include <string>
#include <vector>

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
/// Every member is `const` and one registered instance is shared by every
/// caller. Implementations must never log or persist field values.
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
/// @param formatter the formatter to register; must not be null.
/// @throws std::invalid_argument if `formatter` is null.
/// @throws std::logic_error if formatter->format_name() is already
/// registered; a duplicate registration is a programming error (e.g. two
/// formatter classes both claiming the same name), never a silent
/// overwrite. The message names only the format name.
void register_output_formatter(std::unique_ptr<IOutputFormatter> formatter);

/// @brief Looks up a registered formatter by its exact, case-sensitive name.
/// @param name the format name to look up (e.g. `"json"`).
/// @return a non-owning pointer to the registered formatter, valid for the
/// rest of the program's lifetime, or nullptr if `name` is unregistered.
/// Never throws for an unknown name, so a caller (e.g. the `smbios` CLI)
/// can report it alongside registered_output_formatter_names().
const IOutputFormatter* get_output_formatter(const std::string& name);

/// @brief Enumerates every registered format name.
/// @return every registered format name, sorted ascending.
std::vector<std::string> registered_output_formatter_names();

} // namespace smbios

/// @brief Registers a default-constructed `class_name` (a concrete
/// smbios::IOutputFormatter) under its format_name() at static-init time,
/// via a function-local static map (see output_formatter.cpp) - no edit to
/// output_formatter.{hpp,cpp} is required to add a new format.
/// Place exactly one invocation at namespace scope in the formatter's own
/// .cpp (never its header - a header-placed invocation would register once
/// per translation unit that includes it, tripping the duplicate-
/// registration guard, same reasoning as REGISTER_SMBIOS_ENTRY).
#define REGISTER_OUTPUT_FORMATTER(class_name)                                      \
    namespace {                                                                    \
    const bool class_name##_formatter_registered = [] {                            \
        smbios::register_output_formatter(std::make_unique<class_name>());         \
        return true;                                                               \
    }();                                                                           \
    }
