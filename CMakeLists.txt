cmake_minimum_required(VERSION 3.13) # target_link_options requires 3.13+
project(smbios_utility CXX)

enable_testing()

set(BOOST_MIN_VERSION "1.61.0")
set(Boost_ADDITIONAL_VERSIONS "1.61.0" "1.61")
set(Boost_USE_STATIC_LIBS ON)

option("GIT_TAG" GIT_TAG OFF)

# Milestone 0006, Story 02.0, Task 02 (R4's fourth boundary-test category):
# a locally-runnable libFuzzer harness, off by default and never built by
# CI (see docs/dev/fuzzing.md). libFuzzer is a Clang/LLVM-only feature, so
# this option is rejected outright on any other compiler rather than
# silently skipped or silently miscompiled.
option("SMBIOS_BUILD_FUZZERS" SMBIOS_BUILD_FUZZERS OFF)

if(SMBIOS_BUILD_FUZZERS AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "SMBIOS_BUILD_FUZZERS=ON requires Clang or AppleClang (libFuzzer is "
        "a Clang/LLVM-only feature); detected compiler is "
        "'${CMAKE_CXX_COMPILER_ID}'. Re-configure with a Clang toolchain, or "
        "leave SMBIOS_BUILD_FUZZERS off.")
endif()

find_package(Boost ${BOOST_MIN_VERSION} REQUIRED)

# Language standard: C++20, strictly (no GNU/MS extensions), uniformly for
# every target configured below (libsmbios, its per-OS/legacy libraries,
# the smbios CLI, and the tests/* executables). Setting these as directory-scope variables before the
# add_subdirectory() calls below makes them the default CXX_STANDARD /
# CXX_STANDARD_REQUIRED / CXX_EXTENSIONS target properties for every target
# created in this directory and its subdirectories.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    message("UNIX congiguration, Clang")
    set(SMBIOS_COMPILE_OPTIONS -stdlib=libc++)
    set(SMBIOS_LINK_OPTIONS -stdlib=libc++)
    set(SMBIOS_WARNING_OPTIONS -Wall -Wextra -Werror)
elseif(${CMAKE_COMPILER_IS_GNUCXX})
    message("UNIX congiguration, GCC")
    set(SMBIOS_COMPILE_OPTIONS -pthread -fPIC)
    set(SMBIOS_LINK_OPTIONS -pthread)
    set(SMBIOS_WARNING_OPTIONS -Wall -Wextra -Werror)
elseif(${WIN32})
    message("Windows configuraion")
    set(SMBIOS_COMPILE_OPTIONS /EHsc)
    set(SMBIOS_WARNING_OPTIONS /W4 /WX)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)

endif()

string(TIMESTAMP MY_BUILD_DATE "%Y%m%d" UTC)
message("Current build date " ${MY_BUILD_DATE})

if(GIT_TAG)

find_package (Git)
if (GIT_FOUND)
    message("git found: ${GIT_EXECUTABLE} version ${GIT_VERSION_STRING}")
else()
    message(FATAL_ERROR "Git required, CMake will exit!" )
endif (GIT_FOUND)

set(MY_VERSION 1)
set(MY_MAJOR_REVISION 0)
set(MY_MINOR_REVISION 2)
set(SETTINGS_VERSION 0)

execute_process(
    COMMAND ${GIT_EXECUTABLE} tag
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT)

message("Result: " ${RESULT})
message("Output: " ${OUTPUT})

string(REGEX REPLACE "\n" ";" LIGHTWEIGHT_TAGS_LIST ${OUTPUT})

message("Tags list: " ${LIGHTWEIGHT_TAGS_LIST})
list(REMOVE_ITEM LIGHTWEIGHT_TAGS_LIST "")
list(LENGTH LIGHTWEIGHT_TAGS_LIST LISTCOUNT)


if(LISTCOUNT EQUAL 0)
    message("Tags list is empty, create tag with current version")
    execute_process(COMMAND ${GIT_EXECUTABLE} tag ${MY_VERSION}.${MY_MAJOR_REVISION}.${MY_MINOR_REVISION}.${MY_BUILD_DATE}0)
elseif(LISTCOUNT EQUAL 1)
    list(GET LIGHTWEIGHT_TAGS_LIST -1 LAST_TAG)
