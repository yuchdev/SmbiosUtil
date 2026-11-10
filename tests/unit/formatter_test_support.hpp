#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/field_value.hpp>
#include <libsmbios/output_formatter.hpp>

#include "structured_output_validators.hpp"
#include "synthetic_table_builder.hpp"

// Shared test-support for milestone 0009 Story 02.0's formatter tests
// (formatter_escaping_test.cpp, text/xml/json_formatter_test.cpp).
// Header-only and test-only: never included by libsmbios library sources,
// and adds no dependency. The JSON/XML well-formedness validators
// (is_well_formed_json/is_well_formed_xml/is_valid_utf8) live in
// structured_output_validators.hpp/.cpp (Task 03); this header includes it
// so every formatter test gets them.

namespace smbios {
namespace test {

/// @brief U+FFFD REPLACEMENT CHARACTER as UTF-8, the escapers' substitute
/// for each maximal invalid subpart.
inline const std::string kReplacement = "\xEF\xBF\xBD";

/// @brief Test double whose get_fields() returns exactly the fields it was
/// given, so a formatter test can inject values no real table can produce
/// (UINT64_MAX, an embedded NUL, a 255-byte vector, hostile names). Has no
/// backing buffer (DMIHeader::data is nullptr), so it parses no strings.
/// Always wrap string literals in std::string(...) when building its
/// fields (LOW-5: a pre-P1957 library could bind a `const char*` to bool).
class FieldsTestEntry : public AbstractSMBiosEntry {
public:
    /// @param type the label get_type() returns.
    /// @param fields the exact fields get_fields() returns.
    FieldsTestEntry(std::string type, std::vector<NamedField> fields)
        : AbstractSMBiosEntry(DMIHeader{}, nullptr), type_(std::move(type)), fields_(std::move(fields)) {}

    std::string get_type() const override { return type_; }
    std::string render_to_description() const override { return "Header type: " + type_ + '\n'; }
    std::vector<NamedField> get_fields() const override { return fields_; }

private:
    std::string type_;
    std::vector<NamedField> fields_;
};

/// @brief Header for a buffer produced by SyntheticTableBuilder holding one
/// structure at offset 0.
inline DMIHeader make_header(const std::vector<uint8_t>& buffer)
{
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();
    return header;
}

/// @brief One real entry built through SMBiosEntryFactory from a
/// one-structure synthetic table, owning the buffer the entry views. Not
/// copyable or movable, so the entry can never outlive or lose its buffer.
class SyntheticEntry {
public:
    /// @param type SMBIOS structure type.
    /// @param formatted_area formatted-area bytes (header excluded).
    /// @param strings DMI strings, index 1 first; must not contain NUL.
    /// @param version SMBIOS version the entry is decoded against.
    SyntheticEntry(uint8_t type, std::vector<uint8_t> formatted_area, std::vector<std::string> strings,
                   SMBiosVersion version = SMBiosVersion{3, 9})
        : buffer_(SyntheticTableBuilder().add_structure(type, std::move(formatted_area), std::move(strings)).build()),
          entry_(SMBiosEntryFactory().create(make_header(buffer_), version, buffer_.data() + buffer_.size()))
    {}

    SyntheticEntry(const SyntheticEntry&) = delete;
    SyntheticEntry& operator=(const SyntheticEntry&) = delete;

    /// @return the decoded entry, or nullptr if the type is unregistered.
    const AbstractSMBiosEntry* get() const { return entry_.get(); }

