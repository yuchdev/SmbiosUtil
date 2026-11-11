#pragma once

#include <string_view>

// Test-local structural validators for milestone 0009 Story 02.0's
// machine-readable formatters (Task 03, security-auditor INFO-6).
//
// No XML, JSON or YAML parser is available to smbios_unit_test (vcpkg.json carries
// only iostreams, program-options, test and date-time), so a real round-trip
// parse is not an option inside the unit binary; these validators stand in
// for one. They are written independently of libsmbios's own escaping code
// (formatter_escaping.cpp) so the two cross-check each other, and each has
// its own positive and negative self-tests in
// structured_output_validators_test.cpp, so a validator that accepts
// everything cannot pass silently. Test-only: never linked into a
// production target, and adds no dependency.

namespace smbios {
namespace test {

/// @brief Strict RFC 3629 UTF-8 check: rejects stray continuation bytes,
/// truncated sequences, overlong encodings, surrogates (U+D800-U+DFFF,
/// i.e. CESU-8) and anything above U+10FFFF.
/// @param text bytes to check.
/// @return true iff `text` is entirely well-formed UTF-8 (empty is valid).
bool is_valid_utf8(std::string_view text);

/// @brief Recursive-descent check of the RFC 8259 JSON-text grammar.
///
/// Accepts exactly one JSON value (any of object, array, string, number,
/// true, false, null) with optional whitespace (SP, HTAB, LF, CR) around it.
/// Rejects: invalid UTF-8 anywhere; raw U+0000-U+001F inside a string;
/// escapes other than `\" \\ \/ \b \f \n \r \t \uXXXX`; numbers outside the
/// grammar (leading zeros, bare `-`, `1.`, `.5`, `1e`); trailing commas;
/// trailing garbage after the value; nesting deeper than 256; and duplicate
/// keys within one object. Keys are compared after decoding their escapes
/// (so `"A"` and `"A"` collide), matching what a parser that builds a
/// map would see.
/// @param document the complete document to check.
/// @return true iff `document` is one well-formed JSON text.
bool is_well_formed_json(std::string_view document);

/// @brief Structural XML 1.0 well-formedness check for the document subset
/// this project's writers emit.
///
/// Accepts an optional leading `<?xml version="1.0" ...?>` declaration,
/// then exactly one root element with balanced, correctly nested tags,
/// with only whitespace outside the root. Attribute values must be quoted
/// (`"` or `'`), contain no `<`, and be unique by name within one tag.
/// Entity references are limited to the five predefined entities
/// (`&amp; &lt; &gt; &quot; &apos;`); character references (`&#N;`,
/// `&#xH;`) are accepted only when they name a code point XML 1.0 allows.
/// Rejects: invalid UTF-8; any XML 1.0-forbidden code point (C0 controls
/// other than TAB/LF/CR, U+FFFE, U+FFFF), raw or as a character reference;
/// `]]>` in character data; and, conservatively, every `<!` construct
/// (`<!DOCTYPE`, `<!ENTITY`, comments, CDATA) and every processing
/// instruction other than the leading declaration - none of which the
/// writers ever emit. Element and attribute names are limited to ASCII
/// name characters.
/// @param document the complete document to check.
/// @return true iff `document` is one well-formed document of that subset.
bool is_well_formed_xml(std::string_view document);

/// @brief Strict check of the flat YAML 1.2 subset the `yaml` formatter
/// (milestone 0009 Story 03.0 Task 01) emits; deliberately not a general
/// YAML recogniser, so anything outside that subset is rejected.
///
/// Accepted grammar (every line terminated by exactly one LF):
/// @code
/// document := "---" LF ( LF | item+ )
/// item     := "- type: " dq LF "  fields:" ( " {}" LF | LF field+ )
/// field    := "    " dq ": " ( dq | uint | "true" | "false" ) LF
/// uint     := "0" | [1-9][0-9]*            (at most 2^64-1)
/// dq       := '"' ( raw-char | escape )* '"'
/// @endcode
/// An escape is any YAML 1.2 §5.7 escape (`\0 \a \b \t \v \n \f \r \e`,
/// backslash-space, `\" \/ \\ \N \_ \L \P \xXX \uXXXX \UXXXXXXXX`, hex in
/// either case) whose value is a Unicode scalar value. A raw-char is any
/// code point other than `"` and `\` that is in YAML 1.2 `c-printable`
/// (§5.1) and is not TAB, U+0085, U+2028, U+2029 (YAML 1.1 line breaks) or
/// U+FEFF (BOM, §5.2) - the formatter escapes all of those.
///
/// Rejects: invalid UTF-8; a BOM; CR or CRLF line endings; any raw
/// non-printable code point; unquoted string scalars (`NO`, `~`, `null`,
/// `0x0A`, `1.10`); anchors, aliases, tags and directives (none fit the
/// grammar); numbers with a leading zero, a sign or above 2^64-1; a
/// missing final LF; and duplicate keys within one `fields` mapping,
/// compared after decoding escapes.
/// @param document the complete document to check.
/// @return true iff `document` is one document of that subset.
bool is_well_formed_flat_yaml(std::string_view document);

} // namespace test
} // namespace smbios
