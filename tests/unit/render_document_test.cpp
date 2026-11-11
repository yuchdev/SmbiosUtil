#include <memory>
#include <string>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/formatters/json_formatter.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using smbios::test::FieldsTestEntry;

// Unit tests (milestone 0009, Story 02.0 review fixes)
//
// Type: Unit - deterministic, no hardware/privileges needed.
// Scenario: smbios::render_document() (output_formatter.hpp) frames a
// complete document as begin_document(), each render() with
// entry_separator() between consecutive rendered entries, then
// end_document() - for both the std::vector<const AbstractSMBiosEntry*>
// overload and the iterator overload over owned
// std::vector<std::unique_ptr<AbstractSMBiosEntry>> (the shape
// SMBiosEntryFactory::create() results take in smbios/src/main.cpp).
// Boundaries: zero, one and many entries; a formatter with all three hooks
// overridden and one keeping the empty defaults; null elements (skipped,
// never producing a separator) at the start, middle and end.
// On failure first check: the render_document() template in
// libsmbios/include/libsmbios/output_formatter.hpp (separator placement and
// the null-skip), then the vector overload in output_formatter.cpp.

namespace {

/// @brief Test-local formatter (never registered) overriding every framing
/// hook with a distinct marker; render() emits the entry's type label.
class HookedTestFormatter : public IOutputFormatter {
public:
    std::string render(const AbstractSMBiosEntry& entry) const override { return entry.get_type(); }
    std::string format_name() const override { return "render-document-hooked"; }
    std::string begin_document() const override { return "<"; }
    std::string entry_separator() const override { return "|"; }
    std::string end_document() const override { return ">"; }
};

/// @brief Test-local formatter (never registered) keeping every framing
/// hook's empty default.
class PlainTestFormatter : public IOutputFormatter {
public:
    std::string render(const AbstractSMBiosEntry& entry) const override { return entry.get_type(); }
    std::string format_name() const override { return "render-document-plain"; }
};

/// @brief An owned entry whose get_type() is `type` and which has no fields.
std::unique_ptr<AbstractSMBiosEntry> make_entry(const std::string& type)
{
    return std::make_unique<FieldsTestEntry>(type, std::vector<NamedField>{});
}

} // namespace

BOOST_AUTO_TEST_SUITE(RenderDocumentUnitTests)

/// Edge case: zero entries yield exactly begin_document() + end_document().
BOOST_AUTO_TEST_CASE(RenderDocumentZeroEntriesTestCase)
{
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), {}), "<>");
    BOOST_CHECK_EQUAL(render_document(PlainTestFormatter(), {}), "");
}

/// Normal case: one entry is framed by the hooks with no separator.
BOOST_AUTO_TEST_CASE(RenderDocumentOneEntryTestCase)
{
    FieldsTestEntry a("A", {});
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), {&a}), "<A>");
    BOOST_CHECK_EQUAL(render_document(PlainTestFormatter(), {&a}), "A");
}

/// Normal case: many entries, in order, with the separator only between
/// consecutive entries.
BOOST_AUTO_TEST_CASE(RenderDocumentManyEntriesTestCase)
{
    FieldsTestEntry a("A", {});
    FieldsTestEntry b("B", {});
    FieldsTestEntry c("C", {});
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), {&a, &b, &c}), "<A|B|C>");
    BOOST_CHECK_EQUAL(render_document(PlainTestFormatter(), {&a, &b, &c}), "ABC");
}

/// Normal case: owned entries (as collected from SMBiosEntryFactory::create())
/// render through the iterator overload exactly like borrowed pointers.
BOOST_AUTO_TEST_CASE(RenderDocumentOwnedEntriesThroughIteratorsTestCase)
{
    std::vector<std::unique_ptr<AbstractSMBiosEntry>> owned;
    owned.push_back(make_entry("A"));
    owned.push_back(make_entry("B"));

    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), owned.begin(), owned.end()), "<A|B>");
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), owned.begin(), owned.begin()), "<>");
    BOOST_CHECK_EQUAL(render_document(PlainTestFormatter(), owned.cbegin(), owned.cend()), "AB");
}

/// Edge case: null elements (the factory's result for an unregistered type)
/// are skipped wherever they sit and never produce a separator, so the
/// output equals the null-free document.
BOOST_AUTO_TEST_CASE(RenderDocumentSkipsNullEntriesTestCase)
{
    std::vector<std::unique_ptr<AbstractSMBiosEntry>> owned;
    owned.push_back(nullptr);
    owned.push_back(make_entry("A"));
    owned.push_back(nullptr);
    owned.push_back(nullptr);
    owned.push_back(make_entry("B"));
    owned.push_back(nullptr);
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), owned.begin(), owned.end()), "<A|B>");

    FieldsTestEntry a("A", {});
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), {nullptr}), "<>");
    BOOST_CHECK_EQUAL(render_document(HookedTestFormatter(), {nullptr, &a, nullptr}), "<A>");
}

/// Normal case: with a real formatter the result is exactly the manual
/// begin/render/separator/end assembly the CLI would otherwise hand-roll.
BOOST_AUTO_TEST_CASE(RenderDocumentMatchesManualFramingForJsonTestCase)
{
    const JsonFormatter json;
    FieldsTestEntry a("A", {NamedField{"F", uint64_t{1}}});
    FieldsTestEntry b("B", {});

    const std::string manual = json.begin_document() + json.render(a) + json.entry_separator() + json.render(b)
                             + json.end_document();
    BOOST_CHECK_EQUAL(render_document(json, {&a, &b}), manual);
    BOOST_CHECK_EQUAL(render_document(json, {}), "[\n\n]\n");
    BOOST_CHECK(smbios::test::is_well_formed_json(render_document(json, {&a, &b})));
}

BOOST_AUTO_TEST_SUITE_END()
