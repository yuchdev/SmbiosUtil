# Local-only GUI tier (milestone 0011, Story 04.0 Task 04): proof that the
# test-only auto-exit hook (smbios_gui/src/main.cpp, under
# SMBIOS_GUI_TEST_HOOKS) is inert in the shipped smbios_gui and present in
# the smbios_gui_autoexit test twin (threat model
# docs/security/2026-11-11-gui-test-tiers.md, T-4/R-15).
#
# Run by CTest only (tests/gui/CMakeLists.txt, local tier), as
#   cmake -DMODE=<InertBinaries|InertCompileLines> -DSHIPPED_BINARY=...
#         -DTWIN_BINARY=... -DGREP_EXECUTABLE=... -DBUILD_DIR=...
#         -DGENERATOR=... -DNINJA_EXECUTABLE=... -P hook_inertness_check.cmake
# Every program is run through execute_process() with an argument list (no
# shell). Fails closed: a missing input, file or program, an unreadable
# grep result or an empty command listing is a failure, never a pass. Prints
# counts and target names only.

cmake_minimum_required(VERSION 3.13)

# fail(<message>): one line, non-zero exit.
macro(fail message_text)
    message(FATAL_ERROR "hook inertness: ${message_text}")
endmacro()

foreach(required IN ITEMS MODE SHIPPED_BINARY TWIN_BINARY GREP_EXECUTABLE BUILD_DIR GENERATOR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "" OR "${${required}}" MATCHES "-NOTFOUND$")
        fail("required input ${required} is missing")
    endif()
endforeach()
if(NOT IS_ABSOLUTE "${GREP_EXECUTABLE}" OR NOT EXISTS "${GREP_EXECUTABLE}")
    fail("grep not found (needed for the 'grep -a -c -F -e' count)")
endif()

# grep_count(<out_var> <pattern> <file>): the number of
# matching lines from `grep -a -c -F -e <pattern> <file>`. grep exits 0
# (matches) or 1 (none); anything else, or a non-number, fails closed.
function(grep_count out_var pattern file)
    execute_process(
        COMMAND "${GREP_EXECUTABLE}" -a -c -F -e "${pattern}" "${file}"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE count
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT (rc STREQUAL "0" OR rc STREQUAL "1") OR NOT count MATCHES "^[0-9]+$")
        fail("grep could not read a file (exit ${rc})")
    endif()
    set(${out_var} "${count}" PARENT_SCOPE)
endfunction()

if(MODE STREQUAL "InertBinaries")
    foreach(binary IN ITEMS "${SHIPPED_BINARY}" "${TWIN_BINARY}")
        if(NOT EXISTS "${binary}" OR IS_DIRECTORY "${binary}")
            get_filename_component(binary_name "${binary}" NAME)
            fail("binary ${binary_name} does not exist (build it first)")
        endif()
    endforeach()
    foreach(pattern IN ITEMS "--exit-after-frames" "usage: --exit-after-frames")
        grep_count(shipped_count "${pattern}" "${SHIPPED_BINARY}")
        grep_count(twin_count "${pattern}" "${TWIN_BINARY}")
        if(NOT shipped_count EQUAL 0)
            fail("'${pattern}' in shipped smbios_gui: expected 0 -> ${shipped_count}")
        endif()
        if(twin_count LESS 1)
            fail("'${pattern}' in smbios_gui_autoexit: expected >= 1 -> ${twin_count}")
        endif()
        message(STATUS "'${pattern}': smbios_gui 0, smbios_gui_autoexit ${twin_count}")
    endforeach()

elseif(MODE STREQUAL "InertCompileLines")
    if(NOT GENERATOR MATCHES "Ninja")
        # Not a failure: only Ninja offers `-t commands`. One SKIP: line.
        message("SKIP: smbios_gui_autoexit.InertCompileLines: needs the Ninja generator")
        return()
    endif()
    if(NOT DEFINED NINJA_EXECUTABLE OR NOT EXISTS "${NINJA_EXECUTABLE}")
        fail("ninja not found")
    endif()
    # ninja_grep_count(<out_var> <target> <pattern>): matching lines of
    # `ninja -t commands <target> | grep -a -c -F -e <pattern>`, piped by
    # execute_process() itself (no shell, no scratch file). Fails closed if
    # ninja fails or grep's result is unreadable.
    function(ninja_grep_count out_var target pattern)
        execute_process(
            COMMAND "${NINJA_EXECUTABLE}" -C "${BUILD_DIR}" -t commands ${target}
            COMMAND "${GREP_EXECUTABLE}" -a -c -F -e "${pattern}"
            RESULTS_VARIABLE rcs
            OUTPUT_VARIABLE count
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        list(GET rcs 0 ninja_rc)
        list(GET rcs 1 grep_rc)
        if(NOT ninja_rc STREQUAL "0")
            fail("ninja -t commands ${target} failed (exit ${ninja_rc})")
        endif()
        if(NOT (grep_rc STREQUAL "0" OR grep_rc STREQUAL "1") OR NOT count MATCHES "^[0-9]+$")
            fail("grep could not read ninja's command listing (exit ${grep_rc})")
        endif()
        set(${out_var} "${count}" PARENT_SCOPE)
    endfunction()

    foreach(target IN ITEMS smbios_gui smbios_gui_autoexit)
        # Fail closed on an empty or unrelated listing: it must contain the
        # target's own main.cpp compile line.
        ninja_grep_count(main_lines ${target} "CMakeFiles/${target}.dir/src/main.cpp")
        ninja_grep_count(hook_lines ${target} "SMBIOS_GUI_TEST_HOOKS")
        if(main_lines LESS 1)
            fail("ninja -t commands ${target} lists no main.cpp compile line")
        endif()
        if(target STREQUAL "smbios_gui" AND NOT hook_lines EQUAL 0)
            fail("SMBIOS_GUI_TEST_HOOKS on smbios_gui's compile lines: expected 0 -> ${hook_lines}")
        endif()
        if(target STREQUAL "smbios_gui_autoexit" AND hook_lines LESS 1)
            fail("SMBIOS_GUI_TEST_HOOKS on smbios_gui_autoexit's compile lines: expected >= 1 -> ${hook_lines}")
        endif()
        message(STATUS "ninja -t commands ${target}: SMBIOS_GUI_TEST_HOOKS lines ${hook_lines}")
    endforeach()
    # compile_commands.json is optional (CMAKE_EXPORT_COMPILE_COMMANDS);
    # when present, smbios_gui's own entries must not carry the define.
    set(compile_commands "${BUILD_DIR}/compile_commands.json")
    if(EXISTS "${compile_commands}")
        file(STRINGS "${compile_commands}" shipped_entries REGEX "CMakeFiles/smbios_gui\\.dir/")
        set(hook_entries 0)
        foreach(entry IN LISTS shipped_entries)
            string(FIND "${entry}" "SMBIOS_GUI_TEST_HOOKS" position)
            if(NOT position EQUAL -1)
                math(EXPR hook_entries "${hook_entries} + 1")
            endif()
        endforeach()
        if(NOT hook_entries EQUAL 0)
            fail("SMBIOS_GUI_TEST_HOOKS in smbios_gui's compile_commands.json lines: expected 0 -> ${hook_entries}")
        endif()
        message(STATUS "compile_commands.json smbios_gui lines with SMBIOS_GUI_TEST_HOOKS: 0")
    endif()

else()
    fail("unknown MODE")
endif()
