#include <cstdint>
#include <vector>

#include <libsmbios/smbios.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests for the Story 03.0 bounds-checked table walk
// (SMBios::read_smbios_table() / SMBios::count_smbios_structures(),
// smbios/src/smbios.cpp).
//
// Test seam: SMBios has no public hook to feed it a synthetic raw table -
// read_smbios_table()/count_smbios_structures() are private, and they derive
// table_base/table_size from the private table_buffer_ (populated by
// whichever ISmbiosSource in make_default_source_chain() succeeded - see
// milestone 0005, Story 01.0). To drive SMBios's own private
// read_smbios_table()/count_smbios_structures() against an injected buffer,
// smbios.h declares a narrowly-scoped `friend class SmbiosTableWalkTestAccess;`,
// shared (milestone 0006, Story 01.0) via synthetic_table_builder.h so this
// file and synthetic_table_builder_test.cpp don't each define their own
// copy. SmbiosTableWalkTestAccess overwrites the already-constructed
// SMBios's table_buffer_ directly, resets the cached parse state, and
// re-invokes the private walk - no new production API, no mock.

BOOST_AUTO_TEST_SUITE(SmbiosTableWalkUnitTests)

//////////////////////////////////////////////////////////////////////////
// 1a. Truncated last structure - the story's core scenario

/// Normal/edge case: a synthetic table with two well-formed structures
/// (type 1 "System Information" and type 3 "System Enclosure", each with
/// the minimal 4-byte formatted area and no strings) followed by a third
/// structure whose 4-byte header is itself in-bounds but whose declared
/// `length` (0x20) would read 28 bytes past the 16-byte buffer end.
///
/// Asserts the walk stops cleanly (no crash, and - since this is a
/// non-sanitizer build - no out-of-bounds read to observe directly, but the
/// fix is exactly the check that prevents one) and returns only the two
/// structures that were fully in-bounds; the malformed third structure is
/// silently dropped, not thrown as an exception (per this story's "degrade
/// gracefully" constraint).
BOOST_AUTO_TEST_CASE(ReadSmbiosTableStopsCleanlyOnTruncatedLastStructureTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(1, std::vector<uint8_t>{}); // SystemInformation, no formatted area
    builder.add_structure(3, std::vector<uint8_t>{}); // SystemEnclosure, no formatted area
    // Structure 3: header only (4 bytes, in-bounds), but length=0x20 claims a
    // formatted area that runs 28 bytes past the buffer end - deliberately
    // malformed, so add_raw_bytes() is used (and suppresses the automatic
    // EndOfTable marker, matching this test's original exact-16-byte buffer).
    builder.add_raw_bytes(std::vector<uint8_t>{4, 0x20, 0x03, 0x00});

    std::vector<uint8_t> buffer = builder.build();
    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(16));

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(2));

    auto it = smbios_instance.begin();
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(1));
    ++it;
    BOOST_REQUIRE(it != smbios_instance.end());
    BOOST_CHECK_EQUAL((*it).get_type(), static_cast<size_t>(3));
    ++it;
    BOOST_CHECK(it == smbios_instance.end());
}

//////////////////////////////////////////////////////////////////////////
// 1b. Well-formed table, including a header that starts exactly at the
// table's last 4 bytes - regression against the original off-by-one

/// Normal case: the same two well-formed structures, terminated by a real
/// EndOfTable (type 127) marker whose 4-byte header exactly fills the
/// buffer's last 4 bytes (current_structure_begin + kDmiHeaderWireSize ==
/// table_end). Proves the fix's exact-boundary case (`>` not `>=`) still
/// parses a legitimately-fitting header rather than breaking early.
///
/// count_smbios_structures() has no concept of EndOfTable - like the
/// pre-fix code, it counts every length-valid structure it walks over,
/// including the EndOfTable marker itself, so get_structures_count() is 3
/// here; read_smbios_table() stops *before pushing* the EndOfTable entry
/// into the iterable list, so only 2 real structures are observable via
/// begin()/end(). Both halves are exercised below - this is pre-existing,
/// unchanged-by-this-fix behavior (R8), not a new discrepancy.
BOOST_AUTO_TEST_CASE(ReadSmbiosTableParsesWellFormedTableWithExactBoundaryHeaderTestCase)
{
    // Two well-formed structures, then build()'s own automatically-appended
    // EndOfTable marker (type 127, length 4) starts at offset 12 and ends
    // exactly at offset 16 (the buffer's own end) - no bytes left over.
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(1, std::vector<uint8_t>{}); // SystemInformation, no formatted area
    builder.add_structure(3, std::vector<uint8_t>{}); // SystemEnclosure, no formatted area

    std::vector<uint8_t> buffer = builder.build();
    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(16));

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(3));

    size_t observed_structures = 0;
    for (auto it = smbios_instance.begin(); it != smbios_instance.end(); ++it) {
        ++observed_structures;
    }
    BOOST_CHECK_EQUAL(observed_structures, static_cast<size_t>(2));
}

//////////////////////////////////////////////////////////////////////////
// 1c. Empty table - invalid/degenerate input

/// Invalid-input case: an empty synthetic buffer must not be dereferenced
/// at all; the walk must degrade to zero structures, not crash.
BOOST_AUTO_TEST_CASE(ReadSmbiosTableHandlesEmptyBufferTestCase)
{
    std::vector<uint8_t> buffer;

    SMBios smbios_instance;
    SmbiosTableWalkTestAccess::parse_synthetic_table(smbios_instance, buffer);

    BOOST_CHECK_EQUAL(smbios_instance.get_structures_count(), static_cast<size_t>(0));
    BOOST_CHECK(smbios_instance.begin() == smbios_instance.end());
}

BOOST_AUTO_TEST_SUITE_END()
