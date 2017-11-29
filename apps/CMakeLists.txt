
if(OSVRRM_HAVE_OPENGL_SUPPORT AND OPENGL_FOUND AND GLEW_FOUND)
	# Modified version of the OpenGL "High Poly" test that just renders a head-locked chessboard.
	add_executable(RenderManagerOpenGLChessboard RenderManagerOpenGLChessboard.cpp)
	target_link_libraries(RenderManagerOpenGLChessboard
		PRIVATE
		osvrRenderManager::osvrRenderManagerCpp
		GLEW::GLEW
		${OPENGL_LIBRARY})
	target_include_directories(RenderManagerOpenGLChessboard
		PRIVATE
		${OPENGL_INCLUDE_DIRS})
	target_compile_features(RenderManagerOpenGLChessboard PRIVATE cxx_range_for)
	set_target_properties(RenderManagerOpenGLChessboard PROPERTIES
		FOLDER "Tools")
	install(TARGETS
		RenderManagerOpenGLChessboard
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
