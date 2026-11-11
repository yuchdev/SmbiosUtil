#include <libsmbios/tables/cache_information_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <cassert>
#include <utility>

using namespace smbios;

// Self-register against SMBios::CacheInformation at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::CacheInformation, CacheInformationEntry)

CacheInformationEntry::CacheInformationEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::CacheInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Cache Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone - no
    // SMBiosVersion-only field exists in the Cache Information structure
    // through spec 3.x (confirmed against c-versions/dmidecode_linux's own
    // dmi_decode() for type 7, which also gates purely on h->length).
    init_string_values();
}

void CacheInformationEntry::init_string_values()
{
    operational_mode_map_[WriteThrough] = "Write Through";
    operational_mode_map_[WriteBack] = "Write Back";
    operational_mode_map_[VariesWithMemoryAddress] = "Varies With Memory Address";
    operational_mode_map_[OperationalModeUnknown] = "Unknown";

    location_map_[Internal] = "Internal";
    location_map_[External] = "External";
    location_map_[LocationOutOfSpec] = "OutOfSpec";
    location_map_[LocationUnknown] = "Unknown";

    sram_type_map_[SRAMTypeOther] = "Other";
    sram_type_map_[SRAMTypeUnknown] = "Unknown";
    sram_type_map_[NonBurst] = "Non-burst";
    sram_type_map_[Burst] = "Burst";
    sram_type_map_[PipelineBurst] = "Pipeline Burst";
    sram_type_map_[Synchronous] = "Synchronous";
    sram_type_map_[Asynchronous] = "Asynchronous";

    error_correction_type_map_[ErrorCorrectionOutOfSpec] = "OutOfSpec";
    error_correction_type_map_[ErrorCorrectionOther] = "Other";
    error_correction_type_map_[ErrorCorrectionUnknown] = "Unknown";
    error_correction_type_map_[NoErrorCorrection] = "None";
    error_correction_type_map_[Parity] = "Parity";
    error_correction_type_map_[SingleBitECC] = "Single-bit ECC";
    error_correction_type_map_[MultiBitECC] = "Multi-bit ECC";

    system_cache_type_map_[SystemCacheTypeOutOfSpec] = "OutOfSpec";
    system_cache_type_map_[SystemCacheTypeOther] = "Other";
    system_cache_type_map_[SystemCacheTypeUnknown] = "Unknown";
    system_cache_type_map_[Instruction] = "Instruction";
    system_cache_type_map_[Data] = "Data";
    system_cache_type_map_[Unified] = "Unified";

    associativity_map_[AssociativityOutOfSpec] = "OutOfSpec";
    associativity_map_[AssociativityOther] = "Other";
    associativity_map_[AssociativityUnknown] = "Unknown";
    associativity_map_[DirectMapped] = "Direct Mapped";
    associativity_map_[TwoWaySetAssociative] = "2-way Set-associative";
    associativity_map_[FourWaySetAssociative] = "4-way Set-associative";
    associativity_map_[FullyAssociative] = "Fully Associative";
    associativity_map_[EightWaySetAssociative] = "8-way Set-associative";
    associativity_map_[SixteenWaySetAssociative] = "16-way Set-associative";
    associativity_map_[TwelveWaySetAssociative] = "12-way Set-associative";
    associativity_map_[TwentyFourWaySetAssociative] = "24-way Set-associative";
    associativity_map_[ThirtyTwoWaySetAssociative] = "32-way Set-associative";
    associativity_map_[FortyEightWaySetAssociative] = "48-way Set-associative";
    associativity_map_[SixtyFourWaySetAssociative] = "64-way Set-associative";
    associativity_map_[TwentyWaySetAssociative] = "20-way Set-associative";
}

std::string CacheInformationEntry::get_type() const
{
    return "Cache Information";
}

std::string CacheInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Socket Designation: " << get_socket_designation_string() << '\n';
    description << "Enabled: " << get_enabled_string() << '\n';
    description << "Socketed: " << get_socketed_string() << '\n';
    description << "Cache Level: " << static_cast<unsigned>(get_cache_level()) << '\n';
    description << "Operational Mode: " << get_operational_mode_string() << '\n';
    description << "Location: " << get_location_string() << '\n';
    description << "Maximum Size: " << get_max_cache_size_string() << '\n';
    description << "Installed Size: " << get_installed_cache_size_string() << '\n';
    description << "Supported SRAM Types: " << '\n' << get_supported_sram_type_string();
    description << "Installed SRAM Type: " << '\n' << get_installed_sram_type_string();
    description << "Speed: " << get_cache_speed_string() << '\n';
    description << "Error Correction Type: " << get_error_correction_type_string() << '\n';
    description << "System Type: " << get_system_cache_type_string() << '\n';
    description << "Associativity: " << get_associativity_string() << '\n';

    return description.str();
}

