# Milestone 0011 (ADR 0003): smbios_gui. Two targets live here:
#
# - libsmbios_gui_model (Story 02.0, Task 01): STATIC library holding the
#   Dear-ImGui-free view model (contract GC1, view_model.hpp/.cpp) and the
#   renderer's pure text projections (Task 02, field_text.hpp/.cpp) and the
#   "Copy as..." render/lookup logic (Task 03, copy_as.hpp/.cpp). It needs
#   only libsmbios, so it is ALWAYS built - the root CMakeLists.txt adds this
#   directory unconditionally - and the unit suite (tests/unit/, Task 04) can
#   link it in every build, including default CI, with no imgui/glfw/OpenGL.
#   Its sources are listed explicitly (SMBIOS_GUI_MODEL_SOURCES); any further
#   ImGui-free source that tests must reach belongs in that list too.
# - smbios_gui (Story 01.0, Task 02): the GLFW + OpenGL3 + Dear ImGui
#   executable. Only defined when SMBIOS_BUILD_GUI=ON; every imgui/glfw3/
#   OpenGL lookup stays inside that branch. It globs src/*.cpp minus the
#   model sources and links libsmbios_gui_model.
# - smbios_gui_autoexit (Story 04.0, Task 04): a LOCAL-ONLY TEST TWIN of
#   smbios_gui, not an app. Same sources and settings plus the PRIVATE
#   definition SMBIOS_GUI_TEST_HOOKS=1, which compiles main.cpp's
#   `--exit-after-frames N` auto-exit hook in. Created only under the local
#   GUI tier's fail-closed condition (never on CI), never installed, and
#   written to the test bin directory. See the section at the end.

# ---------------------------------------------------------------------------
# libsmbios_gui_model - always built, no GUI dependencies.
# ---------------------------------------------------------------------------
set(SMBIOS_GUI_MODEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/smbios/gui/view_model.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/field_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/smbios/gui/field_text.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/copy_as.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/smbios/gui/copy_as.hpp)

add_library(libsmbios_gui_model STATIC ${SMBIOS_GUI_MODEL_SOURCES})
set_target_properties(libsmbios_gui_model PROPERTIES PREFIX "")

target_include_directories(libsmbios_gui_model PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libsmbios/include)

target_compile_options(libsmbios_gui_model PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})

# PUBLIC libsmbios: view_model.hpp includes libsmbios/field_value.hpp, and
# build_entry_tree() calls SMBios/SMBiosEntryFactory. The whole-archive link
# that makes the entry factory's and the output-formatter registry
# non-empty is the *final executable's* job (smbios_gui below,
# smbios_unit_test for tests), not this library's: without it,
# render_entry_as() finds no formatter and create_entry_for_node() no entry
# class. Note the CLI-only `csv` example formatter (smbios/examples/) is not
# in libsmbios, so it is never registered here.
target_link_libraries(libsmbios_gui_model PUBLIC libsmbios)

if(NOT SMBIOS_BUILD_GUI)
    return()
endif()

# ---------------------------------------------------------------------------
# smbios_gui executable - SMBIOS_BUILD_GUI=ON only.
# ---------------------------------------------------------------------------
set(TARGET smbios_gui)

# Target names verified against the vcpkg-installed config files
# (share/imgui/imgui-targets.cmake exports imgui::imgui, which itself
# depends on glfw; share/glfw3/glfw3Targets.cmake exports glfw). Both come
# from vcpkg.json's opt-in "gui" manifest feature, so a missing package
# almost always means VCPKG_MANIFEST_FEATURES=gui was not passed.
find_package(imgui CONFIG QUIET)
find_package(glfw3 CONFIG QUIET)
if(NOT imgui_FOUND OR NOT glfw3_FOUND)
    message(FATAL_ERROR
        "SMBIOS_BUILD_GUI=ON but imgui and/or glfw3 were not found. Their "
        "vcpkg dependencies live in vcpkg.json's opt-in \"gui\" manifest "
        "feature: re-configure with -DVCPKG_MANIFEST_FEATURES=gui (or use an "
        "<os>-gui preset, e.g. 'cmake --preset macos-gui'), or leave "
        "SMBIOS_BUILD_GUI off.")
