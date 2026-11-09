#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.h>

// 32-Bit Memory Error Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, '32-Bit Memory Error
// Information' chapter (7.19), DSP0134 3.9.0, Table 81. Field offsets and
// enum tables (7.19.1 Error Type, 7.19.2 Error Granularity, 7.19.3 Error
// Operation) confirmed directly against docs/standard/DSP0134_3.9.0.md.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The 32-Bit Memory Error Information structure (7.19) - identifies
/// the specifics of an error that might be detected within a Physical
/// Memory Array, with every address field 32-bit wide. See
/// MemoryErrorInformation64BitEntry for the 64-bit-address counterpart
/// (7.34) used when the error address is above 4GiB.
///
/// Sentinel discipline (Table 81): Vendor Syndrome's "unknown" marker is
/// all-zero (`0000 0000h`) - a different convention than the three address/
/// resolution fields below it, each of which uses the high-bit-set marker
/// `8000 0000h`. Every `is_*_known()` predicate below checks the exact
/// sentinel for its own field's width/convention, never conflating the two.
class MemoryErrorInformation32BitEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header; every field is distinguished by
    /// get_entry_size() alone (no SMBIOS-version gating - 7.19's own Table
    /// 81 marks every field 2.1+, and the structure's fixed 0x17-byte length
    /// has not changed since).
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    MemoryErrorInformation32BitEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~MemoryErrorInformation32BitEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/dword values

    /// @brief 0x04 offset, BYTE, ENUM
    /// Type of error associated with the current status reported for the
    /// memory array or device; see @see 7.19.1 for defined values. 0 if
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

    /// @brief 0x0B offset, DWORD
    /// 32-bit physical address of the error, based on the addressing of the
    /// bus the memory array is connected to. Unknown sentinel is
    /// `8000 0000h` - see is_memory_array_error_address_known(). 0 if
    /// get_entry_size() < 0x0F.
    uint32_t get_memory_array_error_address() const;

    /// @brief True if get_memory_array_error_address() is a real reading,
    /// i.e. the field is present (get_entry_size() >= 0x0F) and not the
    /// `8000 0000h` "unknown" sentinel.
    bool is_memory_array_error_address_known() const;

    /// @brief 0x0F offset, DWORD
    /// 32-bit physical address of the error relative to the start of the
    /// failing memory device, in bytes. Unknown sentinel is `8000 0000h` -
    /// see is_device_error_address_known(). 0 if get_entry_size() < 0x13.
    uint32_t get_device_error_address() const;

    /// @brief True if get_device_error_address() is a real reading, i.e.
    /// the field is present (get_entry_size() >= 0x13) and not the
    /// `8000 0000h` "unknown" sentinel.
    bool is_device_error_address_known() const;

    /// @brief 0x13 offset, DWORD
    /// Range, in bytes, within which the error can be determined when an
    /// error address is given. Unknown sentinel is `8000 0000h` - see
    /// is_error_resolution_known(). 0 if get_entry_size() < 0x17.
    uint32_t get_error_resolution() const;

    /// @brief True if get_error_resolution() is a real reading, i.e. the
    /// field is present (get_entry_size() >= 0x17) and not the
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
