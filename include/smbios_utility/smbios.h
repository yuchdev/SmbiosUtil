#pragma once
#include <vector>
#include <memory>
#include <cstdint>

class SMBiosImpl;

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

private:

    /// Raw SMBIOS table
    std::unique_ptr<SMBiosImpl> native_impl_;

    /// Cached SMBIOS structures count
    size_t structures_count_ = 0;

    /// Cached SMBIOS headers
    std::vector<DMIHeader> headers_list_;
};
