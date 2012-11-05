cmake_minimum_required(VERSION 2.8.8)

project(ugrep)

if(MSVC)
    # http://msdn.microsoft.com/en-us/library/19z1t1wy%28v=VS.80%29.aspx
    # /GS /Wall
    set(CMAKE_C_FLAGS_MAINTAINER "/MD /D DEBUG /Od" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
    add_definitions(-DREPLACE_GETOPT)
else(MSVC)
    # removed flags: -O -pendantic
    set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -Wall -Wextra -std=gnu99 -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
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

include(CheckIncludeFile)
check_include_file(dlfcn.h HAVE_DLFCN_H)

include(CheckLibraryExists)
check_library_exists("dl" "dlopen" "lib" HAVE_LIBDL)

# set(ICU_DEBUG TRUE)
find_package(ICU 4.4 COMPONENTS i18n io REQUIRED)
find_package(ZLIB QUIET)
find_package(BZip2 QUIET)

# TODO:
# - slist.c only for FTS and ENGINES_SOURCES

set(FTS_BASE_SOURCES )
set(COMMON_BASE_SOURCES io/mmap.c io/stdio.c io/string.c io/reader.c struct/slist.c)
#file(GLOB MISC_SOURCES ${CMAKE_SOURCE_DIR}/misc/*.c)
#list(APPEND COMMON_BASE_SOURCES ${MISC_SOURCES})
list(APPEND COMMON_BASE_SOURCES misc/alloc.c misc/env.c misc/error.c misc/ustring.c)
set(ENGINES_SOURCES engines/fixed.c engines/re.c engines/bin.c struct/intervals.c struct/darray.c)
set(EXTRA_SOURCES "")
set(EXTRA_LIBS "")

option(NO_COLOR "disable color support" OFF)
option(DYNAMIC_READER "enable dynamic loading for readers which depend on a library" OFF)

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

if(DYNAMIC_READER OR (NOT ZLIB_FOUND AND NOT BZIP2_FOUND))
    if(HAVE_LIBDL)
        set(EXTRA_LIBS ${EXTRA_LIBS} "dl")
    endif(HAVE_LIBDL)
    add_definitions(-DDYNAMIC_READERS=1)
    list(APPEND COMMON_BASE_SOURCES io/zlib.c io/bzip2.c)
    include_directories(${CMAKE_SOURCE_DIR}/missing/)
endif()

if(MSVC)
#     file(GLOB WIN32_SPECIFIC_SOURCES ${CMAKE_SOURCE_DIR}/win32/*.c)
#     list(APPEND COMMON_BASE_SOURCES ${WIN32_SPECIFIC_SOURCES} struct/hashtable.c)
    list(APPEND COMMON_BASE_SOURCES win32/getopt_long.c)
    list(APPEND FTS_BASE_SOURCES win32/dirent.c win32/err.c win32/fnmatch.c win32/fts.c struct/hashtable.c)
    include_directories(${CMAKE_SOURCE_DIR}/win32/)
endif(MSVC)

configure_file(
    "config.h.in"
    "config.h"
    @ONLY
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

macro(declare_ugrep_binary _KEY)
    cmake_parse_arguments(UGREP_BINARY "INSTALL" "COMPILE_DEFINITIONS" "SOURCES;OBJECTS" ${ARGN})

    foreach(object ${UGREP_BINARY_OBJECTS})
        list(APPEND UGREP_BINARY_SOURCES "\$<TARGET_OBJECTS:${object}>")
    endforeach(object)

    add_executable(${_KEY} ${UGREP_BINARY_SOURCES})
    target_link_libraries(${_KEY} ${ICU_LIBRARIES} ${EXTRA_LIBS})

    if(UGREP_BINARY_COMPILE_DEFINITIONS)
        set_target_properties(${_KEY} PROPERTIES COMPILE_DEFINITIONS ${UGREP_BINARY_COMPILE_DEFINITIONS})
    endif(UGREP_BINARY_COMPILE_DEFINITIONS)

    if(UGREP_BINARY_INSTALL)
        install(TARGETS ${_KEY} DESTINATION bin)
    endif(UGREP_BINARY_INSTALL)
endmacro(declare_ugrep_binary)

# Cf http://www.cmake.org/Wiki/CMake/Tutorials/Object_Library
add_library(COMMON OBJECT EXCLUDE_FROM_ALL ${COMMON_BASE_SOURCES})

add_library(NONFTS_BASE OBJECT EXCLUDE_FROM_ALL misc/util.c)
set_target_properties(NONFTS_BASE PROPERTIES COMPILE_DEFINITIONS WITHOUT_FTS)

get_target_property(NONFTS_BASE_SOURCES NONFTS_BASE SOURCES)
add_library(FTS_BASE OBJECT EXCLUDE_FROM_ALL ${NONFTS_BASE_SOURCES} ${FTS_BASE_SOURCES})
set_target_properties(FTS_BASE PROPERTIES COMPILE_DEFINITIONS WITH_FTS)

add_library(ENGINES OBJECT EXCLUDE_FROM_ALL ${ENGINES_SOURCES})

declare_ugrep_binary(
    ugrep
    INSTALL TRUE
    SOURCES bin/ugrep.c struct/fixed_circular_list.c
    OBJECTS COMMON FTS_BASE ENGINES
    COMPILE_DEFINITIONS WITH_FTS
)
declare_ugrep_binary(
    ucat
    INSTALL TRUE
    SOURCES bin/ucat.c
    OBJECTS COMMON FTS_BASE
    COMPILE_DEFINITIONS WITH_FTS
)
declare_ugrep_binary(
    utr
    INSTALL TRUE
    SOURCES bin/utr.c struct/hashtable.c
    OBJECTS COMMON NONFTS_BASE
)
declare_ugrep_binary(
    usort
    INSTALL TRUE
    SOURCES bin/usort.c
    struct/rbtree.c
    OBJECTS COMMON NONFTS_BASE
)
declare_ugrep_binary(
    ucut
    INSTALL TRUE
    SOURCES bin/ucut.c
    OBJECTS COMMON NONFTS_BASE ENGINES
)

if(CMAKE_BUILD_TYPE STREQUAL "Maintainer")
    declare_ugrep_binary(
        readuchars
        SOURCES test/readuchars.c
        OBJECTS COMMON NONFTS_BASE
    )
    declare_ugrep_binary(
        unescape
        SOURCES test/ustring_unescape.c
        OBJECTS COMMON NONFTS_BASE
    )
    declare_ugrep_binary(
        intervals
        SOURCES test/intervals.c
        OBJECTS COMMON NONFTS_BASE
    )
endif(CMAKE_BUILD_TYPE STREQUAL "Maintainer")

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

