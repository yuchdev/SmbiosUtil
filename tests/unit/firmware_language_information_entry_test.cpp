#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/firmware_language_information_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive FirmwareLanguageInformationEntry
// (smbios/include/smbios/tables/firmware_language_information_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.14 (Table 58) and
// its own worked Example 1 (long format) / Example 2 (abbreviated format).
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  installable_languages
//   offset 0x05 : uint8_t  flags (2.1+)
//   offset 0x06 : 15 reserved bytes
//   offset 0x15 : uint8_t  current_language_index (STRING)

BOOST_AUTO_TEST_SUITE(FirmwareLanguageInformationEntryUnitTests)

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

/// Build a full-length (0x16) formatted area matching 7.14's Table 58, for
/// the given installable-language count, flags byte and current-language
/// index.
std::vector<uint8_t> make_formatted_area(uint8_t installable_languages, uint8_t flags, uint8_t current_language_index)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(installable_languages); // 0x04
    formatted_area.push_back(flags);                 // 0x05
    for (int i = 0; i < 15; ++i) formatted_area.push_back(0x00); // 0x06-0x14 reserved
    formatted_area.push_back(current_language_index); // 0x15
    return formatted_area;
}

} // namespace

/// Normal case: 7.14's own Example 1 (long format) verbatim - three
/// languages, long-format strings, current language index 2 ("fr|CA|...").
BOOST_AUTO_TEST_CASE(LongFormatExampleOneFromSpecTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(/*installable_languages*/3, /*flags*/0x00, /*current*/2);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x16));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area,
                           std::vector<std::string>{"en|US|iso8859-1", "fr|CA|iso8859-1", "ja|JP|unicode"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_installable_language_count(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_flags(), static_cast<uint8_t>(0x00));
    BOOST_CHECK(!entry.is_abbreviated_format());
    BOOST_CHECK_EQUAL(entry.get_current_language_index(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_current_language_string(), std::string("fr|CA|iso8859-1"));

    BOOST_CHECK_EQUAL(entry.get_language_at(0), std::string("en|US|iso8859-1"));
    BOOST_CHECK_EQUAL(entry.get_language_at(1), std::string("fr|CA|iso8859-1"));
    BOOST_CHECK_EQUAL(entry.get_language_at(2), std::string("ja|JP|unicode"));
}

/// Normal case: 7.14's own Example 2 (abbreviated format) verbatim - same
/// three languages, abbreviated two-character codes, Flags bit 0 set.
BOOST_AUTO_TEST_CASE(AbbreviatedFormatExampleTwoFromSpecTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(/*installable_languages*/3,
                                                                /*flags*/FirmwareLanguageInformationEntry::AbbreviatedFormat,
                                                                /*current*/2);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x16));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area,
                           std::vector<std::string>{"enUS", "frCA", "jaJP"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_installable_language_count(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_flags(), static_cast<uint8_t>(FirmwareLanguageInformationEntry::AbbreviatedFormat));
    BOOST_CHECK(entry.is_abbreviated_format());
    BOOST_CHECK_EQUAL(entry.get_current_language_string(), std::string("frCA"));

    BOOST_CHECK_EQUAL(entry.get_language_at(0), std::string("enUS"));
    BOOST_CHECK_EQUAL(entry.get_language_at(1), std::string("frCA"));
    BOOST_CHECK_EQUAL(entry.get_language_at(2), std::string("jaJP"));
}

