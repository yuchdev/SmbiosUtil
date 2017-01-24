cmake_minimum_required(VERSION 3.0)
project(smbios_utility CXX)

set(BOOST_MIN_VERSION "1.61.0")
set(Boost_ADDITIONAL_VERSIONS "1.61.0" "1.61")
set(Boost_USE_STATIC_LIBS ON)
set(BOOST_ROOT ${MY_BOOST_DIR})

find_package(Boost ${BOOST_MIN_VERSION} REQUIRED)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    message("UNIX congiguration, Clang")
    set(CMAKE_CXX_FLAGS "-std=gnu++11 -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "-std=gnu++11")
elseif(${CMAKE_COMPILER_IS_GNUCXX})
    message("UNIX congiguration, GCC")
    # NOTE! gnu++1y is GCC 4.9 compiler option, in 5 it is deprectaed
    set(CMAKE_CXX_FLAGS "-std=c++11 -std=gnu++1y -pthread -fPIC")
    set(CMAKE_EXE_LINKER_FLAGS "-std=c++11 -std=gnu++1y -pthread")
elseif(${WIN32})
    message("Windows configuraion")
    set(CMAKE_CXX_FLAGS "/EHsc")
    set(MY_BOOST_DIR ${WINDOWS_BOOST_DIR})
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)

endif()

add_subdirectory(smbios)
add_subdirectory(smbios_util)
