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

# Milestone 0009, Story 03.0, Task 03: the standalone library-consumer
# examples under examples/ and their smbios_example_* CTest smoke tests.
# Unlike SMBIOS_BUILD_FUZZERS this is ON by default, so CI always builds and
# runs them and they cannot rot.
option("SMBIOS_BUILD_EXAMPLES" SMBIOS_BUILD_EXAMPLES ON)

# Milestone 0011, Story 01.0: the smbios_gui visualization tool (ADR 0003).
# OFF by default because its imgui/glfw3 dependencies pull in X11/OpenGL
# development packages on Linux that CI and plain CLI/library builds do not
# need. Its dependencies live in vcpkg.json's "gui" manifest feature, so
# enable both together: -DSMBIOS_BUILD_GUI=ON -DVCPKG_MANIFEST_FEATURES=gui
# (or use the <os>-gui presets in CMakePresets.json). Gates only the
# smbios_gui executable; its GUI-free view-model library is always built.
option("SMBIOS_BUILD_GUI" SMBIOS_BUILD_GUI OFF)

# Milestone 0011, Story 04.0, Task 01: the two GUI test tiers under
# tests/gui/ (only added when SMBIOS_BUILD_GUI=ON). The CI-safe tier
# (smbios_gui_ci_test, label gui-ci) is always built with the GUI. The
# local-only tier (smbios_gui_local_test.*, label gui-local) needs a real
# display, GL context, OS clipboard and live SMBIOS table, so it must never
# be configured, compiled or registered on CI.
#
# SMBIOS_CI_ENVIRONMENT is a plain (non-cache) variable recomputed on EVERY
# configure, so neither a stale cache nor a -D can override it (threat
# model docs/security/2026-11-11-gui-test-tiers.md, R-1). Any defined
# GITHUB_ACTIONS or CI counts, even CI=false - the fail-safe direction.
# SMBIOS_GUI_LOCAL_TESTS' default (ON only for a GUI build outside CI) only
# seeds a fresh cache; under CI an ON value (a -D or a reused cache) is
# forced OFF with a warning, and there is deliberately no override.
# tests/gui/CMakeLists.txt re-checks NOT SMBIOS_CI_ENVIRONMENT where the
# local targets are created (R-2), so the tier fails closed even if the
# cached option and this force ever diverge. Never set either variable in
# CMakePresets.json (R-4).
if(DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{CI})
    set(SMBIOS_CI_ENVIRONMENT ON)
else()
    set(SMBIOS_CI_ENVIRONMENT OFF)
endif()

if(SMBIOS_BUILD_GUI AND NOT SMBIOS_CI_ENVIRONMENT)
    set(SMBIOS_GUI_LOCAL_TESTS_DEFAULT ON)
else()
    set(SMBIOS_GUI_LOCAL_TESTS_DEFAULT OFF)
endif()
option(SMBIOS_GUI_LOCAL_TESTS
    "Build/register the local-only GUI tests (display/GL/clipboard/live table; never on CI)"
    ${SMBIOS_GUI_LOCAL_TESTS_DEFAULT})

if(SMBIOS_CI_ENVIRONMENT AND SMBIOS_GUI_LOCAL_TESTS)
    set(SMBIOS_GUI_LOCAL_TESTS OFF CACHE BOOL "" FORCE)
    message(WARNING "SMBIOS_GUI_LOCAL_TESTS forced OFF: CI environment detected (GITHUB_ACTIONS/CI set); the local-only GUI tier never runs on CI.")
endif()

# Not FATAL_ERROR: a cached ON from an earlier GUI configure must not break
# a later non-GUI reconfigure of the same build tree.
if(SMBIOS_GUI_LOCAL_TESTS AND NOT SMBIOS_BUILD_GUI)
    message(STATUS "SMBIOS_GUI_LOCAL_TESTS=ON is ignored because SMBIOS_BUILD_GUI is OFF.")
endif()

if(SMBIOS_BUILD_FUZZERS AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "SMBIOS_BUILD_FUZZERS=ON requires Clang or AppleClang (libFuzzer is "
        "a Clang/LLVM-only feature); detected compiler is "
        "'${CMAKE_CXX_COMPILER_ID}'. Re-configure with a Clang toolchain, or "
        "leave SMBIOS_BUILD_FUZZERS off.")
endif()

find_package(Boost ${BOOST_MIN_VERSION} REQUIRED)

# macOS: Apple clang searches /usr/local/include (where a Homebrew Boost
# lives) BEFORE any -isystem directory, so headers of a Homebrew Boost
# silently shadow the vcpkg Boost that the libraries are linked from. A
# header/library version mismatch then fails at link time (observed:
# undefined boost::program_options::arg, Homebrew 1.90 headers vs vcpkg
# 1.92 libs). Plain -I directories ARE searched before the default system
# paths, so on macOS imported-target include directories (Boost, imgui,
# glfw) are passed as -I rather than -isystem. Must be set before the
# targets that consume them are created. Cost: warnings from those headers
# are no longer suppressed.
if(APPLE)
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
endif()

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

add_subdirectory(sys_interface)

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
if(SMBIOS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
add_subdirectory(smbios_c)
# Always added: smbios_gui/CMakeLists.txt always defines the GUI-free
# libsmbios_gui_model view-model library (so the unit suite can link it in
# every build), and only defines the smbios_gui executable - and only looks
# for imgui/glfw3 - when SMBIOS_BUILD_GUI=ON. Must precede tests/.
add_subdirectory(smbios_gui)
add_subdirectory(tests)

# Milestone 0006, Story 02.0, Task 02: the fuzz harness target itself, built
# only when explicitly requested (see docs/dev/fuzzing.md). Defined here
# rather than via a tests/fuzz/CMakeLists.txt subdirectory, since it is not
# one of tests/CMakeLists.txt's four always-registered ctest categories
# (unit/mock/perf/integration) and must stay entirely out of the default
# build graph. parse_synthetic_table() (the only symbol this target needs
# from tests/unit/synthetic_table_builder.hpp) is defined inline in that
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

    # The harness decodes every walked header through SMBiosEntryFactory
    # (security finding MEDIUM-2) but names no concrete entry class, so -
    # exactly as for the smbios CLI (smbios/src/CMakeLists.txt) - libsmbios
    # must be linked as a whole archive, or no REGISTER_SMBIOS_ENTRY static
    # initializer would run and create() would return nullptr for every type.
    if(APPLE)
        target_link_options(${SMBIOS_FUZZER_TARGET} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
    else()
        target_link_options(${SMBIOS_FUZZER_TARGET} PRIVATE
            "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
    endif()

    target_link_libraries(${SMBIOS_FUZZER_TARGET} PRIVATE libsmbios)

    # Instrument libsmbios itself (coverage + ASan/UBSan), not just the
    # harness translation unit: the decoders under test live there, so
    # without this libFuzzer gets no coverage feedback from them and ASan
    # cannot see their memory accesses. It is also required for correctness:
    # libc++'s ASan container annotations abort ("CHECK failed:
    # asan_poisoning.cpp") when a std::vector grown by uninstrumented
    # libsmbios code is cleared by the instrumented harness (the
    # parse_synthetic_table() seam). The INTERFACE link option lets every
    # other consumer of libsmbios in this fuzz-only build link the
    # sanitizer runtimes too.
    target_compile_options(libsmbios PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -g)
    target_link_options(libsmbios INTERFACE -fsanitize=address,undefined)

    set_target_properties(${SMBIOS_FUZZER_TARGET} PROPERTIES FOLDER "Tests")
endif()

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
