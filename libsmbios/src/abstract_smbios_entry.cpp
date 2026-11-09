#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios.hpp>

#include <algorithm>
#include <cassert>
#include <string>
#include <sstream>

using namespace smbios;

AbstractSMBiosEntry::AbstractSMBiosEntry(const DMIHeader& header, const uint8_t* table_end)
    : header_(std::make_unique<smbios::DMIHeader>(header)), dmi_strings_({"Not Specified"})
{
    parse_dmi_strings(table_end);
}

void AbstractSMBiosEntry::parse_dmi_strings(const uint8_t* table_end)
{
    if (nullptr == header_->data) {
        // most probably we called parsing from the entry which do not contain strings
        // it's ok, just leave only "Not Specified" entry
        return;
    }

    const uint8_t* string_section = header_->data + header_->length;
    while (string_section < table_end && string_section[0] != 0) {
        const char* start = reinterpret_cast<const char*>(string_section);
        const uint8_t* terminator = std::find(string_section, table_end, 0);
        if (terminator == table_end) break; // unterminated - stop, don't read past the buffer
        dmi_strings_.push_back(std::string(start, reinterpret_cast<const char*>(terminator)));
        string_section = terminator + 1;
    }
}

std::string AbstractSMBiosEntry::dmi_string(size_t string_index) const
{
    if (string_index >= dmi_strings_.size()) {
        return std::string("Bad index");
    }
    return dmi_strings_[string_index];
}

size_t smbios::AbstractSMBiosEntry::get_entry_size() const
{
    return static_cast<size_t>(header_->length);
}

const uint8_t* smbios::AbstractSMBiosEntry::raw_data() const
{
    return header_->data;
}

std::string smbios::AbstractSMBiosEntry::address_string(uint16_t string_address) const
{
    std::stringstream address_stream;
    address_stream << std::hex << std::showbase
        << string_address
        << std::dec << std::noshowbase;
    return address_stream.str();
}

