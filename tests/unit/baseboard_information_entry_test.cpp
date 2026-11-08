#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/baseboard_information_entry.h>
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
// smbios::test::SyntheticTableBuilder) drive BaseboardInformationEntry
// (smbios/include/smbios/tables/baseboard_information_entry.h) across the
// header.length tiers named by docs/roadmap/0010-.../02.0-.../
// 01-baseboard-information.md, cross-checked against DSP0134 3.9.0's own
// 7.3/7.3.1/7.3.2 tables and c-versions/dmidecode_linux/dmidecode.c's case 2
// ("Base Board Information"):
//
//   tier 1: header.length == 0x08 (spec's own stated minimum) -> only
//           manufacturer/product/version/serial_number (offsets 0x04-0x07,
//           4 formatted bytes) decode; asset_tag (needs length >= 0x09)
//           onward still defaults
//   tier 2: feature flags byte present (length >= 0x0A) but Number of
//           Contained Object Handles == 0 -> footnote 1's "not present"
//           case, distinct from "present, all bits clear"
//   tier 3: full buffer with 2+ Contained Object Handles -> every field
//           decodes, including the trailing WORD array
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  manufacturer_index
//   offset 0x05 : uint8_t  product_index
//   offset 0x06 : uint8_t  version_index
//   offset 0x07 : uint8_t  serial_number_index
//   offset 0x08 : uint8_t  asset_tag_index
//   offset 0x09 : uint8_t  feature_flags
//   offset 0x0A : uint8_t  location_in_chassis_index
//   offset 0x0B : uint16_t chassis_handle
//   offset 0x0D : uint8_t  board_type
//   offset 0x0E : uint8_t  contained_object_handle_count (n)
//   offset 0x0F : n WORDs, contained object handles

BOOST_AUTO_TEST_SUITE(BaseboardInformationEntryUnitTests)

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

} // namespace

/// Normal case (and spec-literal minimum): header.length == 0x08 - per
/// DSP0134 3.9.0 Table 13's own "at least 08h" minimum, this covers only
/// manufacturer/product/version/serial_number (offsets 0x04-0x07, 4
/// formatted bytes); asset_tag (offset 0x08) needs length >= 0x09 and so
/// still defaults, along with every field beyond it.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDecodesFourStringIndicesOnlyTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x01, // 0x04 manufacturer_index
        0x02, // 0x05 product_index
        0x03, // 0x06 version_index
        0x04, // 0x07 serial_number_index
    };
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x08));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BaseboardInformation, formatted_area,
                           std::vector<std::string>{"Acme", "Model X", "1.0", "SN123"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BaseboardInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_string(), std::string("Acme"));
    BOOST_CHECK_EQUAL(entry.get_product_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_product_string(), std::string("Model X"));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_version_string(), std::string("1.0"));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0x04));
    BOOST_CHECK_EQUAL(entry.get_serial_number_string(), std::string("SN123"));

    // length == 0x08, asset_tag needs length >= 0x09 -> defaults
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_feature_flags(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_location_in_chassis_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_chassis_handle(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_board_type(), static_cast<uint8_t>(BaseboardInformationEntry::BoardTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_count(), static_cast<uint8_t>(0));

    // footnote 1's "not present" case also holds here, since length < 0x0E
    BOOST_CHECK_EQUAL(entry.get_feature_flags_string(), std::string("\tNot Present\n"));
}

/// Edge case: the feature flags byte is physically present (length >=
/// 0x0A) and non-zero, but Number of Contained Object Handles == 0 -
/// footnote 1's condition is NOT met, so get_feature_flags_string() must
/// report "Not Present", not "present, these bits are set."
BOOST_AUTO_TEST_CASE(FeatureFlagsPresentButZeroContainedHandlesReportsNotPresentTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x01, // 0x04 manufacturer_index
        0x02, // 0x05 product_index
        0x03, // 0x06 version_index
        0x04, // 0x07 serial_number_index
        0x05, // 0x08 asset_tag_index
        static_cast<uint8_t>(BaseboardInformationEntry::HostingBoard | BaseboardInformationEntry::Removable), // 0x09 feature_flags
        0x06, // 0x0A location_in_chassis_index
        0x34, 0x12, // 0x0B-0x0C chassis_handle = 0x1234
        static_cast<uint8_t>(BaseboardInformationEntry::Motherboard), // 0x0D board_type
        0x00, // 0x0E contained_object_handle_count = 0
    };
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x0F));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BaseboardInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BaseboardInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    // The raw byte itself is readable and non-zero...
    BOOST_CHECK_EQUAL(entry.get_feature_flags(),
                      static_cast<uint8_t>(BaseboardInformationEntry::HostingBoard | BaseboardInformationEntry::Removable));
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_count(), static_cast<uint8_t>(0));

    // ...but footnote 1's condition (count != 0) is not met, so the string
    // representation must still say "Not Present", distinct from "None".
    BOOST_CHECK_EQUAL(entry.get_feature_flags_string(), std::string("\tNot Present\n"));

    BOOST_CHECK_EQUAL(entry.get_location_in_chassis_index(), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry.get_chassis_handle(), static_cast<uint16_t>(0x1234));
    BOOST_CHECK_EQUAL(entry.get_board_type(), static_cast<uint8_t>(BaseboardInformationEntry::Motherboard));
    BOOST_CHECK_EQUAL(entry.get_board_type_string(), std::string("Motherboard (includes processor, memory, and I/O)"));
}

