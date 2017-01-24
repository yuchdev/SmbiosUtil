#pragma once
#include <cstdint>
#include <map>
#include <smbios/abstract_smbios_entry.h>

namespace smbios {

struct DMIHeader;

// should be aligned to be mapped to physical memory
#pragma pack(push, 1)

// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version
// 'Memory Device' chapter

/// @brief SMBIOS MemoryDevice entry Ver 2.1+
struct PortConnection {
    uint32_t header;
};

#pragma pack(pop)

/// @brief 
class PortConnectionEntry : public AbstractSMBiosEntry {
public:

    // @brief 
    enum ConnectorType : uint8_t {
    };

    // @brief 
    enum PortType : uint8_t {
    };


    /// @brief Parse the header, recognize how much information do we have
    /// in MemoryDevice SMBIOS entry
    PortConnectionEntry(const DMIHeader& header);

    // @brief Parent is abstract
    virtual ~PortConnectionEntry();

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Handle, or instance number, associated with the structure
    uint16_t get_handle() const;

    /// @brief 0x05 offset
    /// 
    uint8_t get_internal_connection_type() const;

    /// @brief 0x07 offset
    /// 
    uint8_t get_external_connection_type() const;

    /// @brief 0x08 offset
    /// 
    uint8_t get_port_type() const;


    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x04 offset
    /// Handle, or instance number, associated with the structure
    std::string get_handle_string() const;

    /// @brief 0x05 offset
    /// 
    std::string get_internal_connection_string() const;

    /// @brief 0x07 offset
    /// 
    std::string get_external_connection_string() const;

    /// @brief 0x08 offset
    /// 
    std::string get_port_string() const;

    /// @brief 
    const PortConnection* get_port_connection_struct() const { return port_connection_; }


private:

    /// Map flags data to string values
    void init_string_values();

private:

    PortConnection* port_connection_;

    /// Bitwise to string representation
    mutable std::map<uint8_t, std::string> connection_type_map_;
    mutable std::map<uint8_t, std::string> port_type_map_;
};

} // namespace smbios