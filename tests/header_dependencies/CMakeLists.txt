#
# Ensure all headers include all dependencies
#

set(IGNORED_HEADERS
        RenderKit/DistortionCorrectTextureCoordinate.h
        RenderKit/GraphicsLibraryOpenGL.h
        RenderKit/GraphicsLibraryD3D11.h
        RenderKit/RenderManagerOpenGL.h
        RenderKit/RenderManagerD3D.h
        RenderKit/RenderManagerD3D11ATW.h
        RenderKit/RenderManagerD3DOpenGL.h
        RenderKit/NDA/OSVR-RenderManager-NVIDIA/RenderManagerNVidiaD3D.h
        RenderKit/NDA/OSVR-RenderManager-NVIDIA/Util.h
        RenderKit/NDA/OSVR-RenderManager-NVIDIA/CheckSuccess.h
        RenderKit/NDA/OSVR-RenderManager-NVIDIA/NVAPIWrappers.h
        RenderKit/NDA/OSVR-RenderManager-NVIDIA/NVIDIADriverVersion.h
        RenderKit/NDA/OSVR-RenderManager-AMD/RenderManagerAMDD3D.h
        RenderKit/NDA/OSVR-RenderManager-Intel/RenderManagerIntelD3D.h
)

# Special cases
if (NOT OSVRRM_HAVE_D3D11_SUPPORT)
	list(APPEND IGNORED_HEADERS
		RenderKit/GraphicsLibraryD3D11.h
		RenderKit/RenderManagerD3D.h
		RenderKit/RenderManagerD3D11ATW.h
		RenderKit/RenderManagerD3D11C.h
		RenderKit/RenderManagerD3DBase.h
		RenderKit/RenderManagerD3DOpenGL.h
	)
endif()

if (SDL2_FOUND)
	set(LIBRARIES_RenderKit_RenderManagerSDLInitQuit SDL2::SDL2)
	set(LIBRARIES_RenderKit_RenderManagerOpenGL SDL2::SDL2)
else()
	list(APPEND IGNORED_HEADERS
		RenderKit/RenderManagerSDLInitQuit.h)
endif()

set(COMMON_TEST_LIBRARIES osvrRenderManager)

add_custom_target(header_dependencies)

file(GLOB_RECURSE headers RELATIVE "${HEADER_BASE}" ${HEADER_BASE}/*.h)
set(test_index 0)
foreach(HEADER ${headers})
	# Sample of relevant variables computed here
	# HEADER: RenderKit/RenderManager.h
	# symbolname: RenderKit_RenderManager

	# Compute symbolname
	string(REPLACE ".h" "" symbolname "${HEADER}")
	string(MAKE_C_IDENTIFIER "${symbolname}" symbolname)

	list(FIND IGNORED_HEADERS "${HEADER}" _index)
	# If we didn't explicitly ignore this and if we built this target
	if(${_index} EQUAL -1)
		#message(STATUS "${HEADER}: '${symbolname}'")

		set(extensions cpp)
		if (${HEADER} MATCHES "C.h$")
			# Header required to be C-safe, enforce it!
			list(APPEND extensions c)
		endif()

		foreach(extension ${extensions})
			# Name the test and output file with a number, to dodge Windows path length limits.
			# Call it header, instead of test, to avoid polluting the 'executable namespace'
			set(test_name "header_${extension}_${test_index}")

			set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/main.${extension}")

			add_executable(${test_name} "${source_file}")
			target_compile_definitions(${test_name} PRIVATE HEADER_TO_TEST="${HEADER}")
			target_include_directories(${test_name}
				PRIVATE
				${BUILDTREE_HEADER_BASE}
				${HEADER_BASE})

			set_target_properties(${test_name} PROPERTIES
				FOLDER "Header dependency tests")

			target_link_libraries(${test_name}
				PRIVATE
				${COMMON_TEST_LIBRARIES}
				${LIBRARIES_${symbolname}})

			add_test(NAME ${test_name}_builds COMMAND ${test_name})
			add_dependencies(header_dependencies ${test_name})
		endforeach()
		math(EXPR test_index "${test_index} + 1")
	endif()
endforeach()

