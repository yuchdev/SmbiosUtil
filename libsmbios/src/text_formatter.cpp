#include <libsmbios/formatters/text_formatter.hpp>

#include <libsmbios/abstract_smbios_entry.hpp>

using namespace smbios;

std::string TextFormatter::render(const AbstractSMBiosEntry& entry) const
{
    // R4's single stated exception: `text` is the legacy description, so it
    // must stay byte-identical to render_to_description() (R3).
    return entry.render_to_description();
}

std::string TextFormatter::format_name() const
{
    return "text";
}

REGISTER_OUTPUT_FORMATTER(TextFormatter)
