#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include <smbios/sys_interface/ismbios_source.hpp>

namespace smbios {

/// @brief `ISmbiosSource` reading the SMBIOS entry point and raw
/// structure table off the macOS IORegistry via Task 01's
/// `read_apple_smbios_property()` - the mechanism this project's own
/// `c-versions/dmidecode_macos/util.c`'s `mem_chunk_ioreg()` confirms,
/// reached through `IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS`'s
/// `"SMBIOS-EPS"` (entry point) and `"SMBIOS"` (structure table) CFData
/// properties.
///
/// Two reads:
/// 1. `"SMBIOS-EPS"` (capped at 0x20 bytes - matches Task 01's own test
///    and the reference's own `mem_chunk_ioreg(0xF0000, ...)` call size,
///    safely covering either entry-point layout), decoded with milestone
///    0003's `read_uN` accessors (`smbios/include/smbios/sys_interface/
///    byte_reader.h`) exactly as the Linux sources decode the identical
///    wire format - no macOS-specific byte-decoding logic here.
/// 2. `"SMBIOS"`, capped at the entry point's own declared table length
///    (`_SM_`: `read_u16` at 0x16; `_SM3_`: `read_u32` at 0x0C) - **not**
///    a hardcoded size. This is a deliberate resolution of an
///    inconsistency between this task's own spec file (which suggests a
///    magic 0x10000 cap, reasoning "no reliable length from entry point")
///    and the story README's "Key constraints" section, which explicitly
///    says the entry point's length field is still worth reading "to
///    size the read request / sanity-check against what comes back" -
///    only the *address* field is meaningless for this source (IORegistry
///    ignores whatever address is requested and simply returns the whole
///    stored `"SMBIOS"` blob, per the confirmed reference). This is
///    consistent with Task 01's own truncate-not-pad contract on
///    `read_apple_smbios_property()`: if the real property is shorter
///    than declared, that contract already yields exactly the real bytes
///    with no padding; if the declared length ever undershoots the real
///    property, a correctly-sized read still beats relying on an
///    arbitrary guess like `0x10000`.
///
/// Milestone 0005, Story 03.0, Task 02.
class MacOsUefiSource : public ISmbiosSource {
public:
    /// @brief Cheap check: re-reads the `"SMBIOS-EPS"` property (same
    /// cost as `try_read()`'s own first stage - IORegistry property reads
    /// are inexpensive and need no root/admin privilege (R7)) and checks
    /// it is non-empty with a recognized anchor (`SMBios32`/`SMBios64`,
    /// not `NoHeader`/`SMBiosLegacy`).
    bool is_available() const override;

    /// @brief Stage 1: read and decode `"SMBIOS-EPS"`. Stage 2: read
    /// `"SMBIOS"`, capped at Stage 1's declared table length. Never
    /// throws - a missing registry entry, an unrecognized/too-short entry
    /// point, or a failed/empty second read all degrade to returning
    /// `false` with `out_table` left untouched.
    bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const override;

    /// @brief Diagnostic name surfaced by `SMBios::render_to_description()`.
    const char* source_name() const override { return "macos-uefi"; }
};

} // namespace smbios
