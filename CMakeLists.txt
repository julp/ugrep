cmake_minimum_required(VERSION 2.6)

set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -Wall -Wextra -std=gnu99 -pedantic -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull" CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
mark_as_advanced(CMAKE_C_FLAGS_MAINTAINER)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Maintainer")

project(ugrep)

set(UGREP_VERSION_MAJOR 0)
set(UGREP_VERSION_MINOR 3)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(ICU REQUIRED)
find_package(ZLIB)
find_package(BZip2)

set(BASE_SOURCES ugrep.c alloc.c ustring.c io/mmio.c io/stdio.c slist.c engines/fixedengine.c engines/icureengine.c engines/icusearchengine.c intervals.c error.c fixed_circular_list.c)
set(EXTRA_SOURCES)
set(EXTRA_LIBS)

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

configure_file(
    "config.h.in"
    "config.h"
)

message(STATUS CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE})
message(STATUS ZLIB_FOUND = ${ZLIB_FOUND})
message(STATUS BZIP2_FOUND = ${BZIP2_FOUND})

list(REMOVE_DUPLICATES EXTRA_SOURCES)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

add_executable(ugrep ${BASE_SOURCES} ${EXTRA_SOURCES})
target_link_libraries(ugrep icudata icui18n icuio icule iculx icutu ${EXTRA_LIBS})

install(TARGETS ugrep DESTINATION bin)
install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_INSTALL_PREFIX}/bin/ugrep\" \"${CMAKE_INSTALL_PREFIX}/bin/ufgrep\")
list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/ufgrep\")
")
install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_INSTALL_PREFIX}/bin/ugrep\" \"${CMAKE_INSTALL_PREFIX}/bin/uegrep\")
list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/uegrep\")
")

add_custom_target(test COMMAND find ${CMAKE_SOURCE_DIR}/test -name '*.sh' -exec bash {} "\;")
