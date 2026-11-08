#include <smbios/macos/macos_uefi_source.h>

#include <optional>

#include <smbios/os_interface/byte_reader.h>
#include <smbios/os_interface/smbios_anchor.h>
#include <smbios/os_interface/smbios_version_fixup.h>
#include <smbios/macos/macos_efi_reader.h>

using namespace smbios;

namespace {

/// @brief `read_apple_smbios_property("SMBIOS-EPS", ...)`'s own cap: 0x20
/// bytes matches Task 01's own test and the reference's own
/// `mem_chunk_ioreg(0xF0000, ...)` call size, safely covering either
/// entry-point layout's version/length fields used here.
constexpr size_t kEntryPointMaxLen = 0x20;

/// @brief Minimum entry-point buffer size needed to safely read the
/// 32-bit (`_SM_`) layout's furthest field used here (declared
/// `structure_table_length`, a u16, at offset 0x16).
constexpr size_t kMinEntryPoint32Size = 0x18;

/// @brief Minimum entry-point buffer size needed to safely read the
/// 64-bit (`_SM3_`) layout's furthest field used here (declared max
/// structure size, a u32, at offset 0x0C).
constexpr size_t kMinEntryPoint64Size = 0x10;

/// @brief Decoded `"SMBIOS-EPS"` result: version plus the entry point's
/// own declared structure-table length. Unlike `LinuxEfiSource`'s
/// `DecodedEntryPoint`, there is no decoded *address* field here - see
/// `MacOsUefiSource`'s header doc comment for why the address is
/// meaningless on this source (IORegistry ignores whatever address it is
/// asked for).
struct DecodedEntryPoint {
    size_t major_version{};
    size_t minor_version{};
    size_t table_length{};
};

/// @brief Decode the `"SMBIOS-EPS"` property bytes: anchor, major/minor
/// version, and the entry point's own declared structure-table length -
/// at the same offsets the Linux sources use for the identical wire
/// format (see `linux_sysfs_source.cpp`/`linux_efi_source.cpp`'s own
/// `decode_entry_point()` for the pattern this mirrors).
///
/// A bare `_DMI_` (legacy) anchor is treated as unrecognized, same
/// reasoning as both Linux sources: that anchor implies
/// `legacy_decode()`'s distinct layout (version packed as a single BCD
/// byte, not two bytes at these offsets), not a smaller case of
/// `smbios_decode()`'s layout.
///
/// @return the recognized anchor type (`SMBios32`/`SMBios64`) with `out`
/// filled; `NoHeader` (leaving `out` unspecified) if the anchor is
/// unrecognized or the buffer is too short to safely read the fields its
/// anchor type implies.
SMBiosAnchorType decode_entry_point(const std::vector<uint8_t>& entry_point, DecodedEntryPoint& out)
{
    if (entry_point.size() < 5) {
        return SMBiosAnchorType::NoHeader;
    }

    const SMBiosAnchorType anchor = detect_smbios_anchor(entry_point.begin());
    const uint8_t* data = entry_point.data();

    switch (anchor) {
    case SMBiosAnchorType::SMBios64:
        if (entry_point.size() < kMinEntryPoint64Size) {
            return SMBiosAnchorType::NoHeader;
        }
        out.major_version = static_cast<size_t>(read_u8(data, 0x07));
        out.minor_version = static_cast<size_t>(read_u8(data, 0x08));
        out.table_length = static_cast<size_t>(read_u32(data, 0x0C));
        return SMBiosAnchorType::SMBios64;

    case SMBiosAnchorType::SMBios32:
        if (entry_point.size() < kMinEntryPoint32Size) {
            return SMBiosAnchorType::NoHeader;
        }
        out.major_version = static_cast<size_t>(read_u8(data, 0x06));
        out.minor_version = static_cast<size_t>(read_u8(data, 0x07));
        out.table_length = static_cast<size_t>(read_u16(data, 0x16));
        return SMBiosAnchorType::SMBios32;

    case SMBiosAnchorType::SMBiosLegacy:
        return SMBiosAnchorType::NoHeader;

    default:
        return SMBiosAnchorType::NoHeader;
    }
}

} // namespace

bool MacOsUefiSource::is_available() const
{
    std::optional<std::vector<uint8_t>> entry_point = read_apple_smbios_property("SMBIOS-EPS", kEntryPointMaxLen);
    if (!entry_point.has_value() || entry_point->empty()) {
        return false;
    }

    DecodedEntryPoint unused;
    return decode_entry_point(*entry_point, unused) != SMBiosAnchorType::NoHeader;
}

bool MacOsUefiSource::try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const
{
    std::optional<std::vector<uint8_t>> entry_point = read_apple_smbios_property("SMBIOS-EPS", kEntryPointMaxLen);
    if (!entry_point.has_value() || entry_point->empty()) {
        return false;
    }

    DecodedEntryPoint decoded;
    if (decode_entry_point(*entry_point, decoded) == SMBiosAnchorType::NoHeader) {
        return false;
    }

    // plan.md R9: applied immediately after decoding, before anything
    // else uses the version - matches dmidecode's own
    // dmi_fixup_version() call ordering, and every other ISmbiosSource in
    // this project.
    fixup_smbios_version(decoded.major_version, decoded.minor_version);

    // Spec-vs-README resolution (see this class's header doc comment):
    // cap the second read at the entry point's own declared table
    // length, not the task spec's suggested hardcoded 0x10000 - the
    // story README's "Key constraints" section is explicit that only the
    // *address* field is meaningless for this source, not the length.
    std::optional<std::vector<uint8_t>> table = read_apple_smbios_property("SMBIOS", decoded.table_length);
    if (!table.has_value() || table->empty()) {
        return false;
    }

    out_table = std::move(*table);
    major_version = decoded.major_version;
    minor_version = decoded.minor_version;
    return true;
}
