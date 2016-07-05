find_package(Qt5Widgets)
find_package(Qt5OpenGL)

set(COMMON_D3D_SOURCES
	D3DCube.h
	D3DSimpleShader.h
	pixelshader3d.h
	vertexshader3d.h)
set(COMMON_D3D_2D_SOURCES
	pixelshader.h
	vertexshader.h)

# D3D
if(OSVRRM_HAVE_D3D11_SUPPORT)
	#-----------------------------------------------------------------------------
	# D3D program that has an adjustable rendering delay, useful for testing the
	# impact of various latency-hiding techniques.
	add_executable(AdjustableRenderingDelayD3D AdjustableRenderingDelayD3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(AdjustableRenderingDelayD3D PRIVATE osvrRM::osvrRenderManagerCpp)
  target_compile_features(AdjustableRenderingDelayD3D PRIVATE cxx_range_for)

	# D3D Example program using the C API
	add_executable(RenderManagerD3DCAPIExample RenderManagerD3DCAPIExample.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DCAPIExample PRIVATE osvrRM::osvrRenderManagerCpp)
  target_compile_features(RenderManagerD3DCAPIExample PRIVATE cxx_range_for)

	# D3D Example program
	add_executable(RenderManagerD3DExample3D RenderManagerD3DExample3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DExample3D PRIVATE osvrRM::osvrRenderManagerCpp)
  target_compile_features(RenderManagerD3DExample3D PRIVATE cxx_range_for)

	# D3D Example program using Present mode
	add_executable(RenderManagerD3DPresentExample3D RenderManagerD3DPresentExample3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DPresentExample3D PRIVATE osvrRM::osvrRenderManagerCpp)
  target_compile_features(RenderManagerD3DPresentExample3D PRIVATE cxx_range_for)

	# D3D Example program using Present mode that also passes in device and context
	add_executable(RenderManagerD3DPresentMakeDeviceExample3D RenderManagerD3DPresentMakeDeviceExample3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DPresentMakeDeviceExample3D PRIVATE osvrRM::osvrRenderManagerCpp D3D11)
  target_compile_features(RenderManagerD3DPresentMakeDeviceExample3D PRIVATE cxx_range_for)

	# D3D Example program using Present mode that also passes in device and context and works with ATW
	add_executable(RenderManagerD3DATWDoubleBufferExample RenderManagerD3DATWDoubleBufferExample.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DATWDoubleBufferExample PRIVATE osvrRM::osvrRenderManagerCpp D3D11)
  target_compile_features(RenderManagerD3DATWDoubleBufferExample PRIVATE cxx_range_for)

	# D3D Example program using Present mode
	add_executable(RenderManagerD3DPresentSideBySideExample RenderManagerD3DPresentSideBySideExample.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DPresentSideBySideExample PRIVATE osvrRM::osvrRenderManagerCpp)
  target_compile_features(RenderManagerD3DPresentSideBySideExample PRIVATE cxx_range_for)

	#-----------------------------------------------------------------------------
	# D3D Test program (basic 2D test)
	add_executable(RenderManagerD3DTest2D RenderManagerD3DTest2D.cpp ${COMMON_D3D_2D_SOURCES})
	target_link_libraries(RenderManagerD3DTest2D PRIVATE osvrRM::osvrRenderManagerCpp)
  target_compile_features(RenderManagerD3DTest2D PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			AdjustableRenderingDelayD3D
			RenderManagerD3DExample3D
			RenderManagerD3DPresentExample3D
			RenderManagerD3DPresentMakeDeviceExample3D
			RenderManagerD3DPresentSideBySideExample
			RenderManagerD3DTest2D
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

	#-----------------------------------------------------------------------------
	# Latency test program
	add_executable(LatencyTestD3DExample LatencyTestD3DExample.cpp ${COMMON_D3D_2D_SOURCES})
	target_link_libraries(LatencyTestD3DExample PRIVATE osvrRM::osvrRenderManagerCpp vendored-quat)
  target_compile_features(LatencyTestD3DExample PRIVATE cxx_range_for)

	# D3D Example program
	add_executable(RenderManagerD3DHeadSpaceExample RenderManagerD3DHeadSpaceExample.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RenderManagerD3DHeadSpaceExample PRIVATE osvrRM::osvrRenderManagerCpp vendored-quat)
  target_compile_features(RenderManagerD3DHeadSpaceExample PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			LatencyTestD3DExample
			RenderManagerD3DHeadSpaceExample
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

	#-----------------------------------------------------------------------------
	# D3D program that spins the world cube at a constant rate, useful for testing the
	# steadiness of the rendering in the absence of motion.
	add_executable(SpinCubeD3D SpinCubeD3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(SpinCubeD3D PRIVATE osvrRM::osvrRenderManagerCpp vendored-vrpn vendored-quat)
  target_compile_features(SpinCubeD3D PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			SpinCubeD3D
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

	#-----------------------------------------------------------------------------
	# D3D program that restarts the RenderManager every once in a while.  For
	# the display, it spins the world cube at a constant rate, useful for testing the
	# steadiness of the rendering in the absence of motion.
	add_executable(RestartD3D RestartD3D.cpp ${COMMON_D3D_SOURCES})
	target_link_libraries(RestartD3D PRIVATE osvrRM::osvrRenderManagerCpp vendored-vrpn vendored-quat D3D11)
  target_compile_features(RestartD3D PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			RestartD3D
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()

if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND)
	# Convenience target to link all we need for basic OpenGL stuff.
	add_library(convenience_gldeps INTERFACE)
	target_link_libraries(convenience_gldeps INTERFACE ${OPENGL_LIBRARY})
	target_include_directories(convenience_gldeps INTERFACE ${OPENGL_INCLUDE_DIRS})

 if (NOT APPLE)
	#-----------------------------------------------------------------------------
	# OpenGL Example program
	add_executable(RenderManagerOpenGLExample RenderManagerOpenGLExample.cpp)
	target_link_libraries(RenderManagerOpenGLExample PRIVATE osvrRM::osvrRenderManagerCpp convenience_gldeps)
  target_compile_features(RenderManagerOpenGLExample PRIVATE cxx_range_for)

	#-----------------------------------------------------------------------------
	# OpenGL Example program
	add_executable(RenderManagerOpenGLHeadSpaceExample RenderManagerOpenGLHeadSpaceExample.cpp)
	target_link_libraries(RenderManagerOpenGLHeadSpaceExample PRIVATE osvrRM::osvrRenderManagerCpp convenience_gldeps)
  target_compile_features(RenderManagerOpenGLHeadSpaceExample PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			RenderManagerOpenGLExample
			RenderManagerOpenGLHeadSpaceExample
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

  if (Qt5Widgets_FOUND AND Qt5OpenGL_FOUND)
	  #-----------------------------------------------------------------------------
	  # OpenGL Example program
	  add_executable(RenderManagerOpenGLQt5Example RenderManagerOpenGLQt5Example.cpp)
	  target_link_libraries(RenderManagerOpenGLQt5Example
      PRIVATE osvrRM::osvrRenderManagerCpp convenience_gldeps
      Qt5::OpenGL Qt5::Widgets)
    target_compile_features(RenderManagerOpenGLQt5Example PRIVATE cxx_range_for)

	  if(OSVRRM_INSTALL_EXAMPLES)
		  install(TARGETS
			  RenderManagerOpenGLQt5Example
			  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	  endif()
  endif()

 endif()
endif()

if(OSVRRM_HAVE_OPENGL_SUPPORT AND GLEW_FOUND)

	#-----------------------------------------------------------------------------
	# OpenGL Example program using the OpenGL Core profile
	add_executable(RenderManagerOpenGLCoreExample RenderManagerOpenGLCoreExample.cpp)
	target_link_libraries(RenderManagerOpenGLCoreExample PRIVATE osvrRM::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
  target_compile_features(RenderManagerOpenGLCoreExample PRIVATE cxx_range_for)
	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			RenderManagerOpenGLCoreExample
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

 if (SDL2_FOUND)
	#-----------------------------------------------------------------------------
	# OpenGL Example program using the OpenGL Core profile and a shared context
	add_executable(RenderManagerOpenGLSharedContextExample RenderManagerOpenGLSharedContextExample.cpp)
	target_link_libraries(RenderManagerOpenGLSharedContextExample PRIVATE osvrRM::osvrRenderManagerCpp GLEW::GLEW SDL2::SDL2 convenience_gldeps)
  target_compile_features(RenderManagerOpenGLSharedContextExample PRIVATE cxx_range_for)
	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			RenderManagerOpenGLSharedContextExample
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
 endif()

 if (NOT APPLE)
	#-----------------------------------------------------------------------------
	# OpenGL program that has an adjustable rendering delay, useful for testing the
	# impact of various latency-hiding techniques.
	add_executable(AdjustableRenderingDelayOpenGL AdjustableRenderingDelayOpenGL.cpp)
	target_link_libraries(AdjustableRenderingDelayOpenGL PRIVATE osvrRM::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
  target_compile_features(AdjustableRenderingDelayOpenGL PRIVATE cxx_range_for)

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	add_executable(RenderManagerOpenGLPresentExample RenderManagerOpenGLPresentExample.cpp)
	target_link_libraries(RenderManagerOpenGLPresentExample PRIVATE osvrRM::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
  target_compile_features(RenderManagerOpenGLPresentExample PRIVATE cxx_range_for)

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the C API.
	add_executable(RenderManagerOpenGLCAPIExample RenderManagerOpenGLCAPIExample.cpp)
	target_link_libraries(RenderManagerOpenGLCAPIExample PRIVATE osvrRM::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
  target_compile_features(RenderManagerOpenGLCAPIExample PRIVATE cxx_range_for)

	#-----------------------------------------------------------------------------
	# OpenGL Example program that uses the Get()/Present() interface, not Render().
	add_executable(RenderManagerOpenGLPresentSideBySideExample RenderManagerOpenGLPresentSideBySideExample.cpp)
	target_link_libraries(RenderManagerOpenGLPresentSideBySideExample PRIVATE osvrRM::osvrRenderManagerCpp GLEW::GLEW convenience_gldeps)
  target_compile_features(RenderManagerOpenGLPresentSideBySideExample PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			AdjustableRenderingDelayOpenGL
			RenderManagerOpenGLPresentExample
			RenderManagerOpenGLPresentSideBySideExample
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
 endif()
endif()

if(RM_USE_OPENGLES20)
	#-----------------------------------------------------------------------------
	# OpenGLES 2.0 Example program
	add_executable(RenderManagerOpenGLES2Example RenderManagerOpenGLES2Example.cpp)
	target_link_libraries(RenderManagerOpenGLES2Example PRIVATE osvrRM::osvrRenderManagerCpp ${OPENGLES2_LIBRARIES})

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			RenderManagerOpenGLES2Example
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()

if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND AND NOT APPLE)
	#-----------------------------------------------------------------------------
	# OpenGL program that spins the world cube at a constant rate, useful for testing the
	# steadiness of the rendering in the absence of motion.
	add_executable(SpinCubeOpenGL SpinCubeOpenGL.cpp font.c)
	target_link_libraries(SpinCubeOpenGL PRIVATE osvrRM::osvrRenderManagerCpp convenience_gldeps vendored-vrpn vendored-quat)
	target_compile_features(SpinCubeOpenGL PRIVATE cxx_range_for)

	if(OSVRRM_INSTALL_EXAMPLES)
		install(TARGETS
			SpinCubeOpenGL
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()

#-----------------------------------------------------------------------------
# Program that changes the solid color sent to a display.
add_executable(SolidColor SolidColor.cpp)
target_link_libraries(SolidColor PRIVATE osvrRM::osvrRenderManagerCpp vendored-vrpn vendored-quat)
target_compile_features(SolidColor PRIVATE cxx_range_for)

if(OSVRRM_INSTALL_EXAMPLES)
	install(TARGETS
		SolidColor
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

