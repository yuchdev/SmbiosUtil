#pragma once
#include <cstdint>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// Out-of-Band Remote Access entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Out-of-Band Remote Access'
// chapter (7.31), DSP0134 3.9.0.
//
// Connections bit layout (DSP0134 3.9.0 Table 103, confirmed directly
// against docs/standard/DSP0134_3.9.0.md's own 7.31, since this task's own
// source excerpt had this row redacted by the markdown-extraction tool):
//   Bits 7:2  Reserved for future definition by this specification; set to
//             all zeros.
//   Bit  1    Outbound Connection Enabled - whether (1) or not (0) the
//             facility is allowed to initiate outbound connections to
//             contact an alert management facility when critical
//             conditions occur.
//   Bit  0    Inbound Connection Enabled - whether (1) or not (0) the
//             facility is allowed to receive incoming connections for the
//             purpose of remote operations or problem management.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Out-of-Band Remote Access structure (7.31) - the
/// manufacturer of a hardware facility usable to gain remote access to the
/// system when the operating system is unavailable (power-down, hardware
/// failure, or boot failure), plus the current inbound/outbound connection
/// policy for that facility.
class OutOfBandRemoteAccessEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    OutOfBandRemoteAccessEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~OutOfBandRemoteAccessEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x04 offset DMI string. Manufacturer of the out-of-band
    /// access facility. Returns an empty string if get_entry_size() < 0x05.
    std::string get_manufacturer_name_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x05 offset, raw byte. See the class doc comment for the
    /// confirmed Table 103 bit layout. Returns 0 if get_entry_size() < 0x06.
    uint8_t get_connections() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Connections sub-fields (Table 103)

    /// @brief Bit 0 of Connections: whether the facility is allowed to
    /// receive incoming connections for remote operations or problem
    /// management. Returns false if get_entry_size() < 0x06.
    bool is_inbound_connection_enabled() const;

    /// @brief Bit 1 of Connections: whether the facility is allowed to
    /// initiate outbound connections to contact an alert management
    /// facility when critical conditions occur. Returns false if
    /// get_entry_size() < 0x06.
    bool is_outbound_connection_enabled() const;
};

} // namespace smbios
