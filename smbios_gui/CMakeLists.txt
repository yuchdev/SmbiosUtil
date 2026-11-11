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

add_executable(${TARGET} ${SOURCES})

# Emit the binary to <build>/smbios_gui/bin, the per-target counterpart of
# the CLI's <build>/smbios/bin.
set_target_properties(${TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libsmbios/include
)

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

# Apple marks every OpenGL entry point deprecated (macOS 10.14+); with
# SMBIOS_WARNING_OPTIONS' -Werror, the glClear()/glViewport() calls in
# main.cpp would otherwise fail the build. The deprecation is known and
# accepted: ADR 0003 / plan.md pick GLFW + OpenGL3 on every platform.
if(APPLE)
    target_compile_definitions(${TARGET} PRIVATE GL_SILENCE_DEPRECATION)
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
    target_link_options(${TARGET} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
elseif(MSVC)
    target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
else()
    target_link_options(${TARGET} PRIVATE
        "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
endif()

target_link_libraries(${TARGET} PRIVATE
    libsmbios_gui_model
    libsmbios
    imgui::imgui
    glfw
    OpenGL::GL)

add_dependencies(${TARGET} libsmbios)
