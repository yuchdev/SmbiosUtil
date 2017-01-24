#include <iostream>
#include <string>
#include <smbios/smbios.h>
#include <smbios/memory_device_entry.h>

using namespace std;
using namespace smbios;

int main(){

    SMBios bios;
    std::cout << "DMI version: " << bios.get_smbios_version() << '\n';

    for (const DMIHeader& header : bios) {

		std::cout << "Header type = " << static_cast<unsigned short>(header.type) << '\n';

        if (header.type == SMBios::MemoryDevice) {

            if (header.length < 15)
                continue;

            MemoryDeviceEntry memory_device_structure(header);
            std::cout << memory_device_structure.render_to_description() << '\n';
        }
    }

    return 0;
}
