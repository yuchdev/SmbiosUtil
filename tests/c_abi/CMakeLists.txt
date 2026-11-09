project(smbios_c_tests CXX C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../smbios_c/bin")

# compile_as_c_test.c and smbios_c_example.c are built as real C
# executables (not C++) - that is the entire point of Story 01.0 Task
# 01's/Story 02.0 Task 03's success criteria: proving smbios.h parses
# under a real C compiler, not a C++ one with extern "C" ignored. Both
# link only against smbios_c - no libsmbios include path or library
# needed, since the whole point of the C ABI is that a C caller never
# touches libsmbios directly.
add_executable(smbios_c_compile_as_c_test compile_as_c_test.c)
set_target_properties(smbios_c_compile_as_c_test PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_link_libraries(smbios_c_compile_as_c_test PRIVATE smbios_c)
add_test(NAME smbios_c_compile_as_c_test COMMAND smbios_c_compile_as_c_test)

add_executable(smbios_c_example smbios_c_example.c)
set_target_properties(smbios_c_example PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_link_libraries(smbios_c_example PRIVATE smbios_c)
add_test(NAME smbios_c_example COMMAND smbios_c_example)

# smbios_open_close_test.cpp only calls the C API (no direct libsmbios
# use), so it only needs smbios_c.
add_executable(smbios_c_open_close_test smbios_open_close_test.cpp)
target_include_directories(smbios_c_open_close_test PRIVATE
    ${CMAKE_SOURCE_DIR}/libsmbios/include
    ${CMAKE_SOURCE_DIR}/sys_interface/include
)
target_link_libraries(smbios_c_open_close_test PRIVATE smbios_c)
add_test(NAME smbios_c_open_close_test COMMAND smbios_c_open_close_test)

# smbios_entry_iteration_test.cpp and smbios_typed_getters_test.cpp each
# build an independent C++ "reference" smbios::SMBios + direct
# SMBiosEntryFactory::create() walk to cross-check the C API's output
# against - so unlike the two targets above, these link libsmbios
# directly too, in addition to smbios_c. They need the same whole-archive
# treatment as smbios_c's own CMakeLists.txt and smbios/src/CMakeLists.txt
# (see either one's comment for the full rationale): without it, their
# own reference walk would silently return "Unknown structure type N" for
# every entry type neither this file nor smbios_c.cpp names directly
# (e.g. CacheInformationEntry) - exactly the bug discovered and fixed
# while manually verifying Story 02.0 before this CMakeLists.txt existed
# (see status.md's 2026-11-09 note under Story 02.0).
foreach(test_name smbios_c_entry_iteration_test smbios_c_typed_getters_test)
    if(test_name STREQUAL "smbios_c_entry_iteration_test")
        set(source_file smbios_entry_iteration_test.cpp)
    else()
        set(source_file smbios_typed_getters_test.cpp)
    endif()

    add_executable(${test_name} ${source_file})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/libsmbios/include
        ${CMAKE_SOURCE_DIR}/sys_interface/include
    )

    if(APPLE)
        target_link_options(${test_name} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
    elseif(MSVC)
        target_link_options(${test_name} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
    else()
        target_link_options(${test_name} PRIVATE
            "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
    endif()

    target_link_libraries(${test_name} PRIVATE smbios_c libsmbios)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

SET_PROPERTY(TARGET
    smbios_c_compile_as_c_test
    smbios_c_example
    smbios_c_open_close_test
    smbios_c_entry_iteration_test
    smbios_c_typed_getters_test
    PROPERTY FOLDER "Tests"
)

# Windows: the loader looks for smbios_c.dll next to the executable (or on
# PATH), but smbios_c's own target writes it to its own build directory
# (smbios_c/<config>/), not to this file's RUNTIME_OUTPUT_DIRECTORY. vcpkg's
# applocal step only copies vcpkg-installed DLLs, never a project-built one.
# Without this copy, every test that actually calls an smbios_* function
# fails to start with STATUS_DLL_NOT_FOUND - and on a headless CI runner
# the loader's "System Error" dialog never gets dismissed, so ctest sees a
# hang (the 60s timeouts first seen on windows-latest, CI run 42630026324),
# not a crash. smbios_c_compile_as_c_test was the only one that passed
# because it references no smbios_* symbol, so MSVC records no import of
# smbios_c.dll at all. $<TARGET_FILE:...> (not $<TARGET_RUNTIME_DLLS:...>)
# keeps this working at the root CMakeLists.txt's cmake_minimum_required
# of 3.13; smbios_c is the only project-built DLL these tests load.
if(WIN32)
    foreach(test_name
            smbios_c_compile_as_c_test
            smbios_c_example
            smbios_c_open_close_test
            smbios_c_entry_iteration_test
            smbios_c_typed_getters_test)
        add_custom_command(TARGET ${test_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:smbios_c> $<TARGET_FILE_DIR:${test_name}>
            VERBATIM
        )
    endforeach()
endif()