    /// @return the decoded entry; the caller must have checked get().
    const AbstractSMBiosEntry& operator*() const { return *entry_; }

private:
    std::vector<uint8_t> buffer_;
    std::unique_ptr<AbstractSMBiosEntry> entry_;
};

/// @brief A synthetic BIOS Information (type 0) structure, header length
/// 0x18, whose Vendor (string 1) is `vendor`; Version is "1.0" and Release
/// Date "01/01/2026". No property bits are set.
inline std::unique_ptr<SyntheticEntry> make_bios_entry(const std::string& vendor)
{
    std::vector<uint8_t> formatted_area{
        0x01,                                           // 0x04 vendor_index
        0x02,                                           // 0x05 version_index
        0x00, 0xF0,                                     // 0x06 starting_segment
        0x03,                                           // 0x08 release_date_index
        0x0F,                                           // 0x09 rom_size
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0A properties
        0x00,                                           // 0x12 properties_extension1
        0x00,                                           // 0x13 properties_extension2
        0x02, 0x05,                                     // 0x14 BIOS release 2.5
        0x01, 0x07,                                     // 0x16 firmware release 1.7
    };
    return std::make_unique<SyntheticEntry>(static_cast<uint8_t>(SMBios::BIOSInformation),
                                            std::move(formatted_area),
                                            std::vector<std::string>{vendor, "1.0", "01/01/2026"});
}

/// @brief The hostile invalid-UTF-8 vendor strings of Task 03 case 1, each
/// paired with its exact sanitize_utf8() result (one U+FFFD per maximal
/// invalid subpart).
inline std::vector<std::pair<std::string, std::string>> invalid_utf8_cases()
{
    const std::string& r = kReplacement;
    return {
        {"\x80", r},                                 // lone continuation byte
        {"\xC3", r},                                 // truncated 2-byte sequence
        {"\xC0\xAF", r + r},                         // overlong '/': C0 and AF never valid
        {"\xED\xA0\x80", r + r + r},                 // CESU-8 surrogate U+D800
        {"\xF4\x90\x80\x80", r + r + r + r},         // U+110000, above U+10FFFF
        {"Caf\xE9", "Caf" + r},                      // Latin-1, truncated at end
    };
}

/// @brief Renders `entries` exactly as the CLI frames a document:
/// begin_document(), then each render() with entry_separator() between
/// consecutive entries, then end_document().
inline std::string render_document(const IOutputFormatter& formatter,
                                   const std::vector<const AbstractSMBiosEntry*>& entries)
{
    std::string out = formatter.begin_document();
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i != 0) {
            out += formatter.entry_separator();
        }
        out += formatter.render(*entries[i]);
    }
    out += formatter.end_document();
    return out;
}

/// @brief True iff `s` contains any raw byte in [low, high].
inline bool contains_byte_in_range(const std::string& s, unsigned char low, unsigned char high)
{
    for (const char c : s) {
        const auto b = static_cast<unsigned char>(c);
        if (b >= low && b <= high) return true;
    }
    return false;
}

/// @brief Every byte 0x01-0x1F followed by 0x7F (Task 03 case 2's input).
inline std::string all_c0_controls_and_del()
{
    std::string s;
    for (int b = 0x01; b <= 0x1F; ++b) {
        s.push_back(static_cast<char>(b));
    }
    s.push_back('\x7F');
    return s;
}

/// @brief The valid-UTF-8 hostile values of Task 03 cases 2-5, each paired
/// with a unique field name: every C0 control plus DEL, an ANSI SGR
/// sequence, bidi overrides/isolates plus U+2028/U+2029, and the XML
/// non-characters U+FFFE/U+FFFF. None contains NUL, so each can also be a
/// DMI string.
inline std::vector<std::pair<std::string, std::string>> hostile_valid_utf8_values()
{
    return {
        {"Controls", all_c0_controls_and_del()},
        {"ANSI", "\x1b[31mRED\x1b[0m"},
        {"Bidi and separators", "\xE2\x80\xAE" "\xE2\x81\xA6" "\xE2\x81\xA9" "\xE2\x80\xA8" "\xE2\x80\xA9"},
        {"Noncharacters", "\xEF\xBF\xBE" "\xEF\xBF\xBF"},
    };
}

