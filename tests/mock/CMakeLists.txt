project(smbios_mock_test CXX)
set(TARGET smbios_mock_test)

cmake_policy(SET CMP0015 NEW)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../smbios/bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework REQUIRED)

file(GLOB SOURCES *.cpp)

include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../smbios/include)

add_executable(${TARGET} ${SOURCES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_CXX_STANDARD_FLAG})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

target_link_libraries(${TARGET}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    smbios)


add_test(NAME smbios_mock_test COMMAND ${TARGET})

SET_PROPERTY(TARGET smbios_mock_test PROPERTY FOLDER "Tests")
