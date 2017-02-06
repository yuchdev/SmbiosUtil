#include <iostream>
#include <string>
#include <fstream>
#include <smbios/smbios.h>
#include <smbios/memory_device_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios_util/command_line_parser.h>

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

int main(int argc, char* argv[]){

    setlocale(0, "");
    std::string dump_to_file;
    std::string read_from_file;

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
    }
    // boost::program_options exception reports
    // about wrong command line parameters usage
    catch (const boost::program_options::error& e) {
        cout << "Program option error: " << e.what() << endl;
        cout << get_params().options_descript() << endl;
        usage();
    }

    if (!read_from_file.empty()) {
        // TODO: read dump from file
    }

    try{
        SMBios bios;
        SMBiosVersion ver = bios.get_smbios_version();
        std::cout << "DMI version: " << ver.major_version << '.' << ver.minor_version << '\n';
        std::cout << "Table size: " << bios.get_table_size() << '\n';
        std::cout << bios.render_to_description();

        if (!dump_to_file.empty()) {
            std::basic_ofstream<uint8_t, std::char_traits<uint8_t>> is(dump_to_file, std::ios::binary);
            const uint8_t* table_begin = bios.get_table_base();
            size_t table_size = bios.get_table_size();
            is.write(table_begin, table_size);
            cout << "Dump SMBIOS table to file " << dump_to_file << ", size = " << table_size << '\n';
            return EXIT_SUCCESS;
        }

        SMBiosEntryFactory smbios_factory;
        for (const DMIHeader& header : bios) {

            std::cout << "Header ID = " << header.get_type() << '\n';
            std::unique_ptr<AbstractSMBiosEntry> entry = smbios_factory.create(header, ver);
            if (entry) {
                std::cout << entry->render_to_description() << '\n';
            }
        }
    }
    catch (const std::exception& e){
        std::cerr << "Unable to read SMBIOS table, exception occur: " << e.what() << '\n';
    }

    return 0;
}