uint8_t CacheInformationEntry::get_socket_designation_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

bool CacheInformationEntry::get_enabled() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return false;
    return (read_u16(raw_data(), offset) & 0x0080) != 0;
}

bool CacheInformationEntry::get_socketed() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return false;
    return (read_u16(raw_data(), offset) & 0x0008) != 0;
}

uint8_t CacheInformationEntry::get_cache_level() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 1;
    return static_cast<uint8_t>((read_u16(raw_data(), offset) & 0x0007) + 1);
}

uint8_t CacheInformationEntry::get_operational_mode() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return CacheOperationalMode::OperationalModeUnknown;
    return static_cast<uint8_t>((read_u16(raw_data(), offset) >> 8) & 0x0003);
}

uint8_t CacheInformationEntry::get_location() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return CacheLocation::LocationUnknown;
    return static_cast<uint8_t>((read_u16(raw_data(), offset) >> 5) & 0x0003);
}

uint64_t CacheInformationEntry::decode_cache_size_kb(uint32_t encoded)
{
    if (encoded & 0x80000000u) {
        return static_cast<uint64_t>(encoded & 0x7FFFFFFFu) << 6; // 64 KB granularity
    }
    return encoded; // 1 KB granularity
}

uint64_t CacheInformationEntry::decode_legacy_cache_size_kb(uint16_t encoded)
{
    // Widen to the uint32_t encoding exactly as dmi_cache_size() does, then
    // reuse the same granularity decode.
    const uint32_t widened = ((static_cast<uint32_t>(encoded) & 0x8000u) << 16) | (encoded & 0x7FFFu);
    return decode_cache_size_kb(widened);
}

uint64_t CacheInformationEntry::get_max_cache_size() const
{
    constexpr size_t widened_offset = 0x13;
    constexpr size_t widened_min_length = 0x17;
    if (get_entry_size() >= widened_min_length) {
        return decode_cache_size_kb(read_u32(raw_data(), widened_offset));
    }

    constexpr size_t legacy_offset = 0x07;
    constexpr size_t legacy_min_length = 0x0F;
    if (get_entry_size() < legacy_min_length) return 0;
    return decode_legacy_cache_size_kb(read_u16(raw_data(), legacy_offset));
}

uint64_t CacheInformationEntry::get_installed_cache_size() const
{
    constexpr size_t widened_offset = 0x17;
    constexpr size_t widened_min_length = 0x1B;
    if (get_entry_size() >= widened_min_length) {
        return decode_cache_size_kb(read_u32(raw_data(), widened_offset));
    }

    constexpr size_t legacy_offset = 0x09;
    constexpr size_t legacy_min_length = 0x0F;
    if (get_entry_size() < legacy_min_length) return 0;
    return decode_legacy_cache_size_kb(read_u16(raw_data(), legacy_offset));
}

uint16_t CacheInformationEntry::get_supported_sram_type() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint16_t CacheInformationEntry::get_installed_sram_type() const
{
    constexpr size_t offset = 0x0D;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint8_t CacheInformationEntry::get_cache_speed() const
{
    constexpr size_t offset = 0x0F;
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t CacheInformationEntry::get_error_correction_type() const
{
    constexpr size_t offset = 0x10;
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return ErrorCorrectionType::ErrorCorrectionOutOfSpec;

    const uint8_t type = read_u8(raw_data(), offset);
    auto it = error_correction_type_map_.find(type);
    if (it != error_correction_type_map_.end()) {
        return (*it).first;
    }
    return ErrorCorrectionType::ErrorCorrectionOutOfSpec;
}

uint8_t CacheInformationEntry::get_system_cache_type() const
{
    constexpr size_t offset = 0x11;
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return SystemCacheType::SystemCacheTypeOutOfSpec;

    const uint8_t type = read_u8(raw_data(), offset);
    auto it = system_cache_type_map_.find(type);
    if (it != system_cache_type_map_.end()) {
        return (*it).first;
    }
    return SystemCacheType::SystemCacheTypeOutOfSpec;
}

uint8_t CacheInformationEntry::get_associativity() const
{
    constexpr size_t offset = 0x12;
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return Associativity::AssociativityOutOfSpec;

    const uint8_t associativity = read_u8(raw_data(), offset);
    auto it = associativity_map_.find(associativity);
    if (it != associativity_map_.end()) {
        return (*it).first;
    }
    return Associativity::AssociativityOutOfSpec;
}

std::string CacheInformationEntry::get_socket_designation_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_socket_designation_index());
}

