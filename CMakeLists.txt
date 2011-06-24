cmake_minimum_required(VERSION 2.6)

project(ugrep)

if(MSVC)
    # http://msdn.microsoft.com/en-us/library/19z1t1wy%28v=VS.80%29.aspx
    # /GS /Wall
    set(CMAKE_C_FLAGS_MAINTAINER "/D REPLACE_GETOPT /D DEBUG /Od" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
else(MSVC)
    set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -Wall -Wextra -std=gnu99 -pedantic -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
endif(MSVC)
mark_as_advanced(CMAKE_C_FLAGS_MAINTAINER)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Maintainer")

set(UGREP_VERSION_MAJOR 0)
set(UGREP_VERSION_MINOR 4)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(ICU REQUIRED)
find_package(ZLIB QUIET)
find_package(BZip2 QUIET)

set(COMMON_BASE_SOURCES alloc.c ustring.c io/mmap.c io/stdio.c io/string.c error.c reader.c util.c)
set(EXTRA_SOURCES "")
set(EXTRA_LIBS "")

option(NO_COLOR "color" OFF)
if(NO_COLOR)
    add_definitions(-DNO_COLOR=1)
endif(NO_COLOR)

# option(DEBUG "debug" OFF)
# if(DEBUG)
#     foreach(cflag -Wall -Wextra -std=gnu99 -pedantic -Wwrite-strings -Wstrict-prototypes -Wunreachable-code -Wno-comment -Wnonnull)
#         check_c_compiler_flag(${cflag} SUPPORT_C_FLAG)
#         if(SUPPORT_C_FLAG)
#             set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${cflag}")
#         endif(SUPPORT_C_FLAG)
#         unset(SUPPORT_C_FLAG)
#     endforeach(cflag)
#     set(CMAKE_REQUIRED_FLAGS ${CMAKE_C_FLAGS})
#     add_definitions(${CMAKE_C_FLAGS})
#
#     add_definitions(-g)
# else(DEBUG)
#     add_definitions(-O2 -Wuninitialized)
# endif(DEBUG)
#
# message(STATUS DEBUG = ${DEBUG})

if(ZLIB_FOUND)
    set(EXTRA_SOURCES ${EXTRA_SOURCES} io/uncompressio.c)
    include_directories(${ZLIB_INCLUDE_DIR})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${ZLIB_LIBRARIES})
    set(HAVE_ZLIB TRUE)
endif(ZLIB_FOUND)

if(BZIP2_FOUND)
    set(EXTRA_SOURCES ${EXTRA_SOURCES} io/uncompressio.c)
    include_directories(${BZIP2_INCLUDE_DIR})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${BZIP2_LIBRARIES})
    set(HAVE_BZIP2 TRUE)
endif(BZIP2_FOUND)

if(MSVC)
    file(GLOB EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/win32/*.c)
    include_directories(${CMAKE_SOURCE_DIR}/win32)
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

include_directories(${ICU_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(UgrepBinaries )
macro(declare_ugrep_binary _KEY)
    list(APPEND UgrepBinaries ${_KEY})
    set("UgrepBinaries_${_KEY}" ${ARGN})
endmacro(declare_ugrep_binary)

declare_ugrep_binary(ugrep ugrep.c slist.c engines/fixed.c engines/re.c intervals.c fixed_circular_list.c ${EXTRA_SOURCES} ${COMMON_BASE_SOURCES})
declare_ugrep_binary(ucat ucat.c ${EXTRA_SOURCES} ${COMMON_BASE_SOURCES})
declare_ugrep_binary(utr utr.c ${EXTRA_SOURCES} ${COMMON_BASE_SOURCES})
declare_ugrep_binary(usort usort.c rbtree.c ${EXTRA_SOURCES} ${COMMON_BASE_SOURCES})
declare_ugrep_binary(ucut ucut.c ${EXTRA_SOURCES} ${COMMON_BASE_SOURCES})

foreach (UgrepBinary ${UgrepBinaries})
    #message(STATUS "UgrepBinary = ${UgrepBinary}")
    #message(STATUS "UgrepBinaries_${UgrepBinary} = ${UgrepBinaries_${UgrepBinary}}")
    add_executable(${UgrepBinary} ${UgrepBinaries_${UgrepBinary}})
    target_link_libraries(${UgrepBinary} ${ICU_LIBRARIES} ${ICU_I18N_LIBRARIES} ${EXTRA_LIBS} icuio)
    install(TARGETS ${UgrepBinary} DESTINATION bin)
endforeach(UgrepBinary)

#install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_INSTALL_PREFIX}/bin/ugrep\" \"${CMAKE_INSTALL_PREFIX}/bin/ufgrep\")
#list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/ufgrep\")
#")
#install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_INSTALL_PREFIX}/bin/ugrep\" \"${CMAKE_INSTALL_PREFIX}/bin/uegrep\")
#list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/uegrep\")
#")

add_custom_target(test COMMAND find ${CMAKE_SOURCE_DIR}/test -name '*.sh' -exec bash {} "\;")
