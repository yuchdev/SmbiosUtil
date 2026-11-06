#pragma once
#include <cstddef>
#include <cstdint>

namespace smbios {

/// @brief Correct known-malformed SMBIOS entry-point version pairs
/// reported by some real-world BIOSes, before an `SMBiosVersion` is built
/// from them.
///
/// Ported from `c-versions/dmidecode_linux/dmidecode.c`'s
/// `dmi_fixup_version()` (~line 5967): "Some BIOS report weird SMBIOS
/// version, fix that up." The reference packs `major`/`minor` into a
/// single `uint16_t` as `(major << 8) | minor` - where `minor` is the
/// *raw* reported byte, not a decimal-digit value, so a BIOS reporting
/// `major=2, minor=0x1F` (31 decimal) or `major=2, minor=0x21` (33
/// decimal) packs to `0x021F`/`0x0221` and is corrected to `2.3`; a BIOS
/// reporting `major=2, minor=0x33` (51 decimal) packs to `0x0233` and is
/// corrected to `2.6`. Any other pair, including already-valid versions,
/// passes through unchanged.
///
/// Shared (plan.md R9) by every `ISmbiosSource` implementation that
/// decodes a raw entry point - do not reimplement this table per source.
///
/// @param major In/out major version; corrected in place if malformed.
/// @param minor In/out minor version; corrected in place if malformed.
inline void fixup_smbios_version(size_t& major, size_t& minor)
{
    const uint16_t packed = static_cast<uint16_t>(((major & 0xFF) << 8) | (minor & 0xFF));

    switch (packed) {
    case 0x021F: // 2.31 -> 2.3
    case 0x0221: // 2.33 -> 2.3
        major = 2;
        minor = 3;
        break;
    case 0x0233: // 2.51 -> 2.6
        major = 2;
        minor = 6;
        break;
    default:
        break;
    }
}

} // namespace smbios
