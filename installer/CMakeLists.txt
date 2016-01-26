cmake_minimum_required(VERSION 3.1.0)
project(myProjectForOsvrRenderManager)

#-----------------------------------------------------------------------------
# Local CMake Modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set (CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};C:/Program Files/jsoncpp;C:/Program Files/Microsoft SDKs/Windows/v6.0;C:/usr/local CACHE PATH "Path that CMake uses to find packages")

#-----------------------------------------------------------------------------
# This looks for an osvrConfig.cmake file - most of the time it can be
# autodetected but you might need to specify osvr_DIR to be something like
# C:/Users/Ryan/Desktop/build/OSVR-Core-vc12 or
# C:/Users/Ryan/Downloads/OSVR-Core-Snapshot-v0.1-406-gaa55515-build54-vs12-32bit
# in the CMake GUI or command line
find_package(osvr REQUIRED)
find_package(jsoncpp REQUIRED)
if(TARGET jsoncpp_lib_static AND NOT TARGET jsoncpp_lib)
	add_library(jsoncpp_lib ALIAS jsoncpp_lib_static)
endif()

find_package(VRPN REQUIRED)	# For time-manipulation routines
find_package(quatlib REQUIRED)	# For transformation construction
include_directories(${VRPN_INCLUDE_DIRS})

find_package(OpenGL)
find_package(GLEW)
find_package(SDL2 REQUIRED)
find_package(osvrRenderManager REQUIRED)
include_directories(${OSVRRENDERMANAGER_INCLUDE_DIRS})

#-----------------------------------------------------------------------------
# D3D Test program (basic 2D test)
add_executable(RenderManagerD3DTest2D RenderManagerD3DTest2D.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
# Surprisingly, this also lets it know where to find the header files.
target_link_libraries(RenderManagerD3DTest2D PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES})

# D3D Example program, which should eventually be open source
add_executable(RenderManagerD3DExample3D RenderManagerD3DExample3D.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
target_link_libraries(RenderManagerD3DExample3D PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES})

# D3D Example program, which should eventually be open source
add_executable(RenderManagerD3DHeadSpaceExample RenderManagerD3DHeadSpaceExample.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
target_link_libraries(RenderManagerD3DHeadSpaceExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES})

# D3D Example program using Present mode, which should eventually be open source
add_executable(RenderManagerD3DPresentExample3D RenderManagerD3DPresentExample3D.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
target_link_libraries(RenderManagerD3DPresentExample3D PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES})

# D3D Example program using Present mode, which should eventually be open source
add_executable(RenderManagerD3DPresentSideBySideExample RenderManagerD3DPresentSideBySideExample.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
target_link_libraries(RenderManagerD3DPresentSideBySideExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${VRPN_LIBRARIES})

# D3D Example program using Present mode, which should eventually be open source
add_executable(RenderManagerD3DPresentMakeDeviceExample3D RenderManagerD3DPresentMakeDeviceExample3D.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
target_link_libraries(RenderManagerD3DPresentMakeDeviceExample3D PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES})
target_link_libraries(RenderManagerD3DPresentMakeDeviceExample3D PRIVATE D3D11)

#-----------------------------------------------------------------------------
# Latency test program, which should eventually be open source
add_executable(LatencyTestD3DExample LatencyTestD3DExample.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
# Surprisingly, this also lets it know where to find the header files.
target_link_libraries(LatencyTestD3DExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES})
target_link_libraries(LatencyTestD3DExample PRIVATE ${QUATLIB_LIBRARY})

#-----------------------------------------------------------------------------
# D3D program that has an adjustable rendering delay, useful for testing the
# impact of various latency-hiding techniques.
add_executable(AdjustableRenderingDelayD3D AdjustableRenderingDelayD3D.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
# Surprisingly, this also lets it know where to find the header files.
target_include_directories(AdjustableRenderingDelayD3D PRIVATE ${OPENGL_INCLUDE_DIRS})
target_link_libraries(AdjustableRenderingDelayD3D PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${SDL2_LIBRARY} ${VRPN_LIBRARIES})

#-----------------------------------------------------------------------------
# D3D program that spins the world cube at a constant rate, useful for testing the
# steadiness of the rendering in the absence of motion.
add_executable(SpinCubeD3D SpinCubeD3D.cpp)

# This example uses the C++ wrapper, as suggested, so you'll need this line.
# Surprisingly, this also lets it know where to find the header files.
target_include_directories(SpinCubeD3D PRIVATE ${OPENGL_INCLUDE_DIRS})
target_link_libraries(SpinCubeD3D PRIVATE osvr::osvrClientKitCpp ${OSVRRENDERMANAGER_LIBRARIES} ${SDL2_LIBRARY} ${VRPN_LIBRARIES})

if (OPENGL_FOUND AND GLEW_FOUND AND SDL2_FOUND)

	#-----------------------------------------------------------------------------
	# OpenGL Example program, which should eventually be open source
	add_executable(RenderManagerOpenGLExample RenderManagerOpenGLExample.cpp)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(RenderManagerOpenGLExample PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(RenderManagerOpenGLExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY})

	#-----------------------------------------------------------------------------
	# OpenGL Example program, which should eventually be open source
	add_executable(RenderManagerOpenGLHeadSpaceExample RenderManagerOpenGLHeadSpaceExample.cpp)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(RenderManagerOpenGLHeadSpaceExample PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(RenderManagerOpenGLHeadSpaceExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY})

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	add_executable(RenderManagerOpenGLPresentExample RenderManagerOpenGLPresentExample.cpp)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(RenderManagerOpenGLPresentExample PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(RenderManagerOpenGLPresentExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY})

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	add_executable(RenderManagerOpenGLPresentSideBySideExample RenderManagerOpenGLPresentSideBySideExample.cpp)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(RenderManagerOpenGLPresentSideBySideExample PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(RenderManagerOpenGLPresentSideBySideExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY} ${VRPN_LIBRARIES})

	#-----------------------------------------------------------------------------
	# OpenGL Example program using the OpenGL Core profile, which should eventually be open source
	add_executable(RenderManagerOpenGLCoreExample RenderManagerOpenGLCoreExample.cpp)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(RenderManagerOpenGLCoreExample PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(RenderManagerOpenGLCoreExample PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY})

	#-----------------------------------------------------------------------------
	# OpenGL program that has an adjustable rendering delay, useful for testing the
	# impact of various latency-hiding techniques.
	add_executable(AdjustableRenderingDelayOpenGL AdjustableRenderingDelayOpenGL.cpp)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(AdjustableRenderingDelayOpenGL PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(AdjustableRenderingDelayOpenGL PRIVATE osvr::osvrClientKitCpp osvr::osvrClient osvr::osvrCommon ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY} ${VRPN_LIBRARIES})

	#-----------------------------------------------------------------------------
	# OpenGL program that spins the world cube at a constant rate, useful for testing the
	# steadiness of the rendering in the absence of motion.
	add_executable(SpinCubeOpenGL SpinCubeOpenGL.cpp font.c)

	# This example uses the C++ wrapper, as suggested, so you'll need this line.
	# Surprisingly, this also lets it know where to find the header files.
	target_include_directories(SpinCubeOpenGL PRIVATE ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(SpinCubeOpenGL PRIVATE osvr::osvrClientKitCpp ${OSVRRENDERMANAGER_LIBRARIES} ${OPENGL_LIBRARY} GLEW::GLEW ${SDL2_LIBRARY} ${VRPN_LIBRARIES})

endif()
