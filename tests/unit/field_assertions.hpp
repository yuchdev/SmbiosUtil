#pragma once

#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <libsmbios/field_value.hpp>

#include <boost/test/unit_test.hpp>

// Shared test-support assertions for milestone 0009's get_fields() unit
// tests (Story 01.0 Tasks 02-05). Header-only and test-only: included by
// tests/unit/*_entry_test.cpp, never by libsmbios production sources.

namespace smbios {
namespace test {

/// @brief Human-readable "<alternative>:<value>" form of a FieldValue, used
/// only in Boost.Test failure messages.
inline std::string describe_field_value(const FieldValue& value)
{
    std::stringstream out;
    if (const auto* text = std::get_if<std::string>(&value)) {
        out << "string:\"" << *text << '"';
    }
    else if (const auto* number = std::get_if<uint64_t>(&value)) {
        out << "uint64:" << *number;
    }
    else if (const auto* flag = std::get_if<bool>(&value)) {
        out << "bool:" << (*flag ? "true" : "false");
    }
    else if (const auto* bytes = std::get_if<std::vector<uint8_t>>(&value)) {
        out << "bytes:[" << std::hex;
        for (size_t i = 0; i < bytes->size(); ++i) {
            out << (i ? " " : "") << static_cast<unsigned>((*bytes)[i]);
        }
        out << ']';
    }
    return out.str();
}

/// @brief Assert that no two fields share a name (contract C1 invariant).
inline void check_unique_field_names(const std::vector<NamedField>& fields)
{
    std::set<std::string> names;
    for (const auto& field : fields) {
        BOOST_CHECK_MESSAGE(names.insert(field.name).second,
                            "duplicate field name: \"" << field.name << '"');
    }
}

/// @brief Assert `actual` equals `expected` exactly: same length, and per
/// position the same name, the same FieldValue alternative and the same
/// value. Also asserts name uniqueness.
inline void check_fields_equal(const std::vector<NamedField>& actual,
                               const std::vector<NamedField>& expected)
{
    BOOST_CHECK_EQUAL(actual.size(), expected.size());
    const size_t count = actual.size() < expected.size() ? actual.size() : expected.size();
    for (size_t i = 0; i < count; ++i) {
        BOOST_CHECK_MESSAGE(actual[i].name == expected[i].name,
                            "field " << i << ": name \"" << actual[i].name
                            << "\" != expected \"" << expected[i].name << '"');
        BOOST_CHECK_MESSAGE(actual[i].value.index() == expected[i].value.index()
                                && actual[i].value == expected[i].value,
                            "field " << i << " (\"" << expected[i].name << "\"): value "
                            << describe_field_value(actual[i].value) << " != expected "
                            << describe_field_value(expected[i].value));
    }
    for (size_t i = count; i < actual.size(); ++i) {
        BOOST_ERROR("unexpected extra field " << i << ": \"" << actual[i].name << '"');
    }
    for (size_t i = count; i < expected.size(); ++i) {
        BOOST_ERROR("missing expected field " << i << ": \"" << expected[i].name << '"');
    }
    check_unique_field_names(actual);
}

/// @brief Assert no field holds a comma-joined or newline-joined string,
/// i.e. a bitmask was not flattened into prose.
inline void check_no_joined_string_fields(const std::vector<NamedField>& fields)
{
    for (const auto& field : fields) {
        if (const auto* text = std::get_if<std::string>(&field.value)) {
            BOOST_CHECK_MESSAGE(text->find('\n') == std::string::npos
                                    && text->find('\t') == std::string::npos,
                                "field \"" << field.name << "\" holds a joined string");
        }
    }
}

/// @brief Convenience: FieldValue holding a std::string (avoids the
/// const char* -> bool implicit conversion trap in brace-init lists).
inline FieldValue str(const char* text) { return FieldValue{std::string(text)}; }

/// @brief Convenience: FieldValue holding a uint64_t.
inline FieldValue num(uint64_t value) { return FieldValue{value}; }

} // namespace test
} // namespace smbios
