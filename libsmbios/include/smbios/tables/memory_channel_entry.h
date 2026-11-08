#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <smbios/abstract_smbios_entry.h>

// Memory Channel entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Memory Channel' chapter
// (7.38), DSP0134 3.9.0. Field offsets (Table 112), the Memory Device Count
// field at 0x06, and the Channel Type field's enumerated values (7.38.1,
// Table 113) were confirmed directly against docs/standard/DSP0134_3.9.0.md.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Memory Channel structure (7.38) - the correlation between a
/// memory channel and the one or more Memory Device (Type 11h) structures
/// associated with it, each contributing some "load" toward the channel's
/// own maximum.
///
/// Structurally this is the same "count then N fixed-width trailing tuples"
/// shape as GroupAssociationsEntry's (Item Type, Item Handle) array (7.15,
/// Story 02.0), but the count is derived in the OPPOSITE direction:
/// GroupAssociationsEntry has no count field at all and *must* derive the
/// item count from `Length` (`(Length - 5) / 3`, per 7.15's own note that no
/// further fields can ever be appended without breaking that derivation).
/// Memory Channel is the reverse: Table 112's own Length row is explicit
/// that "this field must not be used to determine the number of memory
/// devices specified within the structure" - specifically so a future
/// specification revision can append fields after the Load/Handle list
/// without breaking existing parsers that only look at the explicit Memory
/// Device Count field (0x06). get_device_count() therefore reads that
/// field directly and never derives anything from get_entry_size(). Do not
/// copy GroupAssociationsEntry's Length-derived counting pattern here - the
/// two structures use the same trailing-array shape but opposite count
/// sources.
class MemoryChannelEntry : public AbstractSMBiosEntry {
public:

    /// @brief Channel Type field (0x04), DSP0134 3.9.0 Table 113 (7.38.1).
    /// `ChannelTypeOutOfSpec` (value 0) is not a spec value; this project's
    /// own fallback sentinel for "field absent or an unassigned byte value",
    /// matching this codebase's established `*OutOfSpec` precedent.
    enum ChannelType : uint8_t {
        ChannelTypeOutOfSpec = 0x00,
        ChannelTypeOther = 0x01,
        ChannelTypeUnknown = 0x02,
        ChannelTypeRambus = 0x03,
        ChannelTypeSyncLink = 0x04
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    MemoryChannelEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~MemoryChannelEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset, raw byte. See ChannelType for values. Returns
    /// ChannelTypeOutOfSpec if get_entry_size() < 0x05.
    ChannelType get_channel_type() const;

    /// @brief String representation of get_channel_type().
    std::string get_channel_type_string() const;

    /// @brief 0x05 offset, raw byte. Maximum load supported by the channel;
    /// the sum of all device loads cannot exceed this value. Returns 0 if
    /// get_entry_size() < 0x06.
    uint8_t get_maximum_channel_load() const;

    /// @brief 0x06 offset, raw byte: the EXPLICIT Memory Device Count field
    /// (`n`). This is the only correct source for the number of trailing
    /// Load/Handle pairs - see this class's own doc comment for why it must
    /// never be derived from get_entry_size()/Length instead. Returns 0 if
    /// get_entry_size() < 0x07.
    uint8_t get_device_count() const;

    /// @brief Memory Device Load (BYTE) at `0x07 + 3*index`, 0-based.
    /// Double-guarded: `index` must be `< get_device_count()` AND the read
    /// must independently fall within get_entry_size(), so a malformed
    /// structure whose explicit count field disagrees with its own Length
    /// can never be read out of bounds in either direction.
    /// @param index 0-based index into the trailing device array.
    /// @return the raw Load byte at `index`, or 0 if either guard fails.
    uint8_t get_device_load_at(size_t index) const;

    /// @brief Memory Device Handle (WORD) at `0x08 + 3*index`, 0-based; the
    /// handle of the Memory Device (Type 11h) structure associated with this
    /// channel. Same double-guard discipline as get_device_load_at().
    /// @param index 0-based index into the trailing device array.
    /// @return the raw Handle word at `index`, or 0 if either guard fails.
    uint16_t get_device_handle_at(size_t index) const;

private:

    /// Map ChannelType values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> channel_type_map_;
};

} // namespace smbios
