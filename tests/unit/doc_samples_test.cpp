#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <utility>
#include <string>
#include <variant>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/formatters/json_formatter.hpp>
#include <libsmbios/formatters/text_formatter.hpp>
#include <libsmbios/formatters/xml_formatter.hpp>
#include <libsmbios/formatters/yaml_formatter.hpp>

#include "doc_sample_fixture.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;

// Unit tests (milestone 0009, Story 03.0, Task 03, decision D3)
//
// Type: Unit - deterministic, no hardware/privileges needed (the SMBios
// object's live read is discarded; the fixture is injected through the
// SmbiosTableWalkTestAccess seam).
// Scenario: the synthetic doc-sample fixture (doc_sample_fixture.cpp),
// rendered through each of `text`, `xml`, `json`, `yaml` and `csv` with the
// CLI's framing loop (render_document()), equals the checked-in golden file
// tests/data/output_formats/sample.<ext> byte-for-byte. With
// SMBIOS_REGENERATE_DOC_SAMPLES=1 the golden files are (re)written instead.
// The fixture self-check asserts every FieldValue alternative appears and
// no identifying field name does.
// Boundaries: byte-exact comparison (the golden files are `-text` in
// .gitattributes, so no CRLF conversion on Windows checkouts); a mismatch
// reports only the format and the first differing offset, never content.
// On failure first check: whether a formatter or get_fields() change was
// intended - if so, regenerate (SMBIOS_REGENERATE_DOC_SAMPLES=1
// ./smbios_unit_test --run_test=DocSamplesTests, then
// python3 scripts/check_doc_samples.py --write) and review the diff; a null
// `csv` formatter means smbios/examples/csv_formatter.cpp fell out of
// tests/unit/CMakeLists.txt's SOURCES.

