#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// TPM Device entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'TPM Device' chapter
// (7.44), DSP0134 3.9.0. Field offsets (Table 126) and the Characteristics
// bit-field (7.44.1, Table 127) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// VENDOR ID (0x04) - THE ONE RAW-ASCII FIELD IN THIS ENTIRE MILESTONE:
// unlike every other string-shaped field in this codebase's new structures,
// Vendor ID is *not* a DMI string-table index. Table 126's own Description
// column is explicit: "Specified as four ASCII characters, as defined by
// TCG Vendor ID ... For example: Vendor ID string of "ABC" = (41h 42h 43h
// 00h) Vendor ID string of "ABCD" = (41h 42h 43h 44h)". get_vendor_id_string()
// below therefore reads the 4 raw bytes directly via raw_data(), never
// dmi_string() - see that accessor's own doc comment for the explicit
// contrast.
//
// FIRMWARE VERSION 1/2 (0x0A/0x0E) semantics depend on Major Spec Version
// (0x08), per Table 126's own Description column:
//   - Major Spec Version 01h (TPM 1.2): Firmware Version 1 holds the
//     TPM_VERSION structure (TPM Main Specification, Part 2, Section 5.3);
//     Firmware Version 2 is always 00h.
//   - Major Spec Version 02h (TPM 2.0): Firmware Version 1 holds the most
//     significant 32 bits, and Firmware Version 2 the least significant 32
//     bits, of a single 64-bit vendor-specific firmware version value
//     (TPM_PT_FIRMWARE_VERSION_1/TPM_PT_FIRMWARE_VERSION_2). This class does
//     not attempt to further decode either field's vendor-specific/TPM-
//     structure-specific internal layout - both are exposed as raw 32-bit
//     values, consistent with this project's established scope cutoff for
//     vendor-maintained sub-formats (e.g. Processor Additional
//     Information's Processor-Specific Data, Story 07.0 Task 02).
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The TPM Device structure (7.44) - one instance per TPM device
/// present in the system: a raw 4-byte TCG Vendor ID (0x04, NOT a DMI
/// string index), Major/Minor Spec Version (0x08/0x09), a pair of Firmware
/// Version DWORDs whose meaning depends on Major Spec Version (0x0A/0x0E),
/// a Description DMI string (0x12), a QWORD Characteristics bit-field
/// (0x13, 7.44.1), and an OEM-defined DWORD (0x1B).
class TpmDeviceEntry : public AbstractSMBiosEntry {
public:

    /// @brief TPM Device Characteristics field (0x13), DSP0134 3.9.0 Table
    /// 127 (7.44.1). Bits 0:1 and 6:63 are explicitly Reserved by the spec
    /// (not merely undefined); only bits 2:5 carry a decoded meaning, so
    /// only those four values are present in the properties map
    /// bitset_to_properties<uint64_t>() consults.
    enum Characteristics : uint64_t {
        CharacteristicsNotSupported = 0x0000000000000004ULL,
        FamilyConfigurableViaFirmwareUpdate = 0x0000000000000008ULL,
        FamilyConfigurableViaPlatformSoftware = 0x0000000000000010ULL,
        FamilyConfigurableViaOemProprietaryMechanism = 0x0000000000000020ULL
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    TpmDeviceEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~TpmDeviceEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Raw values

    /// @brief 0x04 offset, 4 raw ASCII BYTEs - NOT a DMI string-table index.
    /// This is the one accessor in the whole codebase that reads ASCII text
    /// directly out of the formatted area's own raw bytes rather than
    /// through dmi_string(): Table 126's Vendor ID example ("ABC" stored as
    /// 41h 42h 43h 00h, "ABCD" stored as 41h 42h 43h 44h with no
    /// terminator) means this field is independent of this entry's DMI
    /// string table entirely. Trims exactly one trailing 0x00 byte if the
    /// 4th byte is 0x00 (the 3-character case); a genuine 4-character
    /// vendor ID with no null terminator is returned in full, un-trimmed.
    /// Returns "" if get_entry_size() < 0x08.
    std::string get_vendor_id_string() const;

    /// @brief 0x08 offset, raw byte. Major TPM version supported (e.g. 01h
    /// for TPM 1.2, 02h for TPM 2.0). Returns 0 if get_entry_size() < 0x09.
    uint8_t get_major_spec_version() const;

    /// @brief 0x09 offset, raw byte. Minor TPM version supported (e.g. 02h
    /// for TPM 1.2, 00h for TPM 2.0). Returns 0 if get_entry_size() < 0x0A.
    uint8_t get_minor_spec_version() const;

    /// @brief 0x0A offset, raw DWORD. For Major Spec Version 01h (TPM 1.2):
    /// the TPM_VERSION structure (TPM Main Specification, Part 2, Section
    /// 5.3). For Major Spec Version 02h (TPM 2.0): the most significant 32
    /// bits of a vendor-specific firmware version value
    /// (TPM_PT_FIRMWARE_VERSION_1). Returns 0 if get_entry_size() < 0x0E.
    uint32_t get_firmware_version_1() const;

    /// @brief 0x0E offset, raw DWORD. For Major Spec Version 01h (TPM 1.2):
    /// always 00h. For Major Spec Version 02h (TPM 2.0): the least
    /// significant 32 bits of a vendor-specific firmware version value
    /// (TPM_PT_FIRMWARE_VERSION_2). Returns 0 if get_entry_size() < 0x12.
    uint32_t get_firmware_version_2() const;

    /// @brief 0x13 offset, raw QWORD. The full, unmasked Characteristics
    /// bit-field (7.44.1, Table 127); see Characteristics for the four
    /// decoded bit values. Returns 0 if get_entry_size() < 0x1B.
    uint64_t get_characteristics() const;

    /// @brief 0x1B offset, raw DWORD. OEM- or firmware vendor-specific
    /// information. Returns 0 if get_entry_size() < 0x1F.
    uint32_t get_oem_defined() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x12 offset DMI string: descriptive information of the TPM
    /// device. Unlike get_vendor_id_string(), this one genuinely is a
    /// dmi_string()-backed STRING field. Returns dmi_string(0) ("Not
    /// Specified") if get_entry_size() < 0x13.
    std::string get_description_string() const;

    /// @brief String representation of get_characteristics(), one decoded
    /// bit per line, via bitset_to_properties<uint64_t>() - this entry is
    /// the first QWORD-width bitmask in the project; every prior call site
    /// is uint8_t.
    std::string get_characteristics_string() const;

private:

    /// Map Characteristics bit values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint64_t, std::string> characteristics_map_;
};

} // namespace smbios
