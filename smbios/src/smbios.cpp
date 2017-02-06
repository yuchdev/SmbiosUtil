#if defined(_WIN32) || defined(_WIN64)
#include <smbios/win_bios.h>
#else
#include <smbios/unix_bios.h>
#endif

#include <limits>
#include <algorithm>
#include <sstream>
#include <smbios/smbios.h>
#include <smbios/smbios_anchor.h>
#include <smbios/physical_memory.h>

// DEBUG
#include <iostream>

using std::numeric_limits;
using namespace smbios;

bool smbios::operator>(const SMBiosVersion& lhs, const SMBiosVersion& rhs)
{
    if (lhs.major_version > rhs.major_version) {
        return true;
    }
    if (lhs.minor_version > rhs.minor_version) {
        return true;
    }
    return false;
}

bool smbios::operator<(const SMBiosVersion& lhs, const SMBiosVersion& rhs)
{
    if (lhs.major_version < rhs.major_version) {
        return true;
    }
    if (lhs.minor_version < rhs.minor_version) {
        return true;
    }
    return false;
}


SMBios::SMBios() : native_impl_(std::make_unique<SMBiosImpl>())
{
    static_assert(sizeof(uint8_t) == 1, "Very strange uint8_t size");
    static_assert(sizeof(uint16_t) == 2, "Very strange uint16_t size");
    static_assert(sizeof(uint32_t) == 4, "Very strange uint32_t size");

    // no one of system sources was successful, fallback to physical memory device scan
    if (!native_impl_->smbios_read_success()) {

        // call physical memory
        smbios::PhysicalMemory physical_memory_device;

        // read service memory
        physical_memory_device.map_physical_memory(devmem_base_, devmem_length_);

        std::vector<uint8_t> devmem_array = physical_memory_device.get_memory_dump(0, devmem_length_);

        // scan for headers
        scan_physical_memory(devmem_array);

        // What version do we have (TODO: with some workaround)
        extract_dmi_version();

        size_t smbios_base{};
        size_t smbios_table_length{};

        if(smbios_entry32_ && checksum_validated_){
            smbios_base = smbios_entry32_->structure_table_address;
            smbios_table_length = smbios_entry32_->structure_table_length;
        }

        if(smbios_entry64_ && checksum_validated_){
            smbios_base = smbios_entry64_->structure_table_address;
            smbios_table_length = smbios_entry64_->max_structure_size;
        }
        smbios::PhysicalMemory smbios_physical_memory;
        smbios_physical_memory.map_physical_memory(smbios_base, smbios_table_length);
        std::vector<uint8_t> dump = smbios_physical_memory.get_memory_dump(0, smbios_table_length);
        native_impl_->read_from_physical_memory(dump);
    }
    else{
        // no need to validate checksum, performed by native implementation
        checksum_validated_ = true;
    }

    read_smbios_table();
}

SMBios::~SMBios()
{
}

SMBiosVersion SMBios::get_smbios_version() const
{
    SMBiosVersion ver;
    size_t major_version = native_impl_->get_major_version();
    size_t minor_version = native_impl_->get_minor_version();

    // native implementation provides version
    if (numeric_limits<size_t>::max() != major_version && numeric_limits<size_t>::max() != minor_version) {
        ver.major_version = major_version;
        ver.minor_version = minor_version;
    }
    // Low-level table provides version
    else {
        ver.major_version = major_version_;
        ver.minor_version = minor_version_;
    }

    return ver;
}

size_t SMBios::get_structures_count() const
{
    return structures_count_;
}

const uint8_t *SMBios::get_table_base() const
{
    if(native_impl_ && native_impl_->get_table_base()){
        return native_impl_->get_table_base();
    }
    if(smbios_entry32_ && checksum_validated_){
        // convert from intptr_t
        return reinterpret_cast<uint8_t *>(smbios_entry32_->structure_table_address);
    }
    if(smbios_entry64_ && checksum_validated_){
        // convert from intptr_t
        return reinterpret_cast<uint8_t *>(smbios_entry64_->structure_table_address);
    }
    return nullptr;
}

size_t SMBios::get_table_size() const
{
    if(native_impl_ && native_impl_->get_table_size()){
        return native_impl_->get_table_size();
    }
    if(smbios_entry32_ && checksum_validated_){
        return smbios_entry32_->structure_table_length;
    }
    if(smbios_entry64_ && checksum_validated_){
        return smbios_entry64_->max_structure_size;
    }
    return 0;
}

std::vector<DMIHeader>& SMBios::get_headers_list()
{
    return headers_list_;
}

