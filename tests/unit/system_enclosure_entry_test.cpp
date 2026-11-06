#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/system_enclosure_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive SystemEnclosureEntry (smbios/include/smbios/
// system_enclosure_entry.h) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../01-system-information-and-enclosure.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 3
// ("Chassis Information"):
//   tier 1: header.length < 0x09   -> every length-guarded getter defaults
//   tier 2: header.length == 0x09  -> manufacturer..asset_tag (incl. the
//                                      type byte's two sub-fields) decode;
//                                      boot_up_state onward still default
//   tier 3: header.length == 0x0D  -> boot_up_state..security_status also
//                                      decode; oem_defined still defaults
//   tier 4: header.length == 0x11  -> oem_defined also decodes;
//                                      height/power_cords still default
//   tier 5: header.length == 0x13  -> height/power_cords also decode;
//                                      contained_element_count/record_length
//                                      still default
//   tier 6: header.length == 0x15, count == record_length == 0 ->
//                                      count/record_length also decode;
//                                      sku_number (needs length >= 0x16)
//                                      still defaults
//   tier 7: header.length == 0x16, count == record_length == 0 ->
//                                      sku_number also decodes, at its
//                                      "no contained elements" offset 0x15
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  manufacturer_index
//   offset 0x05 : uint8_t  type (bits 0-6 enclosure type, bit 7 lock present)
//   offset 0x06 : uint8_t  version_index
//   offset 0x07 : uint8_t  serial_number_index
//   offset 0x08 : uint8_t  asset_tag_index
//   offset 0x09 : uint8_t  boot_up_state
//   offset 0x0A : uint8_t  power_supply_state
//   offset 0x0B : uint8_t  thermal_state
//   offset 0x0C : uint8_t  security_status
//   offset 0x0D : uint32_t oem_defined
//   offset 0x11 : uint8_t  height
//   offset 0x12 : uint8_t  number_of_power_cords
//   offset 0x13 : uint8_t  contained_element_count
//   offset 0x14 : uint8_t  contained_element_record_length
//   offset 0x15 : count*record_length bytes, contained elements
//   offset 0x15 + count*record_length : uint8_t sku_number_index

BOOST_AUTO_TEST_SUITE(SystemEnclosureEntryUnitTests)

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

/// @brief Append a minimal well-formed (empty) DMI string-section
/// terminator (double NUL) so AbstractSMBiosEntry::parse_dmi_strings() has a
/// well-formed section to walk and does not run past the buffer.
void append_string_terminator(std::vector<uint8_t>& buffer)
{
    buffer.push_back(0x00);
    buffer.push_back(0x00);
}