/// @brief Task 03 case 9(c)'s multi-entry document content, owning every
/// entry it hands out: one real synthetic representative of each Story 01.0
/// batch (BIOS Information, Processor Information, System Slots,
/// Temperature Probe) whose DMI strings are hostile, a real InactiveEntry,
/// one BIOS Information entry per hostile vendor of cases 1-5, and two test
/// doubles - one carrying the case 2-5 values plus the case 6/8 boundary
/// values, one with no fields. Not copyable or movable (the entries view
/// buffers it owns).
class HostileDocumentCorpus {
public:
    /// @throws std::runtime_error if a synthetic type fails to resolve
    /// through the entry factory (its registration was not linked).
    HostileDocumentCorpus()
    {
        const std::vector<uint8_t> batch_types{
            static_cast<uint8_t>(SMBios::BIOSInformation),      // Story 01.0 batch 1
            static_cast<uint8_t>(SMBios::ProcessorInformation), // batch 2
            static_cast<uint8_t>(SMBios::SystemSlots),          // batch 3
            static_cast<uint8_t>(SMBios::TemperatureProbe),     // batch 4
        };
        // 0x02 fill: every string index selects string 2 (the ANSI value).
        for (const uint8_t type : batch_types) {
            synthetic_.push_back(std::make_unique<SyntheticEntry>(
                type, std::vector<uint8_t>(0x60, 0x02),
                std::vector<std::string>{"Caf\xE9", "\x1b[31mRED\x1b[0m"}));
        }
        synthetic_.push_back(std::make_unique<SyntheticEntry>(static_cast<uint8_t>(SMBios::Inactive),
                                                              std::vector<uint8_t>{}, std::vector<std::string>{}));
        for (const auto& c : invalid_utf8_cases()) {
            synthetic_.push_back(make_bios_entry(c.first));
        }
        std::vector<NamedField> hostile_fields;
        for (const auto& v : hostile_valid_utf8_values()) {
            synthetic_.push_back(make_bios_entry(v.second));
            hostile_fields.push_back(NamedField{v.first, std::string(v.second)});
        }
        std::vector<uint8_t> all_bytes;
        for (int b = 0x00; b <= 0xFE; ++b) {
            all_bytes.push_back(static_cast<uint8_t>(b));
        }
        hostile_fields.push_back(NamedField{"Max", std::numeric_limits<uint64_t>::max()});
        hostile_fields.push_back(NamedField{"2^53+1", uint64_t{9007199254740993ull}});
        hostile_fields.push_back(NamedField{"Bytes", all_bytes});
        hostile_fields.push_back(NamedField{"No Bytes", std::vector<uint8_t>{}});
        hostile_fields.push_back(NamedField{"Flag", true});
        doubles_.push_back(std::make_unique<FieldsTestEntry>("Hostile Values", std::move(hostile_fields)));
        doubles_.push_back(std::make_unique<FieldsTestEntry>("No Fields", std::vector<NamedField>{}));

        for (const auto& s : synthetic_) {
            if (s->get() == nullptr) {
                throw std::runtime_error("HostileDocumentCorpus: synthetic entry type not registered");
            }
            entries_.push_back(s->get());
        }
        for (const auto& d : doubles_) {
            entries_.push_back(d.get());
        }
    }

    HostileDocumentCorpus(const HostileDocumentCorpus&) = delete;
    HostileDocumentCorpus& operator=(const HostileDocumentCorpus&) = delete;

    /// @return every entry, in document order; valid while `*this` lives.
    const std::vector<const AbstractSMBiosEntry*>& entries() const { return entries_; }

private:
    std::vector<std::unique_ptr<SyntheticEntry>> synthetic_;
    std::vector<std::unique_ptr<FieldsTestEntry>> doubles_;
    std::vector<const AbstractSMBiosEntry*> entries_;
};

/// @brief Counts non-overlapping occurrences of `needle` in `haystack`.
inline size_t count_occurrences(const std::string& haystack, const std::string& needle)
{
    size_t count = 0;
    for (size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

} // namespace test
} // namespace smbios
