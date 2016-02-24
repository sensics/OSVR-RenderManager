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
find_package(jsoncpp REQUIRED)
if(TARGET jsoncpp_lib_static AND NOT TARGET jsoncpp_lib)
	add_library(jsoncpp_lib INTERFACE)
	target_link_libraries(jsoncpp_lib INTERFACE jsoncpp_lib_static)
endif()

include_directories("${EIGEN3_INCLUDE_DIR}")

# vendored dependencies
add_subdirectory(vendor)
include_directories(${VRPN_INCLUDE_DIRS})

# Add one of these libraries for each vendor that we've got a driver
# for and add a No driver that tells that it is unavailable when we
# don't find the driver library.
find_package(nvapi COMPONENTS NDA)
find_package(liquidvr)
find_package(OpenGL)
find_package(OpenGLES2)
find_package(GLEW)
find_package(SDL2)
if (WIN32)
	set(RM_USE_D3D11 TRUE)
	message(STATUS " - D3D11 support: enabled (found WIN32)")
endif()

#-----------------------------------------------------------------------------
# Open (non-NDA) files
set (RenderManager_SOURCES
	osvr/RenderKit/ComputeATW.h
	osvr/RenderKit/RenderManagerBase.cpp
	osvr/RenderKit/RenderManagerC.cpp
	osvr/RenderKit/RenderKitGraphicsTransforms.cpp
	osvr/RenderKit/osvr_display_configuration.cpp
	osvr/RenderKit/VendorIdTools.h
)

if (WIN32)
	list(APPEND RenderManager_SOURCES
	osvr/RenderKit/RenderManagerD3D11C.cpp
	osvr/RenderKit/RenderManagerD3DBase.cpp
	osvr/RenderKit/RenderManagerD3D.cpp
	osvr/RenderKit/RenderManagerD3DBase.h
	osvr/RenderKit/RenderManagerD3D.h
	osvr/RenderKit/RenderManagerD3D11ATW.h
)
endif()

###
# Graphics API support
###
set(OSVRRM_HAVE_OPENGL_SUPPORT OFF)
set(OSVRRM_HAVE_D3D11_SUPPORT OFF)
if (WIN32)
	set(OSVRRM_HAVE_D3D11_SUPPORT ON)
endif()

if (NVAPI_FOUND)
	# nVidia NDA files.
	list(APPEND RenderManager_SOURCES osvr/RenderKit/NDA/OSVR-RenderManager-NVIDIA/RenderManagerNVidiaD3D.cpp osvr/RenderKit/NDA/OSVR-RenderManager-NVIDIA/RenderManagerNVidiaD3D.h)
	set(RM_USE_NVIDIA_DIRECT_D3D11 TRUE)
	message(STATUS " - NVIDIA direct D3D11 support: enabled (found NVAPI)")
else()
	message(STATUS " - NVIDIA direct support: disabled (need NVAPI)")
endif()

if (LIQUIDVR_FOUND)
	# AMD NDA files.
	list(APPEND RenderManager_SOURCES osvr/RenderKit/NDA/OSVR-RenderManager-AMD/RenderManagerAMDD3D.cpp osvr/RenderKit/NDA/OSVR-RenderManager-AMD/RenderManagerAMDD3D.h)
	set(RM_USE_AMD_DIRECT_D3D11 TRUE)
	message(STATUS " - AMD direct D3D11 support: enabled (found LIQUIDVR)")
else()
	message(STATUS " - AMD direct support: disabled (need LIQUIDVR)")
endif()

#-----------------------------------------------------------------------------
# OpenGL library as a stand-alone renderer not wrapping D3D
if ( ( (OPENGL_FOUND AND GLEW_FOUND) OR OPENGLES2_FOUND ) AND SDL2_FOUND)
	list(APPEND RenderManager_SOURCES osvr/RenderKit/RenderManagerOpenGL.cpp osvr/RenderKit/RenderManagerOpenGL.h osvr/RenderKit/RenderManagerOpenGLC.cpp)
	message(STATUS " - OpenGL support: enabled")
	set(RM_USE_OPENGL TRUE)
	set(OSVRRM_HAVE_OPENGL_SUPPORT ON)
        if (OPENGLES2_FOUND)
		message(STATUS " - OpenGLES2 support: enabled")
		set(RM_USE_OPENGLES20 TRUE)
	endif()
else()
	message(STATUS " - OpenGL support: disabled)")
endif()

