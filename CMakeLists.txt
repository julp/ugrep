project(ugrep)

cmake_minimum_required(VERSION 2.6)

include(CheckLibraryExists)

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
    add_definitions(-DDEBUG)
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
