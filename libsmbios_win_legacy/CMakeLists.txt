# libsmbios_win_legacy - milestone 0005, Story 01.1 (ADR 0001); split out of
# the former single smbios_legacy library by the build-layout restructure
# that gave each platform flavor of the legacy BIOS shadow-ROM
# physical-memory scan (PhysicalMemory, LegacyMemoryScanSource) its own
# top-level library. This is the Windows flavor: PhysicalMemory here IS the
# former NativePhysicalMemory (win_physical_memory.hpppp/.cpp) - the old PImpl
# wrapper has been merged directly into this one concrete class, since this
# library only ever builds on Windows (selected by the root CMakeLists.txt's
# if(WIN32) branch) and there is no other PhysicalMemory to forward to.
#
# legacy_memory_scan_source.hpppp/.cpp is intentionally duplicated verbatim in
# libsmbios_posix_legacy (not shared via a third library) - see that
# library's CMakeLists.txt comment and the story's own report for why.
#
# Exposes its public headers at smbios/legacy/physical_memory.hpppppppp and
# smbios/legacy/legacy_memory_scan_source.hpppppppp - the identical logical path
# libsmbios_posix_legacy also uses - so every consumer's
# #include <smbios/legacy/...> resolves to whichever flavor got linked,
# with no include-path change required at any call site.
set(TARGET libsmbios_win_legacy)

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS iostreams REQUIRED)

file(GLOB SOURCES src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/smbios/legacy/*.h)

add_library(${TARGET} STATIC ${SOURCES})
set_target_properties(${TARGET} PROPERTIES PREFIX "")

target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET} PUBLIC sys_interface ${Boost_LIBRARIES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
