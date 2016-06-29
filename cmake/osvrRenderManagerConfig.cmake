
# Hook for a super-build to optionally inject hints before target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildPrefix.cmake" OPTIONAL)

# TODO Dependencies
#find_dependency(GLEW)

# The actual exported targets
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerTargets.cmake")

# Fix up imported targets to add deps: these files will only exist in install trees.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigInstalledBoost.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigInstalledOpenCV.cmake" OPTIONAL)

# A list of filenames of required libraries for linking against osvrRenderManager DLL
set(OSVR_RENDERMANAGER_REQUIRED_LIBRARIES "@OSVR_RENDERMANAGER_REQUIRED_LIBRARIES@")

# Location of required libraries
set(OSVR_RENDERMANAGER_REQUIRED_LIBRARIES_DIR "@OSVR_RENDERMANAGER_REQUIRED_LIBRARIES_DIR@")

# Hook for a super-build to optionally inject configuration after target import.
include("${CMAKE_CURRENT_LIST_DIR}/osvrRenderManagerConfigSuperBuildSuffix.cmake" OPTIONAL)

