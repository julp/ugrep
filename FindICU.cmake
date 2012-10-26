# This module can find the International Components for Unicode (ICU) Library
#
# Requirements:
# - CMake >= 2.8.3 (for new version of find_package_handle_standard_args)
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
# For windows or non standard installation, define ICU_ROOT variable to point to the root installation of ICU. Two ways:
#   - run cmake with -DICU_ROOT=<PATH>
#   - define an environment variable with the same name before running cmake
# With cmake-gui, before pressing "Configure":
#   1) Press "Add Entry" button
#   2) Add a new entry defined as:
#     - Name: ICU_ROOT
#     - Type: choose PATH in the selection list
#     - Press "..." button and select the root installation of ICU
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

find_package(PkgConfig QUIET)

########## Private ##########
set(ICU_PUBLIC_VAR_NS "ICU")                          # Prefix for all ICU relative public variables
set(ICU_PRIVATE_VAR_NS "_${ICU_PUBLIC_VAR_NS}")       # Prefix for all ICU relative internal variables
set(PC_ICU_PRIVATE_VAR_NS "_PC${ICU_PRIVATE_VAR_NS}") # Prefix for all pkg-config relative internal variables

function(icudebug _VARNAME)
    if(${ICU_PUBLIC_VAR_NS}_DEBUG)
        if(DEFINED ${ICU_PUBLIC_VAR_NS}_${_VARNAME})
            message("${ICU_PUBLIC_VAR_NS}_${_VARNAME} = ${${ICU_PUBLIC_VAR_NS}_${_VARNAME}}")
        else(DEFINED ${ICU_PUBLIC_VAR_NS}_${_VARNAME})
            message("${ICU_PUBLIC_VAR_NS}_${_VARNAME} = <UNDEFINED>")
        endif(DEFINED ${ICU_PUBLIC_VAR_NS}_${_VARNAME})
    endif(${ICU_PUBLIC_VAR_NS}_DEBUG)
endfunction(icudebug)

set(${ICU_PRIVATE_VAR_NS}_ROOT "")
if(DEFINED ENV{ICU_ROOT})
    set(${ICU_PRIVATE_VAR_NS}_ROOT "$ENV{ICU_ROOT}")
endif(DEFINED ENV{ICU_ROOT})
if (DEFINED ICU_ROOT)
    set(${ICU_PRIVATE_VAR_NS}_ROOT "${ICU_ROOT}")
endif(DEFINED ICU_ROOT)

set(${ICU_PRIVATE_VAR_NS}_BIN_SUFFIXES )
set(${ICU_PRIVATE_VAR_NS}_LIB_SUFFIXES )
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND ${ICU_PRIVATE_VAR_NS}_BIN_SUFFIXES "bin64")
    list(APPEND ${ICU_PRIVATE_VAR_NS}_LIB_SUFFIXES "lib64")
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)
list(APPEND ${ICU_PRIVATE_VAR_NS}_BIN_SUFFIXES "bin")
list(APPEND ${ICU_PRIVATE_VAR_NS}_LIB_SUFFIXES "lib")

set(${ICU_PRIVATE_VAR_NS}_COMPONENTS )
# <icu component name> <library name 1> ... <library name N>
macro(icu_declare_component _NAME)
    list(APPEND ${ICU_PRIVATE_VAR_NS}_COMPONENTS ${_NAME})
    set("${ICU_PRIVATE_VAR_NS}_COMPONENTS_${_NAME}" ${ARGN})
endmacro(icu_declare_component)

icu_declare_component(data icudata)
icu_declare_component(uc   icuuc)         # Common and Data libraries
icu_declare_component(i18n icui18n icuin) # Internationalization library
icu_declare_component(io   icuio ustdio)  # Stream and I/O Library
icu_declare_component(le   icule)         # Layout library
icu_declare_component(lx   iculx)         # Paragraph Layout library

