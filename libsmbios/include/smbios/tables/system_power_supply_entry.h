#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <smbios/abstract_smbios_entry.h>

// System Power Supply entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Power Supply'
// chapter (7.40), DSP0134 3.9.0. Field offsets (Table 116) and the Power
// Supply Characteristics bit layout (7.40.1, Table 117) were confirmed
// directly against docs/standard/DSP0134_3.9.0.md.
//
// UNITS RESOLUTION (Max Power Capacity, 0x0C): Table 116's own Max Power
// Capacity row text reads "Maximum sustained power output in Watts ...
// Note that the units specified by DMTF for this field are milliwatts" -
// i.e. the spec's own field description text and its own parenthetical
// DMTF-units note directly contradict each other within the same cell.
// This class documents the raw WORD value as Watts (matching the field
// description's primary prose, and matching the reference `dmidecode`
// implementation - see c-versions/dmidecode_linux/dmidecode.c and
// c-versions/dmidecode_macos/dmidecode.c, both of which print this field
// as `"%u W"` with no scaling) rather than attempting an unverifiable /1000
// conversion from a note that contradicts the very table it annotates. See
// get_max_power_capacity()'s own Doxygen comment for the citation a future
// reader needs when comparing this project's output against another
// tool's.
//
// Power Supply Characteristics bit layout (7.40.1, Table 117), confirmed
// against the full table (the task's own source excerpt did not include
// these rows):
//   bits 15:14 - Reserved, set to 00b
//   bits 13:10 - DMTF Power Supply Type (PowerSupplyType enum below)
//   bits 9:7   - Status (PowerSupplyStatus enum below)
//   bits 6:3   - DMTF Input Voltage Range Switching (VoltageSwitching enum
//                below)
//   bit 2      - 1b: power supply is unplugged from the wall
//   bit 1      - 1b: power supply is present
//   bit 0      - 1b: power supply is hot-replaceable
//
// Handle "absent" sentinel (Input Voltage Probe Handle 0x10, Cooling
// Device Handle 0x12, Input Current Probe Handle 0x14): DSP0134 3.9.0's
// general Handle field description (7.2, Table 10's own Handle row) states
// "Unless otherwise specified, when referring to another structure's
// handle, the value 0FFFFh is used to indicate that the referenced handle
// is not applicable or does not exist." Table 116 does not otherwise
// specify a different sentinel for any of these three handle fields, so
// the general 0xFFFF convention applies to all three (not 0x0000 - handle
// 0 is a theoretically valid, if unusual, handle value elsewhere in the
// spec).
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Power Supply structure (7.40) - one instance per
/// possible power supply in the system: its redundancy group, identifying
/// strings, maximum sustained power output, a packed Power Supply
/// Characteristics bit-field (7.40.1), and three cross-reference handles
/// into the probe/cooling-device family this milestone already implements
/// (Voltage Probe/Type 26, Cooling Device/Type 27, Electrical Current
/// Probe/Type 29) - exposed as raw handles only, no cross-structure
/// resolution, matching every other handle field in this codebase.
///
/// See this header's own leading comment block for the Max Power
/// Capacity units resolution and the confirmed Power Supply
/// Characteristics bit layout/handle-absent sentinel, each cited against
/// the real spec text rather than re-derived.
class SystemPowerSupplyEntry : public AbstractSMBiosEntry {
public:

    /// @brief Handle sentinel for "no such reference" on any of this
    /// structure's three handle fields. See this header's own leading
    /// comment block for the citation (DSP0134 3.9.0, general Handle field
    /// description, 7.2/Table 10).
    static constexpr uint16_t HandleAbsent = 0xFFFF;

    /// @brief Max Power Capacity sentinel (0x0C): unknown, per Table 116.
    static constexpr uint16_t MaxPowerCapacityUnknown = 0x8000;

    /// @brief DMTF Power Supply Type sub-field, bits 13:10 of Power Supply
    /// Characteristics (0x0E), DSP0134 3.9.0 Table 117.
    /// `PowerSupplyTypeOutOfSpec` (value 0, binary 0000b) is not a spec
    /// value (the spec's own enumeration starts at 0001b); this project's
    /// own fallback sentinel for "field absent or the reserved 0000b
    /// encoding", matching this codebase's established `*OutOfSpec`
    /// precedent.
    enum PowerSupplyType : uint8_t {
        PowerSupplyTypeOutOfSpec = 0x0,
        PowerSupplyTypeOther = 0x1,
        PowerSupplyTypeUnknown = 0x2,
        PowerSupplyTypeLinear = 0x3,
        PowerSupplyTypeSwitching = 0x4,
        PowerSupplyTypeBattery = 0x5,
        PowerSupplyTypeUps = 0x6,
        PowerSupplyTypeConverter = 0x7,
        PowerSupplyTypeRegulator = 0x8
    };

