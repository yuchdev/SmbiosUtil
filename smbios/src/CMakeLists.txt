set(TARGET smbios)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS program_options REQUIRED)

# include/smbios_util/command_line_parser.h is this executable's own
# private header - it stays at that literal path/namespace (not part of
# the library renames), so its glob keeps the literal "smbios_util"
# segment rather than following ${TARGET}.
file(GLOB SOURCES *.cpp ../include/smbios_util/*.h)

include_directories(${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/libsmbios/include
)

add_executable(${TARGET} ${SOURCES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

target_link_libraries(${TARGET}
    ${Boost_LIBRARIES}
    libsmbios)

add_dependencies(${TARGET} libsmbios)