/// Invalid-input/edge case: pre-2.1 SMBiosVersion must not expose a
/// meaningful Flags value even though the raw byte is physically present
/// (get_entry_size() >= 0x06) - is_abbreviated_format() must report false
/// (a sensible "not applicable" default), not garbage derived from reading
/// a byte that did not exist as a defined field in that revision.
BOOST_AUTO_TEST_CASE(Pre21VersionGuardsFlagsFieldTestCase)
{
    // Physically set the abbreviated-format bit in the raw byte, to prove
    // the guard is about the *version*, not the byte's own value.
    std::vector<uint8_t> formatted_area = make_formatted_area(/*installable_languages*/1,
                                                                /*flags*/FirmwareLanguageInformationEntry::AbbreviatedFormat,
                                                                /*current*/1);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x16));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area, std::vector<std::string>{"en|US|iso8859-1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_flags(), static_cast<uint8_t>(0));
    BOOST_CHECK(!entry.is_abbreviated_format());

    // Fields outside the version gate still decode normally.
    BOOST_CHECK_EQUAL(entry.get_installable_language_count(), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_current_language_string(), std::string("en|US|iso8859-1"));
}

/// Invalid-input case: an out-of-range get_language_at() index (>= the
/// Installable Languages count) must fall back to dmi_string()'s own "Not
/// Specified", not read out of bounds into an unrelated string.
BOOST_AUTO_TEST_CASE(LanguageIndexBeyondCountReportsNotSpecifiedTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(/*installable_languages*/2, /*flags*/0x00, /*current*/1);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x16));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area,
                           std::vector<std::string>{"en|US|iso8859-1", "fr|CA|iso8859-1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_language_at(1), std::string("fr|CA|iso8859-1"));
    BOOST_CHECK_EQUAL(entry.get_language_at(2), std::string("Not Specified"));
}

/// Milestone 0010 Story 02.0 Task 02's own success criterion:
/// FirmwareLanguageInformation resolves through SMBiosEntryFactory::create()
/// to a real FirmwareLanguageInformationEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsFirmwareLanguageInformationEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(/*installable_languages*/1, /*flags*/0x00, /*current*/1);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area, std::vector<std::string>{"en|US|iso8859-1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<FirmwareLanguageInformationEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 02)
//
// Type: unit - deterministic, synthetic buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(): count, the
// "Abbreviated Format" flag as a bool, the current language, then each
// language under the text's own 0-based "Language N" label.
// Boundaries: pre-2.1 version (flag forced false), header-only structure
// (count 0, current language "Not Specified").
// On failure first check: FirmwareLanguageInformationEntry::get_fields() in
// libsmbios/src/firmware_language_information_entry.cpp.

/// Normal case: 7.14's Example 2 (abbreviated format) at SMBIOS 2.1.
BOOST_AUTO_TEST_CASE(FirmwareLanguageInformationEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_formatted_area(3, FirmwareLanguageInformationEntry::AbbreviatedFormat, 2);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area,
                          std::vector<std::string>{"enUS", "frCA", "jaJP"});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Installable Languages", num(3)},
        {"Abbreviated Format", true},
        {"Current Language", str("frCA")},
        {"Language 0", str("enUS")},
        {"Language 1", str("frCA")},
        {"Language 2", str("jaJP")},
    });
}

/// Boundary cases: a pre-2.1 table reports the flag as false (the text
/// prints "No"), and a header-only structure defaults every field.
BOOST_AUTO_TEST_CASE(FirmwareLanguageInformationEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> formatted_area = make_formatted_area(1, FirmwareLanguageInformationEntry::AbbreviatedFormat, 1);
        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::FirmwareLanguageInformation, formatted_area,
                              std::vector<std::string>{"enUS"});
        std::vector<uint8_t> buffer = builder.build();
        DMIHeader header = make_header(buffer);
        FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Installable Languages", num(1)},
            {"Abbreviated Format", false},
            {"Current Language", str("enUS")},
            {"Language 0", str("enUS")},
        });
    }
    {
        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::FirmwareLanguageInformation, std::vector<uint8_t>{});
        std::vector<uint8_t> buffer = builder.build();
        DMIHeader header = make_header(buffer);
        FirmwareLanguageInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Installable Languages", num(0)},
            {"Abbreviated Format", false},
            {"Current Language", str("Not Specified")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
