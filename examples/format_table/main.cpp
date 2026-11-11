/// @file main.cpp
/// @brief Example (a): render the live machine's SMBIOS table in any
/// registered output format, using only libsmbios's public API.
///
/// Usage: `smbios_example_format_table <format-name>`, for example
/// `smbios_example_format_table json`.
///
/// Which format names exist depends on which formatter objects are linked
/// into *this* binary: the built-ins (`text`, `xml`, `json`, `yaml`) live in
/// libsmbios and register at static-init time, which is why
/// examples/CMakeLists.txt links libsmbios as a whole archive. `csv` is
/// **not** available here - it is an external example formatter compiled
/// into the `smbios` CLI only (smbios/examples/csv_formatter.cpp).
///
/// Exit status: 0 on success, including when no SMBIOS table could be read
/// (an empty, well-formed document is written and a notice goes to stderr);
/// 2 on a missing or unknown format name.

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

// abstract_smbios_entry.hpp is needed only for the complete
// AbstractSMBiosEntry type, which std::unique_ptr requires to destroy the
// entries SMBiosEntryFactory::create() returns.
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

int main(int argc, char* argv[])
{
    // Step 1: resolve the formatter *before* reading SMBIOS. An unknown
    // name then fails fast, without spending a possibly privileged table
    // read - the same rule the smbios CLI follows for --format.
    const std::string name = argc > 1 ? argv[1] : "";
    const smbios::IOutputFormatter* formatter = smbios::get_output_formatter(name);
    if (formatter == nullptr) {
        std::string names;
        for (const std::string& registered : smbios::registered_output_formatter_names()) {
            names += (names.empty() ? "" : ", ") + registered;
        }
        std::cerr << "Unknown output format '" << name << "'. Registered formats: " << names << '\n';
        return 2;
    }

    // Step 2: read the live machine's table. The constructor never throws
    // for "no table": it leaves the object empty, so degrade to an empty
    // document instead of failing (unprivileged Linux, some VMs).
    smbios::SMBios bios;
    if (bios.get_structures_count() == 0) {
        std::cerr << "notice: no SMBIOS table could be read; writing an empty document\n";
    }

    // Step 3: decode each structure into a typed entry and frame the
    // document with the formatter's own hooks, so the result is one
    // well-formed document for zero, one or many entries.
    const smbios::SMBiosVersion version = bios.get_smbios_version();
    const uint8_t* table_end = bios.get_table_base() + bios.get_table_size();
    smbios::SMBiosEntryFactory factory;
    bool first = true;
    std::cout << formatter->begin_document();
    for (const smbios::DMIHeader& header : bios) {
        // nullptr means "no entry class registered for this type": skip it.
        const std::unique_ptr<smbios::AbstractSMBiosEntry> entry = factory.create(header, version, table_end);
        if (!entry) {
            continue;
        }
        // The separator goes *between* rendered entries, never before the
        // first one (a JSON array must not start with a comma).
        if (!first) {
            std::cout << formatter->entry_separator();
        }
        std::cout << formatter->render(*entry);
        first = false;
    }
    std::cout << formatter->end_document();
    return 0;
}
