#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Baseboard (or Module) Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Baseboard (or Module)
// Information' chapter (7.3), DSP0134 3.9.0. Field offsets, the Table 14
// feature-flags bit layout and footnote 1, and the Table 15 Board Type
// enumeration were all confirmed directly against docs/standard/
// DSP0134_3.9.0.md's own section 7.3/7.3.1/7.3.2 (not assumed from any
// task-document reproduction), and cross-checked against
// c-versions/dmidecode_linux/dmidecode.c's case 2 ("Base Board Information")
// and its dmi_base_board_type() helper.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Baseboard (or Module) Information structure (7.3) -
/// manufacturer/product/version/serial number/asset tag strings, a feature
/// flags bitmask (7.3.1), the location-in-chassis string, the chassis
/// handle, the board type enum (7.3.2), and a trailing array of Contained
/// Object Handles whose count is given by the Number of Contained Object
/// Handles field - the same "count byte, then N fixed-width items right
/// after it" shape SystemEnclosureEntry's contained-elements array already
/// established (milestone 0004, Task 01), here with WORD-sized items
/// instead of opaque byte records.
///
/// Scope note on get_feature_flags_string(): per 7.3.1's Table 14 footnote
/// 1, the feature flags are only meaningful ("present") when
/// `get_entry_size() >= 0x0E` AND `get_contained_object_handle_count() !=
/// 0` - this is a distinct condition from whether the raw byte at offset
/// 0x09 is even readable (which only needs `get_entry_size() >= 0x0A`, see
/// get_feature_flags()). get_feature_flags_string() renders "Not Present"
/// for the footnote-1 "not meaningful" case, "None" when the byte is
/// meaningful but all bits are clear, and the bit list otherwise - mirroring
/// SystemSlotsEntry::get_characteristics_string()'s own "Unknown" vs "None"
/// distinction for an absent-vs-empty bitmask, but keyed off the footnote's
/// own condition rather than a dedicated "unknown" bit.
class BaseboardInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief Feature Flags field (offset 0x09), see 7.3.1's Table 14. Bits
    // 5-7 are reserved by the spec (set to 0) and are not given names here.
    enum FeatureFlags : uint8_t {
        HostingBoard = 0x01,
        RequiresDaughterBoard = 0x02,
        Removable = 0x04,
        Replaceable = 0x08,
        HotSwappable = 0x10
    };

    // @brief Board Type field (offset 0x0D), see 7.3.2's Table 15.
    // BoardTypeOutOfSpec (0x00) is not a spec value; it is this project's
    // own fallback sentinel for "field absent or value not in Table 15",
    // matching SystemEnclosureEntry::ChassisType's own OutOfSpec precedent.
    enum BoardType : uint8_t {
        BoardTypeOutOfSpec = 0x00,
        Unknown = 0x01,
        Other = 0x02,
        ServerBlade = 0x03,
        ConnectivitySwitch = 0x04,
        SystemManagementModule = 0x05,
        ProcessorModule = 0x06,
        IOModule = 0x07,
        MemoryModule = 0x08,
        DaughterBoard = 0x09,
        Motherboard = 0x0A,
        ProcessorMemoryModule = 0x0B,
        ProcessorIOModule = 0x0C,
        InterconnectBoard = 0x0D
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in Baseboard Information SMBIOS entry depending on header.length.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    BaseboardInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~BaseboardInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset
    /// Index of string number of the manufacturer
    uint8_t get_manufacturer_index() const;

    /// @brief 0x05 offset
    /// Index of string number of the product name
    uint8_t get_product_index() const;

    /// @brief 0x06 offset
    /// Index of string number of the version
    uint8_t get_version_index() const;

    /// @brief 0x07 offset
    /// Index of string number of the serial number
    uint8_t get_serial_number_index() const;

    /// @brief 0x08 offset
    /// Index of string number of the asset tag
    uint8_t get_asset_tag_index() const;

    /// @brief 0x09 offset
    /// Raw Feature Flags bitmask; see FeatureFlags enum. Only the raw byte
    /// read is guarded here (get_entry_size() >= 0x0A) - whether the value
    /// is actually meaningful per footnote 1 is a separate question, see
    /// get_feature_flags_string()'s own doc comment and the class doc
    /// comment's scope note.
    uint8_t get_feature_flags() const;

    /// @brief 0x0A offset
    /// Index of string number describing this board's location within its
    /// chassis
    uint8_t get_location_in_chassis_index() const;

    /// @brief 0x0B offset, 2 bytes
    /// Handle of the chassis structure (7.4) this board resides in
    uint16_t get_chassis_handle() const;

    /// @brief 0x0D offset
    /// See BoardType enum for values
    uint8_t get_board_type() const;

    /// @brief 0x0E offset
    /// Number (0-255) of Contained Object Handles that follow at 0x0F; 0 if
    /// get_entry_size() < 0x0F
    uint8_t get_contained_object_handle_count() const;

    /// @brief WORD at `0x0F + index * 2`, 0-based; double-guarded against
    /// both `index < get_contained_object_handle_count()` and
    /// `get_entry_size()`, same two-guard pattern SystemEnclosureEntry's
    /// contained-elements accessor already established.
    /// @param index 0-based index into the Contained Object Handles array.
    /// @return the handle at `index`, or 0 if `index` is out of range for
    /// either guard.
    uint16_t get_contained_object_handle_at(size_t index) const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Manufacturer DMI string
    std::string get_manufacturer_string() const;

    /// @brief Product name DMI string
    std::string get_product_string() const;

    /// @brief Version DMI string
    std::string get_version_string() const;

    /// @brief Serial number DMI string
    std::string get_serial_number_string() const;

    /// @brief Asset tag DMI string
    std::string get_asset_tag_string() const;

    /// @brief Feature flags string representation; see the class doc
    /// comment's scope note and this class's own FeatureFlags enum. Renders
    /// "Not Present" when footnote 1's condition is not met (distinct from
    /// "None", which means the condition is met but every bit is clear).
    std::string get_feature_flags_string() const;

    /// @brief Location-in-chassis DMI string
    std::string get_location_in_chassis_string() const;

    /// @brief Board type string representation
    std::string get_board_type_string() const;

private:

    /// Map flags/codes data to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> feature_flags_map_;
    std::map<uint8_t, std::string> board_type_map_;
};

} // namespace smbios
