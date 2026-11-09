#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.h>

// Built-in Pointing Device entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Built-in Pointing Device'
// chapter (7.22), DSP0134 3.9.0. Field offsets and the Table 88 (Pointing
// Device - Type) / Table 89 (Pointing Device - Interface) enumerations were
// confirmed directly against docs/standard/DSP0134_3.9.0.md's own
// 7.22/7.22.1/7.22.2 (not assumed from any task-document reproduction).

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Built-in Pointing Device structure (7.22) - the device type
/// (7.22.1), its interface (7.22.2), and its button count. The presence of
/// this structure does not imply the device is active for the system's use
/// (per the spec's own prose for 7.22).
///
/// Naming hazard (deliberately avoided): the formatted area's own field at
/// offset 0x04 is, per Table 87, itself literally named "Type" - distinct
/// from the structure header's own `Type` byte at offset 0x00 (already
/// consumed by `AbstractSMBiosEntry`/`SMBiosEntryFactory`) and from
/// `SMBiosInterface::get_type()` (the inherited "structure kind" accessor,
/// which returns `"Built-in Pointing Device"` here, not this per-device
/// enum). To avoid shadowing or confusing that inherited meaning, the
/// accessor for offset 0x04 is named `get_device_type()`
/// (+`get_device_type_string()`), never `get_type()`.
class PointingDeviceEntry : public AbstractSMBiosEntry {
public:

    /// @brief Pointing Device Type field (offset 0x04), see 7.22.1's
    /// Table 88. `DeviceTypeOutOfSpec` (0x00) is not a spec value; it is
    /// this project's own fallback sentinel for "field absent or value not
    /// in Table 88", matching BaseboardInformationEntry::BoardType's own
    /// `BoardTypeOutOfSpec` precedent.
    enum DeviceType : uint8_t {
        DeviceTypeOutOfSpec = 0x00,
        Other = 0x01,
        Unknown = 0x02,
        Mouse = 0x03,
        TrackBall = 0x04,
        TrackPoint = 0x05,
        GlidePoint = 0x06,
        TouchPad = 0x07,
        TouchScreen = 0x08,
        OpticalSensor = 0x09
    };

    /// @brief Pointing Device Interface field (offset 0x05), see 7.22.2's
    /// Table 89. `InterfaceOutOfSpec` (0x00) is this project's own fallback
    /// sentinel (0x00 is not itself a Table 89 value), same precedent as
    /// `DeviceTypeOutOfSpec` above.
    enum Interface : uint8_t {
        InterfaceOutOfSpec = 0x00,
        InterfaceOther = 0x01,
        InterfaceUnknown = 0x02,
        Serial = 0x03,
        PS2 = 0x04,
        Infrared = 0x05,
        HPHIL = 0x06,
        BusMouse = 0x07,
        ADB = 0x08,
        BusMouseDB9 = 0xA0,
        BusMouseMicroDIN = 0xA1,
        USB = 0xA2,
        I2C = 0xA3,
        SPI = 0xA4
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in Built-in Pointing Device SMBIOS entry depending on header.length.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    PointingDeviceEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~PointingDeviceEntry() = default;

    /// @brief String representation of the structure kind (inherited
    /// meaning - NOT the per-device Type field; see get_device_type()).
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset. See DeviceType enum for values. Named
    /// get_device_type() (NOT get_type()) deliberately - see the class doc
    /// comment's naming-hazard note.
    uint8_t get_device_type() const;

    /// @brief 0x05 offset. See Interface enum for values.
    uint8_t get_interface() const;

    /// @brief 0x06 offset. Number of buttons on the pointing device (e.g.
    /// 0x03 for a three-button device); 0 if get_entry_size() < 0x07.
    uint8_t get_button_count() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Device Type string representation; see get_device_type().
    std::string get_device_type_string() const;

    /// @brief Interface string representation; see get_interface().
    std::string get_interface_string() const;

private:

    /// Map flags/codes data to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> device_type_map_;
    std::map<uint8_t, std::string> interface_map_;
};

} // namespace smbios
