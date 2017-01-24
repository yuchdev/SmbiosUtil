#pragma once
#include <cstdint>
#include <map>
#include <smbios/abstract_smbios_entry.h>

namespace smbios {

struct DMIHeader;

// should be aligned to be mapped to physical memory
#pragma pack(push, 1)

// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Port Connection' chapter
// By 3.1 version fully compatible with all previous versions

/// @brief SMBIOS MemoryDevice entry Ver 2.1+
struct PortConnection {
    uint32_t header;
    uint8_t internal_designator;
    uint8_t internal_connection;
    uint8_t external_designator;
    uint8_t external_connection;
    uint8_t port_type;
};

#pragma pack(pop)

/// @brief 
class PortConnectionEntry : public AbstractSMBiosEntry {
public:

    // @brief 
    enum ConnectorType : uint8_t {
        NoneConnector = 0x00,
        Centronics = 0x01,
        MiniCentronics = 0x02,
        Proprietary = 0x03,
        DB25PinMale = 0x04,
        DB25PinFemale = 0x05,
        DB15PinMale = 0x06,
        DB15PinFemale = 0x07,
        DB9PinMale = 0x08,
        DB9PinFemale = 0x09,
        RJ11 = 0x0A,
        RJ45 = 0x0B,
        MiniSCSI50pin = 0x0C,
        MiniDIN = 0x0D,
        MicroDIN = 0x0E,
        PS2 = 0x0F,
        Infrared = 0x10,
        HPHIL = 0x11,
        AccessBusUSB = 0x12,
        SSA_SCSIConnector = 0x13,
        CircularDIN8Male = 0x14,
        CircularDIN8Female = 0x15,
        OnBoardIDE = 0x16,
        OnBoardFloppy = 0x17,
        DualInline9pin = 0x18,
        DualInline25pin = 0x19,
        DualInline50pin = 0x1A,
        DualInline68pin = 0x1B,
        OnBoardSound = 0x1C,
        MiniCentronicsType14 = 0x1D,
        MiniCentronicsType26 = 0x1E,
        MiniJack = 0x1F,
        BNC = 0x20,
        IEEE1394 = 0x21,
        SAS_SATA = 0x22,
        PC98Connector = 0xA0,
        PC98HiresoConnector = 0xA1,
        PCH98Connector = 0xA2,
        PC98Note = 0xA3,
        PC98Full = 0xA4,
        OtherConnector = 0xFF
    };

    // @brief 
    enum PortType : uint8_t {
        NonePort = 0x00,
        ParallelXT_AT = 0x01,
        ParallelPS_2 = 0x02,
        ParallelECP = 0x03,
        ParallelEPP = 0x04,
        ParallelECP_EPP = 0x05,
        SerialXT_AT = 0x06,
        Serial16450 = 0x07,
        Serial16550 = 0x08,
        Serial16550A = 0x09,
        SCSI = 0x0A,
        MIDI = 0x0B,
        JoyStick = 0x0C,
        Keyboard = 0x0D,
        Mouse = 0x0E,
        SSA_SCSIPort = 0x0F,
        USB = 0x10,
        FireWire = 0x11,
        PCMCIA = 0x12,
        PCMCIAType2 = 0x13,
        PCMCIAType3 = 0x14,
        Cardbus = 0x15,
        AccessBusPort = 0x16,
        SCSI2 = 0x17,
        SCSIWide = 0x18,
        PC98Port = 0x19,
        PC98HiresoPort = 0x1A,
        PCH98Port = 0x1B,
        Video = 0x1C,
        Audio = 0x1D,
        Modem = 0x1E,
        Network = 0x1F,
        SATA = 0x20,
        SAS = 0x21,
        Compatible8251 = 0xA0,
        CompatibleFIFO8251 = 0xA1,
        OtherPort = 0xFF
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

    const PortConnection* port_connection_;

    /// Bitwise to string representation
    std::map<uint8_t, std::string> connection_type_map_;
    std::map<uint8_t, std::string> port_type_map_;
};

} // namespace smbios