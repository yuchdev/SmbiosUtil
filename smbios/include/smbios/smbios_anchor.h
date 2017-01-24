#pragma once
#include <algorithm>

namespace smbios {

enum SMBiosAnchorType{
    SMBios32,
    SMBios64,
    SMBiosLegacy,
    NoHeader
};

template <typename IterType>
SMBiosAnchorType detect_smbios_anchor(IterType it)
{
    uint8_t smbios_32_header[] = {'_','S','M','_'};
    uint8_t smbios_64_header[] = {'_','S','M','3','_'};
    uint8_t smbios_legacy_header[] = {'_','D','M','I','_'};

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
