
# Hook for a super-build to optionally inject hints before target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildPrefix.cmake" OPTIONAL)

# TODO Dependencies
#find_dependency(GLEW)

# The actual exported targets
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerTargets.cmake")

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
			COMPONENT Runtime)
	endforeach()
endfunction()

# Hook for a super-build to optionally inject configuration after target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildSuffix.cmake" OPTIONAL)
