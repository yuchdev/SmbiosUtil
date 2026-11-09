#pragma once
#include <cstdint>
#include <string>
#include <libsmbios/abstract_smbios_entry.h>

// Management Device Component entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Management Device
// Component' chapter (7.36), DSP0134 3.9.0. Field offsets (Table 110) were
// confirmed directly against docs/standard/DSP0134_3.9.0.md.
//
// Threshold Handle (0x09) "no threshold" sentinel confirmation: Table 110's
// own row for Threshold Handle does not state an explicit sentinel value
// ("Handle, or instance number, associated with the device thresholds; see
// 7.37."). However, DSP0134 3.9.0's generic structure-header text (6.1.2,
// Table 3, describing the Handle field every structure carries) states the
// project-wide default explicitly: "Unless otherwise specified, when
// referring to another structure's handle, the value 0FFFFh is used to
// indicate that the referenced handle is not applicable or does not exist."
// Since 7.36/Table 110 does not override this default for Threshold Handle,
// the general convention applies: 0xFFFF means "no threshold data attached
// to this component" - confirmed against the spec text rather than assumed.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Management Device Component structure (7.36) - associates a
/// cooling device or environmental probe (7.27-7.30, referenced by
/// Component Handle) with the Management Device (7.35, see
/// ManagementDeviceEntry) that controls it, and optionally with a
/// Management Device Threshold Data structure (7.37, see
/// ManagementDeviceThresholdDataEntry) describing its thresholds. None of
/// the three handle fields below is resolved or validated against the
/// structure it refers to - this class only exposes the raw handle value,
/// matching this codebase's existing precedent (e.g.
/// PhysicalMemoryArrayEntry's Memory Error Information Handle).
class ManagementDeviceComponentEntry : public AbstractSMBiosEntry {
public:

    /// @brief Threshold Handle (0x09) "no threshold data" sentinel. See the
    /// class doc comment above (out_of_band_remote_access_entry-style
    /// confirmation, this time against DSP0134 3.9.0's generic Table 3
    /// Handle-field text) for why this is 0xFFFF rather than an assumption.
    static constexpr uint16_t NoThresholdHandle = 0xFFFF;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ManagementDeviceComponentEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ManagementDeviceComponentEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x04 offset DMI string. Additional descriptive information
    /// about the component. Returns an empty string if get_entry_size() <
    /// 0x05.
    std::string get_description_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Word values (raw, unresolved handles)

    /// @brief 0x05 offset, raw word. Handle, or instance number, of the
    /// Management Device (7.35) that contains this component. Not
    /// validated against any structure. Returns 0 if get_entry_size() <
    /// 0x07.
    uint16_t get_management_device_handle() const;

    /// @brief 0x07 offset, raw word. Handle, or instance number, of the
    /// probe or cooling device that defines this component (7.27-7.30). Not
    /// validated against any structure. Returns 0 if get_entry_size() <
    /// 0x09.
    uint16_t get_component_handle() const;

    /// @brief 0x09 offset, raw word. Handle, or instance number, associated
    /// with the device thresholds (7.37), or NoThresholdHandle (0xFFFF) if
    /// none - see the class doc comment for the sentinel's confirmation.
    /// Not validated against any structure. Returns NoThresholdHandle if
    /// get_entry_size() < 0x0B.
    uint16_t get_threshold_handle() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether get_threshold_handle() refers to an actual Management
    /// Device Threshold Data structure, i.e. is not NoThresholdHandle.
    /// Returns false if get_entry_size() < 0x0B.
    bool has_threshold() const;
};

} // namespace smbios
