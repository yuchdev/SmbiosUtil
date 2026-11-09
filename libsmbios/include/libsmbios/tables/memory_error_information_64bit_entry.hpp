#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// 64-Bit Memory Error Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, '64-Bit Memory Error
// Information' chapter (7.34), DSP0134 3.9.0, Table 106. Field offsets and
// enum tables (7.19.1 Error Type, 7.19.2 Error Granularity, 7.19.3 Error
// Operation, shared with the 32-bit structure and referenced by Table 106
// itself rather than re-defined) confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// Width asymmetry, confirmed directly against Table 106 rather than taken
// on faith from any derived spec reproduction: Memory Array Error Address
// (0x0B) and Device Error Address (0x13) are widened to QWORD relative to
// the 32-bit structure (since this type exists specifically for errors
// above 4GiB), but Error Resolution (0x1B) stays DWORD - unchanged in both
// width and sentinel (`8000 0000h`) from the 32-bit structure's own 0x13
// field. This is not a transcription error in the task that drove this
// class; it is what Table 106 actually specifies.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The 64-Bit Memory Error Information structure (7.34) - identical
/// in shape to MemoryErrorInformation32BitEntry (7.19) except that its two
/// address fields are widened to QWORD so errors above 4GiB can be
/// represented; Error Resolution is NOT widened (see this header's own
/// leading comment). Deliberately does not share a base class with
/// MemoryErrorInformation32BitEntry beyond AbstractSMBiosEntry, matching
/// this codebase's existing precedent (OemStringsEntry/
/// SystemConfigurationOptionsEntry) of not sharing a base purely for
/// incidental shape similarity - the three enum lookup tables are
/// duplicated verbatim rather than factored out.
///
/// Sentinel discipline (Table 106): Vendor Syndrome's "unknown" marker is
/// all-zero (`0000 0000h`); the two QWORD address fields' marker is
/// `8000 0000 0000 0000h`; Error Resolution's marker is `8000 0000h` (DWORD
/// width, same convention as the 32-bit structure). Every `is_*_known()`
/// predicate below checks the exact sentinel for its own field's width.
class MemoryErrorInformation64BitEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header; every field is distinguished by
    /// get_entry_size() alone (no SMBIOS-version gating - 7.34's own Table
    /// 106 marks this a 2.3+ structure as a whole, with no partial/legacy
    /// field layout within it).
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    MemoryErrorInformation64BitEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~MemoryErrorInformation64BitEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/dword/qword values

    /// @brief 0x04 offset, BYTE, ENUM
    /// Type of error associated with the status reported for the memory
    /// array or device; see @see 7.19.1 for defined values. 0 if
    /// get_entry_size() < 0x05.
    uint8_t get_error_type() const;

    /// @brief 0x05 offset, BYTE, ENUM
    /// Granularity to which the error can be resolved (device vs.
    /// partition); see @see 7.19.2 for defined values. 0 if
    /// get_entry_size() < 0x06.
    uint8_t get_error_granularity() const;

    /// @brief 0x06 offset, BYTE, ENUM
    /// Memory access operation that caused the error; see @see 7.19.3 for
    /// defined values. 0 if get_entry_size() < 0x07.
    uint8_t get_error_operation() const;

    /// @brief 0x07 offset, DWORD
    /// Vendor-specific ECC syndrome or CRC data associated with the
    /// erroneous access. Unknown sentinel is all-zero (`0000 0000h`) - see
    /// is_vendor_syndrome_known(). 0 if get_entry_size() < 0x0B.
    uint32_t get_vendor_syndrome() const;

    /// @brief True if get_vendor_syndrome() is a real reading, i.e. the
    /// field is present (get_entry_size() >= 0x0B) and not the all-zero
    /// "unknown" sentinel.
    bool is_vendor_syndrome_known() const;

    /// @brief 0x0B offset, QWORD
    /// 64-bit physical address of the error, based on the addressing of the
    /// bus the memory array is connected to. Unknown sentinel is
    /// `8000 0000 0000 0000h` - see is_memory_array_error_address_known().
    /// 0 if get_entry_size() < 0x13.
    uint64_t get_memory_array_error_address() const;

    /// @brief True if get_memory_array_error_address() is a real reading,
    /// i.e. the field is present (get_entry_size() >= 0x13) and not the
    /// `8000 0000 0000 0000h` "unknown" sentinel.
    bool is_memory_array_error_address_known() const;

    /// @brief 0x13 offset, QWORD
    /// 64-bit physical address of the error relative to the start of the
    /// failing memory device, in bytes. Unknown sentinel is
    /// `8000 0000 0000 0000h` - see is_device_error_address_known(). 0 if
    /// get_entry_size() < 0x1B.
    uint64_t get_device_error_address() const;

    /// @brief True if get_device_error_address() is a real reading, i.e.
    /// the field is present (get_entry_size() >= 0x1B) and not the
    /// `8000 0000 0000 0000h` "unknown" sentinel.
    bool is_device_error_address_known() const;

    /// @brief 0x1B offset, DWORD (NOT widened to QWORD - see this header's
    /// own leading comment on Table 106's width asymmetry).
    /// Range, in bytes, within which the error can be determined when an
    /// error address is given. Unknown sentinel is `8000 0000h` - see
    /// is_error_resolution_known(). 0 if get_entry_size() < 0x1F.
    uint32_t get_error_resolution() const;

    /// @brief True if get_error_resolution() is a real reading, i.e. the
    /// field is present (get_entry_size() >= 0x1F) and not the
    /// `8000 0000h` "unknown" sentinel.
    bool is_error_resolution_known() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Human-readable name for get_error_type(); "Unknown" (the
    /// spec's own literal value name, byte 02h) for both the genuine 02h
    /// reading and any unregistered byte value.
    std::string get_error_type_string() const;

    /// @brief Human-readable name for get_error_granularity(); see
    /// get_error_type_string()'s own fallback note.
    std::string get_error_granularity_string() const;

    /// @brief Human-readable name for get_error_operation(); see
    /// get_error_type_string()'s own fallback note.
    std::string get_error_operation_string() const;

private:

    /// Populate the three lookup tables below with the defined enum values.
    void init_string_values();

private:

    /// Error Type value -> human-readable name, @see 7.19.1.
    std::map<uint8_t, std::string> error_type_names_;

    /// Error Granularity value -> human-readable name, @see 7.19.2.
    std::map<uint8_t, std::string> error_granularity_names_;

    /// Error Operation value -> human-readable name, @see 7.19.3.
    std::map<uint8_t, std::string> error_operation_names_;
};

} // namespace smbios
