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

target_link_libraries(${TARGET}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    libsmbios)


add_test(NAME smbios_unit_test COMMAND ${TARGET})

SET_PROPERTY(TARGET smbios_unit_test PROPERTY FOLDER "Tests")
