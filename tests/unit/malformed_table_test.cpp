#include <cstdint>
#include <vector>

#include <libsmbios/smbios.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Boundary / malformed-table tests (milestone 0006, Story 02.0, Task 01 -
// plan.md R4's first three boundary categories), proving milestone 0003's
// Story 03.0 hardening of SMBios::read_smbios_table()/count_smbios_structures()
// (smbios/src/smbios.cpp) actually holds against adversarial input built via
// the SyntheticTableBuilder (milestone 0006, Story 01.0). Every assertion
// here is about SAFETY (clean stop, no out-of-bounds read, internally
// consistent state) - not about recovering one "correct" parse of garbage
// input, per this task's own constraint.
//
// Seam: SmbiosTableWalkTestAccess::parse_synthetic_table() overwrites an
// already-constructed SMBios's private table_buffer_ and re-invokes the
// private read_smbios_table() (which itself calls count_smbios_structures()
// first) against the synthetic buffer, bypassing every ISmbiosSource.
//
// Non-sanitizer-build proof only here: running this file under ASan/UBSan
// (the `sanitizers` CI job's flags) is the actual proof these inputs don't
// read out of bounds - see this task's own status.md note for whether that
// local verification was performed.

BOOST_AUTO_TEST_SUITE(MalformedTableUnitTests)

//////////////////////////////////////////////////////////////////////////
// 1. Truncated / oversized length

/// Invalid-input case: three well-formed leading structures, followed by a
/// fourth whose 4-byte header is itself in-bounds but whose declared
/// `length` (0xFF) claims far more bytes than remain in the buffer.
///
/// Asserts read_smbios_table() stops cleanly at the malformed structure -
/// the three well-formed leading structures are still observable via
/// begin()/end(), the malformed fourth is silently dropped (not thrown),
/// and (the real proof, see file header) no out-of-bounds read occurs -
/// confirmed under ASan separately from this plain build.
BOOST_AUTO_TEST_CASE(TruncatedLengthOverrunsBufferTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(1, std::vector<uint8_t>{});  // SystemInformation
    builder.add_structure(3, std::vector<uint8_t>{});  // SystemEnclosure
    builder.add_structure(4, std::vector<uint8_t>{});  // ProcessorInformation
    // Malformed trailing structure: header only (4 bytes, in-bounds), but
    // length=0xFF claims a formatted area that runs far past the buffer end.
    builder.add_raw_bytes(std::vector<uint8_t>{17, 0xFF, 0x04, 0x00});  // type=17 (MemoryDevice)

    std::vector<uint8_t> buffer = builder.build();

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    // The malformed structure's length overruns the buffer, so
    // count_smbios_structures() must stop before counting it: only the
    // three well-formed leading structures are counted.
    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(3));

    auto it = smbios_instance.begin();
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(1));
    ++it;
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(3));
    ++it;
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(4));
    ++it;
    BOOST_CHECK(it == smbios_instance.end());
}

//////////////////////////////////////////////////////////////////////////
// 2. Missing double-NUL string-section terminator

/// Invalid-input case: a single structure whose string section never hits
/// the terminating double-NUL before the buffer ends - the buffer is cut
/// off mid-string, with no trailing \0 at all. Exercises
/// read_smbios_table()'s own string-skip loop (lines near
/// `current_structure_begin += 2` in smbios.cpp), which is a separate code
/// path from AbstractSMBiosEntry::parse_dmi_strings() (already covered by
/// dmi_string_parsing_test.cpp at the entry level) - here the walk itself,
/// not string extraction, must stop cleanly at table_end.
///
/// Asserts the one structure is still found (its header is fully in-bounds
/// and its own length is valid - only the string section is malformed), and
/// that the walk does not crash or hang looking for a terminator that does
/// not exist.
BOOST_AUTO_TEST_CASE(MissingDoubleNullTerminatorTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    // Header: type=1 (SystemInformation), length=4 (header only, no
    // formatted area), handle=1.
    builder.add_raw_bytes(std::vector<uint8_t>{1, 4, 0x01, 0x00});
    // String section bytes with NO terminating double-NUL - the buffer
    // simply ends here, as if cut off mid-string by a corrupted/short dump.
    const char unterminated[] = {'N', 'o', 'T', 'e', 'r', 'm'};
    builder.add_raw_bytes(std::vector<uint8_t>(unterminated, unterminated + sizeof(unterminated)));

    std::vector<uint8_t> buffer = builder.build();

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    // The structure's header and declared length are themselves valid and
    // fully in-bounds - only its trailing string section lacks a
    // terminator - so both walkers still find exactly this one structure.
    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(1));

    auto it = smbios_instance.begin();
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(1));
    ++it;
    BOOST_CHECK(it == smbios_instance.end());
}

//////////////////////////////////////////////////////////////////////////
// 3. Structure-count mismatch between count_smbios_structures() and
// read_smbios_table()'s own walk

