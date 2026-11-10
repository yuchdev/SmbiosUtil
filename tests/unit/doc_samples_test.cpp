#include <algorithm>
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
BOOST_AUTO_TEST_CASE(FixtureCoversEveryFieldValueAlternative)
{
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
            for (const char* forbidden : {"Serial Number", "UUID", "Asset Tag"}) {
                BOOST_CHECK_MESSAGE(field.name != forbidden, "fixture carries identifying field " << forbidden);
            }
        }
    }
    BOOST_CHECK_EQUAL(alternatives.size(), std::variant_size_v<FieldValue>);
    BOOST_CHECK(bitmask_bool);
}

BOOST_AUTO_TEST_SUITE_END()
