#include <iostream>
#include <string>
#include <smbios_utility/smbios.h>
#include <smbios_utility/memory_device.h>

using namespace std;

int main(){

    SMBios bios;
    std::cout << "DMI version: " << bios.get_smbios_version() << '\n';

    for (DMIHeader& header : bios) {

        if (header.type == SMBios::MemoryDevice) {

            if (header.length < 15)
                continue;

            MemoryDeviceV1* memory_device_structure = reinterpret_cast<MemoryDeviceV1*>(header.data);
            uint8_t memory_device_type = memory_device_structure->device_type;

            std::cout << "Memory device type = " << static_cast<unsigned short>(memory_device_type) << '\n';
        }
    }

    return 0;
}
