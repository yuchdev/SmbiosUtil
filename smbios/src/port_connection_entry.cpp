#include <smbios/port_connection_entry.h>
#include <smbios/smbios.h>

#include <sstream>
#include <cassert>

using std::string;
using namespace smbios;

PortConnectionEntry::PortConnectionEntry(const DMIHeader& header) {

    if (header.type != SMBios::PortConnection) {
        std::stringstream err;
        err << "Wrong device type, expected Port Connection, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }
    port_connection_ = reinterpret_cast<const PortConnection*>(header.data);

    init_string_values();
}

PortConnectionEntry::~PortConnectionEntry()
{

}


void PortConnectionEntry::init_string_values()
{
    connection_type_map_[] = "Not Provided";
    connection_type_map_[] = "No Error";

    port_type_map_[] = "Unknown";
    port_type_map_[] = "Unknown";
}
