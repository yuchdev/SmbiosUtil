#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace smbios {

/// @brief Strategy interface for one way of obtaining the raw SMBIOS
/// structure-table bytes and the entry point's reported major/minor
/// version - a native OS mechanism (sysfs, UEFI/IORegistry, a native
/// firmware-table API) or an explicit last-resort fallback (the legacy
/// physical-memory scan).
///
/// `SMBios`'s constructor walks an ordered chain of these (see
/// `make_default_source_chain()`), calling `is_available()` then
/// `try_read()` on each in turn and stopping at the first one that
/// succeeds (milestone 0005, contract C1/C2). Implementations own
/// whatever platform-specific state they need internally; `SMBios`
/// itself depends only on this interface, never on a concrete source.
class ISmbiosSource {
public:
    virtual ~ISmbiosSource() = default;

    /// @brief Cheap check: can this source plausibly provide a table on
    /// this machine right now? Must not perform the (potentially
    /// expensive, potentially privileged) actual read.
    virtual bool is_available() const = 0;

    /// @brief Attempt the read.
    /// @param out_table Receives the raw SMBIOS structure-table bytes
    /// (header excluded) on success; left unspecified on failure.
    /// @param major_version Receives the entry point's major version on
    /// success; left unspecified on failure.
    /// @param minor_version Receives the entry point's minor version on
    /// success; left unspecified on failure.
    /// @return true if the table (and version) were read successfully;
    /// false if the source was available but the read still failed -
    /// never throws to signal that case.
    virtual bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const = 0;

    /// @brief Human-readable name for diagnostics (e.g. "linux-sysfs",
    /// "macos-uefi", "legacy-scan", "windows-native"). The returned
    /// pointer must remain valid for the lifetime of the source (a
    /// string literal is expected).
    virtual const char* source_name() const = 0;
};

} // namespace smbios
