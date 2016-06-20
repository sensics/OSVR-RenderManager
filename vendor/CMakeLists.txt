if(NOT (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vrpn/vrpn_Connection.h"))
	message(FATAL_ERROR "You are missing the VRPN git submodule. You can get it now by running git submodule init vendor/vrpn; git submodule update from the root source directory.")
endif()

set(BUILD_SHARED_LIBS OFF)

# Build VRPN as subproject
set(VRPN_GPL_SERVER FALSE CACHE BOOL "" FORCE)
set(VRPN_BUILD_SERVER_LIBRARY FALSE CACHE BOOL "" FORCE)
set(VRPN_BUILD_CLIENT_LIBRARY TRUE CACHE BOOL "" FORCE)
set(VRPN_USE_LOCAL_HIDAPI FALSE CACHE BOOL "" FORCE)
set(VRPN_USE_LOCAL_JSONCPP FALSE CACHE BOOL "" FORCE)
add_subdirectory(vrpn)

# Create an interface target to more easily consume VRPN internally.
add_library(vendored-vrpn INTERFACE)
target_link_libraries(vendored-vrpn INTERFACE ${VRPN_CLIENT_LIBRARY})
target_include_directories(vendored-vrpn INTERFACE ${VRPN_INCLUDE_DIRS})
target_compile_definitions(vendored-vrpn INTERFACE ${VRPN_DEFINITIONS})

# Create an interface target to more easily consume quatlib internally.
add_library(vendored-quat INTERFACE)
target_link_libraries(vendored-quat INTERFACE quat)
target_include_directories(vendored-quat INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vrpn/quat")
