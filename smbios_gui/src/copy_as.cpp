/// @file copy_as.cpp
/// @brief Implementation of copy_as.hpp. Deliberately includes no Dear ImGui
/// header - see copy_as.hpp.

#include <smbios/gui/copy_as.hpp>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

namespace smbios::gui {

std::optional<std::string> render_entry_as(const AbstractSMBiosEntry& entry,
                                           std::string_view format_name)
{
    const IOutputFormatter* const formatter = get_output_formatter(std::string(format_name));
    if (formatter == nullptr) {
        return std::nullopt;
    }
    std::string rendered = formatter->render(entry);
    if (rendered.size() > kCopyAsMaxBytes) {
        return std::nullopt;
    }
    return rendered;
}

size_t default_copy_format_index(const std::vector<std::string>& names) noexcept
{
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == kDefaultCopyFormat) {
            return i;
        }
    }
    return 0;
}

bool is_terminal_safe_format(std::string_view format_name) noexcept
{
    return format_name == kDefaultCopyFormat;
}

std::unique_ptr<AbstractSMBiosEntry> create_entry_for_node(SMBios& bios, size_t node_index,
                                                           const EntryNode& node)
{
    size_t position = 0;
    for (const DMIHeader& header : bios) {
        if (position++ != node_index) {
            continue;
        }
        if (header.type != node.type || header.handle != node.handle) {
            return nullptr;
        }
        return SMBiosEntryFactory{}.create(header, bios.get_smbios_version(),
                                           bios.get_table_base() + bios.get_table_size());
    }
    return nullptr;
}

} // namespace smbios::gui
