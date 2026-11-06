#include <smbios/processor_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::ProcessorInformation at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::ProcessorInformation, ProcessorInformationEntry)

ProcessorInformationEntry::ProcessorInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
    , cache_handle_version_ge_2_3_(version > SMBiosVersion{2, 2})
{
    if (header.type != SMBios::ProcessorInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Processor Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length (plus the 0xFE/
    // 0xFF sentinel checks named in the class doc comment), confirmed
    // against c-versions/dmidecode_linux's own dmi_decode() for type 4.
    init_string_values();
}

void ProcessorInformationEntry::init_string_values()
{
    processor_type_map_[ProcessorTypeOutOfSpec] = "OutOfSpec";
    processor_type_map_[ProcessorTypeOther] = "Other";
    processor_type_map_[ProcessorTypeUnknown] = "Unknown";
    processor_type_map_[CentralProcessor] = "Central Processor";
    processor_type_map_[MathProcessor] = "Math Processor";
    processor_type_map_[DSPProcessor] = "DSP Processor";
    processor_type_map_[VideoProcessor] = "Video Processor";

    // DMTF 7.5.2 family-code-to-name table (dmi_processor_family()'s
    // family2[] array) - see class doc comment's scope note on the two
    // special cases (0x30, 0xBE) intentionally not reproduced here.
    processor_family_map_[0x01] = "Other";
    processor_family_map_[0x02] = "Unknown";
    processor_family_map_[0x03] = "8086";
    processor_family_map_[0x04] = "80286";
    processor_family_map_[0x05] = "80386";
    processor_family_map_[0x06] = "80486";
    processor_family_map_[0x07] = "8087";
    processor_family_map_[0x08] = "80287";
    processor_family_map_[0x09] = "80387";
    processor_family_map_[0x0A] = "80487";
    processor_family_map_[0x0B] = "Pentium";
    processor_family_map_[0x0C] = "Pentium Pro";
    processor_family_map_[0x0D] = "Pentium II";
    processor_family_map_[0x0E] = "Pentium MMX";
    processor_family_map_[0x0F] = "Celeron";
    processor_family_map_[0x10] = "Pentium II Xeon";
    processor_family_map_[0x11] = "Pentium III";
    processor_family_map_[0x12] = "M1";
    processor_family_map_[0x13] = "M2";
    processor_family_map_[0x14] = "Celeron M";
    processor_family_map_[0x15] = "Pentium 4 HT";
    processor_family_map_[0x16] = "Intel";
    processor_family_map_[0x18] = "Duron";
    processor_family_map_[0x19] = "K5";
    processor_family_map_[0x1A] = "K6";
    processor_family_map_[0x1B] = "K6-2";
    processor_family_map_[0x1C] = "K6-3";
    processor_family_map_[0x1D] = "Athlon";
    processor_family_map_[0x1E] = "AMD29000";
    processor_family_map_[0x1F] = "K6-2+";
    processor_family_map_[0x20] = "Power PC";
    processor_family_map_[0x21] = "Power PC 601";
    processor_family_map_[0x22] = "Power PC 603";
    processor_family_map_[0x23] = "Power PC 603+";
    processor_family_map_[0x24] = "Power PC 604";
    processor_family_map_[0x25] = "Power PC 620";
    processor_family_map_[0x26] = "Power PC x704";
    processor_family_map_[0x27] = "Power PC 750";
    processor_family_map_[0x28] = "Core Duo";
    processor_family_map_[0x29] = "Core Duo Mobile";
    processor_family_map_[0x2A] = "Core Solo Mobile";
    processor_family_map_[0x2B] = "Atom";
    processor_family_map_[0x2C] = "Core M";
    processor_family_map_[0x2D] = "Core m3";
    processor_family_map_[0x2E] = "Core m5";
    processor_family_map_[0x2F] = "Core m7";
    processor_family_map_[0x30] = "Alpha";
    processor_family_map_[0x31] = "Alpha 21064";
    processor_family_map_[0x32] = "Alpha 21066";
    processor_family_map_[0x33] = "Alpha 21164";
    processor_family_map_[0x34] = "Alpha 21164PC";
    processor_family_map_[0x35] = "Alpha 21164a";
    processor_family_map_[0x36] = "Alpha 21264";
    processor_family_map_[0x37] = "Alpha 21364";
    processor_family_map_[0x38] = "Turion II Ultra Dual-Core Mobile M";
    processor_family_map_[0x39] = "Turion II Dual-Core Mobile M";
    processor_family_map_[0x3A] = "Athlon II Dual-Core M";
    processor_family_map_[0x3B] = "Opteron 6100";
    processor_family_map_[0x3C] = "Opteron 4100";
    processor_family_map_[0x3D] = "Opteron 6200";
    processor_family_map_[0x3E] = "Opteron 4200";
    processor_family_map_[0x3F] = "FX";
    processor_family_map_[0x40] = "MIPS";
    processor_family_map_[0x41] = "MIPS R4000";
    processor_family_map_[0x42] = "MIPS R4200";
    processor_family_map_[0x43] = "MIPS R4400";
    processor_family_map_[0x44] = "MIPS R4600";
    processor_family_map_[0x45] = "MIPS R10000";
    processor_family_map_[0x46] = "C-Series";
    processor_family_map_[0x47] = "E-Series";
    processor_family_map_[0x48] = "A-Series";
    processor_family_map_[0x49] = "G-Series";
    processor_family_map_[0x4A] = "Z-Series";
    processor_family_map_[0x4B] = "R-Series";
    processor_family_map_[0x4C] = "Opteron 4300";
    processor_family_map_[0x4D] = "Opteron 6300";
    processor_family_map_[0x4E] = "Opteron 3300";
    processor_family_map_[0x4F] = "FirePro";
    processor_family_map_[0x50] = "SPARC";
    processor_family_map_[0x51] = "SuperSPARC";
    processor_family_map_[0x52] = "MicroSPARC II";
    processor_family_map_[0x53] = "MicroSPARC IIep";
    processor_family_map_[0x54] = "UltraSPARC";
    processor_family_map_[0x55] = "UltraSPARC II";
    processor_family_map_[0x56] = "UltraSPARC IIi";
    processor_family_map_[0x57] = "UltraSPARC III";
    processor_family_map_[0x58] = "UltraSPARC IIIi";
    processor_family_map_[0x60] = "68040";
    processor_family_map_[0x61] = "68xxx";
    processor_family_map_[0x62] = "68000";
    processor_family_map_[0x63] = "68010";
    processor_family_map_[0x64] = "68020";
    processor_family_map_[0x65] = "68030";
    processor_family_map_[0x66] = "Athlon X4";
    processor_family_map_[0x67] = "Opteron X1000";
    processor_family_map_[0x68] = "Opteron X2000";
    processor_family_map_[0x69] = "Opteron A-Series";
    processor_family_map_[0x6A] = "Opteron X3000";
    processor_family_map_[0x6B] = "Zen";
    processor_family_map_[0x70] = "Hobbit";
    processor_family_map_[0x78] = "Crusoe TM5000";
    processor_family_map_[0x79] = "Crusoe TM3000";
    processor_family_map_[0x7A] = "Efficeon TM8000";
    processor_family_map_[0x80] = "Weitek";
    processor_family_map_[0x82] = "Itanium";
    processor_family_map_[0x83] = "Athlon 64";
    processor_family_map_[0x84] = "Opteron";
    processor_family_map_[0x85] = "Sempron";
    processor_family_map_[0x86] = "Turion 64";
    processor_family_map_[0x87] = "Dual-Core Opteron";
    processor_family_map_[0x88] = "Athlon 64 X2";
    processor_family_map_[0x89] = "Turion 64 X2";
    processor_family_map_[0x8A] = "Quad-Core Opteron";
    processor_family_map_[0x8B] = "Third-Generation Opteron";
    processor_family_map_[0x8C] = "Phenom FX";
    processor_family_map_[0x8D] = "Phenom X4";
    processor_family_map_[0x8E] = "Phenom X2";
    processor_family_map_[0x8F] = "Athlon X2";
    processor_family_map_[0x90] = "PA-RISC";
    processor_family_map_[0x91] = "PA-RISC 8500";
    processor_family_map_[0x92] = "PA-RISC 8000";
    processor_family_map_[0x93] = "PA-RISC 7300LC";
    processor_family_map_[0x94] = "PA-RISC 7200";
    processor_family_map_[0x95] = "PA-RISC 7100LC";
    processor_family_map_[0x96] = "PA-RISC 7100";
    processor_family_map_[0xA0] = "V30";
    processor_family_map_[0xA1] = "Quad-Core Xeon 3200";
    processor_family_map_[0xA2] = "Dual-Core Xeon 3000";
    processor_family_map_[0xA3] = "Quad-Core Xeon 5300";
    processor_family_map_[0xA4] = "Dual-Core Xeon 5100";
    processor_family_map_[0xA5] = "Dual-Core Xeon 5000";
    processor_family_map_[0xA6] = "Dual-Core Xeon LV";
    processor_family_map_[0xA7] = "Dual-Core Xeon ULV";
    processor_family_map_[0xA8] = "Dual-Core Xeon 7100";
    processor_family_map_[0xA9] = "Quad-Core Xeon 5400";
    processor_family_map_[0xAA] = "Quad-Core Xeon";
    processor_family_map_[0xAB] = "Dual-Core Xeon 5200";
    processor_family_map_[0xAC] = "Dual-Core Xeon 7200";
    processor_family_map_[0xAD] = "Quad-Core Xeon 7300";
    processor_family_map_[0xAE] = "Quad-Core Xeon 7400";
    processor_family_map_[0xAF] = "Multi-Core Xeon 7400";
    processor_family_map_[0xB0] = "Pentium III Xeon";
    processor_family_map_[0xB1] = "Pentium III Speedstep";
    processor_family_map_[0xB2] = "Pentium 4";
    processor_family_map_[0xB3] = "Xeon";
    processor_family_map_[0xB4] = "AS400";
    processor_family_map_[0xB5] = "Xeon MP";
    processor_family_map_[0xB6] = "Athlon XP";
    processor_family_map_[0xB7] = "Athlon MP";
    processor_family_map_[0xB8] = "Itanium 2";
    processor_family_map_[0xB9] = "Pentium M";
    processor_family_map_[0xBA] = "Celeron D";
    processor_family_map_[0xBB] = "Pentium D";
    processor_family_map_[0xBC] = "Pentium EE";
    processor_family_map_[0xBD] = "Core Solo";
    processor_family_map_[0xBF] = "Core 2 Duo";
    processor_family_map_[0xC0] = "Core 2 Solo";
    processor_family_map_[0xC1] = "Core 2 Extreme";
    processor_family_map_[0xC2] = "Core 2 Quad";
    processor_family_map_[0xC3] = "Core 2 Extreme Mobile";
    processor_family_map_[0xC4] = "Core 2 Duo Mobile";
    processor_family_map_[0xC5] = "Core 2 Solo Mobile";
    processor_family_map_[0xC6] = "Core i7";
    processor_family_map_[0xC7] = "Dual-Core Celeron";
    processor_family_map_[0xC8] = "IBM390";
    processor_family_map_[0xC9] = "G4";
    processor_family_map_[0xCA] = "G5";
    processor_family_map_[0xCB] = "ESA/390 G6";
    processor_family_map_[0xCC] = "z/Architecture";
    processor_family_map_[0xCD] = "Core i5";
    processor_family_map_[0xCE] = "Core i3";
    processor_family_map_[0xCF] = "Core i9";
    processor_family_map_[0xD2] = "C7-M";
    processor_family_map_[0xD3] = "C7-D";
    processor_family_map_[0xD4] = "C7";
    processor_family_map_[0xD5] = "Eden";
    processor_family_map_[0xD6] = "Multi-Core Xeon";
    processor_family_map_[0xD7] = "Dual-Core Xeon 3xxx";
    processor_family_map_[0xD8] = "Quad-Core Xeon 3xxx";
    processor_family_map_[0xD9] = "Nano";
    processor_family_map_[0xDA] = "Dual-Core Xeon 5xxx";
    processor_family_map_[0xDB] = "Quad-Core Xeon 5xxx";
    processor_family_map_[0xDD] = "Dual-Core Xeon 7xxx";
    processor_family_map_[0xDE] = "Quad-Core Xeon 7xxx";
    processor_family_map_[0xDF] = "Multi-Core Xeon 7xxx";
    processor_family_map_[0xE0] = "Multi-Core Xeon 3400";
    processor_family_map_[0xE4] = "Opteron 3000";
    processor_family_map_[0xE5] = "Sempron II";
    processor_family_map_[0xE6] = "Embedded Opteron Quad-Core";
    processor_family_map_[0xE7] = "Phenom Triple-Core";
    processor_family_map_[0xE8] = "Turion Ultra Dual-Core Mobile";
    processor_family_map_[0xE9] = "Turion Dual-Core Mobile";
    processor_family_map_[0xEA] = "Athlon Dual-Core";
    processor_family_map_[0xEB] = "Sempron SI";
    processor_family_map_[0xEC] = "Phenom II";
    processor_family_map_[0xED] = "Athlon II";
    processor_family_map_[0xEE] = "Six-Core Opteron";
    processor_family_map_[0xEF] = "Sempron M";
    processor_family_map_[0xFA] = "i860";
    processor_family_map_[0xFB] = "i960";
    processor_family_map_[0x100] = "ARMv7";
    processor_family_map_[0x101] = "ARMv8";
    processor_family_map_[0x102] = "ARMv9";
    processor_family_map_[0x103] = "ARM";
    processor_family_map_[0x104] = "SH-3";
    processor_family_map_[0x105] = "SH-4";
    processor_family_map_[0x118] = "ARM";
    processor_family_map_[0x119] = "StrongARM";
    processor_family_map_[0x12C] = "6x86";
    processor_family_map_[0x12D] = "MediaGX";
    processor_family_map_[0x12E] = "MII";
    processor_family_map_[0x140] = "WinChip";
    processor_family_map_[0x15E] = "DSP";
    processor_family_map_[0x1F4] = "Video Processor";
    processor_family_map_[0x200] = "RV32";
    processor_family_map_[0x201] = "RV64";
    processor_family_map_[0x202] = "RV128";
    processor_family_map_[0x258] = "LoongArch";
    processor_family_map_[0x259] = "Loongson 1";
    processor_family_map_[0x25A] = "Loongson 2";
    processor_family_map_[0x25B] = "Loongson 3";
    processor_family_map_[0x25C] = "Loongson 2K";
    processor_family_map_[0x25D] = "Loongson 3A";
    processor_family_map_[0x25E] = "Loongson 3B";
    processor_family_map_[0x25F] = "Loongson 3C";
    processor_family_map_[0x260] = "Loongson 3D";
    processor_family_map_[0x261] = "Loongson 3E";
    processor_family_map_[0x262] = "Dual-Core Loongson 2K 2xxx";
    processor_family_map_[0x26C] = "Quad-Core Loongson 3A 5xxx";
    processor_family_map_[0x26D] = "Multi-Core Loongson 3A 5xxx";
    processor_family_map_[0x26E] = "Quad-Core Loongson 3B 5xxx";
    processor_family_map_[0x26F] = "Multi-Core Loongson 3B 5xxx";
    processor_family_map_[0x270] = "Multi-Core Loongson 3C 5xxx";
    processor_family_map_[0x271] = "Multi-Core Loongson 3D 5xxx";

    processor_status_map_[StatusUnknown] = "Unknown";
    processor_status_map_[StatusEnabled] = "Enabled";
    processor_status_map_[StatusDisabledByUser] = "Disabled By User";
    processor_status_map_[StatusDisabledByBIOS] = "Disabled By BIOS";
    processor_status_map_[StatusIdle] = "Idle";
    processor_status_map_[StatusOutOfSpec5] = "OutOfSpec";
    processor_status_map_[StatusOutOfSpec6] = "OutOfSpec";
    processor_status_map_[StatusOther] = "Other";

    // DMTF 7.5.5 upgrade table (dmi_processor_upgrade()'s upgrade[] array).
    processor_upgrade_map_[0x01] = "Other";
    processor_upgrade_map_[0x02] = "Unknown";
    processor_upgrade_map_[0x03] = "Daughter Board";
    processor_upgrade_map_[0x04] = "ZIF Socket";
    processor_upgrade_map_[0x05] = "Replaceable Piggy Back";
    processor_upgrade_map_[0x06] = "None";
    processor_upgrade_map_[0x07] = "LIF Socket";
    processor_upgrade_map_[0x08] = "Slot 1";
    processor_upgrade_map_[0x09] = "Slot 2";
    processor_upgrade_map_[0x0A] = "370-pin Socket";
    processor_upgrade_map_[0x0B] = "Slot A";
    processor_upgrade_map_[0x0C] = "Slot M";
    processor_upgrade_map_[0x0D] = "Socket 423";
    processor_upgrade_map_[0x0E] = "Socket A (Socket 462)";
    processor_upgrade_map_[0x0F] = "Socket 478";
    processor_upgrade_map_[0x10] = "Socket 754";
    processor_upgrade_map_[0x11] = "Socket 940";
    processor_upgrade_map_[0x12] = "Socket 939";
    processor_upgrade_map_[0x13] = "Socket mPGA604";
    processor_upgrade_map_[0x14] = "Socket LGA771";
    processor_upgrade_map_[0x15] = "Socket LGA775";
    processor_upgrade_map_[0x16] = "Socket S1";
    processor_upgrade_map_[0x17] = "Socket AM2";
    processor_upgrade_map_[0x18] = "Socket F (1207)";
    processor_upgrade_map_[0x19] = "Socket LGA1366";
    processor_upgrade_map_[0x1A] = "Socket G34";
    processor_upgrade_map_[0x1B] = "Socket AM3";
    processor_upgrade_map_[0x1C] = "Socket C32";
    processor_upgrade_map_[0x1D] = "Socket LGA1156";
    processor_upgrade_map_[0x1E] = "Socket LGA1567";
    processor_upgrade_map_[0x1F] = "Socket PGA988A";
    processor_upgrade_map_[0x20] = "Socket BGA1288";
    processor_upgrade_map_[0x21] = "Socket rPGA988B";
    processor_upgrade_map_[0x22] = "Socket BGA1023";
    processor_upgrade_map_[0x23] = "Socket BGA1224";
    processor_upgrade_map_[0x24] = "Socket BGA1155";
    processor_upgrade_map_[0x25] = "Socket LGA1356";
    processor_upgrade_map_[0x26] = "Socket LGA2011";
    processor_upgrade_map_[0x27] = "Socket FS1";
    processor_upgrade_map_[0x28] = "Socket FS2";
    processor_upgrade_map_[0x29] = "Socket FM1";
    processor_upgrade_map_[0x2A] = "Socket FM2";
    processor_upgrade_map_[0x2B] = "Socket LGA2011-3";
    processor_upgrade_map_[0x2C] = "Socket LGA1356-3";
    processor_upgrade_map_[0x2D] = "Socket LGA1150";
    processor_upgrade_map_[0x2E] = "Socket BGA1168";
    processor_upgrade_map_[0x2F] = "Socket BGA1234";
    processor_upgrade_map_[0x30] = "Socket BGA1364";
    processor_upgrade_map_[0x31] = "Socket AM4";
    processor_upgrade_map_[0x32] = "Socket LGA1151";
    processor_upgrade_map_[0x33] = "Socket BGA1356";
    processor_upgrade_map_[0x34] = "Socket BGA1440";
    processor_upgrade_map_[0x35] = "Socket BGA1515";
    processor_upgrade_map_[0x36] = "Socket LGA3647-1";
    processor_upgrade_map_[0x37] = "Socket SP3";
    processor_upgrade_map_[0x38] = "Socket SP3r2";
    processor_upgrade_map_[0x39] = "Socket LGA2066";
    processor_upgrade_map_[0x3A] = "Socket BGA1392";
    processor_upgrade_map_[0x3B] = "Socket BGA1510";
    processor_upgrade_map_[0x3C] = "Socket BGA1528";
    processor_upgrade_map_[0x3D] = "Socket LGA4189";
    processor_upgrade_map_[0x3E] = "Socket LGA1200";
    processor_upgrade_map_[0x3F] = "Socket LGA4677";
    processor_upgrade_map_[0x40] = "Socket LGA1700";
    processor_upgrade_map_[0x41] = "Socket BGA1744";
    processor_upgrade_map_[0x42] = "Socket BGA1781";
    processor_upgrade_map_[0x43] = "Socket BGA1211";
    processor_upgrade_map_[0x44] = "Socket BGA2422";
    processor_upgrade_map_[0x45] = "Socket LGA1211";
    processor_upgrade_map_[0x46] = "Socket LGA2422";
    processor_upgrade_map_[0x47] = "Socket LGA5773";
    processor_upgrade_map_[0x48] = "Socket BGA5773";
    processor_upgrade_map_[0x49] = "Socket AM5";
    processor_upgrade_map_[0x4A] = "Socket SP5";
    processor_upgrade_map_[0x4B] = "Socket SP6";
    processor_upgrade_map_[0x4C] = "Socket BGA883";
    processor_upgrade_map_[0x4D] = "Socket BGA1190";
    processor_upgrade_map_[0x4E] = "Socket BGA4129";
    processor_upgrade_map_[0x4F] = "Socket LGA4710";
    processor_upgrade_map_[0x50] = "Socket LGA7529";

    characteristics_map_[ProcessorCharacteristicsUnknown] = "Unknown";
    characteristics_map_[Capable64Bit] = "64-bit capable";
    characteristics_map_[MultiCore] = "Multi-Core";
    characteristics_map_[HardwareThread] = "Hardware Thread";
    characteristics_map_[ExecuteProtection] = "Execute Protection";
    characteristics_map_[EnhancedVirtualization] = "Enhanced Virtualization";
    characteristics_map_[PowerPerformanceControl] = "Power/Performance Control";
    characteristics_map_[Capable128Bit] = "128-bit Capable";
    characteristics_map_[Arm64SoCID] = "Arm64 SoC ID";
}

std::string ProcessorInformationEntry::get_type() const
{
    return "Processor Information";
}

std::string ProcessorInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Socket Designation: " << get_socket_designation_string() << '\n';
    description << "Type: " << get_processor_type_string() << '\n';
    description << "Family: " << get_processor_family_string() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "ID: " << get_processor_id_string() << '\n';
    description << "Version: " << get_version_string() << '\n';
    description << "Voltage: " << get_voltage_string() << '\n';
    description << "External Clock: " << get_external_clock_string() << '\n';
    description << "Max Speed: " << get_max_speed_string() << '\n';
    description << "Current Speed: " << get_current_speed_string() << '\n';
    description << "Status: " << get_status_string() << '\n';
    description << "Upgrade: " << get_processor_upgrade_string() << '\n';
    description << "L1 Cache Handle: " << get_l1_cache_handle_string() << '\n';
    description << "L2 Cache Handle: " << get_l2_cache_handle_string() << '\n';
    description << "L3 Cache Handle: " << get_l3_cache_handle_string() << '\n';
    description << "Serial Number: " << get_serial_number_string() << '\n';
    description << "Asset Tag: " << get_asset_tag_string() << '\n';
    description << "Part Number: " << get_part_number_string() << '\n';
    description << "Core Count: " << get_core_count_string() << '\n';
    description << "Core Enabled: " << get_core_enabled_string() << '\n';
    description << "Thread Count: " << get_thread_count_string() << '\n';
    description << "Characteristics: " << '\n' << get_characteristics_string();
    description << "Thread Enabled: " << get_thread_enabled_string() << '\n';

    return description.str();
}

uint8_t ProcessorInformationEntry::get_socket_designation_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t ProcessorInformationEntry::get_processor_type() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return ProcessorType::ProcessorTypeOutOfSpec;

    const uint8_t type = read_u8(raw_data(), offset);
    auto it = processor_type_map_.find(type);
    if (it != processor_type_map_.end()) {
        return (*it).first;
    }
    return ProcessorType::ProcessorTypeOutOfSpec;
}

