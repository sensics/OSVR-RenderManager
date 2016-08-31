
# Hook for a super-build to optionally inject hints before target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildPrefix.cmake" OPTIONAL)

# TODO Dependencies
#find_dependency(GLEW)

# The actual exported targets
set(OSVRRM_PREV_CMAKE_MODULES ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}" ${CMAKE_MODULE_PATH})
include(osvrStashMapConfig)
include(CMakeFindDependencyMacro)
set(OSVRRM_NEED_SDL2 @OSVRRM_NEED_SDL2@)
if(OSVRRM_NEED_SDL2)
	find_dependency(SDL2)
endif()
set(OSVRRM_NEED_GLEW @OSVRRM_NEED_GLEW@)
if(OSVRRM_NEED_GLEW)
	find_dependency(GLEW)
endif()

# Set up config mapping
osvr_stash_common_map_config()

find_dependency(osvr)

# restore module path
set(CMAKE_MODULE_PATH ${OSVRRM_PREV_CMAKE_MODULES})

# The actual exported targets - config mapping still applied.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerTargets.cmake")

# Undo our changes for later consumers
osvr_unstash_common_map_config()

# Fix up imported targets to add deps: these files will only exist in install trees.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigInstalledBoost.cmake" OPTIONAL)

# A list of filenames of required libraries for running with osvrRenderManager DLL
set(OSVRRM_REQUIRED_LIBRARIES_DEBUG "@OSVRRM_REQUIRED_LIBRARIES_DEBUG@")
set(OSVRRM_REQUIRED_LIBRARIES_RELEASE "@OSVRRM_REQUIRED_LIBRARIES_RELEASE@")
set(OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO "@OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO@")
set(OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL "@OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL@")

# Helper function to copy required libraries to an install directory
set(OSVRRM_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}")
function(osvrrm_install_dependencies _destination)
	foreach(_config Debug Release RelWithDebInfo MinSizeRel)
		string(TOUPPER "${_config}" _CONFIG)
		# Canonicalize paths relative to the directory containing this .cmake file
		set(_files)
		foreach(_file IN LISTS OSVRRM_REQUIRED_LIBRARIES_${_CONFIG})
			get_filename_component(_file "${_file}" ABSOLUTE BASE_DIR "${OSVRRM_CONFIG_DIR}")
			list(APPEND _files "${_file}")
		endforeach()
		install(FILES
			${_files}
			CONFIGURATIONS ${_config}
			DESTINATION ${_destination}
			COMPONENT Runtime
			OPTIONAL)
	endforeach()
endfunction()

# Hook for a super-build to optionally inject configuration after target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildSuffix.cmake" OPTIONAL)