/// @brief Build a full-length (0x16) formatted-area buffer with distinct
/// known byte values at every offset and a zero contained-element
/// count/record-length (so the SKU number sits right at 0x15), then
/// truncate to `length` and patch the header's own length byte
/// accordingly. `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length)
{
    std::vector<uint8_t> full;
    full.push_back(SMBios::SystemEnclosure); // 0x00 type = 3
    full.push_back(0x00);                    // 0x01 length (patched below)
    full.push_back(0x01);                    // 0x02 handle low byte
    full.push_back(0x00);                    // 0x03 handle high byte

    full.push_back(0x01); // 0x04 manufacturer_index
    // 0x05 type byte: bit 7 (lock present) = 1, bits 0-6 = Tower (0x07)
    full.push_back(static_cast<uint8_t>(0x80 | SystemEnclosureEntry::Tower));
    full.push_back(0x02); // 0x06 version_index
    full.push_back(0x03); // 0x07 serial_number_index
    full.push_back(0x04); // 0x08 asset_tag_index

    full.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Safe));    // 0x09 boot_up_state
    full.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Warning)); // 0x0A power_supply_state
    full.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Critical)); // 0x0B thermal_state
    full.push_back(static_cast<uint8_t>(SystemEnclosureEntry::ExternalInterfaceLockedOut)); // 0x0C security_status

    // 0x0D-0x10 oem_defined (u32 LE) -> 0xDEADBEEF
    full.push_back(0xEF);
    full.push_back(0xBE);
    full.push_back(0xAD);
    full.push_back(0xDE);

    full.push_back(0x02); // 0x11 height
    full.push_back(0x01); // 0x12 number_of_power_cords
    full.push_back(0x00); // 0x13 contained_element_count = 0
    full.push_back(0x00); // 0x14 contained_element_record_length = 0
    full.push_back(0x05); // 0x15 sku_number_index (offset 0x15 + 0*0)

    BOOST_REQUIRE_EQUAL(full.size(), static_cast<size_t>(0x16));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), full.size());

    std::vector<uint8_t> buffer(full.begin(), full.begin() + length);
    buffer[1] = length;

    append_string_terminator(buffer);
    return buffer;
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x09 means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryTruncatedBelow0x09DefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_enclosure_type(), static_cast<uint8_t>(SystemEnclosureEntry::ChassisTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_lock_present(), false);
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_boot_up_state(), static_cast<uint8_t>(SystemEnclosureEntry::ChassisStateOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_power_supply_state(), static_cast<uint8_t>(SystemEnclosureEntry::ChassisStateOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_thermal_state(), static_cast<uint8_t>(SystemEnclosureEntry::ChassisStateOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_security_status(), static_cast<uint8_t>(SystemEnclosureEntry::SecurityStatusOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_height(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_number_of_power_cords(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_contained_element_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_contained_element_record_length(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
}

/// Normal case: header.length == 0x09 - manufacturer through asset_tag
/// decode, including both sub-fields of the type byte; boot_up_state
/// onward (needs length >= 0x0D) still defaults.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryTier0x09DecodesThroughAssetTagTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x09);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_enclosure_type(), static_cast<uint8_t>(SystemEnclosureEntry::Tower));
    BOOST_CHECK_EQUAL(entry.get_lock_present(), true);
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0x04));

    // length == 0x09, boot_up_state..security_status need length >= 0x0D
    BOOST_CHECK_EQUAL(entry.get_boot_up_state(), static_cast<uint8_t>(SystemEnclosureEntry::ChassisStateOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0));
}

/// Edge case: header.length == 0x0D - boot_up_state through security_status
/// also decode; oem_defined (needs length >= 0x11) still defaults.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryTier0x0DDecodesStateFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0D);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_boot_up_state(), static_cast<uint8_t>(SystemEnclosureEntry::Safe));
    BOOST_CHECK_EQUAL(entry.get_power_supply_state(), static_cast<uint8_t>(SystemEnclosureEntry::Warning));
    BOOST_CHECK_EQUAL(entry.get_thermal_state(), static_cast<uint8_t>(SystemEnclosureEntry::Critical));
    BOOST_CHECK_EQUAL(entry.get_security_status(), static_cast<uint8_t>(SystemEnclosureEntry::ExternalInterfaceLockedOut));

    // length == 0x0D, oem_defined needs length >= 0x11 -> defaults
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_height(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x11 - oem_defined also decodes;
/// height/number_of_power_cords (need length >= 0x13) still default.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryTier0x11DecodesOemDefinedTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x11);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0xDEADBEEF));

    // length == 0x11, height/number_of_power_cords need length >= 0x13
    BOOST_CHECK_EQUAL(entry.get_height(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_number_of_power_cords(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_contained_element_count(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x13 - height/number_of_power_cords also
/// decode; contained_element_count/record_length (need length >= 0x15)
/// still default.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryTier0x13DecodesHeightAndPowerCordsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_height(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_number_of_power_cords(), static_cast<uint8_t>(0x01));

    // length == 0x13, contained_element_count/record_length need length >= 0x15
    BOOST_CHECK_EQUAL(entry.get_contained_element_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_contained_element_record_length(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x15, count == record_length == 0 - the
/// count/record_length fields themselves also decode; sku_number (needs
/// length >= 0x16 + 0*0 == 0x16) still defaults.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryTier0x15DecodesContainedElementFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x15);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_contained_element_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_contained_element_record_length(), static_cast<uint8_t>(0));

    // length == 0x15, sku_number needs length >= 0x16 -> defaults
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
}

/// Normal case: header.length == 0x16 (full buffer), zero contained
/// elements - sku_number decodes at its "no contained elements" offset
/// (0x15 + 0*0 == 0x15).
BOOST_AUTO_TEST_CASE(SystemEnclosureEntryFullBufferDecodesSkuNumberAtZeroElementsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x16);
    DMIHeader header = make_header(buffer);

    SystemEnclosureEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0x05));
}

/// Variable-offset regression/normal case: with a NON-ZERO contained
/// element count/record_length, the SKU number offset must be computed as
/// 0x15 + count*record_length, not assumed fixed at 0x15 or 0x19. Here
/// count == 2, record_length == 3, so the 6-byte contained-elements block
/// occupies 0x15-0x1A and sku_number_index sits at 0x1B.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntrySkuNumberOffsetIsComputedFromContainedElementsTestCase)
{
    std::vector<uint8_t> buffer;
    buffer.push_back(SMBios::SystemEnclosure); // 0x00 type = 3
    buffer.push_back(0x1C);                    // 0x01 length (patched exact)
    buffer.push_back(0x01);                    // 0x02 handle low byte
    buffer.push_back(0x00);                    // 0x03 handle high byte

    buffer.push_back(0x01); // 0x04 manufacturer_index
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Tower)); // 0x05 type
    buffer.push_back(0x02); // 0x06 version_index
    buffer.push_back(0x03); // 0x07 serial_number_index
    buffer.push_back(0x04); // 0x08 asset_tag_index

    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Safe));    // 0x09
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Warning)); // 0x0A
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Critical)); // 0x0B
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::None));    // 0x0C

    buffer.push_back(0x00); buffer.push_back(0x00); buffer.push_back(0x00); buffer.push_back(0x00); // 0x0D-0x10 oem_defined

    buffer.push_back(0x00); // 0x11 height
    buffer.push_back(0x00); // 0x12 number_of_power_cords
    buffer.push_back(0x02); // 0x13 contained_element_count = 2
    buffer.push_back(0x03); // 0x14 contained_element_record_length = 3

    // 0x15-0x1A: 2 contained elements * 3 bytes each = 6 bytes (opaque here,
    // not decoded by this class - see its class-level doc comment)
    for (uint8_t i = 0; i < 6; ++i) {
        buffer.push_back(static_cast<uint8_t>(0xA0 + i));
    }

    buffer.push_back(0x09); // 0x1B sku_number_index (0x15 + 2*3 == 0x1B)

    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(0x1C));

    append_string_terminator(buffer);

    DMIHeader header = make_header(buffer);
    SystemEnclosureEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_contained_element_count(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_contained_element_record_length(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0x09));
}

