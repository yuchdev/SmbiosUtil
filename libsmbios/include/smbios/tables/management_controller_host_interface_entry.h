#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <smbios/abstract_smbios_entry.h>

// Management Controller Host Interface entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Management Controller
// Host Interface' chapter (7.43), DSP0134 3.9.0. Field offsets (Table 122),
// the Interface Type field's enumerated values (7.43.1, Table 124), and the
// Protocol Record layout (7.43.2/Table 123) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// REDACTED-GAP RESOLUTION: this task's own source excerpt redacted Table
// 123's exact Protocol Record layout. The full table (DSP0134 3.9.0 p.142)
// confirms each record is exactly `{Protocol Type (BYTE, ENUM, 7.43.2),
// Protocol Type Specific Data Length (BYTE, N), Protocol Type Specific Data
// (N BYTEs)}` - i.e. the task spec's own "likely" guess was correct: a
// 2-byte fixed header per record followed by that record's own
// variable-length payload, with no padding or alignment between records.
//
// SCOPE CUTOFF (R8, see this story's own README "Key constraints"): this
// class implements the SMBIOS 3.2+ shape only - Interface Type, Interface
// Type Specific Data Length, the variable-length type-specific data it
// sizes, Number of Protocol Records, and the Protocol Records array. A
// pre-3.2 structure with Interface Type == OEM and no Interface Type
// Specific Data Length field is decoded using this same 3.2+ field layout
// (i.e. not specially detected or worked around), matching this project's
// policy of writing against the current spec revision rather than every
// historical revision's binary-compatibility quirk.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Management Controller Host Interface structure (7.43) - a
/// physical management controller host interface (e.g. an MCTP host
/// interface, DSP0239) and the one or more protocols (e.g. MCTP, IPMI,
/// Redfish-over-IP) that share it, each described by a Protocol Record
/// (Table 123).
///
/// Unlike MemoryChannelEntry's fixed-stride trailing array, the Protocol
/// Records array has no fixed per-record size: each record's own Protocol
/// Type Specific Data Length field determines where the next record
/// begins, so get_protocol_record_at() must walk the array record-by-record
/// from its start rather than computing an offset directly from `index`.
/// Every byte read during that walk is bounds-guarded against
/// get_entry_size() so a malformed/truncated record list can never be read
/// out of bounds.
class ManagementControllerHostInterfaceEntry : public AbstractSMBiosEntry {
public:

    /// @brief Interface Type field (0x04), DSP0134 3.9.0 Table 124 (7.43.1).
    /// 0x00-0x3F are MCTP Host Interfaces (DSP0239 assigns the specific
    /// values); only the two named boundary values plus the project's own
    /// absent-field sentinel are given symbolic names here, matching this
    /// codebase's established "only name what's reasonably enumerable"
    /// precedent (e.g. ManagementDeviceEntry's DeviceType list is exhaustive
    /// because Table 108 is short; this table's MCTP range is not).
    enum InterfaceType : uint8_t {
        InterfaceTypeMctpRangeLow = 0x00,
        InterfaceTypeMctpRangeHigh = 0x3F,
        InterfaceTypeNetworkHostInterface = 0x40,
        InterfaceTypeOem = 0xF0,
        InterfaceTypeOutOfSpec = 0xFF
    };

    /// @brief One decoded Protocol Record (Table 123): the Protocol Type
    /// byte plus its own variable-length, type-specific payload (raw, not
    /// further interpreted - matching this project's existing policy for
    /// opaque type-specific-data fields elsewhere, e.g.
    /// ManagementControllerHostInterfaceEntry::get_interface_specific_data()
    /// itself).
    struct ProtocolRecord {
        /// Protocol Type (0x00 offset within the record), see 7.43.2.
        uint8_t protocol_type = 0;
        /// Protocol Type Specific Data (0x02+ offset within the record),
        /// raw bytes, length given by the record's own Protocol Type
        /// Specific Data Length field.
        std::vector<uint8_t> data;
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ManagementControllerHostInterfaceEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ManagementControllerHostInterfaceEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset, raw byte. See InterfaceType for the named
    /// boundary values; most byte values in range are MCTP host interface
    /// type values assigned by DSP0239, not individually enumerated here.
    /// Returns InterfaceTypeOutOfSpec if get_entry_size() < 0x05.
    InterfaceType get_interface_type() const;

    /// @brief String representation of get_interface_type().
    std::string get_interface_type_string() const;

    /// @brief 0x05 offset, raw byte: Interface Type Specific Data Length
    /// (`N`). Returns 0 if get_entry_size() < 0x06.
    uint8_t get_interface_specific_data_length() const;

    /// @brief 0x06..0x06+N-1 offset, raw bytes: the Interface Type Specific
    /// Data itself (Management Controller Host Interface Data, as specified
    /// by Interface Type - e.g. IANA vendor ID for Interface Type == OEM,
    /// or an MCTP Host Interface descriptor per DSP0256). Returned as raw
    /// bytes, not further interpreted. Independently bounds-guarded against
    /// get_entry_size(): returns as many bytes as both `N` and the buffer
    /// actually have, never reading past get_entry_size().
    std::vector<uint8_t> get_interface_specific_data() const;

    /// @brief 0x06+N offset, raw byte: Number of Protocol Records (`X`).
    /// Returns 0 if get_entry_size() < 0x07+N (i.e. the byte itself is out
    /// of bounds).
    uint8_t get_protocol_record_count() const;

    /// @brief Decode and return the Protocol Record at `index`, 0-based, by
    /// walking the variable-length Protocol Records array from its own
    /// start (0x07+N) - there is no fixed per-record stride, so every
    /// preceding record's own length must be read to find where `index`'s
    /// record begins. Every byte read during the walk (each record's own
    /// Protocol Type, Protocol Type Specific Data Length, and Protocol Type
    /// Specific Data bytes) is bounds-guarded against get_entry_size(); a
    /// malformed/truncated record list stops the walk cleanly rather than
    /// reading out of bounds.
    /// @param index 0-based index into the Protocol Records array.
    /// @return the decoded record, or a default-constructed ProtocolRecord
    /// (protocol_type == 0, empty data) if `index >=
    /// get_protocol_record_count()` or the walk runs off the end of the
    /// buffer before reaching `index`.
    ProtocolRecord get_protocol_record_at(size_t index) const;

private:

    /// Map named InterfaceType boundary values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> interface_type_map_;
};

} // namespace smbios
