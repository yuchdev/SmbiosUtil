#include <smbios/tables/processor_additional_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::ProcessorAdditionalInformation at
// static-init time; see REGISTER_SMBIOS_ENTRY's doc comment
// (smbios_entry_factory.h) for why this lives here rather than in the
// header.
REGISTER_SMBIOS_ENTRY(SMBios::ProcessorAdditionalInformation, ProcessorAdditionalInformationEntry)

namespace {

constexpr size_t kReferencedHandleOffset = 0x04;
constexpr size_t kBlockLengthOffset = 0x06;
constexpr size_t kProcessorTypeOffset = 0x07;
constexpr size_t kProcessorSpecificDataOffset = 0x08;

} // namespace

ProcessorAdditionalInformationEntry::ProcessorAdditionalInformationEntry(const DMIHeader& header,
                                                                          const SMBiosVersion& /*version*/,
                                                                          const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::ProcessorAdditionalInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Processor Additional Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.45/7.45.1 (Table
    // 128/129/130).
    init_string_values();
}

void ProcessorAdditionalInformationEntry::init_string_values()
{
    // DSP0134 3.9.0 Table 130 (7.45.1), confirmed in full against the
    // standard document - not left at the placeholder 0x00 = Reserved
    // entry alone.
    processor_type_map_[Reserved] = "Reserved";
    processor_type_map_[Ia32] = "IA32 (x86)";
    processor_type_map_[X64] = "x64 (x86-64, Intel64, AMD64, EM64T)";
    processor_type_map_[ItaniumArchitecture] = "Intel(R) Itanium(R) architecture";
    processor_type_map_[Arm32] = "32-bit ARM (Aarch32)";
    processor_type_map_[Arm64] = "64-bit ARM (Aarch64)";
    processor_type_map_[RiscV32] = "32-bit RISC-V (RV32)";
    processor_type_map_[RiscV64] = "64-bit RISC-V (RV64)";
    processor_type_map_[RiscV128] = "128-bit RISC-V (RV128)";
    processor_type_map_[LoongArch32] = "32-bit LoongArch (LoongArch32)";
    processor_type_map_[LoongArch64] = "64-bit LoongArch (LoongArch64)";
}

std::string ProcessorAdditionalInformationEntry::get_type() const
{
    return "Processor Additional Information";
}

std::string ProcessorAdditionalInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Referenced Handle: 0x" << std::hex << get_referenced_handle() << std::dec << '\n';
    description << "Block Length: " << static_cast<unsigned>(get_block_length()) << '\n';
    description << "Processor Type: " << get_processor_type_string() << '\n';

    const std::vector<uint8_t> data = get_processor_specific_data();
    description << "Processor-Specific Data (" << data.size() << " bytes): ";
    for (uint8_t b : data) {
        description << std::hex << static_cast<unsigned>(b) << ' ' << std::dec;
    }
    description << '\n';

    return description.str();
}

uint16_t ProcessorAdditionalInformationEntry::get_referenced_handle() const
{
    constexpr size_t min_length = kReferencedHandleOffset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), kReferencedHandleOffset);
}

uint8_t ProcessorAdditionalInformationEntry::get_block_length() const
{
    constexpr size_t min_length = kBlockLengthOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kBlockLengthOffset);
}

uint8_t ProcessorAdditionalInformationEntry::get_processor_type() const
{
    constexpr size_t min_length = kProcessorTypeOffset + 1;
    if (get_entry_size() < min_length) return Reserved;
    return read_u8(raw_data(), kProcessorTypeOffset);
}

std::string ProcessorAdditionalInformationEntry::get_processor_type_string() const
{
    const auto found = processor_type_map_.find(get_processor_type());
    if (found == processor_type_map_.end()) return "Unknown";
    return found->second;
}

std::vector<uint8_t> ProcessorAdditionalInformationEntry::get_processor_specific_data() const
{
    std::vector<uint8_t> data;

    const uint8_t block_length = get_block_length();
    const size_t required_length = kProcessorSpecificDataOffset + static_cast<size_t>(block_length);
    if (get_entry_size() < required_length) return data;

    data.reserve(block_length);
    for (size_t i = 0; i < block_length; ++i) {
        data.push_back(read_u8(raw_data(), kProcessorSpecificDataOffset + i));
    }
    return data;
}
