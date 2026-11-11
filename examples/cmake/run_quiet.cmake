# Runs one example program for CTest without echoing what it prints.
#
# Usage (CTest COMMAND):
#   ${CMAKE_COMMAND} -DCMD=<program>[;<arg>...] [-DEXPECT=<exit-code>]
#                    -P run_quiet.cmake
#
# The examples read the live machine's SMBIOS table, so their stdout can
# hold this runner's serial numbers and UUIDs. stdout is therefore
# discarded (never written to the CTest log); stderr is kept, because it
# only ever carries diagnostics and the "no SMBIOS table" notice. Fails
# when the exit code differs from EXPECT (default 0).

if(NOT DEFINED CMD)
    message(FATAL_ERROR "run_quiet.cmake: CMD is not set")
endif()
if(NOT DEFINED EXPECT)
    set(EXPECT 0)
endif()

execute_process(
    COMMAND ${CMD}
    RESULT_VARIABLE result
    OUTPUT_QUIET)

if(NOT "${result}" STREQUAL "${EXPECT}")
    message(FATAL_ERROR "run_quiet.cmake: exit status was '${result}', expected '${EXPECT}'")
endif()
