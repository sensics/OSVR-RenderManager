# - Find liquidvr
# Find the liquidvr header.
#
#  LIQUIDVR_INCLUDE_DIRS - where to find LiquidVRD2D.h
#  LIQUIDVR_FOUND        - True if liquidvr found.
#
# Creates an "liquidvr" interface target with the include dir
# that you can link against instead of using the above variables.
#
# Original Author:
# 2016 Russ Taylor working through ReliaSolve for Sensics.
#
# Copyright Sensics, Inc. 2016.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set(LIQUIDVR_ROOT_DIR
	"${LIQUIDVR_ROOT_DIR}"
	CACHE
	PATH
	"Path to search for liquidvr library")

# Look for the header file.
find_path(LIQUIDVR_INCLUDE_DIR
	NAMES
	LiquidVRD2D.h
	PATH_SUFFIXES
	include
	inc
	PATHS
	"${LIQUIDVR_ROOT_DIR}")

# handle the QUIETLY and REQUIRED arguments and set LIQUIDVR_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(liquidvr
	REQUIRED_VARS LIQUIDVR_INCLUDE_DIR
	HANDLE_COMPONENTS)

if(LIQUIDVR_FOUND)
	set(LIQUIDVR_INCLUDE_DIRS ${LIQUIDVR_INCLUDE_DIR})
	if(NOT TARGET liquidvr)
		add_library(liquidvr INTERFACE)
		target_include_directories(liquidvr INTERFACE "${LIQUIDVR_INCLUDE_DIR}")
	endif()
	mark_as_advanced(LIQUIDVR_ROOT_DIR)
else()
	set(LIQUIDVR_INCLUDE_DIRS)
	set(LIQUIDVR_ROOT_DIR LIQUIDVR_ROOT_DIR_NOTFOUND)
	mark_as_advanced(CLEAR LIQUIDVR_ROOT_DIR)
endif()

mark_as_advanced(LIQUIDVR_LIBRARY LIQUIDVR_INCLUDE_DIR)