std::string CacheInformationEntry::get_enabled_string() const
{
    return get_enabled() ? "Enabled" : "Disabled";
}

std::string CacheInformationEntry::get_socketed_string() const
{
    return get_socketed() ? "Socketed" : "Not Socketed";
}

std::string CacheInformationEntry::get_operational_mode_string() const
{
    assert(!operational_mode_map_.empty());
    return (*operational_mode_map_.find(get_operational_mode())).second;
}

std::string CacheInformationEntry::get_location_string() const
{
    assert(!location_map_.empty());
    return (*location_map_.find(get_location())).second;
}

std::string CacheInformationEntry::cache_size_string(uint64_t size_kb)
{
    if (size_kb != 0 && (size_kb % 1024) == 0) {
        return std::to_string(size_kb / 1024) + " MB";
    }
    return std::to_string(size_kb) + " kB";
}

std::string CacheInformationEntry::get_max_cache_size_string() const
{
    return cache_size_string(get_max_cache_size());
}

std::string CacheInformationEntry::get_installed_cache_size_string() const
{
    return cache_size_string(get_installed_cache_size());
}

std::string CacheInformationEntry::get_supported_sram_type_string() const
{
    assert(!sram_type_map_.empty());
    const uint16_t types = get_supported_sram_type();
    if (sram_types_are_none(types)) {
        return "\tNone\n";
    }
    return AbstractSMBiosEntry::bitset_to_properties<uint16_t>(types, sram_type_map_);
}

std::string CacheInformationEntry::get_installed_sram_type_string() const
{
    assert(!sram_type_map_.empty());
    const uint16_t types = get_installed_sram_type();
    if (sram_types_are_none(types)) {
        return "\tNone\n";
    }
    return AbstractSMBiosEntry::bitset_to_properties<uint16_t>(types, sram_type_map_);
}

std::string CacheInformationEntry::get_cache_speed_string() const
{
    const uint8_t speed = get_cache_speed();
    if (0 == speed) {
        return "Unknown";
    }
    return std::to_string(static_cast<unsigned>(speed)) + " ns";
}

std::string CacheInformationEntry::get_error_correction_type_string() const
{
    assert(!error_correction_type_map_.empty());
    return (*error_correction_type_map_.find(get_error_correction_type())).second;
}

std::string CacheInformationEntry::get_system_cache_type_string() const
{
    assert(!system_cache_type_map_.empty());
    return (*system_cache_type_map_.find(get_system_cache_type())).second;
}

std::string CacheInformationEntry::get_associativity_string() const
{
    assert(!associativity_map_.empty());
    return (*associativity_map_.find(get_associativity())).second;
}

bool CacheInformationEntry::sram_types_are_none(uint16_t types)
{
    return (types & 0x007F) == 0;
}

std::vector<NamedField> CacheInformationEntry::get_fields() const
{
    // Mirrors get_supported/installed_sram_type_string(): the "None"
    // sentinel exactly when the text prints it, otherwise one true field per
    // set bit, qualified by group so the shared bit labels stay unique (C1).
    auto append_sram_types = [this](uint16_t types, const char* label, const char* qualifier,
                                    std::vector<NamedField>& out) {
        if (sram_types_are_none(types)) {
            out.push_back({label, std::string("None")});
            return;
        }
        std::vector<NamedField> bits;
        bitset_to_fields<uint16_t>(types, sram_type_map_, bits);
        for (auto& bit : bits) {
            bit.name += std::string(" (") + qualifier + ")";
            out.push_back(std::move(bit));
        }
    };

    std::vector<NamedField> fields;
    fields.push_back({"Socket Designation", get_socket_designation_string()});
    fields.push_back({"Enabled", get_enabled()});
    fields.push_back({"Socketed", get_socketed()});
    fields.push_back({"Cache Level", static_cast<uint64_t>(get_cache_level())});
    fields.push_back({"Operational Mode", get_operational_mode_string()});
    fields.push_back({"Location", get_location_string()});
    fields.push_back({"Maximum Size", get_max_cache_size_string()});
    fields.push_back({"Installed Size", get_installed_cache_size_string()});
    append_sram_types(get_supported_sram_type(), "Supported SRAM Types", "Supported", fields);
    append_sram_types(get_installed_sram_type(), "Installed SRAM Type", "Installed", fields);
    fields.push_back({"Speed", get_cache_speed_string()});
    fields.push_back({"Error Correction Type", get_error_correction_type_string()});
    fields.push_back({"System Type", get_system_cache_type_string()});
    fields.push_back({"Associativity", get_associativity_string()});
    return fields;
}
