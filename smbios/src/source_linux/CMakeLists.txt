# smbios_source_linux - milestone 0005, Story 01.1 (ADR 0001). Only added
# by smbios/src/CMakeLists.txt's elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
# branch.
set(TARGET smbios_source_linux)

file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/source_linux/*.h)

add_library(${TARGET} STATIC ${SOURCES})

# PUBLIC smbios_source_legacy: UnixNativeOrLegacyScanSource's internal
# fallback (linux_source_chain.cpp) needs LegacyMemoryScanSource.
target_link_libraries(${TARGET} PUBLIC smbios_source_interface smbios_source_legacy)

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
