cmake_minimum_required(VERSION 3.1.0)
project(osvrRenderManager)

#-----------------------------------------------------------------------------
# Local CMake Modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#-----------------------------------------------------------------------------
# Export libraries along with our DLLs if we want to build shared
# Allow the developer to select if Dynamic or Static libraries are built

include (GenerateExportHeader)

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)

#-----------------------------------------------------------------------------
# This looks for an osvrConfig.cmake file - most of the time it can be
# autodetected but you might need to specify osvr_DIR to be something like
# C:/Users/Ryan/Desktop/build/OSVR-Core-vc12 or
# C:/Users/Ryan/Downloads/OSVR-Core-Snapshot-v0.1-406-gaa55515-build54-vs12-32bit
# in the CMake GUI or command line
find_package(osvr REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(JsonCpp REQUIRED)

# Check for the submodules
set(NVIDIA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-NVIDIA")
set(HAVE_NVIDIA_NDA_SUBMODULE FALSE)
if(EXISTS "${NVIDIA_SRC_DIR}/RenderManagerNVidiaD3D.cpp")
	set(HAVE_NVIDIA_NDA_SUBMODULE TRUE)
endif()

set(AMD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-AMD")
set(HAVE_AMD_NDA_SUBMODULE FALSE)
if(EXISTS "${AMD_SRC_DIR}/RenderManagerAMDD3D.cpp")
	set(HAVE_AMD_NDA_SUBMODULE TRUE)
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
	osvr/RenderKit/VendorIdTools.h
  osvr/RenderKit/osvr_display_config_built_in_osvr_hdks.h
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

set(OSVRRM_HAVE_OPENGL_SUPPORT OFF)
set(OSVRRM_HAVE_D3D11_SUPPORT OFF)
if (WIN32)
	set(OSVRRM_HAVE_D3D11_SUPPORT ON)
	set(RM_USE_D3D11 TRUE)
	message(STATUS " - D3D11 support: enabled (found WIN32)")
endif()

if (NVAPI_FOUND AND HAVE_NVIDIA_NDA_SUBMODULE)
	# Usage dependencies
	add_library(osvrRM-nvidia-requirements INTERFACE)
	target_link_libraries(osvrRM-nvidia-requirements INTERFACE nvapi)
	target_include_directories(osvrRM-nvidia-requirements INTERFACE "${NVIDIA_SRC_DIR}")
	# nVidia NDA files.
	list(APPEND RenderManager_SOURCES
		"${NVIDIA_SRC_DIR}/RenderManagerNVidiaD3D.cpp"
		"${NVIDIA_SRC_DIR}/RenderManagerNVidiaD3D.h")
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
	# AMD NDA files.
	list(APPEND RenderManager_SOURCES
		"${AMD_SRC_DIR}/RenderManagerAMDD3D.cpp"
		"${AMD_SRC_DIR}/RenderManagerAMDD3D.h")
	set(RM_USE_AMD_DIRECT_D3D11 TRUE)
	message(STATUS " - AMD direct D3D11 support: enabled (found LIQUIDVR and AMD NDA submodule)")
else()
	message(STATUS " - AMD direct support: disabled (need LIQUIDVR and AMD NDA submodule)")
endif()

#-----------------------------------------------------------------------------
# OpenGL library as a stand-alone renderer not wrapping D3D
if ( ( (OPENGL_FOUND AND GLEW_FOUND) OR OPENGLES2_FOUND ) AND SDL2_FOUND)
	list(APPEND RenderManager_SOURCES osvr/RenderKit/RenderManagerOpenGL.cpp osvr/RenderKit/RenderManagerOpenGL.h osvr/RenderKit/RenderManagerOpenGLC.cpp)
	message(STATUS " - OpenGL support: enabled")
	set(RM_USE_OPENGL TRUE)
	set(OSVRRM_HAVE_OPENGL_SUPPORT ON)
    if (OPENGLES2_FOUND AND ANDROID)
		message(STATUS " - OpenGLES2 support: enabled")
		set(RM_USE_OPENGLES20 TRUE)
	endif()
else()
	message(STATUS " - OpenGL support: disabled)")
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

###
# Helper for dependencies
###
include(CopyImportedTarget)
## Copy and install shared libraries from imported targets as required
function(osvrrm_copy_deps)
	copy_imported_targets(osvrRenderManager ${ARGN})
	foreach(_dep ${ARGN})
		install_imported_target(${_dep} DESTINATION ${OSVRRM_SHARED_LIBRARY_DIR} COMPONENT Runtime)
	endforeach()
endfunction()

###
# Build the actual library
###

# Generate the header with the defines we need.
configure_file(RenderManagerBackends.h.in "${CMAKE_CURRENT_BINARY_DIR}/RenderManagerBackends.h")

set (RenderManager_PUBLIC_HEADERS
	osvr/RenderKit/RenderManager.h
	osvr/RenderKit/RenderManagerC.h
	osvr/RenderKit/RenderManagerD3D11C.h
	osvr/RenderKit/RenderManagerOpenGLC.h
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
endif()

if (OPENGL_FOUND)
	target_include_directories(osvrRenderManager PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(osvrRenderManager PRIVATE ${OPENGL_LIBRARY})
endif()

if (ANDROID)
	target_include_directories(osvrRenderManager PRIVATE ${OPENGLES2_INCLUDE_DIR})
	target_link_libraries(osvrRenderManager PRIVATE ${OPENGLES2_LIBRARIES})
	target_link_libraries(osvrRenderManager PRIVATE android)
endif()

if (GLEW_FOUND)
	target_link_libraries(osvrRenderManager PRIVATE GLEW::GLEW)
endif()

if (SDL2_FOUND)
	target_link_libraries(osvrRenderManager PRIVATE SDL2::SDL2)
endif()

if(SDL2_DYNAMIC AND WIN32)
	osvrrm_copy_deps(SDL2::SDL2)
endif()

# This also lets it know where to find the header files.
target_link_libraries(osvrRenderManager
	PUBLIC
	osvr::osvrClientKitCpp
	PRIVATE
	JsonCpp::JsonCpp
	osvr::osvrClient
	vendored-vrpn
	vendored-quat)
osvrrm_copy_deps(osvr::osvrClientKit osvr::osvrClient osvr::osvrCommon osvr::osvrUtil)

# Add the C++ interface target.
add_library(osvrRenderManagerCpp INTERFACE)
target_link_libraries(osvrRenderManagerCpp INTERFACE osvrRenderManager osvr::osvrClientKitCpp)

# Alias targets, so the examples can be used as-is.
add_library(osvrRM::osvrRenderManager ALIAS osvrRenderManager)
add_library(osvrRM::osvrRenderManagerCpp ALIAS osvrRenderManagerCpp)


if(WIN32)
	# Install d3dcompiler_47
	install(FILES ${DIRECT3D_COMPILER_REDISTRIBUTABLE}
		DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# The goal with this variable is so that the same CMakeLists file that is used to build
# the examples in-tree can be shipped to build the examples out-of-tree.
set(OSVRRM_INSTALL_EXAMPLES ON)
add_subdirectory(examples)

install(TARGETS
	osvrRenderManager
	EXPORT ${PROJECT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	PUBLIC_HEADER DESTINATION include/osvr/RenderKit
)

install(EXPORT
	${PROJECT_NAME}
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
	FILE ${PROJECT_NAME}Config.cmake
)

if (NVAPI_FOUND AND HAVE_NVIDIA_NDA_SUBMODULE)
	set(NVAPI_EXTRA_HEADERS "${NVIDIA_SRC_DIR}/CheckSuccess.h" "${NVIDIA_SRC_DIR}/Util.h" "${NVIDIA_SRC_DIR}/NVAPIWrappers.h")

	#-----------------------------------------------------------------------------
	# Enable DirectMode on attached OSVR HDKs
	add_executable(EnableOSVRDirectMode "${NVIDIA_SRC_DIR}/EnableOSVRDirectMode.cpp" ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(EnableOSVRDirectMode PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-nvidia-requirements)

	#-----------------------------------------------------------------------------
	# Disable DirectMode on attached OSVR HDKs
	add_executable(DisableOSVRDirectMode "${NVIDIA_SRC_DIR}/DisableOSVRDirectMode.cpp" ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(DisableOSVRDirectMode PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-nvidia-requirements)

	#-----------------------------------------------------------------------------
	# Debugging/troubleshooting application for direct mode.
	add_executable(DirectModeDebugging "${NVIDIA_SRC_DIR}/DirectModeDebugging.cpp" ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(DirectModeDebugging PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-nvidia-requirements)

	install(TARGETS EnableOSVRDirectMode DisableOSVRDirectMode DirectModeDebugging RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if (LIQUIDVR_FOUND AND HAVE_AMD_NDA_SUBMODULE)

	#-----------------------------------------------------------------------------
	# Enable DirectMode on attached OSVR HDKs
	add_executable(EnableOSVRDirectModeAMD "${AMD_SRC_DIR}/EnableOSVRDirectModeAMD.cpp")
	target_link_libraries(EnableOSVRDirectModeAMD PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-amd-requirements)

	#-----------------------------------------------------------------------------
	# Disable DirectMode on attached OSVR HDKs
	add_executable(DisableOSVRDirectModeAMD "${AMD_SRC_DIR}/DisableOSVRDirectModeAMD.cpp")
	target_link_libraries(DisableOSVRDirectModeAMD PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp osvrRM-amd-requirements)

	install(TARGETS EnableOSVRDirectModeAMD DisableOSVRDirectModeAMD RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