########## Public ##########
set(${ICU_PUBLIC_VAR_NS}_FOUND TRUE)
set(${ICU_PUBLIC_VAR_NS}_LIBRARIES )
set(${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS )
set(${ICU_PUBLIC_VAR_NS}_DEFINITIONS )
foreach(${ICU_PRIVATE_VAR_NS}_COMPONENT ${${ICU_PRIVATE_VAR_NS}_COMPONENTS})
    string(TOUPPER "${${ICU_PRIVATE_VAR_NS}_COMPONENT}" ${ICU_PRIVATE_VAR_NS}_UPPER_COMPONENT)
    set("${ICU_PUBLIC_VAR_NS}_${${ICU_PRIVATE_VAR_NS}_UPPER_COMPONENT}_FOUND" FALSE) # may be done in the icu_declare_component macro
endforeach(${ICU_PRIVATE_VAR_NS}_COMPONENT)

# Check components
if(NOT ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS) # uc required at least
    set(${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS uc)
else(NOT ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS)
    list(APPEND ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS uc)
    list(REMOVE_DUPLICATES ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS)
    foreach(${ICU_PRIVATE_VAR_NS}_COMPONENT ${${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS})
        if(NOT DEFINED ${ICU_PRIVATE_VAR_NS}_COMPONENTS_${${ICU_PRIVATE_VAR_NS}_COMPONENT})
            message(FATAL_ERROR "Unknown ICU component: ${${ICU_PRIVATE_VAR_NS}_COMPONENT}")
        endif(NOT DEFINED ${ICU_PRIVATE_VAR_NS}_COMPONENTS_${${ICU_PRIVATE_VAR_NS}_COMPONENT})
    endforeach(${ICU_PRIVATE_VAR_NS}_COMPONENT)
endif(NOT ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS)

# Includes
find_path(
    ${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS
    NAMES unicode/utypes.h utypes.h
    HINTS ${${ICU_PRIVATE_VAR_NS}_ROOT}
    PATH_SUFFIXES "include"
    DOC "Include directories for ICU"
)

if(${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS)
    ########## <part to keep synced with tests/version/CMakeLists.txt> ##########
    if(EXISTS "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/unicode/uvernum.h") # ICU >= 4
        file(READ "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/unicode/uvernum.h" ${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS)
    elseif(EXISTS "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/unicode/uversion.h") # ICU [2;4[
        file(READ "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/unicode/uversion.h" ${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS)
    elseif(EXISTS "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/unicode/utypes.h") # ICU [1.4;2[
        file(READ "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/unicode/utypes.h" ${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS)
    elseif(EXISTS "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/utypes.h") # ICU 1.3
        file(READ "${${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS}/utypes.h" ${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS)
    else()
        message(FATAL_ERROR "ICU version header not found")
    endif()

    if(${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS MATCHES ".*# *define *ICU_VERSION *\"([0-9]+)\".*") # ICU 1.3
        # [1.3;1.4[ as #define ICU_VERSION "3" (no patch version, ie all 1.3.X versions will be detected as 1.3.0)
        set(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION "1")
        set(${ICU_PUBLIC_VAR_NS}_MINOR_VERSION "${CMAKE_MATCH_1}")
        set(${ICU_PUBLIC_VAR_NS}_PATCH_VERSION "0")
    elseif(${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS MATCHES ".*# *define *U_ICU_VERSION_MAJOR_NUM *([0-9]+).*")
        set(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION "${CMAKE_MATCH_1}")
        #
        # Since version 4.9.1, ICU release version numbering was totaly changed, see:
        # - http://site.icu-project.org/download/49
        # - http://userguide.icu-project.org/design#TOC-Version-Numbers-in-ICU
        #
        if(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION LESS 49)
            string(REGEX REPLACE ".*# *define *U_ICU_VERSION_MINOR_NUM *([0-9]+).*" "\\1" ${ICU_PUBLIC_VAR_NS}_MINOR_VERSION "${${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS}")
            string(REGEX REPLACE ".*# *define *U_ICU_VERSION_PATCHLEVEL_NUM *([0-9]+).*" "\\1" ${ICU_PUBLIC_VAR_NS}_PATCH_VERSION "${${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS}")
        else(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION LESS 49)
            string(REGEX MATCH [0-9]$ ${ICU_PUBLIC_VAR_NS}_MINOR_VERSION "${${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION}")
            string(REGEX REPLACE [0-9]$ "" ${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION "${${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION}")
            string(REGEX REPLACE ".*# *define *U_ICU_VERSION_MINOR_NUM *([0-9]+).*" "\\1" ${ICU_PUBLIC_VAR_NS}_PATCH_VERSION "${${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS}")
        endif(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION LESS 49)
    elseif(${ICU_PRIVATE_VAR_NS}_VERSION_HEADER_CONTENTS MATCHES ".*# *define *U_ICU_VERSION *\"(([0-9]+)(\\.[0-9]+)*)\".*") # ICU [1.4;1.8[
        # [1.4;1.8[ as #define U_ICU_VERSION "1.4.1.2" but it seems that some 1.4.1(?:\.\d)? have releasing error and appears as 1.4.0
        set(${ICU_PRIVATE_VAR_NS}_FULL_VERSION "${CMAKE_MATCH_1}") # copy CMAKE_MATCH_1, no longer valid on the following if
        if(${ICU_PRIVATE_VAR_NS}_FULL_VERSION MATCHES "^([0-9]+)\\.([0-9]+)$")
            set(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION "${CMAKE_MATCH_1}")
            set(${ICU_PUBLIC_VAR_NS}_MINOR_VERSION "${CMAKE_MATCH_2}")
            set(${ICU_PUBLIC_VAR_NS}_PATCH_VERSION "0")
        elseif(${ICU_PRIVATE_VAR_NS}_FULL_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
            set(${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION "${CMAKE_MATCH_1}")
            set(${ICU_PUBLIC_VAR_NS}_MINOR_VERSION "${CMAKE_MATCH_2}")
            set(${ICU_PUBLIC_VAR_NS}_PATCH_VERSION "${CMAKE_MATCH_3}")
        endif()
    else()
        message(FATAL_ERROR "failed to detect ICU version")
    endif()
    set(${ICU_PUBLIC_VAR_NS}_VERSION "${${ICU_PUBLIC_VAR_NS}_MAJOR_VERSION}.${${ICU_PUBLIC_VAR_NS}_MINOR_VERSION}.${${ICU_PUBLIC_VAR_NS}_PATCH_VERSION}")
    ########## </part to keep synced with tests/version/CMakeLists.txt> ##########

    # Check dependencies (implies pkg-config)
    if(PKG_CONFIG_FOUND)
        set(${ICU_PRIVATE_VAR_NS}_COMPONENTS_DUP ${${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS})
        foreach(${ICU_PRIVATE_VAR_NS}_COMPONENT ${${ICU_PRIVATE_VAR_NS}_COMPONENTS_DUP})
            pkg_check_modules(PC_ICU_PRIVATE_VAR_NS "icu-${${ICU_PRIVATE_VAR_NS}_COMPONENT}" QUIET)

            if(${PC_ICU_PRIVATE_VAR_NS}_FOUND)
                foreach(${PC_ICU_PRIVATE_VAR_NS}_LIBRARY ${PC_ICU_LIBRARIES})
                    string(REGEX REPLACE "^icu" "" ${PC_ICU_PRIVATE_VAR_NS}_STRIPPED_LIBRARY ${${PC_ICU_PRIVATE_VAR_NS}_LIBRARY})
                    list(APPEND ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS ${${PC_ICU_PRIVATE_VAR_NS}_STRIPPED_LIBRARY})
                endforeach(${PC_ICU_PRIVATE_VAR_NS}_LIBRARY)
            endif(${PC_ICU_PRIVATE_VAR_NS}_FOUND)
        endforeach(${ICU_PRIVATE_VAR_NS}_COMPONENT)
        list(REMOVE_DUPLICATES ${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS)
    endif(PKG_CONFIG_FOUND)

    # Check libraries
    foreach(${ICU_PRIVATE_VAR_NS}_COMPONENT ${${ICU_PUBLIC_VAR_NS}_FIND_COMPONENTS})
        set(${ICU_PRIVATE_VAR_NS}_POSSIBLE_RELEASE_NAMES )
        set(${ICU_PRIVATE_VAR_NS}_POSSIBLE_DEBUG_NAMES )
        foreach(${ICU_PRIVATE_VAR_NS}_BASE_NAME ${${ICU_PRIVATE_VAR_NS}_COMPONENTS_${${ICU_PRIVATE_VAR_NS}_COMPONENT}})
            list(APPEND ${ICU_PRIVATE_VAR_NS}_POSSIBLE_RELEASE_NAMES "${${ICU_PRIVATE_VAR_NS}_BASE_NAME}")
            list(APPEND ${ICU_PRIVATE_VAR_NS}_POSSIBLE_DEBUG_NAMES "${${ICU_PRIVATE_VAR_NS}_BASE_NAME}d")
            list(APPEND ${ICU_PRIVATE_VAR_NS}_POSSIBLE_RELEASE_NAMES "${${ICU_PRIVATE_VAR_NS}_BASE_NAME}${ICU_MAJOR_VERSION}${ICU_MINOR_VERSION}")
            list(APPEND ${ICU_PRIVATE_VAR_NS}_POSSIBLE_DEBUG_NAMES "${${ICU_PRIVATE_VAR_NS}_BASE_NAME}${ICU_MAJOR_VERSION}${ICU_MINOR_VERSION}d")
        endforeach(${ICU_PRIVATE_VAR_NS}_BASE_NAME)

        find_library(
            ${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT}
            NAMES ${${ICU_PRIVATE_VAR_NS}_POSSIBLE_RELEASE_NAMES}
            HINTS ${${ICU_PRIVATE_VAR_NS}_ROOT}
            PATH_SUFFIXES ${_ICU_LIB_SUFFIXES}
            DOC "Release libraries for ICU"
        )
        find_library(
            ${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT}
            NAMES ${${ICU_PRIVATE_VAR_NS}_POSSIBLE_DEBUG_NAMES}
            HINTS ${${ICU_PRIVATE_VAR_NS}_ROOT}
            PATH_SUFFIXES ${_ICU_LIB_SUFFIXES}
            DOC "Debug libraries for ICU"
        )

        string(TOUPPER "${${ICU_PRIVATE_VAR_NS}_COMPONENT}" ${ICU_PRIVATE_VAR_NS}_UPPER_COMPONENT)
        if(NOT ${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT} AND NOT ${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT}) # both not found
            set("${ICU_PUBLIC_VAR_NS}_${${ICU_PRIVATE_VAR_NS}_UPPER_COMPONENT}_FOUND" FALSE)
            set("${ICU_PUBLIC_VAR_NS}_FOUND" FALSE)
        else(NOT ${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT} AND NOT ${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT}) # one or both found
            set("${ICU_PUBLIC_VAR_NS}_${${ICU_PRIVATE_VAR_NS}_UPPER_COMPONENT}_FOUND" TRUE)
            if(NOT ${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT}) # release not found => we are in debug
                set(${ICU_PRIVATE_VAR_NS}_LIB_${${ICU_PRIVATE_VAR_NS}_COMPONENT} "${${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT}}")
            elseif(NOT ${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT}) # debug not found => we are in release
                set(${ICU_PRIVATE_VAR_NS}_LIB_${${ICU_PRIVATE_VAR_NS}_COMPONENT} "${${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT}}")
            else() # both found
                set(
                    ${ICU_PRIVATE_VAR_NS}_LIB_${${ICU_PRIVATE_VAR_NS}_COMPONENT}
                    optimized ${${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT}}
                    debug ${${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT}}
                )
            endif()
            list(APPEND ${ICU_PUBLIC_VAR_NS}_LIBRARIES ${${ICU_PRIVATE_VAR_NS}_LIB_${${ICU_PRIVATE_VAR_NS}_COMPONENT}})
        endif(NOT ${ICU_PRIVATE_VAR_NS}_LIB_RELEASE_${${ICU_PRIVATE_VAR_NS}_COMPONENT} AND NOT ${ICU_PRIVATE_VAR_NS}_LIB_DEBUG_${${ICU_PRIVATE_VAR_NS}_COMPONENT})
    endforeach(${ICU_PRIVATE_VAR_NS}_COMPONENT)
endif(${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS)

if(${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS)
    include(FindPackageHandleStandardArgs)
    if(${ICU_PUBLIC_VAR_NS}_FIND_REQUIRED AND NOT ${ICU_PUBLIC_VAR_NS}_FIND_QUIETLY)
        find_package_handle_standard_args(
            ${ICU_PUBLIC_VAR_NS}
            REQUIRED_VARS ${ICU_PUBLIC_VAR_NS}_LIBRARIES ${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS
            VERSION_VAR ${ICU_PUBLIC_VAR_NS}_VERSION
        )
    else(${ICU_PUBLIC_VAR_NS}_FIND_REQUIRED AND NOT ${ICU_PUBLIC_VAR_NS}_FIND_QUIETLY)
        find_package_handle_standard_args(${ICU_PUBLIC_VAR_NS} "ICU not found" ${ICU_PUBLIC_VAR_NS}_LIBRARIES ${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS)
    endif(${ICU_PUBLIC_VAR_NS}_FIND_REQUIRED AND NOT ${ICU_PUBLIC_VAR_NS}_FIND_QUIETLY)
else(${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS)
    if(${ICU_PUBLIC_VAR_NS}_FIND_REQUIRED AND NOT ${ICU_PUBLIC_VAR_NS}_FIND_QUIETLY)
        message(FATAL_ERROR "Could not find ICU include directory")
    endif(${ICU_PUBLIC_VAR_NS}_FIND_REQUIRED AND NOT ${ICU_PUBLIC_VAR_NS}_FIND_QUIETLY)
endif(${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS)

mark_as_advanced(
    ${ICU_PUBLIC_VAR_NS}_INCLUDE_DIRS
    ${ICU_PUBLIC_VAR_NS}_LIBRARIES
)

########## <resource bundle support> ##########

########## Private ##########
function(extract_locale_from_rb _BUNDLE_SOURCE _RETURN_VAR_NAME)
    file(READ "${_BUNDLE_SOURCE}" _BUNDLE_CONTENTS)
    string(REGEX REPLACE "//[^\n]*\n" "" _BUNDLE_CONTENTS_WITHOUT_COMMENTS ${_BUNDLE_CONTENTS})
    string(REGEX REPLACE "[ \t\n]" "" _BUNDLE_CONTENTS_WITHOUT_COMMENTS_AND_SPACES ${_BUNDLE_CONTENTS_WITHOUT_COMMENTS})
    string(REGEX MATCH "^([a-zA-Z_-]+)(:table)?{" LOCALE_FOUND ${_BUNDLE_CONTENTS_WITHOUT_COMMENTS_AND_SPACES})
    set("${_RETURN_VAR_NAME}" "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction(extract_locale_from_rb)

########## Public ##########
find_program(${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE genrb)
find_program(${ICU_PUBLIC_VAR_NS}_PKGDATA_EXECUTABLE pkgdata)

if(NOT ${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE)
    message(FATAL_ERROR "genrb not found")
endif(NOT ${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE)
if(NOT ${ICU_PUBLIC_VAR_NS}_PKGDATA_EXECUTABLE)
    message(FATAL_ERROR "pkgdata not found")
endif(NOT ${ICU_PUBLIC_VAR_NS}_PKGDATA_EXECUTABLE)


#
# Prototype:
#   generate_icu_resource_bundle([PACKAGE <name>] [DESTINATION <location>] [FILES <list of files>])
#
# Common arguments:
#   - FILES <file 1> ... <file N>      : list of resource bundles sources
#   - DEPENDS <target1> ... <target N> : optional (default: all) but required to package as library (shared or static),
#                                        a list of cmake parent targets on which resource bundles or package depends
#                                        Note: only (PREVIOUSLY DECLARED) add_executable and add_library as dependencies
#   - DESTINATION <location>           : optional, directory where to install final binary file(s)
#   - FORMAT <name>                    : optional, one of none (ICU4C binary format, default), java (plain java) or xliff (XML), see below
#
# Arguments depending on FORMAT:
#   - none (default):
#       * PACKAGE <name> : optional, to package all resource bundles together
#       * TYPE <name>    : one of :
#           + common or archive (default) : archive all ressource bundles into a single .dat file
#           + library or dll              : assemble all ressource bundles into a separate and loadable library (.dll/.so)
#           + static                      : integrate all ressource bundles to targets designed by DEPENDS parameter (as a static library)
#   - JAVA:
#       * BUNDLE <name> : required, prefix for generated classnames
#   - XLIFF:
#       (none)
#

#
# TODO:
# - default locale is "fr_FR", not "root" when running rb? => LANG seems to be in conflict with argv[1]? (./rb it = "fr.txt" but LANG=it_IT ./rb it = "root.txt")
# - DEPENDS argument:
#     + ALL as default value
#     + modify add_custom_target (s/ALL/${PARSED_ARGS_DEPENDS} || ALL/)
#     + assert ${PARSED_ARGS_DEPENDS} != "" if ${PARSED_ARGS_PACKAGE} != "" and ${PKGDATA_LIBRARY_${TYPE}_TYPE} != ""
# - return (via an output variable) all final generated files BEFORE installation?
# - let the user name its target (add an argument to generate_icu_resource_bundle)?
# - genrb (add_custom_command), when ${PARSED_ARGS_PACKAGE} == "", chdir to resource bundle source's directory
# - automatically add -fPIC to COMPILE_FLAGS for shared type?
# - dynamically forge "fake_foo_bar" target?
# - rename all functions ("icu_" prefix for public functions, "_icu_" prefix for private functions)?
# - cleanup

function(generate_icu_resource_bundle)

    ##### <hash constants> #####
    # filename extension of built resource bundle (without dot)
    set(BUNDLES__SUFFIX "res")
    set(BUNDLES_JAVA_SUFFIX "java")
    set(BUNDLES_XLIFF_SUFFIX "xlf")
    # alias: none (default) = common = archive ; dll = library ; static
    set(PKGDATA__ALIAS "")
    set(PKGDATA_COMMON_ALIAS "")
    set(PKGDATA_ARCHIVE_ALIAS "")
    set(PKGDATA_DLL_ALIAS "LIBRARY")
    set(PKGDATA_LIBRARY_ALIAS "LIBRARY")
    set(PKGDATA_STATIC_ALIAS "STATIC")
    # filename prefix of built package
    set(PKGDATA__PREFIX "")
    set(PKGDATA_LIBRARY_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
    set(PKGDATA_STATIC_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
    # filename extension of built package (with dot)
    set(PKGDATA__SUFFIX ".dat")
    set(PKGDATA_LIBRARY_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(PKGDATA_STATIC_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    # pkgdata option mode specific
    set(PKGDATA__OPTIONS "-m" "common")
    set(PKGDATA_STATIC_OPTIONS "-m" "static")
    set(PKGDATA_LIBRARY_OPTIONS "-m" "library")
    # cmake library type for output package
    set(PKGDATA_LIBRARY__TYPE "")
    set(PKGDATA_LIBRARY_STATIC_TYPE STATIC)
    set(PKGDATA_LIBRARY_LIBRARY_TYPE SHARED)
    ##### </hash constants> #####

    set(PACKAGE_TARGET_PREFIX "ICU_PKG_")
    set(RESOURCE_TARGET_PREFIX "ICU_RB_")

    cmake_parse_arguments(
        PARSED_ARGS # output variable name
        # options (true/false) (default value: false)
        ""
        # univalued parameters (default value: "")
        "PACKAGE;DESTINATION;TYPE;FORMAT;BUNDLE"
        # multivalued parameters (default value: "")
        "FILES;DEPENDS"
        ${ARGN}
    )

    # assert(length(PARSED_ARGS_FILES) > 0)
    list(LENGTH PARSED_ARGS_FILES PARSED_ARGS_FILES_LEN)
    if(PARSED_ARGS_FILES_LEN LESS 1)
        message(FATAL_ERROR "generate_icu_resource_bundle() expects at least 1 resource bundle as FILES argument, 0 given")
    endif(PARSED_ARGS_FILES_LEN LESS 1)

    string(TOUPPER "${PARSED_ARGS_FORMAT}" UPPER_FORMAT)
    # assert(${UPPER_FORMAT} in ['', 'java', 'xlif'])
    if(NOT DEFINED BUNDLES_${UPPER_FORMAT}_SUFFIX)
        message(FATAL_ERROR "generate_icu_resource_bundle(): unknown FORMAT '${PARSED_ARGS_FORMAT}'")
    endif(NOT DEFINED BUNDLES_${UPPER_FORMAT}_SUFFIX)

    if(UPPER_FORMAT STREQUAL "JAVA")
        # assert(${PARSED_ARGS_BUNDLE} != "")
        if(NOT PARSED_ARGS_BUNDLE)
            message(FATAL_ERROR "generate_icu_resource_bundle(): java bundle name expected, BUNDLE parameter missing")
        endif(NOT PARSED_ARGS_BUNDLE)
    endif(UPPER_FORMAT STREQUAL "JAVA")

    if(PARSED_ARGS_PACKAGE)
        # assert(${PARSED_ARGS_FORMAT} == "")
        if(PARSED_ARGS_FORMAT)
            message(FATAL_ERROR "generate_icu_resource_bundle(): packaging is only supported for binary format, not xlif neither java outputs")
        endif(PARSED_ARGS_FORMAT)

        string(TOUPPER "${PARSED_ARGS_TYPE}" UPPER_MODE)
        # assert(${UPPER_MODE} in ['', 'common', 'archive', 'dll', library'])
        if(NOT DEFINED PKGDATA_${UPPER_MODE}_ALIAS)
            message(FATAL_ERROR "generate_icu_resource_bundle(): unknown TYPE '${PARSED_ARGS_TYPE}'")
        else(NOT DEFINED PKGDATA_${UPPER_MODE}_ALIAS)
            set(TYPE "${PKGDATA_${UPPER_MODE}_ALIAS}")
        endif(NOT DEFINED PKGDATA_${UPPER_MODE}_ALIAS)

        # Package name: strip file extension if present
        get_filename_component(PACKAGE_NAME_WE ${PARSED_ARGS_PACKAGE} NAME_WE)
        # Target name to build package
        set(PACKAGE_TARGET_NAME "${PACKAGE_TARGET_PREFIX}${PACKAGE_NAME_WE}")
        # Target name to build intermediate list file
        set(PACKAGE_LIST_TARGET_NAME "${PACKAGE_TARGET_NAME}_LIST")
        # Directory (absolute) to set as "current directory" for genrb (does not include package directory, -p)
        set(RESOURCE_GENRB_CHDIR_DIR "${CMAKE_PLATFORM_ROOT_BIN}/${PACKAGE_TARGET_NAME}.dir/")
        # Directory (absolute) where resource bundles are built: concatenation of RESOURCE_GENRB_CHDIR_DIR and package name
        set(RESOURCE_OUTPUT_DIR "${RESOURCE_GENRB_CHDIR_DIR}/${PACKAGE_NAME_WE}/")
        # Output (relative) path for built package
#         if(NOT TYPE)
            set(PACKAGE_OUTPUT_PATH "${RESOURCE_GENRB_CHDIR_DIR}/${PKGDATA_${TYPE}_PREFIX}${PACKAGE_NAME_WE}${PKGDATA_${TYPE}_SUFFIX}")
#         else(NOT TYPE)
#             set(PACKAGE_OUTPUT_PATH "${RESOURCE_GENRB_CHDIR_DIR}/${PACKAGE_NAME_WE}/${PKGDATA_${TYPE}_PREFIX}${PACKAGE_NAME_WE}${PKGDATA_${TYPE}_SUFFIX}")
#         endif(NOT TYPE)
        # Output (absolute) path for the list file
        set(PACKAGE_LIST_OUTPUT_PATH "${RESOURCE_GENRB_CHDIR_DIR}/pkglist.txt")

        file(MAKE_DIRECTORY "${RESOURCE_OUTPUT_DIR}")
    else(PARSED_ARGS_PACKAGE)
        set(RESOURCE_OUTPUT_DIR "")
        set(RESOURCE_GENRB_CHDIR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif(PARSED_ARGS_PACKAGE)

    set(TARGET_RESOURCES )
    set(COMPILED_RESOURCES_PATH )
    # Use a whitespace separated string not a semicolon separated list
    # last one is system dependant (the following echo will fail on an Unix system)
    set(COMPILED_RESOURCES_BASENAME "")
    foreach(RESOURCE_SOURCE ${PARSED_ARGS_FILES})
        get_filename_component(RESOURCE_NAME_WE ${RESOURCE_SOURCE} NAME_WE)
        get_filename_component(SOURCE_BASENAME ${RESOURCE_SOURCE} NAME)
        get_filename_component(RELATIVE_SOURCE_DIRECTORY ${RESOURCE_SOURCE} PATH)
        if(NOT RELATIVE_SOURCE_DIRECTORY)
            set(RELATIVE_SOURCE_DIRECTORY ".")
        endif(NOT RELATIVE_SOURCE_DIRECTORY)
        get_filename_component(ABSOLUTE_SOURCE_DIRECTORY ${RELATIVE_SOURCE_DIRECTORY} ABSOLUTE)
        file(RELATIVE_PATH SOURCE_DIRECTORY ${RESOURCE_GENRB_CHDIR_DIR} ${ABSOLUTE_SOURCE_DIRECTORY})

        if(NOT PARSED_ARGS_PACKAGE)
            file(MD5 ${RESOURCE_SOURCE} PACKAGE_NAME_WE)
        endif(NOT PARSED_ARGS_PACKAGE)

        if(UPPER_FORMAT STREQUAL "XLIFF")
            if(RESOURCE_NAME_WE STREQUAL "root")
                set(XLIFF_LANGUAGE "en")
            else(RESOURCE_NAME_WE STREQUAL "root")
                string(REGEX REPLACE "[^a-z].*$" "" XLIFF_LANGUAGE "${RESOURCE_NAME_WE}")
            endif(RESOURCE_NAME_WE STREQUAL "root")
        endif(UPPER_FORMAT STREQUAL "XLIFF")

        if(NOT SOURCE_DIRECTORY)
            set(SOURCE_DIRECTORY ".")
        endif(NOT SOURCE_DIRECTORY)

        ##### <templates> #####
        set(RESOURCE_TARGET_NAME "${RESOURCE_TARGET_PREFIX}${PACKAGE_NAME_WE}_${RESOURCE_NAME_WE}")

        # <TODO>
        # assert(extract_locale_from_rb(${RESOURCE_SOURCE}) == ${RESOURCE_NAME_WE})
        # or better: s/get_filename_component(RESOURCE_NAME_WE ${RESOURCE_SOURCE} NAME_WE)/extract_locale_from_rb(${RESOURCE_SOURCE} RESOURCE_NAME_WE)/
        set(RESOURCE_OUTPUT__PATH "${RESOURCE_NAME_WE}.res")
        # </TODO>
        if(RESOURCE_NAME_WE STREQUAL "root")
            set(RESOURCE_OUTPUT_JAVA_PATH "${PARSED_ARGS_BUNDLE}.java")
        else(RESOURCE_NAME_WE STREQUAL "root")
            set(RESOURCE_OUTPUT_JAVA_PATH "${PARSED_ARGS_BUNDLE}_${RESOURCE_NAME_WE}.java")
        endif(RESOURCE_NAME_WE STREQUAL "root")
        set(RESOURCE_OUTPUT_XLIFF_PATH "${RESOURCE_NAME_WE}.xlf")

        set(GENRB__OPTIONS "")
        set(GENRB_JAVA_OPTIONS "-j" "-b" "${PARSED_ARGS_BUNDLE}")
        set(GENRB_XLIFF_OPTIONS "-x" "-l" "${XLIFF_LANGUAGE}")
        ##### </templates> #####

        if(PARSED_ARGS_PACKAGE)
            add_custom_command(
                OUTPUT "${RESOURCE_OUTPUT_DIR}${RESOURCE_OUTPUT_${UPPER_FORMAT}_PATH}"
                #COMMAND ${${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE} ${GENRB_ARGS} ${RESOURCE_SOURCE}
                COMMAND ${CMAKE_COMMAND} -E chdir ${RESOURCE_GENRB_CHDIR_DIR} ${${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE} ${GENRB_${UPPER_FORMAT}_OPTIONS} -d${PACKAGE_NAME_WE} -s${ABSOLUTE_SOURCE_DIRECTORY} ${SOURCE_BASENAME}
                #COMMAND ${${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE} ${GENRB_ARGS} -s"${SOURCE_DIRECTORY}" ${SOURCE_BASENAME}
                DEPENDS ${RESOURCE_SOURCE}
            )
        else(PARSED_ARGS_PACKAGE)
            add_custom_command(
                OUTPUT "${RESOURCE_OUTPUT_DIR}${RESOURCE_OUTPUT_${UPPER_FORMAT}_PATH}"
                COMMAND ${${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE} ${GENRB_${UPPER_FORMAT}_OPTIONS} ${RESOURCE_SOURCE}
                #COMMAND ${${ICU_PUBLIC_VAR_NS}_GENRB_EXECUTABLE} -s"${SOURCE_DIRECTORY}" ${SOURCE_BASENAME}
                DEPENDS ${RESOURCE_SOURCE}
            )
        endif(PARSED_ARGS_PACKAGE)
        add_custom_target(
            "${RESOURCE_TARGET_NAME}" ALL
            COMMENT ""
            DEPENDS "${RESOURCE_OUTPUT_DIR}${RESOURCE_OUTPUT_${UPPER_FORMAT}_PATH}"
            SOURCES ${RESOURCE_SOURCE}
        )

        if(PARSED_ARGS_DESTINATION AND NOT PARSED_ARGS_PACKAGE)
            install(FILES "${RESOURCE_OUTPUT_DIR}${RESOURCE_OUTPUT_${UPPER_FORMAT}_PATH}" DESTINATION ${PARSED_ARGS_DESTINATION} PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
        endif(PARSED_ARGS_DESTINATION AND NOT PARSED_ARGS_PACKAGE)

        list(APPEND TARGET_RESOURCES "${RESOURCE_TARGET_NAME}")
        list(APPEND COMPILED_RESOURCES_PATH "${RESOURCE_OUTPUT_DIR}${RESOURCE_OUTPUT_${UPPER_FORMAT}_PATH}")
        set(COMPILED_RESOURCES_BASENAME "${COMPILED_RESOURCES_BASENAME} ${RESOURCE_NAME_WE}.${BUNDLES_${UPPER_FORMAT}_SUFFIX}")
    endforeach(RESOURCE_SOURCE)

    if(PARSED_ARGS_PACKAGE)
        add_custom_command(
            OUTPUT "${PACKAGE_LIST_OUTPUT_PATH}"
            COMMAND ${CMAKE_COMMAND} -E echo "${COMPILED_RESOURCES_BASENAME}" > "${PACKAGE_LIST_OUTPUT_PATH}"
            DEPENDS ${COMPILED_RESOURCES_PATH}
        )
        add_custom_command(
            OUTPUT "${PACKAGE_OUTPUT_PATH}"
            COMMAND ${CMAKE_COMMAND} -E chdir ${RESOURCE_GENRB_CHDIR_DIR} ${${ICU_PUBLIC_VAR_NS}_PKGDATA_EXECUTABLE} ${PKGDATA_${TYPE}_OPTIONS} -s ${PACKAGE_NAME_WE} -p ${PACKAGE_NAME_WE} -F ${PACKAGE_LIST_OUTPUT_PATH}
            DEPENDS "${PACKAGE_LIST_OUTPUT_PATH}"
            VERBATIM
        )
        if(PKGDATA_LIBRARY_${TYPE}_TYPE)
            # TODO: assert(${PARSED_ARGS_DEPENDS} != "")
            add_library(${PACKAGE_TARGET_NAME} ${PKGDATA_LIBRARY_${TYPE}_TYPE} IMPORTED)
            set_target_properties(${PACKAGE_TARGET_NAME} PROPERTIES IMPORTED_LOCATION ${PACKAGE_OUTPUT_PATH} IMPORTED_IMPLIB ${PACKAGE_OUTPUT_PATH})
            foreach(DEPENDENCY ${PARSED_ARGS_DEPENDS})
                target_link_libraries(${DEPENDENCY} ${PACKAGE_TARGET_NAME})
            endforeach(DEPENDENCY)
            # http://www.mail-archive.com/cmake-commits@cmake.org/msg01135.html
            add_custom_target(
                fake_foo_bar
                COMMENT ""
                DEPENDS "${PACKAGE_OUTPUT_PATH}"
            )
            add_dependencies("${PACKAGE_TARGET_NAME}" fake_foo_bar)
        else(PKGDATA_LIBRARY_${TYPE}_TYPE)
                add_custom_target(
                    "${PACKAGE_TARGET_NAME}" ALL
                    COMMENT ""
                    DEPENDS "${PACKAGE_OUTPUT_PATH}"
                )
        endif(PKGDATA_LIBRARY_${TYPE}_TYPE)
        add_custom_target(
            "${PACKAGE_LIST_TARGET_NAME}" ALL
            COMMENT ""
            DEPENDS "${PACKAGE_LIST_OUTPUT_PATH}"
        )
        add_dependencies("${PACKAGE_TARGET_NAME}" "${PACKAGE_LIST_TARGET_NAME}")
        add_dependencies("${PACKAGE_LIST_TARGET_NAME}" ${TARGET_RESOURCES})

        if(PARSED_ARGS_DESTINATION)
            install(FILES "${PACKAGE_OUTPUT_PATH}" DESTINATION ${PARSED_ARGS_DESTINATION} PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
        endif(PARSED_ARGS_DESTINATION)
    endif(PARSED_ARGS_PACKAGE)

endfunction(generate_icu_resource_bundle)

########## </resource bundle support> ##########

# IN (args)
icudebug("FIND_COMPONENTS")
icudebug("FIND_REQUIRED")
icudebug("FIND_QUIETLY")
icudebug("FIND_VERSION")
# OUT
# Found
icudebug("FOUND")
icudebug("UC_FOUND")
icudebug("I18N_FOUND")
icudebug("IO_FOUND")
icudebug("LE_FOUND")
icudebug("LX_FOUND")
icudebug("DATA_FOUND")
# Linking
icudebug("INCLUDE_DIRS")
icudebug("LIBRARIES")
# Executables
icudebug("GENRB_EXECUTABLE")
icudebug("PKGDATA_EXECUTABLE")
# Version
icudebug("MAJOR_VERSION")
icudebug("MINOR_VERSION")
icudebug("PATCH_VERSION")
icudebug("VERSION")
