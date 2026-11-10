#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Processor Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Processor Information'
// chapter (7.5). Cross-checked against c-versions/dmidecode_linux/
// dmidecode.c's case 4 (dmi_decode(), ~line 4574, "Processor Information")
// and its dmi_processor_type()/dmi_processor_family()/
// dmi_processor_voltage()/dmi_processor_status()/dmi_processor_upgrade()/
// dmi_processor_characteristics() helpers for field offsets, length guards,
// sentinel/bitfield encodings and enum value tables.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Processor Information structure (7.5) - socket designation,
/// processor type/family/manufacturer, processor ID, version string,
/// voltage, external/max/current clock speed, status, upgrade, and the
/// 2.3+/2.5+/2.6+/3.0+ additions: L1/L2/L3 cache handle, serial
/// number/asset tag/part number string indices, core count/enabled/thread
/// count, characteristics bitmask, the widened 2-byte core/thread-count
/// fields (3.0+) that supersede the original 1-byte ones when the latter
/// read 0xFF, and the widened 2-byte processor family field (2.6+) that
/// supersedes the 1-byte one when it reads 0xFE.
///
/// Scope note on processor_id (offset 0x08, 8 raw bytes): exposed as a raw
/// uint64_t accessor (little-endian, via read_u64 like every other
/// multi-byte field in this project) plus a hex-string accessor. The
/// reference's own dmi_processor_id() does architecture-specific decode
/// (x86 CPUID feature bits, ARM MIDR fields, etc.) that is out of
/// proportion to this class and is intentionally not reproduced here.
///
/// Scope note on get_processor_family_string(): the full DMTF 7.5.2
/// family-code-to-name table (dmi_processor_family()'s family2[] array) is
/// reproduced, keyed directly by the numeric code (no enum - the table has
/// ~230 entries, many of them not valid C++ identifiers, e.g. "80286",
/// "68xxx", "C7-M"). The two manufacturer-string- and SMBIOS-version-
/// dependent ambiguous-value special cases dmidecode.c itself special-cases
/// (code 0x30 under SMBIOS 2.0 meaning "Pentium Pro" for an Intel
/// manufacturer string; code 0xBE meaning "Core 2" vs "K7" depending on the
/// manufacturer string) are NOT reproduced - resolving them needs a second,
/// unrelated field (the manufacturer DMI string) and is a presentation
/// nuance on top of the family code itself, not the sentinel-widening
/// correctness concern this class's own task names. Both fall back to the
/// family2[] table's own entries for those codes (0x30 = "Alpha", 0xBE =
/// unmapped -> "OutOfSpec") instead. get_processor_family() itself (the
/// numeric code, after the 0xFE sentinel check) is unaffected by this scope
/// note and is always correct.
///
/// Scope note on get_processor_upgrade_string(): same rationale as family -
/// the DMTF 7.5.5 upgrade table (dmi_processor_upgrade()'s upgrade[] array)
/// has 80 entries; reproduced keyed by numeric code rather than as 80 enum
/// identifiers.
class ProcessorInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief Processor Type field (offset 0x05), see dmi_processor_type()
    // (7.5.1 of the spec).
    enum ProcessorType : uint8_t {
        ProcessorTypeOutOfSpec = 0x00,
        ProcessorTypeOther = 0x01,
        ProcessorTypeUnknown = 0x02,
        CentralProcessor = 0x03,
        MathProcessor = 0x04,
        DSPProcessor = 0x05,
        VideoProcessor = 0x06
    };

    // @brief Processor Status field (offset 0x18, bits 0-2), see
    // dmi_processor_status() (only meaningful when bit 6, "populated", is
    // set - see get_populated()).
    enum ProcessorStatusCode : uint8_t {
        StatusUnknown = 0x00,
        StatusEnabled = 0x01,
        StatusDisabledByUser = 0x02,
        StatusDisabledByBIOS = 0x03,
        StatusIdle = 0x04,
        StatusOutOfSpec5 = 0x05,
        StatusOutOfSpec6 = 0x06,
        StatusOther = 0x07
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in Processor Information SMBIOS entry depending on version and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ProcessorInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ProcessorInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order. Decorated
    /// values (speeds in MHz, hex cache handles, "Unknown" sentinels, the
    /// Processor ID hex string - get_processor_id() returns a uint64_t, not
    /// raw bytes) stay the printed string. Characteristics is a single
    /// {"Characteristics", "None"} field exactly when the text prints
    /// "None", otherwise one {label, true} field per set bit.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset
    /// Index of string number of the processor socket designation
    uint8_t get_socket_designation_index() const;

    /// @brief 0x05 offset
    /// See ProcessorType enum for values
    uint8_t get_processor_type() const;

    /// @brief 0x06 offset, widened (2.6+) to a uint16_t at 0x28 when the
    /// byte at 0x06 reads 0xFE (and length >= 0x2A) - see dmi_processor_
    /// family()'s own `data[0x06] == 0xFE` sentinel check
    uint16_t get_processor_family() const;

    /// @brief 0x07 offset
    /// Index of string number of the processor manufacturer
    uint8_t get_manufacturer_index() const;

    /// @brief 0x08 offset, 8 raw bytes
    /// Raw processor ID (CPUID-style signature on x86; architecture-
    /// specific decode is out of scope here, see class doc comment
    uint64_t get_processor_id() const;

    /// @brief 0x10 offset
    /// Index of string number of the processor version
    uint8_t get_version_index() const;

    /// @brief 0x11 offset
    /// Raw voltage byte; see get_voltage_string() for the decoded value -
    /// bit 7 set means an explicit voltage in bits 0-6 (/10 V), bit 7 clear
    /// means bits 0-2 are a bitmask of legacy supported voltages
    uint8_t get_voltage() const;

    /// @brief 0x12 offset
    /// External Clock, in MHz; 0 means unknown
    uint16_t get_external_clock() const;

    /// @brief 0x14 offset
    /// Max Speed, in MHz; 0 means unknown
    uint16_t get_max_speed() const;

    /// @brief 0x16 offset
    /// Current Speed, in MHz; 0 means unknown
    uint16_t get_current_speed() const;

    /// @brief 0x18 offset, bit 6 (0x40)
    /// Whether the socket is populated
    bool get_populated() const;

    /// @brief 0x18 offset, bits 0-2 - only meaningful when get_populated()
    /// is true. See ProcessorStatusCode enum for values
    uint8_t get_status_code() const;

    /// @brief 0x19 offset
    /// Processor Upgrade raw code; see get_processor_upgrade_string() for
    /// the decoded value (DMTF 7.5.5 table, see class doc comment)
    uint8_t get_processor_upgrade() const;

    /// @brief 0x1A offset (2.3+)
    /// Handle of the L1 Cache Information structure, or 0xFFFF if none
    uint16_t get_l1_cache_handle() const;

    /// @brief 0x1C offset (2.3+)
    /// Handle of the L2 Cache Information structure, or 0xFFFF if none
    uint16_t get_l2_cache_handle() const;

    /// @brief 0x1E offset (2.3+)
    /// Handle of the L3 Cache Information structure, or 0xFFFF if none
    uint16_t get_l3_cache_handle() const;

    /// @brief 0x20 offset (2.3+)
    /// Index of string number of the processor serial number
    uint8_t get_serial_number_index() const;

    /// @brief 0x21 offset (2.3+)
    /// Index of string number of the processor asset tag
    uint8_t get_asset_tag_index() const;

    /// @brief 0x22 offset (2.3+)
    /// Index of string number of the processor part number
    uint8_t get_part_number_index() const;

    /// @brief 0x23 offset (2.5+), widened (3.0+) to a uint16_t at 0x2A when
    /// the byte at 0x23 reads 0xFF (and length >= 0x2C). 0 means unknown.
    uint16_t get_core_count() const;

    /// @brief 0x24 offset (2.5+), widened (3.0+) to a uint16_t at 0x2C when
    /// the byte at 0x24 reads 0xFF (and length >= 0x2E). 0 means unknown.
    uint16_t get_core_enabled() const;

    /// @brief 0x25 offset (2.5+), widened (3.0+) to a uint16_t at 0x2E when
    /// the byte at 0x25 reads 0xFF (and length >= 0x30). 0 means unknown.
    uint16_t get_thread_count() const;

    /// @brief 0x26 offset (2.5+)
    /// Processor Characteristics bitmask, see ProcessorCharacteristics enum
    enum ProcessorCharacteristics : uint16_t {
        ProcessorCharacteristicsUnknown = 0x1u << 1,
        Capable64Bit = 0x1u << 2,
        MultiCore = 0x1u << 3,
        HardwareThread = 0x1u << 4,
        ExecuteProtection = 0x1u << 5,
        EnhancedVirtualization = 0x1u << 6,
        PowerPerformanceControl = 0x1u << 7,
        Capable128Bit = 0x1u << 8,
        Arm64SoCID = 0x1u << 9
    };
    uint16_t get_characteristics() const;

    /// @brief 0x30 offset (3.0+)
    /// Thread Enabled count; 0 means unknown
    uint16_t get_thread_enabled() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Socket designation DMI string
    std::string get_socket_designation_string() const;

    /// @brief Processor type string representation
    std::string get_processor_type_string() const;

    /// @brief Processor family string representation (see class doc comment
    /// for this method's scope note)
    std::string get_processor_family_string() const;

    /// @brief Processor manufacturer DMI string
    std::string get_manufacturer_string() const;

    /// @brief Processor ID, as a fixed-width hex string
    std::string get_processor_id_string() const;

    /// @brief Version DMI string
    std::string get_version_string() const;

    /// @brief Voltage, decoded per dmi_processor_voltage(): bit 7 set ->
    /// "<n.n> V"; bit 7 clear -> space-separated list of legacy supported
    /// voltages (any of "5.0 V"/"3.3 V"/"2.9 V"), or "Unknown" if none
    std::string get_voltage_string() const;

    /// @brief External Clock string representation ("Unknown" when 0)
    std::string get_external_clock_string() const;

    /// @brief Max Speed string representation ("Unknown" when 0)
    std::string get_max_speed_string() const;

    /// @brief Current Speed string representation ("Unknown" when 0)
    std::string get_current_speed_string() const;

    /// @brief Status string representation: "Populated, <status>" or
    /// "Unpopulated" (status code is not rendered when unpopulated)
    std::string get_status_string() const;

    /// @brief Processor Upgrade string representation (see class doc
    /// comment for this method's scope note)
    std::string get_processor_upgrade_string() const;

    /// @brief L1 Cache Handle string representation: "Not Provided" (3.x)
    /// or "No L1 Cache" (older) when the handle is 0xFFFF, else a hex string
    std::string get_l1_cache_handle_string() const;

    /// @brief L2 Cache Handle string representation, see
    /// get_l1_cache_handle_string()
    std::string get_l2_cache_handle_string() const;

    /// @brief L3 Cache Handle string representation, see
    /// get_l1_cache_handle_string()
    std::string get_l3_cache_handle_string() const;

    /// @brief Serial number DMI string
    std::string get_serial_number_string() const;

    /// @brief Asset tag DMI string
    std::string get_asset_tag_string() const;

    /// @brief Part number DMI string
    std::string get_part_number_string() const;

    /// @brief Core count string representation ("Unknown" when 0)
    std::string get_core_count_string() const;

    /// @brief Core enabled string representation ("Unknown" when 0)
    std::string get_core_enabled_string() const;

    /// @brief Thread count string representation ("Unknown" when 0)
    std::string get_thread_count_string() const;

    /// @brief Return a \n\t-separated list of processor characteristics
    std::string get_characteristics_string() const;

    /// @brief Thread enabled string representation ("Unknown" when 0)
    std::string get_thread_enabled_string() const;

private:

    /// True when the text renders Characteristics as "None" (bits 2-7 all
    /// clear, see get_characteristics_string()); shared with get_fields()
    bool characteristics_are_none() const;

    /// Cache-handle string helper shared by L1/L2/L3
    std::string cache_handle_string(uint16_t handle) const;

    /// Map flags/codes data to string values
    void init_string_values();

private:

    /// Precomputed once from the constructor's SMBiosVersion argument
    /// (`version > SMBiosVersion{2, 2}`), needed only to pick between the
    /// "Not Provided" (2.3+) and "No Cache" (pre-2.3) renderings of an
    /// absent (0xFFFF) cache handle - see dmi_processor_cache()'s own
    /// `ver >= 0x0203` check and cache_handle_string()'s implementation.
    /// Stored as a bool rather than the full SMBiosVersion so this header
    /// only needs to forward-declare SMBiosVersion, matching every other
    /// entry class's header in this project.
    bool cache_handle_version_ge_2_3_;

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> processor_type_map_;
    std::map<uint16_t, std::string> processor_family_map_;
    std::map<uint8_t, std::string> processor_status_map_;
    std::map<uint8_t, std::string> processor_upgrade_map_;
    std::map<uint16_t, std::string> characteristics_map_;
};

} // namespace smbios
