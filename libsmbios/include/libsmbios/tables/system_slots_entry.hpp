#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// System Slots entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Slots' chapter
// (7.10). Cross-checked against c-versions/dmidecode_linux/dmidecode.c's
// case 9 (dmi_decode(), ~line 4724, "System Slot Information") and its
// dmi_slot_type()/dmi_slot_bus_width()/dmi_slot_current_usage()/
// dmi_slot_length()/dmi_slot_characteristics() helpers for field offsets,
// length guards and enum value tables.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Slots structure (7.10) - slot designation string
/// index, slot type, data bus width, current usage, slot length, slot ID
/// (low/high byte pair), characteristics bitmasks 1/2 (2.1+), and the
/// segment group number/bus number/device-function number triple (2.6+).
///
/// Scope cutoff (deliberate, matching this milestone's established
/// precedent of documenting scope narrowing rather than silently stopping
/// short): this class stops at the segment group/bus/device-function
/// number fields (offset 0x0D-0x10, guarded by length >= 0x11). The 3.2+
/// additions that follow - data bus width (base), peer devices count, the
/// variable-length peer-device array itself (at the computed offset
/// `0x13 + peer_count * 5`, a second layer of the same computed-offset
/// complexity SystemEnclosureEntry's contained-elements array already
/// demonstrated once in this milestone's Task 01), slot information, slot
/// physical width, slot pitch, and slot height - are NOT implemented here.
/// The task's own "Files" section names only designation through segment
/// group/bus/device-function number, so this cutoff is intentional, not an
/// oversight.
///
/// Scope note on get_slot_type_string(): the DMTF 7.10.1 slot-type table
/// (dmi_slot_type()'s type[]/type_0x30[]/type_0xA0[] arrays) is reproduced
/// keyed directly by the numeric code (no enum - many entries are not
/// valid C++ identifiers, e.g. "PCI Express x1", "M.2 Socket 1-DP"), same
/// rationale as ProcessorInformationEntry::get_processor_family_string()'s
/// own scope note.
///
/// Scope note on get_slot_id_string(): the reference's own dmi_slot_id()
/// decodes the (slot_id_low, slot_id_high) pair differently depending on
/// slot_type (e.g. a single decimal ID for most bus types, "Adapter N,
/// Socket M" for PCMCIA) - that per-type dispatch is a presentation nuance
/// on top of the two raw ID bytes, not reproduced here; both raw bytes are
/// exposed via get_slot_id_low()/get_slot_id_high() and a generic
/// low[/high] string, mirroring ProcessorInformationEntry::
/// get_processor_id()'s own "raw value, no architecture-specific decode"
/// precedent.
class SystemSlotsEntry : public AbstractSMBiosEntry {
public:

    // @brief Data Bus Width field (offset 0x06), see dmi_slot_bus_width()
    // (7.10.2 of the spec).
    enum DataBusWidth : uint8_t {
        DataBusWidthOutOfSpec = 0x00,
        DataBusWidthOther = 0x01,
        DataBusWidthUnknown = 0x02,
        DataBusWidth8Bit = 0x03,
        DataBusWidth16Bit = 0x04,
        DataBusWidth32Bit = 0x05,
        DataBusWidth64Bit = 0x06,
        DataBusWidth128Bit = 0x07,
        DataBusWidth1xOrX1 = 0x08,
        DataBusWidth2xOrX2 = 0x09,
        DataBusWidth4xOrX4 = 0x0A,
        DataBusWidth8xOrX8 = 0x0B,
        DataBusWidth12xOrX12 = 0x0C,
        DataBusWidth16xOrX16 = 0x0D,
        DataBusWidth32xOrX32 = 0x0E
    };

    // @brief Current Usage field (offset 0x07), see dmi_slot_current_usage()
    // (7.10.3 of the spec).
    enum CurrentUsage : uint8_t {
        CurrentUsageOutOfSpec = 0x00,
        CurrentUsageOther = 0x01,
        CurrentUsageUnknown = 0x02,
        Available = 0x03,
        InUse = 0x04,
        Unavailable = 0x05
    };

    // @brief Slot Length field (offset 0x08), see dmi_slot_length() (7.10.4
    // of the spec).
    enum SlotLength : uint8_t {
        SlotLengthOutOfSpec = 0x00,
        SlotLengthOther = 0x01,
        SlotLengthUnknown = 0x02,
        Short = 0x03,
        Long = 0x04,
        DriveFormFactor2Dot5Inch = 0x05,
        DriveFormFactor3Dot5Inch = 0x06
    };

    // @brief Slot Characteristics 1 field (offset 0x0B), see
    // dmi_slot_characteristics() (7.10.6 of the spec). Bit 0 ("Unknown") is
    // handled specially by get_characteristics_string() rather than listed
    // alongside the other bits - see that method's own comment.
    enum Characteristics1 : uint8_t {
        Characteristics1Unknown = 0x01,
        Provides5V = 0x02,
        Provides3V3 = 0x04,
        OpeningShared = 0x08,
        PCCard16Supported = 0x10,
        CardBusSupported = 0x20,
        ZoomVideoSupported = 0x40,
        ModemRingResumeSupported = 0x80
    };