/// Normal case: full buffer with 2 Contained Object Handles - every field
/// decodes, feature flags footnote 1's condition IS met (count != 0, so
/// "None"/bit-list rendering applies instead of "Not Present"), and the
/// trailing WORD array is read via get_contained_object_handle_at().
BOOST_AUTO_TEST_CASE(FullBufferWithTwoContainedObjectHandlesDecodesTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x01, // 0x04 manufacturer_index
        0x02, // 0x05 product_index
        0x03, // 0x06 version_index
        0x04, // 0x07 serial_number_index
        0x05, // 0x08 asset_tag_index
        static_cast<uint8_t>(BaseboardInformationEntry::HostingBoard), // 0x09 feature_flags
        0x06, // 0x0A location_in_chassis_index
        0x34, 0x12, // 0x0B-0x0C chassis_handle = 0x1234
        static_cast<uint8_t>(BaseboardInformationEntry::DaughterBoard), // 0x0D board_type
        0x02, // 0x0E contained_object_handle_count = 2
        0xAD, 0xDE, // 0x0F-0x10 handle[0] = 0xDEAD
        0xEF, 0xBE, // 0x11-0x12 handle[1] = 0xBEEF
    };
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x13));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BaseboardInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BaseboardInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_count(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_at(0), static_cast<uint16_t>(0xDEAD));
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_at(1), static_cast<uint16_t>(0xBEEF));

    // Index out of range for the count guard -> defaults, no out-of-bounds read.
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_at(2), static_cast<uint16_t>(0));

    BOOST_CHECK_EQUAL(entry.get_board_type_string(), std::string("Daughter Board"));

    // Footnote 1's condition is met (length >= 0x0E and count != 0), and
    // exactly one bit is set.
    BOOST_CHECK_EQUAL(entry.get_feature_flags_string(), std::string("\tBoard is a hosting board\n"));
}

/// Invalid-input case: a non-zero Contained Object Handle count, but the
/// buffer is one byte too short for the computed handle offset - the
/// second guard (get_entry_size()) must still prevent an out-of-bounds
/// read, defaulting to 0 rather than reading past the formatted area.
BOOST_AUTO_TEST_CASE(ContainedObjectHandleDefaultsWhenBufferTooShortForComputedOffsetTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x01, 0x02, 0x03, 0x04, 0x05, // 0x04-0x08 string indices
        0x00, // 0x09 feature_flags
        0x00, // 0x0A location_in_chassis_index
        0x00, 0x00, // 0x0B-0x0C chassis_handle
        static_cast<uint8_t>(BaseboardInformationEntry::Unknown), // 0x0D board_type
        0x02, // 0x0E contained_object_handle_count = 2 (claims 2, but...)
        0xAD, 0xDE, // 0x0F-0x10 handle[0] only - handle[1] is missing
    };
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x11));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BaseboardInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BaseboardInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_count(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_at(0), static_cast<uint16_t>(0xDEAD));
    // handle[1] would need length >= 0x13, but the structure is only 0x11 long.
    BOOST_CHECK_EQUAL(entry.get_contained_object_handle_at(1), static_cast<uint16_t>(0));
}

/// Milestone 0010 Story 02.0 Task 01's own success criterion:
/// BaseboardInformation resolves through SMBiosEntryFactory::create() to a
/// real BaseboardInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsBaseboardInformationEntryTestCase)
{
    std::vector<uint8_t> formatted_area{0x01, 0x02, 0x03, 0x04};
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BaseboardInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<BaseboardInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
