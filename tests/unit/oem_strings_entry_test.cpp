#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/oem_strings_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive OemStringsEntry
// (smbios/include/smbios/tables/oem_strings_entry.h), cross-checked against
// DSP0134 3.9.0's own 7.12 (Table 56): the only fixed field is Count
// (offset 0x04); everything else is the trailing DMI string list.
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  count

BOOST_AUTO_TEST_SUITE(OemStringsEntryUnitTests)

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

/// Edge case: zero OEM strings - Count is 0, get_string_at() for any index
/// falls back to dmi_string()'s own "Not Specified".
BOOST_AUTO_TEST_CASE(ZeroStringsTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x00 }; // 0x04 count = 0

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_string_count(), static_cast<uint8_t>(0));
    // No strings were ever added, so dmi_string()'s own internal string
    // table only holds index 0's "Not Specified" sentinel - index 1 is past
    // its end and falls into dmi_string()'s "Bad index" sentinel instead.
    BOOST_CHECK_EQUAL(entry.get_string_at(1), std::string("Bad index"));
}

/// Normal case: a single OEM string, 1-based index matching dmi_string()'s
/// own convention exactly.
BOOST_AUTO_TEST_CASE(OneStringTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x01 }; // 0x04 count = 1

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, formatted_area, std::vector<std::string>{"Part Number: 12345"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_string_count(), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_string_at(1), std::string("Part Number: 12345"));
}

/// Normal case: 3+ OEM strings - every index 1..Count resolves to the
/// matching string, in order.
BOOST_AUTO_TEST_CASE(ThreeOrMoreStringsTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x04 }; // 0x04 count = 4

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, formatted_area,
                           std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_string_count(), static_cast<uint8_t>(4));
    BOOST_CHECK_EQUAL(entry.get_string_at(1), std::string("Alpha"));
    BOOST_CHECK_EQUAL(entry.get_string_at(2), std::string("Bravo"));
    BOOST_CHECK_EQUAL(entry.get_string_at(3), std::string("Charlie"));
    BOOST_CHECK_EQUAL(entry.get_string_at(4), std::string("Delta"));
}

/// Invalid-input case: Count claims more strings than the string section
/// actually carries - out-of-range index must fall back to dmi_string()'s
/// own "Bad index" sentinel, not read out of bounds.
BOOST_AUTO_TEST_CASE(IndexBeyondAvailableStringsReportsBadIndexTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x05 }; // 0x04 count = 5, but only 2 strings follow

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, formatted_area, std::vector<std::string>{"One", "Two"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_string_count(), static_cast<uint8_t>(5));
    BOOST_CHECK_EQUAL(entry.get_string_at(1), std::string("One"));
    BOOST_CHECK_EQUAL(entry.get_string_at(2), std::string("Two"));
    BOOST_CHECK_EQUAL(entry.get_string_at(3), std::string("Bad index"));
}

/// Milestone 0010 Story 02.0 Task 02's own success criterion: OemStrings
/// resolves through SMBiosEntryFactory::create() to a real OemStringsEntry,
/// not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsOemStringsEntryTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x01 };
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, formatted_area, std::vector<std::string>{"Acme"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<OemStringsEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
