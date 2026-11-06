#include <algorithm>
#include <vector>

#include <smbios/legacy_memory_scan_source.h>
#include <smbios/physical_memory.h>
#include <smbios/smbios.h>
#include <smbios/smbios_anchor.h>
#include <smbios/smbios_version_fixup.h>

using namespace smbios;

namespace {

/// @brief Scan this legacy BIOS shadow-ROM window for an SMBIOS entry
/// point. Unchanged from the pre-refactor `SMBios::devmem_base_`/
/// `devmem_length_` members.
constexpr size_t kLegacyScanBase = 0xF0000;
constexpr size_t kLegacyScanLength = 0x10000;

/// @brief Checksum from `start_offset` to `start_offset + length` over
/// `entry_point_buffer` should be zero (SMBIOS spec). Mirrors the
/// pre-refactor `SMBios::smbios_crc()` exactly.
uint8_t smbios_crc(const std::vector<uint8_t>& entry_point_buffer, uint8_t start_offset, uint8_t length)
{
    uint8_t sum{};
    for (size_t a = start_offset; a < length; a++) {
        sum += entry_point_buffer[a];
    }
    return sum;
}

/// @brief Validate a 32-bit entry point's intermediate anchor and both of
/// its checksums. Mirrors the pre-refactor `SMBios::checksum32()` exactly.
bool checksum32(const std::vector<uint8_t>& entry_point_buffer, const SMBIOSEntryPoint32* entry32)
{
    uint8_t smbios_intermediate_anchor[] = {'_', 'D', 'M', 'I', '_'};
    bool is_anchor_correct = std::equal(
        std::begin(smbios_intermediate_anchor),
        std::end(smbios_intermediate_anchor),
        entry32->intermediate_anchor);

    bool crc_correct = (0u == smbios_crc(entry_point_buffer, 0, entry32->entry_point_length)
                         && (0u == smbios_crc(entry_point_buffer, 0x10, 0x0F)));

    return is_anchor_correct && crc_correct;
}

/// @brief Validate a 64-bit entry point's checksum. Mirrors the
/// pre-refactor `SMBios::checksum64()` exactly.
bool checksum64(const std::vector<uint8_t>& entry_point_buffer, const SMBIOSEntryPoint64* entry64)
{
    return (0u == smbios_crc(entry_point_buffer, 0, entry64->entry_point_length));
}

/// @brief Scan `devmem_array` 16 bytes at a time for a `_SM_`/`_SM3_`
/// anchor, validating whichever entry point is found. Mirrors the
/// pre-refactor `SMBios::scan_physical_memory()` exactly - the only
/// difference is the entry-point pointers and the validated flag are
/// returned via out-parameter instead of being stored on `SMBios` itself.
void scan_physical_memory_for_entry_point(
    const std::vector<uint8_t>& devmem_array,
    std::vector<uint8_t>& entry_point_buffer,
    const SMBIOSEntryPoint32*& entry32,
    const SMBIOSEntryPoint64*& entry64,
    bool& checksum_validated)
{
    constexpr size_t smbios32_header_size = sizeof(SMBIOSEntryPoint32);
    constexpr size_t smbios64_header_size = sizeof(SMBIOSEntryPoint64);

    for (auto it = devmem_array.begin(); std::distance(it, devmem_array.end()) > 16; it += 16) {

        if (detect_smbios_anchor(it) == SMBiosAnchorType::SMBios32) {
            entry_point_buffer.assign(it, it + smbios32_header_size);
            entry32 = reinterpret_cast<const SMBIOSEntryPoint32*>(&entry_point_buffer[0]);
            checksum_validated = checksum32(entry_point_buffer, entry32);
        }

        if (detect_smbios_anchor(it) == SMBiosAnchorType::SMBios64) {
            entry_point_buffer.assign(it, it + smbios64_header_size);
            entry64 = reinterpret_cast<const SMBIOSEntryPoint64*>(&entry_point_buffer[0]);
            checksum_validated = checksum64(entry_point_buffer, entry64);
        }
    }
}

} // namespace

bool LegacyMemoryScanSource::is_available() const
{
    // See the class-level comment: there is no cheaper, meaningful
    // availability probe than attempting the scan itself, and this source
    // is only ever reached last, after every other source has already
    // failed. try_read() reports success/failure precisely.
    return true;
}

bool LegacyMemoryScanSource::try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const
{
    // Stage 1: map the legacy BIOS shadow-ROM window and scan it for an
    // entry point anchor.
    PhysicalMemory physical_memory_device;
    std::vector<uint8_t> devmem_array;

    try {
        physical_memory_device.map_physical_memory(kLegacyScanBase, kLegacyScanLength);
        devmem_array = physical_memory_device.get_memory_dump(0, kLegacyScanLength);
    }
    catch (const std::exception&) {
        // No accessible legacy physical-memory device on this host (e.g.
        // no /dev/mem at all - removed on modern macOS - or insufficient
        // privilege on *nix/Windows): degrade to "nothing found" rather
        // than letting the exception escape, consistent with this
        // project's convention of failing an inaccessible/malformed
        // source cleanly rather than with an uncaught exception.
        devmem_array.clear();
    }

    std::vector<uint8_t> entry_point_buffer;
    const SMBIOSEntryPoint32* entry32 = nullptr;
    const SMBIOSEntryPoint64* entry64 = nullptr;
    bool checksum_validated = true;

    scan_physical_memory_for_entry_point(devmem_array, entry_point_buffer, entry32, entry64, checksum_validated);

    // Stage 2: entry-point version, corrected for known-malformed BIOS
    // reports (plan.md R9) before it is used for anything else.
    size_t local_major = 0;
    size_t local_minor = 0;
    if (entry32 && checksum_validated) {
        local_major = static_cast<size_t>(entry32->major_version);
        local_minor = static_cast<size_t>(entry32->minor_version);
    }
    if (entry64 && checksum_validated) {
        local_major = static_cast<size_t>(entry64->major_version);
        local_minor = static_cast<size_t>(entry64->minor_version);
    }
    fixup_smbios_version(local_major, local_minor);

    // Stage 3: map and read the real structure table the entry point
    // points to.
    size_t smbios_base{};
    size_t smbios_table_length{};

    if (entry32 && checksum_validated) {
        smbios_base = entry32->structure_table_address;
        smbios_table_length = entry32->structure_table_length;
    }
    if (entry64 && checksum_validated) {
        smbios_base = entry64->structure_table_address;
        smbios_table_length = entry64->max_structure_size;
    }

    PhysicalMemory smbios_physical_memory;
    std::vector<uint8_t> dump;

    try {
        smbios_physical_memory.map_physical_memory(smbios_base, smbios_table_length);
        dump = smbios_physical_memory.get_memory_dump(0, smbios_table_length);
    }
    catch (const std::exception&) {
        // same rationale as above
        dump.clear();
    }

    if (dump.empty()) {
        return false;
    }

    out_table = std::move(dump);
    major_version = local_major;
    minor_version = local_minor;
    return true;
}

const char* LegacyMemoryScanSource::source_name() const
{
    return "legacy-scan";
}
