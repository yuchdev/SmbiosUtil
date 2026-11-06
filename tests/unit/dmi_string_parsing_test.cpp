#include <cstdint>
#include <string>
#include <vector>

#include <smbios/abstract_smbios_entry.h>
#include <smbios/smbios.h>

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
    std::vector<uint8_t> buffer;
    buffer.push_back(0x00); // type (arbitrary, unused by TestEntry)
    buffer.push_back(0x04); // length = 4 (header only, no formatted fields)
    buffer.push_back(0x01); // handle low byte
    buffer.push_back(0x00); // handle high byte

    const char strings[] = "First\0Second\0";
    buffer.insert(buffer.end(), strings, strings + sizeof(strings));
    buffer.push_back(0x00); // final NUL completes the double-NUL terminator

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
    std::vector<uint8_t> buffer;
    buffer.push_back(0x00); // type
    buffer.push_back(0x04); // length = 4 (header only)
    buffer.push_back(0x01); // handle low byte
    buffer.push_back(0x00); // handle high byte

    // "First" is a complete, NUL-terminated string.
    const char first[] = "First";
    buffer.insert(buffer.end(), first, first + sizeof(first)); // includes '\0'

    // "Trunc" has NO terminating NUL - the buffer simply ends here, as if
    // the structure table was cut off mid-string by a corrupted/short dump.
    const char truncated[] = {'T', 'r', 'u', 'n', 'c'};
    buffer.insert(buffer.end(), truncated, truncated + sizeof(truncated));

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
    std::vector<uint8_t> buffer;
    buffer.push_back(0x00); // type
    buffer.push_back(0x04); // length = 4 (header only)
    buffer.push_back(0x01); // handle low byte
    buffer.push_back(0x00); // handle high byte
    buffer.push_back(0x00); // immediate double-NUL: no strings at all
    buffer.push_back(0x00);

    DMIHeader header = make_header(buffer);
    TestEntry entry(header, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.dmi_string(0), "Not Specified");
    BOOST_CHECK_EQUAL(entry.dmi_string(1), "Bad index");
}

BOOST_AUTO_TEST_SUITE_END()
