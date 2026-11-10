project(smbios_integration_test CXX)
set(TARGET smbios_integration_test)

cmake_policy(SET CMP0015 NEW)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../libsmbios/bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework date_time REQUIRED)

file(GLOB SOURCES *.cpp)

include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../libsmbios/include)

add_executable(${TARGET} ${SOURCES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

target_link_libraries(${TARGET}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    libsmbios)


add_test(NAME smbios_integration_test COMMAND ${TARGET})

# CLI smoke tests for `smbios --format` (milestone 0009 Story 03.0 Task 02).
# Classified Integration: each runs the real built CLI as a process.
# - smbios_cli_unknown_format / smbios_cli_unknown_format_lists_names need no
#   hardware or privilege (the format is rejected before any SMBIOS read), so
#   CI's must-pass step runs them.
# - smbios_cli_format_csv reads the live machine's SMBIOS table, so like the
#   rest of this category it may fail cleanly without privilege; CI runs it
#   in the warning-only integration step.
# On failure first check: smbios/src/main.cpp's get_output_formatter()
# check, and that smbios/examples/ (the `csv` format) is linked into smbios.
add_test(NAME smbios_cli_unknown_format COMMAND $<TARGET_FILE:smbios> --format bogus)
set_tests_properties(smbios_cli_unknown_format PROPERTIES WILL_FAIL TRUE)

add_test(NAME smbios_cli_unknown_format_lists_names COMMAND $<TARGET_FILE:smbios> --format bogus)
set_tests_properties(smbios_cli_unknown_format_lists_names PROPERTIES
    PASS_REGULAR_EXPRESSION "Registered formats:.*csv.*json.*text.*xml.*yaml")

add_test(NAME smbios_cli_format_csv COMMAND $<TARGET_FILE:smbios> --format csv)
set_tests_properties(smbios_cli_format_csv PROPERTIES
    PASS_REGULAR_EXPRESSION "type,field_name,field_value")

SET_PROPERTY(TARGET smbios_integration_test PROPERTY FOLDER "Tests")
