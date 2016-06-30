
# Hook for a super-build to optionally inject hints before target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildPrefix.cmake" OPTIONAL)

# TODO Dependencies
#find_dependency(GLEW)

# The actual exported targets
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerTargets.cmake")

# Fix up imported targets to add deps: these files will only exist in install trees.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigInstalledBoost.cmake" OPTIONAL)

# A list of filenames of required libraries for running with osvrRenderManager DLL
set(OSVRRM_REQUIRED_LIBRARIES_DEBUG "@OSVRRM_REQUIRED_LIBRARIES_DEBUG@")
set(OSVRRM_REQUIRED_LIBRARIES_RELEASE "@OSVRRM_REQUIRED_LIBRARIES_RELEASE@")
set(OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO "@OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO@")
set(OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL "@OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL@")

# Helper function to copy required libraries to an install directory
function(osvrrm_install_dependencies _destination)
	install(FILES
		${OSVRRM_REQUIRED_LIBRARIES_DEBUG}
		CONFIGURATIONS Debug
		DESTINATION ${_destination}
		COMPONENT Runtime)
	install(FILES
		${OSVRRM_REQUIRED_LIBRARIES_RELEASE}
		CONFIGURATIONS Release
		DESTINATION ${_destination}
		COMPONENT Runtime)
	install(FILES
		${OSVRRM_REQUIRED_LIBRARIES_RELWITHDEBINFO}
		CONFIGURATIONS RelWithDebInfo
		DESTINATION ${_destination}
		COMPONENT Runtime)
	install(FILES
		${OSVRRM_REQUIRED_LIBRARIES_MINSIZEREL}
		CONFIGURATIONS MinSizeRel
		DESTINATION ${_destination}
		COMPONENT Runtime)
endfunction()

# Hook for a super-build to optionally inject configuration after target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildSuffix.cmake" OPTIONAL)