void SMBios::read_smbios_table()
{
    count_smbios_structures();

    const uint8_t* table_base = get_table_base();
    const uint8_t* table_end = table_base + get_table_size();
    
    size_t number_of_structures = get_structures_count();
    const uint8_t* current_structure_begin = table_base;

    for (size_t i = 0; i < number_of_structures && current_structure_begin <= table_end; ++i) {

        DMIHeader header = *reinterpret_cast<const DMIHeader*>(current_structure_begin);
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
    const uint8_t* start_table = get_table_base();
    const uint8_t* end_table = start_table + get_table_size();

    //points to the actual address in the buff that's been checked
    const uint8_t* offset = start_table;

    //header of the struct been read to get the length to increase the offset
    const DMIHeader* header = nullptr;

    size_t structures_count = 0;

    //searches structures on the whole SMBIOS Table
    while (offset < end_table) {

        //get the header to read the length and to increase the offset
        header = reinterpret_cast<const DMIHeader*>(offset);
        offset += header->length;

        structures_count++;

        // increases the offset to point to the next header that's
        // after the strings at the end of the structure.
        while ((*reinterpret_cast<const uint16_t*>(offset) != 0) && (offset < end_table)) {
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
    constexpr size_t smbios32_header_size = sizeof(SMBIOSEntryPoint32);
    constexpr size_t smbios64_header_size = sizeof(SMBIOSEntryPoint64);

    for (auto it = devmem_array.begin(); std::distance(it, devmem_array.end()) > 16; it += 16) {

        if(detect_smbios_anchor(it) == SMBiosAnchorType::SMBios32){

            entry_point_buffer_.assign(it, it + smbios32_header_size);
            smbios_entry32_ = reinterpret_cast<const SMBIOSEntryPoint32*>(&entry_point_buffer_[0]);
            checksum_validated_ = checksum32();
        }

        if(detect_smbios_anchor(it) == SMBiosAnchorType::SMBios64){

            entry_point_buffer_.assign(it, it + smbios64_header_size);
            smbios_entry64_ = reinterpret_cast<const SMBIOSEntryPoint64*>(&entry_point_buffer_[0]);
            checksum_validated_ = checksum64();
        }
    }
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

bool SMBios::checksum32() const
{
    uint8_t smbios_intermediate_anchor[] = {'_','D','M','I','_'};
    bool is_anchor_correct = std::equal(
                std::begin(smbios_intermediate_anchor),
                std::end(smbios_intermediate_anchor),
                smbios_entry32_->intermediate_anchor);

    bool crc_correct = (0u == smbios_crc(0, smbios_entry32_->entry_point_length)
                        && (0u == smbios_crc(0x10, 0x0F)));

    return is_anchor_correct && crc_correct;
}

bool SMBios::checksum64() const
{
    return (0u == smbios_crc(0, smbios_entry64_->entry_point_length));
}

uint8_t SMBios::smbios_crc(uint8_t start_offset, uint8_t length) const
{
    uint8_t sum{};
    for (size_t a = start_offset; a < length; a++)
        sum += entry_point_buffer_[a];
    return sum;
}

std::string SMBios::render_to_description() const
{
    if(smbios_entry32_ && checksum_validated_) {

        std::stringstream decsription;
        decsription << "SMBIOS checksum: " << static_cast<size_t>(smbios_entry32_->entry_point_checksum) << '\n';
        decsription << "SMBIOS length: " << static_cast<size_t>(smbios_entry32_->entry_point_length) << '\n';
        decsription << "SMBIOS major version: " << static_cast<size_t>(smbios_entry32_->major_version) << '\n';
        decsription << "SMBIOS minor version: " << static_cast<size_t>(smbios_entry32_->minor_version) << '\n';
        decsription << "Maximum structure size: " << smbios_entry32_->max_structure_size << '\n';
        decsription << "Entry point revision: " << static_cast<size_t>(smbios_entry32_->entry_point_revision) << '\n';
        decsription << "SMBIOS intermediate checksum: " << static_cast<size_t>(smbios_entry32_->intermediate_checksum) << '\n';
        decsription << "Structure table length: " << smbios_entry32_->structure_table_length << '\n';
        decsription << "Table address: " << std::hex << smbios_entry32_->structure_table_address << std::dec << '\n';
        decsription << "SMBIOS structures count: " << smbios_entry32_->smbios_structures_number << '\n';
        decsription << "SMBIOS BCD revision: " << static_cast<size_t>(smbios_entry32_->smbios_bcd_revision) << '\n';
        return std::move(decsription.str());
    }

    if(smbios_entry64_ && checksum_validated_) {

        std::stringstream decsription;
        decsription << "SMBIOS checksum: " << static_cast<size_t>(smbios_entry64_->entry_point_checksum) << '\n';
        decsription << "SMBIOS length: " << static_cast<size_t>(smbios_entry64_->entry_point_length) << '\n';
        decsription << "SMBIOS major version: " << static_cast<size_t>(smbios_entry64_->major_version) << '\n';
        decsription << "SMBIOS minor version: " << static_cast<size_t>(smbios_entry32_->minor_version) << '\n';
        decsription << "SMBIOS doc version: " << static_cast<size_t>(smbios_entry64_->smbios_docrev) << '\n';
        decsription << "Reserved byte: " <<  static_cast<size_t>(smbios_entry64_->reserved) << '\n';
        decsription << "Maximum structure size: " << smbios_entry64_->max_structure_size << '\n';
        decsription << "Table address: " << std::hex << smbios_entry64_->structure_table_address << std::dec << '\n';
        return std::move(decsription.str());
    }
    return std::string{};
}
