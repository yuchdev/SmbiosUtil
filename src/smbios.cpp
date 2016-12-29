#if defined(_WIN32) || defined(_WIN64)
#include <smbios_utility/win_bios.h>
#else
// #include posix/macos
#endif

#include <sstream>
#include <smbios_utility/smbios.h>

SMBios::SMBios() : native_impl_(std::make_unique<SMBiosImpl>())
{
    static_assert(sizeof(uint8_t) == 1, "Very strange uint8_t size");
    static_assert(sizeof(uint16_t) == 2, "Very strange uint16_t size");
    static_assert(sizeof(uint32_t) == 4, "Very strange uint32_t size");

    read_smbios_table();
}

SMBios::~SMBios()
{
}

std::string SMBios::get_smbios_version() const
{
    std::stringstream versionstr;
    versionstr << native_impl_->get_major_version() << '.' << native_impl_->get_minor_version();
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

    RawSMBIOSData* smbios_data = native_impl_->get_formatted_smbios_table();
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
        while (current_structure_begin < table_end && (current_structure_begin[0] != 0 || current_structure_begin[1] != 0)) {
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