endif()
find_package(OpenGL REQUIRED)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/smbios/gui/*.hpp)
list(REMOVE_ITEM SOURCES ${SMBIOS_GUI_MODEL_SOURCES})

# smbios_gui_configure_executable(<target>): every compile, include and
# link setting of the GUI executable, shared by smbios_gui and its test twin
# smbios_gui_autoexit so the two cannot drift apart. It adds no definition
# beyond GL_SILENCE_DEPRECATION; the twin's SMBIOS_GUI_TEST_HOOKS is added
# separately, to the twin only.
function(smbios_gui_configure_executable target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/libsmbios/include
    )

    target_compile_options(${target} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
    target_link_options(${target} PRIVATE ${SMBIOS_LINK_OPTIONS})

    # Apple marks every OpenGL entry point deprecated (macOS 10.14+); with
    # SMBIOS_WARNING_OPTIONS' -Werror, the glClear()/glViewport() calls in
    # main.cpp would otherwise fail the build. The deprecation is known and
    # accepted: ADR 0003 / plan.md pick GLFW + OpenGL3 on every platform.
    if(APPLE)
        target_compile_definitions(${target} PRIVATE GL_SILENCE_DEPRECATION)
    endif()

    # Whole-archive link of libsmbios - the identical three-way block
    # smbios/src/CMakeLists.txt, smbios_c/CMakeLists.txt and
    # examples/CMakeLists.txt already carry, and for the identical reason
    # (REGISTER_SMBIOS_ENTRY's and REGISTER_OUTPUT_FORMATTER's static
    # initializers only run for object files actually linked in). Needed from
    # this task onward even though main.cpp names no libsmbios type yet, so
    # Story 02.0 does not have to rediscover the missing-whole-archive bug.
    # See any of those three files' own comment for the full rationale.
    if(APPLE)
        target_link_options(${target} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
    elseif(MSVC)
        target_link_options(${target} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
    else()
        target_link_options(${target} PRIVATE
            "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
    endif()

    target_link_libraries(${target} PRIVATE
        libsmbios_gui_model
        libsmbios
        imgui::imgui
        glfw
        OpenGL::GL)

    add_dependencies(${target} libsmbios)
endfunction()

add_executable(${TARGET} ${SOURCES})

# Emit the binary to <build>/smbios_gui/bin, the per-target counterpart of
# the CLI's <build>/smbios/bin.
set_target_properties(${TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

smbios_gui_configure_executable(${TARGET})

# ---------------------------------------------------------------------------
# Shipped-binary guard (Story 04.0 Task 04; threat model
# docs/security/2026-11-11-gui-test-tiers.md, T-4/R-13).
# ---------------------------------------------------------------------------
# SMBIOS_GUI_TEST_HOOKS compiles main.cpp's test-only auto-exit hook in. It
# must only ever be a PRIVATE definition of smbios_gui_autoexit. Fail the
# configure if it could reach the shipped smbios_gui through any other
# route: smbios_gui's own definitions/options, this directory's definitions
# (which include any inherited from the root via add_definitions()/
# add_compile_definitions()), CMAKE_CXX_FLAGS (seeded from the CXXFLAGS
# environment variable) and every CMAKE_CXX_FLAGS_<CONFIG>, the shared
# SMBIOS_*_OPTIONS lists, or the INTERFACE definitions/options of the
# libraries smbios_gui links. The check is deferred to the end of the
# top-level configure (CMake >= 3.19) where possible, so a definition added
# after this point is caught too; older CMake checks it here only. It only
# reads properties: smbios_gui's compile line is unchanged.
# smbios_gui_fail_if_test_hooks_in(<where> <value>): FATAL_ERROR naming
# <where> if <value> (a string or list) mentions SMBIOS_GUI_TEST_HOOKS.
function(smbios_gui_fail_if_test_hooks_in where value)
    string(FIND "${value}" "SMBIOS_GUI_TEST_HOOKS" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR
            "SMBIOS_GUI_TEST_HOOKS reaches the shipped smbios_gui target via "
            "${where}. It may only be a PRIVATE definition of the local-only "
            "test twin smbios_gui_autoexit (threat model "
            "docs/security/2026-11-11-gui-test-tiers.md, R-13). Remove it "
            "(check CXXFLAGS and -DCMAKE_CXX_FLAGS* too) and re-configure.")
    endif()
endfunction()

function(smbios_gui_fail_if_test_hooks_reach_shipped_target gui_dir)
    set(configs DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES CMAKE_BUILD_TYPE)
        string(TOUPPER "${config}" config_upper)
        list(APPEND configs ${config_upper})
    endforeach()
    smbios_gui_fail_if_test_hooks_in(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    foreach(config IN LISTS configs)
        smbios_gui_fail_if_test_hooks_in(CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
    endforeach()
    smbios_gui_fail_if_test_hooks_in(SMBIOS_COMPILE_OPTIONS "${SMBIOS_COMPILE_OPTIONS}")
    smbios_gui_fail_if_test_hooks_in(SMBIOS_WARNING_OPTIONS "${SMBIOS_WARNING_OPTIONS}")
    foreach(property IN ITEMS COMPILE_DEFINITIONS COMPILE_OPTIONS)
        get_directory_property(value DIRECTORY ${gui_dir} ${property})
        smbios_gui_fail_if_test_hooks_in("the smbios_gui directory's ${property}" "${value}")
        get_target_property(value smbios_gui ${property})
        smbios_gui_fail_if_test_hooks_in("smbios_gui's ${property}" "${value}")
    endforeach()
    foreach(library IN ITEMS libsmbios_gui_model libsmbios imgui::imgui glfw)
        if(TARGET ${library})
            foreach(property IN ITEMS INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_OPTIONS)
                get_target_property(value ${library} ${property})
                smbios_gui_fail_if_test_hooks_in("${library}'s ${property}" "${value}")
            endforeach()
        endif()
    endforeach()
endfunction()

smbios_gui_fail_if_test_hooks_reach_shipped_target("${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT CMAKE_VERSION VERSION_LESS 3.19)
    # EVAL bakes this directory's path into the deferred call now; a plain
    # DEFER CALL would expand ${CMAKE_CURRENT_SOURCE_DIR} when it runs, in
    # the root directory's scope.
    cmake_language(EVAL CODE "
        cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
            CALL smbios_gui_fail_if_test_hooks_reach_shipped_target [[${CMAKE_CURRENT_SOURCE_DIR}]])")
endif()

# ---------------------------------------------------------------------------
# smbios_gui_autoexit - local-only TEST TWIN (Story 04.0 Task 04; R-12).
# ---------------------------------------------------------------------------
# Not an app: the shipped smbios_gui plus main.cpp's test-only
# `--exit-after-frames N` hook, for the local GUI tier
# (tests/gui/CMakeLists.txt) to launch. Rules (threat model R-12):
# - Created only under the local tier's own fail-closed condition, the
#   same three-part check tests/gui/CMakeLists.txt makes, so it never exists
#   on CI (SMBIOS_CI_ENVIRONMENT is recomputed on every configure).
# - Its own add_executable compiles main.cpp itself from the same
#   ${SOURCES}; no OBJECT library is shared with smbios_gui, so the define
#   cannot leak into the shipped object files.
# - SMBIOS_GUI_TEST_HOOKS=1 is a PRIVATE definition of this target only.
# - No install rule (smbios_gui has none either); the output goes to the
#   test bin directory (<build>/libsmbios/bin), never smbios_gui/bin.
if(SMBIOS_BUILD_GUI AND SMBIOS_GUI_LOCAL_TESTS AND NOT SMBIOS_CI_ENVIRONMENT)
    add_executable(smbios_gui_autoexit ${SOURCES})
    set_target_properties(smbios_gui_autoexit PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/libsmbios/bin"
        FOLDER "Tests")
    smbios_gui_configure_executable(smbios_gui_autoexit)
    target_compile_definitions(smbios_gui_autoexit PRIVATE SMBIOS_GUI_TEST_HOOKS=1)
endif()
