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
# Copyright (c) 2011-2012, julp
#
# Distributed under the OSI-approved BSD License
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#=============================================================================

cmake_minimum_required(VERSION 2.8.3 FATAL_ERROR)

find_package(PkgConfig)

########## Private ##########
function(icudebug _varname)
    if(ICU_DEBUG)
        message("${_varname} = ${${_varname}}")
    endif(ICU_DEBUG)
endfunction(icudebug)

set(IcuRoot "")
if(DEFINED ENV{ICU_ROOT})
    set(IcuRoot "$ENV{ICU_ROOT}")
endif(DEFINED ENV{ICU_ROOT})
if (DEFINED ICU_ROOT)
    set(IcuRoot "${ICU_ROOT}")
endif(DEFINED ICU_ROOT)

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
    HINTS ${IcuRoot}
    PATH_SUFFIXES "include"
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
        HINTS ${IcuRoot}
        PATH_SUFFIXES "bin" "lib"
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
    #
    # From 4.9.1, ICU release version numbering was totaly changed, see:
    # - http://site.icu-project.org/download/49
    # - http://userguide.icu-project.org/design#TOC-Version-Numbers-in-ICU
    #
    if(ICU_MAJOR_VERSION LESS 49)
        string(REGEX REPLACE ".*# *define *U_ICU_VERSION_MINOR_NUM *([0-9]+).*" "\\1" ICU_MINOR_VERSION "${_icu_contents}")
        string(REGEX REPLACE ".*# *define *U_ICU_VERSION_PATCHLEVEL_NUM *([0-9]+).*" "\\1" ICU_PATCH_VERSION "${_icu_contents}")
    else(ICU_MAJOR_VERSION LESS 49)
        string(REGEX MATCH [0-9]$ ICU_MINOR_VERSION "${ICU_MAJOR_VERSION}")
        string(REGEX REPLACE [0-9]$ "" ICU_MAJOR_VERSION "${ICU_MAJOR_VERSION}")
        string(REGEX REPLACE ".*# *define *U_ICU_VERSION_MINOR_NUM *([0-9]+).*" "\\1" ICU_PATCH_VERSION "${_icu_contents}")
    endif(ICU_MAJOR_VERSION LESS 49)
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

