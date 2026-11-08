#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/tpm_device_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive TpmDeviceEntry
// (smbios/include/smbios/tables/tpm_device_entry.h), cross-checked directly
// against DSP0134 3.9.0's own 7.44/7.44.1 (Table 126/127).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : 4 raw ASCII bytes  vendor_id            (NOT a STRING index)
//   offset 0x08 : uint8_t            major_spec_version
//   offset 0x09 : uint8_t            minor_spec_version
//   offset 0x0A : uint32_t           firmware_version_1
//   offset 0x0E : uint32_t           firmware_version_2
//   offset 0x12 : uint8_t            description          (STRING)
//   offset 0x13 : uint64_t           characteristics
//   offset 0x1B : uint32_t           oem_defined
//
// Minimum (full) structure length is 0x1F (Table 126's own "1Fh").

BOOST_AUTO_TEST_SUITE(TpmDeviceEntryUnitTests)

namespace {

DMIHeader make_header(const std::vector<uint8_t>& buffer)
{
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();
    return header;
}

void append_u32(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void append_u64(std::vector<uint8_t>& buffer, uint64_t value)
{
    append_u32(buffer, static_cast<uint32_t>(value & 0xFFFFFFFFULL));
    append_u32(buffer, static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFULL));
}

/// Build the full (minimum-length, 0x1F) formatted area.
std::vector<uint8_t> make_formatted_area(const uint8_t vendor_id[4], uint8_t major_spec_version,
                                          uint8_t minor_spec_version, uint32_t firmware_version_1,
                                          uint32_t firmware_version_2, uint8_t description_index,
                                          uint64_t characteristics, uint32_t oem_defined)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.insert(formatted_area.end(), vendor_id, vendor_id + 4); // 0x04
    formatted_area.push_back(major_spec_version);                        // 0x08
    formatted_area.push_back(minor_spec_version);                        // 0x09
    append_u32(formatted_area, firmware_version_1);                      // 0x0A
    append_u32(formatted_area, firmware_version_2);                      // 0x0E
    formatted_area.push_back(description_index);                        // 0x12
    append_u64(formatted_area, characteristics);                         // 0x13
    append_u32(formatted_area, oem_defined);                             // 0x1B
    return formatted_area;
}

} // namespace

/// Normal case: TPM 1.2-shaped buffer (Major Spec Version 0x01).
BOOST_AUTO_TEST_CASE(Tpm12ShapedBufferTestCase)
{
    const uint8_t vendor_id[4] = { 'I', 'F', 'X', 0x00 }; // "IFX" + trailing null
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x01, 0x02, 0x01010000, 0x00000000,
                                                                0x01, 0x00, 0x00000000);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x1F));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area, {"TPM 1.2 Device"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TpmDeviceEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_id_string(), std::string("IFX"));
    BOOST_CHECK_EQUAL(entry.get_major_spec_version(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_minor_spec_version(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_firmware_version_1(), static_cast<uint32_t>(0x01010000));
    BOOST_CHECK_EQUAL(entry.get_firmware_version_2(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_description_string(), std::string("TPM 1.2 Device"));
    BOOST_CHECK_EQUAL(entry.get_characteristics(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0));
}

/// Normal case: TPM 2.0-shaped buffer (Major Spec Version 0x02), firmware
/// version split across both DWORDs per 7.44's own description.
BOOST_AUTO_TEST_CASE(Tpm20ShapedBufferTestCase)
{
    const uint8_t vendor_id[4] = { 'A', 'M', 'D', 0x00 }; // "AMD" + trailing null
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x02, 0x00, 0x00010203, 0x04050607,
                                                                0x01, 0x00, 0x00000000);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area, {"TPM 2.0 Device"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TpmDeviceEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_id_string(), std::string("AMD"));
    BOOST_CHECK_EQUAL(entry.get_major_spec_version(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_minor_spec_version(), static_cast<uint8_t>(0x00));
    BOOST_CHECK_EQUAL(entry.get_firmware_version_1(), static_cast<uint32_t>(0x00010203));
    BOOST_CHECK_EQUAL(entry.get_firmware_version_2(), static_cast<uint32_t>(0x04050607));
    BOOST_CHECK_EQUAL(entry.get_description_string(), std::string("TPM 2.0 Device"));
}

/// Edge case: a 3-character Vendor ID ("ABC" + trailing 0x00) must trim to
/// exactly "ABC" - Table 126's own documented example.
BOOST_AUTO_TEST_CASE(ThreeCharacterVendorIdTrimsTestCase)
{
    const uint8_t vendor_id[4] = { 'A', 'B', 'C', 0x00 };
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x01, 0x02, 0x00000000, 0x00000000,
                                                                0x00, 0x00, 0x00000000);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TpmDeviceEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_id_string(), std::string("ABC"));
    BOOST_CHECK_EQUAL(entry.get_vendor_id_string().size(), static_cast<size_t>(3));
}

