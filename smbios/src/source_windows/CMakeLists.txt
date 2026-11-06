# smbios_source_windows - milestone 0005, Story 01.1 (ADR 0001). Only
# added by smbios/src/CMakeLists.txt's if(WIN32) branch.
set(TARGET smbios_source_windows)

file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/source_windows/*.h)

add_library(${TARGET} STATIC ${SOURCES})

# PUBLIC smbios_source_legacy: WindowsNativeSource's internal fallback
# (windows_native_source.cpp) needs LegacyMemoryScanSource.
target_link_libraries(${TARGET} PUBLIC smbios_source_interface smbios_source_legacy)

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
