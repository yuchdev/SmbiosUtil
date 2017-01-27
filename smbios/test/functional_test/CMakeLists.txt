project(smbios_functional_test CXX)
set(TARGET smbios_functional_test)

cmake_policy(SET CMP0015 NEW)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework date_time REQUIRED) 

file(GLOB SOURCES *.cpp)
 
include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY})

 
add_test(NAME smbios_functional_test COMMAND ${TARGET})

SET_PROPERTY(TARGET smbios_functional_test PROPERTY FOLDER "Tests")
