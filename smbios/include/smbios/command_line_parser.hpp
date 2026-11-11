#pragma once
#include <memory>
#include <string>
#include <boost/program_options.hpp>

// The header contains command-line parser for the SMBIOS command line utility
// It uses Boost.ProgramOptions

namespace smbios {

/// @brief Storage for all passed and default command-line params
class CommandLineParams {
public:

    /**@brief Should have a constructor by default */
    CommandLineParams();

    /**@brief Parse raw command-line parameters */
    void read_params(int argc, char* argv[]);

    /**@brief Set some logical param */
    void set_flag(const boost::program_options::variables_map& vm, bool& flag, const char* str);

    /**@brief Get available options list*/
    boost::program_options::options_description& options_descript(){ return cmd_options_description; }

    // Accessors

    bool is_help() const{
        return _help;
    }

    bool is_version() const {
        return _version;
    }

    bool is_memory_scan() const {
        return _memory_scan;
    }

    const std::string& read_from_file() const {
        return _from_file;
    }

    const std::string& dump_to_file() const {
        return _to_file;
    }

    /// @brief The requested output format name (`--format`/`-f`).
    /// @return the name exactly as given on the command line, or `"text"`
    /// when the option is absent. Not validated here: the caller resolves it
    /// with smbios::get_output_formatter() and rejects an unknown name.
    const std::string& format() const {
        return _format;
    }


private:

    /// Show help
    bool _help = false;

    /// Show version
    bool _version = false;

    /// Fallback right to memory scan
    bool _memory_scan = false;

    /// This file should contain SMBios dump
    std::string _from_file;

    /// Dump SMBios to that file
    std::string _to_file;

    /// Output format name, a key of the output-formatter registry
    std::string _format;

    /// Command-line params description
    boost::program_options::options_description cmd_options_description;
};

} // namespace smbios
