#pragma once
#include <algorithm>

namespace smbios {

enum SMBiosAnchorType{
    SMBios32,
    SMBios64,
    SMBiosLegacy,
    NoHeader
};

/// @brief Check any sequence (normally raw bytes) for the SMBIOS anchor
/// which is mark the beginning of entry point
template <typename IterType>
SMBiosAnchorType detect_smbios_anchor(IterType it)
{
    constexpr uint8_t smbios_32_header[] = {'_','S','M','_'};
    constexpr uint8_t smbios_64_header[] = {'_','S','M','3','_'};
    constexpr uint8_t smbios_legacy_header[] = {'_','D','M','I','_'};

    if(std::equal(std::begin(smbios_32_header), std::end(smbios_32_header), it)){
        return SMBiosAnchorType::SMBios32;
    }

    if(std::equal(std::begin(smbios_64_header), std::end(smbios_64_header), it)){
        return SMBiosAnchorType::SMBios64;
    }

    if(std::equal(std::begin(smbios_legacy_header), std::end(smbios_legacy_header), it)){
        return SMBiosAnchorType::SMBiosLegacy;
    }
    return SMBiosAnchorType::NoHeader;
}

} // namespace smbios
