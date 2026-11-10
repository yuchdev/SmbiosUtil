# Milestone 0009, Story 03.0, Task 03: standalone library-consumer examples.
#
# Each example is its own executable that links libsmbios exactly as an
# external consumer would (unlike smbios/examples/csv_formatter.cpp, which is
# compiled straight into the smbios CLI). Built by default; disable with
# -DSMBIOS_BUILD_EXAMPLES=OFF. See examples/README.md.

set(SMBIOS_EXAMPLE_TARGETS
    smbios_example_format_table
    smbios_example_read_fields)

add_executable(smbios_example_format_table format_table/main.cpp)
add_executable(smbios_example_read_fields read_fields/main.cpp)

foreach(example ${SMBIOS_EXAMPLE_TARGETS})
    target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR}/libsmbios/include)
    target_compile_options(${example} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
    target_link_options(${example} PRIVATE ${SMBIOS_LINK_OPTIONS})

    # Whole-archive linking of libsmbios - deliberately the same three-way
    # block as smbios/src/CMakeLists.txt, because this is the pitfall every
    # consumer of the static archive hits. The built-in formatters and every
    # entry class register themselves only from static initializers in their
    # own .cpp files, and nothing here names a concrete class, so a plain
    # link never extracts those objects: get_output_formatter("json") would
    # return nullptr and SMBiosEntryFactory::create() would return nullptr
    # for every structure type. (A shared smbios_link_whole_archive() helper
    # is a separate follow-up; CMake 3.24's $<LINK_LIBRARY:WHOLE_ARCHIVE,...>
    # is above this project's cmake_minimum_required.)
    if(APPLE)
        target_link_options(${example} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
    elseif(MSVC)
        target_link_options(${example} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
    else()
        target_link_options(${example} PRIVATE
            "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
    endif()

    target_link_libraries(${example} PRIVATE libsmbios)
    set_target_properties(${example} PROPERTIES FOLDER "Examples")
endforeach()

# ---------------------------------------------------------------------------
# CTest smoke tests (smbios_example_*). The examples read the live machine
# and degrade to an empty document when no table is readable, so they assert
# exit status and well-formedness only - never content - and never echo
# stdout (it may hold this runner's serials/UUIDs): run_quiet.cmake discards
# it, check_structured_output.py prints only a structure count.
# ---------------------------------------------------------------------------
set(SMBIOS_RUN_QUIET ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_quiet.cmake)
set(SMBIOS_EXAMPLE_TESTS "")

foreach(format text xml json yaml)
    add_test(NAME smbios_example_format_table_${format}
        COMMAND ${CMAKE_COMMAND}
            "-DCMD=$<TARGET_FILE:smbios_example_format_table>;${format}"
            -P ${SMBIOS_RUN_QUIET})
    list(APPEND SMBIOS_EXAMPLE_TESTS smbios_example_format_table_${format})
endforeach()

add_test(NAME smbios_example_format_table_unknown_format
    COMMAND ${CMAKE_COMMAND}
        "-DCMD=$<TARGET_FILE:smbios_example_format_table>;bogus"
        -DEXPECT=2
        -P ${SMBIOS_RUN_QUIET})
list(APPEND SMBIOS_EXAMPLE_TESTS smbios_example_format_table_unknown_format)

add_test(NAME smbios_example_read_fields
    COMMAND ${CMAKE_COMMAND}
        "-DCMD=$<TARGET_FILE:smbios_example_read_fields>"
        -P ${SMBIOS_RUN_QUIET})
list(APPEND SMBIOS_EXAMPLE_TESTS smbios_example_read_fields)

# Python is optional: it is never needed to build, only for these three
# structural checks.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(SMBIOS_CHECK_STRUCTURED ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_structured_output.py)

    add_test(NAME smbios_example_format_table_json_parses
        COMMAND ${Python3_EXECUTABLE} -I ${SMBIOS_CHECK_STRUCTURED} --format json --
            $<TARGET_FILE:smbios_example_format_table> json)
    add_test(NAME smbios_example_format_table_xml_parses
        COMMAND ${Python3_EXECUTABLE} -I ${SMBIOS_CHECK_STRUCTURED} --format xml --
            $<TARGET_FILE:smbios_example_format_table> xml)
    add_test(NAME smbios_example_cli_json_parses
        COMMAND ${Python3_EXECUTABLE} -I ${SMBIOS_CHECK_STRUCTURED} --format json --
            $<TARGET_FILE:smbios> --format json)
    list(APPEND SMBIOS_EXAMPLE_TESTS
        smbios_example_format_table_json_parses
        smbios_example_format_table_xml_parses
        smbios_example_cli_json_parses)
else()
    message(STATUS "Python3 interpreter not found: skipping the smbios_example_*_parses structural checks")
endif()

# Match CI's ctest --timeout 60.
set_tests_properties(${SMBIOS_EXAMPLE_TESTS} PROPERTIES TIMEOUT 60)
