# 01 - macOS IORegistry reader

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Port `c-versions/dmidecode_macos/util.c`'s `mem_chunk_ioreg()` into a
focused C++ reader: look up IOKit's `AppleSMBIOS` registry entry and
return the raw bytes of a named property. This is a direct port against a
confirmed, working reference - not an exploratory spike (an earlier draft
of this task, written before `c-versions/dmidecode_macos` had actually
been inspected, treated the mechanism as unverified; it has since been
confirmed by reading that reference directly).

## Files

- **Create** `smbios/include/smbios/macos_efi_reader.h` / `.cpp`:
  ```cpp
  std::optional<std::vector<uint8_t>>
  read_apple_smbios_property(const char* property_name, size_t max_len);
  ```
  implemented per the confirmed reference:
  ```cpp
  // Ported from c-versions/dmidecode_macos/util.c::mem_chunk_ioreg()
  io_registry_entry_t entry = IORegistryEntryFromPath(
      kIOMasterPortDefault,
      "IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS");
  if (entry == MACH_PORT_NULL) return std::nullopt;

  CFStringRef field = CFStringCreateWithCString(kCFAllocatorDefault, property_name, kCFStringEncodingASCII);
  CFDataRef data = IORegistryEntryCreateCFProperty(entry, field, kCFAllocatorDefault, 0);
  IOObjectRelease(entry);
  CFRelease(field);

  if (data == nullptr || CFGetTypeID(data) != CFDataGetTypeID()) {
      if (data) CFRelease(data);
      return std::nullopt;
  }
  std::vector<uint8_t> blob(CFDataGetBytePtr(data), CFDataGetBytePtr(data) + CFDataGetLength(data));
  CFRelease(data);
  return blob;
  ```
  The reference calls this with `"SMBIOS-EPS"` for the entry point and
  `"SMBIOS"` for the table (selected by whether the conceptual legacy-scan
  `base` being requested is `0xF0000`, a detail specific to how the
  reference reuses its x86 legacy-scan call site - this C++ port instead
  takes `property_name` as an explicit, named parameter, since
  `MacOsUefiSource` (Task 02) calls it twice with the real property names
  directly, with no legacy-scan-address indirection to reverse-engineer).

## Functions and Classes

- `smbios::read_apple_smbios_property(const char*, size_t)` - new.

## Constraints

- This function is architecture-neutral - it must not be compiled behind
  any `#if defined(__i386__) || defined(__x86_64__)` guard (see this
  story's Key constraints on the deliberate deviation from the reference).
- `max_len` truncates the returned blob the same way the reference's
  `len`/`org_len` comparison does (`if (org_len < len) len = org_len;`) -
  i.e., the caller's requested cap is a ceiling, and the function returns
  whatever the property actually holds, up to that ceiling, not padded.

## Tests

- `tests/unit/macos_efi_reader_test.cpp` (macOS-only): confirms the
  function compiles and links against IOKit/CoreFoundation; a real-Mac
  manual run confirms `read_apple_smbios_property("SMBIOS-EPS", 0x20)`
  returns a non-empty blob starting with `_SM_` or `_SM3_`.

## Success criteria

- [ ] `read_apple_smbios_property("SMBIOS-EPS", 0x20)` returns a blob
      starting with a valid SMBIOS anchor on a real Mac.
- [ ] The function is reachable and callable regardless of whether the
      binary is built for `arm64` or `x86_64`.
