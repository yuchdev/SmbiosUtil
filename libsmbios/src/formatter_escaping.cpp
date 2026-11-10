#include <libsmbios/formatters/formatter_escaping.hpp>

#include <cstddef>

using namespace smbios;

namespace {

/// U+FFFD REPLACEMENT CHARACTER.
constexpr uint32_t kReplacementCharacter = 0xFFFD;

/// Lowercase hex digits, shared by every \xXX / \u00XX / hex_encode output.
constexpr char kHexDigits[] = "0123456789abcdef";

/// @brief True iff `byte` lies in the inclusive range [low, high].
constexpr bool in_range(unsigned char byte, unsigned char low, unsigned char high)
{
    return byte >= low && byte <= high;
}

/// @brief The single strict UTF-8 decoder behind every escaper (threat model
/// INFO-6: one shared decoding step, never one per escaper - CWE-176).
///
/// Calls `emit(code_point)` once per decoded code point, in order. Each
/// maximal subpart of an ill-formed sequence is reported as exactly one
/// U+FFFD. The well-formed byte sequences are exactly Unicode 15 Table 3-7:
///
///   U+0000..U+007F     00..7F
///   U+0080..U+07FF     C2..DF 80..BF
///   U+0800..U+0FFF     E0     A0..BF 80..BF
///   U+1000..U+CFFF     E1..EC 80..BF 80..BF
///   U+D000..U+D7FF     ED     80..9F 80..BF
///   U+E000..U+FFFF     EE..EF 80..BF 80..BF
///   U+10000..U+3FFFF   F0     90..BF 80..BF 80..BF
///   U+40000..U+FFFFF   F1..F3 80..BF 80..BF 80..BF
///   U+100000..U+10FFFF F4     80..8F 80..BF 80..BF
///
/// Restricting the *second* byte's range per lead byte is what rejects
/// overlongs (E0/F0), surrogates (ED) and > U+10FFFF (F4) at the earliest
/// possible byte, which is what makes the subparts maximal. Every index is
/// bounds-checked against s.size() before it is read.
template <typename Emit>
void for_each_code_point(const std::string& s, Emit&& emit)
{
    const size_t size = s.size();
    size_t i = 0;
    while (i < size) {
        const auto lead = static_cast<unsigned char>(s[i]);

        if (lead < 0x80) {
            emit(static_cast<uint32_t>(lead));
            ++i;
            continue;
        }

        size_t continuation_count = 0;   // continuation bytes after the lead
        unsigned char second_low = 0x80; // allowed range of the 2nd byte
        unsigned char second_high = 0xBF;
        uint32_t code_point = 0;

        if (in_range(lead, 0xC2, 0xDF)) {
            continuation_count = 1;
            code_point = lead & 0x1Fu;
        }
        else if (in_range(lead, 0xE0, 0xEF)) {
            continuation_count = 2;
            code_point = lead & 0x0Fu;
            if (lead == 0xE0) {
                second_low = 0xA0;  // reject overlong
            }
            else if (lead == 0xED) {
                second_high = 0x9F; // reject surrogates U+D800-U+DFFF
            }
        }
        else if (in_range(lead, 0xF0, 0xF4)) {
            continuation_count = 3;
            code_point = lead & 0x07u;
            if (lead == 0xF0) {
                second_low = 0x90;  // reject overlong
            }
            else if (lead == 0xF4) {
                second_high = 0x8F; // reject > U+10FFFF
            }
        }
        else {
            // 80..BF (stray continuation), C0/C1 (always overlong), F5..FF
            // (never valid): a one-byte maximal subpart.
            emit(kReplacementCharacter);
            ++i;
            continue;
        }

        size_t consumed = 1; // bytes of this sequence accepted so far
        bool well_formed = true;
        for (size_t k = 0; k < continuation_count; ++k) {
            if (i + consumed >= size) {
                well_formed = false; // truncated by end of input
                break;
            }
            const auto byte = static_cast<unsigned char>(s[i + consumed]);
            const unsigned char low = (k == 0) ? second_low : 0x80;
            const unsigned char high = (k == 0) ? second_high : 0xBF;
            if (!in_range(byte, low, high)) {
                well_formed = false; // truncated by a non-matching byte
                break;
            }
            code_point = (code_point << 6) | (byte & 0x3Fu);
            ++consumed;
        }

        // Ill-formed: the `consumed` bytes accepted so far are one maximal
        // subpart; resume at the byte that broke the sequence (not after
        // it), so that byte is decoded on its own merits.
        emit(well_formed ? code_point : kReplacementCharacter);
        i += consumed;
    }
}

/// @brief Appends `code_point` (a Unicode scalar value, never a surrogate,
/// at most U+10FFFF - guaranteed by for_each_code_point()) to `out` as
/// shortest-form UTF-8.
void append_utf8(std::string& out, uint32_t code_point)
{
    if (code_point < 0x80) {
        out.push_back(static_cast<char>(code_point));
    }
    else if (code_point < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else if (code_point < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

/// @brief Appends `prefix` followed by `digits` lowercase hex digits of
/// `value` (most significant first), e.g. ("\\x", 0x1F, 2) -> `\x1f`.
void append_hex_escape(std::string& out, const char* prefix, uint32_t value, int digits)
{
    out += prefix;
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        out.push_back(kHexDigits[(value >> shift) & 0xF]);
    }
}

/// @brief True iff XML 1.0 (§2.2 `Char`) forbids `code_point` in a document,
/// even as a character reference. Surrogates are also outside `Char`, but
/// for_each_code_point() never yields one.
constexpr bool is_xml_forbidden(uint32_t code_point)
{
    if (code_point < 0x20) {
        return code_point != 0x09 && code_point != 0x0A && code_point != 0x0D;
    }
    return code_point == 0xFFFE || code_point == 0xFFFF;
}

} // namespace

std::string smbios::sanitize_utf8(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for_each_code_point(s, [&out](uint32_t code_point) { append_utf8(out, code_point); });
    return out;
}

std::string smbios::xml_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for_each_code_point(s, [&out](uint32_t code_point) {
        if (is_xml_forbidden(code_point)) {
            append_utf8(out, kReplacementCharacter); // replace, never &#x..;
            return;
        }
        switch (code_point) {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:   append_utf8(out, code_point); break;
        }
    });
    return out;
}

std::string smbios::json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for_each_code_point(s, [&out](uint32_t code_point) {
        switch (code_point) {
        case '"':    out += "\\\""; return;
        case '\\':   out += "\\\\"; return;
        case '\b':   out += "\\b";  return;
        case '\f':   out += "\\f";  return;
        case '\n':   out += "\\n";  return;
        case '\r':   out += "\\r";  return;
        case '\t':   out += "\\t";  return;
        case 0x2028: out += "\\u2028"; return;
        case 0x2029: out += "\\u2029"; return;
        default: break;
        }
        if (code_point < 0x20) {
            append_hex_escape(out, "\\u", code_point, 4); // e.g. ESC -> \u001b
            return;
        }
        append_utf8(out, code_point);
    });
    return out;
}

