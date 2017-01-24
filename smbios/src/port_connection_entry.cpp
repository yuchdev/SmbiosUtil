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
    connection_type_map_[MiniCentronics] = "Mini Centronics";
    connection_type_map_[Proprietary] = "Proprietary";
    connection_type_map_[DB25PinMale] = "DB-25 Pin Male";
    connection_type_map_[DB25PinFemale] = "DB-25 Pin Female";
    connection_type_map_[DB15PinMale] = "DB-15 Pin Male";
    connection_type_map_[DB15PinFemale] = "DB-15 Pin Female";
    connection_type_map_[DB9PinMale] = "DB-9 Pin Male";
    connection_type_map_[DB9PinFemale] = "DB-9 Pin Female";
    connection_type_map_[RJ11] = "RJ-11";
    connection_type_map_[RJ45] = "RJ-45";
    connection_type_map_[MiniSCSI50pin] = "50-pin MiniSCSI";
    connection_type_map_[MiniDIN] = "Mini-DIN";
    connection_type_map_[MicroDIN] = "Micro-DIN";
    connection_type_map_[PS2] = "PS/2";
    connection_type_map_[Infrared] = "Infrared";
    connection_type_map_[HPHIL] = "HP-HIL";
    connection_type_map_[AccessBusUSB] = "Access Bus (USB)";
    connection_type_map_[SSA_SCSIConnector] = "SSA SCSI";
    connection_type_map_[CircularDIN8Male] = "Circular DIN-8 Male";
    connection_type_map_[CircularDIN8Female] = "Circular DIN-8 Female";
    connection_type_map_[OnBoardIDE] = "On Board IDE";
    connection_type_map_[OnBoardFloppy] = "On Board Floppy";
    connection_type_map_[DualInline9pin] = "9-pin Dual Inline(pin 10 cut)";
    connection_type_map_[DualInline25pin] = "25-pin Dual Inline(pin 26 cut)";
    connection_type_map_[DualInline50pin] = "50-pin Dual Inline";
    connection_type_map_[DualInline68pin] = "68-pin Dual Inline";
    connection_type_map_[OnBoardSound] = "On Board Sound Input from CD-ROM";
    connection_type_map_[MiniCentronicsType14] = "Mini-Centronics Type-14";
    connection_type_map_[MiniCentronicsType26] = "Mini-Centronics Type-26";
    connection_type_map_[MiniJack] = "Mini-jack(headphones)";
    connection_type_map_[BNC] = "BNC";
    connection_type_map_[IEEE1394] = "1394";
    connection_type_map_[SAS_SATA] = "SAS/SATA Plug Receptacle";
    connection_type_map_[PC98Connector] = "PC-98";
    connection_type_map_[PC98HiresoConnector] = "PC-98Hireso";
    connection_type_map_[PCH98Connector] = "PC-H98";
    connection_type_map_[PC98Note] = "PC-98Note";
    connection_type_map_[PC98Full] = "PC-98Full";
    connection_type_map_[OtherConnector] = "Other - See Reference Designator Strings";

    port_type_map_[NonePort] = "None";
    port_type_map_[ParallelXT_AT] = "Parallel Port XT/AT Compatible";
    port_type_map_[ParallelPS_2] = "Parallel Port PS/2";
    port_type_map_[ParallelECP] = "Parallel Port ECP";
    port_type_map_[ParallelEPP] = "Parallel Port EPP";
    port_type_map_[ParallelECP_EPP] = "Parallel Port ECP/EPP";
    port_type_map_[SerialXT_AT] = "Serial Port XT/AT Compatible";
    port_type_map_[Serial16450] = "Serial Port 16450 Compatible";
    port_type_map_[Serial16550] = "Serial Port 16550 Compatible";
    port_type_map_[Serial16550A] = "Serial Port 16550A Compatible";
    port_type_map_[SCSI] = "SCSI Port";
    port_type_map_[MIDI] = "MIDI Port";
    port_type_map_[JoyStick] = "Joy Stick Port";
    port_type_map_[Keyboard] = "Keyboard Port";
    port_type_map_[Mouse] = "Mouse Port";

    port_type_map_[SSA_SCSIPort] = "SSA SCSI";
    port_type_map_[USB] = "USB";
    port_type_map_[FireWire] = "FireWire(IEEE P1394)";
    port_type_map_[PCMCIA] = "PCMCIA Type I";
    port_type_map_[PCMCIAType2] = "PCMCIA Type II";
    port_type_map_[PCMCIAType3] = "PCMCIA Type III";
    port_type_map_[Cardbus] = "Cardbus";
    port_type_map_[AccessBusPort] = "Access Bus Port";
    port_type_map_[SCSI2] = "SCSI II";
    port_type_map_[SCSIWide] = "SCSI Wide";
    port_type_map_[PC98Port] = "PC-98";
    port_type_map_[PC98HiresoPort] = "PC-98-Hireso";
    port_type_map_[PCH98Port] = "PC-H98";
    port_type_map_[Video] = "Video Port";
    port_type_map_[Audio] = "Audio Port";
    port_type_map_[Modem] = "Modem Port";
    port_type_map_[Network] = "Network Port";
    port_type_map_[SATA] = "SATA";
    port_type_map_[SAS] = "SAS";
    port_type_map_[Compatible8251] = "8251 Compatible";
    port_type_map_[CompatibleFIFO8251] = "8251 FIFO Compatible";
    port_type_map_[OtherPort] = "Other";
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
