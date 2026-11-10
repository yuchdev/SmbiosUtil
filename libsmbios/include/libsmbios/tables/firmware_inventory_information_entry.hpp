#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Firmware Inventory Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Firmware Inventory
// Information' chapter (7.46), DSP0134 3.9.0. Field offsets (Table 133),
// the Version Format enum (7.46.1, Table 134), the Firmware ID Format enum
// (7.46.2, Table 135), the Characteristics bit-field (7.46.3, Table 136),
// and the State enum (7.46.4, Table 137) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// ASSOCIATED COMPONENT HANDLES OFFSET - CONFIRMED AGAINST THE FULL TABLE
// 133, NOT JUST THIS TASK'S OWN (DELIBERATELY TRUNCATED) SOURCE EXCERPT:
// Table 133 is split across two source pages; the row for 0x17 (Number of
// Associated Components, n) is the LAST row on the first page, and the
// very next row - with no field in between - is 0x18 (Associated
// Component Handles, n WORDs). There is nothing between the 0x17 count
// byte and the 0x18 array start.
//
// EXPLICIT COUNT FIELD (0x17) - NEVER DERIVE FROM LENGTH:
// Table 133's own Length row states Length is "computed by the platform
// firmware as 24 + (2*n)" but explicitly warns: "To allow future structure
// growth by appending information after the Associated Components Handles
// list, this field must not be used to determine the number of Associated
// Components Handles specified within the structure." This is the third
// confirmed instance of this exact idiom in this milestone, after Memory
// Channel's Memory Device Count (Story 05.0) and Additional Information's
// entry count (Story 06.0) - get_associated_component_count() therefore
// always reads the explicit 0x17 byte directly, never derives it from
// get_entry_size()/header Length.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Firmware Inventory Information structure (7.46) - one
/// instance per firmware component in the system (platform firmware, BMC,
/// or firmware for any other device), used by software to display a
/// uniform firmware inventory or by a management controller for remote
/// system management. Not intended to replace other standard programmatic
/// firmware-update interfaces.
///
/// See this header's own leading comment block for the confirmed
/// Associated Component Handles array offset (0x18, immediately following
/// the 0x17 count byte with nothing in between) and for the explicit-
/// count-field discipline this class follows for that trailing array.
class FirmwareInventoryInformationEntry : public AbstractSMBiosEntry {
public:

    /// @brief Version Format field (0x06), DSP0134 3.9.0 Table 134
    /// (7.46.1). Governs the string format of both Firmware Version (0x05)
    /// and Lowest Supported Firmware Version (0x0B). Values 0x04-0x7F are
    /// reserved for future assignment by the specification; 0x80-0xFF are
    /// firmware vendor/OEM-specific - both ranges are rendered distinctly
    /// by get_version_format_string(), same rationale as
    /// StringPropertyEntry::get_property_id_string()'s reserved-range
    /// rendering.
    enum VersionFormat : uint8_t {
        VersionFormatFreeForm = 0x00,
        VersionFormatMajorMinor = 0x01,
        VersionFormatHex32 = 0x02,
        VersionFormatHex64 = 0x03
    };

    /// @brief Firmware ID Format field (0x08), DSP0134 3.9.0 Table 135
    /// (7.46.2). Governs the string format of Firmware ID (0x07). Values
    /// 0x02-0x7F are reserved for future assignment by the specification;
    /// 0x80-0xFF are firmware vendor/OEM-specific.
    enum FirmwareIdFormat : uint8_t {
        FirmwareIdFormatFreeForm = 0x00,
        FirmwareIdFormatUefiGuid = 0x01
    };

    /// @brief Characteristics field (0x14), DSP0134 3.9.0 Table 136
    /// (7.46.3). Bits 2-15 are explicitly Reserved by the spec; only bits
    /// 0-1 carry a decoded meaning.
    enum Characteristics : uint16_t {
        Updatable = 0x0001,
        WriteProtect = 0x0002
    };

    /// @brief State field (0x16), DSP0134 3.9.0 Table 137 (7.46.4). These
    /// values correspond to Redfish Status.State property enumeration
    /// values (DSP2046). `StateOutOfSpec` (0) is not a spec value; this
    /// project's own fallback sentinel for "field absent or an unassigned
    /// byte value", matching this codebase's established `*OutOfSpec`
    /// precedent (e.g. MemoryChannelEntry::ChannelTypeOutOfSpec).
    enum State : uint8_t {
        StateOutOfSpec = 0x00,
        StateOther = 0x01,
        StateUnknown = 0x02,
        StateDisabled = 0x03,
        StateEnabled = 0x04,
        StateAbsent = 0x05,
        StateStandbyOffline = 0x06,
        StateStandbySpare = 0x07,
        StateUnavailableOffline = 0x08
    };

    /// @brief Sentinel Image Size value (0x0C, QWORD) meaning "unknown",
    /// per Table 133's own Description column.
    static constexpr uint64_t kImageSizeUnknown = 0xFFFFFFFFFFFFFFFFULL;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    FirmwareInventoryInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~FirmwareInventoryInformationEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the
    /// same order; each set Characteristics bit becomes one {label, true} field and
    /// each associated component handle is "Handle N" (N 1-based).
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x04 offset DMI string: Firmware Component Name.
    /// EXAMPLE: 'BMC Firmware'. Returns dmi_string(0) ("Not Specified") if
    /// get_entry_size() < 0x05.
    std::string get_component_name_string() const;

