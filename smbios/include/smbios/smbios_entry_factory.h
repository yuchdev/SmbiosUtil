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

class SMBiosEntryFactory{
public:

    SMBiosEntryFactory();

    std::unique_ptr<AbstractSMBiosEntry> create(const DMIHeader&, const SMBiosVersion&);

private:
    std::map<uint8_t, boost::function<AbstractSMBiosEntry*(const DMIHeader&, const SMBiosVersion&)>> entries_factory_;
};

} // namespace smbios