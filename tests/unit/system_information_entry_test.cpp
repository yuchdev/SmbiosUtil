#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_information_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive SystemInformationEntry (smbios/include/smbios/
// system_information_entry.hpp) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../01-system-information-and-enclosure.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 1:
//   tier 1: header.length < 0x08   -> every length-guarded getter defaults
//   tier 2: header.length == 0x08  -> manufacturer..serial_number decode;
//                                      uuid/wake_up_type still default
//   tier 3: header.length == 0x19  -> uuid + wake_up_type also decode;
//                                      sku_number/family still default
//   tier 4: header.length == 0x1B  -> sku_number + family also decode
//
// Wire layout:
//   offset 0x00 : uint8_t   type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t   length
//   offset 0x02 : uint16_t  handle
//   offset 0x04 : uint8_t   manufacturer_index
//   offset 0x05 : uint8_t   product_name_index
//   offset 0x06 : uint8_t   version_index
//   offset 0x07 : uint8_t   serial_number_index
//   offset 0x08 : 16 bytes  uuid
//   offset 0x18 : uint8_t   wake_up_type
//   offset 0x19 : uint8_t   sku_number_index
//   offset 0x1A : uint8_t   family_index

BOOST_AUTO_TEST_SUITE(SystemInformationEntryUnitTests)

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

/// @brief Build a full-length (0x1B) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` (via
/// SyntheticTableBuilder::add_structure(), which computes header.length
/// automatically from the formatted area's size). `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length)
{
    std::vector<uint8_t> full_formatted_area{
        0x01, // 0x04 manufacturer_index
        0x02, // 0x05 product_name_index
        0x03, // 0x06 version_index
        0x04, // 0x07 serial_number_index
    };

    // 0x08-0x17 uuid - 16 distinct bytes
    for (uint8_t i = 0; i < 16; ++i) {
        full_formatted_area.push_back(static_cast<uint8_t>(0x10 + i));
    }

    full_formatted_area.push_back(static_cast<uint8_t>(SystemInformationEntry::PowerSwitch)); // 0x18 wake_up_type
    full_formatted_area.push_back(0x05); // 0x19 sku_number_index
    full_formatted_area.push_back(0x06); // 0x1A family_index

    BOOST_REQUIRE_EQUAL(full_formatted_area.size(), static_cast<size_t>(0x1B - 4));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), static_cast<size_t>(0x1B));
    BOOST_REQUIRE_GE(static_cast<size_t>(length), static_cast<size_t>(4));

    std::vector<uint8_t> formatted_area(full_formatted_area.begin(),
                                         full_formatted_area.begin() + (length - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemInformation, formatted_area);
    return builder.build();
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x08 means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(SystemInformationEntryTruncatedBelow0x08DefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_product_name_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_wake_up_type(), static_cast<uint8_t>(SystemInformationEntry::Reserved));
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_family_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_uuid_string(), "Not Specified");
}

/// Normal case: header.length == 0x08 - manufacturer through serial_number
/// decode; uuid/wake_up_type (need length >= 0x19) still default.
BOOST_AUTO_TEST_CASE(SystemInformationEntryTier0x08DecodesThroughSerialNumberTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x08);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_product_name_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0x04));

    // length == 0x08, uuid/wake_up_type need length >= 0x19 -> default
    BOOST_CHECK_EQUAL(entry.get_uuid_string(), "Not Specified");
    BOOST_CHECK_EQUAL(entry.get_wake_up_type(), static_cast<uint8_t>(SystemInformationEntry::Reserved));
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_family_index(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x19 - uuid + wake_up_type also decode;
/// sku_number/family (need length >= 0x1B) still default.
BOOST_AUTO_TEST_CASE(SystemInformationEntryTier0x19DecodesUuidAndWakeUpTypeTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x19);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_wake_up_type(), static_cast<uint8_t>(SystemInformationEntry::PowerSwitch));
    BOOST_CHECK_NE(entry.get_uuid_string(), "Not Specified");

    // length == 0x19, sku_number/family need length >= 0x1B -> default
    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_family_index(), static_cast<uint8_t>(0));
}

/// Normal case: header.length == 0x1B (full buffer) - sku_number + family
/// also decode.
BOOST_AUTO_TEST_CASE(SystemInformationEntryFullBufferDecodesAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1B);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_sku_number_index(), static_cast<uint8_t>(0x05));
    BOOST_CHECK_EQUAL(entry.get_family_index(), static_cast<uint8_t>(0x06));
}