std::string smbios::yaml_double_quoted_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for_each_code_point(s, [&out](uint32_t code_point) {
        switch (code_point) {
        case '"':    out += "\\\""; return;
        case '\\':   out += "\\\\"; return;
        case 0x00:   out += "\\0";  return;
        case 0x07:   out += "\\a";  return;
        case 0x08:   out += "\\b";  return;
        case 0x09:   out += "\\t";  return;
        case 0x0A:   out += "\\n";  return;
        case 0x0B:   out += "\\v";  return;
        case 0x0C:   out += "\\f";  return;
        case 0x0D:   out += "\\r";  return;
        case 0x1B:   out += "\\e";  return;
        case 0x85:   out += "\\N";  return;
        case 0x2028: out += "\\L";  return;
        case 0x2029: out += "\\P";  return;
        case 0xFFFE: out += "\\ufffe"; return;
        case 0xFFFF: out += "\\uffff"; return;
        default: break;
        }
        if (code_point < 0x20 || (code_point >= 0x7F && code_point <= 0x9F)) {
            append_hex_escape(out, "\\x", code_point, 2); // C0 rest, DEL, C1
            return;
        }
        append_utf8(out, code_point);
    });
    return out;
}

std::string smbios::hex_encode(const std::vector<uint8_t>& bytes)
{
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const uint8_t byte : bytes) {
        out.push_back(kHexDigits[byte >> 4]);
        out.push_back(kHexDigits[byte & 0x0F]);
    }
    return out;
}
