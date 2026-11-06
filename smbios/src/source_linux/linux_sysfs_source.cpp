#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <smbios/source_linux/linux_sysfs_source.h>
#include <smbios/source_interface/byte_reader.h>
#include <smbios/source_interface/smbios_anchor.h>
#include <smbios/source_interface/smbios_version_fixup.h>

using namespace smbios;

namespace {

/// @brief Minimum entry-point buffer size needed to safely read the
/// 32-bit (`_SM_`/`_DMI_`) layout's furthest field used here
/// (`smbios_structures_number` (u16) at offset 0x1C).
constexpr size_t kMinEntryPoint32Size = 0x1E;

/// @brief Minimum entry-point buffer size needed to safely read the
/// 64-bit (`_SM3_`) layout's furthest field used here (the max structure
/// size (u32) at offset 0x0C).
constexpr size_t kMinEntryPoint64Size = 0x10;

/// @brief Read a whole file into `out_bytes`, binary mode, no seeking.
/// Returns false (leaving `out_bytes` unspecified) if the file cannot be
/// opened; an empty-but-openable file yields `out_bytes.empty() == true`
/// and a `true` return - the caller decides whether empty is a failure.
bool read_entire_file(const std::string& path, std::vector<uint8_t>& out_bytes)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    out_bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

/// @brief Real permission probe for one sysfs file: open read-only and
/// close immediately. Unlike `std::filesystem::exists()`, this correctly
/// reports `false` when the file exists but the current user lacks read
/// permission (some distributions restrict these files to root).
bool is_readable(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return file.is_open();
}

/// @brief Decode major/minor version plus the raw anchor type from the
/// entry-point file's bytes, reading fields directly at their documented
/// offsets (see `LinuxSysfsSource`'s class comment for why this does not
/// overlay `SMBIOSEntryPoint32`/`64`). Returns `NoHeader` - and leaves
/// `major`/`minor` unspecified - if the anchor is unrecognized or the
/// buffer is too short to safely read the fields its anchor type implies.
SMBiosAnchorType decode_entry_point(const std::vector<uint8_t>& entry_point, size_t& major, size_t& minor)
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
        major = static_cast<size_t>(read_u8(data, 0x07));
        minor = static_cast<size_t>(read_u8(data, 0x08));
        return SMBiosAnchorType::SMBios64;

    case SMBiosAnchorType::SMBios32:
    case SMBiosAnchorType::SMBiosLegacy:
        // Sysfs skips straight to the (legacy, `_DMI_`-rooted) structure
        // table layout - no intermediate anchor to walk through - so both
        // anchor types share the same field offsets here.
        if (entry_point.size() < kMinEntryPoint32Size) {
            return SMBiosAnchorType::NoHeader;
        }
        major = static_cast<size_t>(read_u8(data, 0x06));
        minor = static_cast<size_t>(read_u8(data, 0x07));
        return anchor;

    default:
        return SMBiosAnchorType::NoHeader;
    }
}

} // namespace

LinuxSysfsSource::LinuxSysfsSource(std::string entry_point_path, std::string table_path)
    : entry_point_path_(std::move(entry_point_path)), table_path_(std::move(table_path))
{
}

bool LinuxSysfsSource::is_available() const
{
    return is_readable(entry_point_path_) && is_readable(table_path_);
}

bool LinuxSysfsSource::try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const
{
    std::vector<uint8_t> entry_point;
    if (!read_entire_file(entry_point_path_, entry_point)) {
        return false;
    }

    size_t local_major = 0;
    size_t local_minor = 0;
    const SMBiosAnchorType anchor = decode_entry_point(entry_point, local_major, local_minor);
    if (anchor == SMBiosAnchorType::NoHeader) {
        return false;
    }

    // plan.md R9: applied immediately after decoding, before anything else
    // uses the version - matches dmidecode's own `dmi_fixup_version()`
    // call ordering.
    fixup_smbios_version(local_major, local_minor);

    // Read the DMI file whole, from its own offset 0, exactly as
    // dmidecode's dmi_table_get() does for FLAG_NO_FILE_OFFSET sources:
    // the entry point's declared table length is only a size hint/cap for
    // this source, not a hard requirement. A shorter-than-announced file
    // is expected (SMBIOS v3 only announces a maximum size) and tolerated,
    // not an error.
    std::vector<uint8_t> table;
    if (!read_entire_file(table_path_, table) || table.empty()) {
        return false;
    }

    out_table = std::move(table);
    major_version = local_major;
    minor_version = local_minor;
    return true;
}
