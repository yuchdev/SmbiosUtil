#include <cstdint>
#include <vector>

#include <smbios/smbios.h>
#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
#include <smbios/unix_bios.h>
#endif

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
// table_base/table_size from the private native_impl_ (or the legacy
// physical-memory entry points). The one *real*, non-mocked seam that
// reaches this path is the same one Story 01.0's regression test used at
// the SMBiosImpl layer (SMBiosImpl::read_from_physical_memory(), see
// unit_tests.cpp's SMBiosImplGetTableSizeReflectsPhysicalMemoryDumpTestCase):
// it just std::move()s a caller-supplied buffer into SMBiosImpl's own
// table_buffer_, after which SMBiosImpl::get_table_base()/get_table_size()
// (and therefore SMBios::get_table_base()/get_table_size()) reflect it
// exactly. Constructing SMBiosImpl performs a real (sysfs/EFI) discovery
// attempt that fails cleanly without root/real hardware (harmless here,
// since we overwrite table_buffer_ immediately after).
//
// To drive SMBios's own private read_smbios_table()/count_smbios_structures()
// against that injected buffer (rather than re-testing SMBiosImpl in
// isolation), smbios.h declares a narrowly-scoped
// `friend class SmbiosTableWalkTestAccess;` for exactly this test file.
// SmbiosTableWalkTestAccess (below) injects the synthetic buffer into the
// already-constructed SMBios's native_impl_, resets the cached parse state,
// and re-invokes the private walk - no new production API, no mock.

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)

namespace smbios {

/// @brief Test-only friend accessor granting smbios_table_walk_test.cpp
/// access to SMBios's private members needed to drive a synthetic table
/// through the real bounds-checked walk. See file header comment above.
class SmbiosTableWalkTestAccess
{
public:
    /// @brief Inject a synthetic raw structure-table buffer into an
    /// already-constructed SMBios instance (via its native_impl_), discard
    /// any state from the instance's own constructor-time real-hardware
    /// attempt, and re-run the private table walk against the synthetic
    /// buffer only.
    static void parse_synthetic_table(SMBios& instance, std::vector<uint8_t> synthetic_buffer)
    {
        instance.native_impl_->read_from_physical_memory(synthetic_buffer);
        instance.headers_list_.clear();
        instance.structures_count_ = 0;
        instance.read_smbios_table();
    }
};

} // namespace smbios

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
    std::vector<uint8_t> buffer = {
        // Structure 1: type=1 (SystemInformation), length=4, handle=0x0001
        1, 4, 0x01, 0x00,
        // string-section terminator (no strings)
        0, 0,
        // Structure 2: type=3 (SystemEnclosure), length=4, handle=0x0002
        3, 4, 0x02, 0x00,
        0, 0,
        // Structure 3: header only (4 bytes, in-bounds), but length=0x20
        // claims a formatted area that runs 28 bytes past the buffer end
        4, 0x20, 0x03, 0x00
    };
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
    std::vector<uint8_t> buffer = {
        1, 4, 0x01, 0x00,
        0, 0,
        3, 4, 0x02, 0x00,
        0, 0,
        // EndOfTable header starts at offset 12, ends exactly at offset 16
        // (the buffer's own end) - no bytes left over
        127, 4, 0x03, 0x00
    };
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

#endif //defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
