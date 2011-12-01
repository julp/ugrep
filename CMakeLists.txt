cmake_minimum_required(VERSION 2.8.3)

project(ugrep)

if(MSVC)
    # http://msdn.microsoft.com/en-us/library/19z1t1wy%28v=VS.80%29.aspx
    # /GS /Wall
    set(CMAKE_C_FLAGS_MAINTAINER "/D REPLACE_GETOPT /D DEBUG /Od" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
else(MSVC)
    set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -O -Wall -Wextra -std=gnu99 -pedantic -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
endif(MSVC)
mark_as_advanced(CMAKE_C_FLAGS_MAINTAINER)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Maintainer")

set(UGREP_VERSION_MAJOR 0)
set(UGREP_VERSION_MINOR 5)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

include(CheckTypeSize)
check_type_size("void *" SIZEOF_VOIDP)
check_type_size("long" SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)

# set(ICU_DEBUG TRUE)
find_package(ICU 4.4 COMPONENTS i18n io REQUIRED)
find_package(ZLIB QUIET)
find_package(BZip2 QUIET)

# TODO:
# - slist.c only for FTS and ENGINES_SOURCES
# - set WITHOUT_FTS for binaries which don't use it

set(COMMON_BASE_SOURCES io/mmap.c io/stdio.c io/string.c io/reader.c struct/slist.c)
file(GLOB MISC_SOURCES ${CMAKE_SOURCE_DIR}/misc/*.c)
set(ENGINES_SOURCES engines/fixed.c engines/re.c struct/intervals.c struct/dptrarray.c)
list(APPEND COMMON_BASE_SOURCES ${MISC_SOURCES})
set(EXTRA_SOURCES "")
set(EXTRA_LIBS "")

option(NO_COLOR "color" OFF)
if(NO_COLOR)
    add_definitions(-DNO_COLOR=1)
endif(NO_COLOR)

if(${CMAKE_SYSTEM_NAME} MATCHES "BSD$")
    add_definitions(-DBSD=1)
    set(EXTRA_LIBS ${EXTRA_LIBS} "util")
endif(${CMAKE_SYSTEM_NAME} MATCHES "BSD$")

if(ZLIB_FOUND)
    list(APPEND COMMON_BASE_SOURCES io/zlib.c)
    include_directories(${ZLIB_INCLUDE_DIR})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${ZLIB_LIBRARIES})
    set(HAVE_ZLIB TRUE)
endif(ZLIB_FOUND)

if(BZIP2_FOUND)
    list(APPEND COMMON_BASE_SOURCES io/bzip2.c)
    include_directories(${BZIP2_INCLUDE_DIR})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${BZIP2_LIBRARIES})
    set(HAVE_BZIP2 TRUE)
endif(BZIP2_FOUND)

if(MSVC)
    file(GLOB WIN32_SPECIFIC_SOURCES ${CMAKE_SOURCE_DIR}/win32/*.c)
    list(APPEND COMMON_BASE_SOURCES ${WIN32_SPECIFIC_SOURCES} struct/hashtable.c)
    include_directories(${CMAKE_SOURCE_DIR}/win32/)
endif(MSVC)

configure_file(
    "config.h.in"
    "config.h"
)

message("==> CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
message("==> BZIP2 : ${BZIP2_FOUND}")
message("==> ICU   : ${ICU_VERSION}")
message("==> ZLIB  : ${ZLIB_VERSION_STRING}")

list(REMOVE_DUPLICATES EXTRA_SOURCES)

include_directories(
    ${ICU_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/misc/
    ${CMAKE_SOURCE_DIR}/struct/
    ${CMAKE_BINARY_DIR}
)

set(UgrepBinaries )
macro(declare_ugrep_binary _KEY)
    list(APPEND UgrepBinaries ${_KEY})
    set("UgrepBinaries_${_KEY}" ${ARGN})
endmacro(declare_ugrep_binary)

declare_ugrep_binary(ugrep bin/ugrep.c struct/fixed_circular_list.c ${COMMON_BASE_SOURCES} ${ENGINES_SOURCES})
declare_ugrep_binary(ucat bin/ucat.c ${COMMON_BASE_SOURCES})
declare_ugrep_binary(utr bin/utr.c struct/hashtable.c ${COMMON_BASE_SOURCES})
declare_ugrep_binary(usort bin/usort.c struct/rbtree.c ${COMMON_BASE_SOURCES})
declare_ugrep_binary(ucut bin/ucut.c ${COMMON_BASE_SOURCES} ${ENGINES_SOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Maintainer")
    declare_ugrep_binary(readuchars test/readuchars.c ${COMMON_BASE_SOURCES})
    declare_ugrep_binary(unescape test/ustring_unescape.c ${COMMON_BASE_SOURCES})
    declare_ugrep_binary(intervals test/intervals.c ${COMMON_BASE_SOURCES})
endif(CMAKE_BUILD_TYPE STREQUAL "Maintainer")

foreach (UgrepBinary ${UgrepBinaries})
    #message(STATUS "UgrepBinary = ${UgrepBinary}")
    #message(STATUS "UgrepBinaries_${UgrepBinary} = ${UgrepBinaries_${UgrepBinary}}")
    add_executable(${UgrepBinary} ${UgrepBinaries_${UgrepBinary}})
    target_link_libraries(${UgrepBinary} ${ICU_LIBRARIES} ${EXTRA_LIBS})
    install(TARGETS ${UgrepBinary} DESTINATION bin)
endforeach(UgrepBinary)

set_target_properties(utr usort ucut PROPERTIES COMPILE_FLAGS "-DWITHOUT_FTS")

#install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_INSTALL_PREFIX}/bin/ugrep\" \"${CMAKE_INSTALL_PREFIX}/bin/ufgrep\")
#list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/ufgrep\")
#")
#install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_INSTALL_PREFIX}/bin/ugrep\" \"${CMAKE_INSTALL_PREFIX}/bin/uegrep\")
#list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/uegrep\")
#")

add_custom_target(test COMMAND find ${CMAKE_SOURCE_DIR}/test -name '*.sh' -exec bash -i {} "\;")

# include(CTest)
# enable_testing()
# add_test(unescape ${CMAKE_CURRENT_BINARY_DIR}/unescape)

