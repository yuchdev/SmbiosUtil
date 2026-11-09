#include <cstdint>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>

#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;
using namespace smbios::test;

// Unit tests for SyntheticTableBuilder itself (milestone 0006, Story 01.0,
// Task 01). Deterministic, no hardware/privileges: every table here is
// built in-memory and fed through the real SMBios::read_smbios_table() via
// the SmbiosTableWalkTestAccess seam (synthetic_table_builder.hpp), never
// through any ISmbiosSource.

BOOST_AUTO_TEST_SUITE(SyntheticTableBuilderUnitTests)

//////////////////////////////////////////////////////////////////////////
// Normal case: two well-formed structures, one with strings, one without,
// fed through the real table walk.

BOOST_AUTO_TEST_CASE(BuildTwoStructuresAreFoundWithExpectedFieldsTestCase)
{
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemInformation, std::vector<uint8_t>{0xAA, 0xBB, 0xCC, 0xDD},
                           std::vector<std::string>{"Manufacturer", "ProductName"});
    builder.add_structure(SMBios::SystemEnclosure, std::vector<uint8_t>{0x01, 0x02});

    std::vector<uint8_t> buffer = builder.build();

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    // get_structures_count()/count_smbios_structures() counts every
    // length-valid structure walked, including the EndOfTable marker
    // build() appends (see smbios_table_walk_test.cpp's
    // ReadSmbiosTableParsesWellFormedTableWithExactBoundaryHeaderTestCase
    // for the same, pre-existing, documented behavior) - so this is 2 real
    // structures + 1 EndOfTable = 3. begin()/end() iteration below is what
    // actually excludes the EndOfTable marker.
    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(3));

    auto it = smbios_instance.begin();
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(SMBios::SystemInformation));
    BOOST_CHECK_EQUAL((*it).get_length(), static_cast<size_t>(8)); // 4-byte header + 4-byte formatted area
    BOOST_CHECK_EQUAL((*it).handle, static_cast<uint16_t>(1));

    ++it;
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(SMBios::SystemEnclosure));
    BOOST_CHECK_EQUAL((*it).get_length(), static_cast<size_t>(6)); // 4-byte header + 2-byte formatted area
    BOOST_CHECK_EQUAL((*it).handle, static_cast<uint16_t>(2));

    ++it;
    BOOST_CHECK(it == smbios_instance.end());
}

/// Confirms the strings written for the first structure land exactly where
/// AbstractSMBiosEntry::parse_dmi_strings() expects them: immediately after
/// the formatted area, each NUL-terminated, closed by the double-NUL
/// marker. Walked here directly off the raw buffer rather than via an
/// AbstractSMBiosEntry subclass, since this test is about the builder's
/// byte layout, not any particular entry type's parsing.
BOOST_AUTO_TEST_CASE(BuildStringSectionIsLaidOutAsDoubleNulTerminatedTestCase)
{
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemInformation, std::vector<uint8_t>{0x01, 0x02},
                           std::vector<std::string>{"Hello", "World"});

    std::vector<uint8_t> buffer = builder.build();

    // header(4) + formatted area(2) = offset 6 is where the string section
    // starts: "Hello\0World\0\0"
    const std::string expected_tail = std::string("Hello") + '\0' + "World" + '\0' + '\0';
    BOOST_REQUIRE_GE(buffer.size(), static_cast<size_t>(6 + expected_tail.size()));
    std::string actual_tail(reinterpret_cast<const char*>(buffer.data() + 6), expected_tail.size());
    BOOST_CHECK(actual_tail == expected_tail);
}

//////////////////////////////////////////////////////////////////////////
// Edge case: a structure with no strings at all - just the bare double-NUL
// marker - still walks cleanly and produces zero strings.

BOOST_AUTO_TEST_CASE(BuildStructureWithNoStringsTestCase)
{
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemEnclosure, std::vector<uint8_t>{});

    std::vector<uint8_t> buffer = builder.build();

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    // 1 real structure + 1 EndOfTable marker - see the comment in
    // BuildTwoStructuresAreFoundWithExpectedFieldsTestCase above.
    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(2));

    auto it = smbios_instance.begin();
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(SMBios::SystemEnclosure));
    BOOST_CHECK_EQUAL((*it).get_length(), static_cast<size_t>(4)); // header only, no formatted area
    ++it;
    BOOST_CHECK(it == smbios_instance.end());
}

//////////////////////////////////////////////////////////////////////////
// Invalid-input / escape-hatch case: add_raw_bytes() bypasses the
// well-formed-by-default logic and suppresses the automatic EndOfTable
// marker, so a deliberately malformed table (no terminator at all) can be
// constructed byte-for-byte - the contract Story 02.0 depends on.

BOOST_AUTO_TEST_CASE(AddRawBytesSuppressesAutomaticEndOfTableMarkerTestCase)
{
    SyntheticTableBuilder builder;
    builder.add_raw_bytes(std::vector<uint8_t>{1, 4, 0x01, 0x00, 0, 0});

    std::vector<uint8_t> buffer = builder.build();

    // No EndOfTable (127, 4, ...) appended: the buffer is exactly what was
    // passed to add_raw_bytes(), untouched.
    const std::vector<uint8_t> expected{1, 4, 0x01, 0x00, 0, 0};
    BOOST_CHECK(buffer == expected);
}

BOOST_AUTO_TEST_SUITE_END()
