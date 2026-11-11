/// @file main.cpp
/// @brief Example (b): read typed field values directly from the data
/// model (AbstractSMBiosEntry::get_fields()), with no output formatter.
///
/// Usage: `smbios_example_read_fields` (no arguments).
///
/// Prints the BIOS vendor, version and release date, the number of set BIOS
/// characteristics, and the locator/size/type of every memory device.
///
/// Privacy: this program deliberately never prints `Serial Number`,
/// `Asset Tag`, `Part Number` or `UUID`. Those values can identify a
/// specific machine, and this program's output lands in CI logs.
///
/// Exit status: always 0. If no SMBIOS table could be read, a notice goes to
/// stderr and nothing else is printed.

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/field_value.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

namespace {

/// @brief The classic "overloaded lambdas" helper for std::visit.
template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/// @brief Describes a FieldValue, handling every alternative explicitly.
/// std::visit cannot throw std::bad_variant_access here, unlike std::get.
std::string describe(const smbios::FieldValue& value)
{
    return std::visit(overloaded{
                          [](const std::string& s) { return s; },
                          [](uint64_t n) { return std::to_string(n); },
                          [](bool b) { return std::string(b ? "yes" : "no"); },
                          [](const std::vector<uint8_t>& bytes) { return std::to_string(bytes.size()) + " bytes"; },
                      },
                      value);
}

/// @brief Prints the fields of `fields` whose names are in `wanted`.
void print_fields(const std::vector<smbios::NamedField>& fields, std::initializer_list<const char*> wanted)
{
    for (const smbios::NamedField& field : fields) {
        for (const char* name : wanted) {
            if (field.name == name) {
                std::cout << "  " << name << ": " << describe(field.value) << '\n';
            }
        }
    }
}

} // namespace

int main()
{
    // Read the live machine's table; an empty table is a notice, not an error.
    smbios::SMBios bios;
    if (bios.get_structures_count() == 0) {
        std::cerr << "notice: no SMBIOS table could be read; nothing to print\n";
        return 0;
    }

    const smbios::SMBiosVersion version = bios.get_smbios_version();
    const uint8_t* table_end = bios.get_table_base() + bios.get_table_size();
    smbios::SMBiosEntryFactory factory;

    for (const smbios::DMIHeader& header : bios) {
        const std::unique_ptr<smbios::AbstractSMBiosEntry> entry = factory.create(header, version, table_end);
        if (!entry) {
            continue; // no entry class registered for this type
        }
        const std::vector<smbios::NamedField> fields = entry->get_fields();

        switch (header.type) {
        case smbios::SMBios::BIOSInformation: {
            std::cout << "BIOS Information\n";
            print_fields(fields, {"Vendor", "Version", "Release Date"});
            // BIOS characteristics are a bitmask: only *set* bits become
            // fields (each `name: true`), so counting bools counts set bits.
            const auto set_bits = std::count_if(fields.begin(), fields.end(), [](const smbios::NamedField& f) {
                return std::holds_alternative<bool>(f.value);
            });
            std::cout << "  Set characteristics: " << set_bits << '\n';
            break;
        }
        case smbios::SMBios::MemoryDevice:
            // Text parity: fields mirror the text output's getters, so
            // "Device size" is a std::string such as "8192 MB", not a
            // uint64_t. describe() shows the alternative; it does not parse.
            std::cout << "Memory Device\n";
            print_fields(fields, {"Device locator", "Device size", "Device type"});
            break;
        default: // every other structure type is ignored by this example
            break;
        }
    }
    return 0;
}
