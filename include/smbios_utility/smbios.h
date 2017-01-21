#pragma once
#include <vector>
#include <memory>
#include <cstdint>

class SMBiosImpl;
class NativePhysicalMemory;

// should be aligned to be mapped to the physical memory
#pragma pack(push, 1)

struct SMBIOSEntryPoint32 {
    uint8_t entry_point_anchor[4];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint16_t max_structure_size;
    uint8_t entry_point_revision;
    uint8_t formatted_area[5];
    uint8_t intermediate_anchor[5];
    uint8_t intermediate_checksum;
    uint16_t structure_table_length;
    uint32_t structure_table_address;
    uint16_t smbios_structures_number;
    uint8_t smbios_bcd_revision;
};

struct SMBIOSEntryPoint64 {
    uint8_t entry_point_anchor[5];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t smbios_docrev;
    uint8_t reserved;
    uint32_t max_structure_size;
    uint64_t structure_table_address;
};
#pragma pack(pop)

struct DMIHeader
{
    // Specifies the type of structure. Types 0 through 127 (7Fh) are reserved for and
    // defined by this specification. Types 128 through 256 (80h to FFh) are available for
    // system- and OEM-specific information
    uint8_t type;

    // Specifies the length of the formatted area of the structure, starting at the Type field
    uint8_t length;

    // Specifies the structure's handle, a unique 16-bit number in the range 0 to 0FFFEh
    // If the system configuration changes, a previously assigned handle might no longer exist!
    uint16_t handle;
    uint8_t *data;
};

/// @brief Class owns system-independent SMBIOS table 
/// which however has been read using system-dependent API
/// It also have a cache like table structures count and headers
/// (it's obvious information could not be updated while computer is active)
/// Class support bidirectional iterators to be used in STL algorithms and cycles
class SMBios
{
public:

    // SMBios header type
    // Stored in the DMIHeader::handle field
    // See SMBIOS specification
    // http://www.dmtf.org/standards/smbios
    // Table called 'Required structures and data'
    enum SMBiosHandler
    {
        BIOSInformation = 0,
        SystemInformation = 1,
        SystemEnclosure = 3,
        ProcessorInformation = 4,
        CacheInformation = 7,
        SystemSlots = 9,
        PhysicalMemoryArray = 16,
        MemoryDevice = 17,
        MemoryArrayMappedAddress = 19,
        SystemBootInformation = 32,
        EndOfTable = 127
    };

    /// @brief Read SMBIOS table using native OS-specific method
    SMBios();
    
    /// @brief Should be exist to satisfy compiler
    ~SMBios();

    /// @brief Get SMBIOS string version
    std::string get_smbios_version() const;

    /// @brief Get SMBIOS structures stored in class
    size_t get_structures_count() const;

    /// @brief Implement bidirectional iterator for STL-style processing
    class iterator {
    public:

        friend class SMBios;
        enum EndTag { end };

        iterator() {}

        iterator(std::vector<DMIHeader>& headers) :headers_list_(&headers) {
            pointed_header_ = headers_list_->begin();
        };

        iterator(std::vector<DMIHeader>& headers, EndTag) :headers_list_(&headers) {
            pointed_header_ = headers_list_->end();
        };

        const DMIHeader& operator*() {
            return *pointed_header_; 
        };

        const iterator& operator++() {
            ++pointed_header_;
            return *this;
        }

        const iterator& operator--() {
            --pointed_header_;
            return *this;
        }

        bool operator==(const iterator& it) const {
            return pointed_header_ == it.pointed_header_;
        }

        bool operator!=(const iterator& it) const {
            return pointed_header_ != it.pointed_header_;
        }

    private:
        std::vector<DMIHeader>* headers_list_ = nullptr;
        std::vector<DMIHeader>::iterator pointed_header_;
    };

    /// @brief Iterator begin - for STL-style processing
    iterator begin()
    {
        return iterator(get_headers_list());
    }

    /// @brief Iterator end - for STL-style processing
    iterator end()
    {
        return iterator(get_headers_list(), iterator::end);
    }

private:

    /// Friend-only access for iterator class
    std::vector<DMIHeader>& get_headers_list();

    /// Parse and save headers for every entry
    void read_smbios_table();

    /// Count all low-level structures stored in SMBios
    /// Do it once at start
    void count_smbios_structures();

    /// Fallback to physical memory scan if no one of system-specific interfaces
    /// was available
    void scan_physical_memory(const std::vector<uint8_t>& devmem_array);

    /// Get DMI version major.minor
    void extract_dmi_version();

    /// Display SMBIOS entry point data
    void display_entry_point() const;

private:

    /// Raw SMBIOS table system-specific implementation
    std::unique_ptr<SMBiosImpl> native_impl_;

    /// Physical memory device, initialized if necessary
    //std::unique_ptr<NativePhysicalMemory> physical_memory_device_;

    /// Cached SMBIOS structures count
    size_t structures_count_ = 0;

    /// Cached major SMBIOS version
    size_t major_version_ = 0;

    /// Cached major SMBIOS version
    size_t minor_version_ = 0;

    /// Save SMBIOS entry point here
    std::vector<uint8_t> entry_point_buffer_;

    /// Cached SMBIOS headers
    std::vector<DMIHeader> headers_list_;

    /// Entry points, mapped to memory dump
    const SMBIOSEntryPoint32* smbios_entry32_ = nullptr;
    const SMBIOSEntryPoint64* smbios_entry64_ = nullptr;

    /// Set this flag if SMBIOS entry point checksum is valid
    bool checksum_validated_ = true;

    /// Scan physical memory from this address
    static const size_t devmem_base_ = 0xF0000;

    /// Scanned lenght (SMBIOS could not be beyond this offset)
    static const size_t devmem_length_ = 0x10000;
};
