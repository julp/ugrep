# This module can find the International Components for Unicode (ICU) Library
#
# Requirements:
# - CMake >= 2.8.3
#
# The following variables will be defined for your use:
#   - ICU_FOUND             : were all of your specified components found (include dependencies)?
#   - ICU_INCLUDE_DIRS      : ICU include directory
#   - ICU_LIBRARIES         : ICU libraries
#   - ICU_VERSION           : complete version of ICU (x.y.z)
#   - ICU_MAJOR_VERSION     : major version of ICU
#   - ICU_MINOR_VERSION     : minor version of ICU
#   - ICU_PATCH_VERSION     : patch version of ICU
#   - ICU_<COMPONENT>_FOUND : were <COMPONENT> found? (FALSE for non specified component if it is not a dependency)
#
# Example Usage:
#
#   1. Copy this file in the root of your project source directory
#   2. Then, tell CMake to search this non-standard module in your project directory by adding to your CMakeLists.txt:
#     set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
#   3. Finally call find_package() once, here are some examples to pick from
#
#   Require ICU 4.4 or later
#     find_package(ICU 4.4 REQUIRED)
#
#   if(ICU_FOUND)
#      include_directories(${ICU_INCLUDE_DIRS})
#      add_executable(myapp myapp.c)
#      target_link_libraries(myapp ${ICU_LIBRARIES})
#   endif()

#=============================================================================
# Copyright (c) 2011, julp
#
# Distributed under the OSI-approved BSD License
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#=============================================================================

find_package(PkgConfig)

########## Private ##########
function(icudebug _varname)
    if(ICU_DEBUG)
        message("${_varname} = ${${_varname}}")
    endif(ICU_DEBUG)
endfunction(icudebug)

set(IcuComponents )
# <icu component name> <library name 1> ... <library name N>
macro(declare_icu_component _NAME)
    list(APPEND IcuComponents ${_NAME})
    set("IcuComponents_${_NAME}" ${ARGN})
endmacro(declare_icu_component)

declare_icu_component(data icudata)
declare_icu_component(uc   icuuc)         # Common and Data libraries
declare_icu_component(i18n icui18n icuin) # Internationalization library
declare_icu_component(io   icuio)         # Stream and I/O Library
declare_icu_component(le   icule)         # Layout library
declare_icu_component(lx   iculx)         # Paragraph Layout library

########## Public ##########
set(ICU_FOUND TRUE)
set(ICU_LIBRARIES )
set(ICU_INCLUDE_DIRS )
set(ICU_DEFINITIONS )
foreach(_icu_component ${IcuComponents})
    string(TOUPPER "${_icu_component}" _icu_upper_component)
    set("ICU_${_icu_upper_component}_FOUND" FALSE) # may be done in the declare_icu_component macro
endforeach(_icu_component)

# Check components
if(NOT ICU_FIND_COMPONENTS) # uc required at least
    set(ICU_FIND_COMPONENTS uc)
else()
    list(APPEND ICU_FIND_COMPONENTS uc)
    list(REMOVE_DUPLICATES ICU_FIND_COMPONENTS)
    foreach(_icu_component ${ICU_FIND_COMPONENTS})
        if(NOT DEFINED "IcuComponents_${_icu_component}")
            message(FATAL_ERROR "Unknown ICU component: ${_icu_component}")
        endif()
    endforeach(_icu_component)
endif()

# Includes
find_path(
    ICU_INCLUDE_DIRS
    NAMES unicode/utypes.h
    DOC "Include directories for ICU"
)

# Check dependencies
if(PKG_CONFIG_FOUND)
    set(_components_dup ${ICU_FIND_COMPONENTS})
    foreach(_icu_component ${_components_dup})
        pkg_check_modules(PC_ICU "icu-${_icu_component}" QUIET)

        if(PC_ICU_FOUND)
            foreach(_pc_icu_library ${PC_ICU_LIBRARIES})
                string(REGEX REPLACE "^icu" "" _pc_stripped_icu_library ${_pc_icu_library})
                list(APPEND ICU_FIND_COMPONENTS ${_pc_stripped_icu_library})
            endforeach(_pc_icu_library)
        endif(PC_ICU_FOUND)
    endforeach(_icu_component)
    list(REMOVE_DUPLICATES ICU_FIND_COMPONENTS)
endif(PKG_CONFIG_FOUND)

# Check libraries
foreach(_icu_component ${ICU_FIND_COMPONENTS})
    find_library(
        _icu_lib
        NAMES ${IcuComponents_${_icu_component}}
        DOC "Libraries for ICU"
    )

    string(TOUPPER "${_icu_component}" _icu_upper_component)
    if(_icu_lib-NOTFOUND)
        set("ICU_${_icu_upper_component}_FOUND" FALSE)
        set("ICU_FOUND" FALSE)
    else(_icu_lib-NOTFOUND)
        set("ICU_${_icu_upper_component}_FOUND" TRUE)
    endif(_icu_lib-NOTFOUND)

    list(APPEND ICU_LIBRARIES ${_icu_lib})

    set(_icu_lib _icu_lib-NOTFOUND) # Workaround
endforeach(_icu_component)

list(REMOVE_DUPLICATES ICU_LIBRARIES)

if(ICU_FOUND)
    if(EXISTS "${ICU_INCLUDE_DIRS}/unicode/uvernum.h")
        file(READ "${ICU_INCLUDE_DIRS}/unicode/uvernum.h" _icu_contents)
