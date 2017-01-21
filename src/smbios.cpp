#if defined(_WIN32) || defined(_WIN64)
#include <smbios_utility/win_bios.h>
#else
#include <smbios_utility/unix_bios.h>
#endif

#include <algorithm>
#include <iostream>
#include <sstream>
#include <smbios_utility/smbios.h>
#include <smbios_utility/physical_memory.h>

using std::cout;
using std::cerr;
using std::endl;

SMBios::SMBios() : native_impl_(std::make_unique<SMBiosImpl>())
{
    static_assert(sizeof(uint8_t) == 1, "Very strange uint8_t size");
    static_assert(sizeof(uint16_t) == 2, "Very strange uint16_t size");
    static_assert(sizeof(uint32_t) == 4, "Very strange uint32_t size");

    // no one of system sources was successful, fallback to physical memory device scan
    if (!native_impl_->smbios_read_success()) {

        std::cout << "no one of system sources was successful, fallback to physical memory device scan\n";

        // call physical memory
        PhysicalMemory physical_memory_device;

        // read service memory
        std::cout << "read service memory\n";
        physical_memory_device.map_physical_memory(devmem_base_, devmem_length_);

        std::vector<uint8_t> devmem_array = physical_memory_device.get_memory_dump(0, devmem_length_);
        std::cout << "Memory dump received, size = " << devmem_array.size() << '\n';

        // scan for headers
        scan_physical_memory(devmem_array);

        // What version do we have (with some workaround)
        extract_dmi_version();

        // display if success
        display_entry_point();
    }

    if(smbios_entry32_ && checksum_validated_){
        PhysicalMemory smbios_physical_memory;
        size_t smbios_base = smbios_entry32_->structure_table_address;
        size_t smbios_table_length = smbios_entry32_->structure_table_length;
        smbios_physical_memory.map_physical_memory(smbios_base, smbios_table_length);
        native_impl_->read_from_physical_memory(smbios_physical_memory, smbios_table_length);
    }

    read_smbios_table();
}

SMBios::~SMBios()
{
}

std::string SMBios::get_smbios_version() const
{
    std::stringstream versionstr;
#if defined(_WIN32) || defined(_WIN64)
    versionstr << native_impl_->get_major_version() << '.' << native_impl_->get_minor_version();
#else
    versionstr << major_version_ << '.' << minor_version_;
#endif
    return std::move(versionstr.str());
}

size_t SMBios::get_structures_count() const
{
    return structures_count_;
}

std::vector<DMIHeader>& SMBios::get_headers_list()
{
    return headers_list_;
}

void SMBios::read_smbios_table()
{
    count_smbios_structures();

    uint8_t* table_base = native_impl_->get_table_base();
    uint8_t* table_end = table_base + native_impl_->get_table_size();
    
    size_t number_of_structures = get_structures_count();
    uint8_t* current_structure_begin = table_base;

    for (size_t i = 0; i < number_of_structures && current_structure_begin <= table_end; ++i) {

        DMIHeader header = *reinterpret_cast<DMIHeader*>(current_structure_begin);
        header.data = current_structure_begin;

        if (header.length < 4) {
            // Invalid entry length. DMI table is broken
            break;
        }

        if (header.type == SMBiosHandler::EndOfTable) {
            // end of table marker. Exit
            break;
        }

        headers_list_.push_back(header);

        // look to the current structure end '\0\0'
        current_structure_begin = current_structure_begin + header.length;
        while (current_structure_begin < table_end &&
               (current_structure_begin[0] != 0 || current_structure_begin[1] != 0)) {

            current_structure_begin++;
        }
        current_structure_begin += 2;
    }
}


void SMBios::count_smbios_structures()
{
    // start and end of the BIOS table
    uint8_t* start_table = native_impl_->get_table_base();
    uint8_t* end_table = start_table + native_impl_->get_table_size();

    //points to the actual address in the buff that's been checked
    uint8_t* offset = start_table;

    //header of the struct been read to get the length to increase the offset
    DMIHeader* header = nullptr;

    size_t structures_count = 0;

    //searches structures on the whole SMBIOS Table
    while (offset < end_table) {

        //get the header to read the length and to increase the offset
        header = reinterpret_cast<DMIHeader*>(offset);
        offset += header->length;

        structures_count++;

        // increases the offset to point to the next header that's
        // after the strings at the end of the structure.
        while ((*reinterpret_cast<uint16_t*>(offset) != 0) && (offset < end_table)) {
            offset++;
        }

        // Points to the next structure that after two null bytes
        // at the end of the strings section
        offset += 2;
    }

    structures_count_ = structures_count;
}

