#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace smbios {

/// @brief Read a named CFData property off the `AppleSMBIOS` IORegistry
/// entry (`IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS`), the
/// mechanism this project's own `c-versions/dmidecode_macos/util.c`
/// confirms as `mem_chunk_ioreg()` (~line 163-215). Used by
/// `MacOsUefiSource` (milestone 0005, Story 03.0, Task 02) with
/// `property_name` `"SMBIOS-EPS"` for the entry point and `"SMBIOS"` for
/// the raw structure table - both passed explicitly, unlike the
/// reference's call site, which infers the property name from whether
/// the conceptual legacy-scan `base` it was asked for is `0xF0000`.
///
/// Looks up the registry entry, reads `property_name` as a `CFData`
/// property, and returns its bytes truncated to at most `max_len`.
/// Returns `std::nullopt` if the registry entry does not exist, the
/// property is absent, or the property exists but is not `CFData`.
///
/// Requires no root/admin privilege (R7) - this is the first SMBIOS
/// source in this project usable by a non-root macOS user.
///
/// Deliberately architecture-neutral: unlike the reference, which only
/// reaches this code path through a call site gated
/// `#if defined(__i386__) || defined(__x86_64__)`, this function carries
/// no such guard - IOKit property reads do not depend on CPU
/// architecture, and this story's entire point is native Apple Silicon
/// support.
///
/// @param property_name The CFString name of the property to read (e.g.
///   `"SMBIOS-EPS"` or `"SMBIOS"`), encoded as ASCII.
/// @param max_len A true ceiling on the returned blob's size - never a
///   padding target. This is a deliberate deviation from the reference:
///   `mem_chunk_ioreg()` does `blob = calloc(1, len)` using the
///   *originally requested* `len`, then clamps `len` down to the
///   property's actual size if shorter (`org_len = CFDataGetLength(data);
///   if (org_len < len) len = org_len;`), then `memcpy`s only that many
///   bytes - so the C function always returns a buffer of exactly the
///   originally-requested size, zero-padded at the end whenever the real
///   property is shorter. That padding is a C idiom for writing into a
///   fixed-size `calloc`'d destination, not a meaningful "pad to request
///   size" contract worth preserving in C++: a `std::vector` has no
///   reason to carry synthetic trailing zero bytes that are not real
///   property data. Carrying it forward would also risk corrupting the
///   real SMBIOS table (Task 02's second call, reading the `"SMBIOS"`
///   property) with fabricated zero bytes, if the entry point's declared
///   table length ever exceeded the real property's actual length -
///   unlikely in practice (IORegistry publishes exact data) but not worth
///   risking. So this function returns exactly
///   `min(CFDataGetLength(data), max_len)` bytes: truncated to the cap,
///   never padded past the real property's actual length.
/// @return The property's bytes (truncated to `max_len`), or
///   `std::nullopt` on any failure.
std::optional<std::vector<uint8_t>> read_apple_smbios_property(const char* property_name, size_t max_len);

} // namespace smbios
