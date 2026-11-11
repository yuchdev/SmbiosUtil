# Milestone 0009 Story 03.0 Task 02 (R5): the `csv` output format, an
# example of a formatter added from *outside* libsmbios.
#
# csv_formatter.cpp is compiled as an OBJECT library and its object file is
# added straight to the `smbios` executable's sources - it is never placed in
# a static archive, so its REGISTER_OUTPUT_FORMATTER static initializer
# always runs without any whole-archive linker flag. It only needs
# libsmbios's public headers; the symbols it uses resolve against the
# libsmbios archive the executable already links (smbios/src/CMakeLists.txt).
#
# tests/unit/CMakeLists.txt compiles the same source into smbios_unit_test
# for csv_formatter_test.cpp.
set(SMBIOS_CSV_FORMATTER_TARGET smbios_csv_formatter)

add_library(${SMBIOS_CSV_FORMATTER_TARGET} OBJECT csv_formatter.cpp)

target_include_directories(${SMBIOS_CSV_FORMATTER_TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/libsmbios/include)

target_compile_options(${SMBIOS_CSV_FORMATTER_TARGET} PRIVATE
    ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})

target_sources(smbios PRIVATE $<TARGET_OBJECTS:${SMBIOS_CSV_FORMATTER_TARGET}>)
