project(ugrep)

cmake_minimum_required(VERSION 2.6)

set(UGREP_VERSION_MAJOR 0)
set(UGREP_VERSION_MINOR 1)

configure_file(
    "config.h.in"
    "config.h"
)

# http://github.com/facebook/hiphop-php/blob/master/CMake/FindICU.cmake
#find_library(ICUDATA icudata)
#find_library(ICUI18N icui18n)
#find_library(ICUIO icuio)
#find_library(ICULE icule)
#find_library(ICULX iculx)
#find_library(ICUTU icutu)
find_package(ICU REQUIRED)

find_package(ZLIB)

set(BASE_SOURCES ugrep.c alloc.c ustring.c mmio.c stdio.c)
set(EXTRA_SOURCES)
set(EXTRA_LIBS)

option(DEBUG "debug" OFF)
if(DEBUG)
    add_definitions(-DDEBUG)
endif(DEBUG)

if(ZLIB_FOUND)
    SET(EXTRA_SOURCES ${EXTRA_SOURCES} zlib.c)
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${ZLIB_LIBRARIES} )
endif(ZLIB_FOUND)

add_executable(ugrep ${BASE_SOURCES} ${EXTRA_SOURCES})
target_link_libraries(ugrep icudata icui18n icuio icule iculx icutu ${EXTRA_LIBS})

add_executable(utf16 utf16.c)
target_link_libraries(utf16 icudata icui18n icuio icule iculx icutu)

install(TARGETS ugrep DESTINATION bin)