/// Adversarial-input case engineered to make count_smbios_structures()'s
/// independent walk and read_smbios_table()'s walk genuinely disagree, by
/// exploiting a real discrepancy between the two (milestone 0003's Story
/// 03.0 flagged the duplication but did not deduplicate it; see
/// smbios_table_walk_test.cpp's own `ReadSmbiosTableParsesWellFormedTable-
/// WithExactBoundaryHeaderTestCase` comment for the narrower, already-known
/// half of this - that count_smbios_structures() counts the EndOfTable
/// marker itself as a "structure").
///
/// This test goes one step further: count_smbios_structures() has NO
/// special handling at all for SMBiosHandler::EndOfTable - unlike
/// read_smbios_table(), which breaks its outer loop entirely the moment it
/// sees type == EndOfTable, count_smbios_structures() just treats
/// EndOfTable as an ordinary type value, counts it, and KEEPS WALKING past
/// it into whatever bytes follow, as long as they still look like a
/// length-valid, in-bounds structure. So this buffer places a well-formed
/// "fake" structure (type 0x50, a value with no registered meaning here)
/// *after* a real EndOfTable marker:
///
///   [0] structure A (type=1, SystemInformation, length=4)       offset  0-5
///   [1] EndOfTable marker (type=127, length=4)                  offset  6-9
///       explicit empty string section (double-NUL)              offset 10-11
///   [2] fake trailing "structure" (type=0x50, length=6)         offset 12-19
///       (2 formatted bytes + empty string section)
///
/// OBSERVED / DOCUMENTED BEHAVIOR (not assumed - see status.md):
///  - count_smbios_structures() walks all three length-valid regions and
///    reports get_structures_count() == 3 (structure A, the EndOfTable
///    marker, AND the fake trailing structure it never should have looked
///    at, since EndOfTable is supposed to terminate the table).
///  - read_smbios_table() walks structure A, then on its second iteration
///    sees type == EndOfTable and breaks its outer loop immediately -
///    never even looking at the fake trailing structure's bytes. Only
///    structure A ends up in the iterable headers_list_ (begin()/end()
///    yields exactly 1 entry).
///  - The two walkers DISAGREE (3 vs 1) on this adversarial input. This is
///    safe (bounds-checked throughout, no OOB read, no crash) but is a
///    real correctness gap: count_smbios_structures() can be made to
///    report an arbitrarily inflated structure count by placing
///    length-valid garbage after the real EndOfTable marker, because it
///    does not recognize EndOfTable as a terminator at all. Any caller that
///    trusts get_structures_count() as "how many real structures exist"
///    without also walking headers_list_ would be misled by this input -
///    though no caller in this codebase currently does that (consumers use
///    begin()/end(), which is unaffected).
BOOST_AUTO_TEST_CASE(StructureCountMismatchTestCase)
{
    smbios::test::SyntheticTableBuilder builder;

    // Structure A: type=1 (SystemInformation), header-only, auto-terminated
    // empty string section. 6 bytes: offsets 0-5.
    builder.add_structure(1, std::vector<uint8_t>{});

    // EndOfTable marker (type=127, length=4, handle=2), followed by an
    // explicit empty string section (double-NUL) so the shared string-skip
    // loop both walkers run after processing it terminates immediately,
    // rather than consuming bytes belonging to the fake structure below.
    // 6 bytes total: offsets 6-11.
    builder.add_raw_bytes(std::vector<uint8_t>{
        static_cast<uint8_t>(SMBios::SMBiosHandler::EndOfTable), 4, 0x02, 0x00,
        0x00, 0x00});

    // Fake trailing "structure": type=0x50 (unregistered, arbitrary),
    // length=6 (header + 2 formatted bytes), handle=3, then an explicit
    // empty string section. Entirely length-valid and in-bounds - the point
    // is that it LOOKS like a real structure to a walker that doesn't know
    // the table already ended. 8 bytes total: offsets 12-19.
    builder.add_raw_bytes(std::vector<uint8_t>{0x50, 6, 0x03, 0x00, 0xAA, 0xBB, 0x00, 0x00});

    std::vector<uint8_t> buffer = builder.build();
    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(20));

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    // count_smbios_structures()'s independent walk: counts structure A, the
    // EndOfTable marker, AND the fake trailing structure - see the
    // documented finding above.
    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(3));

    // read_smbios_table()'s own walk: stops dead at the real EndOfTable
    // marker and never observes the fake trailing structure - only
    // structure A is iterable.
    size_t observed_structures = 0;
    for (auto it = smbios_instance.begin(); it != smbios_instance.end(); ++it) {
        BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(1));
        ++observed_structures;
    }
    BOOST_CHECK_EQUAL(observed_structures, static_cast<size_t>(1));

    // The two walkers disagree (3 vs 1) on this input - documented as a
    // real finding, not asserted-away as "must match". Both remain
    // internally consistent and safe: get_structures_count() is bounded by
    // the actual buffer size (never counts beyond what is truly in-bounds),
    // and headers_list_/the iterator never exposes the fake trailing
    // structure's bytes as a real entry.
    BOOST_CHECK_NE(smbios_instance.get_structures_count(), observed_structures);
}

BOOST_AUTO_TEST_SUITE_END()
