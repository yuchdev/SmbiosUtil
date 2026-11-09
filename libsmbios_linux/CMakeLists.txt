# libsmbios_linux - milestone 0005, Story 01.1 (ADR 0001). Only added
# by the root CMakeLists.txt's elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
# branch, which also sets SMBIOS_LEGACY_LIB to libsmbios_posix_legacy.
set(TARGET libsmbios_linux)

file(GLOB SOURCES src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/smbios/linux/*.h)

add_library(${TARGET} STATIC ${SOURCES})
set_target_properties(${TARGET} PROPERTIES PREFIX "")

target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# PUBLIC libsmbios_posix_legacy: UnixNativeOrLegacyScanSource's internal
# fallback (linux_source_chain.cpp) needs LegacyMemoryScanSource.
target_link_libraries(${TARGET} PUBLIC sys_interface libsmbios_posix_legacy)

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
