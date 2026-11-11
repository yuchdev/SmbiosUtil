#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <libsmbios/output_formatter.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Type: Unit - deterministic, no hardware/privileges needed.
// Scenario: milestone 0009 Story 02.0 Task 01 (contract C2) - the
// self-registering output formatter registry in isolation, against
// throwaway test-local formatters. Mirrors smbios_entry_factory_test.cpp's
// structure case-for-case: registered lookup succeeds, unregistered lookup
// returns nullptr, duplicate registration throws std::logic_error. Also: a
// namespace-qualified class name registers through the macro, and an empty
// format name throws std::invalid_argument.
// Boundaries: no built-in formatter (text/xml/json) is referenced here, so
// this suite does not depend on which of them are linked in; the
// enumeration case asserts "sorted and contains the fake", never an exact
// list.
// On failure first check: libsmbios/src/output_formatter.cpp's
// formatter_registry() (a function-local static), and that this file's
// REGISTER_OUTPUT_FORMATTER invocation still sits at namespace scope.

namespace {

/// @brief Registry key of this file's throwaway formatter; deliberately not
/// one of the built-in names (`text`, `xml`, `json`, `yaml`).
const char* const kFakeFormatName = "fake-test-format";

/// @brief A name nothing ever registers, to exercise
/// get_output_formatter()'s "no formatter found" path.
const char* const kUnregisteredFormatName = "fake-unregistered-format";

/// @brief Minimal throwaway formatter, used only to confirm the
/// registry/macro plumbing. Overrides only the two pure members, so it also
/// proves the framing hooks' empty-string defaults.
class FakeTestFormatter : public IOutputFormatter {
public:
    std::string render(const AbstractSMBiosEntry& /*entry*/) const override { return "fake-render"; }
    std::string format_name() const override { return kFakeFormatName; }
};

/// @brief Second formatter class claiming the same name as
/// FakeTestFormatter, to exercise the duplicate-registration guard. Never
/// registered through the macro.
class DuplicateNameTestFormatter : public IOutputFormatter {
public:
    std::string render(const AbstractSMBiosEntry& /*entry*/) const override { return "duplicate-render"; }
    std::string format_name() const override { return kFakeFormatName; }
};

/// @brief Formatter overriding every framing hook, to confirm overrides
/// dispatch virtually through the IOutputFormatter interface.
class FramingTestFormatter : public IOutputFormatter {
public:
    std::string render(const AbstractSMBiosEntry& /*entry*/) const override { return "entry"; }
    std::string format_name() const override { return "fake-framing-format"; }
    std::string begin_document() const override { return "["; }
    std::string entry_separator() const override { return ","; }
    std::string end_document() const override { return "]"; }
};

} // namespace

// Registered at namespace scope (static-init time), exactly as a real
// formatter's own .cpp file would do.
REGISTER_OUTPUT_FORMATTER(FakeTestFormatter)

namespace formatter_registry_test_ns {

/// @brief Registry key of the namespace-qualified throwaway formatter.
const char* const kNamespacedFormatName = "fake-namespaced-format";

/// @brief Throwaway formatter living in a named namespace, so it can only be
/// registered under its qualified name (the shape of Story 03.0's
/// third-party CSV example, e.g. `ns::CsvFormatter`).
class NamespacedTestFormatter : public smbios::IOutputFormatter {
public:
    std::string render(const smbios::AbstractSMBiosEntry& /*entry*/) const override { return "namespaced-render"; }
    std::string format_name() const override { return kNamespacedFormatName; }
};

/// @brief Formatter that forgets to name itself; never registered through
/// the macro (it would terminate at static init).
class EmptyNameTestFormatter : public smbios::IOutputFormatter {
public:
    std::string render(const smbios::AbstractSMBiosEntry& /*entry*/) const override { return {}; }
    std::string format_name() const override { return {}; }
};

} // namespace formatter_registry_test_ns

// A namespace-qualified class name used to break the macro (it pasted
// `class_name##_formatter_registered`). Also a second registration in this
// TU, proving the generated variable names do not collide.
REGISTER_OUTPUT_FORMATTER(formatter_registry_test_ns::NamespacedTestFormatter)

BOOST_AUTO_TEST_SUITE(OutputFormatterRegistryUnitTests)

/// Normal case: a formatter registered via REGISTER_OUTPUT_FORMATTER
/// resolves by name to a correctly-typed instance.
BOOST_AUTO_TEST_CASE(GetReturnsRegisteredFakeFormatterTestCase)
{
    const IOutputFormatter* formatter = get_output_formatter(kFakeFormatName);

    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const FakeTestFormatter*>(formatter) != nullptr);
    BOOST_CHECK_EQUAL(formatter->format_name(), kFakeFormatName);
}

/// Edge case: a name nothing ever registered returns nullptr, rather than
/// throwing.
BOOST_AUTO_TEST_CASE(GetReturnsNullForUnregisteredNameTestCase)
{
    BOOST_CHECK(get_output_formatter(kUnregisteredFormatName) == nullptr);
    BOOST_CHECK(get_output_formatter("") == nullptr);
}

