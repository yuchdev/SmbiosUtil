set(TARGET libsmbios)

# file(GLOB) is non-recursive, so this only ever picks up *.cpp sitting
# directly in libsmbios/src/ itself (the SMBios class, the entry-type
# classes, the factory) - never any of the sibling per-OS/legacy/
# sys_interface top-level libraries, each of which globs its own directory
# in its own CMakeLists.txt.
file(GLOB SOURCES *.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../include/libsmbios/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/../include/libsmbios/tables/*.h)

# fallback_source_chain.cpp supplies make_default_source_chain() only for
# platforms with no per-OS library at all (SMBIOS_PLATFORM_LIB empty - BSD/
# Solaris/other *nix, set by the root CMakeLists.txt's else() branch) -
# remove it from the default glob here and re-add it explicitly only in
# that case, so it never coexists in the same build as libsmbios_win/
# libsmbios_macos/libsmbios_linux's own definition of the same symbol
# (which would otherwise be a duplicate-definition link error on every
# platform that *does* have a per-OS library).
list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fallback_source_chain.cpp)

include_directories(${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)

if(NOT SMBIOS_PLATFORM_LIB)
    # BSD/Solaris/other *nix: no per-OS library exists for these targets
    # (ADR 0001) - fallback_source_chain.cpp (removed from SOURCES above,
    # re-added only here) supplies make_default_source_chain() directly
    # inside libsmbios core, reproducing today's real behavior on these
    # platforms exactly (legacy scan only).
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fallback_source_chain.cpp)
endif()

add_library(${TARGET} ${SOURCES})
set_target_properties(${TARGET} PROPERTIES PREFIX "")

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

target_link_libraries(${TARGET} PUBLIC sys_interface ${SMBIOS_LEGACY_LIB} ${SMBIOS_PLATFORM_LIB})
