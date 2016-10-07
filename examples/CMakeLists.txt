cmake_minimum_required(VERSION 3.1.0)
project(osvrRenderManagerExamples)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET osvrRenderManager::osvrRenderManagerCpp)
	# This means we're building stand-alone, not in an RM build system.
	find_package(osvrRenderManager REQUIRED)
	set(OSVRRM_EXAMPLES_STANDALONE ON)
else()
	set(OSVRRM_EXAMPLES_STANDALONE OFF)
endif()

message(STATUS "Searching for optional dependencies")
if(OSVRRM_HAVE_OPENGL_SUPPORT AND OSVRRM_EXAMPLES_STANDALONE)
	find_package(OpenGL)
	#find_package(SDL2)
	osvrrm_find_SDL2()
	#find_package(GLEW)
	osvrrm_find_GLEW()
	# TODO search for GLES if applicable
endif()
find_package(Qt5Widgets QUIET)
find_package(Qt5OpenGL QUIET)

set(COMMON_D3D_SOURCES
	D3DCube.h
	D3DSimpleShader.h
	pixelshader3d.h
	vertexshader3d.h)
set(COMMON_D3D_2D_SOURCES
	pixelshader.h
	vertexshader.h)
set(BUILT_EXAMPLES)
# D3D
if(OSVRRM_HAVE_D3D11_SUPPORT)

	set(OSVRRM_HAVE_COMMON_D3D_SOURCES ON)
	foreach(SRC ${COMMON_D3D_SOURCES})
		if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
			message(WARNING "Missing commmon D3D source file ${SRC} - some examples may not be built.")
			set(OSVRRM_HAVE_COMMON_D3D_SOURCES OFF)
		endif()
	endforeach()

	set(OSVRRM_HAVE_COMMON_D3D_2D_SOURCES ON)
	foreach(SRC ${COMMON_D3D_2D_SOURCES})
		if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
			message(WARNING "Missing commmon D3D 2D source file ${SRC} - some examples may not be built.")
			set(OSVRRM_HAVE_COMMON_D3D_2D_SOURCES OFF)
		endif()
	endforeach()
endif()

if(OSVRRM_HAVE_D3D11_SUPPORT AND OSVRRM_HAVE_COMMON_D3D_SOURCES)
	#-----------------------------------------------------------------------------
	# D3D program that has an adjustable rendering delay, useful for testing the
	# impact of various latency-hiding techniques.
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AdjustableRenderingDelayD3D.cpp")
		list(APPEND BUILT_EXAMPLES AdjustableRenderingDelayD3D)
		add_executable(AdjustableRenderingDelayD3D AdjustableRenderingDelayD3D.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(AdjustableRenderingDelayD3D PRIVATE osvrRenderManager::osvrRenderManagerCpp)
		target_compile_features(AdjustableRenderingDelayD3D PRIVATE cxx_range_for)
	endif()

	# D3D Example program using the C API
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DCAPIExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerD3DCAPIExample)
		add_executable(RenderManagerD3DCAPIExample RenderManagerD3DCAPIExample.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(RenderManagerD3DCAPIExample PRIVATE osvrRenderManager::osvrRenderManagerCpp)
		target_compile_features(RenderManagerD3DCAPIExample PRIVATE cxx_range_for)
	endif()

	# D3D Example program
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DExample3D.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerD3DExample3D)
		add_executable(RenderManagerD3DExample3D RenderManagerD3DExample3D.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(RenderManagerD3DExample3D PRIVATE osvrRenderManager::osvrRenderManagerCpp)
		target_compile_features(RenderManagerD3DExample3D PRIVATE cxx_range_for)
	endif()

	# D3D Example program using Present mode
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DPresentExample3D.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerD3DPresentExample3D)
		add_executable(RenderManagerD3DPresentExample3D RenderManagerD3DPresentExample3D.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(RenderManagerD3DPresentExample3D PRIVATE osvrRenderManager::osvrRenderManagerCpp)
		target_compile_features(RenderManagerD3DPresentExample3D PRIVATE cxx_range_for)
	endif()

	# D3D Example program using Present mode that also passes in device and context
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DPresentMakeDeviceExample3D.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerD3DPresentMakeDeviceExample3D)
		add_executable(RenderManagerD3DPresentMakeDeviceExample3D RenderManagerD3DPresentMakeDeviceExample3D.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(RenderManagerD3DPresentMakeDeviceExample3D PRIVATE osvrRenderManager::osvrRenderManagerCpp D3D11)
		target_compile_features(RenderManagerD3DPresentMakeDeviceExample3D PRIVATE cxx_range_for)
	endif()

	# D3D Example program using Present mode that also passes in device and context and works with ATW
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DATWDoubleBufferExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerD3DATWDoubleBufferExample)
		add_executable(RenderManagerD3DATWDoubleBufferExample RenderManagerD3DATWDoubleBufferExample.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(RenderManagerD3DATWDoubleBufferExample PRIVATE osvrRenderManager::osvrRenderManagerCpp D3D11)
		target_compile_features(RenderManagerD3DATWDoubleBufferExample PRIVATE cxx_range_for)
	endif()

	# D3D Example program using Present mode, single buffer for both eyes
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DPresentSideBySideExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerD3DPresentSideBySideExample)
		add_executable(RenderManagerD3DPresentSideBySideExample RenderManagerD3DPresentSideBySideExample.cpp ${COMMON_D3D_SOURCES})
		target_link_libraries(RenderManagerD3DPresentSideBySideExample PRIVATE osvrRenderManager::osvrRenderManagerCpp)
		target_compile_features(RenderManagerD3DPresentSideBySideExample PRIVATE cxx_range_for)
	endif()
