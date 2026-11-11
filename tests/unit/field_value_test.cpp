#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/field_value.hpp>
#include <libsmbios/smbios.hpp>

#include <boost/test/unit_test.hpp>

using namespace smbios;

// Unit tests (milestone 0009, Story 01.0, Task 01)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: contract C1's FieldValue/NamedField hold each variant
// alternative as expected; AbstractSMBiosEntry::bitset_to_fields() turns a
// bitmask into one {label, true} field per set, labelled bit in ascending
// bit order. (get_fields() is pure virtual since Story 01.0 Task 05, so
// FieldTestEntry carries its own trivial override.)
// Boundaries: no bits set, unlabelled set bits, highest bit of the type.
// On failure first check: libsmbios/include/libsmbios/field_value.hpp and
// bitset_to_fields() in libsmbios/include/libsmbios/abstract_smbios_entry.hpp
// (loop order / label lookup).

BOOST_AUTO_TEST_SUITE(FieldValueUnitTests)

namespace {

/// @brief Minimal concrete AbstractSMBiosEntry exposing the protected
/// bitset_to_fields() helper. Built with no backing buffer (header.data
/// nullptr, table_end nullptr), which the base constructor permits.
class FieldTestEntry : public AbstractSMBiosEntry {
public:
    FieldTestEntry() : AbstractSMBiosEntry(DMIHeader{}, nullptr) {}

    std::string get_type() const override { return "FieldTest"; }
    std::string render_to_description() const override { return ""; }
    std::vector<NamedField> get_fields() const override { return {}; }

    using AbstractSMBiosEntry::bitset_to_fields;
};

} // namespace

/// Normal case: every FieldValue alternative round-trips through NamedField.
BOOST_AUTO_TEST_CASE(NamedFieldHoldsEachAlternativeTestCase)
{
    NamedField text{"Vendor", std::string("Acme")};
    BOOST_REQUIRE(std::holds_alternative<std::string>(text.value));
    BOOST_CHECK_EQUAL(text.name, "Vendor");
    BOOST_CHECK_EQUAL(std::get<std::string>(text.value), "Acme");

    NamedField number{"Size", uint64_t{0xFFFFFFFFFFFFFFFFull}};
    BOOST_REQUIRE(std::holds_alternative<uint64_t>(number.value));
    BOOST_CHECK_EQUAL(std::get<uint64_t>(number.value), 0xFFFFFFFFFFFFFFFFull);

    NamedField flag{"PCI is supported", true};
    BOOST_REQUIRE(std::holds_alternative<bool>(flag.value));
    BOOST_CHECK(std::get<bool>(flag.value));

    const std::vector<uint8_t> raw{0x00, 0x01, 0xAB, 0xFF};
    NamedField bytes{"Processor ID", raw};
    BOOST_REQUIRE(std::holds_alternative<std::vector<uint8_t>>(bytes.value));
    BOOST_CHECK(std::get<std::vector<uint8_t>>(bytes.value) == raw);
}

/// Security case (Story 02.0 Task 03, LOW-5): a string value stays a
/// string. The project convention `NamedField{"x", std::string("y")}` must
/// hold the std::string alternative; and a bare literal - which a
/// pre-P0608/P1957 standard library would bind to `bool` (value true) -
/// must also select std::string under this toolchain.
/// On failure: the standard library predates P1957; wrap every literal in
/// std::string(...) (FieldsTestEntry's rule) or add a `const char*`
/// NamedField constructor, per the threat model's LOW-5 mitigation.
BOOST_AUTO_TEST_CASE(NamedFieldStringLiteralStaysStringTestCase)
{
    NamedField wrapped{"x", std::string("y")};
    BOOST_REQUIRE(std::holds_alternative<std::string>(wrapped.value));
    BOOST_CHECK_EQUAL(std::get<std::string>(wrapped.value), "y");

    NamedField bare{"x", "y"};
    BOOST_REQUIRE(std::holds_alternative<std::string>(bare.value));
    BOOST_CHECK(!std::holds_alternative<bool>(bare.value));
    BOOST_CHECK_EQUAL(std::get<std::string>(bare.value), "y");
}

/// Invalid-access case: std::get with the wrong alternative throws.
BOOST_AUTO_TEST_CASE(NamedFieldWrongAlternativeThrowsTestCase)
{
    NamedField flag{"Flag", false};
    BOOST_CHECK(!std::holds_alternative<std::string>(flag.value));
    BOOST_CHECK(!std::holds_alternative<uint64_t>(flag.value));
    BOOST_CHECK_THROW(std::get<std::string>(flag.value), std::bad_variant_access);
}

/// Normal case: one true field per set, labelled bit, ascending order;
/// set bits without a label are skipped; unset labelled bits are skipped.
BOOST_AUTO_TEST_CASE(BitsetToFieldsAscendingAndSkipsUnlabelledTestCase)
{
    const std::map<uint16_t, std::string> labels{
        {0x0001, "Bit0"}, {0x0004, "Bit2"}, {0x0010, "Bit4"}, {0x8000, "Bit15"}};
    // Set: bit0, bit1 (no label), bit4, bit15. Bit2 is labelled but unset.
    const uint16_t properties = 0x0001 | 0x0002 | 0x0010 | 0x8000;

    FieldTestEntry entry;
    std::vector<NamedField> out;
    entry.bitset_to_fields(properties, labels, out);

    BOOST_REQUIRE_EQUAL(out.size(), 3u);
    const char* expected[] = {"Bit0", "Bit4", "Bit15"};
    for (size_t i = 0; i < out.size(); ++i) {
        BOOST_CHECK_EQUAL(out[i].name, expected[i]);
        BOOST_REQUIRE(std::holds_alternative<bool>(out[i].value));
        BOOST_CHECK(std::get<bool>(out[i].value));
    }
}

/// Edge case: no bits set yields nothing, and existing contents of out are kept.
BOOST_AUTO_TEST_CASE(BitsetToFieldsNoBitsAppendsNothingTestCase)
{
    const std::map<uint64_t, std::string> labels{{0x1, "Low"}, {0x8000000000000000ull, "High"}};
    FieldTestEntry entry;
    std::vector<NamedField> out{NamedField{"Existing", uint64_t{7}}};

    entry.bitset_to_fields(uint64_t{0}, labels, out);
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_CHECK_EQUAL(out[0].name, "Existing");

    entry.bitset_to_fields(uint64_t{0x8000000000000001ull}, labels, out);
    BOOST_REQUIRE_EQUAL(out.size(), 3u);
    BOOST_CHECK_EQUAL(out[1].name, "Low");
    BOOST_CHECK_EQUAL(out[2].name, "High");
}

BOOST_AUTO_TEST_SUITE_END()
