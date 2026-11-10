#pragma once

#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/field_value.hpp>
#include <libsmbios/output_formatter.hpp>

#include "synthetic_table_builder.hpp"

// Shared test-support for milestone 0009 Story 02.0 Task 02's formatter
// tests (formatter_escaping_test.cpp, text/xml/json_formatter_test.cpp).
// Header-only and test-only: never included by libsmbios production
// sources. Adds no dependency - the structural checks here are deliberately
// small; the full RFC 8259 / XML 1.0 validators belong to Task 03.

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

/// @brief Strict RFC 3629 UTF-8 check, written independently of
/// libsmbios's sanitize_utf8() so the two cross-check each other.
inline bool is_valid_utf8(const std::string& s)
{
    size_t i = 0;
    while (i < s.size()) {
        const auto b0 = static_cast<unsigned char>(s[i]);
        size_t length = 0;
        uint32_t cp = 0;
        if (b0 < 0x80) { ++i; continue; }
        else if ((b0 & 0xE0) == 0xC0) { length = 2; cp = b0 & 0x1F; }
        else if ((b0 & 0xF0) == 0xE0) { length = 3; cp = b0 & 0x0F; }
        else if ((b0 & 0xF8) == 0xF0) { length = 4; cp = b0 & 0x07; }
        else { return false; }
        if (i + length > s.size()) return false;
        for (size_t k = 1; k < length; ++k) {
            const auto b = static_cast<unsigned char>(s[i + k]);
            if ((b & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (b & 0x3F);
        }
        const uint32_t min_for_length[] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < min_for_length[length]) return false;      // overlong
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;     // surrogate
        if (cp > 0x10FFFF) return false;                    // out of range
        i += length;
    }
    return true;
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

/// @brief Minimal structural XML check for this writer's output subset:
/// valid UTF-8; no XML 1.0-forbidden C0 byte; an optional leading
/// `<?xml ...?>` declaration; then exactly one root element with balanced,
/// correctly nested tags; attribute values double-quoted; every `&` one of
/// the five predefined entities; no `<!` construct (DOCTYPE, ENTITY,
/// comment, CDATA) and no other processing instruction; only whitespace
/// outside the root. Not a general XML parser (the full validator is Story
/// 02.0 Task 03's), but it rejects every structural break an escaping bug
/// could cause.
inline bool is_structurally_valid_xml(const std::string& doc)
{
    if (!is_valid_utf8(doc)) return false;
    for (const char c : doc) {
        const auto b = static_cast<unsigned char>(c);
        if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D) return false;
    }
    if (doc.find("\xEF\xBF\xBE") != std::string::npos || doc.find("\xEF\xBF\xBF") != std::string::npos) {
        return false; // U+FFFE / U+FFFF
    }
    auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    auto is_name_char = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    };
    auto valid_reference = [&doc](size_t amp) {
        for (const char* entity : {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;"}) {
            if (doc.compare(amp, std::char_traits<char>::length(entity), entity) == 0) return true;
        }
        return false;
    };

    size_t i = 0;
    const std::string declaration = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    if (doc.compare(0, declaration.size(), declaration) == 0) i = declaration.size();

    std::vector<std::string> open;
    bool root_seen = false;
    while (i < doc.size()) {
        const char c = doc[i];
        if (c == '&') {
            if (open.empty() || !valid_reference(i)) return false;
            ++i;
        }
        else if (c == '>') {
            if (open.empty()) return false;
            ++i; // legal in character data, though this writer escapes it
        }
        else if (c != '<') {
            if (open.empty() && !is_space(c)) return false; // text outside root
            ++i;
        }
        else if (doc.compare(i, 2, "</") == 0) {
            i += 2;
            const size_t start = i;
            while (i < doc.size() && is_name_char(doc[i])) ++i;
            if (open.empty() || doc.substr(start, i - start) != open.back()) return false;
            if (i >= doc.size() || doc[i] != '>') return false;
            open.pop_back();
            ++i;
        }
        else {
            ++i;
            const size_t start = i;
            while (i < doc.size() && is_name_char(doc[i])) ++i;
            if (i == start) return false; // `<!`, `<?`, `< ` ...
            if (open.empty()) {
                if (root_seen) return false; // second root
                root_seen = true;
            }
            open.push_back(doc.substr(start, i - start));
            while (true) {
                while (i < doc.size() && is_space(doc[i])) ++i;
                if (i >= doc.size()) return false;
                if (doc[i] == '>') { ++i; break; }
                if (doc.compare(i, 2, "/>") == 0) { i += 2; open.pop_back(); break; }
                const size_t attr = i;
                while (i < doc.size() && is_name_char(doc[i])) ++i;
                if (i == attr || i + 1 >= doc.size() || doc[i] != '=' || doc[i + 1] != '"') return false;
                i += 2;
                while (i < doc.size() && doc[i] != '"') {
                    if (doc[i] == '<') return false;
                    if (doc[i] == '&' && !valid_reference(i)) return false;
                    ++i;
                }
                if (i >= doc.size()) return false;
                ++i; // closing quote
            }
        }
    }
    return root_seen && open.empty();
}

/// @brief Minimal RFC 8259 recursive-descent check: valid UTF-8; one value
/// with only whitespace around it; strings with no raw U+0000-U+001F and
/// only the standard escapes (`\uXXXX` with four hex digits); numbers per
/// the grammar; no duplicate key within one object (compared on the
/// escaped key text, which is exact for this writer's canonical escaping).
/// The fuller validator with its own self-tests is Story 02.0 Task 03's.
class JsonStructureChecker {
public:
    /// @return true iff `doc` is one well-formed JSON text.
    static bool check(const std::string& doc)
    {
        if (!is_valid_utf8(doc)) return false;
        JsonStructureChecker checker(doc);
        checker.skip_space();
        if (!checker.value(0)) return false;
        checker.skip_space();
        return checker.pos_ == doc.size();
    }

private:
    explicit JsonStructureChecker(const std::string& doc) : doc_(doc) {}

    bool at_end() const { return pos_ >= doc_.size(); }
    char peek() const { return at_end() ? '\0' : doc_[pos_]; }
    void skip_space()
    {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) ++pos_;
    }
    bool literal(const char* word)
    {
        const size_t n = std::char_traits<char>::length(word);
        if (doc_.compare(pos_, n, word) != 0) return false;
        pos_ += n;
        return true;
    }
    bool value(int depth)
    {
        if (depth > 64) return false;
        switch (peek()) {
        case '{': return object(depth);
        case '[': return array(depth);
        case '"': { std::string ignored; return string(ignored); }
        case 't': return literal("true");
        case 'f': return literal("false");
        case 'n': return literal("null");
        default: return number();
        }
    }
    bool object(int depth)
    {
        ++pos_; // '{'
        skip_space();
        if (peek() == '}') { ++pos_; return true; }
        std::vector<std::string> keys;
        while (true) {
            std::string key;
            if (peek() != '"' || !string(key)) return false;
            for (const auto& seen : keys) {
                if (seen == key) return false; // duplicate key
            }
            keys.push_back(key);
            skip_space();
            if (peek() != ':') return false;
            ++pos_;
            skip_space();
            if (!value(depth + 1)) return false;
            skip_space();
            if (peek() == ',') { ++pos_; skip_space(); continue; }
            if (peek() == '}') { ++pos_; return true; }
            return false;
        }
    }
    bool array(int depth)
    {
        ++pos_; // '['
        skip_space();
        if (peek() == ']') { ++pos_; return true; }
        while (true) {
            if (!value(depth + 1)) return false;
            skip_space();
            if (peek() == ',') { ++pos_; skip_space(); continue; }
            if (peek() == ']') { ++pos_; return true; }
            return false;
        }
    }
    bool string(std::string& raw)
    {
        const size_t start = ++pos_; // past opening quote
        while (!at_end()) {
            const auto c = static_cast<unsigned char>(doc_[pos_]);
            if (c == '"') { raw = doc_.substr(start, pos_ - start); ++pos_; return true; }
            if (c < 0x20) return false; // raw control character
            if (c == '\\') {
                ++pos_;
                const char e = peek();
                if (e == 'u') {
                    for (int k = 1; k <= 4; ++k) {
                        if (pos_ + k >= doc_.size() || !std::isxdigit(static_cast<unsigned char>(doc_[pos_ + k]))) return false;
                    }
                    pos_ += 5;
                    continue;
                }
                if (std::string("\"\\/bfnrt").find(e) == std::string::npos || e == '\0') return false;
            }
            ++pos_;
        }
        return false; // unterminated
    }
    bool number()
    {
        auto digit = [this] { return !at_end() && peek() >= '0' && peek() <= '9'; };
        if (peek() == '-') ++pos_;
        if (peek() == '0') { ++pos_; }
        else if (digit()) { while (digit()) ++pos_; }
        else return false;
        if (peek() == '.') { ++pos_; if (!digit()) return false; while (digit()) ++pos_; }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!digit()) return false;
            while (digit()) ++pos_;
        }
        return true;
    }

    const std::string& doc_;
    size_t pos_ = 0;
};

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

} // namespace test
} // namespace smbios