    /// @brief Status sub-field, bits 9:7 of Power Supply Characteristics
    /// (0x0E), DSP0134 3.9.0 Table 117. `PowerSupplyStatusOutOfSpec`
    /// (value 0, binary 000b) is not a spec value (the spec's own
    /// enumeration starts at 001b); this project's own fallback sentinel,
    /// matching the PowerSupplyType convention above. Note this sub-field
    /// has no "Critical; power supply has failed and has been taken
    /// off-line" successor value beyond 101b - unlike VoltageProbeEntry's/
    /// CoolingDeviceEntry's/TemperatureProbeEntry's/
    /// ElectricalCurrentProbeEntry's shared six-value Status sub-field
    /// (which also defines Non-recoverable, 110b), this one stops at
    /// Critical (101b); it is a different, shorter enumeration, not the
    /// same sub-field reused.
    enum PowerSupplyStatus : uint8_t {
        PowerSupplyStatusOutOfSpec = 0x0,
        PowerSupplyStatusOther = 0x1,
        PowerSupplyStatusUnknown = 0x2,
        PowerSupplyStatusOk = 0x3,
        PowerSupplyStatusNonCritical = 0x4,
        PowerSupplyStatusCritical = 0x5
    };

    /// @brief DMTF Input Voltage Range Switching sub-field, bits 6:3 of
    /// Power Supply Characteristics (0x0E), DSP0134 3.9.0 Table 117.
    /// `VoltageSwitchingOutOfSpec` (value 0, binary 0000b) is not a spec
    /// value (the spec's own enumeration starts at 0001b); this project's
    /// own fallback sentinel, matching the PowerSupplyType convention
    /// above.
    enum VoltageSwitching : uint8_t {
        VoltageSwitchingOutOfSpec = 0x0,
        VoltageSwitchingOther = 0x1,
        VoltageSwitchingUnknown = 0x2,
        VoltageSwitchingManual = 0x3,
        VoltageSwitchingAutoSwitch = 0x4,
        VoltageSwitchingWideRange = 0x5,
        VoltageSwitchingNotApplicable = 0x6
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemPowerSupplyEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemPowerSupplyEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset, raw byte. Power unit group this supply is
    /// associated with; 0 means the supply is not a member of a redundant
    /// power unit (see is_redundant()). Returns 0 if get_entry_size() <
    /// 0x05.
    uint8_t get_power_unit_group() const;

    /// @brief 0x0C offset, 2 raw bytes. MaxPowerCapacityUnknown (0x8000)
    /// means unknown, per spec. Returns MaxPowerCapacityUnknown if
    /// get_entry_size() < 0x0E.
    ///
    /// @note Units: documented here as Watts, matching Table 116's own
    /// field description prose ("Maximum sustained power output in
    /// Watts") and the reference `dmidecode` implementation's unscaled
    /// `"%u W"` rendering (c-versions/dmidecode_linux/dmidecode.c,
    /// c-versions/dmidecode_macos/dmidecode.c). DSP0134 3.9.0's own Table
    /// 116 text immediately contradicts this in the same cell with "Note
    /// that the units specified by DMTF for this field are milliwatts" -
    /// that inconsistency is the spec's own, not introduced by this
    /// class. This accessor returns the raw WORD value unscaled either
    /// way; if a future reader needs to cross-check against a tool that
    /// *does* apply a /1000 milliwatts-to-watts conversion, that
    /// discrepancy traces back to this unresolved spec text, not a bug
    /// here.
    uint16_t get_max_power_capacity() const;

    /// @brief 0x0E offset, 2 raw bytes. See PowerSupplyType/
    /// PowerSupplyStatus/VoltageSwitching enums and the decoded sub-field
    /// accessors below for the confirmed Table 117 bit layout. Returns 0
    /// if get_entry_size() < 0x10.
    uint16_t get_characteristics() const;

    /// @brief 0x10 offset, 2 raw bytes. Handle, or instance number, of a
    /// voltage probe (Type 26) monitoring this power supply's input
    /// voltage, or HandleAbsent (0xFFFF) if none. Returns HandleAbsent if
    /// get_entry_size() < 0x12.
    uint16_t get_input_voltage_probe_handle() const;

    /// @brief 0x12 offset, 2 raw bytes. Handle, or instance number, of a
    /// cooling device (Type 27) associated with this power supply, or
    /// HandleAbsent (0xFFFF) if none. Returns HandleAbsent if
    /// get_entry_size() < 0x14.
    uint16_t get_cooling_device_handle() const;