uint16_t ProcessorInformationEntry::get_processor_family() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;

    const uint8_t family = read_u8(raw_data(), offset);

    // 2.6+ widened field: see dmi_processor_family()'s own
    // `data[0x06] == 0xFE && h->length >= 0x2A` sentinel check.
    constexpr size_t widened_offset = 0x28;
    constexpr size_t widened_min_length = 0x2A;
    if (family == 0xFE && get_entry_size() >= widened_min_length) {
        return read_u16(raw_data(), widened_offset);
    }
    return family;
}

uint8_t ProcessorInformationEntry::get_manufacturer_index() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint64_t ProcessorInformationEntry::get_processor_id() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u64(raw_data(), offset);
}

uint8_t ProcessorInformationEntry::get_version_index() const
{
    constexpr size_t offset = 0x10;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t ProcessorInformationEntry::get_voltage() const
{
    constexpr size_t offset = 0x11;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_external_clock() const
{
    constexpr size_t offset = 0x12;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_max_speed() const
{
    constexpr size_t offset = 0x14;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_current_speed() const
{
    constexpr size_t offset = 0x16;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

bool ProcessorInformationEntry::get_populated() const
{
    constexpr size_t offset = 0x18;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return false;
    return (read_u8(raw_data(), offset) & 0x40) != 0;
}

uint8_t ProcessorInformationEntry::get_status_code() const
{
    constexpr size_t offset = 0x18;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return ProcessorStatusCode::StatusUnknown;
    return read_u8(raw_data(), offset) & 0x07;
}

uint8_t ProcessorInformationEntry::get_processor_upgrade() const
{
    constexpr size_t offset = 0x19;
    constexpr size_t min_length = 0x1A;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_l1_cache_handle() const
{
    constexpr size_t offset = 0x1A;
    constexpr size_t min_length = 0x20;
    if (get_entry_size() < min_length) return 0xFFFF;
    return read_u16(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_l2_cache_handle() const
{
    constexpr size_t offset = 0x1C;
    constexpr size_t min_length = 0x20;
    if (get_entry_size() < min_length) return 0xFFFF;
    return read_u16(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_l3_cache_handle() const
{
    constexpr size_t offset = 0x1E;
    constexpr size_t min_length = 0x20;
    if (get_entry_size() < min_length) return 0xFFFF;
    return read_u16(raw_data(), offset);
}

uint8_t ProcessorInformationEntry::get_serial_number_index() const
{
    constexpr size_t offset = 0x20;
    constexpr size_t min_length = 0x23;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t ProcessorInformationEntry::get_asset_tag_index() const
{
    constexpr size_t offset = 0x21;
    constexpr size_t min_length = 0x23;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t ProcessorInformationEntry::get_part_number_index() const
{
    constexpr size_t offset = 0x22;
    constexpr size_t min_length = 0x23;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_core_count() const
{
    constexpr size_t offset = 0x23;
    constexpr size_t min_length = 0x28;
    if (get_entry_size() < min_length) return 0;

    const uint8_t core_count = read_u8(raw_data(), offset);

    // 3.0+ widened field: sentinel 0xFF at the legacy byte means "see the
    // wider field instead" - same idiom as BiosInformationEntry::
    // get_rom_size()'s 0xFF -> "see extended ROM size" precedent.
    constexpr size_t widened_offset = 0x2A;
    constexpr size_t widened_min_length = 0x2C;
    if (core_count == 0xFF && get_entry_size() >= widened_min_length) {
        return read_u16(raw_data(), widened_offset);
    }
    return core_count;
}

uint16_t ProcessorInformationEntry::get_core_enabled() const
{
    constexpr size_t offset = 0x24;
    constexpr size_t min_length = 0x28;
    if (get_entry_size() < min_length) return 0;

    const uint8_t core_enabled = read_u8(raw_data(), offset);

    constexpr size_t widened_offset = 0x2C;
    constexpr size_t widened_min_length = 0x2E;
    if (core_enabled == 0xFF && get_entry_size() >= widened_min_length) {
        return read_u16(raw_data(), widened_offset);
    }
    return core_enabled;
}

uint16_t ProcessorInformationEntry::get_thread_count() const
{
    constexpr size_t offset = 0x25;
    constexpr size_t min_length = 0x28;
    if (get_entry_size() < min_length) return 0;

    const uint8_t thread_count = read_u8(raw_data(), offset);

    constexpr size_t widened_offset = 0x2E;
    constexpr size_t widened_min_length = 0x30;
    if (thread_count == 0xFF && get_entry_size() >= widened_min_length) {
        return read_u16(raw_data(), widened_offset);
    }
    return thread_count;
}

uint16_t ProcessorInformationEntry::get_characteristics() const
{
    constexpr size_t offset = 0x26;
    constexpr size_t min_length = 0x28;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint16_t ProcessorInformationEntry::get_thread_enabled() const
{
    constexpr size_t offset = 0x30;
    constexpr size_t min_length = 0x32;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

std::string ProcessorInformationEntry::get_socket_designation_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_socket_designation_index());
}

std::string ProcessorInformationEntry::get_processor_type_string() const
{
    assert(!processor_type_map_.empty());
    return (*processor_type_map_.find(get_processor_type())).second;
}

std::string ProcessorInformationEntry::get_processor_family_string() const
{
    auto it = processor_family_map_.find(get_processor_family());
    if (it != processor_family_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string ProcessorInformationEntry::get_manufacturer_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_manufacturer_index());
}

std::string ProcessorInformationEntry::get_processor_id_string() const
{
    std::stringstream id_stream;
    id_stream << std::hex << std::setfill('0') << std::setw(16) << get_processor_id() << std::dec;
    return id_stream.str();
}

std::string ProcessorInformationEntry::get_version_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_version_index());
}

std::string ProcessorInformationEntry::get_voltage_string() const
{
    const uint8_t voltage = get_voltage();

    // Explicit voltage: bit 7 set, bits 0-6 are the value / 10 V. See
    // dmi_processor_voltage()'s own `code & 0x80` branch.
    if (voltage & 0x80) {
        const unsigned tenths = voltage & 0x7F;
        std::stringstream voltage_stream;
        voltage_stream << (tenths / 10) << '.' << (tenths % 10) << " V";
        return voltage_stream.str();
    }

    // Legacy bitmask of supported voltages: bits 0-2 may be set
    // simultaneously (a CPU may support more than one).
    std::string voltage_string;
    if (voltage & 0x01) voltage_string += "5.0 V";
    if (voltage & 0x02) voltage_string += (voltage_string.empty() ? "" : " ") + std::string("3.3 V");
    if (voltage & 0x04) voltage_string += (voltage_string.empty() ? "" : " ") + std::string("2.9 V");

    if (voltage_string.empty()) {
        return "Unknown";
    }
    return voltage_string;
}

std::string ProcessorInformationEntry::get_external_clock_string() const
{
    const uint16_t clock = get_external_clock();
    if (0 == clock) {
        return "Unknown";
    }
    return std::to_string(clock) + " MHz";
}

std::string ProcessorInformationEntry::get_max_speed_string() const
{
    const uint16_t speed = get_max_speed();
    if (0 == speed) {
        return "Unknown";
    }
    return std::to_string(speed) + " MHz";
}

std::string ProcessorInformationEntry::get_current_speed_string() const
{
    const uint16_t speed = get_current_speed();
    if (0 == speed) {
        return "Unknown";
    }
    return std::to_string(speed) + " MHz";
}

std::string ProcessorInformationEntry::get_status_string() const
{
    if (!get_populated()) {
        return "Unpopulated";
    }

    assert(!processor_status_map_.empty());
    auto it = processor_status_map_.find(get_status_code());
    const std::string status = (it != processor_status_map_.end()) ? (*it).second : "OutOfSpec";
    return "Populated, " + status;
}

std::string ProcessorInformationEntry::get_processor_upgrade_string() const
{
    auto it = processor_upgrade_map_.find(get_processor_upgrade());
    if (it != processor_upgrade_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string ProcessorInformationEntry::cache_handle_string(uint16_t handle) const
{
    // See dmi_processor_cache(): 0xFFFF means "no such cache", rendered as
    // "Not Provided" from SMBIOS 2.3 onward, "No <level> Cache" before that.
    if (0xFFFF == handle) {
        return cache_handle_version_ge_2_3_ ? "Not Provided" : "No Cache";
    }

    std::stringstream handle_stream;
    handle_stream << "0x" << std::hex << std::setfill('0') << std::setw(4) << handle << std::dec;
    return handle_stream.str();
}

std::string ProcessorInformationEntry::get_l1_cache_handle_string() const
{
    return cache_handle_string(get_l1_cache_handle());
}

std::string ProcessorInformationEntry::get_l2_cache_handle_string() const
{
    return cache_handle_string(get_l2_cache_handle());
}

std::string ProcessorInformationEntry::get_l3_cache_handle_string() const
{
    return cache_handle_string(get_l3_cache_handle());
}

std::string ProcessorInformationEntry::get_serial_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_serial_number_index());
}

std::string ProcessorInformationEntry::get_asset_tag_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_asset_tag_index());
}

std::string ProcessorInformationEntry::get_part_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_part_number_index());
}

std::string ProcessorInformationEntry::get_core_count_string() const
{
    const uint16_t count = get_core_count();
    if (0 == count) {
        return "Unknown";
    }
    return std::to_string(count);
}

std::string ProcessorInformationEntry::get_core_enabled_string() const
{
    const uint16_t count = get_core_enabled();
    if (0 == count) {
        return "Unknown";
    }
    return std::to_string(count);
}

std::string ProcessorInformationEntry::get_thread_count_string() const
{
    const uint16_t count = get_thread_count();
    if (0 == count) {
        return "Unknown";
    }
    return std::to_string(count);
}

std::string ProcessorInformationEntry::get_characteristics_string() const
{
    assert(!characteristics_map_.empty());
    const uint16_t characteristics = get_characteristics();
    // Mirrors dmi_processor_characteristics()'s own `(code & 0x00FC) == 0`
    // "None" check exactly (bits 2-7 only; the reference does not extend
    // this mask to cover bits 8-9 even though it does print them).
    if ((characteristics & 0x00FC) == 0) {
        return "\tNone\n";
    }
    return AbstractSMBiosEntry::bitset_to_properties<uint16_t>(characteristics, characteristics_map_);
}

std::string ProcessorInformationEntry::get_thread_enabled_string() const
{
    const uint16_t enabled = get_thread_enabled();
    if (0 == enabled) {
        return "Unknown";
    }
    return std::to_string(enabled);
}