elseif(LISTCOUNT GREATER 1)
    message("Tags list size  = " ${LISTCOUNT})
    list(GET LIGHTWEIGHT_TAGS_LIST -1 LAST_TAG)
    foreach(CURRENT_TAG ${LIGHTWEIGHT_TAGS_LIST})
        if(NOT ${CURRENT_TAG} STREQUAL ${LAST_TAG})
            message("Should be deleted " ${CURRENT_TAG})
            execute_process(COMMAND ${GIT_EXECUTABLE} tag -d ${CURRENT_TAG})
        endif()
    endforeach()
endif()

message("Version file contains: " ${LAST_TAG})

string(REGEX REPLACE "[.]|\n" ";" OLD_VERSION_FILE_LIST ${LAST_TAG})

message("Parse version tag")
list(GET OLD_VERSION_FILE_LIST 0 MY_OLD_VERSION)
list(GET OLD_VERSION_FILE_LIST 1 MY_OLD_MAJOR_REVISION)
list(GET OLD_VERSION_FILE_LIST 2 MY_OLD_MINOR_REVISION)
list(GET OLD_VERSION_FILE_LIST 3 MY_OLD_BUILD_NUMBER)
string(SUBSTRING ${MY_OLD_BUILD_NUMBER} 0 8 MY_OLD_BUILD_DATE)

message("Old build date: " ${MY_OLD_BUILD_DATE})
message("New build date: " ${MY_BUILD_DATE})

if(${MY_OLD_BUILD_DATE} EQUAL ${MY_BUILD_DATE})
  message("Build same date, increment build number")
  MATH(EXPR MY_BUILD_NUMBER "${MY_OLD_BUILD_NUMBER}+1")
else()
  message("New build date,set build number to zero")
  MATH(EXPR MY_BUILD_NUMBER "${MY_BUILD_DATE}*10")
endif()

message("New build number: " ${MY_BUILD_NUMBER})
message("Application version: " ${MY_VERSION})
message("Major revision: " ${MY_MAJOR_REVISION})
message("Minor revision: " ${MY_MINOR_REVISION})
message("Settings version: " ${SETTINGS_VERSION})

execute_process(COMMAND ${GIT_EXECUTABLE} tag -d ${LAST_TAG})
execute_process(COMMAND ${GIT_EXECUTABLE} tag ${MY_VERSION}.${MY_MAJOR_REVISION}.${MY_MINOR_REVISION}.${MY_BUILD_NUMBER})

endif()
# GIT_TAG

add_subdirectory(os_interface)

if(WIN32)
    add_subdirectory(libsmbios_win_legacy)
    add_subdirectory(libsmbios_win)
    set(SMBIOS_LEGACY_LIB libsmbios_win_legacy)
    set(SMBIOS_PLATFORM_LIB libsmbios_win)
