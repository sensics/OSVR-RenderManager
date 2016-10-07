
# Hook for a super-build to optionally inject hints before target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildPrefix.cmake" OPTIONAL)

set(OSVRRM_IN_BUILD_TREE @OSVRRM_IN_BUILD_TREE@)

if(NOT OSVRRM_IN_BUILD_TREE)
	# Compute the installation prefix relative to this file.
	get_filename_component(OSVRRM_ROOT "${CMAKE_CURRENT_LIST_FILE}" PATH)
	get_filename_component(OSVRRM_ROOT "${OSVRRM_ROOT}" PATH)
	get_filename_component(OSVRRM_ROOT "${OSVRRM_ROOT}" PATH)
	get_filename_component(OSVRRM_ROOT "${OSVRRM_ROOT}" PATH)
endif()

set(OSVRRM_NEED_SDL2 @OSVRRM_NEED_SDL2@)
set(OSVRRM_NEED_GLEW @OSVRRM_NEED_GLEW@)

set(OSVRRM_PREV_CMAKE_MODULES ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}" ${CMAKE_MODULE_PATH})
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigBuildTreeHints.cmake" OPTIONAL)
include(osvrStashMapConfig)
include(CMakeFindDependencyMacro)
if(OSVRRM_NEED_SDL2)
    # TODO
	#find_dependency(SDL2)
endif()
if(OSVRRM_NEED_GLEW)
    # TODO
	#find_dependency(GLEW)
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

if(NOT TARGET osvrRenderManager::osvrRenderManagerCpp)
    add_library(osvrRenderManager::osvrRenderManagerCpp INTERFACE IMPORTED)
    set_target_properties(osvrRenderManager::osvrRenderManagerCpp PROPERTIES
        INTERFACE_LINK_LIBRARIES "osvrRenderManager::osvrRenderManager;osvr::osvrClientKitCpp")
endif()

# Fix up imported targets to add deps: these files will only exist in install trees.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigInstalledBoost.cmake" OPTIONAL)

# A list of filenames of required libraries for running with osvrRenderManager DLL
set(OSVRRM_REQUIRED_LIBRARIES_DEBUG "@OSVRRM_REQUIRED_LIBRARIES_DEBUG@")
set(OSVRRM_REQUIRED_LIBRARIES_RELEASE "@OSVRRM_REQUIRED_LIBRARIES_RELEASE@")
set(OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO "@OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO@")
set(OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL "@OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL@")
set(OSVRRM_NEED_SDL2_COPIED @OSVRRM_NEED_SDL2_COPIED@)
set(OSVRRM_NEED_GLEW_COPIED @OSVRRM_NEED_GLEW_COPIED@)

# Options
set(OSVRRM_HAVE_D3D11_SUPPORT @OSVRRM_HAVE_D3D11_SUPPORT@)
set(OSVRRM_HAVE_OPENGL_SUPPORT @OSVRRM_HAVE_OPENGL_SUPPORT@)
set(OSVRRM_USE_OPENGLES20 @OSVRRM_USE_OPENGLES20@)

# Helper function to copy required libraries to an install directory
set(OSVRRM_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "" FORCE)
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
	set(OSVRRM_NEED_D3DCOMPILER_COPIED @OSVRRM_NEED_D3DCOMPILER_COPIED@)
	if(OSVRRM_NEED_D3DCOMPILER_COPIED)
		install(FILES "@OSVRRM_D3DCOMPILER_FILE@"
			DESTINATION ${_destination}
			COMPONENT Runtime
			OPTIONAL)
	endif()
endfunction()

# Find SDL2 using the script bundled with RenderManager.
macro(osvrrm_find_SDL2)
    set(OSVRRM_PREV_CMAKE_MODULES  ${CMAKE_MODULE_PATH})
    set(CMAKE_MODULE_PATH "${OSVRRM_CONFIG_DIR}" ${CMAKE_MODULE_PATH})
    find_package(SDL2 ${ARGN})
    set(CMAKE_MODULE_PATH ${OSVRRM_PREV_CMAKE_MODULES})
endmacro()

# Find GLEW using the script bundled with RenderManager.
macro(osvrrm_find_GLEW)
    set(OSVRRM_PREV_CMAKE_MODULES  ${CMAKE_MODULE_PATH})
    set(CMAKE_MODULE_PATH "${OSVRRM_CONFIG_DIR}" ${CMAKE_MODULE_PATH})
    find_package(GLEW ${ARGN})
    set(CMAKE_MODULE_PATH ${OSVRRM_PREV_CMAKE_MODULES})
endmacro()

# Hook for a super-build to optionally inject configuration after target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildSuffix.cmake" OPTIONAL)
