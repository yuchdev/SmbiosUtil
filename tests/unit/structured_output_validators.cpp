#include "structured_output_validators.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// See structured_output_validators.hpp for the contract of each validator.
// Implementation notes: both validators decode UTF-8 with the same strict
// decoder below (decode_utf8_at), never with libsmbios's own
// for_each_code_point(), so a decoder bug in the library cannot hide itself
// by being reused here.

namespace smbios {
namespace test {

namespace {

/// @brief Decodes one strict RFC 3629 UTF-8 scalar value at `text[pos]`.
/// @param text the input.
/// @param pos offset of the lead byte; must be < text.size().
/// @param code_point receives the decoded scalar value on success.
/// @return the sequence length (1-4), or 0 if the bytes at `pos` do not
/// start a well-formed sequence.
size_t decode_utf8_at(std::string_view text, size_t pos, uint32_t& code_point)
{
    const auto b0 = static_cast<unsigned char>(text[pos]);
    size_t length = 0;
    uint32_t cp = 0;
    if (b0 < 0x80) {
        code_point = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) { length = 2; cp = b0 & 0x1Fu; }
    else if ((b0 & 0xF0) == 0xE0) { length = 3; cp = b0 & 0x0Fu; }
    else if ((b0 & 0xF8) == 0xF0) { length = 4; cp = b0 & 0x07u; }
    else { return 0; }
    if (pos + length > text.size()) return 0;
    for (size_t k = 1; k < length; ++k) {
        const auto b = static_cast<unsigned char>(text[pos + k]);
        if ((b & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (b & 0x3Fu);
    }
    static constexpr uint32_t kMinForLength[] = {0, 0, 0x80, 0x800, 0x10000};
    if (cp < kMinForLength[length]) return 0;     // overlong
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;   // surrogate (CESU-8)
    if (cp > 0x10FFFF) return 0;                  // above Unicode range
    code_point = cp;
    return length;
}

// ------------------------------------------------------------------ JSON ---

/// @brief RFC 8259 recursive-descent recogniser over an already
/// UTF-8-validated document.
class JsonValidator {
public:
    explicit JsonValidator(std::string_view doc) : doc_(doc) {}

    /// @return true iff the whole document is exactly one JSON value.
    bool run()
    {
        skip_space();
        if (!value(0)) return false;
        skip_space();
        return pos_ == doc_.size();
    }

private:
    static constexpr int kMaxDepth = 256;

    bool at_end() const { return pos_ >= doc_.size(); }
    char peek() const { return at_end() ? '\0' : doc_[pos_]; }
    bool is_digit() const { return !at_end() && peek() >= '0' && peek() <= '9'; }

    void skip_space()
    {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) ++pos_;
    }

    bool literal(std::string_view word)
    {
        if (doc_.substr(pos_, word.size()) != word) return false;
        pos_ += word.size();
        return true;
    }

    bool value(int depth)
    {
        if (depth > kMaxDepth) return false;
        switch (peek()) {
        case '{': return object(depth);
        case '[': return array(depth);
        case '"': { std::u32string ignored; return string(ignored); }
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
        std::set<std::u32string> keys;
        while (true) {
            std::u32string key;
            if (peek() != '"' || !string(key)) return false;
            if (!keys.insert(key).second) return false; // duplicate key
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

    /// @brief Reads four hex digits at pos_ into `unit`.
    bool hex4(uint32_t& unit)
    {
        if (pos_ + 4 > doc_.size()) return false;
        unit = 0;
        for (size_t k = 0; k < 4; ++k) {
            const char c = doc_[pos_ + k];
            uint32_t digit = 0;
            if (c >= '0' && c <= '9') digit = static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') digit = static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = static_cast<uint32_t>(c - 'A' + 10);
            else return false;
            unit = (unit << 4) | digit;
        }
        pos_ += 4;
        return true;
    }

    /// @brief Parses one string starting at its opening quote; `decoded`
    /// receives its code points (surrogate pairs combined; a lone surrogate
    /// escape, which RFC 8259's grammar permits, is kept as its own value).
    bool string(std::u32string& decoded)
    {
        ++pos_; // opening quote
        while (!at_end()) {
            const auto c = static_cast<unsigned char>(doc_[pos_]);
            if (c == '"') { ++pos_; return true; }
            if (c < 0x20) return false; // raw control character
            if (c != '\\') {
                uint32_t cp = 0;
                const size_t length = decode_utf8_at(doc_, pos_, cp);
                if (length == 0) return false; // unreachable after the up-front check
                decoded.push_back(cp);
                pos_ += length;
                continue;
            }
            ++pos_; // backslash
            const char e = peek();
            ++pos_;
            switch (e) {
            case '"': decoded.push_back(U'"'); break;
            case '\\': decoded.push_back(U'\\'); break;
            case '/': decoded.push_back(U'/'); break;
            case 'b': decoded.push_back(U'\b'); break;
            case 'f': decoded.push_back(U'\f'); break;
            case 'n': decoded.push_back(U'\n'); break;
            case 'r': decoded.push_back(U'\r'); break;
            case 't': decoded.push_back(U'\t'); break;
            case 'u': {
                uint32_t unit = 0;
                if (!hex4(unit)) return false;
                if (unit >= 0xD800 && unit <= 0xDBFF && doc_.substr(pos_, 2) == "\\u") {
                    const size_t saved = pos_;
                    pos_ += 2;
                    uint32_t low = 0;
                    if (hex4(low) && low >= 0xDC00 && low <= 0xDFFF) {
                        decoded.push_back(0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
                        break;
                    }
                    pos_ = saved; // not a pair: re-read the next escape on its own
                }
                decoded.push_back(unit);
                break;
            }
            default:
                return false; // invalid escape, or backslash at end of input
            }
        }
        return false; // unterminated
    }

    bool number()
    {
        if (peek() == '-') ++pos_;
        if (peek() == '0') {
            ++pos_;
        }
        else if (is_digit()) {
            while (is_digit()) ++pos_;
        }
        else {
            return false;
        }
        if (peek() == '.') {
            ++pos_;
            if (!is_digit()) return false;
            while (is_digit()) ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!is_digit()) return false;
            while (is_digit()) ++pos_;
        }
        return true;
    }

    std::string_view doc_;
    size_t pos_ = 0;
};

// ------------------------------------------------------------------- XML ---

/// @return true iff `cp` matches XML 1.0's Char production.
bool is_xml_char(uint32_t cp)
{
    return cp == 0x09 || cp == 0x0A || cp == 0x0D || (cp >= 0x20 && cp <= 0xD7FF) ||
           (cp >= 0xE000 && cp <= 0xFFFD) || (cp >= 0x10000 && cp <= 0x10FFFF);
}

bool is_xml_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_name_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':';
}

bool is_name_char(char c)
{
    return is_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

/// @brief Recogniser for the XML subset described in the header, over an
/// already UTF-8- and Char-validated document.
class XmlValidator {
public:
    explicit XmlValidator(std::string_view doc) : doc_(doc) {}

    bool run()
    {
        if (!declaration()) return false;
        std::vector<std::string_view> open;
        bool root_seen = false;
        while (!at_end()) {
            const char c = doc_[pos_];
            if (c == '<') {
                if (starts_with("</")) {
                    if (!end_tag(open)) return false;
                }
                else if (starts_with("<!") || starts_with("<?")) {
                    return false; // DOCTYPE/ENTITY/comment/CDATA/PI: never emitted
                }
                else {
                    if (open.empty()) {
                        if (root_seen) return false; // second root element
                        root_seen = true;
                    }
                    if (!start_tag(open)) return false;
                }
            }
            else if (open.empty()) {
                if (!is_xml_space(c)) return false; // character data outside the root
                ++pos_;
            }
            else if (c == '&') {
                if (!reference()) return false;
            }
            else {
                if (starts_with("]]>")) return false; // forbidden in character data
                ++pos_;
            }
        }
        return root_seen && open.empty();
    }

private:
    bool at_end() const { return pos_ >= doc_.size(); }
    bool starts_with(std::string_view s) const { return doc_.substr(pos_, s.size()) == s; }

    void skip_space()
    {
        while (!at_end() && is_xml_space(doc_[pos_])) ++pos_;
    }

    /// @brief Optional `<?xml version="1.0" ...?>` at offset 0 only.
    bool declaration()
    {
        if (!starts_with("<?xml")) return true;
        pos_ += 5;
        if (at_end() || !is_xml_space(doc_[pos_])) return false; // e.g. `<?xml-stylesheet`
        skip_space();
        if (!starts_with("version=\"1.0\"") && !starts_with("version='1.0'")) return false;
        const size_t close = doc_.find("?>", pos_);
        if (close == std::string_view::npos) return false;
        pos_ = close + 2;
        return true;
    }

    bool name(std::string_view& out)
    {
        const size_t start = pos_;
        if (at_end() || !is_name_start(doc_[pos_])) return false;
        while (!at_end() && is_name_char(doc_[pos_])) ++pos_;
        out = doc_.substr(start, pos_ - start);
        return true;
    }

    /// @brief One `&...;` reference at pos_ (entity or character).
    bool reference()
    {
        ++pos_; // '&'
        if (!at_end() && doc_[pos_] == '#') {
            ++pos_;
            const bool hex = !at_end() && doc_[pos_] == 'x';
            if (hex) ++pos_;
            uint32_t cp = 0;
            size_t digits = 0;
            while (!at_end() && doc_[pos_] != ';') {
                const char c = doc_[pos_];
                uint32_t d = 0;
                if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
                else if (hex && c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
                else if (hex && c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
                else return false;
                cp = cp * (hex ? 16u : 10u) + d;
                if (cp > 0x10FFFF) return false; // also bounds the accumulator
                ++digits;
                ++pos_;
            }
            if (at_end() || digits == 0) return false;
            ++pos_; // ';'
            return is_xml_char(cp);
        }
        for (std::string_view entity : {"amp;", "lt;", "gt;", "quot;", "apos;"}) {
            if (starts_with(entity)) {
                pos_ += entity.size();
                return true;
            }
        }
        return false; // undeclared entity (there is no DTD)
    }

    bool start_tag(std::vector<std::string_view>& open)
    {
        ++pos_; // '<'
        std::string_view element;
        if (!name(element)) return false;
        std::set<std::string_view> attributes;
        while (true) {
            const size_t before_space = pos_;
            skip_space();
            if (at_end()) return false;
            if (doc_[pos_] == '>') { ++pos_; open.push_back(element); return true; }
            if (starts_with("/>")) { pos_ += 2; return true; }
            if (pos_ == before_space) return false; // attributes need leading whitespace
            std::string_view attribute;
            if (!name(attribute)) return false;
            if (!attributes.insert(attribute).second) return false; // duplicate attribute
            skip_space();
            if (at_end() || doc_[pos_] != '=') return false;
            ++pos_;
            skip_space();
            if (at_end() || (doc_[pos_] != '"' && doc_[pos_] != '\'')) return false;
            const char quote = doc_[pos_++];
            while (!at_end() && doc_[pos_] != quote) {
                if (doc_[pos_] == '<') return false;
                if (doc_[pos_] == '&') {
                    if (!reference()) return false;
                }
                else {
                    ++pos_;
                }
            }
            if (at_end()) return false; // unterminated value
            ++pos_; // closing quote
        }
    }

    bool end_tag(std::vector<std::string_view>& open)
    {
        pos_ += 2; // "</"
        std::string_view element;
        if (!name(element)) return false;
        if (open.empty() || element != open.back()) return false; // mismatched or misnested
        skip_space();
        if (at_end() || doc_[pos_] != '>') return false;
        ++pos_;
        open.pop_back();
        return true;
    }

    std::string_view doc_;
    size_t pos_ = 0;
};

} // namespace

bool is_valid_utf8(std::string_view text)
{
    size_t pos = 0;
    while (pos < text.size()) {
        uint32_t cp = 0;
        const size_t length = decode_utf8_at(text, pos, cp);
        if (length == 0) return false;
        pos += length;
    }
    return true;
}

bool is_well_formed_json(std::string_view document)
{
    if (!is_valid_utf8(document)) return false;
    return JsonValidator(document).run();
}

bool is_well_formed_xml(std::string_view document)
{
    size_t pos = 0;
    while (pos < document.size()) {
        uint32_t cp = 0;
        const size_t length = decode_utf8_at(document, pos, cp);
        if (length == 0 || !is_xml_char(cp)) return false;
        pos += length;
    }
    return XmlValidator(document).run();
}

} // namespace test
} // namespace smbios
