#pragma once
#include <memory>
#include <vector>

#include <smbios/os_interface/ismbios_source.h>

namespace smbios {

/// @brief Build this platform's ordered chain of `ISmbiosSource`
/// implementations, highest-priority first (contract C2). `SMBios`'s
/// constructor tries each in order - `is_available()` then `try_read()` -
/// and stops at the first one that succeeds.
///
/// Declared once, here, in the platform-neutral `smbios_source_interface`
/// library; *defined* exactly once per build, in whichever library CMake
/// actually links for this platform (milestone 0005, Story 01.1, ADR 0001):
/// `smbios_windows`, `smbios_macos`, or `smbios_linux`
/// (selected by `smbios/src/CMakeLists.txt`'s `if(WIN32)/elseif(APPLE)/
/// elseif(... Linux)` branch), or - for BSD/Solaris, which have no per-OS
/// library - a trivial fallback definition compiled directly into `smbios`
/// core's own `else()` branch. This is a *link-seam*, not a preprocessor
/// seam: no `#if defined(_WIN32)` (or any platform macro) exists anywhere
/// in this call chain's C++ source any more - the platform branch is a
/// CMake `if()` choosing which library to link, nothing else. If this
/// symbol appears unresolved while reading only this header, that is
/// expected: its definition lives in whichever per-OS library is actually
/// linked, never here.
///
/// Milestone 0005, Story 01.0: every platform's chain has exactly one
/// element today (the existing native source, falling back internally to
/// the legacy physical-memory scan exactly as `SMBios::SMBios()` used to
/// do inline) - matching current behavior byte-for-byte. Stories
/// 02.0-04.0 add real additional entries (Linux sysfs/EFI, macOS UEFI)
/// ahead of that one element in the returned vector.
std::vector<std::unique_ptr<ISmbiosSource>> make_default_source_chain();

} // namespace smbios
