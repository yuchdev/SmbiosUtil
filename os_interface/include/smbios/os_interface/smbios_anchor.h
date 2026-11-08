#pragma once
#include <algorithm>
#include <cstdint>

namespace smbios {

/// @brief Which SMBIOS entry-point anchor string, if any, was found at a
/// given position in a byte sequence - see `detect_smbios_anchor()`.
enum SMBiosAnchorType{
    ///< `"_SM_"` (4 bytes): a 32-bit entry point follows - decode via
    ///< `SMBIOSEntryPoint32`.
    SMBios32,

    ///< `"_SM3_"` (5 bytes): a 64-bit entry point follows - decode via
    ///< `SMBIOSEntryPoint64`.
    SMBios64,

    ///< `"_DMI_"` (5 bytes): the legacy intermediate anchor on its own,
    ///< without a preceding `_SM_`/`_SM3_` match at this position.
    SMBiosLegacy,

    ///< None of the recognized anchor strings matched at this position.
    NoHeader
};

/// @brief Checks whether any of the SMBIOS entry-point anchor strings
/// (`_SM_`, `_SM3_`, `_DMI_`) begin at `it`, without consuming or advancing
/// the iterator itself - callers (e.g. the legacy 16-byte-aligned physical
/// memory scan) call this at each candidate position and only commit to
/// parsing an entry point on a non-`NoHeader` result.
/// @tparam IterType A forward iterator over `uint8_t`-compatible elements;
/// must have at least 5 valid elements reachable from `it` (the longest
/// anchor, `_SM3_`) or this reads past the caller's intended range - the
/// caller is responsible for bounds-checking before calling, this function
/// does not.
/// @param it Iterator to the position to test; not advanced by this call.
/// @return The anchor type found at `it`, or `NoHeader` if none matched.
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
