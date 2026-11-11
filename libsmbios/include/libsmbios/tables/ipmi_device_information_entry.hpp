#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// IPMI Device Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'IPMI Device Information'
// chapter (7.39), DSP0134 3.9.0. Field offsets (Table 114) and the
// Interface Type field's enumerated values (7.39.1, Table 115) were
// confirmed directly against docs/standard/DSP0134_3.9.0.md.
//
// REDACTED-GAP RESOLUTION: this task's own source excerpt redacted the row
// immediately before Interrupt Number (the 0x10/0x11 boundary). Table 114's
// full text (DSP0134 3.9.0 p.135) confirms 0x10 is a single BYTE named
// "Base Address Modifier / Interrupt Info", a bit-field: bits 7:6 register
// spacing, bit 5 reserved, bit 4 LS-bit for addresses (unused/0 for SSIF),
// bit 3 interrupt-info-specified flag, bit 2 reserved, bit 1 interrupt
// polarity, bit 0 interrupt trigger mode - followed by 0x11 Interrupt
// Number (BYTE), confirming the task spec's own guess ("likely a 'Base
// Address Modifier' bit-field") was correct, with the added confirmation
// that the same byte also carries the Interrupt Info sub-fields.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The IPMI Device Information structure (7.39) - attributes of an
/// Intelligent Platform Management Interface (IPMI) Baseboard Management
/// Controller (BMC).
class IpmiDeviceInformationEntry : public AbstractSMBiosEntry {
public:

    /// @brief Interface Type field (0x04), DSP0134 3.9.0 Table 115 (7.39.1).
    /// `InterfaceTypeOutOfSpec` is this project's own fallback sentinel for
    /// a byte value this project has not seen documented, distinct from the
    /// spec's own `InterfaceTypeUnknown` (0x00) value.
    enum InterfaceType : uint8_t {
        InterfaceTypeUnknown = 0x00,
        InterfaceTypeKcs = 0x01,
        InterfaceTypeSmic = 0x02,
        InterfaceTypeBt = 0x03,
        InterfaceTypeSsif = 0x04,
        InterfaceTypeOutOfSpec = 0xFF
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    IpmiDeviceInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~IpmiDeviceInformationEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order.
    /// Specification Revision and I2C Target Address are uint64_t (plain
    /// decimals in the text); NV Storage Device Address, Base Address and
    /// Interrupt Number stay the printed decorated strings.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/qword values

    /// @brief 0x04 offset, raw byte. See InterfaceType for values. Returns
    /// InterfaceTypeOutOfSpec if get_entry_size() < 0x05 (note this differs
    /// from the spec's own InterfaceTypeUnknown = 0x00, matching this
    /// codebase's established "distinct absent-field sentinel" convention).
    InterfaceType get_interface_type() const;

    /// @brief String representation of get_interface_type().
    std::string get_interface_type_string() const;

    /// @brief 0x05 offset, raw BCD byte. IPMI specification revision to
    /// which the BMC was designed; bits 7:4 the most significant decimal
    /// digit, bits 3:0 the least significant (e.g. 0x10 = revision 1.0).
    /// Returns 0 if get_entry_size() < 0x06.
    uint8_t get_specification_revision_bcd() const;

    /// @brief get_specification_revision_bcd() converted via
    /// bcd_to_decimal() and expressed as `major * 10 + minor` (e.g. 0x10 ->
    /// 10, meaning revision "1.0"); this is a purely mechanical nibble
    /// decode with no range validation, matching
    /// SystemPowerControlsEntry::bcd_to_decimal()'s own documented
    /// convention (7.26.1 idiom, reused here for 7.39's BCD field).
    uint8_t get_specification_revision() const;

    /// @brief 0x06 offset, raw byte. Target address on the I2C bus of this
    /// BMC. Returns 0 if get_entry_size() < 0x07.
    uint8_t get_i2c_target_address() const;

    /// @brief 0x07 offset, raw byte. Bus ID of the NV storage device; 0xFF
    /// means no storage device exists for this BMC (see
    /// has_nv_storage_device()). Returns 0xFF if get_entry_size() < 0x08.
    uint8_t get_nv_storage_device_address() const;

    /// @brief True unless get_nv_storage_device_address() is the 0xFF
    /// sentinel (no NV storage device for this BMC), per Table 114.
    bool has_nv_storage_device() const;

    /// @brief 0x08 offset, raw QWORD. Base address (either memory-mapped or
    /// I/O) of the BMC; see is_base_address_io_space() for bit 0's meaning.
    /// Returns 0 if get_entry_size() < 0x10.
    uint64_t get_base_address() const;

    /// @brief True if get_base_address()'s bit 0 is set (address is in I/O
    /// space); false if clear (address is memory-mapped), per Table 114.
    bool is_base_address_io_space() const;

    /// @brief 0x10 offset, raw byte: the confirmed "Base Address Modifier /
    /// Interrupt Info" field (see this header's own REDACTED-GAP RESOLUTION
    /// note). Returns 0 if get_entry_size() < 0x11.
    uint8_t get_base_address_modifier_and_interrupt_info() const;

    /// @brief Register spacing sub-field, bits 7:6 of
    /// get_base_address_modifier_and_interrupt_info(): 0 = successive byte
    /// boundaries, 1 = 32-bit boundaries, 2 = 16-byte boundaries, 3 =
    /// reserved. This field is unused (set to 0) for SSIF, per Table 114.
    uint8_t get_register_spacing() const;

    /// @brief LS-bit-for-addresses sub-field, bit 4 of
    /// get_base_address_modifier_and_interrupt_info(): if set, address bit
    /// 0 is 1; if clear, address bit 0 is 0.
    bool get_address_bit0() const;

    /// @brief True if bit 3 (Interrupt Info specified) of
    /// get_base_address_modifier_and_interrupt_info() is set - i.e.
    /// get_interrupt_polarity()/get_interrupt_trigger_mode() are meaningful.
    bool has_interrupt_info() const;

    /// @brief Interrupt polarity sub-field, bit 1 of
    /// get_base_address_modifier_and_interrupt_info(): true = active high,
    /// false = active low. Only meaningful if has_interrupt_info().
    bool get_interrupt_polarity_active_high() const;

    /// @brief Interrupt trigger mode sub-field, bit 0 of
    /// get_base_address_modifier_and_interrupt_info(): true = level
    /// triggered, false = edge triggered. Only meaningful if
    /// has_interrupt_info().
    bool get_interrupt_trigger_mode_level() const;

    /// @brief 0x11 offset, raw byte. Interrupt number for the IPMI System
    /// Interface; 0x00 means unspecified/unsupported (see has_interrupt()).
    /// Returns 0 if get_entry_size() < 0x12.
    uint8_t get_interrupt_number() const;

    /// @brief True unless get_interrupt_number() is the 0x00 sentinel
    /// (unspecified/unsupported), per Table 114.
    bool has_interrupt() const;

private:

    /// Map InterfaceType values to string values
    void init_string_values();

    /// @brief Mechanical BCD-nibble-pair-to-decimal conversion (same idiom
    /// as SystemPowerControlsEntry::bcd_to_decimal(), 7.26.1): `(b >> 4) *
    /// 10 + (b & 0x0F)`. Does not validate that either nibble is a valid
    /// BCD digit.
    static uint8_t bcd_to_decimal(uint8_t bcd);

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> interface_type_map_;
};

} // namespace smbios
