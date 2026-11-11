#include "doc_sample_fixture.hpp"

#include <string>

#include "synthetic_table_builder.hpp"

// Every byte of the doc-sample fixture is traceable to a literal in this
// file (D3 privacy rule). Changing any value here changes every golden file
// under tests/data/output_formats/ - regenerate them with
// SMBIOS_REGENERATE_DOC_SAMPLES=1 (see doc_samples_test.cpp).

namespace smbios::test {

namespace {

/// BIOS Characteristics bit 7 ("PCI is supported").
constexpr uint64_t kPciSupported = uint64_t{1} << 7;

/// BIOS Characteristics bit 11 ("BIOS is upgradeable").
constexpr uint64_t kBiosUpgradeable = uint64_t{1} << 11;

/// @brief Type 0 formatted area (offsets 0x04..0x19, length 0x1A).
std::vector<uint8_t> bios_information_area()
{
    std::vector<uint8_t> area{
        0x01,       // 0x04 vendor string index
        0x02,       // 0x05 version string index
        0x00, 0xF0, // 0x06 starting address segment (0xF000)
        0x03,       // 0x08 release date string index
        0x01,       // 0x09 ROM size -> 128 kB
    };
    const uint64_t characteristics = kPciSupported | kBiosUpgradeable;
    for (int i = 0; i < 8; ++i) { // 0x0A..0x11 characteristics, little-endian
        area.push_back(static_cast<uint8_t>(characteristics >> (8 * i)));
    }
    area.push_back(0x00); // 0x12 characteristics extension byte 1: none set
    area.push_back(0x00); // 0x13 characteristics extension byte 2: none set
    area.push_back(0x01); // 0x14 system BIOS major release
    area.push_back(0x00); // 0x15 system BIOS minor release
    area.push_back(0x02); // 0x16 embedded controller firmware major release
    area.push_back(0x00); // 0x17 embedded controller firmware minor release
    area.push_back(0x00); // 0x18 extended BIOS ROM size (unused)
    area.push_back(0x00);
    return area;
}

} // namespace

std::vector<uint8_t> build_doc_sample_table()
{
    SyntheticTableBuilder builder;

    builder.add_structure(SMBios::BIOSInformation, bios_information_area(),
                          std::vector<std::string>{
                              "Example \"Firmware\" & Co.", // XML/JSON/YAML/CSV escaping
                              "Example 1.0",
                              "Example 01/01/2026",
                          });

    builder.add_structure(SMBios::OemStrings,
                          std::vector<uint8_t>{0x01}, // 0x04 string count
                          std::vector<std::string>{"=EXAMPLE()"}); // CSV formula prefix

    builder.add_structure(SMBios::SystemBootInformation,
                          std::vector<uint8_t>{
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x04..0x09 reserved
                              0x00,                               // 0x0A boot status: no errors
                              0x01, 0x23, 0xAB, 0xCD,             // 0x0B.. status data
                          });

    return builder.build();
}

SMBiosVersion doc_sample_version()
{
    return SMBiosVersion{3, 4};
}

} // namespace smbios::test
