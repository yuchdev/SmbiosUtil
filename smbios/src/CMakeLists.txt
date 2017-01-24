set(TARGET smbios)

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS iostreams REQUIRED)

file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../include/${TARGET}/*.h)

include_directories(${Boost_INCLUDE_DIRS} 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_library(${TARGET} ${SOURCES})

target_link_libraries(${TARGET} ${Boost_LIBRARIES})

