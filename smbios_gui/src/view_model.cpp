/// @file view_model.cpp
/// @brief Implementation of smbios_gui's view model (contract GC1) and of
/// display_text(). Deliberately includes no Dear ImGui header - see
/// view_model.hpp.

#include <smbios/gui/view_model.hpp>

#include <cstdio>
#include <exception>
#include <memory>
#include <new>
#include <utility>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

namespace smbios::gui {

namespace {

/// U+FFFD REPLACEMENT CHARACTER, UTF-8 encoded.
constexpr char kReplacementUtf8[] = "\xEF\xBF\xBD";

/// Fixed parts of display_text()'s truncation suffix, around the count.
constexpr char kTruncatedPrefix[] = " ... [";
constexpr char kTruncatedSuffix[] = " bytes truncated]";

/// Widest possible decimal size_t (2^64 - 1 has 20 digits); used to reserve
/// room for the suffix before the exact count is known.
constexpr size_t kMaxCountDigits = 20;

/// @brief Decodes the code point starting at `text[pos]` and returns its
/// encoded length in bytes.
/// @pre `text` is valid UTF-8 (sanitize_utf8() output) and `pos` is a
/// code-point boundary inside it.
/// @param text valid UTF-8.
/// @param pos byte index of a lead byte.
/// @param[out] code_point the decoded code point.
/// @return 1-4, the sequence's length.
size_t decode_valid_utf8(const std::string& text, size_t pos, uint32_t& code_point)
{
    const auto byte_at = [&text](size_t i) {
        return static_cast<uint32_t>(static_cast<unsigned char>(text[i]));
    };
    const uint32_t lead = byte_at(pos);
    if (lead < 0x80) {
        code_point = lead;
        return 1;
    }
    if (lead < 0xE0) {
        code_point = ((lead & 0x1F) << 6) | (byte_at(pos + 1) & 0x3F);
        return 2;
    }
    if (lead < 0xF0) {
        code_point = ((lead & 0x0F) << 12) | ((byte_at(pos + 1) & 0x3F) << 6)
                   | (byte_at(pos + 2) & 0x3F);
        return 3;
    }
    code_point = ((lead & 0x07) << 18) | ((byte_at(pos + 1) & 0x3F) << 12)
               | ((byte_at(pos + 2) & 0x3F) << 6) | (byte_at(pos + 3) & 0x3F);
    return 4;
}

/// @brief True iff display_text() must replace `code_point` (threat model
/// G-4): C0, DEL, C1, U+2028/U+2029, U+202A-U+202E, U+2066-U+2069.
constexpr bool is_display_unsafe(uint32_t code_point)
{
    return code_point < 0x20
        || (code_point >= 0x7F && code_point <= 0x9F)
        || (code_point >= 0x2028 && code_point <= 0x202E)
        || (code_point >= 0x2066 && code_point <= 0x2069);
}

/// @brief True iff `byte` is a UTF-8 continuation byte (10xxxxxx).
constexpr bool is_continuation_byte(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

/// @brief Copies one entry's get_fields() result into FieldRows, preserving
/// order exactly.
/// The entry object lives only inside this function, so it is destroyed
/// while `bios` (and the table buffer it points into) is still alive -
/// nothing created from `bios` escapes (threat model G-7).
/// @return the rows; empty if no entry class is registered for the type.
/// @throws whatever create()/get_fields() throw; the caller isolates it.
std::vector<FieldRow> decode_fields(const DMIHeader& header, SMBios& bios)
{
    std::vector<FieldRow> rows;
    const std::unique_ptr<AbstractSMBiosEntry> entry = SMBiosEntryFactory{}.create(
        header, bios.get_smbios_version(), bios.get_table_base() + bios.get_table_size());
    if (!entry) {
        return rows;
    }
    std::vector<NamedField> fields = entry->get_fields();
    rows.reserve(fields.size());
    for (NamedField& field : fields) {
        rows.push_back(FieldRow{std::move(field.name), std::move(field.value)});
    }
    return rows;
}

} // namespace

std::vector<EntryNode> build_entry_tree(SMBios& bios)
{
    std::vector<EntryNode> nodes;
    for (const DMIHeader& header : bios) {
        EntryNode node;
        node.type = header.type;
        node.handle = header.handle;
        node.label = entry_type_label(node.type);
        // Per-entry exception isolation (threat model G-6): a throwing entry
        // still yields exactly one node. `fields` is only assigned on
        // success, so a partial result never leaks into the node. The
        // exception's message is firmware-influenced and is never kept.
        try {
            node.fields = decode_fields(header, bios);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            node.fields.clear();
            node.decode_failed = true;
        }
        nodes.push_back(std::move(node));
    }
    return nodes;
}

std::string entry_type_label(size_t type)
{
    if (type <= 0xFF) {
        switch (static_cast<SMBios::SMBiosHandler>(type)) {
        case SMBios::BIOSInformation: return "BIOS Information";
        case SMBios::SystemInformation: return "System Information";
        case SMBios::BaseboardInformation: return "Baseboard Information";
        case SMBios::SystemEnclosure: return "System Enclosure";
        case SMBios::ProcessorInformation: return "Processor Information";
        case SMBios::CacheInformation: return "Cache Information";
        case SMBios::PortConnection: return "Port Connection";
        case SMBios::SystemSlots: return "System Slots";
        case SMBios::OemStrings: return "OEM Strings";
        case SMBios::SystemConfigurationOptions: return "System Configuration Options";
        case SMBios::FirmwareLanguageInformation: return "Firmware Language Information";
        case SMBios::GroupAssociations: return "Group Associations";
        case SMBios::SystemEventLog: return "System Event Log";
        case SMBios::PhysicalMemoryArray: return "Physical Memory Array";
        case SMBios::MemoryDevice: return "Memory Device";
        case SMBios::MemoryErrorInformation32Bit: return "32-Bit Memory Error Information";
        case SMBios::MemoryArrayMappedAddress: return "Memory Array Mapped Address";
        case SMBios::MemoryDeviceMappedAddress: return "Memory Device Mapped Address";
        case SMBios::PointingDevice: return "Built-in Pointing Device";
        case SMBios::PortableBattery: return "Portable Battery";
        case SMBios::SystemReset: return "System Reset";
        case SMBios::HardwareSecurity: return "Hardware Security";
        case SMBios::SystemPowerControls: return "System Power Controls";
        case SMBios::VoltageProbe: return "Voltage Probe";
        case SMBios::CoolingDevice: return "Cooling Device";
        case SMBios::TemperatureProbe: return "Temperature Probe";
        case SMBios::ElectricalCurrentProbe: return "Electrical Current Probe";
        case SMBios::OutOfBandRemoteAccess: return "Out-of-Band Remote Access";
        case SMBios::BootIntegrityServices: return "Boot Integrity Services Entry Point";
        case SMBios::SystemBootInformation: return "System Boot Information";
        case SMBios::MemoryErrorInformation64Bit: return "64-Bit Memory Error Information";
        case SMBios::ManagementDevice: return "Management Device";
        case SMBios::ManagementDeviceComponent: return "Management Device Component";
        case SMBios::ManagementDeviceThresholdData: return "Management Device Threshold Data";
        case SMBios::MemoryChannel: return "Memory Channel";
        case SMBios::IpmiDeviceInformation: return "IPMI Device Information";
        case SMBios::SystemPowerSupply: return "System Power Supply";
        case SMBios::AdditionalInformation: return "Additional Information";
        case SMBios::OnboardDevicesExtendedInformation: return "Onboard Devices Extended Information";
        case SMBios::ManagementControllerHostInterface: return "Management Controller Host Interface";
        case SMBios::TpmDevice: return "TPM Device";
        case SMBios::ProcessorAdditionalInformation: return "Processor Additional Information";
        case SMBios::FirmwareInventoryInformation: return "Firmware Inventory Information";
        case SMBios::StringProperty: return "String Property";
        case SMBios::Inactive: return "Inactive";
        case SMBios::EndOfTable: return "End Of Table";
        }
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "Unknown (0x%02zX)", type);
    return buffer;
}

std::string display_text(const std::string& raw)
{
    // Bounded work (security review P-1, CWE-400): this runs for every
    // visible row every frame, so it must never touch more than about
    // kDisplayTextMaxBytes of `raw`, whatever raw.size() is.
    //
    // The input is consumed in *chunks* that sanitize_utf8() decodes
    // independently: a chunk is one byte plus the continuation bytes
    // (10xxxxxx) after it, at most 4 bytes in total. A chunk always ends
    // just before a byte where sanitize_utf8() is guaranteed to restart
    // decoding: a non-continuation byte can never continue a sequence, and
    // no sequence is longer than 4 bytes, so whatever follows a 4-byte
    // chunk is a fresh start (the rest of the lead's sequence or a stray
    // continuation byte). Hence sanitize_utf8(a + b) == sanitize_utf8(a) +
    // sanitize_utf8(b) at every chunk boundary, so the chunk-wise output
    // equals the whole-string output byte for byte.
    //
    // Every raw byte yields at least one output byte, so the loop stops after
    // at most kDisplayTextMaxBytes + 1 raw bytes (plus the rest of the last
    // chunk): the cost is O(kDisplayTextMaxBytes), not O(raw.size()).
    constexpr size_t kMaxChunkBytes = 4;
    constexpr size_t kSuffixReserve =
        (sizeof(kTruncatedPrefix) - 1) + kMaxCountDigits + (sizeof(kTruncatedSuffix) - 1);
    static_assert(kSuffixReserve < kDisplayTextMaxBytes);
    constexpr size_t kKeepBudget = kDisplayTextMaxBytes - kSuffixReserve;

    std::string safe;
    safe.reserve(raw.size() < kDisplayTextMaxBytes ? raw.size() : kDisplayTextMaxBytes + 3 * kMaxChunkBytes);
    size_t keep_output = 0; // output bytes at the last chunk boundary within kKeepBudget
    size_t keep_raw = 0;    // raw bytes consumed at that same boundary
    size_t pos = 0;
    while (pos < raw.size() && safe.size() <= kDisplayTextMaxBytes) {
        size_t end = pos + 1;
        while (end < raw.size() && end - pos < kMaxChunkBytes
               && is_continuation_byte(static_cast<unsigned char>(raw[end]))) {
            ++end;
        }
        const std::string valid = smbios::sanitize_utf8(raw.substr(pos, end - pos));
        for (size_t at = 0; at < valid.size();) {
            uint32_t code_point = 0;
            const size_t length = decode_valid_utf8(valid, at, code_point);
            if (is_display_unsafe(code_point)) {
                safe += kReplacementUtf8;
            } else {
                safe.append(valid, at, length);
            }
            at += length;
        }
        pos = end;
        if (safe.size() <= kKeepBudget) {
            keep_output = safe.size();
            keep_raw = pos;
        }
    }

    if (pos == raw.size() && safe.size() <= kDisplayTextMaxBytes) {
        return safe;
    }

    // Truncate at the last chunk boundary that leaves room for the widest
    // possible suffix, so the result (suffix included) never exceeds the cap
    // - which is what keeps display_text() idempotent. A chunk boundary is
    // always a code-point boundary of the output. N counts the *raw input*
    // bytes not shown, which is computable without reading them.
    const size_t dropped = raw.size() - keep_raw;
    safe.resize(keep_output);
    safe += kTruncatedPrefix;
    safe += std::to_string(dropped);
    safe += kTruncatedSuffix;
    return safe;
}

} // namespace smbios::gui
