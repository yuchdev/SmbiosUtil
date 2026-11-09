#include <cstdint>
#include <string>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios.hpp>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive AbstractSMBiosEntry::parse_dmi_strings() directly (via a
// minimal concrete subclass, since AbstractSMBiosEntry itself is abstract)
// to confirm it is bounds-checked against table_end (02-bounds-checked-
// string-parsing.md). Follows the house style of
// tests/unit/port_connection_entry_test.cpp / bios_information_entry_test.cpp
// for synthetic DMIHeader + buffer construction.

BOOST_AUTO_TEST_SUITE(DmiStringParsingUnitTests)

namespace {

/// @brief Minimal concrete AbstractSMBiosEntry so the test can drive
/// parse_dmi_strings() through the real constructor without pulling in any
/// of the three production entry classes. Exposes the protected
/// dmi_string() accessor so the test can inspect what was actually parsed.
class TestEntry : public AbstractSMBiosEntry {
public:
    TestEntry(const DMIHeader& header, const uint8_t* table_end)
        : AbstractSMBiosEntry(header, table_end) {}

    std::string get_type() const override { return "Test"; }
    std::string render_to_description() const override { return ""; }

    using AbstractSMBiosEntry::dmi_string;
};

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

/// Normal case: a well-formed, fully double-NUL-terminated string section
/// parses both strings in order.
BOOST_AUTO_TEST_CASE(ParseDmiStringsWellFormedDoubleNulTerminatedTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(0x00 /* type, arbitrary, unused by TestEntry */, std::vector<uint8_t>{},
                           std::vector<std::string>{"First", "Second"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TestEntry entry(header, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.dmi_string(0), "Not Specified");
    BOOST_CHECK_EQUAL(entry.dmi_string(1), "First");
    BOOST_CHECK_EQUAL(entry.dmi_string(2), "Second");
    BOOST_CHECK_EQUAL(entry.dmi_string(3), "Bad index");
}

/// Invalid-input/regression case (the OOB-read bug this task closes): the
/// string section is deliberately truncated BEFORE any double-NUL
/// terminator - the buffer ends mid-string, with no trailing \0 at all.
/// table_end is the backing std::vector's own data()+size(). Parsing must
/// stop cleanly at that boundary: the one fully-readable string before the
/// truncation point is kept, the truncated partial string is dropped
/// (never read past table_end to find a terminator that doesn't exist), and
/// nothing is read past the vector's own end.
BOOST_AUTO_TEST_CASE(ParseDmiStringsTruncatedBeforeTerminatorStopsAtBoundaryTestCase)
{
    // Deliberately malformed (no double-NUL terminator at all), so this
    // uses SyntheticTableBuilder's add_raw_bytes() escape hatch rather than
    // add_structure(), which always produces a well-formed string section.
    smbios::test::SyntheticTableBuilder builder;
    builder.add_raw_bytes(std::vector<uint8_t>{0x00, 0x04, 0x01, 0x00}); // header: type, length=4, handle

    // "First" is a complete, NUL-terminated string.
    const char first[] = "First";
    builder.add_raw_bytes(std::vector<uint8_t>(first, first + sizeof(first))); // includes '\0'

    // "Trunc" has NO terminating NUL - the buffer simply ends here, as if
    // the structure table was cut off mid-string by a corrupted/short dump.
    const char truncated[] = {'T', 'r', 'u', 'n', 'c'};
    builder.add_raw_bytes(std::vector<uint8_t>(truncated, truncated + sizeof(truncated)));

    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    const uint8_t* table_end = buffer.data() + buffer.size();

    // The real assertion of this test: constructing the entry (which drives
    // parse_dmi_strings() through its bounds-checked loop) must not read
    // past table_end and must not crash.
    TestEntry entry(header, table_end);

    BOOST_CHECK_EQUAL(entry.dmi_string(0), "Not Specified");
    BOOST_CHECK_EQUAL(entry.dmi_string(1), "First");

    // The truncated, unterminated "Trunc" string must NOT have been
    // appended - index 2 is out of range.
    BOOST_CHECK_EQUAL(entry.dmi_string(2), "Bad index");
}

/// Edge case: the string section is empty (first byte is the structure's
/// own double-NUL terminator) - only the default "Not Specified" entry is
/// present, and table_end equal to the first byte position is still handled
/// without reading anything.
BOOST_AUTO_TEST_CASE(ParseDmiStringsEmptyStringSectionTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(0x00 /* type */, std::vector<uint8_t>{}); // no strings -> bare double-NUL
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    TestEntry entry(header, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.dmi_string(0), "Not Specified");
    BOOST_CHECK_EQUAL(entry.dmi_string(1), "Bad index");
}

BOOST_AUTO_TEST_SUITE_END()
