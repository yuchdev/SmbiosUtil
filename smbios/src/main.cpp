#include <cstdlib>
#include <clocale>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/memory_device_entry.hpp>
#include <libsmbios/tables/baseboard_information_entry.hpp>
#include <libsmbios/tables/oem_strings_entry.hpp>
#include <libsmbios/tables/system_configuration_options_entry.hpp>
#include <libsmbios/tables/firmware_language_information_entry.hpp>
#include <libsmbios/tables/group_associations_entry.hpp>
#include <libsmbios/tables/system_event_log_entry.hpp>
#include <libsmbios/tables/memory_error_information_32bit_entry.hpp>
#include <libsmbios/tables/memory_error_information_64bit_entry.hpp>
#include <libsmbios/tables/memory_device_mapped_address_entry.hpp>
#include <libsmbios/tables/pointing_device_entry.hpp>
#include <libsmbios/tables/portable_battery_entry.hpp>
#include <libsmbios/tables/system_reset_entry.hpp>
#include <libsmbios/tables/hardware_security_entry.hpp>
#include <libsmbios/tables/system_power_controls_entry.hpp>
#include <libsmbios/tables/voltage_probe_entry.hpp>
#include <libsmbios/tables/cooling_device_entry.hpp>
#include <libsmbios/tables/temperature_probe_entry.hpp>
#include <libsmbios/tables/electrical_current_probe_entry.hpp>
#include <libsmbios/tables/out_of_band_remote_access_entry.hpp>
#include <libsmbios/tables/boot_integrity_services_entry.hpp>
#include <libsmbios/tables/management_device_entry.hpp>
#include <libsmbios/tables/management_device_component_entry.hpp>
#include <libsmbios/tables/management_device_threshold_data_entry.hpp>
#include <libsmbios/tables/memory_channel_entry.hpp>
#include <libsmbios/tables/ipmi_device_information_entry.hpp>
#include <libsmbios/tables/system_power_supply_entry.hpp>
#include <libsmbios/tables/additional_information_entry.hpp>
#include <libsmbios/tables/onboard_devices_extended_information_entry.hpp>
#include <libsmbios/tables/management_controller_host_interface_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/output_formatter.hpp>
#include <smbios/command_line_parser.hpp>

using namespace std;
using namespace smbios;

static CommandLineParams& get_params()
{
    static CommandLineParams p;
    return p;
}

void usage()
{
    cout << get_params().options_descript() << endl;
    exit(EXIT_SUCCESS);
}


void print_version()
{
    cout << "version" << endl;
    exit(EXIT_SUCCESS);
}

/// @brief Comma-joins every registered output format name, for the
/// unknown-format diagnostic.
static std::string registered_format_list()
{
    std::string list;
    for (const std::string& name : registered_output_formatter_names()) {
        if (!list.empty()) {
            list += ", ";
        }
        list += name;
    }
    return list;
}

int main(int argc, char* argv[]){

    setlocale(0, "");
    std::string dump_to_file;
    std::string read_from_file;
    const IOutputFormatter* formatter = nullptr;

    try {
        get_params().read_params(argc, argv);
        const CommandLineParams& cmd_line_params = get_params();

        if (cmd_line_params.is_help()) {
            usage();
        }

        if (cmd_line_params.is_version()) {
            print_version();
        }

        dump_to_file = cmd_line_params.dump_to_file();
        read_from_file = cmd_line_params.read_from_file();

        // Resolve --format before any SMBIOS read: an unknown name must fail
        // fast with a non-zero exit (not via usage(), which exits 0), and
        // must not first spend a possibly privileged table read.
        formatter = get_output_formatter(cmd_line_params.format());
        if (formatter == nullptr) {
            std::cerr << "Unknown output format '" << cmd_line_params.format()
                      << "'. Registered formats: " << registered_format_list() << '\n';
            return EXIT_FAILURE;
        }
    }
    // boost::program_options exception reports
    // about wrong command line parameters usage
    catch (const boost::program_options::error& e) {
        cout << "Program option error: " << e.what() << endl;
        cout << get_params().options_descript() << endl;
        usage();
    }


    try{
        SMBios bios;

        if (!read_from_file.empty()) {
            // TODO: read dump from file
            // create SMBios in empty state
            // use read_physical_memory without scanning sources
        }

        SMBiosVersion ver = bios.get_smbios_version();

        // The legacy preamble and per-header lines belong to the `text`
        // format only (ratified framing decision), keeping it byte-identical
        // to the pre-milestone output. --dump-file ignores --format, so its
        // output is unchanged too.
        const bool legacy_text = formatter->format_name() == "text" || !dump_to_file.empty();
        if (legacy_text) {
            std::cout << "DMI version: " << ver.major_version << '.' << ver.minor_version << '\n';
            std::cout << "Table size: " << bios.get_table_size() << '\n';
            std::cout << bios.render_to_description();
        }

        if (!dump_to_file.empty()) {
            std::ofstream is(dump_to_file, std::ios::binary);
            const uint8_t* table_begin = bios.get_table_base();
            size_t table_size = bios.get_table_size();
            is.write(reinterpret_cast<const char*>(table_begin), table_size);
            cout << "Dump SMBIOS table to file " << dump_to_file << ", size = " << table_size << '\n';
            return EXIT_SUCCESS;
        }

        SMBiosEntryFactory smbios_factory;
        const uint8_t* table_end = bios.get_table_base() + bios.get_table_size();
        if (legacy_text) {
            for (const DMIHeader& header : bios) {

                std::cout << "Header ID = " << header.get_type() << '\n';
                std::unique_ptr<AbstractSMBiosEntry> entry = smbios_factory.create(header, ver, table_end);
                if (entry) {
                    std::cout << formatter->render(*entry) << '\n';
                }
            }
        }
        else {
            // Structured formats: one complete document, framed by the
            // formatter's own hooks; unregistered structure types (null
            // entries) are skipped by render_document().
            std::vector<std::unique_ptr<AbstractSMBiosEntry>> entries;
            for (const DMIHeader& header : bios) {
                entries.push_back(smbios_factory.create(header, ver, table_end));
            }
            std::cout << render_document(*formatter, entries.begin(), entries.end());
        }
    }
    catch (const std::exception& e){
        std::cerr << "Unable to read SMBIOS table, exception occur: " << e.what() << '\n';
    }

    return 0;
}
