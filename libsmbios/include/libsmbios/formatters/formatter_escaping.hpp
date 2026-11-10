#pragma once
#include <cstdint>
#include <string>
#include <vector>

/// @file formatter_escaping.hpp
/// @brief Shared, format-specific escaping helpers for the structured output
/// formatters (milestone 0009 Story 02.0 Task 02): XML, JSON and YAML
/// escaping plus the raw-byte hex encoding.
///
/// Every string that reaches these helpers may be firmware-controlled (a DMI
/// string, or text from a `--read-file` dump): any byte 0x01-0xFF, arbitrary
/// length, not guaranteed to be UTF-8 (threat model
/// `docs/security/2026-11-10-structured-field-model.md`, INFO-6). Each
/// escaper therefore runs the same strict UTF-8 sanitising step,
/// sanitize_utf8(), *before* any format-specific escaping, so:
/// - every output is valid UTF-8, whatever the input bytes were;
/// - an invalid input sequence can never combine with an escape to form a
///   structural character in the target format (CWE-176);
/// - the output is a pure function of the input bytes - no locale, host,
///   time or platform dependence, and no platform line endings.
///
/// Each helper returns the escaped *content* only; the caller (the
/// formatter) adds the surrounding quotes or markup. None of them logs,
/// throws on any input byte, or truncates.

