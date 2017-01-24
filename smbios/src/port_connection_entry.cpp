#include <smbios/port_connection_entry.h>
#include <smbios/smbios.h>

#include <sstream>
#include <cassert>

using std::string;
using namespace smbios;

PortConnectionEntry::PortConnectionEntry(const DMIHeader& header) {

    // TODO: check entry size
    if (header.type != SMBios::PortConnection) {
        std::stringstream err;
        err << "Wrong device type, expected Port Connection, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }
    port_connection_ = reinterpret_cast<const PortConnection*>(header.data);

    init_string_values();
}

PortConnectionEntry::~PortConnectionEntry()
{

}


void PortConnectionEntry::init_string_values()
{
    connection_type_map_[NoneConnector] = "None";
    connection_type_map_[Centronics] = "Centronics";
    connection_type_map_[MiniCentronics] = "MiniCentronics";
    connection_type_map_[Proprietary] = "Proprietary";
    connection_type_map_[DB25PinMale] = "DB25PinMale";
    connection_type_map_[DB25PinFemale] = "DB25PinFemale";
    connection_type_map_[DB15PinMale] = "DB15PinMale";
    connection_type_map_[DB15PinFemale] = "DB15PinFemale";
    connection_type_map_[DB9PinMale] = "DB9PinMale";
    connection_type_map_[DB9PinFemale] = "DB9PinFemale";
    connection_type_map_[RJ11] = "RJ11";
    connection_type_map_[RJ45] = "RJ45";
    connection_type_map_[MiniSCSI50pin] = "MiniSCSI50pin";
    connection_type_map_[MiniDIN] = "MiniDIN";
    connection_type_map_[MicroDIN] = "Micro_DIN";
    connection_type_map_[PS2] = "PS2";
    connection_type_map_[Infrared] = "Infrared";
    connection_type_map_[HPHIL] = "HPHIL";
    connection_type_map_[AccessBusUSB] = "AccessBusUSB";
    connection_type_map_[SSA_SCSIConnector] = "SSA_SCSI";
    connection_type_map_[CircularDIN8Male] = "CircularDIN8Male";
    connection_type_map_[CircularDIN8Female] = "";
    connection_type_map_[OnBoardIDE] = "";
    connection_type_map_[OnBoardFloppy] = "";
    connection_type_map_[DualInline9pin] = "";
    connection_type_map_[DualInline25pin] = "";
    connection_type_map_[DualInline50pin] = "";
    connection_type_map_[DualInline68pin] = "";
    connection_type_map_[OnBoardSound] = "";
    connection_type_map_[MiniCentronicsType14] = "";
    connection_type_map_[MiniCentronicsType26] = "";
    connection_type_map_[MiniJack] = "";
    connection_type_map_[BNC] = "";
    connection_type_map_[IEEE1394] = "";
    connection_type_map_[SAS_SATA] = "";
    connection_type_map_[PC98Connector] = "";
    connection_type_map_[PC98HiresoConnector] = "";
    connection_type_map_[PCH98Connector] = "";
    connection_type_map_[PC98Note] = "";
    connection_type_map_[PC98Full] = "";
    connection_type_map_[OtherConnector] = "";

    port_type_map_[NonePort] = "";
    port_type_map_[ParallelXT_AT] = "";
    port_type_map_[ParallelPS_2] = "";
    port_type_map_[ParallelECP] = "";
    port_type_map_[ParallelEPP] = "";
    port_type_map_[ParallelECP_EPP] = "";
    port_type_map_[SerialXT_AT] = "";
    port_type_map_[Serial16450] = "";
    port_type_map_[Serial16550] = "";
    port_type_map_[Serial16550A] = "";
    port_type_map_[SCSI] = "";
    port_type_map_[MIDI] = "";
    port_type_map_[JoyStick] = "";
    port_type_map_[Keyboard] = "";
    port_type_map_[Mouse] = "";
    port_type_map_[SSA_SCSIPort] = "";
    port_type_map_[USB] = "";
    port_type_map_[FireWire] = "";
    port_type_map_[PCMCIA] = "";
    port_type_map_[PCMCIAType2] = "";
    port_type_map_[PCMCIAType3] = "";
    port_type_map_[Cardbus] = "";
    port_type_map_[AccessBusPort] = "";
    port_type_map_[SCSI2] = "";
    port_type_map_[SCSIWide] = "";
    port_type_map_[PC98Port] = "";
    port_type_map_[PC98HiresoPort] = "";
    port_type_map_[PCH98Port] = "";
    port_type_map_[Video] = "";
    port_type_map_[Audio] = "";
    port_type_map_[Modem] = "";
    port_type_map_[Network] = "";
    port_type_map_[SATA] = "";
    port_type_map_[SAS] = "";
    port_type_map_[Compatible8251] = "";
    port_type_map_[CompatibleFIFO8251] = "";
    port_type_map_[OtherPort] = "";
}

std::string PortConnectionEntry::get_type() const
{
    return "Port Connection";
}

std::string smbios::PortConnectionEntry::render_to_description() const
{
    return "NOT IMPLEMENTED";
}


uint8_t smbios::PortConnectionEntry::get_internal_connection_type() const
{
    if (nullptr == port_connection_) {
        return ConnectorType::NoneConnector;
    }

    auto it = connection_type_map_.find(port_connection_->internal_connection);
    if (it != connection_type_map_.end()) {
        return (*it).first;
    }
    assert(false);
    return ConnectorType::NoneConnector;
}

uint8_t smbios::PortConnectionEntry::get_external_connection_type() const
{
    if (nullptr == port_connection_) {
        return ConnectorType::NoneConnector;
    }

    auto it = connection_type_map_.find(port_connection_->external_connection);
    if (it != connection_type_map_.end()) {
        return (*it).first;
    }
    assert(false);
    return ConnectorType::NoneConnector;
}

uint8_t smbios::PortConnectionEntry::get_port_type() const
{
    if (nullptr == port_connection_) {
        return PortType::NonePort;
    }

    auto it = port_type_map_.find(port_connection_->port_type);
    if (it != port_type_map_.end()) {
        return (*it).first;
    }
    assert(false);
    return PortType::NonePort;
}

std::string smbios::PortConnectionEntry::get_internal_connection_string() const
{
    return (*connection_type_map_.find(get_internal_connection_type())).second;
}

std::string smbios::PortConnectionEntry::get_external_connection_string() const
{
    return (*connection_type_map_.find(get_external_connection_type())).second;
}

std::string smbios::PortConnectionEntry::get_port_string() const
{
    return (*port_type_map_.find(get_port_type())).second;
}