elseif(APPLE)
    add_subdirectory(libsmbios_posix_legacy)
    add_subdirectory(libsmbios_macos)
    set(SMBIOS_LEGACY_LIB libsmbios_posix_legacy)
    set(SMBIOS_PLATFORM_LIB libsmbios_macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(libsmbios_posix_legacy)
    add_subdirectory(libsmbios_linux)
    set(SMBIOS_LEGACY_LIB libsmbios_posix_legacy)
    set(SMBIOS_PLATFORM_LIB libsmbios_linux)
else()
    add_subdirectory(libsmbios_posix_legacy)
    set(SMBIOS_LEGACY_LIB libsmbios_posix_legacy)
    set(SMBIOS_PLATFORM_LIB "")
endif()

add_subdirectory(libsmbios)
add_subdirectory(smbios)
add_subdirectory(smbios_c)
add_subdirectory(tests)

# Milestone 0006, Story 02.0, Task 02: the fuzz harness target itself, built
# only when explicitly requested (see docs/dev/fuzzing.md). Defined here
# rather than via a tests/fuzz/CMakeLists.txt subdirectory, since it is not
# one of tests/CMakeLists.txt's four always-registered ctest categories
# (unit/mock/perf/integration) and must stay entirely out of the default
# build graph. parse_synthetic_table() (the only symbol this target needs
# from tests/unit/synthetic_table_builder.h) is defined inline in that
# header, so this target needs no dependency on the smbios_unit_test
# sources themselves - only the header's include path and libsmbios to link
# against.
if(SMBIOS_BUILD_FUZZERS)
    set(SMBIOS_FUZZER_TARGET read_smbios_table_fuzzer)

    add_executable(${SMBIOS_FUZZER_TARGET}
        tests/fuzz/read_smbios_table_fuzzer.cpp)

    target_include_directories(${SMBIOS_FUZZER_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libsmbios/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit)

    target_compile_options(${SMBIOS_FUZZER_TARGET} PRIVATE
        ${SMBIOS_COMPILE_OPTIONS}
        -fsanitize=fuzzer,address,undefined
        -g)

    # Apple's own Clang (CMAKE_CXX_COMPILER_ID "AppleClang", the default
    # compiler selected by the "macos" preset) accepts "-fsanitize=fuzzer"
    # at *compile* time (it only adds SanitizerCoverage instrumentation
    # hooks) but never ships libclang_rt.fuzzer_osx.a - the actual libFuzzer
    # runtime/driver - in its own resource directory, so passing "fuzzer" to
    # its *link* step fails with a plain "file not found" (confirmed on this
    # project's own dev machine: Xcode 14 / AppleClang 14 has no fuzzer
    # runtime at all; this is a standing Apple Clang limitation, not a
    # version-specific bug). Detect this and, if found, fall back to an
    # externally-installed real LLVM's runtime archive (e.g. Homebrew's
    # `llvm` formula - `brew install llvm`) linked in explicitly, with
    # "fuzzer" dropped from the *link*-time sanitizer spec so AppleClang's
    # own driver does not also try (and fail) to auto-link its own missing
    # copy. Any other Clang (mainline LLVM on Linux, etc.) ships its own
    # runtime and uses the straightforward path.
    set(SMBIOS_FUZZER_RUNTIME "")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
            OUTPUT_VARIABLE SMBIOS_APPLECLANG_RESOURCE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        find_file(SMBIOS_FUZZER_RUNTIME libclang_rt.fuzzer_osx.a
            PATHS ${SMBIOS_APPLECLANG_RESOURCE_DIR}/lib/darwin
            NO_DEFAULT_PATH)

        if(NOT SMBIOS_FUZZER_RUNTIME)
            # AppleClang has no fuzzer runtime of its own (expected) - look
            # for a separately-installed real LLVM that does, in the
            # locations Homebrew's `llvm` formula is known to use.
            file(GLOB SMBIOS_EXTERNAL_LLVM_RESOURCE_DIRS
                /usr/local/opt/llvm/lib/clang/*
                /opt/homebrew/opt/llvm/lib/clang/*)
            foreach(SMBIOS_CANDIDATE_DIR ${SMBIOS_EXTERNAL_LLVM_RESOURCE_DIRS})
                if(EXISTS ${SMBIOS_CANDIDATE_DIR}/lib/darwin/libclang_rt.fuzzer_osx.a)
                    set(SMBIOS_FUZZER_RUNTIME
                        ${SMBIOS_CANDIDATE_DIR}/lib/darwin/libclang_rt.fuzzer_osx.a)
                endif()
            endforeach()
        endif()

        if(NOT SMBIOS_FUZZER_RUNTIME)
            message(FATAL_ERROR
                "SMBIOS_BUILD_FUZZERS=ON: AppleClang (the compiler selected "
                "by this configure) does not ship a libFuzzer runtime, and "
                "no externally-installed LLVM with one "
                "(libclang_rt.fuzzer_osx.a) was found under "
                "/usr/local/opt/llvm or /opt/homebrew/opt/llvm. Install a "
                "real LLVM (e.g. 'brew install llvm') and re-configure, or "
                "build on Linux with mainline Clang instead.")
        endif()

        target_link_options(${SMBIOS_FUZZER_TARGET} PRIVATE
            ${SMBIOS_LINK_OPTIONS}
            -fsanitize=address,undefined)
        target_link_libraries(${SMBIOS_FUZZER_TARGET} PRIVATE ${SMBIOS_FUZZER_RUNTIME})
    else()
        target_link_options(${SMBIOS_FUZZER_TARGET} PRIVATE
            ${SMBIOS_LINK_OPTIONS}
            -fsanitize=fuzzer,address,undefined)
    endif()

    target_link_libraries(${SMBIOS_FUZZER_TARGET} PRIVATE libsmbios)

    set_target_properties(${SMBIOS_FUZZER_TARGET} PROPERTIES FOLDER "Tests")
endif()

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
