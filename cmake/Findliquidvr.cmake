# - Find liquidvr
# Find the liquidvr header.
#
#  LIQUIDVR_INCLUDE_DIRS - where to find LiquidVRD2D.h
#  LIQUIDVR_FOUND        - True if liquidvr found.
#
# Creates an "liquidvr" interface target with the include dir
# that you can link against instead of using the above variables.
#
# Copyright 2015 Sensics, Inc.
# Proprietary under NDA with AMD.  Not to be distributed beyond
# Sensics or AMD.

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
	PATHS
	"${LIQUIDVR_ROOT_DIR}"
	C:/usr/local
	/usr/local)

# handle the QUIETLY and REQUIRED arguments and set LIQUIDVR_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(liquidvr
	REQUIRED_VARS LIQUIDVR_INCLUDE_DIR
	HANDLE_COMPONENTS)

if(LIQUIDVR_FOUND)
	set(LIQUIDVR_INCLUDE_DIRS ${LIQUIDVR_INCLUDE_DIR})
	# @todo Consider adding this as an (empty) library so we can
	# do a target include dir rather than adding it to all.
	include_directories(INTERFACE "${LIQUIDVR_INCLUDE_DIR}")
	mark_as_advanced(LIQUIDVR_ROOT_DIR)
else()
	set(LIQUIDVR_INCLUDE_DIRS)
	set(LIQUIDVR_ROOT_DIR LIQUIDVR_ROOT_DIR_NOTFOUND)
	mark_as_advanced(CLEAR LIQUIDVR_ROOT_DIR)
endif()

mark_as_advanced(LIQUIDVR_LIBRARY LIQUIDVR_INCLUDE_DIR)

