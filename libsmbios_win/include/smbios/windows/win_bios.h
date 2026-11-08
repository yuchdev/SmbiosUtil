#pragma once
#include <vector>
#include <cstdint>
#include <memory>

#if defined(_WIN32) || defined(_WIN64)

namespace smbios {

class NativeSystemInformation;
class PhysicalMemory;

/// @brief The buffer layout `GetSystemFirmwareTable('RSMB', ...)` fills:
/// its own small header, immediately followed by the raw SMBIOS structure
/// table bytes. `SMBiosImpl::compose_native_smbios_table()` reinterprets
/// the raw byte buffer it allocated as this struct in place - there is no
/// separate copy.
struct RawSMBIOSData {
    ///< Reserved/informational calling-method identifier the firmware
    ///< table provider writes; not interpreted by this project.
    uint8_t calling_method;

    ///< SMBIOS specification major version reported by the firmware table
    ///< provider.
    uint8_t major_version;

    ///< SMBIOS specification minor version reported by the firmware table
    ///< provider.
    uint8_t minor_version;

    ///< DMI/SMBIOS BCD revision; not otherwise interpreted by this
    ///< project.
    uint8_t dmi_revision;

    ///< Length, in bytes, of `smbios_table_data` that follows (i.e. the
    ///< raw structure table size, header excluded).
    uint32_t length;

    ///< First byte of the raw SMBIOS structure table; the array is
    ///< declared `[1]` only as a C-style "flexible array member" idiom -
    ///< the real extent is `length` bytes, read via `get_table_base()`/
    ///< `get_table_size()` rather than indexing past this declared bound
    ///< directly.
    uint8_t smbios_table_data[1];
};

/// @brief Wraps `GetSystemFirmwareTable('RSMB', ...)` (Vista+, via
/// `kernel32.dll`'s runtime-resolved export, see `is_ntdll_compatible()`)
/// to obtain the raw SMBIOS table, owning the allocated buffer and
/// exposing the table's base offset/size and version once read.
class SMBiosImpl
{
public:

    /// @brief Attempts the `GetSystemFirmwareTable()` read immediately;
    /// never throws. Check `smbios_read_success()` afterward - on an
    /// unsupported OS (pre-Vista) or if the kernel32 export is not the
    /// genuine `GetSystemFirmwareTable` (see `is_ntdll_compatible()`),
    /// this object is simply left with no table, not an error.
    SMBiosImpl();

    /// @brief Destructor; empty body - `table_buffer_`/
    /// `native_system_information_` are self-managed, and `smbios_data_`
    /// is a non-owning view into `table_buffer_`.
    ~SMBiosImpl();

    /// @brief Whether the constructor successfully obtained a non-empty
    /// SMBIOS table.
    /// @return `true` if `table_buffer_` is non-empty and `smbios_data_`
    /// was set; `false` otherwise (unsupported OS, failed/zero-length
    /// `GetSystemFirmwareTable()` call, or a table supplied later via
    /// `read_from_physical_memory()` that was never reinterpreted as a
    /// `RawSMBIOSData` by this class).
    bool smbios_read_success() const;

    /// @brief Reinterprets the owned buffer as `RawSMBIOSData`.
    /// @return pointer into `table_buffer_`; only valid while this object
    /// is alive. Caller must check `smbios_read_success()` first - calling
    /// this when it would return `false` asserts in debug builds.
    RawSMBIOSData* get_formatted_smbios_table() const;

    /// @brief Actual table base (offset from header beginning)
    /// @return pointer to the first byte of `smbios_table_data`, or
    /// `nullptr` if no table was successfully read.
    uint8_t* get_table_base()  const;

    /// @brief Actual table size from table beginning (without header)
    /// @return `0` if no table was ever read; otherwise `RawSMBIOSData::length`
    /// when the buffer was obtained via `GetSystemFirmwareTable()`, or the
    /// raw buffer's own size when it was instead supplied via
    /// `read_from_physical_memory()` (no `RawSMBIOSData` header to read a
    /// length from in that case).
    size_t get_table_size()  const;

    /// @brief Major version (from header)
    /// @return the major version from `RawSMBIOSData`, or
    /// `std::numeric_limits<size_t>::max()` if no table was read.
    size_t get_major_version() const;

    /// @brief Minor version (from header)
    /// @return the minor version from `RawSMBIOSData`, or
    /// `std::numeric_limits<size_t>::max()` if no table was read.
    size_t get_minor_version() const;

    /// @brief Legacy-fallback path: adopts an already-captured physical
    /// memory dump (e.g. from the legacy BIOS shadow-ROM scan) as this
    /// object's table buffer instead of a `GetSystemFirmwareTable()` read.
    /// @param physical_memory_dump The raw bytes to adopt; moved out of
    /// (left in an unspecified but valid state) rather than copied - this
    /// parameter is intentionally non-const so the caller's buffer can be
    /// transferred without an extra allocation. No `RawSMBIOSData` header
    /// is assumed to be present in this buffer - `smbios_data_` is left
    /// null, so `get_major_version()`/`get_minor_version()` return the
    /// "not read" sentinel afterward.
    void read_from_physical_memory(std::vector<uint8_t>& physical_memory_dump);

private:

    /// @brief Checks that `kernel32.dll`'s exported `GetSystemFirmwareTable`
    /// symbol resolves to the same address the statically-linked import
    /// does, guarding against a tampered/incompatible DLL silently
    /// exporting an unrelated function under the same name.
    bool is_ntdll_compatible() const;

    /// @brief Populates `table_buffer_`/`smbios_data_` via
    /// `GetSystemFirmwareTable('RSMB', ...)`, gated on
    /// `native_system_information_->major_version() >= 6` (Vista+) and
    /// `is_ntdll_compatible()`. Leaves both untouched (empty/null) if
    /// either precondition fails or the call reports zero size.
    void compose_native_smbios_table();

    /// Save table with header here
    std::vector<uint8_t> table_buffer_;

    std::unique_ptr<NativeSystemInformation> native_system_information_;

    /// Apply to the table with header
    RawSMBIOSData* smbios_data_ = nullptr;
};

} // namespace smbios

#endif // defined(_WIN32) || defined(_WIN64)
