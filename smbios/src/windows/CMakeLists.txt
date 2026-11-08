# smbios_source_windows - milestone 0005, Story 01.1 (ADR 0001). Only
# added by smbios/src/CMakeLists.txt's if(WIN32) branch.
set(TARGET smbios_source_windows)

file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/source_windows/*.h)

add_library(${TARGET} STATIC ${SOURCES})

# PUBLIC smbios_source_legacy: windows_native_source.cpp's
# make_default_source_chain() needs LegacyMemoryScanSource as the chain's
# second entry (milestone 0005, Story 04.0 - a separate top-level entry,
# not WindowsNativeSource's own internal fallback as it was before).
target_link_libraries(${TARGET} PUBLIC smbios_source_interface smbios_source_legacy)

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
