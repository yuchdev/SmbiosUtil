#pragma once
#include <memory>
#include <vector>

#include <smbios/ismbios_source.h>

namespace smbios {

/// @brief Build this platform's ordered chain of `ISmbiosSource`
/// implementations, highest-priority first (contract C2). `SMBios`'s
/// constructor tries each in order - `is_available()` then `try_read()` -
/// and stops at the first one that succeeds; it never branches on
/// `#if defined(_WIN32)` itself, only this factory does (R8).
///
/// Milestone 0005, Story 01.0: every platform's chain has exactly one
/// element today (the existing native source, falling back internally to
/// the legacy physical-memory scan exactly as `SMBios::SMBios()` used to
/// do inline) - matching current behavior byte-for-byte. Stories
/// 02.0-04.0 add real additional entries (Linux sysfs/EFI, macOS UEFI)
/// ahead of that one element in the returned vector.
std::vector<std::unique_ptr<ISmbiosSource>> make_default_source_chain();

} // namespace smbios
