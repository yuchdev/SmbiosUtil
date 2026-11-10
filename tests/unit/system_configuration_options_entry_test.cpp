#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_configuration_options_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.hpp"
#include "field_assertions.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive SystemConfigurationOptionsEntry
// (smbios/include/smbios/tables/system_configuration_options_entry.hpp),
// cross-checked against DSP0134 3.9.0's own 7.13 (Table 57): the only
// fixed field is Count (offset 0x04); everything else is the trailing DMI
// string list - byte-for-byte identical in shape to OemStringsEntry (7.12)
// but a deliberately separate class (see this story's README "Key
// constraints").
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  count

BOOST_AUTO_TEST_SUITE(SystemConfigurationOptionsEntryUnitTests)

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

/// Edge case: zero configuration-option strings - Count is 0,
/// get_option_at() for any index falls back to dmi_string()'s own "Not
/// Specified".
BOOST_AUTO_TEST_CASE(ZeroOptionsTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x00 }; // 0x04 count = 0

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_option_count(), static_cast<uint8_t>(0));
    // No strings were ever added, so dmi_string()'s own internal string
    // table only holds index 0's "Not Specified" sentinel - index 1 is past
    // its end and falls into dmi_string()'s "Bad index" sentinel instead.
    BOOST_CHECK_EQUAL(entry.get_option_at(1), std::string("Bad index"));
}

/// Normal case: a single configuration-option string, 1-based index
/// matching dmi_string()'s own convention exactly.
BOOST_AUTO_TEST_CASE(OneOptionTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x01 }; // 0x04 count = 1

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, formatted_area,
                           std::vector<std::string>{"JP2: 1-2 Cache Size is 256K, 2-3 Cache Size is 512K"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_option_count(), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_option_at(1), std::string("JP2: 1-2 Cache Size is 256K, 2-3 Cache Size is 512K"));
}

/// Normal case: 3+ configuration-option strings - every index 1..Count
/// resolves to the matching string, in order.
BOOST_AUTO_TEST_CASE(ThreeOrMoreOptionsTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x03 }; // 0x04 count = 3

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, formatted_area,
                           std::vector<std::string>{"JP1: Enable", "JP2: Disable", "SW1-1: Close to Disable On Board Video"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_option_count(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_option_at(1), std::string("JP1: Enable"));
    BOOST_CHECK_EQUAL(entry.get_option_at(2), std::string("JP2: Disable"));
    BOOST_CHECK_EQUAL(entry.get_option_at(3), std::string("SW1-1: Close to Disable On Board Video"));
}

/// Invalid-input case: Count claims more strings than the string section
/// actually carries - out-of-range index must fall back to dmi_string()'s
/// own "Bad index" sentinel, not read out of bounds.
BOOST_AUTO_TEST_CASE(IndexBeyondAvailableOptionsReportsBadIndexTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x05 }; // 0x04 count = 5, but only 1 string follows

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, formatted_area, std::vector<std::string>{"Only One"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_option_count(), static_cast<uint8_t>(5));
    BOOST_CHECK_EQUAL(entry.get_option_at(1), std::string("Only One"));
    BOOST_CHECK_EQUAL(entry.get_option_at(2), std::string("Bad index"));
}

/// Milestone 0010 Story 02.0 Task 02's own success criterion:
/// SystemConfigurationOptions resolves through SMBiosEntryFactory::create()
/// to a real SystemConfigurationOptionsEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemConfigurationOptionsEntryTestCase)
{
    std::vector<uint8_t> formatted_area{ 0x01 };
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, formatted_area, std::vector<std::string>{"Acme"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemConfigurationOptionsEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 02)
//
// Type: unit - deterministic, synthetic buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(): "Option Count"
// then one "Option N" field per string, N 1-based, exactly the text's labels.
// Boundaries: header-only structure (count field absent -> count 0, no
// option fields); count larger than the string section ("Bad index").
// On failure first check: SystemConfigurationOptionsEntry::get_fields() in
// libsmbios/src/system_configuration_options_entry.cpp.

/// Normal case: count 3, three strings, uniquely named "Option 1".."Option 3".
BOOST_AUTO_TEST_CASE(SystemConfigurationOptionsEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, std::vector<uint8_t>{0x03},
                          std::vector<std::string>{"Alpha", "Bravo", "Charlie"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Option Count", num(3)},
        {"Option 1", str("Alpha")},
        {"Option 2", str("Bravo")},
        {"Option 3", str("Charlie")},
    });
}

/// Boundary case: header-only structure (length 0x04 < 0x05) - the count
/// getter defaults to 0, so only the count field is present.
BOOST_AUTO_TEST_CASE(SystemConfigurationOptionsEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, std::vector<uint8_t>{},
                          std::vector<std::string>{"Ignored"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Option Count", num(0)},
    });
}

/// Invalid-input case: count claims more strings than exist - the extra
/// field carries dmi_string()'s "Bad index" sentinel, exactly as the text does.
BOOST_AUTO_TEST_CASE(SystemConfigurationOptionsEntryGetFieldsCountBeyondStringsReportsBadIndexTestCase)
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemConfigurationOptions, std::vector<uint8_t>{0x02},
                          std::vector<std::string>{"Only"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    SystemConfigurationOptionsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Option Count", num(2)},
        {"Option 1", str("Only")},
        {"Option 2", str("Bad index")},
    });
}

BOOST_AUTO_TEST_SUITE_END()