/// Edge case: a 4-character Vendor ID ("ABCD", no null terminator) must be
/// returned in full, un-trimmed - Table 126's own documented example.
BOOST_AUTO_TEST_CASE(FourCharacterVendorIdNotOverTrimmedTestCase)
{
    const uint8_t vendor_id[4] = { 'A', 'B', 'C', 'D' };
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x01, 0x02, 0x00000000, 0x00000000,
                                                                0x00, 0x00, 0x00000000);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TpmDeviceEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_id_string(), std::string("ABCD"));
    BOOST_CHECK_EQUAL(entry.get_vendor_id_string().size(), static_cast<size_t>(4));
}

/// At least one Characteristics bit set - confirms
/// bitset_to_properties<uint64_t>() actually compiles and renders
/// correctly for this QWORD field (7.44.1, Table 127), the project's first
/// QWORD-width bitmask.
BOOST_AUTO_TEST_CASE(CharacteristicsBitSetTestCase)
{
    const uint8_t vendor_id[4] = { 'N', 'T', 'C', 0x00 };
    const uint64_t characteristics = TpmDeviceEntry::FamilyConfigurableViaFirmwareUpdate
                                      | TpmDeviceEntry::FamilyConfigurableViaOemProprietaryMechanism;
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x02, 0x00, 0x00000000, 0x00000000,
                                                                0x00, characteristics, 0xDEADBEEF);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TpmDeviceEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_characteristics(), characteristics);
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0xDEADBEEF));

    const std::string characteristics_string = entry.get_characteristics_string();
    BOOST_CHECK(characteristics_string.find("Family configurable via firmware update") != std::string::npos);
    BOOST_CHECK(characteristics_string.find("Family configurable via OEM proprietary mechanism") != std::string::npos);
    BOOST_CHECK(characteristics_string.find("TPM Device Characteristics are not supported") == std::string::npos);
    BOOST_CHECK(characteristics_string.find("Family configurable via platform software support") == std::string::npos);
}

/// Edge case: structure shorter than the minimum 0x1F length - every
/// accessor must fall back cleanly rather than reading past the buffer.
BOOST_AUTO_TEST_CASE(TruncatedStructureTestCase)
{
    // Only Vendor ID + Major/Minor Spec Version present (0x0A total,
    // length < 0x1F).
    std::vector<uint8_t> formatted_area = { 'A', 'B', 'C', 0x00, 0x01, 0x02 };
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TpmDeviceEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    // Vendor ID and Spec Versions are still in bounds.
    BOOST_CHECK_EQUAL(entry.get_vendor_id_string(), std::string("ABC"));
    BOOST_CHECK_EQUAL(entry.get_major_spec_version(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_minor_spec_version(), static_cast<uint8_t>(0x02));

    // Everything beyond the buffer falls back to 0/"Not Specified".
    BOOST_CHECK_EQUAL(entry.get_firmware_version_1(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_firmware_version_2(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_description_string(), std::string("Not Specified"));
    BOOST_CHECK_EQUAL(entry.get_characteristics(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0));
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    const uint8_t vendor_id[4] = { 0x00, 0x00, 0x00, 0x00 };
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x00, 0x00, 0x00000000, 0x00000000,
                                                                0x00, 0x00, 0x00000000);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(TpmDeviceEntry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size()), std::runtime_error);
}

/// TpmDevice resolves through SMBiosEntryFactory::create() to a real
/// TpmDeviceEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsTpmDeviceEntryTestCase)
{
    const uint8_t vendor_id[4] = { 0x00, 0x00, 0x00, 0x00 };
    std::vector<uint8_t> formatted_area = make_formatted_area(vendor_id, 0x00, 0x00, 0x00000000, 0x00000000,
                                                                0x00, 0x00, 0x00000000);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TpmDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<TpmDeviceEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