/// Invalid-input case: the same non-zero count/record_length, but the
/// buffer is one byte too short for the computed SKU offset (0x1B instead
/// of 0x1C) - sku_number_index must default, not read past the formatted
/// area.
BOOST_AUTO_TEST_CASE(SystemEnclosureEntrySkuNumberDefaultsWhenComputedOffsetExceedsLengthTestCase)
{
    std::vector<uint8_t> buffer;
    buffer.push_back(SMBios::SystemEnclosure); // 0x00 type = 3
    buffer.push_back(0x1B);                    // 0x01 length - one short of 0x1C
    buffer.push_back(0x01);                    // 0x02 handle low byte
    buffer.push_back(0x00);                    // 0x03 handle high byte

    buffer.push_back(0x01); // 0x04 manufacturer_index
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Tower)); // 0x05 type
    buffer.push_back(0x02); // 0x06 version_index
    buffer.push_back(0x03); // 0x07 serial_number_index
    buffer.push_back(0x04); // 0x08 asset_tag_index

    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Safe));
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Warning));
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::Critical));
    buffer.push_back(static_cast<uint8_t>(SystemEnclosureEntry::None));

    buffer.push_back(0x00); buffer.push_back(0x00); buffer.push_back(0x00); buffer.push_back(0x00);

    buffer.push_back(0x00); // 0x11 height
    buffer.push_back(0x00); // 0x12 number_of_power_cords
    buffer.push_back(0x02); // 0x13 contained_element_count = 2
    buffer.push_back(0x03); // 0x14 contained_element_record_length = 3

    for (uint8_t i = 0; i < 6; ++i) {
        buffer.push_back(static_cast<uint8_t>(0xA0 + i));
    }
    // deliberately no sku_number_index byte appended - length stops at 0x1B

    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(0x1B));

    append_string_terminator(buffer);

    DMIHeader header = make_header(buffer);
    SystemEnclosureEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
}

/// Milestone 0004 Story 02.0's own success criterion: SystemEnclosure
/// resolves through SMBiosEntryFactory::create() to a real
/// SystemEnclosureEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemEnclosureEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x16);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemEnclosureEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
