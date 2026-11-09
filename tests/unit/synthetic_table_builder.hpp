#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <libsmbios/smbios.hpp>

// Shared hermetic test-support seam (milestone 0006, Story 01.0, Task 01).
//
// SMBios's constructor (post milestone 0005) always walks the
// ISmbiosSource chain (see smbios/sys_interface/smbios_source_chain.hpp) -
// there is no production entry point that takes a raw buffer directly, and
// this task deliberately does not add one: SMBios's public API stays
// untouched. Instead, smbios/include/smbios/smbios.hpp declares a narrowly
// scoped `friend class SmbiosTableWalkTestAccess;` that lets test code
// overwrite an already-constructed SMBios's private table_buffer_, reset
// its cached parse state, and re-invoke the private read_smbios_table()
// against a synthetic buffer - no mock, no second parallel mechanism.
//
// This header is the single shared home for that accessor (previously
// defined only inside tests/unit/smbios_table_walk_test.cpp) plus the
// SyntheticTableBuilder that generalizes hand-rolled synthetic buffers
// (e.g. unit_tests.cpp's build_port_connection_buffer()) into a reusable,
// fluent builder. Test-support code only: included by tests/unit/*.cpp via
// file(GLOB SOURCES *.cpp) in tests/unit/CMakeLists.txt, never by
// libsmbios/smbios production sources.

namespace smbios {

/// @brief Test-only friend accessor granting hermetic unit tests access to
/// SMBios's private members needed to drive a synthetic table through the
/// real bounds-checked walk, bypassing every ISmbiosSource entirely.
class SmbiosTableWalkTestAccess
{
public:
    /// @brief Inject a synthetic raw structure-table buffer into an
    /// already-constructed SMBios instance (via its table_buffer_), discard
    /// any state from the instance's own constructor-time real-hardware
    /// attempt, and re-run the private table walk against the synthetic
    /// buffer only.
    static void parse_synthetic_table(SMBios& instance, std::vector<uint8_t> synthetic_buffer)
    {
        instance.table_buffer_ = std::move(synthetic_buffer);
        instance.headers_list_.clear();
        instance.structures_count_ = 0;
        instance.read_smbios_table();
    }
};

/// @brief Test-support-only types (not shipped in libsmbios/smbios). Kept in
/// a dedicated namespace to make clear at every call site that these are
/// hermetic test fixtures, not production API.
namespace test {

/// @brief Fluent builder for an in-memory, byte-for-byte SMBIOS structure
/// table (header + formatted area + string section + terminator, repeated
/// per structure, ending in an EndOfTable marker), usable directly as input
/// to SMBios's private read_smbios_table()/count_smbios_structures() via
/// SmbiosTableWalkTestAccess::parse_synthetic_table() - without going
/// through any ISmbiosSource. Satisfies plan.md's R1.
///
/// Well-formed by default: add_structure() always produces a
/// spec-conformant structure (correct length byte, correctly-terminated
/// string section) and build() appends a valid EndOfTable marker
/// automatically. add_raw_bytes() is the deliberate escape hatch for Story
/// 02.0's malformed-table tests (a structure whose length overruns the
/// buffer, a missing double-null, etc.) - once it is used, build() no
/// longer appends EndOfTable automatically, since the caller has taken over
/// full control of what the tail of the buffer looks like.
class SyntheticTableBuilder
{
public:
    /// @brief Append one well-formed structure: a 4-byte header
    /// (type/length/handle, with length computed automatically as 4 +
    /// formatted_area.size() and handle auto-assigned sequentially starting
    /// at 1), the formatted area verbatim, then the string section (each
    /// string written followed by a terminating '\0', with the whole
    /// section closed by the trailing '\0' that completes the spec's
    /// double-NUL marker) - matching exactly what
    /// AbstractSMBiosEntry::parse_dmi_strings() expects to walk.
    /// @param type SMBIOS structure type (SMBios::SMBiosHandler or an
    /// OEM-specific value).
    /// @param formatted_area Raw bytes of the structure's formatted area,
    /// not including the 4-byte header.
    /// @param strings DMI strings referenced by the formatted area's string
    /// indices, in index order (index 1 first). May be empty.
    /// @return *this, for chaining.
    SyntheticTableBuilder& add_structure(uint8_t type, std::vector<uint8_t> formatted_area,
                                          std::vector<std::string> strings = {});

    /// @brief Append arbitrary bytes verbatim, bypassing every well-formed-
    /// by-default guarantee add_structure() provides. Intended for Story
    /// 02.0's malformed-table tests (e.g. a structure whose declared length
    /// overruns the buffer, or a string section missing its terminating
    /// double-null). Once called, build() stops auto-appending an
    /// EndOfTable marker, since the caller now owns the buffer's tail.
    /// @param raw Bytes to append as-is.
    /// @return *this, for chaining.
    SyntheticTableBuilder& add_raw_bytes(std::vector<uint8_t> raw);

    /// @brief Assemble the final buffer: every add_structure()/
    /// add_raw_bytes() call's bytes, in call order, followed by a 4-byte
    /// EndOfTable marker (type 127, length 4) - unless add_raw_bytes() was
    /// ever called, in which case the caller is assumed to have already
    /// terminated the buffer as needed (or deliberately left it malformed).
    /// @return The assembled byte buffer, ready to feed to
    /// SmbiosTableWalkTestAccess::parse_synthetic_table().
    std::vector<uint8_t> build() const;

private:
    /// Accumulated bytes from every add_structure()/add_raw_bytes() call.
    std::vector<uint8_t> buffer_;

    /// Next handle to assign to a structure added via add_structure().
    uint16_t next_handle_ = 1;

    /// True once add_raw_bytes() has been called at least once; suppresses
    /// build()'s automatic EndOfTable marker.
    bool raw_bytes_added_ = false;
};

} // namespace test
} // namespace smbios
