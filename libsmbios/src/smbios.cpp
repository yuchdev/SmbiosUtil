#include <sstream>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/smbios_source_chain.hpp>
#include <smbios/sys_interface/byte_reader.hpp>

using namespace smbios;

namespace {

/// @brief Size, in bytes, of a DMIHeader's on-wire representation: Type (1)
/// + Length (1) + Handle (2), per the DMTF SMBIOS spec. Deliberately NOT
/// `sizeof(DMIHeader)`: that in-memory struct also carries a `const
/// uint8_t* data` convenience pointer (smbios/include/smbios/smbios.h)
/// that is never serialized on the wire, so `sizeof(DMIHeader)` is 12 on a
/// 64-bit host - using it as the pre-dereference bounds margin would
/// over-reject legitimately in-bounds trailing structures.
constexpr size_t kDmiHeaderWireSize = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t);

} // namespace

bool smbios::operator>(const SMBiosVersion& lhs, const SMBiosVersion& rhs)
{
    if (lhs.major_version != rhs.major_version) {
        return lhs.major_version > rhs.major_version;
    }
    return lhs.minor_version > rhs.minor_version;
}

bool smbios::operator<(const SMBiosVersion& lhs, const SMBiosVersion& rhs)
{
    if (lhs.major_version != rhs.major_version) {
        return lhs.major_version < rhs.major_version;
    }
    return lhs.minor_version < rhs.minor_version;
}


SMBios::SMBios()
{
    static_assert(sizeof(uint8_t) == 1, "Very strange uint8_t size");
    static_assert(sizeof(uint16_t) == 2, "Very strange uint16_t size");
    static_assert(sizeof(uint32_t) == 4, "Very strange uint32_t size");

    // Try every source in this platform's chain (make_default_source_chain(),
    // declared in smbios/include/smbios/sys_interface/smbios_source_chain.h),
    // in priority order, stopping at the first one that is both available
    // and successfully reads a table. This loop itself is entirely
    // platform-neutral C++ - see plan.md's R8: milestone 0005, Story 01.1
    // (ADR 0001) resolves R8 completely by moving the platform selection out
    // of C++ entirely. make_default_source_chain() is defined exactly once
    // per build, inside whichever per-OS library smbios/src/CMakeLists.txt's
    // CMake-time `if(WIN32)/elseif(APPLE)/elseif(...Linux)` branch actually
    // links (smbios_windows/smbios_macos/smbios_linux), or a trivial fallback
    // compiled into this library directly on BSD/Solaris - never behind a
    // C++ `#if defined(_WIN32)` anywhere in this call chain.
    for (auto& source : make_default_source_chain()) {
        if (source->is_available() && source->try_read(table_buffer_, major_version_, minor_version_)) {
            source_name_ = source->source_name();
            break;
        }
    }

    read_smbios_table();
}

SMBios::~SMBios()
{
}

SMBiosVersion SMBios::get_smbios_version() const
{
    SMBiosVersion ver;
    ver.major_version = static_cast<uint16_t>(major_version_);
    ver.minor_version = static_cast<uint16_t>(minor_version_);
    return ver;
}

size_t SMBios::get_structures_count() const
{
    return structures_count_;
}

const uint8_t *SMBios::get_table_base() const
{
    return table_buffer_.empty() ? nullptr : table_buffer_.data();
}

size_t SMBios::get_table_size() const
{
    return table_buffer_.size();
}

std::vector<DMIHeader>& SMBios::get_headers_list()
{
    return headers_list_;
}

void SMBios::read_smbios_table()
{
    count_smbios_structures();

    const uint8_t* table_base = get_table_base();
    if (nullptr == table_base) {
        // No table at all (no native source, no legacy physical-memory scan
        // success): nothing to walk. Applying any non-zero offset to a null
        // pointer below would itself be undefined behavior, independent of
        // whether the result is ever dereferenced - bail out before any
        // pointer arithmetic is attempted.
        return;
    }
    const uint8_t* table_end = table_base + get_table_size();

    size_t number_of_structures = get_structures_count();
    const uint8_t* current_structure_begin = table_base;

    for (size_t i = 0; i < number_of_structures; ++i) {

        if (current_structure_begin + kDmiHeaderWireSize > table_end) {
            // Not enough bytes left for even a header. DMI table is truncated
            break;
        }

        DMIHeader header;
        header.type   = read_u8(current_structure_begin, 0);
        header.length = read_u8(current_structure_begin, 1);
        header.handle = read_u16(current_structure_begin, 2);
        header.data   = current_structure_begin;

        if (header.length < 4) {
            // Invalid entry length. DMI table is broken
            break;
        }

        if (current_structure_begin + header.length > table_end) {
            // Declared length would read past the table end. DMI table is truncated
            break;
        }

        if (header.type == SMBiosHandler::EndOfTable) {
            // end of table marker. Exit
            break;
        }

        headers_list_.push_back(header);

        // look to the current structure end '\0\0'
        current_structure_begin = current_structure_begin + header.length;
        while (current_structure_begin + 1 < table_end &&
               (current_structure_begin[0] != 0 || current_structure_begin[1] != 0)) {

            current_structure_begin++;
        }
        current_structure_begin += 2;
    }
}


void SMBios::count_smbios_structures()
{
    // start and end of the BIOS table
    const uint8_t* start_table = get_table_base();
    if (nullptr == start_table) {
        // No table at all: nothing to count. See read_smbios_table()'s own
        // identical guard for why this must come before any pointer
        // arithmetic, not just before any dereference.
        structures_count_ = 0;
        return;
    }
    const uint8_t* end_table = start_table + get_table_size();

    //points to the actual address in the buff that's been checked
    const uint8_t* offset = start_table;

    size_t structures_count = 0;

    //searches structures on the whole SMBIOS Table
    while (offset + kDmiHeaderWireSize <= end_table) {

        // only the length field (offset 1, one byte) is needed here
        const uint8_t length = read_u8(offset, 1);

        if (length < 4) {
            // Invalid entry length. DMI table is broken
            break;
        }

        if (offset + length > end_table) {
            // Declared length would read past the table end. DMI table is truncated
            break;
        }

        offset += length;
        structures_count++;

        // increases the offset to point to the next header that's
        // after the strings at the end of the structure.
        while (offset + 1 < end_table &&
               (read_u8(offset, 0) != 0 || read_u8(offset, 1) != 0)) {
            offset++;
        }

        // Points to the next structure that after two null bytes
        // at the end of the strings section
        offset += 2;
    }

    structures_count_ = structures_count;
}

std::string SMBios::render_to_description() const
{
    // Ratified 2026-11-07 (docs/roadmap/0005-uefi-and-native-source-abstraction/status.md):
    // the old implementation printed the raw entry-point struct's fields
    // (checksum, entry-point revision, BCD version, intermediate checksum)
    // directly from smbios_entry32_/smbios_entry64_ - members the new
    // one-source-per-platform chain model (ISmbiosSource) encapsulates away
    // inside whichever concrete source succeeded, so that struct is no
    // longer available here to print. Simplified to a diagnostic report of
    // what a caller can still observe through SMBios's own public API: the
    // resolved SMBIOS version, the table size, the structure count, and
    // which source in the chain actually produced the data.
    SMBiosVersion ver = get_smbios_version();

    std::stringstream description;
    description << "SMBIOS version: " << ver.major_version << '.' << ver.minor_version << '\n';
    description << "Table size: " << get_table_size() << '\n';
    description << "Structures count: " << get_structures_count() << '\n';
    description << "Source: " << (source_name_.empty() ? "none" : source_name_) << '\n';
    return description.str();
}
