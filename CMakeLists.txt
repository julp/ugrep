project(ugrep)

cmake_minimum_required(VERSION 2.6)

include(CheckCCompilerFlag)

set(UGREP_VERSION_MAJOR 0)
set(UGREP_VERSION_MINOR 1)

configure_file(
    "config.h.in"
    "config.h"
)

# http://github.com/facebook/hiphop-php/blob/master/CMake/FindICU.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
find_package(ICU REQUIRED)

find_package(ZLIB)
message(STATUS ZLIB_FOUND = ${ZLIB_FOUND})

find_package(BZip2)
message(STATUS BZIP2_FOUND = ${BZIP2_FOUND})

set(BASE_SOURCES ugrep.c alloc.c ustring.c mmio.c stdio.c slist.c fixedengine.c icureengine.c)
set(EXTRA_SOURCES )
set(EXTRA_LIBS)

option(DEBUG "debug" OFF)
if(DEBUG)
    foreach(cflag -Wall -Wextra -std=c99 -pedantic -Wwrite-strings -Wstrict-prototypes -Wunreachable-code -Wno-comment -Wnonnull ${DEBUG_C_FLAGS})
        check_c_compiler_flag(${cflag} SUPPORT_C_FLAG)
        if(SUPPORT_C_FLAG)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${cflag}")
        endif(SUPPORT_C_FLAG)
        unset(SUPPORT_C_FLAG)
    endforeach(cflag)
    set(CMAKE_REQUIRED_FLAGS ${CMAKE_C_FLAGS})
    add_definitions(${CMAKE_C_FLAGS})

    add_definitions(-DDEBUG)
    set(DEBUG_C_FLAGS -g)
else(DEBUG)
    set(DEBUG_C_FLAGS -O2 -Wuninitialized)
endif(DEBUG)

if(ZLIB_FOUND)
    set(EXTRA_SOURCES ${EXTRA_SOURCES} uncompressio.c)
    include_directories(${ZLIB_INCLUDE_DIR})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${ZLIB_LIBRARIES})
    add_definitions(-DHAVE_ZLIB)
endif(ZLIB_FOUND)

if(BZIP2_FOUND)
    set(EXTRA_SOURCES ${EXTRA_SOURCES} uncompressio.c)
    include_directories(${BZIP2_INCLUDE_DIR})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${BZIP2_LIBRARIES})
    add_definitions(-DHAVE_BZIP2)
endif(BZIP2_FOUND)

list(REMOVE_DUPLICATES EXTRA_SOURCES)

add_executable(ugrep ${BASE_SOURCES} ${EXTRA_SOURCES})
target_link_libraries(ugrep icudata icui18n icuio icule iculx icutu ${EXTRA_LIBS})

install(TARGETS ugrep DESTINATION bin)
