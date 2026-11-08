#include <smbios/source_macos/macos_efi_reader.h>

#include <algorithm>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

// Ported from c-versions/dmidecode_macos/util.c::mem_chunk_ioreg()
// (~line 163-215). See macos_efi_reader.h's doc comment on `max_len` for
// the one deliberate behavioral deviation from that reference (no
// zero-padding to the originally-requested size).
std::optional<std::vector<uint8_t>> smbios::read_apple_smbios_property(const char* property_name, size_t max_len)
{
// kIOMasterPortDefault is deprecated (in favor of kIOMainPortDefault) as of
// macOS 12 but is what c-versions/dmidecode_macos/util.c::mem_chunk_ioreg()
// actually uses, and it remains functional - kept verbatim per this task's
// spec (a direct port, not a modernization) rather than swapped for the
// newer name, with the deprecation warning silenced locally so this
// project's -Werror build setting does not reject the intentional choice.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    io_registry_entry_t entry = IORegistryEntryFromPath(
        kIOMasterPortDefault, "IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS");
#pragma clang diagnostic pop
    if (entry == MACH_PORT_NULL) {
        return std::nullopt;
    }

    CFStringRef field = CFStringCreateWithCString(kCFAllocatorDefault, property_name, kCFStringEncodingASCII);
    CFDataRef data = static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(entry, field, kCFAllocatorDefault, 0));
    IOObjectRelease(entry);
    CFRelease(field);

    if (data == nullptr || CFGetTypeID(data) != CFDataGetTypeID()) {
        if (data != nullptr) {
            CFRelease(data);
        }
        return std::nullopt;
    }

    // Deliberate deviation from mem_chunk_ioreg(): truncate to max_len,
    // never pad past the property's real length. See the header's doc
    // comment on `max_len` for the full rationale.
    const size_t actual_len = static_cast<size_t>(CFDataGetLength(data));
    const size_t copy_len = std::min(actual_len, max_len);

    const uint8_t* bytes = CFDataGetBytePtr(data);
    std::vector<uint8_t> blob(bytes, bytes + copy_len);

    CFRelease(data);
    return blob;
}