/// Edge case: lookup is case-sensitive - an upper-cased spelling of a
/// registered name is a miss, not a match.
BOOST_AUTO_TEST_CASE(GetIsCaseSensitiveTestCase)
{
    BOOST_CHECK(get_output_formatter("FAKE-TEST-FORMAT") == nullptr);
}

/// Normal case: registered_output_formatter_names() is sorted ascending
/// and contains the fake.
BOOST_AUTO_TEST_CASE(RegisteredNamesAreSortedAndContainFakeTestCase)
{
    std::vector<std::string> names = registered_output_formatter_names();

    BOOST_CHECK(std::is_sorted(names.begin(), names.end()));
    BOOST_CHECK(std::find(names.begin(), names.end(), kFakeFormatName) != names.end());
    BOOST_CHECK(std::find(names.begin(), names.end(), kUnregisteredFormatName) == names.end());
}

/// Invalid-input case: registering an already-registered name is a
/// programming error and must throw std::logic_error, never silently
/// overwrite the first registration. Exercised via a direct call from a
/// test-case body (not a static initializer), mirroring
/// smbios_entry_factory_test.cpp's DuplicateRegistrationThrowsTestCase.
BOOST_AUTO_TEST_CASE(DuplicateRegistrationThrowsTestCase)
{
    BOOST_CHECK_THROW(register_output_formatter(std::make_unique<DuplicateNameTestFormatter>()), std::logic_error);

    // The original registration survives the rejected duplicate.
    const IOutputFormatter* formatter = get_output_formatter(kFakeFormatName);
    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const FakeTestFormatter*>(formatter) != nullptr);
}

/// Invalid-input case: the duplicate-name diagnostic names the format and
/// nothing else (threat model INFO-6: no field values in diagnostics).
BOOST_AUTO_TEST_CASE(DuplicateRegistrationMessageNamesOnlyFormatTestCase)
{
    try {
        register_output_formatter(std::make_unique<DuplicateNameTestFormatter>());
        BOOST_FAIL("expected std::logic_error");
    } catch (const std::logic_error& e) {
        BOOST_CHECK_EQUAL(std::string(e.what()),
            std::string("output formatter '") + kFakeFormatName + "' is already registered");
    }
}

/// Invalid-input case: a null formatter is rejected rather than stored
/// (and later dereferenced).
BOOST_AUTO_TEST_CASE(NullRegistrationThrowsTestCase)
{
    BOOST_CHECK_THROW(register_output_formatter(nullptr), std::invalid_argument);
}

/// Regression case: REGISTER_OUTPUT_FORMATTER accepts a namespace-qualified
/// class name, and the formatter resolves by name to that type.
BOOST_AUTO_TEST_CASE(NamespaceQualifiedRegistrationResolvesTestCase)
{
    const IOutputFormatter* formatter = get_output_formatter(formatter_registry_test_ns::kNamespacedFormatName);

    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const formatter_registry_test_ns::NamespacedTestFormatter*>(formatter) != nullptr);
    BOOST_CHECK_EQUAL(formatter->format_name(), formatter_registry_test_ns::kNamespacedFormatName);
}

/// Invalid-input case: a formatter with an empty format_name() is rejected
/// with std::invalid_argument, and nothing is stored under "".
BOOST_AUTO_TEST_CASE(EmptyFormatNameRegistrationThrowsTestCase)
{
    BOOST_CHECK_THROW(register_output_formatter(std::make_unique<formatter_registry_test_ns::EmptyNameTestFormatter>()),
                      std::invalid_argument);
    BOOST_CHECK(get_output_formatter("") == nullptr);

    const std::vector<std::string> names = registered_output_formatter_names();
    BOOST_CHECK(std::find(names.begin(), names.end(), std::string()) == names.end());
}

/// Normal case: the three framing hooks default to the empty string when a
/// formatter does not override them.
BOOST_AUTO_TEST_CASE(FramingHooksDefaultToEmptyTestCase)
{
    const IOutputFormatter* formatter = get_output_formatter(kFakeFormatName);
    BOOST_REQUIRE(formatter != nullptr);

    BOOST_CHECK_EQUAL(formatter->begin_document(), "");
    BOOST_CHECK_EQUAL(formatter->entry_separator(), "");
    BOOST_CHECK_EQUAL(formatter->end_document(), "");
}

/// Normal case: overridden framing hooks dispatch through the interface.
BOOST_AUTO_TEST_CASE(FramingHooksOverrideDispatchesVirtuallyTestCase)
{
    FramingTestFormatter concrete;
    const IOutputFormatter& formatter = concrete;

    BOOST_CHECK_EQUAL(formatter.begin_document(), "[");
    BOOST_CHECK_EQUAL(formatter.entry_separator(), ",");
    BOOST_CHECK_EQUAL(formatter.end_document(), "]");
}

BOOST_AUTO_TEST_SUITE_END()
