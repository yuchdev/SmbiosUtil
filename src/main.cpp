#include <iostream>
#include <string>
#include <smbios_utility/smbios.h>
#include <smbios_utility/memory_device_entry.h>

using namespace std;

int main(){

    SMBios bios;
    std::cout << "DMI version: " << bios.get_smbios_version() << '\n';

    for (const DMIHeader& header : bios) {

		std::cout << "Header type = " << static_cast<unsigned short>(header.type) << '\n';

        if (header.type == SMBios::MemoryDevice) {

            if (header.length < 15)
                continue;

            MemoryDeviceParser memory_device_structure(header);

            if (MemoryDeviceParser::DeviceTypeUnknown == memory_device_structure.get_device_type()) {
                std::cout << "Memory device type unknown\n";
            }

            if (MemoryDeviceParser::DRAM == memory_device_structure.get_device_type()) {
                std::cout << "Memory device type = DRAM\n";
            }

            if (MemoryDeviceParser::DDR3 == memory_device_structure.get_device_type()) {
                std::cout << "Memory device type = DDR3\n";
            }

        }
    }

    return 0;
}
