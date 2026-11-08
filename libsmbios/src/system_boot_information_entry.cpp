#include <smbios/tables/system_boot_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::SystemBootInformation at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemBootInformation, SystemBootInformationEntry)

SystemBootInformationEntry::SystemBootInformationEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemBootInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected System Boot Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Boot status is distinguished by header.length alone (R2); the
    // trailing payload has no fixed length at all - see class doc comment.
    // Confirmed against both c-versions/dmidecode_linux and c-versions/
    // dmidecode_macos's own dmi_decode() for type 32 (identical logic in
    // both).
    init_string_values();
}

void SystemBootInformationEntry::init_string_values()
{
    boot_status_map_[NoErrorsDetected] = "No errors detected";
    boot_status_map_[NoBootableMedia] = "No bootable media";
    boot_status_map_[OperatingSystemFailedToLoad] = "Operating system failed to load";
    boot_status_map_[FirmwareDetectedHardwareFailure] = "Firmware-detected hardware failure";
    boot_status_map_[OperatingSystemDetectedHardwareFailure] = "Operating system-detected hardware failure";
    boot_status_map_[UserRequestedBoot] = "User-requested boot";
    boot_status_map_[SystemSecurityViolation] = "System security violation";
    boot_status_map_[PreviouslyRequestedImage] = "Previously-requested image";
    boot_status_map_[SystemWatchdogTimerExpired] = "System watchdog timer expired";
}

std::string SystemBootInformationEntry::get_type() const
{
    return "System Boot Information";
}

std::string SystemBootInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Status: " << get_boot_status_string() << '\n';

    const std::string status_data = get_status_data_string();
    if (!status_data.empty()) {
        description << "Status Data: " << status_data << '\n';
    }

    return description.str();
}

uint8_t SystemBootInformationEntry::get_boot_status() const
{
    constexpr size_t offset = 0x0A;
    constexpr size_t min_length = 0x0B;
    if (get_entry_size() < min_length) return NoErrorsDetected;
    return read_u8(raw_data(), offset);
}

std::vector<uint8_t> SystemBootInformationEntry::get_status_data() const
{
    constexpr size_t payload_offset = 0x0B;
    const size_t entry_size = get_entry_size();
    if (entry_size <= payload_offset) return {};

    const uint8_t* data = raw_data();
    return std::vector<uint8_t>(data + payload_offset, data + entry_size);
}

std::string SystemBootInformationEntry::get_boot_status_string() const
{
    assert(!boot_status_map_.empty());
    const uint8_t status = get_boot_status();

    auto it = boot_status_map_.find(status);
    if (it != boot_status_map_.end()) {
        return (*it).second;
    }
    if (status >= 128 && status <= 191) {
        return "OEM-specific";
    }
    if (status >= 192) {
        return "Product-specific";
    }
    return "OutOfSpec";
}

std::string SystemBootInformationEntry::get_status_data_string() const
{
    const std::vector<uint8_t> payload = get_status_data();
    if (payload.empty()) return "";

    std::stringstream payload_stream;
    payload_stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < payload.size(); ++i) {
        if (i > 0) payload_stream << ' ';
        payload_stream << std::setw(2) << static_cast<unsigned>(payload[i]);
    }
    return payload_stream.str();
}
