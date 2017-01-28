#include <iostream>
#include <string>
#include <smbios/smbios.h>
#include <smbios/memory_device_entry.h>
#include <smbios/smbios_entry_factory.h>

using namespace std;
using namespace smbios;

int main(){


    try{

        SMBios bios;
        SMBiosVersion ver = bios.get_smbios_version();
        std::cout << "DMI version: " << ver.major_version << '.' << ver.minor_version << '\n';
        std::cout << "Table size: " << bios.get_table_size() << '\n';
        std::cout << bios.render_to_description();

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
