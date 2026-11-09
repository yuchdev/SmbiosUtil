#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// Group Associations entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Group Associations' chapter
// (7.15), DSP0134 3.9.0. Field offsets (0x04 Group Name, 0x05 + 3*i Item
// Type, 0x06 + 3*i Item Handle) and the item-count formula were confirmed
// directly against docs/standard/DSP0134_3.9.0.md's own section 7.15
// (Table 59) and its own note and worked Example 1/Example 2, not assumed
// from any task-document reproduction. No discrepancy was found this time:
// the task document's own reproduction of 7.15 matches the spec text.
//
// Per 7.15's own note: "Because this structure was originally defined with
// the Length implicitly defining the number of items present, no further
// fields can be added to this structure without adversely affecting
// existing software's ability to properly parse the data." There is no
// count *field* anywhere in this structure - the number of trailing
// (Item Type, Item Handle) pairs is purely `(Length - 5) / 3`, per Table
// 59's own "Length" row ("Computed by the firmware as 5 + (3 bytes for
// each item in the group)... the user of this structure determines the
// number of items as (Length - 5) / 3").

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Group Associations structure (7.15) - a Group Name string
/// followed by a trailing array of (Item Type, Item Handle) pairs, 3 bytes
/// per item, with no count field of its own: the number of items is purely
/// derived from the structure's own `Length` byte as `(Length - 5) / 3`
/// (see 7.15's own note and Table 59). Each item's Item Type is itself an
/// `SMBios::SMBiosHandler` value naming the *other* structure type being
/// referenced (e.g. `SMBios::ProcessorInformation` for a CPU, per 7.15's own
/// worked Example 1), and Item Handle is that other structure's handle.
///
/// Bounds-safety note: because item count is derived rather than stored,
/// `get_item_type_at()`/`get_item_handle_at()` are each independently
/// guarded against `get_entry_size()` directly (not merely against
/// `get_item_count()`) - a malformed structure whose `Length` does not
/// divide evenly by 3 after subtracting 5 must still never be read past
/// its own buffer.
class GroupAssociationsEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header; Group Associations has no version-gated
    /// fields, every field is distinguished by header.length alone (R2).
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    GroupAssociationsEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~GroupAssociationsEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset
    /// Index of string number describing this group; 0 if
    /// get_entry_size() < 0x05
    uint8_t get_group_name_index() const;

    /// @brief Number of trailing (Item Type, Item Handle) pairs, derived -
    /// never read as a structure field - as `(get_entry_size() - 5) / 3`
    /// per 7.15's own formula (Table 59). Returns 0 if
    /// `get_entry_size() < 5`. Integer division truncates if `Length` does
    /// not divide evenly by 3 after subtracting 5 (a malformed structure);
    /// see get_item_type_at()/get_item_handle_at()'s own doc comments for
    /// why that truncation alone is not sufficient to guarantee in-bounds
    /// reads.
    size_t get_item_count() const;

    /// @brief Item Type (BYTE) at `0x05 + 3*index`, 0-based; an
    /// `SMBios::SMBiosHandler` value naming the structure type the item at
    /// `index` refers to. Independently guarded against `get_entry_size()`
    /// (not just against `get_item_count()`) so a malformed `Length` can
    /// never cause an out-of-bounds read.
    /// @param index 0-based index into the trailing item array.
    /// @return the raw Item Type byte at `index`, or 0 if the read would
    /// fall at or beyond `get_entry_size()`.
    uint8_t get_item_type_at(size_t index) const;

    /// @brief Item Handle (WORD) at `0x06 + 3*index`, 0-based; the handle of
    /// the other structure named by `get_item_type_at(index)`.
    /// Independently guarded against `get_entry_size()` (not just against
    /// `get_item_count()`) so a malformed `Length` can never cause an
    /// out-of-bounds read.
    /// @param index 0-based index into the trailing item array.
    /// @return the raw Item Handle word at `index`, or 0 if the read would
    /// fall at or beyond `get_entry_size()`.
    uint16_t get_item_handle_at(size_t index) const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Group Name DMI string
    std::string get_group_name_string() const;

    /// @brief Human-readable name for get_item_type_at(index), resolved
    /// against `SMBios::SMBiosHandler` where the value happens to be one of
    /// this project's known/registered structure types; falls back to the
    /// raw numeric value (e.g. "Unknown (42)") otherwise. No new enum is
    /// introduced for this - Item Type values are already
    /// `SMBios::SMBiosHandler` values by definition (7.15).
    /// @param index 0-based index into the trailing item array; same
    /// bounds guard as get_item_type_at().
    std::string get_item_type_string_at(size_t index) const;

private:

    /// Populate item_type_names_ with this project's known SMBiosHandler
    /// values and their display names.
    void init_string_values();

private:

    /// SMBios::SMBiosHandler value -> human-readable name, for rendering
    /// get_item_type_string_at(); only the subset of SMBiosHandler this
    /// project happens to know a name for is populated here.
    std::map<uint8_t, std::string> item_type_names_;
};

} // namespace smbios