#-----------------------------------------------------------------------------
# OpenGL wrapped around Direct3D
if (NVAPI_FOUND OR LIQUIDVR_FOUND AND NOT RM_USE_OPENGLES20)
	#-----------------------------------------------------------------------------
	# OpenGL library as a wrapper for D3D DirectMode
	if (OPENGL_FOUND AND GLEW_FOUND AND SDL2_FOUND)
		message(STATUS " - D3D11+OpenGL support: enabled (found NVAPI or LIQUIDVR, OpenGL, GLEW, and SDL2)")
		list(APPEND RenderManager_SOURCES osvr/RenderKit/RenderManagerD3DOpenGL.cpp osvr/RenderKit/RenderManagerD3DOpenGL.h)
		set(RM_USE_NVIDIA_DIRECT_D3D11_OPENGL TRUE)
	else()
		message(STATUS " - NVIDIA direct D3D11+OpenGL support: disabled (need all of NVAPI, OpenGL, GLEW, and SDL2, at least one was missing)")
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
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if (NVAPI_FOUND)
	target_link_libraries(osvrRenderManager PRIVATE nvapi)
	target_include_directories(osvrRenderManager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-NVIDIA")
endif()
if (LIQUIDVR_FOUND)
	target_include_directories(osvrRenderManager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/osvr/RenderKit/NDA/OSVR-RenderManager-AMD")
endif()
if (WIN32)
	target_link_libraries(osvrRenderManager PRIVATE D3D11)
endif()

generate_export_header(osvrRenderManager
	BASE_NAME OSVR_RENDERMANAGER
	EXPORT_FILE_NAME osvr/RenderKit/Export.h)

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

if (OPENGLES2_FOUND)
	target_include_directories(osvrRenderManager PRIVATE ${OPENGLES2_INCLUDE_DIR})
	target_link_libraries(osvrRenderManager PRIVATE ${OPENGLES2_LIBRARIES})
	# @todo Assuming for now we're on Android if we are using GLES
	message(STATUS "GLES Found, assuming compile for Android")
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
target_link_libraries(osvrRenderManager PUBLIC osvr::osvrClientKitCpp PRIVATE osvr::osvrClient jsoncpp_lib vendored-vrpn vendored-quat)
osvrrm_copy_deps(osvr::osvrClientKit osvr::osvrClient osvr::osvrCommon osvr::osvrUtil)

# Add the C++ interface target.
add_library(osvrRenderManagerCpp INTERFACE)
target_link_libraries(osvrRenderManagerCpp INTERFACE osvrRenderManager osvr::osvrClientKitCpp)

# Alias targets, so the examples can be used as-is.
add_library(osvrRM::osvrRenderManager ALIAS osvrRenderManager)
add_library(osvrRM::osvrRenderManagerCpp ALIAS osvrRenderManagerCpp)

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

if (NVAPI_FOUND)
	set(NV_SRC_ROOT osvr/RenderKit/NDA/OSVR-RenderManager-NVIDIA)
	set(NVAPI_EXTRA_HEADERS ${NV_SRC_ROOT}/CheckSuccess.h ${NV_SRC_ROOT}/Util.h ${NV_SRC_ROOT}/NVAPIWrappers.h)

	#-----------------------------------------------------------------------------
	# Enable DirectMode on attached OSVR HDKs
	add_executable(EnableOSVRDirectMode ${NV_SRC_ROOT}/EnableOSVRDirectMode.cpp ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(EnableOSVRDirectMode PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp nvapi)

	#-----------------------------------------------------------------------------
	# Disable DirectMode on attached OSVR HDKs
	add_executable(DisableOSVRDirectMode ${NV_SRC_ROOT}/DisableOSVRDirectMode.cpp ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(DisableOSVRDirectMode PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp nvapi)

	#-----------------------------------------------------------------------------
	# Debugging/troubleshooting application for direct mode.
	add_executable(DirectModeDebugging ${NV_SRC_ROOT}/DirectModeDebugging.cpp ${NVAPI_EXTRA_HEADERS})
	target_link_libraries(DirectModeDebugging PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp nvapi)

	install(TARGETS EnableOSVRDirectMode DisableOSVRDirectMode DirectModeDebugging RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if (LIQUIDVR_FOUND)
	set(AMD_SRC_ROOT osvr/RenderKit/NDA/OSVR-RenderManager-AMD)

	#-----------------------------------------------------------------------------
	# Enable DirectMode on attached OSVR HDKs
	add_executable(EnableOSVRDirectModeAMD ${AMD_SRC_ROOT}/EnableOSVRDirectModeAMD.cpp)
	target_link_libraries(EnableOSVRDirectModeAMD PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp)

	#-----------------------------------------------------------------------------
	# Disable DirectMode on attached OSVR HDKs
	add_executable(DisableOSVRDirectModeAMD ${AMD_SRC_ROOT}/DisableOSVRDirectModeAMD.cpp)
	target_link_libraries(DisableOSVRDirectModeAMD PRIVATE osvr::osvrClientKitCpp osvrRenderManagerCpp)

	install(TARGETS EnableOSVRDirectModeAMD DisableOSVRDirectModeAMD RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
