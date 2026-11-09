#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <smbios/linux/linux_efi_source.hpp>
#include <smbios/legacy/physical_memory.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <smbios/sys_interface/smbios_anchor.hpp>
#include <smbios/sys_interface/smbios_version_fixup.hpp>

using namespace smbios;

namespace {

/// @brief `mem_chunk(fp, 0x20, opt.devmem)`'s own constant: 32 bytes
/// safely covers either entry-point layout's checksum-relevant fields
/// (far enough to also reach the structure-table address/length fields
/// this source, unlike `LinuxSysfsSource`, actually uses).
constexpr size_t kEntryPointProbeLength = 0x20;

/// @brief Minimum bytes needed to safely read the 32-bit (`_SM_`) entry
/// point's structure-table address field: `read_u32` at offset 0x18.
constexpr size_t kMinEntryPoint32Size = 0x1C;

/// @brief Minimum bytes needed to safely read the 64-bit (`_SM3_`) entry
/// point's structure-table address field: `read_u64` at offset 0x10.
constexpr size_t kMinEntryPoint64Size = 0x18;

/// @brief Decoded Stage-2 result: version plus where/how big the real
/// structure table is.
struct DecodedEntryPoint {
    size_t major_version{};
    size_t minor_version{};
    size_t table_address{};
    size_t table_length{};
};

/// @brief Decode the entry point bytes `PhysicalMemory` dumped at the
/// Stage-1 address: anchor, major/minor version, and - unlike
/// `LinuxSysfsSource`, which deliberately ignores these fields - the
/// structure-table address/length this source needs to find the second
/// mapping. Offsets confirmed against
/// `c-versions/dmidecode_linux/dmidecode.c`'s `smbios3_decode()`/
/// `smbios_decode()`.
///
/// A bare `_DMI_` (legacy) anchor is treated as unrecognized here, same
/// as `LinuxSysfsSource`: that anchor implies `legacy_decode()`'s
/// completely different field layout, which this source is not scoped to
/// support.
///
/// @return true and fills `out` on a recognized, safely-sized anchor;
/// false (leaving `out` unspecified) otherwise.
bool decode_entry_point(const std::vector<uint8_t>& entry_point, DecodedEntryPoint& out)
{
    if (entry_point.size() < 5) {
        return false;
    }

    const SMBiosAnchorType anchor = detect_smbios_anchor(entry_point.begin());
    const uint8_t* data = entry_point.data();

    switch (anchor) {
    case SMBiosAnchorType::SMBios64:
        if (entry_point.size() < kMinEntryPoint64Size) {
            return false;
        }
        out.major_version = static_cast<size_t>(read_u8(data, 0x07));
        out.minor_version = static_cast<size_t>(read_u8(data, 0x08));
        out.table_length = static_cast<size_t>(read_u32(data, 0x0C));
        out.table_address = static_cast<size_t>(read_u64(data, 0x10));
        return true;

    case SMBiosAnchorType::SMBios32:
        if (entry_point.size() < kMinEntryPoint32Size) {
            return false;
        }
        out.major_version = static_cast<size_t>(read_u8(data, 0x06));
        out.minor_version = static_cast<size_t>(read_u8(data, 0x07));
        out.table_length = static_cast<size_t>(read_u16(data, 0x16));
        out.table_address = static_cast<size_t>(read_u32(data, 0x18));
        return true;

    case SMBiosAnchorType::SMBiosLegacy:
    default:
        return false;
    }
}

} // namespace

LinuxEfiSource::LinuxEfiSource(std::string systab_path_primary, std::string systab_path_fallback)
    : systab_path_primary_(std::move(systab_path_primary)), systab_path_fallback_(std::move(systab_path_fallback))
{
}

bool LinuxEfiSource::find_efi_smbios_address(size_t& out_address) const
{
    // `address_from_efi()`: try the primary path first, then the
    // fallback only if the primary did not open at all.
    std::ifstream systab_file(systab_path_primary_);
    if (!systab_file.is_open()) {
        systab_file.open(systab_path_fallback_);
    }
    if (!systab_file.is_open()) {
        // No EFI interface at all.
        return false;
    }

    std::string line;
    while (std::getline(systab_file, line)) {
        // `strchr(linebuf, '=')` / splitting the line at its first '='.
        const size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, equals_pos);
        if (key != "SMBIOS3" && key != "SMBIOS") {
            continue;
        }

        const std::string value = line.substr(equals_pos + 1);
        try {
            // `strtoull(addrp, NULL, 0)`: base 0 auto-detects a `0x`
            // prefix, same as std::stoull's base-0 behavior.
            out_address = static_cast<size_t>(std::stoull(value, nullptr, 0));
        }
        catch (const std::exception&) {
            // A line that matched the key but had an unparsable value -
            // treat as "found interface but no SMBIOS pointer", same as
            // no matching line at all.
            return false;
        }

        // dmidecode's own loop breaks on the first match.
        return true;
    }

    // File opened but no SMBIOS/SMBIOS3 line found.
    return false;
}

bool LinuxEfiSource::is_available() const
{
    size_t address = 0;
    return find_efi_smbios_address(address);
}

bool LinuxEfiSource::try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const
{
    // Stage 1: re-run the (cheap) systab parse - no state is shared with
    // is_available(), matching this project's other ISmbiosSource
    // implementations.
    size_t entry_point_address = 0;
    if (!find_efi_smbios_address(entry_point_address)) {
        return false;
    }

    // Stage 2a: map and dump the entry point itself, at the address EFI
    // published - the opposite mechanic from LinuxSysfsSource's "ignore
    // the address field, read the whole file": this is a real
    // /dev/mem mapping, exactly like LegacyMemoryScanSource.
    PhysicalMemory entry_point_memory;
    std::vector<uint8_t> entry_point_dump;

    try {
        entry_point_memory.map_physical_memory(entry_point_address, kEntryPointProbeLength);
        entry_point_dump = entry_point_memory.get_memory_dump(0, kEntryPointProbeLength);
    }
    catch (const std::exception&) {
        // No accessible /dev/mem (insufficient privilege, or no such
        // device on this host): degrade to "nothing found" rather than
        // letting the exception escape, consistent with
        // LegacyMemoryScanSource's convention.
        entry_point_dump.clear();
    }

    if (entry_point_dump.empty()) {
        return false;
    }

    DecodedEntryPoint decoded;
    if (!decode_entry_point(entry_point_dump, decoded)) {
        return false;
    }

    // plan.md R9: applied immediately after decoding, before anything
    // else uses the version - matches dmidecode's own
    // dmi_fixup_version() call ordering.
    fixup_smbios_version(decoded.major_version, decoded.minor_version);

    // Stage 2b: map and dump the real structure table at the
    // address/length the entry point itself named. Unlike Stage 2a's
    // fixed-size probe, this is the exact requested length with no
    // truncation tolerance - a short/failed map or an empty dump is a
    // hard failure.
    PhysicalMemory table_memory;
    std::vector<uint8_t> table_dump;

    try {
        table_memory.map_physical_memory(decoded.table_address, decoded.table_length);
        table_dump = table_memory.get_memory_dump(0, decoded.table_length);
    }
    catch (const std::exception&) {
        // same rationale as above
        table_dump.clear();
    }

    if (table_dump.empty()) {
        return false;
    }

    out_table = std::move(table_dump);
    major_version = decoded.major_version;
    minor_version = decoded.minor_version;
    return true;
}