    /// @brief 0x14 offset, 2 raw bytes. Handle, or instance number, of the
    /// electrical current probe (Type 29) monitoring this power supply's
    /// input current, or HandleAbsent (0xFFFF) if none. Returns
    /// HandleAbsent if get_entry_size() < 0x16.
    uint16_t get_input_current_probe_handle() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x05 offset DMI string: the power supply's location. Returns
    /// dmi_string(0) ("Not Specified") if get_entry_size() < 0x06.
    std::string get_location_string() const;

    /// @brief 0x06 offset DMI string: the power supply device's name.
    /// Returns dmi_string(0) ("Not Specified") if get_entry_size() < 0x07.
    std::string get_device_name_string() const;

    /// @brief 0x07 offset DMI string: the power supply's manufacturer.
    /// Returns dmi_string(0) ("Not Specified") if get_entry_size() < 0x08.
    std::string get_manufacturer_string() const;

    /// @brief 0x08 offset DMI string: the power supply's serial number.
    /// Returns dmi_string(0) ("Not Specified") if get_entry_size() < 0x09.
    std::string get_serial_number_string() const;

    /// @brief 0x09 offset DMI string: the power supply's Asset Tag Number.
    /// Returns dmi_string(0) ("Not Specified") if get_entry_size() < 0x0A.
    std::string get_asset_tag_string() const;

    /// @brief 0x0A offset DMI string: the power supply's OEM Part Order
    /// Number. Returns dmi_string(0) ("Not Specified") if get_entry_size()
    /// < 0x0B.
    std::string get_model_part_number_string() const;

    /// @brief 0x0B offset DMI string: the power supply's revision string.
    /// Returns dmi_string(0) ("Not Specified") if get_entry_size() < 0x0C.
    std::string get_revision_level_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Power Supply Characteristics sub-fields (Table 117)

    /// @brief Bits 13:10 of Power Supply Characteristics. See
    /// PowerSupplyType for values. Returns PowerSupplyTypeOutOfSpec if
    /// get_entry_size() < 0x10.
    PowerSupplyType get_power_supply_type() const;

    /// @brief String representation of get_power_supply_type().
    std::string get_power_supply_type_string() const;

    /// @brief Bits 9:7 of Power Supply Characteristics. See
    /// PowerSupplyStatus for values. Returns PowerSupplyStatusOutOfSpec if
    /// get_entry_size() < 0x10.
    PowerSupplyStatus get_power_supply_status() const;

    /// @brief String representation of get_power_supply_status().
    std::string get_power_supply_status_string() const;

    /// @brief Bits 6:3 of Power Supply Characteristics. See
    /// VoltageSwitching for values. Returns VoltageSwitchingOutOfSpec if
    /// get_entry_size() < 0x10.
    VoltageSwitching get_voltage_switching() const;

    /// @brief String representation of get_voltage_switching().
    std::string get_voltage_switching_string() const;

    /// @brief Bit 2 of Power Supply Characteristics: 1b means the power
    /// supply is unplugged from the wall. Returns false if get_entry_size()
    /// < 0x10.
    bool is_unplugged() const;

    /// @brief Bit 1 of Power Supply Characteristics: 1b means the power
    /// supply is present. Returns false if get_entry_size() < 0x10.
    bool is_present() const;

    /// @brief Bit 0 of Power Supply Characteristics: 1b means the power
    /// supply is hot-replaceable. Returns false if get_entry_size() <
    /// 0x10.
    bool is_hot_replaceable() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether Power Unit Group (0x04) is non-zero, i.e. this power
    /// supply shares its group with at least one other power supply
    /// (redundant configuration), per spec. Returns false if
    /// get_entry_size() < 0x05.
    bool is_redundant() const;

    /// @brief Whether Max Power Capacity (0x0C) is known, i.e. not
    /// MaxPowerCapacityUnknown. Note this does not independently
    /// length-guard beyond get_max_power_capacity()'s own guard (the field
    /// is within this structure's minimum 0x10 length, so it is always
    /// physically present in any valid structure this class accepts).
    bool is_max_power_capacity_known() const;

    /// @brief Whether Input Voltage Probe Handle (0x10) references a real
    /// structure, i.e. the field is present (get_entry_size() >= 0x12) and
    /// not HandleAbsent.
    bool has_input_voltage_probe() const;

    /// @brief Whether Cooling Device Handle (0x12) references a real
    /// structure, i.e. the field is present (get_entry_size() >= 0x14) and
    /// not HandleAbsent.
    bool has_cooling_device() const;

    /// @brief Whether Input Current Probe Handle (0x14) references a real
    /// structure, i.e. the field is present (get_entry_size() >= 0x16) and
    /// not HandleAbsent.
    bool has_input_current_probe() const;

private:

    /// Map PowerSupplyType/PowerSupplyStatus/VoltageSwitching values to
    /// string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> power_supply_type_map_;
    std::map<uint8_t, std::string> power_supply_status_map_;
    std::map<uint8_t, std::string> voltage_switching_map_;
};

} // namespace smbios
