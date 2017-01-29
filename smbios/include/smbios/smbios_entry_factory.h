#pragma once
#include <cstdint>
#include <memory>
#include <map>
#include <boost/function.hpp>

#include <smbios/bios_information_entry.h>
#include <smbios/port_connection_entry.h>
#include <smbios/memory_device_entry.h>

namespace smbios {

class AbstractSMBiosEntry;
class DMIHeader;
class SMBiosVersion;

/// @brief Abstract factory class. Generates SMBIOS headers according to header and version provided
/// Header represent type, version usually helps to know the amount of useful information in the structure
/// Based on Boost.Functional and Boost.Factory
class SMBiosEntryFactory{
public:

    /// @brief Init factory map
    SMBiosEntryFactory();

    /// @brief Create concrete instance of the SMBIOS entry
    std::unique_ptr<AbstractSMBiosEntry> create(const DMIHeader&, const SMBiosVersion&);

private:

    /// Map SMBIOS header types to class instance generators
    std::map<uint8_t, boost::function<AbstractSMBiosEntry*(const DMIHeader&, const SMBiosVersion&)>> entries_factory_;
};

} // namespace smbios