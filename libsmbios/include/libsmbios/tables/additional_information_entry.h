#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.h>

// Additional Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Additional Information'
// chapter (7.41), DSP0134 3.9.0. Field offsets (Table 118) and the
// Additional Information Entry format (7.41.1, Table 119) were confirmed
// directly against docs/standard/DSP0134_3.9.0.md.
//
// SCOPE CUTOFF (R8, see this story's own README "Key constraints"): each
// Additional Information Entry's Value field (0x05.. of that entry) is
// "the same type and size as the field being referenced by this Additional
// Information Entry" per Table 119's own description - its width is only
// known from `Entry Length - 5`, not from any type information, since
// decoding the field actually being referenced would require generic
// cross-structure reflection this codebase has no mechanism for. This class
// therefore returns the Value field as raw bytes only
// (get_value_bytes_at()) and does not attempt to interpret them as any
// specific C++ integer width - `Entry Length - 5` already *is* the
// complete, correct answer at this structure's own level of information.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Additional Information structure (7.41) - a variable-length
/// array of Additional Information Entries (7.41.1), each itself
/// variable-length, used to carry unspecified/interim enumerated values or
/// field updates for another structure's field.
///
/// Unlike Group Associations' or Memory Channel's fixed-stride trailing
/// arrays, neither the number of entries nor any individual entry's size is
/// fixed: the explicit Number of Additional Information entries field
/// (0x04) gives the count `n` directly (not derived from Length, since
/// there is no fixed per-entry stride to divide by), and each entry's own
/// Entry Length byte (its own offset 0x00) determines where the next entry
/// begins.
///
/// Caching design: because every accessor below needs the same "walk from
/// entry 0, summing each preceding entry's own Entry Length" computation to
/// find entry `i`'s start offset, this class performs that walk exactly
/// once, in the constructor, caching each entry's start offset in
/// entry_offsets_ (see that member's own comment). Every accessor then
/// indexes directly into this cache rather than re-walking the buffer on
/// every call. The walk is bounds-guarded against get_entry_size()
/// independently at each step, so a malformed structure whose declared `n`
/// or any individual entry's Entry Length would walk past the structure's
/// actual buffer simply yields a shorter entry_offsets_ than `n` claims -
/// every accessor's own index bound check against entry_offsets_.size()
/// (not the raw declared `n`) then keeps every read safely in bounds.
class AdditionalInformationEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    AdditionalInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~AdditionalInformationEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Entry count / per-entry accessors

    /// @brief 0x04 offset, raw byte: Number of Additional Information
    /// entries (`n`), read explicitly (not derived from Length, since each
    /// entry is itself variable-length). Note this is the structure's own
    /// declared count, which may legitimately exceed the number of entries
    /// actually recoverable from a malformed/truncated buffer - callers
    /// that need the safe, walk-confirmed count should use
    /// get_entry_offsets_count() instead. Returns 0 if get_entry_size() <
    /// 0x05.
    uint8_t get_entry_count() const;

    /// @brief Number of entries actually reachable by the constructor's own
    /// bounds-guarded offset-caching walk (entry_offsets_.size()) - always
    /// <= get_entry_count(), and strictly less than it if the declared `n`
    /// or any individual entry's own Entry Length would have walked past
    /// get_entry_size(). Every get_*_at(index) accessor below treats this
    /// value, not get_entry_count(), as the valid index range.
    size_t get_entry_offsets_count() const;

    /// @brief Entry Length (+0x00 of entry `index`), a minimum of 6 per
    /// spec. Returns 0 if `index >= get_entry_offsets_count()`.
    uint8_t get_entry_length_at(size_t index) const;

    /// @brief Referenced Handle (+0x01 of entry `index`, WORD): handle, or
    /// instance number, of the structure for which additional information
    /// is provided. Returns 0 if `index >= get_entry_offsets_count()`.
    uint16_t get_referenced_handle_at(size_t index) const;

    /// @brief Referenced Offset (+0x03 of entry `index`, BYTE): offset of
    /// the field within the structure referenced by Referenced Handle for
    /// which additional information is provided. Returns 0 if
    /// `index >= get_entry_offsets_count()`.
    uint8_t get_referenced_offset_at(size_t index) const;

    /// @brief String (+0x04 of entry `index`, BYTE, STRING index): the
    /// optional string number associated with the field referenced by
    /// Referenced Offset. Returns 0 if `index >= get_entry_offsets_count()`.
    uint8_t get_string_index_at(size_t index) const;

    /// @brief Resolved DMI string for get_string_index_at(index). Returns
    /// dmi_string(0) ("Not Specified") if `index >=
    /// get_entry_offsets_count()`.
    std::string get_string_at(size_t index) const;

    /// @brief Value (+0x05.. of entry `index`, raw bytes, width = `Entry
    /// Length - 5`): the enumerated value or updated field content this
    /// entry carries, returned as raw bytes only - see this header's own
    /// leading comment block for the documented scope cutoff (R8): this
    /// class never attempts to interpret these bytes as a specific C++
    /// integer width, since doing so correctly would require decoding the
    /// referenced structure's own field, which this codebase has no generic
    /// mechanism for. Returns an empty vector if `index >=
    /// get_entry_offsets_count()`.
    std::vector<uint8_t> get_value_bytes_at(size_t index) const;

private:

    /// @brief Populate entry_offsets_ with one forward pass over the
    /// Additional Information entries array (starting at 0x05), each step
    /// advancing by that entry's own Entry Length (+0x00 of that entry)
    /// rather than a fixed stride. Every byte read during the walk (each
    /// entry's own Entry Length byte, and the bound implied by that Entry
    /// Length) is independently guarded against get_entry_size(); the walk
    /// stops cleanly, without recording a further offset, the moment either
    /// the declared count `n` is exhausted or the next entry's header/body
    /// would read past get_entry_size() - so a malformed structure simply
    /// yields fewer cached offsets than `n` claims, never an out-of-bounds
    /// read.
    void cache_entry_offsets();

private:

    /// Cached start offset (relative to the structure's own formatted-area
    /// start, i.e. directly usable with raw_data()) of each Additional
    /// Information Entry successfully walked by cache_entry_offsets(). See
    /// this class's own header doc comment for the rationale: every
    /// accessor needs the same "sum of preceding entries' own Entry Length"
    /// walk, so it is performed exactly once, here, rather than per-call.
    /// entry_offsets_.size() may be less than get_entry_count() if the
    /// buffer is malformed/truncated - see get_entry_offsets_count().
    std::vector<size_t> entry_offsets_;
};

} // namespace smbios
