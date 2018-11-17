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

string(TIMESTAMP MY_BUILD_DATE "%Y%m%d" UTC)
message("Current build date " ${MY_BUILD_DATE})

find_package (Git)
if (GIT_FOUND)
    message("git found: ${GIT_EXECUTABLE} version ${GIT_VERSION_STRING}")
else()
    message(FATAL_ERROR "Git required, CMake will exit!" )
endif (GIT_FOUND)

set(MY_VERSION 1)
set(MY_MAJOR_REVISION 0)
set(MY_MINOR_REVISION 2)
set(SETTINGS_VERSION 0)

execute_process(
    COMMAND ${GIT_EXECUTABLE} tag
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT)

message("Result: " ${RESULT})
message("Output: " ${OUTPUT})

string(REGEX REPLACE "\n" ";" LIGHTWEIGHT_TAGS_LIST ${OUTPUT})

message("Tags list: " ${LIGHTWEIGHT_TAGS_LIST})
list(REMOVE_ITEM LIGHTWEIGHT_TAGS_LIST "")
list(LENGTH LIGHTWEIGHT_TAGS_LIST LISTCOUNT)


if(LISTCOUNT EQUAL 0)
    message("Tags list is empty, create tag with current version")
    execute_process(COMMAND ${GIT_EXECUTABLE} tag ${MY_VERSION}.${MY_MAJOR_REVISION}.${MY_MINOR_REVISION}.${MY_BUILD_DATE}0)
elseif(LISTCOUNT EQUAL 1)
    list(GET LIGHTWEIGHT_TAGS_LIST -1 LAST_TAG)
elseif(LISTCOUNT GREATER 1)
    message("Tags list size  = " ${LISTCOUNT})
    list(GET LIGHTWEIGHT_TAGS_LIST -1 LAST_TAG)
    foreach(CURRENT_TAG ${LIGHTWEIGHT_TAGS_LIST})
        if(NOT ${CURRENT_TAG} STREQUAL ${LAST_TAG})
            message("Should be deleted " ${CURRENT_TAG})
            execute_process(COMMAND ${GIT_EXECUTABLE} tag -d ${CURRENT_TAG})
        endif()
    endforeach()
endif()

message("Version file contains: " ${LAST_TAG})

string(REGEX REPLACE "[.]|\n" ";" OLD_VERSION_FILE_LIST ${LAST_TAG})

message("Parse version tag")
list(GET OLD_VERSION_FILE_LIST 0 MY_OLD_VERSION)
list(GET OLD_VERSION_FILE_LIST 1 MY_OLD_MAJOR_REVISION)
list(GET OLD_VERSION_FILE_LIST 2 MY_OLD_MINOR_REVISION)
list(GET OLD_VERSION_FILE_LIST 3 MY_OLD_BUILD_NUMBER)
string(SUBSTRING ${MY_OLD_BUILD_NUMBER} 0 8 MY_OLD_BUILD_DATE)

message("Old build date: " ${MY_OLD_BUILD_DATE})
message("New build date: " ${MY_BUILD_DATE})

if(${MY_OLD_BUILD_DATE} EQUAL ${MY_BUILD_DATE})
  message("Build same date, increment build number")
  MATH(EXPR MY_BUILD_NUMBER "${MY_OLD_BUILD_NUMBER}+1")
else()
  message("New build date,set build number to zero")
  MATH(EXPR MY_BUILD_NUMBER "${MY_BUILD_DATE}*10")
endif()

message("New build number: " ${MY_BUILD_NUMBER})
message("Application version: " ${MY_VERSION})
message("Major revision: " ${MY_MAJOR_REVISION})
message("Minor revision: " ${MY_MINOR_REVISION})
message("Settings version: " ${SETTINGS_VERSION})

execute_process(COMMAND ${GIT_EXECUTABLE} tag -d ${LAST_TAG})
execute_process(COMMAND ${GIT_EXECUTABLE} tag ${MY_VERSION}.${MY_MAJOR_REVISION}.${MY_MINOR_REVISION}.${MY_BUILD_NUMBER})

add_subdirectory(smbios)
add_subdirectory(smbios_util)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
