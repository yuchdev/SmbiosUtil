#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <smbios/sys_interface/ismbios_source.hpp>

namespace smbios {

/// @brief `ISmbiosSource` reading the modern Linux kernel-exposed sysfs
/// mirror of the SMBIOS entry point and structure table - no raw
/// `/dev/mem` access needed - direct-ported from
/// `c-versions/dmidecode_linux/dmidecode.c`'s `SYS_ENTRY_FILE`
/// (`/sys/firmware/dmi/tables/smbios_entry_point`) / `SYS_TABLE_FILE`
/// (`/sys/firmware/dmi/tables/DMI`) constants and its "First try reading
/// from sysfs tables" block in `main()`.
///
/// `smbios_entry_point` is parsed by reading raw bytes directly with
/// milestone 0003's `read_u8`/`read_u16`/`read_u32` accessors
/// (`smbios/include/smbios/sys_interface/byte_reader.h`) - deliberately
/// not via a `reinterpret_cast` onto `SMBIOSEntryPoint32`/`64` (those
/// structs document the wire layout for reference only; their
/// `structure_table_address` member is an `intptr_t`, i.e. a
/// platform-dependent size, which would silently misalign every field
/// after it on a real overlay).
///
/// `DMI` is read in full, from its own offset 0, exactly as dmidecode's
/// `dmi_table_get()` does for `FLAG_NO_FILE_OFFSET` sources: the entry
/// point's declared table length is only a size hint/cap for this source,
/// not a hard requirement - a shorter actual file (expected for SMBIOS v3,
/// which only announces a maximum size) is tolerated, not an error.
///
/// Milestone 0005, Story 02.0, Task 01.
class LinuxSysfsSource : public ISmbiosSource {
public:
    /// @brief Reads from the real sysfs paths - used everywhere in
    /// production, including `make_default_source_chain()`.
    LinuxSysfsSource() = default;

    /// @brief Testability seam only: points this source at an arbitrary
    /// pair of files instead of the real sysfs paths, so
    /// `tests/unit/linux_sysfs_source_test.cpp` can exercise
    /// `is_available()`/`try_read()` against a temp-directory fixture
    /// mimicking `/sys/firmware/dmi/tables/`'s layout without touching
    /// real sysfs. Not used by `make_default_source_chain()` or any other
    /// production call site.
    LinuxSysfsSource(std::string entry_point_path, std::string table_path);

    /// @brief Opens both `smbios_entry_point` and `DMI` read-only and
    /// closes them immediately - a real permission probe, not just a
    /// `std::filesystem::exists()` path check, since sysfs commonly
    /// restricts these files to root on some distributions.
    bool is_available() const override;

    /// @brief Parse the entry point's anchor/version/length fields and
    /// read the `DMI` file whole as the raw structure table. Never
    /// throws - a missing/unopenable file, an unrecognized anchor, or an
    /// empty `DMI` file all degrade to returning `false` with `out_table`
    /// left untouched.
    bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const override;

    /// @brief Diagnostic name surfaced by `SMBios::render_to_description()`.
    const char* source_name() const override { return "linux-sysfs"; }

private:
    /// @brief `c-versions/dmidecode_linux/dmidecode.c`'s `SYS_ENTRY_FILE`.
    std::string entry_point_path_{"/sys/firmware/dmi/tables/smbios_entry_point"};
    /// @brief `c-versions/dmidecode_linux/dmidecode.c`'s `SYS_TABLE_FILE`.
    std::string table_path_{"/sys/firmware/dmi/tables/DMI"};
};

} // namespace smbios