    // @brief Slot Characteristics 2 field (offset 0x0C, 2.1+), see
    // dmi_slot_characteristics() (7.10.7 of the spec).
    enum Characteristics2 : uint8_t {
        PMESignalSupported = 0x01,
        HotPlugDevicesSupported = 0x02,
        SMBusSignalSupported = 0x04,
        PCIeSlotBifurcationSupported = 0x08,
        AsyncSurpriseRemovalSupported = 0x10,
        Flexbus1Dot0Capable = 0x20,
        Flexbus2Dot0Capable = 0x40,
        Flexbus3Dot0Capable = 0x80
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in System Slots SMBIOS entry depending on version and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemSlotsEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemSlotsEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order; every
    /// enumerated value, the Slot ID ("low" or "low/high") and the Bus
    /// Address (empty when absent or all-FF) stay the printed string.
    /// Characteristics is a single {"Characteristics", "Unknown"|"None"}
    /// field exactly when the text prints that sentinel, otherwise one
    /// {label, true} field per set bit of characteristics 1 then 2.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset
    /// Index of string number of the slot designation
    uint8_t get_designation_index() const;

    /// @brief 0x05 offset
    /// Raw Slot Type code; see get_slot_type_string() scope note - no enum,
    /// the table is reproduced keyed by numeric code
    uint8_t get_slot_type() const;

    /// @brief 0x06 offset
    /// See DataBusWidth enum for values
    uint8_t get_data_bus_width() const;

    /// @brief 0x07 offset
    /// See CurrentUsage enum for values
    uint8_t get_current_usage() const;

    /// @brief 0x08 offset
    /// See SlotLength enum for values
    uint8_t get_slot_length() const;

    /// @brief 0x09 offset
    /// Raw Slot ID low byte; meaning depends on slot type, see class doc
    /// comment's scope note on get_slot_id_string()
    uint8_t get_slot_id_low() const;

    /// @brief 0x0A offset
    /// Raw Slot ID high byte, see get_slot_id_low()
    uint8_t get_slot_id_high() const;

    /// @brief 0x0B offset
    /// Slot Characteristics 1 bitmask, see Characteristics1 enum
    uint8_t get_characteristics1() const;

    /// @brief 0x0C offset (2.1+). Per dmi_slot_characteristics()'s own
    /// `if (h->length < 0x0D) ... data[0x0B], 0x00) else ... data[0x0B],
    /// data[0x0C]` branch: this field always participates in rendering, but
    /// reads as 0 (not "absent"/skipped) when length < 0x0D rather than
    /// 0x0D itself. See Characteristics2 enum for bit values.
    uint8_t get_characteristics2() const;

    /// @brief 0x0D offset (2.6+), 2 bytes
    /// Segment Group Number; 0 when the slot's bus is on segment 0 or the
    /// field is absent
    uint16_t get_segment_group_number() const;

    /// @brief 0x0F offset (2.6+)
    /// Bus Number
    uint8_t get_bus_number() const;

    /// @brief 0x10 offset (2.6+)
    /// Device/Function Number: bits 3-7 are the device number, bits 0-2 are
    /// the function number - see get_device_number()/get_function_number()
    uint8_t get_device_function_number() const;

    /// @brief Device number, bits 3-7 of get_device_function_number()
    uint8_t get_device_number() const;

    /// @brief Function number, bits 0-2 of get_device_function_number()
    uint8_t get_function_number() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Slot designation DMI string
    std::string get_designation_string() const;

    /// @brief Slot type string representation (see class doc comment's
    /// scope note)
    std::string get_slot_type_string() const;

    /// @brief Data bus width string representation
    std::string get_data_bus_width_string() const;

    /// @brief Current usage string representation
    std::string get_current_usage_string() const;

    /// @brief Slot length string representation
    std::string get_slot_length_string() const;

    /// @brief Slot ID string representation (see class doc comment's scope
    /// note); renders the low byte alone, or "low/high" when the high byte
    /// is non-zero
    std::string get_slot_id_string() const;

    /// @brief Characteristics 1+2 string representation, mirroring
    /// dmi_slot_characteristics(): bit 0 of characteristics1 set ->
    /// "Unknown"; both bytes entirely zero (apart from that bit) -> "None";
    /// otherwise a \n\t-separated list of the set bits from both bytes
    std::string get_characteristics_string() const;

    /// @brief Segment:Bus:Device.Function string representation, e.g.
    /// "0000:01:02.3"; empty when segment/bus/device/function are all the
    /// all-FF/all-FFFF sentinel (meaning "not applicable"), mirroring
    /// dmi_slot_segment_bus_func()'s own `code1==0xFFFF && code2==0xFF &&
    /// code3==0xFF` check, or when length < 0x11 (field absent)
    std::string get_bus_address_string() const;

private:

    /// The single-word Characteristics sentinel the text prints instead of a
    /// bit list - "Unknown" (bit 0 of characteristics1 set) or "None" (no
    /// other bit set in either byte) - or an empty string when the set bits
    /// are listed; shared by get_characteristics_string() and get_fields()
    std::string characteristics_sentinel() const;

    /// Map flags/codes data to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> slot_type_map_;
    std::map<uint8_t, std::string> data_bus_width_map_;
    std::map<uint8_t, std::string> current_usage_map_;
    std::map<uint8_t, std::string> slot_length_map_;
    std::map<uint8_t, std::string> characteristics1_map_;
    std::map<uint8_t, std::string> characteristics2_map_;
};

} // namespace smbios