    /// @brief 0x05 offset DMI string: Firmware Version of this firmware,
    /// formatted per get_version_format(). Returns dmi_string(0) if
    /// get_entry_size() < 0x06.
    std::string get_version_string() const;

    /// @brief 0x07 offset DMI string: Firmware ID of this firmware,
    /// formatted per get_firmware_id_format(). Returns dmi_string(0) if
    /// get_entry_size() < 0x08.
    std::string get_firmware_id_string() const;

    /// @brief 0x09 offset DMI string: firmware release date. If supplied,
    /// follows the DSP0266 Date-Time values format (e.g.
    /// '2021-05-15T04:14:33+06:00'); this class does not parse that
    /// date-time format, it is returned verbatim as a string per this
    /// task's own explicit scope. Returns dmi_string(0) if
    /// get_entry_size() < 0x0A.
    std::string get_release_date_string() const;

    /// @brief 0x0A offset DMI string: manufacturer or producer of this
    /// firmware. Returns dmi_string(0) if get_entry_size() < 0x0B.
    std::string get_manufacturer_string() const;

    /// @brief 0x0B offset DMI string: lowest version to which this
    /// firmware can be rolled back, formatted per get_version_format().
    /// Returns dmi_string(0) if get_entry_size() < 0x0C.
    std::string get_lowest_supported_version_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Raw/enum values

    /// @brief 0x06 offset, raw byte enum (Table 134, 7.46.1). Returns
    /// VersionFormatFreeForm (0) if get_entry_size() < 0x07.
    uint8_t get_version_format() const;

    /// @brief String representation of get_version_format(), including the
    /// confirmed 0x00-0x03 values plus the two reserved ranges (0x04-0x7F
    /// future-DSP0134, 0x80-0xFF vendor/OEM-specific) rendered distinctly.
    std::string get_version_format_string() const;

    /// @brief 0x08 offset, raw byte enum (Table 135, 7.46.2). Returns
    /// FirmwareIdFormatFreeForm (0) if get_entry_size() < 0x09.
    uint8_t get_firmware_id_format() const;

    /// @brief String representation of get_firmware_id_format(), including
    /// the confirmed 0x00-0x01 values plus the two reserved ranges
    /// (0x02-0x7F future-DSP0134, 0x80-0xFF vendor/OEM-specific) rendered
    /// distinctly.
    std::string get_firmware_id_format_string() const;

    /// @brief 0x0C offset, raw QWORD: size in bytes of the firmware image
    /// currently programmed in the device. Returns kImageSizeUnknown if
    /// get_entry_size() < 0x14 (field absent) - the same sentinel the spec
    /// itself uses for "unknown", so callers never need a second check
    /// beyond is_image_size_known().
    uint64_t get_image_size() const;

    /// @brief True unless get_image_size() == kImageSizeUnknown (whether
    /// because the device reported the sentinel itself, or because the
    /// field is absent from a truncated/older structure).
    bool is_image_size_known() const;

    /// @brief 0x14 offset, raw WORD bit-field (Table 136, 7.46.3). Returns
    /// 0 if get_entry_size() < 0x16.
    uint16_t get_characteristics() const;

    /// @brief String representation of get_characteristics(), one decoded
    /// bit per line, via bitset_to_properties<uint16_t>().
    std::string get_characteristics_string() const;

    /// @brief True if the Updatable bit (bit 0) is set: this firmware can
    /// be updated by software.
    bool is_updatable() const;

    /// @brief True if the Write-Protect bit (bit 1) is set: this firmware
    /// is in a write-protected state.
    bool is_write_protected() const;

    /// @brief 0x16 offset, raw byte enum (Table 137, 7.46.4). Returns
    /// StateOutOfSpec (0) if get_entry_size() < 0x17.
    uint8_t get_state() const;

    /// @brief String representation of get_state().
    std::string get_state_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Associated Component Handles trailing array

    /// @brief 0x17 offset, raw byte: the EXPLICIT Number of Associated
    /// Components field (`n`). This is the only correct source for the
    /// number of trailing Associated Component Handles - see this header's
    /// own leading comment block for why it must never be derived from
    /// get_entry_size()/Length instead. Returns 0 if get_entry_size() <
    /// 0x18.
    uint8_t get_associated_component_count() const;

    /// @brief Associated Component Handle (WORD) at `0x18 + 2*index`,
    /// 0-based. Each handle refers to some other SMBIOS structure
    /// representing a device with a firmware component (e.g. Type 9, 17,
    /// 41, or 43). Double-guarded: `index` must be
    /// `< get_associated_component_count()` AND the read must
    /// independently fall within get_entry_size(), so a malformed
    /// structure whose explicit count field disagrees with its own Length
    /// can never be read out of bounds in either direction.
    /// @param index 0-based index into the trailing handle array.
    /// @return the raw handle at `index`, or 0 if either guard fails.
    uint16_t get_associated_component_handle_at(size_t index) const;

private:

    /// Map VersionFormat enum values (and reserved-range sentinels) to
    /// string representations.
    void init_string_values();

private:

    /// Version Format to string representation (confirmed values only;
    /// reserved ranges are handled separately in
    /// get_version_format_string()).
    std::map<uint8_t, std::string> version_format_map_;

    /// Firmware ID Format to string representation (confirmed values
    /// only; reserved ranges are handled separately in
    /// get_firmware_id_format_string()).
    std::map<uint8_t, std::string> firmware_id_format_map_;

    /// Characteristics bit value to string representation.
    std::map<uint16_t, std::string> characteristics_map_;

    /// State value to string representation.
    std::map<uint8_t, std::string> state_map_;
};

} // namespace smbios