namespace smbios {

/// @brief Strict UTF-8 validation (RFC 3629 / Unicode 15 Table 3-7) with
/// deterministic replacement; the single shared decoding step behind every
/// escaper in this header.
///
/// Well-formed sequences are copied through unchanged. Everything else is
/// invalid and replaced, including:
/// - stray continuation bytes (`80`-`BF`) and bytes that can never start a
///   sequence (`C0`, `C1`, `F5`-`FF`);
/// - overlong encodings (e.g. `C0 AF`, `E0 80 80`);
/// - UTF-16 surrogates U+D800-U+DFFF, i.e. CESU-8 (e.g. `ED A0 80`);
/// - code points above U+10FFFF (e.g. `F4 90 80 80`);
/// - sequences truncated by a following non-continuation byte or by the end
///   of the string (e.g. a trailing `C3`, or Latin-1 `"Caf\xE9"`).
///
/// Each *maximal subpart* of an ill-formed sequence (Unicode's "U+FFFD
/// substitution of maximal subparts" practice, the same one Python's
/// `bytes.decode("utf-8", "replace")` and WHATWG follow) becomes exactly one
/// U+FFFD (`EF BF BD`): the longest prefix that could still start a
/// well-formed sequence is consumed as one unit, and decoding resumes at the
/// first byte that broke it. So `ED A0 80` yields three U+FFFD, a trailing
/// `E2 82` yields one, and `C0 AF` yields two.
///
/// U+0000 (which a DMI string cannot contain, but a std::string can) is a
/// valid code point and passes through; the escapers decide what to do with
/// it.
/// @param s arbitrary input bytes.
/// @return valid UTF-8; identical to `s` iff `s` was already valid UTF-8.
/// Idempotent: sanitize_utf8(sanitize_utf8(s)) == sanitize_utf8(s).
std::string sanitize_utf8(const std::string& s);

/// @brief Escapes `s` for use as XML 1.0 character data or as a
/// double- or single-quoted attribute value.
///
/// Steps, in order:
/// 1. sanitize_utf8(): invalid UTF-8 becomes U+FFFD.
/// 2. Code points XML 1.0 forbids in a document - U+0000-U+0008, U+000B,
///    U+000C, U+000E-U+001F, U+FFFE and U+FFFF - are *replaced* with U+FFFD.
///    They are illegal even as character references (`&#x1B;` is not
///    well-formed XML 1.0), so they cannot be escaped. ESC (and therefore
///    any ANSI escape sequence's introducer) is replaced by this rule.
/// 3. `&` `<` `>` `"` `'` become `&amp;` `&lt;` `&gt;` `&quot;` `&apos;`.
///
/// Passed through unchanged as data: TAB, LF and CR; DEL and the C1
/// controls U+007F-U+009F (legal in XML 1.0); bidi overrides
/// (U+202A-U+202E, U+2066-U+2069) and every other valid character.
/// Neutralising bidi or C1 for display is the consumer's job (the escaper
/// guarantees document structure, not display safety). Note that a
/// conforming XML parser normalises a raw CR (or CR LF) in character data to
/// LF, and TAB/LF/CR in attribute values to spaces; this escaper does not
/// encode them as character references.
///
/// No character reference (`&#...;`) is ever emitted, so the output only
/// ever uses the five predefined entities.
/// @param s arbitrary input bytes.
/// @return valid UTF-8 containing no XML-forbidden code point and no raw
/// `&`, `<`, `>`, `"` or `'`.
std::string xml_escape(const std::string& s);

/// @brief Escapes `s` for use inside a JSON string literal (RFC 8259 §7);
/// the caller supplies the surrounding double quotes.
///
/// Steps, in order:
/// 1. sanitize_utf8(): invalid UTF-8 becomes U+FFFD.
/// 2. `"` becomes `\"` and `\` becomes `\\`.
/// 3. Every code point U+0000-U+001F is escaped: `\b` `\f` `\n` `\r` `\t`
///    where a short form exists, otherwise `\u00XX` with lowercase hex
///    digits (so ESC becomes `\u001b`). No raw C0 byte is ever emitted.
/// 4. U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR become ` `
///    and ` `. Both are legal raw in JSON, but escaping them keeps the
///    output safe to embed in a JavaScript source context.
///
/// Every other code point - including `/`, DEL, the C1 controls, U+FFFE,
/// U+FFFF and the bidi overrides - is emitted as raw UTF-8.
/// @param s arbitrary input bytes.
/// @return valid UTF-8 that forms a valid JSON string body.
std::string json_escape(const std::string& s);

/// @brief Escapes `s` for use inside a YAML 1.2 double-quoted scalar
/// (§7.3.1); the caller supplies the surrounding double quotes. Shared with
/// Story 03.0's YAML writer.
///
/// Steps, in order:
/// 1. sanitize_utf8(): invalid UTF-8 becomes U+FFFD.
/// 2. `"` becomes `\"` and `\` becomes `\\`.
/// 3. U+0000-U+001F use the YAML short forms where they exist - `\0` `\a`
///    `\b` `\t` `\n` `\v` `\f` `\r` `\e` - and `\xXX` otherwise (so ESC
///    becomes `\e`).
/// 4. DEL and the C1 controls U+007F-U+009F become `\xXX`, except U+0085
///    NEXT LINE, which becomes `\N`.
/// 5. U+2028 and U+2029 become `\L` and `\P` (YAML 1.1 parsers treat them,
///    and U+0085, as line breaks).
/// 6. U+FFFE and U+FFFF become `￾` and `￿`.
///
/// Hex digits in every escape are lowercase. After these rules no code
/// point outside YAML 1.2's `c-printable` set (§5.1) remains raw. Bidi
/// overrides are printable and pass through as data. The result is always
/// meant to be double-quoted, which also defeats YAML's implicit typing
/// (`NO`, `~`, `0x0A`, `&anchor`, `*alias`, `!tag`, `%`, ...).
/// @param s arbitrary input bytes.
/// @return valid UTF-8 that forms a valid YAML double-quoted scalar body.
std::string yaml_double_quoted_escape(const std::string& s);

/// @brief Hex-encodes raw bytes: two lowercase hex digits per byte, no
/// separators, no prefix (e.g. `{0x00, 0xAB}` -> `"00ab"`). Used by every
/// structured formatter for `std::vector<uint8_t>` field values, which are
/// never emitted as a raw string in any format.
///
/// No length cap, by construction: every byte-vector field today is sliced
/// from one structure's formatted area, which `DMIHeader::length` (a
/// `uint8_t`) bounds to at most 255 bytes, so the output is at most 510
/// characters and is never truncated. A future field sourcing bytes from
/// *outside* one formatted area (e.g. a reassembled multi-structure payload)
/// must reopen this decision and add a cap before it lands.
/// @param bytes the bytes to encode; may be empty.
/// @return a string of exactly `2 * bytes.size()` characters from
/// `[0-9a-f]`; empty for empty input.
std::string hex_encode(const std::vector<uint8_t>& bytes);

} // namespace smbios
