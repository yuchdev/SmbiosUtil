#include <string>
#include <list>
#include <iostream>
#include <algorithm>
#include <smbios/smbios.h>

using namespace smbios;

namespace po = boost::program_options;

using std::string;

CommandLineParams::CommandLineParams() : cmd_options_description("SMBios utility console has the following options:")
{}

void CommandLineParams::read_params(int argc, char* argv[])
{
    cmd_options_description.add_options()
        ("help,h", "Print usage")
        ("version,V", "Print version")
        ("memory-scan,m", "Fallback to memory scan without trying EFI or SysFS (Linux only)")
        ("read-file,r", po::value<string>(&_from_file), "Log to file")
        ("dump-file,d", po::value<string>(&_to_file), "Logging severity (fatal, error, warning, info, debug, trace)")
        ;

    // command line params processing
    po::variables_map cmd_variables_map;
    po::store(parse_command_line(argc, argv, cmd_options_description), cmd_variables_map);
    po::notify(cmd_variables_map);

    set_flag(cmd_variables_map, _help, "help");
    set_flag(cmd_variables_map, _version, "version");
    set_flag(cmd_variables_map, _memory_scan, "memory-scan");

    // do not check debug flags!
    std::list<bool> mutually_exclusives = { _help, _version, _memory_scan };
    size_t options_count = std::count(mutually_exclusives.begin(), mutually_exclusives.end(), true);
    if (options_count > 1){
        throw std::logic_error("Incompatible command line parameters set, use only one");
    }

}

/**@brief Set some logical param */
void CommandLineParams::set_flag(const po::variables_map& vm, bool& flag, const char* str)
{
    if (vm.count(str)) {
        flag = true;
    }
}