namespace {

/// @brief Names every built-in formatter class so the linker keeps each
/// one's object file (and its REGISTER_OUTPUT_FORMATTER registration) in
/// smbios_unit_test, which does not force-load libsmbios.
bool built_in_formatters_are_linked()
{
    return !JsonFormatter().format_name().empty() && !TextFormatter().format_name().empty()
        && !XmlFormatter().format_name().empty() && !YamlFormatter().format_name().empty();
}

/// @brief Typed entries decoded from the doc-sample fixture.
///
/// Owns the SMBios object the entries view into, so the entries never
/// outlive the buffer they point at.
struct DocSampleEntries {
    SMBios bios;
    std::vector<std::unique_ptr<AbstractSMBiosEntry>> entries;
};

/// @brief Parses the fixture and creates one typed entry per structure,
/// exactly as smbios/src/main.cpp does for structured formats.
std::unique_ptr<DocSampleEntries> decode_fixture()
{
    auto decoded = std::make_unique<DocSampleEntries>();
    SmbiosTableWalkTestAccess::parse_synthetic_table(decoded->bios, test::build_doc_sample_table());

    const SMBiosVersion version = test::doc_sample_version();
    const uint8_t* table_end = decoded->bios.get_table_base() + decoded->bios.get_table_size();
    SMBiosEntryFactory factory;
    for (const DMIHeader& header : decoded->bios) {
        decoded->entries.push_back(factory.create(header, version, table_end));
    }
    return decoded;
}

/// @brief Whether golden files should be written instead of compared.
bool regenerate_requested()
{
#ifdef _MSC_VER
#pragma warning(suppress : 4996) // getenv: read-only use, no thread contention here
#endif
    const char* value = std::getenv("SMBIOS_REGENERATE_DOC_SAMPLES");
    return value != nullptr && std::string(value) == "1";
}

/// @brief Absolute path of the golden file for `extension`.
std::string golden_path(const std::string& extension)
{
    return std::string(SMBIOS_TEST_DATA_DIR) + "/output_formats/sample." + extension;
}

/// @brief Reads `path` as raw bytes; sets `ok` to false if it cannot be opened.
std::string read_binary(const std::string& path, bool& ok)
{
    std::ifstream in(path, std::ios::binary);
    ok = static_cast<bool>(in);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/// @brief Offset of the first byte where `a` and `b` differ (or the shorter
/// length if one is a prefix of the other).
size_t first_difference(const std::string& a, const std::string& b)
{
    const auto mismatch = std::mismatch(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(std::min(a.size(), b.size())),
                                        b.begin());
    return static_cast<size_t>(mismatch.first - a.begin());
}

/// @brief Format name -> golden-file extension, for every documented sample.
const std::vector<std::pair<std::string, std::string>>& sample_formats()
{
    static const std::vector<std::pair<std::string, std::string>> formats{
        {"text", "txt"}, {"xml", "xml"}, {"json", "json"}, {"yaml", "yaml"}, {"csv", "csv"},
    };
    return formats;
}

} // namespace

BOOST_AUTO_TEST_SUITE(DocSamplesTests)

/// Regression/golden case: every documented sample equals the fixture's
/// real rendering, byte-for-byte (or is rewritten on request).
BOOST_AUTO_TEST_CASE(GoldenOutputsMatchFixtureRender)
{
    BOOST_REQUIRE(built_in_formatters_are_linked());
    const std::unique_ptr<DocSampleEntries> decoded = decode_fixture();
    BOOST_REQUIRE_EQUAL(decoded->entries.size(), static_cast<size_t>(3));
    for (const auto& entry : decoded->entries) {
        BOOST_REQUIRE(entry != nullptr);
    }

    const bool regenerate = regenerate_requested();
    for (const auto& [name, extension] : sample_formats()) {
        BOOST_TEST_CONTEXT("format " << name)
        {
            const IOutputFormatter* formatter = get_output_formatter(name);
            BOOST_REQUIRE(formatter != nullptr);
            const std::string rendered =
                render_document(*formatter, decoded->entries.begin(), decoded->entries.end());
            const std::string path = golden_path(extension);

            if (regenerate) {
                std::ofstream out(path, std::ios::binary | std::ios::trunc);
                BOOST_REQUIRE_MESSAGE(static_cast<bool>(out), "cannot write golden file for format " << name);
                out.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
                BOOST_REQUIRE(static_cast<bool>(out));
                BOOST_TEST_MESSAGE("regenerated golden sample for format " << name);
                continue;
            }

            bool opened = false;
            const std::string golden = read_binary(path, opened);
            BOOST_REQUIRE_MESSAGE(opened, "missing golden file for format " << name
                                              << " (regenerate with SMBIOS_REGENERATE_DOC_SAMPLES=1)");
            BOOST_CHECK_MESSAGE(rendered == golden,
                                "format " << name << " differs from its golden file at byte offset "
                                          << first_difference(rendered, golden) << " (rendered "
                                          << rendered.size() << " bytes, golden " << golden.size()
                                          << " bytes)");
        }
    }
}

/// Edge/privacy case: the fixture exercises all four FieldValue
/// alternatives, at least one bitmask bool, and no identifying field.
///
/// The privacy half (D3 hard rule) is enforced three independent ways, so a
/// parser bug that hides a field cannot also hide a privacy regression:
/// 1. raw-table walk, independent of SMBios/the entry factory: no structure
///    of type 1/2/3/17 (System, Baseboard, Chassis, Memory Device);
/// 2. raw-table walk: every DMI string is an obviously synthetic literal
///    (contains "Example"/"EXAMPLE");
/// 3. decoded fields: no field whose name contains (case-insensitively)
///    "serial", "uuid", "asset", "part number" or "sku" - every identifying
///    name any libsmbios entry emits ("Serial Number", "Serial", "UUID",
///    "Asset Tag", "Part Number", "Model Part Number", "SKU Number", ...).
BOOST_AUTO_TEST_CASE(FixtureCoversEveryFieldValueAlternative)
{
    // (1) + (2): walk the raw fixture bytes directly.
    const std::vector<uint8_t> table = test::build_doc_sample_table();
    size_t offset = 0;
    size_t structures = 0;
    bool saw_end_of_table = false;
    while (offset + 4 <= table.size() && !saw_end_of_table) {
        const uint8_t type = table[offset];
        const uint8_t length = table[offset + 1];
        BOOST_REQUIRE_GE(length, 4);
        BOOST_REQUIRE_LE(offset + length, table.size());
        BOOST_TEST_CONTEXT("structure " << structures << " (type " << static_cast<int>(type) << ")")
        {
            for (const uint8_t identifying_type : {uint8_t{1}, uint8_t{2}, uint8_t{3}, uint8_t{17}}) {
                BOOST_CHECK_MESSAGE(type != identifying_type, "fixture carries identifying structure type "
                                                                  << static_cast<int>(identifying_type));
            }
            if (type == SMBios::EndOfTable) {
                // SyntheticTableBuilder emits the 4-byte End-of-Table marker
                // with no string set; tolerate an optional double NUL too.
                saw_end_of_table = true;
                offset += length;
                if (offset + 2 <= table.size() && table[offset] == 0 && table[offset + 1] == 0) {
                    offset += 2;
                }
                continue;
            }
            size_t cursor = offset + length;
            const size_t strings_begin = cursor;
            while (cursor < table.size() && table[cursor] != 0) {
                const auto string_end = std::find(table.begin() + static_cast<std::ptrdiff_t>(cursor), table.end(),
                                                  uint8_t{0});
                BOOST_REQUIRE(string_end != table.end());
                const std::string value(table.begin() + static_cast<std::ptrdiff_t>(cursor), string_end);
                BOOST_CHECK_MESSAGE(value.find("Example") != std::string::npos
                                        || value.find("EXAMPLE") != std::string::npos,
                                    "fixture string is not an obviously synthetic Example literal");
                cursor = static_cast<size_t>(string_end - table.begin()) + 1;
            }
            // Skip the string-set terminator: one NUL after the last string,
            // or the double NUL of an empty string set.
            cursor += (cursor == strings_begin) ? 2 : 1;
            offset = cursor;
        }
        ++structures;
    }
    BOOST_CHECK(saw_end_of_table);
    BOOST_CHECK_EQUAL(offset, table.size());

    // (3) + coverage: the decoded fields.
    const std::unique_ptr<DocSampleEntries> decoded = decode_fixture();

    std::set<size_t> alternatives;
    bool bitmask_bool = false;
    for (const auto& entry : decoded->entries) {
        BOOST_REQUIRE(entry != nullptr);
        for (const NamedField& field : entry->get_fields()) {
            alternatives.insert(field.value.index());
            if (entry->get_type() == "BIOS Information" && std::holds_alternative<bool>(field.value)) {
                bitmask_bool = true;
            }
            std::string lowered(field.name);
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            for (const char* forbidden : {"serial", "uuid", "asset", "part number", "sku"}) {
                BOOST_CHECK_MESSAGE(lowered.find(forbidden) == std::string::npos,
                                    "fixture carries identifying field matching '" << forbidden << "'");
            }
        }
    }
    BOOST_CHECK_EQUAL(alternatives.size(), std::variant_size_v<FieldValue>);
    BOOST_CHECK(bitmask_bool);
}

BOOST_AUTO_TEST_SUITE_END()
