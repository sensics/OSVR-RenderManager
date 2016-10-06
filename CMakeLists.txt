cmake_minimum_required(VERSION 3.1.0)
project(osvrRenderManager)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-----------------------------------------------------------------------------
# Local CMake Modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Version)

#-----------------------------------------------------------------------------
# Export libraries along with our DLLs if we want to build shared
# Allow the developer to select if Dynamic or Static libraries are built

include (GenerateExportHeader)

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)

include(osvrStashMapConfig)

#-----------------------------------------------------------------------------
# This looks for an osvrConfig.cmake file - most of the time it can be
# autodetected but you might need to specify osvr_DIR to be something like
# C:/Users/Ryan/Desktop/build/OSVR-Core-vc12 or
# C:/Users/Ryan/Downloads/OSVR-Core-Snapshot-v0.1-406-gaa55515-build54-vs12-32bit
# in the CMake GUI or command line
osvr_stash_common_map_config()
find_package(osvr REQUIRED)
osvr_unstash_common_map_config()

find_package(Eigen3 REQUIRED)
find_package(JsonCpp REQUIRED)

# Check for the NDA submodules
set(HAVE_NVIDIA_NDA_SUBMODULE FALSE)
set(HAVE_AMD_NDA_SUBMODULE FALSE)
if(WIN32)
	set(NVIDIA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-NVIDIA")
	if(EXISTS "${NVIDIA_SRC_DIR}/RenderManagerNVidiaD3D.cpp")
		set(HAVE_NVIDIA_NDA_SUBMODULE TRUE)
	endif()

	set(AMD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-AMD")
	if(EXISTS "${AMD_SRC_DIR}/RenderManagerAMDD3D.cpp")
		set(HAVE_AMD_NDA_SUBMODULE TRUE)
	endif()

	set(INTEL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-Intel")
	if(EXISTS "${INTEL_SRC_DIR}/RenderManagerIntelD3D.cpp")
		set(HAVE_INTEL_NDA_SUBMODULE TRUE)
	endif()
endif()

# Add one of these libraries for each vendor that we've got a driver
# for and add a No driver that tells that it is unavailable when we
# don't find the driver library.
find_package(nvapi COMPONENTS NDA)
find_package(liquidvr)
find_package(OpenGL)
find_package(OpenGLES2)
find_package(GLEW)
find_package(SDL2)
if(WIN32)
	# Well, redistributables technically, not tools, but close enough.
	find_package(WindowsSDK REQUIRED COMPONENTS tools)
	# Find redistributable version of d3dcompiler_47 which is required for pre-Win8 systems.
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_DIR x64)
	else()
		set(ARCH_DIR x86)
	endif()
	find_file(DIRECT3D_COMPILER_REDISTRIBUTABLE d3dcompiler_47.dll
		PATH_SUFFIXES Redist/D3D/${ARCH_DIR}
		PATHS ${WINDOWSSDK_DIRS}
		NO_DEFAULT_PATH)
endif()

# Set the debug postfix default
if(WIN32 AND NOT DEFINED CMAKE_DEBUG_POSTFIX)
	set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "A string to append to the end of target filenames when built in debug mode.")
endif()

# These configurations have the tools installed.
set(TOOL_CONFIGURATIONS Release RelWithDebInfo MinSizeRel)

# Finally, vendored dependencies
add_subdirectory(vendor)
include_directories(${VRPN_INCLUDE_DIRS})

#-----------------------------------------------------------------------------
# Open (non-NDA) files
set (RenderManager_SOURCES
	osvr/RenderKit/RenderManagerBase.cpp
	osvr/RenderKit/RenderManagerC.cpp
	osvr/RenderKit/RenderKitGraphicsTransforms.cpp
	osvr/RenderKit/osvr_display_configuration.cpp
	osvr/RenderKit/DistortionParameters.cpp
	osvr/RenderKit/CleanPNPIDString.h
	osvr/RenderKit/DirectModeVendors.h
	osvr/RenderKit/VendorIdTools.h
	osvr/RenderKit/osvr_display_config_built_in_osvr_hdks.h
	osvr/RenderKit/DistortionCorrectTextureCoordinate.h
	osvr/RenderKit/DistortionParameters.h
	osvr/RenderKit/UnstructuredMeshInterpolator.cpp
	osvr/RenderKit/UnstructuredMeshInterpolator.h
	osvr/RenderKit/ComputeDistortionMesh.cpp
	osvr/RenderKit/ComputeDistortionMesh.h
	osvr/RenderKit/DistortionMesh.h
	osvr/RenderKit/Float2.h
)

if (WIN32)
	list(APPEND RenderManager_SOURCES
		osvr/RenderKit/RenderManagerD3D11C.cpp
		osvr/RenderKit/RenderManagerD3DBase.cpp
		osvr/RenderKit/RenderManagerD3D.cpp
		osvr/RenderKit/RenderManagerD3DBase.h
		osvr/RenderKit/RenderManagerD3D.h
		osvr/RenderKit/RenderManagerD3D11ATW.h)
endif()

###
# Graphics API support
###

set(OSVRRM_IMPL_COMMON_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" ${EIGEN3_INCLUDE_DIR} "$<TARGET_PROPERTY:osvr::osvrClient,INTERFACE_INCLUDE_DIRECTORIES>")
function(osvrrm_setup_nda_object_target _target)
	# Set up defines and includes as if we were building as a part of the main rendermanager library
	target_compile_definitions(${_target} PRIVATE osvrRenderManager_EXPORTS)
	target_include_directories(${_target} PRIVATE "${NVAPI_INCLUDE_DIRS}" ${OSVRRM_IMPL_COMMON_INCLUDE_DIRS})

	# Add the objects to the main library sources.
	list(APPEND RenderManager_SOURCES $<TARGET_OBJECTS:${_target}>)
	set(RenderManager_SOURCES "${RenderManager_SOURCES}" PARENT_SCOPE)
endfunction()

# How to build the NDA modules in "Debug" mode (since MSVC has a separate debug runtime) without potentially NDA-violating debug symbols
option(BUILD_WITH_NDA_DEBUG_SYMBOLS "Should the DEBUG configuration contain symbols for the NDA components?" OFF)

set(_debug_flags_regex "(/Z[io07I)|(/Yd)|(/d2Zi[+])")
if(MSVC AND NOT BUILD_WITH_NDA_DEBUG_SYMBOLS)
	macro(osvrrm_no_symbols_in_debug)
		# Backup the original flags
		set(_osvrrm_debug_flags_bak "${CMAKE_CXX_FLAGS_DEBUG}")
		set(_osvrrm_relwithdebinfo_flags_bak "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
		string(REGEX REPLACE "${_debug_flags_regex}" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REGEX REPLACE "${_debug_flags_regex}" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
		#message(STATUS "Old CMAKE_CXX_FLAGS_DEBUG: '${_osvrrm_debug_flags_bak}'  New: '${CMAKE_CXX_FLAGS_DEBUG}'")
		#message(STATUS "Old CMAKE_CXX_FLAGS_RELWITHDEBINFO: '${_osvrrm_relwithdebinfo_flags_bak}'  New: '${CMAKE_CXX_FLAGS_RELWITHDEBINFO}'")
	endmacro()
	macro(osvrrm_restore_symbols_in_debug)
		set(_osvrrm_flags_bak "${CMAKE_CXX_FLAGS_DEBUG}")
	endmacro()
else()
	# no-ops
	macro(osvrrm_no_symbols_in_debug)
	endmacro()
	macro(osvrrm_restore_symbols_in_debug)
	endmacro()
endif()

set(OSVRRM_HAVE_OPENGL_SUPPORT OFF)
set(OSVRRM_HAVE_D3D11_SUPPORT OFF)
set(OSVRRM_USE_OPENGLES20 OFF)
if (WIN32)
	set(OSVRRM_HAVE_D3D11_SUPPORT ON)
	set(RM_USE_D3D11 TRUE)
	message(STATUS " - D3D11 support: enabled (found WIN32)")
endif()

#-----------------------------------------------------------------------------
# OpenGL library as a stand-alone renderer not wrapping D3D
if ( ( (OPENGL_FOUND AND GLEW_FOUND) OR OPENGLES2_FOUND ) AND (SDL2_FOUND OR ANDROID))
	list(APPEND RenderManager_SOURCES osvr/RenderKit/RenderManagerOpenGL.cpp osvr/RenderKit/RenderManagerOpenGL.h osvr/RenderKit/RenderManagerOpenGLC.cpp)
	message(STATUS " - OpenGL support: enabled")
	set(RM_USE_OPENGL TRUE)
	set(OSVRRM_HAVE_OPENGL_SUPPORT ON)
	if (OPENGLES2_FOUND AND ANDROID)
		message(STATUS " - OpenGLES2 support: enabled")
		set(RM_USE_OPENGLES20 TRUE)
		set(OSVRRM_USE_OPENGLES20 ON)
	endif()
else()
	message(STATUS " - OpenGL support: disabled)")
endif()

if(RM_USE_OPENGL)
	list(APPEND OSVRRM_IMPL_COMMON_INCLUDE_DIRS ${OPENGL_INCLUDE_DIRS})
endif()

if(RM_USE_OPENGLES20)
	list(APPEND OSVRRM_IMPL_COMMON_INCLUDE_DIRS ${OPENGLES2_INCLUDE_DIR})
endif()

if (NVAPI_FOUND AND HAVE_NVIDIA_NDA_SUBMODULE)
	# Usage dependencies
	add_library(osvrRM-nvidia-requirements INTERFACE)
	target_link_libraries(osvrRM-nvidia-requirements INTERFACE nvapi)
	target_include_directories(osvrRM-nvidia-requirements INTERFACE "${NVIDIA_SRC_DIR}")

	# nVidia NDA files - object library so we can specify different debug compile flags.
	osvrrm_no_symbols_in_debug()
	add_library(osvrRenderManager-NVIDIA OBJECT
		"${NVIDIA_SRC_DIR}/RenderManagerNVidiaD3D.cpp"
		"${NVIDIA_SRC_DIR}/RenderManagerNVidiaD3D.h")
	target_include_directories(osvrRenderManager-NVIDIA PRIVATE "${NVAPI_INCLUDE_DIRS}")
	osvrrm_setup_nda_object_target(osvrRenderManager-NVIDIA)
	osvrrm_restore_symbols_in_debug()

	set(RM_USE_NVIDIA_DIRECT_D3D11 TRUE)
	message(STATUS " - NVIDIA direct D3D11 support: enabled (found NVAPI and NVIDIA NDA submodule)")
else()
	message(STATUS " - NVIDIA direct support: disabled (need NVAPI and NVIDIA NDA submodule)")
endif()

if (LIQUIDVR_FOUND AND HAVE_AMD_NDA_SUBMODULE)
	# Usage dependencies
	add_library(osvrRM-amd-requirements INTERFACE)
	target_link_libraries(osvrRM-amd-requirements INTERFACE liquidvr)
	target_include_directories(osvrRM-amd-requirements INTERFACE "${AMD_SRC_DIR}")

	# AMD NDA files - object library so we can specify different debug compile flags.
	osvrrm_no_symbols_in_debug()
	add_library(osvrRenderManager-AMD OBJECT
		"${AMD_SRC_DIR}/RenderManagerAMDD3D.cpp"
		"${AMD_SRC_DIR}/RenderManagerAMDD3D.h")
	target_include_directories(osvrRenderManager-AMD PRIVATE "${LIQUIDVR_INCLUDE_DIR}")
	osvrrm_setup_nda_object_target(osvrRenderManager-AMD)
	osvrrm_restore_symbols_in_debug()

	set(RM_USE_AMD_DIRECT_D3D11 TRUE)
	message(STATUS " - AMD direct D3D11 support: enabled (found LIQUIDVR and AMD NDA submodule)")
else()
	message(STATUS " - AMD direct support: disabled (need LIQUIDVR and AMD NDA submodule)")
endif()

if (HAVE_INTEL_NDA_SUBMODULE)
	# Usage dependencies
	add_library(osvrRM-intel-requirements INTERFACE)
	target_include_directories(osvrRM-intel-requirements INTERFACE "${INTEL_SRC_DIR}")
	# Intel NDA files - object library so we can specify different debug compile flags.
	osvrrm_no_symbols_in_debug()
	add_library(osvrRenderManager-Intel OBJECT
		"${INTEL_SRC_DIR}/RenderManagerIntelD3D.cpp"
		"${INTEL_SRC_DIR}/RenderManagerIntelD3D.h")
	target_include_directories(osvrRenderManager-Intel PRIVATE "${INTEL_SRC_DIR}")
	osvrrm_setup_nda_object_target(osvrRenderManager-Intel)
	osvrrm_restore_symbols_in_debug()

	set(RM_USE_INTEL_DIRECT_D3D11 TRUE)
	message(STATUS " - Intel direct D3D11 support: enabled (found Intel NDA submodule)")
else()
	message(STATUS " - Intel direct support: disabled (need Intel NDA submodule)")
endif()

#-----------------------------------------------------------------------------
# SDL init/quit handler code that makes sure we only do it once.
if (SDL2_FOUND)
	list(APPEND RenderManager_SOURCES osvr/RenderKit/RenderManagerSDLInitQuit.cpp osvr/RenderKit/RenderManagerSDLInitQuit.h )
endif()

#-----------------------------------------------------------------------------
# OpenGL wrapped around Direct3D
if ((RM_USE_NVIDIA_DIRECT_D3D11 OR RM_USE_AMD_DIRECT_D3D11) AND NOT RM_USE_OPENGLES20)
	#-----------------------------------------------------------------------------
	# OpenGL library as a wrapper for D3D DirectMode
	if (OPENGL_FOUND AND GLEW_FOUND AND SDL2_FOUND)
		message(STATUS " - D3D11+OpenGL support: enabled (found NVAPI or LIQUIDVR, OpenGL, GLEW, and SDL2)")
		list(APPEND RenderManager_SOURCES osvr/RenderKit/RenderManagerD3DOpenGL.cpp osvr/RenderKit/RenderManagerD3DOpenGL.h)
		set(RM_USE_NVIDIA_DIRECT_D3D11_OPENGL TRUE)
	else()
		message(STATUS " - Vendor direct D3D11+OpenGL support: disabled (need all of a vendor direct-mode D3D module, OpenGL, GLEW, and SDL2, at least one was missing)")
	endif()
endif()

###
# Set up build product locations
###
include(GNUInstallDirs)
# Sometimes GNUInstallDirs misses this one.
if(NOT CMAKE_INSTALL_DOCDIR)
	set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME})
endif()

# Win-specific: we want shared libs (dlls) in same dir as exe files.
if(WIN32)
	set(OSVRRM_SHARED_LIBRARY_DIR "${CMAKE_INSTALL_BINDIR}")
else()
	set(OSVRRM_SHARED_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}")
endif()

# Let's build into a parallel(ish) structure as we'll install to.
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${OSVRRM_SHARED_LIBRARY_DIR}")
endif()
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
endif()

set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/osvrRenderManager")


###
# Helper for dependencies
###
include(CopyImportedTarget)
## Copy and install shared libraries from imported targets as required.
## Also generate a list of imported targets for clients of RM to make use of.
get_filename_component(OSVRRM_REQUIRED_LIBRARIES_DIR "${OSVRRM_SHARED_LIBRARY_DIR}" ABSOLUTE BASE_DIR "${CMAKE_INSTALL_PREFIX}")
get_filename_component(OSVRRM_CONFIG_INSTALL_DIR "${CONFIG_INSTALL_DIR}" ABSOLUTE BASE_DIR "${CMAKE_INSTALL_PREFIX}")
file(RELATIVE_PATH OSVRRM_REQUIRED_LIBRARIES_DIR "${OSVRRM_CONFIG_INSTALL_DIR}" "${OSVRRM_REQUIRED_LIBRARIES_DIR}/.")

set(OSVRRM_REQUIRED_LIBRARIES_DEBUG)
set(OSVRRM_REQUIRED_LIBRARIES_RELEASE)
set(OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO)
set(OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL)
function(osvrrm_copy_deps)
	copy_imported_targets(osvrRenderManager ${ARGN})
	foreach(_dep ${ARGN})
		install_imported_target(${_dep} DESTINATION ${OSVRRM_SHARED_LIBRARY_DIR} COMPONENT Runtime)
		foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
			get_target_property(_lib_location ${_dep} LOCATION_${_config})
			get_filename_component(_lib_filename "${_lib_location}" NAME)
			set(_lib_filename "${OSVRRM_REQUIRED_LIBRARIES_DIR}/${_lib_filename}")
			list(APPEND OSVRRM_REQUIRED_LIBRARIES_${_config} "${_lib_filename}")
			set(OSVRRM_REQUIRED_LIBRARIES_${_config} "${OSVRRM_REQUIRED_LIBRARIES_${_config}}" PARENT_SCOPE)
		endforeach()
	endforeach()
endfunction()

###
# Build the actual library
###

# Generate the header with the defines we need.
configure_file(RenderManagerBackends.h.in "${CMAKE_CURRENT_BINARY_DIR}/RenderManagerBackends.h")

set (RenderManager_PUBLIC_HEADERS
	osvr/RenderKit/Float2.h
	osvr/RenderKit/UnstructuredMeshInterpolator.h
	osvr/RenderKit/ComputeDistortionMesh.h
	osvr/RenderKit/DistortionCorrectTextureCoordinate.h
	osvr/RenderKit/DistortionMesh.h
	osvr/RenderKit/DistortionParameters.h
	osvr/RenderKit/RenderManager.h
	osvr/RenderKit/RenderManagerC.h
	osvr/RenderKit/RenderManagerD3D11C.h
	osvr/RenderKit/RenderManagerOpenGLC.h
	osvr/RenderKit/RenderManagerOpenGLVersion.h
	osvr/RenderKit/GraphicsLibraryD3D11.h
	osvr/RenderKit/GraphicsLibraryOpenGL.h
	osvr/RenderKit/MonoPointMeshTypes.h
	osvr/RenderKit/RGBPointMeshTypes.h
	osvr/RenderKit/RenderKitGraphicsTransforms.h
	osvr/RenderKit/osvr_display_configuration.h
	osvr/RenderKit/osvr_compiler_tests.h
	"${CMAKE_CURRENT_BINARY_DIR}/osvr/RenderKit/Export.h"
)
add_library(osvrRenderManager ${RenderManager_SOURCES} ${RenderManager_PUBLIC_HEADERS})
if (NOT ANDROID)
  target_compile_features(osvrRenderManager PRIVATE cxx_range_for)
endif()
target_include_directories(osvrRenderManager PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE
	${EIGEN3_INCLUDE_DIR})
if (RM_USE_NVIDIA_DIRECT_D3D11)
	target_link_libraries(osvrRenderManager
		PRIVATE
		osvrRM-nvidia-requirements)
endif()
if (RM_USE_AMD_DIRECT_D3D11)
	target_link_libraries(osvrRenderManager
		PRIVATE
		osvrRM-amd-requirements)
endif()
if (RM_USE_INTEL_DIRECT_D3D11)
	target_link_libraries(osvrRenderManager
		PRIVATE
		osvrRM-intel-requirements)
endif()
if (WIN32)
	target_link_libraries(osvrRenderManager PRIVATE D3D11)
endif()

set(LIBNAME_FULL osvrRenderManager)
set(EXPORT_BASENAME OSVR_RENDERMANAGER)
configure_file("Export.h.in"
	osvr/RenderKit/Export.h
	@ONLY NEWLINE_STYLE LF)

if(NOT BUILD_SHARED_LIBS)
	target_compile_definitions(osvrRenderManager PUBLIC OSVR_RENDERMANAGER_STATIC_DEFINE)
endif()

set_property(TARGET
	osvrRenderManager
	PROPERTY
	PUBLIC_HEADER
	${RenderManager_PUBLIC_HEADERS})

# If we are using dynamic GLEW on Windows, let's copy it.
if(GLEW_FOUND AND WIN32 AND NOT GLEW_LIBRARY MATCHES ".*s.lib")
	osvrrm_copy_deps(GLEW::GLEW)
	set(OSVRRM_NEED_GLEW_COPIED TRUE)
else()
	set(OSVRRM_NEED_GLEW_COPIED FALSE)
endif()

if (OPENGL_FOUND)
	target_include_directories(osvrRenderManager PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(osvrRenderManager PRIVATE ${OPENGL_LIBRARY})
endif()

if (ANDROID)
	target_include_directories(osvrRenderManager PRIVATE ${OPENGLES2_INCLUDE_DIR})
	target_link_libraries(osvrRenderManager PRIVATE ${OPENGLES2_LIBRARIES})
	target_link_libraries(osvrRenderManager PRIVATE GLESv1_CM log android)
endif()

if (GLEW_FOUND)
	target_link_libraries(osvrRenderManager PRIVATE GLEW::GLEW)
endif()

if (SDL2_FOUND)
	target_link_libraries(osvrRenderManager PRIVATE SDL2::SDL2)
	set(OSVRRM_NEED_SDL2 TRUE)
else()
	set(OSVRRM_NEED_SDL2 FALSE)
endif()

if(SDL2_DYNAMIC AND WIN32)
	osvrrm_copy_deps(SDL2::SDL2)
	set(OSVRRM_NEED_SDL2_COPIED TRUE)
else()
	set(OSVRRM_NEED_SDL2_COPIED FALSE)
endif()

# This also lets it know where to find the header files.
target_link_libraries(osvrRenderManager
	PUBLIC
	osvr::osvrClientKitCpp
	PRIVATE
	JsonCpp::JsonCpp
	osvr::osvrClient
	osvr::osvrUtil
	vendored-vrpn
	vendored-quat)
osvrrm_copy_deps(osvr::osvrClientKit osvr::osvrClient osvr::osvrCommon osvr::osvrUtil)

# Add the C++ interface target.
add_library(osvrRenderManagerCpp INTERFACE)
target_link_libraries(osvrRenderManagerCpp INTERFACE osvrRenderManager osvr::osvrClientKitCpp)

# Alias targets, so the examples can be used as-is.
add_library(osvrRenderManager::osvrRenderManager ALIAS osvrRenderManager)
add_library(osvrRenderManager::osvrRenderManagerCpp ALIAS osvrRenderManagerCpp)

if(WIN32)
	# Install d3dcompiler_47
	install(FILES ${DIRECT3D_COMPILER_REDISTRIBUTABLE}
		DESTINATION ${CMAKE_INSTALL_BINDIR})
	get_filename_component(OSVRRM_D3DCOMPILER_FILENAME "${DIRECT3D_COMPILER_REDISTRIBUTABLE}" NAME)
	set(OSVRRM_NEED_D3DCOMPILER_COPIED TRUE)
else()
	set(OSVRRM_NEED_D3DCOMPILER_COPIED FALSE)
endif()

# The goal with this variable is so that the same CMakeLists file that is used to build
# the examples in-tree can be shipped to build the examples out-of-tree.
set(OSVRRM_INSTALL_EXAMPLES ON)

if(NOT ANDROID)
	add_subdirectory(examples)
endif()

install(TARGETS
	osvrRenderManager
	${OSVR_RENDERMANAGER_EXPORTED_TARGETS}
	EXPORT osvrRenderManagerTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	PUBLIC_HEADER DESTINATION include/osvr/RenderKit
)

if (NVAPI_FOUND AND HAVE_NVIDIA_NDA_SUBMODULE)
	set(NVAPI_EXTRA_HEADERS "${NVIDIA_SRC_DIR}/CheckSuccess.h" "${NVIDIA_SRC_DIR}/Util.h" "${NVIDIA_SRC_DIR}/NVAPIWrappers.h")

	osvrrm_no_symbols_in_debug()
	#-----------------------------------------------------------------------------
	# Enable DirectMode on attached OSVR HDKs
	# TODO rename to EnableOSVRDirectModeNVIDIA
	add_executable(EnableOSVRDirectMode "${NVIDIA_SRC_DIR}/EnableOSVRDirectMode.cpp" ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(EnableOSVRDirectMode PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-nvidia-requirements)

	#-----------------------------------------------------------------------------
	# Disable DirectMode on attached OSVR HDKs
	# TODO rename to DisableOSVRDirectModeNVIDIA
	add_executable(DisableOSVRDirectMode "${NVIDIA_SRC_DIR}/DisableOSVRDirectMode.cpp" ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(DisableOSVRDirectMode PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-nvidia-requirements)

	#-----------------------------------------------------------------------------
	# Debugging/troubleshooting application for direct mode.
	add_executable(DirectModeDebugging "${NVIDIA_SRC_DIR}/DirectModeDebugging.cpp" ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(DirectModeDebugging PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-nvidia-requirements)
	osvrrm_restore_symbols_in_debug()

	install(TARGETS
		EnableOSVRDirectMode
		DisableOSVRDirectMode
		DirectModeDebugging
		EXPORT osvrRenderManagerTargets
		RUNTIME
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		CONFIGURATIONS ${TOOL_CONFIGURATIONS})
endif()

if (LIQUIDVR_FOUND AND HAVE_AMD_NDA_SUBMODULE)

	osvrrm_no_symbols_in_debug()
	#-----------------------------------------------------------------------------
	# Enable DirectMode on attached OSVR HDKs
	add_executable(EnableOSVRDirectModeAMD "${AMD_SRC_DIR}/EnableOSVRDirectModeAMD.cpp")
	target_link_libraries(EnableOSVRDirectModeAMD PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-amd-requirements)

	#-----------------------------------------------------------------------------
	# Disable DirectMode on attached OSVR HDKs
	add_executable(DisableOSVRDirectModeAMD "${AMD_SRC_DIR}/DisableOSVRDirectModeAMD.cpp")
	target_link_libraries(DisableOSVRDirectModeAMD PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-amd-requirements)
	osvrrm_restore_symbols_in_debug()

	install(TARGETS
		EnableOSVRDirectModeAMD
		DisableOSVRDirectModeAMD
		EXPORT osvrRenderManagerTargets
		RUNTIME
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		CONFIGURATIONS ${TOOL_CONFIGURATIONS})
endif()

###
# Set Up Package for External Usage
###
include(CMakePackageConfigHelpers)
include(GenerateCompatibilityVersionFile)

generate_compatibility_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerConfigVersion.cmake"
	VERSION ${OSVR_RENDERMANAGER_VERSION}
	COMPATIBILITY AnyNewerVersion
	C_ABI)

generate_compatibility_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerInternalConfigVersion.cmake"
	VERSION ${OSVR_RENDERMANAGER_VERSION}
	COMPATIBILITY AnyNewerVersion
	CXX_ABI)

# Export the targets we want in the build tree.
export(TARGETS
	osvrRenderManager
	${OSVR_RENDERMANAGER_EXPORTED_TARGETS}
	FILE "${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerTargets.cmake"
	NAMESPACE osvrRenderManager::
)

# Register the current build dir as a package search location
export(PACKAGE osvrRenderManager)

# Generate the config file for build-tree usage.
set(OSVRRM_IN_BUILD_TREE TRUE)
set(OSVRRM_D3DCOMPILER_FILE "${DIRECT3D_COMPILER_REDISTRIBUTABLE}")
configure_file("cmake/osvrRenderManagerConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerConfig.cmake"
	@ONLY)

# Path hints for build-tree usage only.
configure_file("cmake/osvrRenderManagerConfigBuildTreeHints.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerConfigBuildTreeHints.cmake"
	@ONLY)


# Generate the config file for installed usage.
set(OSVRRM_IN_BUILD_TREE FALSE)
set(OSVRRM_D3DCOMPILER_FILE "\${OSVRRM_ROOT}/${CMAKE_INSTALL_BINDIR}/${OSVRRM_D3DCOMPILER_FILENAME}")
configure_file("cmake/osvrRenderManagerConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerConfigInstallTree.cmake"
	@ONLY)

# Copy over the shared CMake modules for use in a build tree,
# and install for an install tree.
foreach(HELPER CMakeBoostHelper FindSDL2 FindGLEW CopyImportedTarget osvrStashMapConfig)
	configure_file("cmake/${HELPER}.cmake" "${CMAKE_CURRENT_BINARY_DIR}/${HELPER}.cmake" COPYONLY)
	install(FILES
		"cmake/${HELPER}.cmake"
		DESTINATION ${CONFIG_INSTALL_DIR}
		COMPONENT Devel)
endforeach()

if(WIN32)
	# Needed by CMakeBoostHelper
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GetMSVCVersion.cmake" "${CMAKE_CURRENT_BINARY_DIR}/GetMSVCVersion.cmake" COPYONLY)
	install(FILES
		"cmake/GetMSVCVersion.cmake"
		DESTINATION ${CONFIG_INSTALL_DIR}
		COMPONENT Devel)
endif()

install(EXPORT osvrRenderManagerTargets
	FILE osvrRenderManagerTargets.cmake
	NAMESPACE osvrRenderManager::
	DESTINATION ${CONFIG_INSTALL_DIR}
	COMPONENT Devel)

# Install an install-tree only helper and a generated file
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerConfigVersion.cmake"
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/osvrRenderManagerConfigInstalledBoost.cmake"
	DESTINATION ${CONFIG_INSTALL_DIR}
	COMPONENT Devel)

# Install the install-tree version of the CMake config file.
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/osvrRenderManagerConfigInstallTree.cmake"
	RENAME osvrRenderManagerConfig.cmake
	DESTINATION ${CONFIG_INSTALL_DIR}
	COMPONENT Devel)

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
	set(HEADER_BASE "${CMAKE_CURRENT_SOURCE_DIR}/osvr")
	add_subdirectory(tests)
endif()
