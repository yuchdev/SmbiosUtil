#pragma once
#include <cstdint>
#include <map>
#include <smbios/abstract_smbios_entry.h>

// Cache Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Cache Information' chapter
// (7.8). Cross-checked against c-versions/dmidecode_linux/dmidecode.c's
// case 7 (dmi_decode(), ~line 4676, "Cache Information") and its
// dmi_cache_mode()/dmi_cache_location()/dmi_cache_size()/dmi_cache_size_2()/
// dmi_cache_types()/dmi_cache_ec_type()/dmi_cache_type()/
// dmi_cache_associativity() helpers for field offsets, length guards,
// bitfield encodings and enum value tables.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Cache Information structure (7.8) - socket designation,
/// cache configuration bitfield (enabled/socketed/level/operational
/// mode/location), max/installed size, supported/current SRAM type
/// bitmasks, cache speed, error correction type, system cache type,
/// associativity, and the 3.1+ widened maximum/installed size fields that
/// supersede the original ones when `get_entry_size()` reaches their
/// respective length thresholds (0x17 / 0x1B) - there is no 0xFF sentinel
/// byte for this structure the way Processor Information has; the legacy
/// vs. widened field is chosen purely by header.length, matching how
/// dmidecode.c's own case 7 picks between dmi_cache_size() and
/// dmi_cache_size_2() by `h->length >= 0x17`/`0x1B` alone.
///
/// Scope note on the size fields' internal encoding: both the legacy
/// uint16_t (0x07/0x09) and widened uint32_t (0x13/0x17) size fields carry
/// their own granularity flag in their top bit (bit 15 / bit 31: 1 means
/// the remaining bits count in 64 KB units, 0 means they count in 1 KB
/// units - see dmi_cache_size_2()). Both get_max_cache_size()/
/// get_installed_cache_size() decode this to a plain KB count (uint64_t,
/// wide enough for the 64 KB-granularity widened field's maximum: 2^31 - 1
/// units * 64 KB can exceed 32 bits), matching how
/// BiosInformationEntry::get_rom_size()/get_runtime_size() already decode
/// their own encoded fields into a plain unit instead of leaving the raw
/// encoding to the caller.
class CacheInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief Cache Configuration field (offset 0x05), bits 8-9: Operational
    // Mode, see dmi_cache_mode().
    enum CacheOperationalMode : uint8_t {
        WriteThrough = 0x00,
        WriteBack = 0x01,
        VariesWithMemoryAddress = 0x02,
        OperationalModeUnknown = 0x03
    };

    // @brief Cache Configuration field (offset 0x05), bits 5-6: Location,
    // see dmi_cache_location().
    enum CacheLocation : uint8_t {
        Internal = 0x00,
        External = 0x01,
        LocationOutOfSpec = 0x02,
        LocationUnknown = 0x03
    };

    // @brief Error Correction Type field (offset 0x10), see
    // dmi_cache_ec_type() (7.8.3 of the spec).
    enum ErrorCorrectionType : uint8_t {
        ErrorCorrectionOutOfSpec = 0x00,
        ErrorCorrectionOther = 0x01,
        ErrorCorrectionUnknown = 0x02,
        NoErrorCorrection = 0x03,
        Parity = 0x04,
        SingleBitECC = 0x05,
        MultiBitECC = 0x06
    };

    // @brief System Cache Type field (offset 0x11), see dmi_cache_type()
    // (7.8.4 of the spec).
    enum SystemCacheType : uint8_t {
        SystemCacheTypeOutOfSpec = 0x00,
        SystemCacheTypeOther = 0x01,
        SystemCacheTypeUnknown = 0x02,
        Instruction = 0x03,
        Data = 0x04,
        Unified = 0x05
    };

    // @brief Associativity field (offset 0x12), see
    // dmi_cache_associativity() (7.8.5 of the spec).
    enum Associativity : uint8_t {
        AssociativityOutOfSpec = 0x00,
        AssociativityOther = 0x01,
        AssociativityUnknown = 0x02,
        DirectMapped = 0x03,
        TwoWaySetAssociative = 0x04,
        FourWaySetAssociative = 0x05,
        FullyAssociative = 0x06,
        EightWaySetAssociative = 0x07,
        SixteenWaySetAssociative = 0x08,
        TwelveWaySetAssociative = 0x09,
        TwentyFourWaySetAssociative = 0x0A,
        ThirtyTwoWaySetAssociative = 0x0B,
        FortyEightWaySetAssociative = 0x0C,
        SixtyFourWaySetAssociative = 0x0D,
        TwentyWaySetAssociative = 0x0E
    };

    // @brief Supported SRAM Type / Current SRAM Type bitmask (offsets
    // 0x0B/0x0D), see dmi_cache_types() (7.8.2 of the spec). Both fields
    // share this same bit layout.
    enum SRAMType : uint16_t {
        SRAMTypeOther = 0x1u << 0,
        SRAMTypeUnknown = 0x1u << 1,
        NonBurst = 0x1u << 2,
        Burst = 0x1u << 3,
        PipelineBurst = 0x1u << 4,
        Synchronous = 0x1u << 5,
        Asynchronous = 0x1u << 6
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in Cache Information SMBIOS entry depending on version and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    CacheInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~CacheInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset
    /// Index of string number of the cache socket designation
    uint8_t get_socket_designation_index() const;

    /// @brief 0x05 offset, bit 7 (0x0080)
    /// Whether the cache is currently enabled
    bool get_enabled() const;

    /// @brief 0x05 offset, bit 3 (0x0008)
    /// Whether the cache is socketed
    bool get_socketed() const;

    /// @brief 0x05 offset, bits 0-2 (0x0007) + 1
    /// Cache level, 1-8
    uint8_t get_cache_level() const;

    /// @brief 0x05 offset, bits 8-9
    /// See CacheOperationalMode enum for values
    uint8_t get_operational_mode() const;

    /// @brief 0x05 offset, bits 5-6
    /// See CacheLocation enum for values
    uint8_t get_location() const;

    /// @brief Decoded maximum cache size, in KB: the widened uint32_t field
    /// at 0x13 when get_entry_size() >= 0x17, else the legacy uint16_t
    /// field at 0x07 - see class doc comment for the granularity decode
    uint64_t get_max_cache_size() const;

    /// @brief Decoded installed cache size, in KB: the widened uint32_t
    /// field at 0x17 when get_entry_size() >= 0x1B, else the legacy
    /// uint16_t field at 0x09 - see class doc comment for the granularity
    /// decode
    uint64_t get_installed_cache_size() const;

    /// @brief 0x0B offset
    /// Supported SRAM Type bitmask, see SRAMType enum
    uint16_t get_supported_sram_type() const;

    /// @brief 0x0D offset
    /// Current/Installed SRAM Type bitmask, see SRAMType enum
    uint16_t get_installed_sram_type() const;

    /// @brief 0x0F offset
    /// Cache speed, in nanoseconds; 0 means unknown
    uint8_t get_cache_speed() const;

    /// @brief 0x10 offset
    /// See ErrorCorrectionType enum for values
    uint8_t get_error_correction_type() const;

    /// @brief 0x11 offset
    /// See SystemCacheType enum for values
    uint8_t get_system_cache_type() const;

    /// @brief 0x12 offset
    /// See Associativity enum for values
    uint8_t get_associativity() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Socket designation DMI string
    std::string get_socket_designation_string() const;

    /// @brief Enabled string representation ("Enabled"/"Disabled")
    std::string get_enabled_string() const;

    /// @brief Socketed string representation ("Socketed"/"Not Socketed")
    std::string get_socketed_string() const;

    /// @brief Operational mode string representation
    std::string get_operational_mode_string() const;

    /// @brief Location string representation
    std::string get_location_string() const;

    /// @brief Decoded maximum cache size string representation (kB/MB)
    std::string get_max_cache_size_string() const;

    /// @brief Decoded installed cache size string representation (kB/MB)
    std::string get_installed_cache_size_string() const;

    /// @brief Return a \n\t-separated list of supported SRAM types
    /// ("\tNone\n" when the bitmask is empty)
    std::string get_supported_sram_type_string() const;

    /// @brief Return a \n\t-separated list of installed SRAM types
    /// ("\tNone\n" when the bitmask is empty)
    std::string get_installed_sram_type_string() const;

    /// @brief Cache speed string representation ("Unknown" when 0)
    std::string get_cache_speed_string() const;

    /// @brief Error correction type string representation
    std::string get_error_correction_type_string() const;

    /// @brief System cache type string representation
    std::string get_system_cache_type_string() const;

    /// @brief Associativity string representation
    std::string get_associativity_string() const;

private:

    /// Decode a KB-granularity-encoded uint32_t (bit 31 = 64KB granularity
    /// flag, bits 0-30 = count) into a plain KB count
    static uint64_t decode_cache_size_kb(uint32_t encoded);

    /// Decode the legacy KB-granularity-encoded uint16_t (bit 15 =
    /// granularity flag, bits 0-14 = count) into a plain KB count, by
    /// widening it to the uint32_t encoding first (see dmi_cache_size())
    static uint64_t decode_legacy_cache_size_kb(uint16_t encoded);

    /// Format a decoded KB count as a human string (kB/MB)
    static std::string cache_size_string(uint64_t size_kb);

    /// Map flags/codes data to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> operational_mode_map_;
    std::map<uint8_t, std::string> location_map_;
    std::map<uint16_t, std::string> sram_type_map_;
    std::map<uint8_t, std::string> error_correction_type_map_;
    std::map<uint8_t, std::string> system_cache_type_map_;
    std::map<uint8_t, std::string> associativity_map_;
};

} // namespace smbios
