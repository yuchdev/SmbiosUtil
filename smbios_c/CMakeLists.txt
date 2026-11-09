set(TARGET smbios_c)

include(GenerateExportHeader)

add_library(${TARGET} SHARED src/smbios_c.cpp)

target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/libsmbios/include
    ${CMAKE_SOURCE_DIR}/os_interface/include
)

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

# Built SHARED (not a plain static archive, unlike every other library in
# this project) specifically so the whole-archive step below can bake
# libsmbios's entry classes into this one self-contained binary once and
# for all - the "a consumer links smbios_c only, never libsmbios" contract
# (Story 03.0's own Key constraints) would not otherwise hold: a STATIC
# smbios_c only records a usage requirement on libsmbios, it does not
# merge its objects, so the whole-archive problem below would simply
# reappear at every downstream consumer's own link step instead of being
# solved once, here.
#
# Force every object file in libsmbios's static archive into this shared
# library - the exact same problem and the exact same fix as the smbios
# CLI's own smbios/src/CMakeLists.txt (see its comment for the full
# rationale): each concrete entry class (BiosInformationEntry,
# SystemSlotsEntry, ...) registers itself with SMBiosEntryFactory only via
# REGISTER_SMBIOS_ENTRY's static initializer in its own .cpp, and nothing
# in smbios_c.cpp names most concrete entry classes directly (only the
# handful Story 02.0 Task 02 dynamic_casts to), so without this, those
# objects are never pulled out of libsmbios.a, their initializers never
# run, and SMBiosEntryFactory::create() silently returns nullptr for
# every type smbios_c.cpp does not name - exactly the gap discovered
# while manually verifying Story 02.0 before this CMake target existed
# (see status.md's 2026-11-09 note under Story 02.0).
if(APPLE)
    target_link_options(${TARGET} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
elseif(MSVC)
    target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
else()
    target_link_options(${TARGET} PRIVATE
        "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
endif()

# C_VISIBILITY_PRESET/CXX_VISIBILITY_PRESET hidden (below) only controls
# the visibility attribute baked into THIS target's own object file
# (src/smbios_c.cpp) at compile time - it cannot retroactively hide
# symbols already baked into libsmbios.a's object files by their own,
# unrelated compilation (libsmbios's CMakeLists.txt sets no visibility
# option, so its symbols default to exported). The whole-archive step
# above pulls every one of those objects into this shared library
# wholesale, so without an explicit allowlist at the link step, every
# AbstractSMBiosEntry/libc++ symbol libsmbios happens to use would leak
# out as a public export of libsmbios_c - exactly what Story 03.0 Task
# 01's own success criterion checks for. An explicit linker export
# allowlist is the standard, platform-correct way to constrain this
# regardless of any object's own compile-time visibility: macOS's
# -exported_symbols_list and a GNU-ld-compatible --version-script for
# Linux (and any other non-Apple, non-MSVC platform - this project never
# supports GNU ld's ELF variant on Apple targets). MSVC needs neither:
# nothing is exported from a Windows DLL by default unless explicitly
# marked, which generate_export_header() already does via
# __declspec(dllexport) on SMBIOS_C_EXPORT.
if(APPLE)
    target_link_options(${TARGET} PRIVATE
        "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/smbios_c.exported_symbols")
elseif(NOT MSVC)
    target_link_options(${TARGET} PRIVATE
        "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/smbios_c.version")
endif()

# PRIVATE: a consumer of smbios_c must never need to also link or include
# libsmbios's C++ headers (Story 03.0's own Key constraints) - the C ABI
# declared in smbios_c.h is the complete interface. libsmbios's own
# dependencies (its per-OS/legacy libraries) are themselves PUBLIC from
# libsmbios, so linking libsmbios here already pulls in everything the
# whole-archive step above needs resolved.
target_link_libraries(${TARGET} PRIVATE libsmbios)

add_dependencies(${TARGET} libsmbios)

# Only the intended extern "C" smbios_* functions are exported from the
# shared library - not every C++ symbol smbios_c.cpp happens to pull in
# from libsmbios - via hidden-by-default visibility plus a generated
# export header. Every extern "C" function in smbios_c.h already carries
# no class/template machinery of its own, so SMBIOS_C_EXPORT expanding to
# nothing on a platform/build where hidden visibility is not the default
# (MSVC's own __declspec(dllexport)/dllimport pair, handled automatically
# by generate_export_header()) is still correct.
set_target_properties(${TARGET} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

generate_export_header(${TARGET}
    BASE_NAME smbios_c
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/smbios_c/smbios_c_export.h
)
