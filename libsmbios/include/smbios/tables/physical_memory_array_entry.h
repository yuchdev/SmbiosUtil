#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <smbios/abstract_smbios_entry.h>

// Physical Memory Array entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Physical Memory Array'
// chapter (7.17). Cross-checked against c-versions/dmidecode_linux/
// dmidecode.c's case 16 (dmi_decode(), ~line 4826, "Physical Memory Array")
// and its dmi_memory_array_location()/dmi_memory_array_use()/
// dmi_memory_array_ec_type()/dmi_memory_array_error_handle() helpers for
// field offsets, length guards, the capacity sentinel and enum value
// tables.
//
// Scope note on get_maximum_capacity()'s unit: per DMTF 7.17.4/7.17.7, the
// legacy Maximum Capacity field (0x07, 4 bytes) is in KILOBYTES, while the
// Extended Maximum Capacity field (0x0F, 8 bytes, 2.7+) it is superseded by
// is in BYTES - the reference's own case 16 confirms this with two
// different `shift` arguments to dmi_print_memory_size() (1 for the legacy
// branch, 0 for the extended one). get_maximum_capacity() returns whichever
// raw value is in effect without normalizing the unit (same "raw value, no
// semantic decode" precedent as ProcessorInformationEntry::
// get_processor_id()); get_maximum_capacity_string() documents which unit
// applies to the value it renders.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Physical Memory Array structure (7.17) - location, use,
/// memory error correction type, maximum capacity (with the `0x80000000`
/// sentinel meaning "see extended maximum capacity instead"), memory error
/// information handle, number of memory devices, and the 2.7+ 8-byte
/// extended maximum capacity field that supersedes the legacy 4-byte one
/// when it reads the sentinel.
class PhysicalMemoryArrayEntry : public AbstractSMBiosEntry {
public:

    // @brief Location field (offset 0x04), see dmi_memory_array_location()
    // (7.17.1 of the spec).
    enum Location : uint8_t {
        LocationOutOfSpec = 0x00,
        LocationOther = 0x01,
        LocationUnknown = 0x02,
        SystemBoardOrMotherboard = 0x03,
        ISAAddOnCard = 0x04,
        EISAAddOnCard = 0x05,
        PCIAddOnCard = 0x06,
        MCAAddOnCard = 0x07,
        PCMCIAAddOnCard = 0x08,
        ProprietaryAddOnCard = 0x09,
        NuBus = 0x0A,
        PC98C20AddOnCard = 0xA0,
        PC98C24AddOnCard = 0xA1,
        PC98EAddOnCard = 0xA2,
        PC98LocalBusAddOnCard = 0xA3,
        CXLAddOnCard = 0xA4
    };

    // @brief Use field (offset 0x05), see dmi_memory_array_use() (7.17.2 of
    // the spec).
    enum Use : uint8_t {
        UseOutOfSpec = 0x00,
        UseOther = 0x01,
        UseUnknown = 0x02,
        SystemMemory = 0x03,
        VideoMemory = 0x04,
        FlashMemory = 0x05,
        NonVolatileRAM = 0x06,
        CacheMemory = 0x07
    };

    // @brief Memory Error Correction field (offset 0x06), see
    // dmi_memory_array_ec_type() (7.17.3 of the spec).
    enum MemoryErrorCorrection : uint8_t {
        MemoryErrorCorrectionOutOfSpec = 0x00,
        MemoryErrorCorrectionOther = 0x01,
        MemoryErrorCorrectionUnknown = 0x02,
        None = 0x03,
        Parity = 0x04,
        SingleBitECC = 0x05,
        MultiBitECC = 0x06,
        CRC = 0x07
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in Physical Memory Array SMBIOS entry depending on version and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    PhysicalMemoryArrayEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~PhysicalMemoryArrayEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/qword values

    /// @brief 0x04 offset
    /// See Location enum for values
    uint8_t get_location() const;

    /// @brief 0x05 offset
    /// See Use enum for values
    uint8_t get_use() const;

    /// @brief 0x06 offset
    /// See MemoryErrorCorrection enum for values
    uint8_t get_memory_error_correction() const;

    /// @brief 0x07 offset, 4 bytes, widened (2.7+) to an 8-byte field at
    /// 0x0F when the DWORD at 0x07 reads the `0x80000000` sentinel (and
    /// length >= 0x17) - see dmi_decode()'s own `DWORD(data + 0x07) ==
    /// 0x80000000` check for case 16. Returns 0 when the sentinel is set
    /// but the extended field is not available (length < 0x17) - the
    /// capacity is genuinely unknown in that case, not "fall back to the
    /// sentinel value itself". See class doc comment for this field's
    /// unit, which differs between the legacy (kB) and extended (bytes)
    /// branches.
    uint64_t get_maximum_capacity() const;

    /// @brief 0x0B offset
    /// Handle of the memory error information structure, or 0xFFFE/0xFFFF
    /// for "not provided"/"no error" - see get_memory_error_information_
    /// handle_string()
    uint16_t get_memory_error_information_handle() const;

    /// @brief 0x0D offset
    /// Number of memory slots or sockets available in this array; 0 means
    /// absent/unknown
    uint16_t get_number_of_memory_devices() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Location string representation
    std::string get_location_string() const;

    /// @brief Use string representation
    std::string get_use_string() const;

    /// @brief Memory error correction string representation
    std::string get_memory_error_correction_string() const;

    /// @brief Maximum capacity string representation: "Unknown" when the
    /// sentinel is set but the extended field is unavailable, else the
    /// resolved value with its unit ("kB" for the legacy branch, "bytes"
    /// for the extended branch) - see class doc comment
    std::string get_maximum_capacity_string() const;

    /// @brief Memory error information handle string representation:
    /// "Not Provided" (0xFFFE) / "No Error" (0xFFFF), else a hex string -
    /// see dmi_memory_array_error_handle()
    std::string get_memory_error_information_handle_string() const;

    /// @brief Number of memory devices string representation ("Unknown"
    /// when 0)
    std::string get_number_of_memory_devices_string() const;

private:

    /// Map flags/codes data to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> location_map_;
    std::map<uint8_t, std::string> use_map_;
    std::map<uint8_t, std::string> memory_error_correction_map_;
};

} // namespace smbios
