#include <cstdint>

#include <smbios/sys_interface/byte_reader.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: pure free-function tests
// against hand-built byte buffers, decoding known little-endian sequences
// with smbios::read_u8/u16/u32/u64 (smbios/include/smbios/byte_reader.hpp).

BOOST_AUTO_TEST_SUITE(ByteReaderUnitTests)

//////////////////////////////////////////////////////////////////////////
// 1a. read_u8 - trivial pass-through

/// Normal case: single byte is returned unchanged
BOOST_AUTO_TEST_CASE(ReadU8DecodesKnownByteTestCase)
{
    const uint8_t buffer[] = {0x7F};

    BOOST_CHECK_EQUAL(read_u8(buffer, 0), static_cast<uint8_t>(0x7F));
}

//////////////////////////////////////////////////////////////////////////
// 1b. read_u16 - classic endian-swap sanity check

/// Normal case: {0x34, 0x12} little-endian decodes to 0x1234, proving the
/// byte-by-byte composition is independent of host endianness (a naive
/// reinterpret_cast on a big-endian host would instead yield 0x3412).
BOOST_AUTO_TEST_CASE(ReadU16DecodesLittleEndianPairTestCase)
{
    const uint8_t buffer[] = {0x34, 0x12};

    BOOST_CHECK_EQUAL(read_u16(buffer, 0), static_cast<uint16_t>(0x1234));
}

/// Edge case: non-zero offset reads the correct sub-sequence, not offset 0
BOOST_AUTO_TEST_CASE(ReadU16DecodesAtNonZeroOffsetTestCase)
{
    const uint8_t buffer[] = {0xAA, 0x78, 0x56, 0xBB};

    BOOST_CHECK_EQUAL(read_u16(buffer, 1), static_cast<uint16_t>(0x5678));
}

//////////////////////////////////////////////////////////////////////////
// 1c. read_u32

/// Normal case: four-byte little-endian sequence decodes in byte order
BOOST_AUTO_TEST_CASE(ReadU32DecodesLittleEndianQuadTestCase)
{
    const uint8_t buffer[] = {0x78, 0x56, 0x34, 0x12};

    BOOST_CHECK_EQUAL(read_u32(buffer, 0), static_cast<uint32_t>(0x12345678));
}

//////////////////////////////////////////////////////////////////////////
// 1d. read_u64

/// Normal case: eight-byte little-endian sequence decodes in byte order,
/// exercising read_u64's composition from two read_u32 halves
BOOST_AUTO_TEST_CASE(ReadU64DecodesLittleEndianOctetTestCase)
{
    const uint8_t buffer[] = {0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12};

    BOOST_CHECK_EQUAL(read_u64(buffer, 0), static_cast<uint64_t>(0x123456789ABCDEF0ULL));
}

//////////////////////////////////////////////////////////////////////////
// 1e. constexpr property - compile-time verification via static_assert

namespace {

/// @brief Backing storage for the static_assert cases below; must be a
/// constexpr/static constexpr array so the expression is evaluable at
/// compile time, proving read_u16/read_u32 are genuinely constexpr and not
/// merely callable at runtime.
constexpr uint8_t kConstexprBuffer[] = {
    0x7F, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12};

static_assert(smbios::read_u8(kConstexprBuffer, 0) == 0x7F,
              "read_u8 must decode a single byte at compile time");

static_assert(smbios::read_u16(kConstexprBuffer, 1) == 0x1234,
              "read_u16 must decode {0x34, 0x12} as 0x1234 at compile time");

static_assert(smbios::read_u32(kConstexprBuffer, 3) == 0x12345678,
              "read_u32 must decode {0x78, 0x56, 0x34, 0x12} as 0x12345678 at compile time");

static_assert(smbios::read_u64(kConstexprBuffer, 7) == 0x123456789ABCDEF0ULL,
              "read_u64 must decode the eight-byte little-endian sequence at compile time");

} // namespace

BOOST_AUTO_TEST_SUITE_END()
