# - Find nvapi
# Find the nvapi headers and libraries. Can request component NDA which
# will only succeed if an NDA version of the header is found.
#
#  NVAPI_INCLUDE_DIRS - where to find nvapi.h
#  NVAPI_LIBRARIES    - List of libraries when using nvapi.
#  NVAPI_FOUND        - True if nvapi found.
#
# Creates an "nvapi" interface target with the libraries and include dir
# that you can link against instead of using the above variables.
#
# Original Author:
# 2016 Russ Taylor working through ReliaSolve for Sensics.
#
# Copyright Sensics, Inc. 2016.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set(NVAPI_ROOT_DIR
	"${NVAPI_ROOT_DIR}"
	CACHE
	PATH
	"Path to search for nvapi library")

# Look for the header file.
find_path(NVAPI_INCLUDE_DIR
	NAMES
	nvapi.h
	PATH_SUFFIXES
	include
	PATHS
	"${NVAPI_ROOT_DIR}"
	C:/usr/local
	/usr/local)

# Allow user to have us re-detect the edition by clearing NVAPI_EDITION
if(NOT DEFINED NVAPI_EDITION)
	set(NVAPI_LAST_INCLUDE_DIR "" CACHE INTERNAL "" FORCE)
endif()
if(NVAPI_INCLUDE_DIR AND NOT "${NVAPI_INCLUDE_DIR}" STREQUAL "${NVAPI_LAST_INCLUDE_DIR}")
	file(STRINGS "${NVAPI_INCLUDE_DIR}/nvapi.h" _nvapi_strings REGEX "Target Profile:")
	if("${_nvapi_strings}" MATCHES ".* NDA-developer")
		set(NVAPI_EDITION "NDA-developer" CACHE STRING "The detected edition of the NVAPI")
	elseif("${_nvapi_strings}" MATCHES ".* developer")
		set(NVAPI_EDITION "developer" CACHE STRING "The detected edition of the NVAPI")
	endif()
	# If we found an edition, keep track of where we found it so that we don't re-check next time CMake runs.
	if(DEFINED NVAPI_EDITION)
		set(NVAPI_LAST_INCLUDE_DIR "${NVAPI_INCLUDE_DIR}" CACHE INTERNAL "" FORCE)
	endif()
endif()

if(NVAPI_INCLUDE_DIR)
	# Prioritize finding the lib near the header to hopefully ensure they match.
	set(_nvapi_hints "${NVAPI_INCLUDE_DIR}")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_nvapiextraname nvapi64 nvapi64.lib)
	set(_nvapipathsuffix amd64)
else()
	set(_nvapiextraname nvapi nvapi.lib)
	set(_nvapipathsuffix x86)
endif()

# Look for the library.
find_library(NVAPI_LIBRARY
	NAMES
	${_nvapiextraname}
	libnvapi.a
	HINTS
	${_nvapi_hints}
	PATH_SUFFIXES
	${_nvapipathsuffix}
	PATHS
	"${NVAPI_ROOT_DIR}"
	C:/usr/local
	/usr/local)

# Handle asking for component "NDA"
if("${NVAPI_EDITION}" STREQUAL "NDA-developer")
	set(nvapi_NDA_FOUND TRUE)
endif()

# handle the QUIETLY and REQUIRED arguments and set NVAPI_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(nvapi
	REQUIRED_VARS NVAPI_LIBRARY NVAPI_INCLUDE_DIR
	HANDLE_COMPONENTS)

if(NVAPI_FOUND)
	set(NVAPI_LIBRARIES ${NVAPI_LIBRARY})
	set(NVAPI_INCLUDE_DIRS ${NVAPI_INCLUDE_DIR})
	if(NOT TARGET nvapi)
		add_library(nvapi INTERFACE)
		target_include_directories(nvapi INTERFACE "${NVAPI_INCLUDE_DIR}")
		target_link_libraries(nvapi INTERFACE "${NVAPI_LIBRARY}")
	endif()
	mark_as_advanced(NVAPI_ROOT_DIR)
else()
	set(NVAPI_LIBRARIES)
	set(NVAPI_INCLUDE_DIRS)
	set(NVAPI_ROOT_DIR NVAPI_ROOT_DIR_NOTFOUND)
	mark_as_advanced(CLEAR NVAPI_ROOT_DIR)
endif()

mark_as_advanced(NVAPI_LIBRARY NVAPI_INCLUDE_DIR)
