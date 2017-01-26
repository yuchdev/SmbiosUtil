#include <smbios/smbios_entry_factory.h>
#include <smbios/smbios.h>
#include <boost/functional/factory.hpp>
#include <boost/bind.hpp>

using namespace smbios;

SMBiosEntryFactory::SMBiosEntryFactory()
{
    entries_factory_[SMBios::BIOSInformation] = boost::bind(boost::factory<BiosInformationEntry*>(), _1, _2);
    entries_factory_[SMBios::PortConnection] = boost::bind(boost::factory<PortConnectionEntry*>(), _1, _2);
    entries_factory_[SMBios::MemoryDevice] = boost::bind(boost::factory<MemoryDeviceEntry*>(), _1, _2);
}

std::unique_ptr<AbstractSMBiosEntry> smbios::SMBiosEntryFactory::create(const DMIHeader& header, 
    const SMBiosVersion& version)
{
    try{
        return std::unique_ptr<AbstractSMBiosEntry>(entries_factory_.at(header.type)(header, version));
    }
    catch (const std::out_of_range& e){
        // no such index, just proceed
        return nullptr;
    }
}
