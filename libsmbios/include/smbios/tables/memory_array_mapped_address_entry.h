#pragma once
#include <cstdint>
#include <smbios/abstract_smbios_entry.h>

// Memory Array Mapped Address entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Memory Array Mapped
// Address' chapter (7.20). Cross-checked against c-versions/
// dmidecode_linux/dmidecode.c's case 19 (dmi_decode(), ~line 4966, "Memory
// Array Mapped Address") for field offsets, length guards and the address
// sentinel.
//
// Scope note on starting/ending address encoding: per the reference's own
// rendering (`DWORD(data+0x04) >> 2` for the kB-aligned value and
// `(DWORD(data+0x04) & 0x3) << 10` for the sub-kilobyte fractional byte
// offset), the raw starting_address/ending_address fields are kilobyte-
// granular with a 2-bit sub-kilobyte fraction packed into their own low 2
// bits - this class does not decode that packing; get_starting_address()/
// get_ending_address() expose the raw value (widened to the extended 8-byte
// field when the sentinel applies) without semantic decode, matching
// BiosInformationEntry::get_starting_address()'s own "raw segment value, no
// decode" precedent. The extended 2.7+ fields are, by contrast, exact byte
// addresses (no kB granularity) - so the unit of the returned value differs
// between the two branches exactly as it does for PhysicalMemoryArrayEntry::
// get_maximum_capacity(); see that class's own doc comment for the same
// legacy-kB-vs-extended-bytes split.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Memory Array Mapped Address structure (7.20) - starting/
/// ending address (with the `0xFFFFFFFF` sentinel on the starting address
/// meaning "see the extended 8-byte starting/ending address pair
/// instead"), the owning Physical Memory Array's handle, and partition
/// width.
class MemoryArrayMappedAddressEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header, recognize how much information do we have
    /// in Memory Array Mapped Address SMBIOS entry depending on version
    /// and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    MemoryArrayMappedAddressEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~MemoryArrayMappedAddressEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword/qword values

    /// @brief 0x04 offset, 4 bytes, widened (2.7+) to an 8-byte field at
    /// 0x0F when the DWORD at 0x04 reads the `0xFFFFFFFF` sentinel (and
    /// length >= 0x1F) - see dmi_decode()'s own `DWORD(data + 0x04) ==
    /// 0xFFFFFFFF` check for case 19 (note: a DIFFERENT sentinel value than
    /// PhysicalMemoryArrayEntry's `0x80000000` maximum-capacity sentinel).
    /// Returns 0 when the sentinel is set but the extended field is not
    /// available. Raw value, no kB-to-byte decode - see class doc comment.
    uint64_t get_starting_address() const;

    /// @brief 0x08 offset, 4 bytes, widened (2.7+) to an 8-byte field at
    /// 0x17 under the same condition as get_starting_address() (the
    /// sentinel is checked against the STARTING address field only, per
    /// the reference's own logic - the ending address never carries its
    /// own independent sentinel check).
    uint64_t get_ending_address() const;

    /// @brief 0x0C offset
    /// Handle of the owning Physical Memory Array structure
    uint16_t get_memory_array_handle() const;

    /// @brief 0x0E offset
    /// Partition Width: number of Memory Devices that form a single row of
    /// memory for the physical memory array
    uint8_t get_partition_width() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Starting address string representation: the raw resolved
    /// value (see get_starting_address()) as a hex string, annotated with
    /// its unit ("kB" for the legacy branch, "bytes" for the extended
    /// branch) - see class doc comment. "Unknown" when the sentinel is set
    /// but the extended field is unavailable.
    std::string get_starting_address_string() const;

    /// @brief Ending address string representation, see
    /// get_starting_address_string()
    std::string get_ending_address_string() const;

    /// @brief Physical Array Handle string representation, hex
    std::string get_memory_array_handle_string() const;

    /// @brief Partition width string representation
    std::string get_partition_width_string() const;

private:

    /// @brief Whether the extended (2.7+) 8-byte address fields are in
    /// effect: the DWORD at 0x04 reads the `0xFFFFFFFF` sentinel AND the
    /// structure is long enough (length >= 0x1F) to actually carry them.
    bool uses_extended_addressing() const;
};

} // namespace smbios
