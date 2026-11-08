# os_interface (was smbios_source_interface) - milestone 0005, Story 01.1
# (ADR 0001); relocated to the repo root as part of the build-layout
# restructure that renamed smbios -> libsmbios and gave every per-OS/legacy
# library its own top-level directory.
#
# This is an INTERFACE library, not STATIC: every file it owns
# (ismbios_source.h, smbios_source_chain.h, byte_reader.h, smbios_anchor.h,
# smbios_entry_point.h, smbios_version_fixup.h) is header-only - there is no
# .cpp to compile, so there is no artifact for a STATIC library to produce.
# This is NOT a deviation from CLAUDE.md's "static libraries, not
# shared/dynamic" convention: that convention exists to avoid .so/.dll
# *runtime* linkage, which is orthogonal to CMake's INTERFACE target kind.
# INTERFACE here means "propagate an include directory to whoever links
# this; build nothing of its own" - the correct CMake target kind for a
# header-only library, not an oversight.
add_library(os_interface INTERFACE)

target_include_directories(os_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
