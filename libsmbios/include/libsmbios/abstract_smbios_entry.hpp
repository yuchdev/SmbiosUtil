#pragma once
#include <cstdint>
#include <map>
#include <sstream>
#include <type_traits>
#include <vector>
#include <string>
#include <memory>
#include <libsmbios/smbios_entry_interface.hpp>
#include <libsmbios/field_value.hpp>

namespace smbios {

struct DMIHeader;

/// @brief Basic functionality implementation for any SMBIOS entry
/// Working with DMI strings, offsets, convert bitwise properties to description etc
/// Class instance does not "own" this memory, it just provide more convenient interface
/// and useful types and enumerations
class AbstractSMBiosEntry : public SMBiosInterface {
public:

    /// @brief Accept a copy of own header
    /// @param header copy of the entry's own DMI header
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table; required so string-section parsing never reads past
    /// the buffer, even for a truncated/corrupted structure. Pass nullptr
    /// only when header.data is itself nullptr (no backing buffer at all).
    AbstractSMBiosEntry(const DMIHeader& header, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~AbstractSMBiosEntry() = default;

    /// @brief Entry size without string section
    size_t get_entry_size() const override;

    /// @brief Structured, renderer-neutral view of the same data render_to_description() prints.
    /// Every concrete entry must implement it (pure virtual, milestone 0009 R1).
    /// Names are unique within the returned list (repeated groups carry a
    /// 1-based index suffix); order is stable and matches the text output.
    /// @return the entry's decoded fields, built fresh on every call. Values
    /// are copies, so the result does not borrow from the table, but the
    /// entry itself must not outlive its `SMBios`. Not `noexcept`: may throw
    /// `std::bad_alloc`.
    virtual std::vector<NamedField> get_fields() const = 0;

protected:

    /// Implementation of SMBIOS string extractor
    /// Note: First string index is 1, 0 is "Not Specified"
    std::string dmi_string(size_t string_index) const;

    /// Print segment-based offset
    std::string address_string(uint16_t string_index) const;

    /// Default implementation of SMBIOS bitwise properties to string representation
    template <typename T>
    std::string bitset_to_properties(T properties, const std::map<T, std::string>& properties_map) const
    {
        static_assert(std::is_integral<T>(), "Bitwise type should be integer");

        std::stringstream properties_stream;
        for (T current_property = 0x1; current_property; current_property <<= 1) {
            auto it = properties_map.find(current_property);
            if ((properties & current_property) && (it != properties_map.end())) {
                properties_stream << '\t' << (*it).second << '\n';
            }
        }
        return properties_stream.str();
    }

    /// @brief One NamedField{label, true} per set bit that has a label in properties_map.
    /// Walks bits in the same ascending order as bitset_to_properties(), so
    /// field order matches the text output. Set bits without a label are skipped.
    /// @tparam T integral bitmask type (enforced by `static_assert`); must
    /// match the key type of `properties_map` exactly.
    /// @param properties bitmask value to decode
    /// @param properties_map single-bit mask -> label
    /// @param[in,out] out fields are appended here (existing contents are
    /// kept); never cleared. Nothing is returned (`void`).
    template <typename T>
    void bitset_to_fields(T properties, const std::map<T, std::string>& properties_map,
                          std::vector<NamedField>& out) const
    {
        static_assert(std::is_integral<T>(), "Bitwise type should be integer");

        for (T current_property = 0x1; current_property; current_property <<= 1) {
            auto it = properties_map.find(current_property);
            if ((properties & current_property) && (it != properties_map.end())) {
                out.push_back(NamedField{it->second, true});
            }
        }
    }

    /// @brief Raw pointer to the entry's own formatted-area bytes (header.data)
    /// Used by concrete entry classes together with the little-endian
    /// read_u8/read_u16/read_u32/read_u64 helpers (smbios/sys_interface/byte_reader.hpp) and a
    /// get_entry_size()-based length guard, instead of reinterpret_cast onto a
    /// #pragma pack struct. Non-owning: lifetime tied to the owning SMBios buffer.
    const uint8_t* raw_data() const;

private:

    /// Extract all strings from the string section
    /// which is plain char array separated with \0 symbols
    /// End of section is \0\0 sequence, which it followed by the next SMBIOS entry
    /// Bounds-checked against table_end: an unterminated/truncated string
    /// section stops parsing cleanly at the buffer boundary instead of
    /// reading past it.
    void parse_dmi_strings(const uint8_t* table_end);

private:
    
    /// copy of entry header
    std::unique_ptr<DMIHeader> header_;

    /// Extracted DMI strings
    std::vector<std::string> dmi_strings_;
};

} // namespace smbios