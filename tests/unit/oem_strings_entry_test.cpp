#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/oem_strings_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive OemStringsEntry
// (smbios/include/smbios/tables/oem_strings_entry.hpp), cross-checked against
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

// get_fields() (milestone 0009, Story 01.0, Task 02)
//
// Type: unit - deterministic, synthetic buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(): "String Count"
// then one "String N" field per string, N 1-based, exactly the text's labels.
// Boundaries: header-only structure (count field absent -> count 0, no
// string fields); count larger than the string section ("Bad index").
// On failure first check: OemStringsEntry::get_fields() in
// libsmbios/src/oem_strings_entry.cpp.

/// Normal case: count 3, three strings, uniquely named "String 1".."String 3".
BOOST_AUTO_TEST_CASE(OemStringsEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, std::vector<uint8_t>{0x03},
                          std::vector<std::string>{"Alpha", "Bravo", "Charlie"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"String Count", num(3)},
        {"String 1", str("Alpha")},
        {"String 2", str("Bravo")},
        {"String 3", str("Charlie")},
    });
}

/// Boundary case: header-only structure (length 0x04 < 0x05) - the count
/// getter defaults to 0, so only the count field is present.
BOOST_AUTO_TEST_CASE(OemStringsEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, std::vector<uint8_t>{},
                          std::vector<std::string>{"Ignored"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"String Count", num(0)},
    });
}

/// Invalid-input case: count claims more strings than exist - the extra
/// field carries dmi_string()'s "Bad index" sentinel, exactly as the text does.
BOOST_AUTO_TEST_CASE(OemStringsEntryGetFieldsCountBeyondStringsReportsBadIndexTestCase)
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, std::vector<uint8_t>{0x02},
                          std::vector<std::string>{"Only"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"String Count", num(2)},
        {"String 1", str("Only")},
        {"String 2", str("Bad index")},
    });
}

// Regression: security finding HIGH-1 (CWE-835/770), see
// docs/security/2026-11-10-structured-field-model.md.
//
// Type: unit - deterministic, synthetic buffers, no hardware/privileges.
// Scenario: Count = 0xFF (spec-legal maximum). The 1-based loop over
// 1..Count used a uint8_t counter, which wrapped 255 -> 0 and never
// terminated, growing the output until allocation failed.
// Boundaries: count = 255 with only two real strings - indices 3..255 fall
// back to dmi_string()'s "Bad index" sentinel; output must stop at index 255.
// On failure first check: the loop counter type in OemStringsEntry::get_fields()
// / render_to_description() (libsmbios/src/oem_strings_entry.cpp) - it must be wider than uint8_t. A
// timeout (rather than an assertion failure) means the wrap-around is back.

/// Regression case: get_fields() with Count = 0xFF returns exactly 256 fields
/// ("String Count" + "String 1".."String 255") and terminates.
BOOST_AUTO_TEST_CASE(OemStringsEntryGetFieldsMaxCountTerminatesTestCase, *boost::unit_test::timeout(10))
{
    using namespace smbios::test;
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, std::vector<uint8_t>{0xFF},
                          std::vector<std::string>{"First", "Second"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    std::vector<NamedField> expected{{"String Count", num(255)}};
    for (unsigned index = 1; index <= 255; ++index) {
        const char* value = index == 1 ? "First" : index == 2 ? "Second" : "Bad index";
        expected.push_back({"String " + std::to_string(index), str(value)});
    }

    const std::vector<NamedField> fields = entry.get_fields();
    BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(256));
    check_fields_equal(fields, expected);
}

/// Regression case: render_to_description() with Count = 0xFF terminates and
/// emits exactly 256 count/item lines ("String Count" + 255 "String N"),
/// after the "Header type" line.
BOOST_AUTO_TEST_CASE(OemStringsEntryRenderMaxCountTerminatesTestCase, *boost::unit_test::timeout(10))
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, std::vector<uint8_t>{0xFF},
                          std::vector<std::string>{"First", "Second"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    OemStringsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    std::string expected = "Header type: OEM Strings\nString Count: 255\n";
    for (unsigned index = 1; index <= 255; ++index) {
        const char* value = index == 1 ? "First" : index == 2 ? "Second" : "Bad index";
        expected += "String " + std::to_string(index) + ": " + value + "\n";
    }

    const std::string description = entry.render_to_description();
    std::istringstream lines(description);
    std::string line;
    size_t item_lines = 0;
    while (std::getline(lines, line)) {
        if (line.rfind("String ", 0) == 0) ++item_lines;
    }
    BOOST_CHECK_EQUAL(item_lines, static_cast<size_t>(256));
    BOOST_CHECK_EQUAL(description, expected);
}

BOOST_AUTO_TEST_SUITE_END()
