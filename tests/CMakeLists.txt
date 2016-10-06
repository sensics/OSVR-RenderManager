#
# Tests
#

# Ensure headers include their own dependencies
if(BUILD_TESTS)
    add_subdirectory(header_dependencies)
endif()

if(BUILD_TESTING)
    ###
    # Set up self-contained examples test - make sure they can build against a build tree
    set(CONFIG_FLAGS "-DosvrRenderManager_DIR=${PROJECT_BINARY_DIR}")
    set(BUILD_FLAGS)
    if(MSVC)
        # TODO this is really for any multi-config generators, not just MSVC
        set(BUILD_FLAGS --config $<CONFIGURATION>)
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        set(CONFIG_FLAGS "${CONFIG_FLAGS} -T \"${CMAKE_GENERATOR_TOOLSET}\"")
    endif()

    configure_file(selfcontained_test.cmake "${CMAKE_CURRENT_BINARY_DIR}/selfcontained_test.cmake" @ONLY)
    add_test(NAME StandaloneExamplesBuild
        COMMAND
        "${CMAKE_COMMAND}"
        "-DCONFIG_FLAGS=${CONFIG_FLAGS}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../examples"
        "-DBUILD_FLAGS=${BUILD_FLAGS}"
        -P "${CMAKE_CURRENT_BINARY_DIR}/selfcontained_test.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()
