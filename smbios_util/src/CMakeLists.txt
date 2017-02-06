set(TARGET smbios_util)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS program_options REQUIRED) 

file(GLOB SOURCES *.cpp ../include/${TARGET}/*.h)
 
include_directories(${Boost_INCLUDE_DIRS} 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(${TARGET} ${SOURCES})

target_link_libraries(${TARGET}
    ${Boost_LIBRARIES}
    smbios)

add_dependencies(${TARGET} smbios)
