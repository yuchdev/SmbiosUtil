# smbios_source_macos - milestone 0005, Story 01.1 (ADR 0001). Only added
# by smbios/src/CMakeLists.txt's elseif(APPLE) branch. Story 03.0, Task 02
# adds MacOsUefiSource, macOS's first native ISmbiosSource; macos_source_chain.cpp's
# make_default_source_chain() is now {MacOsUefiSource, LegacyMemoryScanSource}.
set(TARGET smbios_source_macos)

file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/source_macos/*.h)

add_library(${TARGET} STATIC ${SOURCES})

target_link_libraries(${TARGET} PUBLIC smbios_source_interface smbios_source_legacy)

# Milestone 0005, Story 03.0, Task 01: macos_efi_reader.cpp reads SMBIOS
# properties off the IORegistry via IOKit/CoreFoundation - system
# frameworks, not vcpkg packages. This library only builds under the root
# CMakeLists.txt's elseif(APPLE) branch, so no extra __APPLE__ guard is
# needed here.
find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
target_link_libraries(${TARGET} PUBLIC ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