void SMBios::scan_physical_memory(const std::vector<uint8_t> &devmem_array)
{
    uint8_t smbios_32_header[] = {'_','S','M','_'};
    uint8_t smbios_64_header[] = {'_','S','M','3','_'};
    constexpr size_t smbios32_header_size = sizeof(SMBIOSEntryPoint32);
    constexpr size_t smbios64_header_size = sizeof(SMBIOSEntryPoint64);

    // perform scanning here
    unsigned long checksum = 0;

    for (auto it = devmem_array.begin(); std::distance(it, devmem_array.end()) > 16; it += 16) {

        checksum += (*it);
        if(std::equal(std::begin(smbios_32_header), std::end(smbios_32_header), it)){

            std::cout << "32-bit SMBIOS header found\n";
            entry_point_buffer_.assign(it, it + smbios32_header_size);
            smbios_entry32_ = reinterpret_cast<const SMBIOSEntryPoint32*>(&entry_point_buffer_[0]);

            // TODO: checksum
        }

        if(std::equal(std::begin(smbios_64_header), std::end(smbios_64_header), it)){

            std::cout << "64-bit SMBIOS header found\n";
            entry_point_buffer_.assign(it, it + smbios64_header_size);
            smbios_entry64_ = reinterpret_cast<const SMBIOSEntryPoint64*>(&entry_point_buffer_[0]);

            // TODO: checksum
        }

    }
    std::cout << "checksum = " << checksum << std::endl;
}

void SMBios::extract_dmi_version()
{
    if(smbios_entry32_ && checksum_validated_) {
        major_version_ = static_cast<size_t>(smbios_entry32_->major_version);
        minor_version_ = static_cast<size_t>(smbios_entry32_->minor_version);
    }
    if(smbios_entry64_ && checksum_validated_) {
        major_version_ = static_cast<size_t>(smbios_entry64_->major_version);
        minor_version_ = static_cast<size_t>(smbios_entry64_->minor_version);
    }
}

void SMBios::display_entry_point() const
{
    if(smbios_entry32_ && checksum_validated_) {

        cout << "SMBIOS checksum: " << static_cast<size_t>(smbios_entry32_->entry_point_checksum) << '\n';
        cout << "SMBIOS length: " << static_cast<size_t>(smbios_entry32_->entry_point_length) << '\n';
        cout << "SMBIOS major version: " << static_cast<size_t>(smbios_entry32_->major_version) << '\n';
        cout << "SMBIOS minor version: " << static_cast<size_t>(smbios_entry32_->minor_version) << '\n';
        cout << "Maximum structure size: " << smbios_entry32_->max_structure_size << '\n';
        cout << "Entry point revision: " << static_cast<size_t>(smbios_entry32_->entry_point_revision) << '\n';
        cout << "SMBIOS intermediate checksum: " << static_cast<size_t>(smbios_entry32_->intermediate_checksum) << '\n';
        cout << "Structure table length: " << smbios_entry32_->structure_table_length << '\n';
        cout << "Table address: " << std::hex << smbios_entry32_->structure_table_address << std::dec << '\n';
        cout << "SMBIOS structures count: " << smbios_entry32_->smbios_structures_number << '\n';
        cout << "SMBIOS BCD revision: " << static_cast<size_t>(smbios_entry32_->smbios_bcd_revision) << '\n';

    }

    if(smbios_entry64_ && checksum_validated_) {

        cout << "SMBIOS checksum: " << static_cast<size_t>(smbios_entry64_->entry_point_checksum) << '\n';
        cout << "SMBIOS length: " << static_cast<size_t>(smbios_entry64_->entry_point_length) << '\n';
        cout << "SMBIOS major version: " << static_cast<size_t>(smbios_entry64_->major_version) << '\n';
        cout << "SMBIOS minor version: " << static_cast<size_t>(smbios_entry32_->minor_version) << '\n';
        cout << "SMBIOS doc version: " << static_cast<size_t>(smbios_entry64_->smbios_docrev) << '\n';
        cout << "Reserved byte: " <<  static_cast<size_t>(smbios_entry64_->reserved) << '\n';
        cout << "Maximum structure size: " << smbios_entry64_->max_structure_size << '\n';
        cout << "Table address: " << std::hex << smbios_entry64_->structure_table_address << std::dec << '\n';
    }
}