# generate_icu_resource_bundle([PACKAGE <name>] [DESTINATION <destination>] [pattern or list of files])
function(generate_icu_resource_bundle)
    cmake_parse_arguments(ICU_RES "" "PACKAGE;DESTINATION" "" ${ARGN})

    icudebug("ICU_RES_DESTINATION")
    icudebug("ICU_RES_PACKAGE")
    icudebug("ICU_RES_UNPARSED_ARGUMENTS")

    # Variables:
    # - prefixes:
    #   + "_txt"   = input file (.txt)
    #   + "_res"   = output file (.res)
    #   + "_genrb" = genrb argument
    #   + "_pkg"   = ouput packaged file (.dat)
    # - suffixes:
    #   + "abs"  = absolute
    #   + "rel"  = relative
    #   + "dir"  = path part (dirname)
    #   + "base" = filename part (basename)

    if(ICU_RES_PACKAGE)
        get_filename_component(_pkg_name ${ICU_RES_PACKAGE} NAME_WE)
    endif(ICU_RES_PACKAGE)

    set(_all_res )
    foreach(_txt ${ICU_RES_UNPARSED_ARGUMENTS})
        get_filename_component(_locale ${_txt} NAME_WE)
        get_filename_component(_txt_base ${_txt} NAME)
        get_filename_component(_txt_abspath ${_txt} ABSOLUTE)
        file(RELATIVE_PATH _txt_relpath ${CMAKE_CURRENT_SOURCE_DIR} ${_txt_abspath})
        set(_res_base "${_locale}.res")
        # Default values
        set(_res_relpath "${_res_base}")
        set(_txt_dir ${CMAKE_CURRENT_SOURCE_DIR})

        if(_txt_relpath MATCHES "/")
            get_filename_component(_txt_dir ${_txt_relpath} PATH)
            set(_res_relpath "${_txt_dir}/${_res_base}")
        endif(_txt_relpath MATCHES "/")

        if(NOT ICU_RES_PACKAGE)
            file(MD5 ${_txt} _pkg_name) # find better?
        endif(NOT ICU_RES_PACKAGE)

        icudebug("_txt")
        icudebug("_txt_dir")
        icudebug("_locale")
        icudebug("_txt_abspath")
        icudebug("_txt_relpath")
        icudebug("_res_base")
        icudebug("_res_relpath")

        # cmake wait an absolute or relative path while genrb wait a filename only (basename), path(s) should be specified with its options -s and -d

        # 1) add_custom_command(OUTPUT <output file> DEPENDS <input file> [...])
        # 2) add_custom_target([...] DEPENDS <previous ouput file from add_custom_command>)

        add_custom_command(
            OUTPUT ${_res_relpath}
            COMMAND ${CMAKE_COMMAND} -E chdir ${_txt_dir} ${ICU_GENRB_EXECUTABLE} ${_txt_base}
            DEPENDS ${_txt}
        )
        add_custom_target(
            "rb_${_pkg_name}_${_locale}" ALL
            COMMENT "Generate or update ICU ResourceBundles for ${_locale}"
            DEPENDS ${_res_relpath}
        )

        if(ICU_RES_DESTINATION AND NOT ICU_RES_PACKAGE)
            install(FILES ${_res_relpath} DESTINATION ${ICU_RES_DESTINATION} PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
        endif(ICU_RES_DESTINATION AND NOT ICU_RES_PACKAGE)

        list(APPEND _all_res ${_res_relpath})
    endforeach(_txt)

    icudebug("_all_res")

    if(ICU_RES_PACKAGE)
        get_filename_component(_pkg_dir ${ICU_RES_PACKAGE} PATH)
        get_filename_component(_pkg_abspath ${ICU_RES_PACKAGE} ABSOLUTE)
        file(RELATIVE_PATH _pkg_relpath ${CMAKE_CURRENT_SOURCE_DIR} ${_pkg_abspath})
        set(_pkg_base "${_pkg_name}.dat")
        set(_pkg_dir ${CMAKE_CURRENT_SOURCE_DIR})
        if(_pkg_relpath MATCHES "/")
            get_filename_component(_pkg_dir ${_pkg_relpath} PATH)
            set(_pkgdata_args "-d${CMAKE_CURRENT_SOURCE_DIR}/${_pkg_dir}") # no space after -d else it will be escaped so becomes a space in path
        endif(_pkg_relpath MATCHES "/")
        set(_tmp "${_pkg_dir}/${_pkg_base}.txt")

        icudebug("_pkg_name")
        icudebug("_pkg_base")
        icudebug("_pkg_dir")
        icudebug("_pkg_abspath")
        icudebug("_pkg_relpath")
        icudebug("_tmp")
        icudebug("_pkgdata_args")

        file(WRITE ${_tmp} "")
        foreach(_res ${_all_res})
            file(APPEND ${_tmp} "${_res}\n")
        endforeach(_res)

        add_custom_command(
            OUTPUT "${_pkg_dir}/${_pkg_base}"
            COMMAND ${ICU_PKGDATA_EXECUTABLE} -v -F -s ${PROJECT_SOURCE_DIR} ${_pkgdata_args} -p ${_pkg_name} -T ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY} -m common ${_tmp}
            DEPENDS ${_all_res}
        )
        add_custom_target(
            "pkg_${_pkg_name}" ALL
            COMMENT "Generate or update all ICU ResourceBundles for ${_pkg_name}"
            DEPENDS "${_pkg_dir}/${_pkg_base}"
        )

        if(ICU_RES_DESTINATION)
            install(FILES "${_pkg_dir}/${_pkg_base}" DESTINATION ${ICU_RES_DESTINATION} PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
        endif(ICU_RES_DESTINATION)

    endif(ICU_RES_PACKAGE)

endfunction(generate_icu_resource_bundle)

############################################################

mark_as_advanced(
    ICU_INCLUDE_DIRS
    ICU_LIBRARIES
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
# Binaries
icudebug("ICU_GENRB_EXECUTABLE")
icudebug("ICU_PKGDATA_EXECUTABLE")
# Version
icudebug("ICU_MAJOR_VERSION")
icudebug("ICU_MINOR_VERSION")
icudebug("ICU_PATCH_VERSION")
icudebug("ICU_VERSION")
