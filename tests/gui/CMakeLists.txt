# Milestone 0011, Story 04.0 (Task 01 scaffolding): the two smbios_gui test
# tiers. Only added when SMBIOS_BUILD_GUI=ON (tests/CMakeLists.txt).
#
# - smbios_gui_ci_test (labels gui;gui-ci): CI-safe and hermetic. A Dear
#   ImGui context with no window, no GL and no OS clipboard, synthetic
#   tables only. Always built with the GUI, runs in CI's gui-build job.
#   ci/ never includes local_only_guard.hpp, a GLFW header or imgui_impl_*.h.
# - smbios_gui_local_test (labels gui;gui-local): needs a real display, GL
#   context, OS clipboard and the live table. Never configured, compiled or
#   registered on CI - see the gate below and the root CMakeLists.txt's
#   SMBIOS_GUI_LOCAL_TESTS comment.
#
# Both write to <build>/libsmbios/bin, the directory every other test
# executable uses (tests/unit/CMakeLists.txt). Test names all start with
# smbios_gui_, which matches none of ci.yml's ctest -R filters
# (smbios_unit_test, smbios_mock_test, smbios_c_, smbios_cli_*,
# smbios_example_, smbios_integration_test, smbios_perf_test).

set(SMBIOS_GUI_TEST_BIN_DIR "${CMAKE_BINARY_DIR}/libsmbios/bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework REQUIRED)
# imgui::imgui is imported in smbios_gui/'s directory scope only, so look it
# up again here (same CONFIG package; smbios_gui/CMakeLists.txt already
# failed the configure with a helpful message if it is missing).
find_package(imgui CONFIG REQUIRED)

# Whole-archive link of libsmbios - the identical three-way block
# smbios/src/CMakeLists.txt, smbios_gui/CMakeLists.txt and
# tests/unit/CMakeLists.txt carry, for the identical reason:
# REGISTER_SMBIOS_ENTRY's and REGISTER_OUTPUT_FORMATTER's static
# initializers only run for object files actually linked in.
function(smbios_gui_test_whole_archive target)
    if(APPLE)
        target_link_options(${target} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
    elseif(MSVC)
        target_link_options(${target} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
    else()
        target_link_options(${target} PRIVATE
            "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# smbios_gui_ci_test - CI-safe tier, always built with the GUI.
# ---------------------------------------------------------------------------
# entry_tree_view.cpp/copy_as_action.cpp are compiled straight in (the
# csv_formatter.cpp precedent in tests/unit/CMakeLists.txt), so the
# smbios_gui target itself is untouched. They must never also be in
# libsmbios_gui_model (threat model T-10/R-10): the configure-time check
# below fails the configure if either appears in that library's source list
# (the SMBIOS_GUI_MODEL_SOURCES variable of smbios_gui/CMakeLists.txt, and
# the target's own SOURCES property, in case it is extended elsewhere).
# support/headless_imgui.* is the CI tier's headless harness (Task 02): no
# window, no GL, no OS hooks; support/browser_fixture.* is the shared
# render_entry_browser() fixture of the Task 03 widget suites, and
# doc_sample_fixture.cpp is the unit suite's synthetic doc-sample table.
set(SMBIOS_GUI_CI_RENDERING_SOURCES
    ${CMAKE_SOURCE_DIR}/smbios_gui/src/entry_tree_view.cpp
    ${CMAKE_SOURCE_DIR}/smbios_gui/src/copy_as_action.cpp)

get_directory_property(SMBIOS_GUI_MODEL_SOURCES_CHECK
    DIRECTORY ${CMAKE_SOURCE_DIR}/smbios_gui
    DEFINITION SMBIOS_GUI_MODEL_SOURCES)
get_target_property(SMBIOS_GUI_MODEL_TARGET_SOURCES libsmbios_gui_model SOURCES)
if(NOT SMBIOS_GUI_MODEL_SOURCES_CHECK OR NOT SMBIOS_GUI_MODEL_TARGET_SOURCES)
    message(FATAL_ERROR "tests/gui: cannot read libsmbios_gui_model's source list "
        "(threat model R-10 check would be vacuous)")
endif()
foreach(rendering_source IN LISTS SMBIOS_GUI_CI_RENDERING_SOURCES)
    get_filename_component(rendering_name "${rendering_source}" NAME)
    foreach(model_source IN LISTS SMBIOS_GUI_MODEL_SOURCES_CHECK SMBIOS_GUI_MODEL_TARGET_SOURCES)
        get_filename_component(model_name "${model_source}" NAME)
        if(model_name STREQUAL rendering_name)
            message(FATAL_ERROR "tests/gui: ${rendering_name} is in libsmbios_gui_model's sources; "
                "it must only be compiled into smbios_gui and smbios_gui_ci_test (threat model R-10)")
        endif()
    endforeach()
endforeach()

file(GLOB SMBIOS_GUI_CI_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/ci/*.cpp)
list(APPEND SMBIOS_GUI_CI_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/support/headless_imgui.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/headless_imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/browser_fixture.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/browser_fixture.cpp
    ${SMBIOS_GUI_CI_RENDERING_SOURCES}
    ${CMAKE_SOURCE_DIR}/tests/unit/synthetic_table_builder.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/doc_sample_fixture.cpp)

add_executable(smbios_gui_ci_test ${SMBIOS_GUI_CI_TEST_SOURCES})
set_target_properties(smbios_gui_ci_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SMBIOS_GUI_TEST_BIN_DIR}"
    FOLDER "Tests")

# tests/unit/ for synthetic_table_builder.hpp and gui_test_support.hpp;
# smbios_gui/include for entry_tree_view.hpp/copy_as_action.hpp (the model
# headers also arrive through libsmbios_gui_model's PUBLIC include).
target_include_directories(smbios_gui_ci_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/unit
    ${CMAKE_SOURCE_DIR}/smbios_gui/include
    ${CMAKE_SOURCE_DIR}/libsmbios/include)

target_compile_options(smbios_gui_ci_test PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(smbios_gui_ci_test PRIVATE ${SMBIOS_LINK_OPTIONS})
smbios_gui_test_whole_archive(smbios_gui_ci_test)

# No glfw and no OpenGL::GL: nothing in this tier calls into them.
# imgui::imgui pulls glfw in transitively through the vcpkg port, which is
# accepted for the same reason.
target_link_libraries(smbios_gui_ci_test PRIVATE
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    libsmbios_gui_model
    libsmbios
    imgui::imgui)

add_test(NAME smbios_gui_ci_test COMMAND smbios_gui_ci_test)
set_tests_properties(smbios_gui_ci_test PROPERTIES
    LABELS "gui;gui-ci"
    TIMEOUT 60)

# ---------------------------------------------------------------------------
# smbios_gui_local_test - local-only tier.
# ---------------------------------------------------------------------------
# Fail closed (threat model R-2): SMBIOS_CI_ENVIRONMENT is re-checked HERE,
# where the targets are created, not only where the option is seeded and
# forced, so a cached ON can never create local targets under CI. Every
# other local-only artifact (Task 04's smbios_gui_autoexit twin and its
# tests) must sit behind this same three-part condition.
if(NOT (SMBIOS_BUILD_GUI AND SMBIOS_GUI_LOCAL_TESTS AND NOT SMBIOS_CI_ENVIRONMENT))
    return()
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Every source here includes support/local_only_guard.hpp first. Task 04
# adds support/glfw_test_window.{hpp,cpp} and the spawn/clipboard helpers.
file(GLOB SMBIOS_GUI_LOCAL_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/local/*.cpp)
list(APPEND SMBIOS_GUI_LOCAL_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/support/local_only_guard.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/local_skip.hpp)

# One CTest entry per case, so a skipped case shows as Skipped without
# hiding its passing siblings. Keep this list in sync with the
# BOOST_AUTO_TEST_CASEs of suite SmbiosGuiLocalTests in local/*.cpp.
set(SMBIOS_GUI_LOCAL_TEST_CASES
    ScaffoldingPlaceholder
    SkipHelperPlaceholder)

add_executable(smbios_gui_local_test ${SMBIOS_GUI_LOCAL_TEST_SOURCES})
set_target_properties(smbios_gui_local_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SMBIOS_GUI_TEST_BIN_DIR}"
    FOLDER "Tests")

target_include_directories(smbios_gui_local_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/unit
    ${CMAKE_SOURCE_DIR}/smbios_gui/include
    ${CMAKE_SOURCE_DIR}/libsmbios/include)

# PRIVATE and on this target only - never directory scope, PUBLIC/INTERFACE,
# SMBIOS_COMPILE_OPTIONS or a preset (threat model R-3). Without it every
# local source fails with local_only_guard.hpp's #error.
target_compile_definitions(smbios_gui_local_test PRIVATE SMBIOS_GUI_LOCAL_ONLY_TESTS=1)
# Same accepted OpenGL deprecation silencing as smbios_gui.
if(APPLE)
    target_compile_definitions(smbios_gui_local_test PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_compile_options(smbios_gui_local_test PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(smbios_gui_local_test PRIVATE ${SMBIOS_LINK_OPTIONS})
smbios_gui_test_whole_archive(smbios_gui_local_test)

target_link_libraries(smbios_gui_local_test PRIVATE
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    libsmbios_gui_model
    libsmbios
    imgui::imgui
    glfw
    OpenGL::GL)

# SKIP_REGULAR_EXPRESSION is matched against the whole test output, and
# CMake's regex "^" anchors only to the start of that whole string, not of
# each line. Boost.Test prints "Running 1 test case..." first, so the
# task spec's "^SKIP: " never matches: verified on macOS (CMake 3.31), the
# always-skipping SkipHelperPlaceholder case was reported Passed with it.
# "(^|\n)SKIP: " (the "\n" is a real newline after CMake's argument
# parsing) anchors to the start of any line and is reported Skipped. Only
# skip_unless()'s own line can match, because no local case prints live or
# captured data (threat model T-3/T-9).
foreach(case_name IN LISTS SMBIOS_GUI_LOCAL_TEST_CASES)
    set(test_name smbios_gui_local_test.${case_name})
    add_test(NAME ${test_name}
        COMMAND smbios_gui_local_test --run_test=SmbiosGuiLocalTests/${case_name})
    set_tests_properties(${test_name} PROPERTIES
        LABELS "gui;gui-local"
        TIMEOUT 120)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
        set_tests_properties(${test_name} PROPERTIES
            SKIP_REGULAR_EXPRESSION "(^|\n)SKIP: ")
    endif()
endforeach()
