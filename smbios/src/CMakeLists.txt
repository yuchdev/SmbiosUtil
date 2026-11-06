set(TARGET smbios)

# file(GLOB) is non-recursive, so this only ever picks up *.cpp sitting
# directly in smbios/src/ itself (the SMBios class, the entry-type classes,
# the factory) - never the per-OS/shared-library subdirectories added via
# add_subdirectory() below, each of which globs its own directory in its
# own CMakeLists.txt.
file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../include/${TARGET}/*.h)

# fallback_source_chain.cpp supplies make_default_source_chain() only for
# platforms with no per-OS library at all (the else() branch below, BSD/
# Solaris/other *nix) - remove it from the default glob here and re-add it
# explicitly only in that one branch, so it never coexists in the same
# build as smbios_source_windows/_macos/_linux's own definition of the
# same symbol (which would otherwise be a duplicate-definition link error
# on every platform that *does* have a per-OS library).
list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fallback_source_chain.cpp)

include_directories(${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Platform-neutral shared libraries (milestone 0005, Story 01.1, ADR 0001):
# smbios_source_interface (header-only decode primitives + the
# ISmbiosSource/make_default_source_chain() contract) and
# smbios_source_legacy (the legacy BIOS physical-memory scan, reused by
# every platform's chain either directly or as a per-OS library's own
# internal fallback) are built unconditionally.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_interface)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_legacy)

# Exactly one per-OS library is selected at CMake configure time - this
# replaces the role smbios.cpp's old C++ `#if defined(_WIN32)` used to
# play (R8); no two of smbios_source_windows/_macos/_linux are ever linked
# into the same build.
if(WIN32)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_windows)
    set(SMBIOS_PLATFORM_SOURCE_LIB smbios_source_windows)
elseif(APPLE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_macos)
    set(SMBIOS_PLATFORM_SOURCE_LIB smbios_source_macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_linux)
    set(SMBIOS_PLATFORM_SOURCE_LIB smbios_source_linux)
else()
    # BSD/Solaris/other *nix: no per-OS library exists for these targets
    # (ADR 0001) - fallback_source_chain.cpp (removed from SOURCES above,
    # re-added only here) supplies make_default_source_chain() directly
    # inside smbios core, reproducing today's real behavior on these
    # platforms exactly (legacy scan only).
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fallback_source_chain.cpp)
    set(SMBIOS_PLATFORM_SOURCE_LIB "")
endif()

add_library(${TARGET} ${SOURCES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

target_link_libraries(${TARGET} PUBLIC smbios_source_interface smbios_source_legacy ${SMBIOS_PLATFORM_SOURCE_LIB})