#     else()
#         todo
    endif()

    string(REGEX REPLACE ".*# *define *U_ICU_VERSION_MAJOR_NUM *([0-9]+).*" "\\1" ICU_MAJOR_VERSION "${_icu_contents}")
    string(REGEX REPLACE ".*# *define *U_ICU_VERSION_MINOR_NUM *([0-9]+).*" "\\1" ICU_MINOR_VERSION "${_icu_contents}")
    string(REGEX REPLACE ".*# *define *U_ICU_VERSION_PATCHLEVEL_NUM *([0-9]+).*" "\\1" ICU_PATCH_VERSION "${_icu_contents}")
    set(ICU_VERSION "${ICU_MAJOR_VERSION}.${ICU_MINOR_VERSION}.${ICU_PATCH_VERSION}")
endif(ICU_FOUND)

if(ICU_INCLUDE_DIRS)
    include(FindPackageHandleStandardArgs)
    if(ICU_FIND_REQUIRED AND NOT ICU_FIND_QUIETLY)
        find_package_handle_standard_args(ICU REQUIRED_VARS ICU_LIBRARIES ICU_INCLUDE_DIRS VERSION_VAR ICU_VERSION)
    else()
        find_package_handle_standard_args(ICU "ICU not found" ICU_LIBRARIES ICU_INCLUDE_DIRS)
    endif()
else(ICU_INCLUDE_DIRS)
    if(ICU_FIND_REQUIRED AND NOT ICU_FIND_QUIETLY)
        message(FATAL_ERROR "Could not find ICU include directory")
    endif()
endif(ICU_INCLUDE_DIRS)

############################################################

find_program(ICU_GENRB_EXECUTABLE genrb)
find_program(ICU_PKGDATA_EXECUTABLE pkgdata)

if(NOT ICU_GENRB_EXECUTABLE)
    message(FATAL_ERROR "genrb not found")
endif(NOT ICU_GENRB_EXECUTABLE)
if(NOT ICU_PKGDATA_EXECUTABLE)
    message(FATAL_ERROR "pkgdata not found")
endif(NOT ICU_PKGDATA_EXECUTABLE)

# genrb([PACKAGE <name>] [pattern or list of files])
function(genrb _arg1)
    set(IcuRb )
    if (${_arg1} STREQUAL "PACKAGE")
        #set(_pkg_name ${ARGV1})
        get_filename_component(_pkg_name ${ARGV1} NAME_WE)
        get_filename_component(_pkg_dir ${ARGV1} PATH)
# message("name = ${_pkg_name}; dir = ${_pkg_dir} ; ")
        set(_list_index 2)
    else(${_arg1} STREQUAL "PACKAGE")
        set(_list_index 0)
    endif(${_arg1} STREQUAL "PACKAGE")
    list(GET ARGV ${_list_index} -1 _sources)
    foreach(_source ${_sources})
        get_filename_component(_outputwe ${_source} NAME_WE)
        set(_output "${_outputwe}.res")
        add_custom_command(
            OUTPUT ${_output}
            COMMAND ${ICU_GENRB_EXECUTABLE} ${_source}
            DEPENDS ${_source}
        )
        add_custom_target(
            rb-${_outputwe}
            COMMENT "Generate/Update ICU ResourceBundles for ${_outputwe}"
            DEPENDS ${_source}
        )
        list(APPEND IcuRb ${_output})
# message(":${_source}: => :${_output}:")
#         if(NOT _pkg_name)
#             install(FILES ${output} DESTINATION ???)
#         endif(NOT _pkg_name)
    endforeach(_source)
    if(_pkg_name)
        set(_pkg_list_file "${_pkg_name}.dat.txt")
        add_custom_target(
            rb ALL
            COMMENT "Generate/Update ICU ResourceBundles"
            DEPENDS ${IcuRb}
        )
        file(WRITE ${_pkg_list_file} "")
        foreach(_rb ${IcuRb})
            file(APPEND ${_pkg_list_file} "${_rb}\n")
        endforeach(_rb)
        add_custom_command(
            OUTPUT "${_pkg_name}.dat" # ${_pkg_dir}
            COMMAND ${ICU_PKGDATA_EXECUTABLE} -F -p ${_pkg_name} -T ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY} -m common ${_pkg_list_file} # ${_pkg_dir}
            DEPENDS ${IcuRb}
        )
        add_custom_target(
            pkg ALL
            COMMENT "Packaging data"
            DEPENDS "${_pkg_name}.dat" # ${_pkg_dir}
        )
#     install(FILES ${output} DESTINATION ???)
    endif(_pkg_name)
endfunction(genrb)

icudebug("ICU_GENRB_EXECUTABLE")
icudebug("ICU_PKGDATA_EXECUTABLE")

############################################################

mark_as_advanced(
    ICU_INCLUDE_DIRS
    ICU_LIBRARIES
    ICU_DEFINITIONS
    ICU_VERSION
    ICU_MAJOR_VERSION
    ICU_MINOR_VERSION
    ICU_PATCH_VERSION
)

# IN (args)
icudebug("ICU_FIND_COMPONENTS")
icudebug("ICU_FIND_REQUIRED")
icudebug("ICU_FIND_QUIETLY")
icudebug("ICU_FIND_VERSION")
# OUT
# Found
icudebug("ICU_FOUND")
icudebug("ICU_UC_FOUND")
icudebug("ICU_I18N_FOUND")
icudebug("ICU_IO_FOUND")
icudebug("ICU_LE_FOUND")
icudebug("ICU_LX_FOUND")
# Linking
icudebug("ICU_INCLUDE_DIRS")
icudebug("ICU_LIBRARIES")
# Version
icudebug("ICU_MAJOR_VERSION")
icudebug("ICU_MINOR_VERSION")
icudebug("ICU_PATCH_VERSION")
icudebug("ICU_VERSION")