project(smbios_unit_test CXX)
set(TARGET smbios_unit_test)

cmake_policy(SET CMP0015 NEW)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../libsmbios/bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework REQUIRED)

file(GLOB SOURCES *.cpp)

# The `csv` example formatter lives outside libsmbios (smbios/examples/, R5);
# compile it in directly so csv_formatter_test.cpp can reach it through the
# registry. Build wiring only - the example source is not edited for tests.
list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/smbios/examples/csv_formatter.cpp)

include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../libsmbios/include)

add_executable(${TARGET} ${SOURCES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS})

# Root of the checked-in test data (tests/data/), read - and, with
# SMBIOS_REGENERATE_DOC_SAMPLES=1, written - by doc_samples_test.cpp's
# golden-file comparison (tests/data/output_formats/sample.*).
target_compile_definitions(${TARGET} PRIVATE SMBIOS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

# Whole-archive link of libsmbios - the identical three-way block
# smbios/src/CMakeLists.txt, smbios_c/CMakeLists.txt, tests/c_abi/ and
# smbios_gui/CMakeLists.txt carry, for the identical reason:
# REGISTER_SMBIOS_ENTRY's and REGISTER_OUTPUT_FORMATTER's static
# initializers only run for object files actually linked in. Before
# milestone 0011 this suite got away without it, because some test TU named
# every concrete entry/formatter class; the GUI view-model tests
# (gui_*_test.cpp, Story 02.0 Task 04) reach entries and formatters only
# through SMBiosEntryFactory/get_output_formatter(), so the registries must
# be complete regardless of which classes other TUs happen to name.
if(APPLE)
    target_link_options(${TARGET} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
elseif(MSVC)
    target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
else()
    target_link_options(${TARGET} PRIVATE
        "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
endif()

# libsmbios_gui_model (smbios_gui/CMakeLists.txt, always built, no
# imgui/glfw/OpenGL dependency) carries the Dear-ImGui-free view model,
# field-text projections and "Copy as..." logic that gui_*_test.cpp test.
# Its PUBLIC include directory provides <smbios/gui/*.hpp>.
target_link_libraries(${TARGET}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    libsmbios_gui_model
    libsmbios)


add_test(NAME smbios_unit_test COMMAND ${TARGET})

SET_PROPERTY(TARGET smbios_unit_test PROPERTY FOLDER "Tests")
