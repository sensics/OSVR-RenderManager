# - Find osvrRenderManager
# Find the osvrRenderManager headers and libraries.
#
#  OSVRRENDERMANAGER_INCLUDE_DIRS - where to find header files
#  OSVRRENDERMANAGER_LIBRARIES    - List of libraries when using osvrRenderManager.
#  OSVRRENDERMANAGER_FOUND        - True if osvrRenderManager found.
#
# Original Author:
# 2015 Russ Taylor <russ@reliasolve.com>
#
# Copyright Sensics 2015.
# Distributed under the Apache Software License, Version 2.0.

set(OSVRRENDERMANAGER_ROOT_DIR
	"${OSVRRENDERMANAGER_ROOT_DIR}"
	CACHE
	PATH
	"Root directory to search for RenderManager")

if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
	set(_libsuffixes lib64 lib)

	# 64-bit dir: only set on win64
	file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _progfiles)
else()
	set(_libsuffixes lib)
	set(_PF86 "ProgramFiles(x86)")
	if(NOT "$ENV{${_PF86}}" STREQUAL "")
		# 32-bit dir: only set on win64
		file(TO_CMAKE_PATH "$ENV{${_PF86}}" _progfiles)
	else()
		# 32-bit dir on win32, useless to us on win64
		file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _progfiles)
	endif()
endif()

# Look for the header file.
find_path(OSVRRENDERMANAGER_INCLUDE_DIR
	NAMES
	osvr/RenderKit/RenderManager.h
	HINTS
	"${OSVRRENDERMANAGER_ROOT_DIR}"
	PATH_SUFFIXES
	include
	PATHS
	"${_progfiles}/osvrRenderManager"
	"${_progfiles}/Sensics/osvrRenderManager0.6.18"
	"${_progfiles}/Sensics/osvrRenderManager0.6.25"
	)

# Look for the library.
find_library(OSVRRENDERMANAGER_LIBRARY
	NAMES
	osvrRenderManager.lib
	libosvrRenderManager.a
	HINTS
	"${OSVRRENDERMANAGER_ROOT_DIR}"
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"${_progfiles}/osvrRenderManager"
	"${_progfiles}/Sensics/osvrRenderManager0.6.18"
	"${_progfiles}/Sensics/osvrRenderManager0.6.25"
	)

# handle the QUIETLY and REQUIRED arguments and set OSVRRENDERMANAGER_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(osvrRenderManager
	DEFAULT_MSG
	OSVRRENDERMANAGER_LIBRARY
	OSVRRENDERMANAGER_INCLUDE_DIR)

if(OSVRRENDERMANAGER_FOUND)
	set(OSVRRENDERMANAGER_LIBRARIES ${OSVRRENDERMANAGER_LIBRARY})
	set(OSVRRENDERMANAGER_INCLUDE_DIRS ${OSVRRENDERMANAGER_INCLUDE_DIR})
	mark_as_advanced(OSVRRENDERMANAGER_ROOT_DIR)
else()
	set(OSVRRENDERMANAGER_LIBRARIES)
	set(OSVRRENDERMANAGER_INCLUDE_DIRS)
endif()

mark_as_advanced(OSVRRENDERMANAGER_LIBRARY OSVRRENDERMANAGER_INCLUDE_DIR)