/// Milestone 0004 Story 02.0's own success criterion: SystemInformation
/// resolves through SMBiosEntryFactory::create() to a real
/// SystemInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemInformationEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1B);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// UUID formatting fixture
//
// Dedicated fixture per the task brief: a known 16-byte sequence
// (0x00..0x0F) and its expected hyphenated-hex form, covering the real
// byte-order quirk confirmed against dmi_system_uuid() in
// c-versions/dmidecode_linux/dmidecode.c (~line 449):
//  - >= 2.6: the first 3 fields are byte-swapped to big-endian-looking hex
//    (p[3] p[2] p[1] p[0] - p[5] p[4] - p[7] p[6] - p[8] p[9] - p[10..15])
//  - < 2.6: no swapping at all (p[0] through p[15] straight through) - this
//    is the regression a copy-paste-without-the-version-check would
//    produce if the >= 2.6 byte order were applied unconditionally.
//  - sentinels (all 0xFF / all 0x00) checked before the version branch.

BOOST_AUTO_TEST_SUITE(SystemInformationEntryUuidFormattingUnitTests)

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

/// @brief Build a length-0x19 buffer (through wake_up_type) whose UUID
/// bytes (0x08-0x17) are exactly `uuid_bytes`.
std::vector<uint8_t> build_uuid_buffer(const std::vector<uint8_t>& uuid_bytes)
{
    BOOST_REQUIRE_EQUAL(uuid_bytes.size(), static_cast<size_t>(16));

    std::vector<uint8_t> formatted_area{
        0x00, // 0x04 manufacturer_index
        0x00, // 0x05 product_name_index
        0x00, // 0x06 version_index
        0x00, // 0x07 serial_number_index
    };
    formatted_area.insert(formatted_area.end(), uuid_bytes.begin(), uuid_bytes.end()); // 0x08-0x17 uuid
    formatted_area.push_back(SystemInformationEntry::Reserved); // 0x18 wake_up_type

    BOOST_REQUIRE_EQUAL(formatted_area.size(), static_cast<size_t>(0x19 - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemInformation, formatted_area);
    return builder.build();
}

} // namespace

/// Normal case: a known, non-sentinel UUID at version >= 2.6 is byte-swapped
/// in its first 3 fields.
BOOST_AUTO_TEST_CASE(UuidFormattingSwapsFirstThreeFieldsAtVersion26OrAboveTestCase)
{
    std::vector<uint8_t> uuid_bytes;
    for (uint8_t i = 0; i < 16; ++i) uuid_bytes.push_back(i);

    std::vector<uint8_t> buffer = build_uuid_buffer(uuid_bytes);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_uuid_string(), "03020100-0504-0706-0809-0a0b0c0d0e0f");
}

/// Regression test: the SAME raw bytes at version < 2.6 must NOT be
/// byte-swapped - this is the regression a copy-paste-without-the-
/// version-check would produce.
BOOST_AUTO_TEST_CASE(UuidFormattingDoesNotSwapBelowVersion26TestCase)
{
    std::vector<uint8_t> uuid_bytes;
    for (uint8_t i = 0; i < 16; ++i) uuid_bytes.push_back(i);

    std::vector<uint8_t> buffer = build_uuid_buffer(uuid_bytes);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_uuid_string(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

/// Edge case: all 16 bytes 0xFF is the "Not Present" sentinel, checked
/// before the version branch (so the version used here is irrelevant).
BOOST_AUTO_TEST_CASE(UuidFormattingAllOxFFIsNotPresentTestCase)
{
    std::vector<uint8_t> uuid_bytes(16, 0xFF);

    std::vector<uint8_t> buffer = build_uuid_buffer(uuid_bytes);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_uuid_string(), "Not Present");
}

/// Edge case: all 16 bytes 0x00 is the "Not Settable" sentinel, checked
/// before the version branch.
BOOST_AUTO_TEST_CASE(UuidFormattingAllOx00IsNotSettableTestCase)
{
    std::vector<uint8_t> uuid_bytes(16, 0x00);

    std::vector<uint8_t> buffer = build_uuid_buffer(uuid_bytes);
    DMIHeader header = make_header(buffer);

    SystemInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_uuid_string(), "Not Settable");
}

BOOST_AUTO_TEST_SUITE_END()
