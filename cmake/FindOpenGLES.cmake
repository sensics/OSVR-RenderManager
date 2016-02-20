# - Find OpenGLES
# Find the native OpenGLES includes and libraries
#
#  OPENGLES_INCLUDE_DIR - where to find GLES/gl.h, etc.
#  OPENGLES_LIBRARIES   - List of libraries when using OpenGLES.
#  OPENGLES_FOUND       - True if OpenGLES found.

# Downloaded from
# https://sourceforge.net/p/alleg/allegro/ci/5.1/tree/cmake/FindOpenGLES.cmake
# Modified to use the v2 library rather than the v1 library.

if(OPENGLES_INCLUDE_DIR)
    # Already in cache, be silent
    set(OPENGLES_FIND_QUIETLY TRUE)
endif(OPENGLES_INCLUDE_DIR)

find_path(OPENGLES_INCLUDE_DIR GLES/gl.h)

find_library(OPENGLES_gl_LIBRARY NAMES GLESv2)

# Handle the QUIETLY and REQUIRED arguments and set OPENGLES_FOUND
# to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OPENGLES DEFAULT_MSG
    OPENGLES_INCLUDE_DIR OPENGLES_gl_LIBRARY)

set(OPENGLES_LIBRARIES ${OPENGLES_gl_LIBRARY})

mark_as_advanced(OPENGLES_INCLUDE_DIR)
mark_as_advanced(OPENGLES_gl_LIBRARY)

