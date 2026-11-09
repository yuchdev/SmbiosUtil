#pragma once
#include <cstdint>
#include <libsmbios/abstract_smbios_entry.hpp>

// Memory Device Mapped Address entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Memory Device Mapped
// Address' chapter (7.21). Cross-checked against c-versions/
// dmidecode_linux/dmidecode.c's case 20 ("Memory Device Mapped Address")
// for field offsets, length guards and the address sentinel.
//
// Scope note on starting/ending address units: per DSP0134 3.9.0 Table 86,
// the legacy Starting Address/Ending Address DWORD fields (0x04/0x08) are
// in KIBIBYTES, while the 2.7+ Extended Starting Address/Extended Ending
// Address QWORD fields (0x13/0x1B) that supersede them (when the legacy
// DWORD reads the 0xFFFFFFFF sentinel) are in BYTES - the same
// legacy-narrow-unit-vs-extended-wide-unit split already documented by
// PhysicalMemoryArrayEntry::get_maximum_capacity() (kB vs bytes) and
// MemoryArrayMappedAddressEntry::get_starting_address() (also kB vs bytes).
// Unlike those two precedents, this class normalizes the unit itself:
// get_starting_address()/get_ending_address() always return BYTES, so
// callers never need to know whether the legacy or extended storage backed
// the value - the legacy kibibyte value is multiplied by 1024 before being
// returned.
//
// Scope note on the omitted Interleaved Data Depth field (0x12, BYTE,
// 2.1+, between Interleave Position and the 2.7+ extended address pair):
// not named by this task's own field list, and not exposed by an accessor
// here - only its presence between 0x11 and 0x13 matters for this class,
// since it keeps the extended fields' offsets (0x13/0x1B) correct.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Memory Device Mapped Address structure (7.21) - starting/
/// ending address of a range mapped to a specific Memory Device (with the
/// `0xFFFFFFFF` sentinel on the legacy DWORD fields meaning "see the
/// extended 8-byte starting/ending address pair instead", 2.7+), the
/// owning Memory Device's handle, the owning Memory Array Mapped Address
/// structure's handle, and the device's partition-row/interleave position
/// within the mapped range.
class MemoryDeviceMappedAddressEntry : public AbstractSMBiosEntry {
public:

    /// @brief Partition Row Position sentinel (0xFF): the position is
    /// unknown. Distinct from the value 0, which per spec is reserved
    /// (never a valid position) - see get_partition_row_position()'s own
    /// doc comment for why both are passed through raw rather than
    /// collapsed into one "unknown" bucket.
    static constexpr uint8_t PartitionRowPositionUnknown = 0xFF;

    /// @brief Partition Row Position reserved value (0): per spec, this
    /// value is reserved and never a valid position - NOT the same
    /// semantic as Interleave Position's value 0 (which means
    /// "non-interleaved", a valid, meaningful state for that field).
    static constexpr uint8_t PartitionRowPositionReserved = 0x00;

    /// @brief Interleave Position sentinel (0xFF): the position is
    /// unknown.
    static constexpr uint8_t InterleavePositionUnknown = 0xFF;

    /// @brief Interleave Position value meaning "non-interleaved" (0) - a
    /// valid, meaningful value for this field, unlike Partition Row
    /// Position's reserved 0.
    static constexpr uint8_t InterleavePositionNonInterleaved = 0x00;

    /// @brief Parse the header, recognize how much information do we have
    /// in Memory Device Mapped Address SMBIOS entry depending on version
    /// and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    MemoryDeviceMappedAddressEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~MemoryDeviceMappedAddressEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword/qword values

    /// @brief 0x04 offset, 4 bytes, legacy value in KIBIBYTES, widened
    /// (2.7+) to the 8-byte Extended Starting Address field at 0x13 (in
    /// BYTES) when the DWORD at 0x04 reads the `0xFFFFFFFF` sentinel (and
    /// length >= 0x23) - see dmi_decode()'s own `DWORD(data + 0x04) ==
    /// 0xFFFFFFFF` check for case 20. The return value is always
    /// normalized to BYTES: the legacy branch multiplies its kibibyte
    /// value by 1024 before returning, so callers never need to know which
    /// storage width backed the result. Returns 0 when the sentinel is set
    /// but the extended field is not available (length < 0x23) - the
    /// address is genuinely unknown in that case, not "fall back to the
    /// sentinel value itself".
    uint64_t get_starting_address() const;

    /// @brief 0x08 offset, same pattern/unit-normalization as
    /// get_starting_address(); the sentinel is checked against the
    /// STARTING address field only, per the reference's own logic - the
    /// ending address never carries its own independent sentinel check.
    uint64_t get_ending_address() const;

    /// @brief 0x0C offset
    /// Handle of the Memory Device structure to which this address range
    /// is mapped.
    uint16_t get_memory_device_handle() const;

    /// @brief 0x0E offset
    /// Handle of the Memory Array Mapped Address structure to which this
    /// device address range is mapped.
    uint16_t get_memory_array_mapped_address_handle() const;

    /// @brief 0x10 offset. Raw byte, passed through unmodified: 0 is
    /// reserved per spec (not a valid position - see
    /// PartitionRowPositionReserved), 0xFF means unknown (see
    /// PartitionRowPositionUnknown), any other value is the 1-based
    /// position of this Memory Device within its address partition's row.
    /// These are TWO DIFFERENT distinguished states, not one "unknown"
    /// bucket - get_partition_row_position_string() renders them
    /// distinctly ("Reserved" vs "Unknown"), matching SystemSlotsEntry's
    /// existing "Unknown" vs "None" precedent for keeping two different
    /// sentinel meanings visually distinct.
    uint8_t get_partition_row_position() const;

    /// @brief 0x11 offset. Raw byte, passed through unmodified: 0 means
    /// "non-interleaved" here (see InterleavePositionNonInterleaved) - a
    /// valid, meaningful value, NOT the same semantic as Partition Row
    /// Position's reserved 0 despite sharing the same byte value - 0xFF
    /// means unknown (see InterleavePositionUnknown), any other value is
    /// the 1-based interleave position.
    uint8_t get_interleave_position() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Starting address string representation: the normalized
    /// (bytes) resolved value (see get_starting_address()) as a hex
    /// string. "Unknown" when the sentinel is set but the extended field
    /// is unavailable.
    std::string get_starting_address_string() const;

    /// @brief Ending address string representation, see
    /// get_starting_address_string()
    std::string get_ending_address_string() const;

    /// @brief Memory Device Handle string representation, hex
    std::string get_memory_device_handle_string() const;

    /// @brief Memory Array Mapped Address Handle string representation, hex
    std::string get_memory_array_mapped_address_handle_string() const;

    /// @brief Partition Row Position string representation: "Reserved" for
    /// 0, "Unknown" for 0xFF, else the numeric position - see
    /// get_partition_row_position()'s own doc comment.
    std::string get_partition_row_position_string() const;

    /// @brief Interleave Position string representation: "Non-interleaved"
    /// for 0, "Unknown" for 0xFF, else the numeric position - see
    /// get_interleave_position()'s own doc comment.
    std::string get_interleave_position_string() const;

private:

    /// @brief Whether the extended (2.7+) 8-byte address fields are in
    /// effect: the DWORD at 0x04 reads the `0xFFFFFFFF` sentinel AND the
    /// structure is long enough (length >= 0x23) to actually carry them.
    bool uses_extended_addressing() const;
};

} // namespace smbios
