# smbios_legacy - milestone 0005, Story 01.1 (ADR 0001).
#
# The legacy BIOS shadow-ROM physical-memory scan (PhysicalMemory/
# NativePhysicalMemory, LegacyMemoryScanSource) - the one piece of this
# subsystem with zero platform-specific code of its own; its only OS
# dependency is already hidden behind an existing PImpl + #if-split .cpp
# pair (posix_physical_memory.cpp/win_physical_memory.cpp), moved here
# verbatim. Depends only on smbios_source_interface - never on any per-OS
# library (smbios_windows/smbios_macos/smbios_linux); verified by grep in this
# story's own report.
set(TARGET smbios_legacy)

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS iostreams REQUIRED)

file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/legacy/*.h)

add_library(${TARGET} STATIC ${SOURCES})

target_link_libraries(${TARGET} PUBLIC smbios_source_interface ${Boost_LIBRARIES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