endif() # have common d3d sources

#-----------------------------------------------------------------------------
# D3D Test program (basic 2D test)
if(OSVRRM_HAVE_D3D11_SUPPORT
	AND OSVRRM_HAVE_COMMON_D3D_2D_SOURCES
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DTest2D.cpp")

	list(APPEND BUILT_EXAMPLES RenderManagerD3DTest2D)
	add_executable(RenderManagerD3DTest2D RenderManagerD3DTest2D.cpp ${COMMON_D3D_2D_SOURCES})
	target_link_libraries(RenderManagerD3DTest2D PRIVATE osvrRenderManager::osvrRenderManagerCpp)
	target_compile_features(RenderManagerD3DTest2D PRIVATE cxx_range_for)
endif()

#-----------------------------------------------------------------------------
# Latency test program
if(OSVRRM_HAVE_D3D11_SUPPORT
	AND OSVRRM_HAVE_COMMON_D3D_2D_SOURCES
	AND TARGET vendored-quat
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LatencyTestD3DExample.cpp")

	list(APPEND BUILT_EXAMPLES LatencyTestD3DExample)
	add_executable(LatencyTestD3DExample LatencyTestD3DExample.cpp ${COMMON_D3D_2D_SOURCES})
	target_link_libraries(LatencyTestD3DExample PRIVATE osvrRenderManager::osvrRenderManagerCpp vendored-quat)
	target_compile_features(LatencyTestD3DExample PRIVATE cxx_range_for)
endif()


if(OSVRRM_HAVE_D3D11_SUPPORT
	AND OSVRRM_HAVE_COMMON_D3D_SOURCES
	AND TARGET vendored-quat
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerD3DHeadSpaceExample.cpp")

	list(APPEND BUILT_EXAMPLES RenderManagerD3DHeadSpaceExample)
	# D3D Example program
	add_executable(RenderManagerD3DHeadSpaceExample RenderManagerD3DHeadSpaceExample.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DHeadSpaceExample PRIVATE osvrRenderManager::osvrRenderManagerCpp vendored-quat)
	target_compile_features(RenderManagerD3DHeadSpaceExample PRIVATE cxx_range_for)
endif()

#-----------------------------------------------------------------------------
# D3D program that spins the world cube at a constant rate, useful for testing the
# steadiness of the rendering in the absence of motion.
if(OSVRRM_HAVE_D3D11_SUPPORT
	AND OSVRRM_HAVE_COMMON_D3D_SOURCES
	AND TARGET vendored-quat
	AND TARGET vendored-vrpn
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SpinCubeD3D.cpp")

	list(APPEND BUILT_EXAMPLES SpinCubeD3D)
	add_executable(SpinCubeD3D SpinCubeD3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(SpinCubeD3D PRIVATE osvrRenderManager::osvrRenderManagerCpp vendored-vrpn vendored-quat)
	target_compile_features(SpinCubeD3D PRIVATE cxx_range_for)
endif()

#-----------------------------------------------------------------------------
# D3D program that restarts the RenderManager every once in a while. For
# the display, it spins the world cube at a constant rate, useful for testing the
# steadiness of the rendering in the absence of motion.
if(OSVRRM_HAVE_D3D11_SUPPORT
	AND OSVRRM_HAVE_COMMON_D3D_SOURCES
	AND TARGET vendored-quat
	AND TARGET vendored-vrpn
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RestartD3D.cpp")

	list(APPEND BUILT_EXAMPLES RestartD3D)
	add_executable(RestartD3D RestartD3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RestartD3D PRIVATE osvrRenderManager::osvrRenderManagerCpp vendored-vrpn vendored-quat D3D11)
	target_compile_features(RestartD3D PRIVATE cxx_range_for)
endif()


if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND)
	# Convenience target to link all we need for basic OpenGL stuff.
	add_library(convenience_gldeps INTERFACE)
	target_link_libraries(convenience_gldeps INTERFACE ${OPENGL_LIBRARY})
	target_include_directories(convenience_gldeps INTERFACE ${OPENGL_INCLUDE_DIRS})
endif()

if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND AND SDL2_FOUND AND GLEW_FOUND)
	#-----------------------------------------------------------------------------
	# OpenGL Example program using the OpenGL Core profile and a shared context
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLSharedContextExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLSharedContextExample)
		add_executable(RenderManagerOpenGLSharedContextExample RenderManagerOpenGLSharedContextExample.cpp)
		target_link_libraries(RenderManagerOpenGLSharedContextExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW SDL2::SDL2 convenience_gldeps)
		target_compile_features(RenderManagerOpenGLSharedContextExample PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the C API and a shared context.
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLCAPISharedContextExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLCAPISharedContextExample)
		add_executable(RenderManagerOpenGLCAPISharedContextExample RenderManagerOpenGLCAPISharedContextExample.cpp)
		target_link_libraries(RenderManagerOpenGLCAPISharedContextExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW SDL2::SDL2 convenience_gldeps)
		target_compile_features(RenderManagerOpenGLCAPISharedContextExample PRIVATE cxx_range_for)
	endif()

 	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLSharedContextPresentExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLSharedContextPresentExample)
		add_executable(RenderManagerOpenGLSharedContextPresentExample RenderManagerOpenGLSharedContextPresentExample.cpp)
		target_link_libraries(RenderManagerOpenGLSharedContextPresentExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW SDL2::SDL2 convenience_gldeps)
		target_compile_features(RenderManagerOpenGLSharedContextPresentExample PRIVATE cxx_range_for)
	endif()
endif()


if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND AND NOT APPLE)
	#-----------------------------------------------------------------------------
	# OpenGL Example program
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLExample)
		add_executable(RenderManagerOpenGLExample RenderManagerOpenGLExample.cpp)
		target_link_libraries(RenderManagerOpenGLExample PRIVATE osvrRenderManager::osvrRenderManagerCpp convenience_gldeps)
		target_compile_features(RenderManagerOpenGLExample PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL Example program
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLHeadSpaceExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLHeadSpaceExample)
		add_executable(RenderManagerOpenGLHeadSpaceExample RenderManagerOpenGLHeadSpaceExample.cpp)
		target_link_libraries(RenderManagerOpenGLHeadSpaceExample PRIVATE osvrRenderManager::osvrRenderManagerCpp convenience_gldeps)
		target_compile_features(RenderManagerOpenGLHeadSpaceExample PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL Example program
	if (Qt5Widgets_FOUND
		AND Qt5OpenGL_FOUND
		AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLQt5Example.cpp")

		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLQt5Example)
		add_executable(RenderManagerOpenGLQt5Example RenderManagerOpenGLQt5Example.cpp)
		target_link_libraries(RenderManagerOpenGLQt5Example
			PRIVATE
			osvrRenderManager::osvrRenderManagerCpp
			convenience_gldeps
			Qt5::OpenGL
			Qt5::Widgets)
		target_compile_features(RenderManagerOpenGLQt5Example PRIVATE cxx_range_for)
	endif()
endif()



if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND AND GLEW_FOUND)

	#-----------------------------------------------------------------------------
	# OpenGL Example program using the OpenGL Core profile
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLCoreExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLCoreExample)
		add_executable(RenderManagerOpenGLCoreExample RenderManagerOpenGLCoreExample.cpp)
		target_link_libraries(RenderManagerOpenGLCoreExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
		target_compile_features(RenderManagerOpenGLCoreExample PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL test program using the OpenGL Core profile that renders lots of polygons
	if(TARGET vendored-vrpn AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLHighPolyTest.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLHighPolyTest)
		add_executable(RenderManagerOpenGLHighPolyTest RenderManagerOpenGLHighPolyTest.cpp)
		target_link_libraries(RenderManagerOpenGLHighPolyTest PRIVATE vendored-vrpn osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
		target_compile_features(RenderManagerOpenGLHighPolyTest PRIVATE cxx_range_for)
	endif()
endif()

if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND AND GLEW_FOUND AND NOT APPLE)
	#-----------------------------------------------------------------------------
	# OpenGL program that has an adjustable rendering delay, useful for testing the
	# impact of various latency-hiding techniques.
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AdjustableRenderingDelayOpenGL.cpp")
		list(APPEND BUILT_EXAMPLES AdjustableRenderingDelayOpenGL)
		add_executable(AdjustableRenderingDelayOpenGL AdjustableRenderingDelayOpenGL.cpp)
		target_link_libraries(AdjustableRenderingDelayOpenGL PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
		target_compile_features(AdjustableRenderingDelayOpenGL PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLPresentExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLPresentExample)
		add_executable(RenderManagerOpenGLPresentExample RenderManagerOpenGLPresentExample.cpp)
		target_link_libraries(RenderManagerOpenGLPresentExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
		target_compile_features(RenderManagerOpenGLPresentExample PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the C API.
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLCAPIExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLCAPIExample)
		add_executable(RenderManagerOpenGLCAPIExample RenderManagerOpenGLCAPIExample.cpp)
		target_link_libraries(RenderManagerOpenGLCAPIExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
		target_compile_features(RenderManagerOpenGLCAPIExample PRIVATE cxx_range_for)
	endif()

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLPresentSideBySideExample.cpp")
		list(APPEND BUILT_EXAMPLES RenderManagerOpenGLPresentSideBySideExample)
		add_executable(RenderManagerOpenGLPresentSideBySideExample RenderManagerOpenGLPresentSideBySideExample.cpp)
		target_link_libraries(RenderManagerOpenGLPresentSideBySideExample PRIVATE osvrRenderManager::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
		target_compile_features(RenderManagerOpenGLPresentSideBySideExample PRIVATE cxx_range_for)
	endif()
endif()

#-----------------------------------------------------------------------------
# OpenGLES 2.0 Example program
if(OSVRRM_USE_OPENGLES20
	AND OPENGLES2_FOUND
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderManagerOpenGLES2Example.cpp")

	list(APPEND BUILT_EXAMPLES RenderManagerOpenGLES2Example)
	add_executable(RenderManagerOpenGLES2Example RenderManagerOpenGLES2Example.cpp)
	target_link_libraries(RenderManagerOpenGLES2Example PRIVATE osvrRenderManager::osvrRenderManagerCpp ${OPENGLES2_LIBRARIES})
endif()

if(OSVRRM_HAVE_OPENGL_SUPPORT
	AND OPENGL_FOUND
	AND NOT APPLE
	AND TARGET vendored-vrpn
	AND TARGET vendored-quat
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SpinCubeOpenGL.cpp"
	AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/font.c")
	#-----------------------------------------------------------------------------
	# OpenGL program that spins the world cube at a constant rate, useful for testing the
	# steadiness of the rendering in the absence of motion.
	list(APPEND BUILT_EXAMPLES SpinCubeOpenGL)
	add_executable(SpinCubeOpenGL SpinCubeOpenGL.cpp font.c)
	target_link_libraries(SpinCubeOpenGL PRIVATE osvrRenderManager::osvrRenderManagerCpp convenience_gldeps vendored-vrpn vendored-quat)
	target_compile_features(SpinCubeOpenGL PRIVATE cxx_range_for)

endif()

#-----------------------------------------------------------------------------
# Program that changes the solid color sent to a display.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SolidColor.cpp")

	list(APPEND BUILT_EXAMPLES SolidColor)
	add_executable(SolidColor SolidColor.cpp)
	target_link_libraries(SolidColor PRIVATE osvrRenderManager::osvrRenderManagerCpp)
	target_compile_features(SolidColor PRIVATE cxx_range_for)
endif()

list(LENGTH BUILT_EXAMPLES NUM_BUILT_EXAMPLES)
message(STATUS "Building ${NUM_BUILT_EXAMPLES} RenderManager examples.")

if(OSVRRM_INSTALL_EXAMPLES AND CMAKE_INSTALL_BINDIR)
	install(TARGETS
		${BUILT_EXAMPLES}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
