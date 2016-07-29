IMPORTANT: Version 0.6.21 has some changes to the API that will require applications built against RenderManager to be updated.  See the release notes and example programs.

In addition to binaries, the RenderManager installer includes all of the source code needed to build and link the example programs.
It also includes a CMakeLists.txt file and associated Find files in the cmake/ directory.

Building from source requires a number of external libraries:
	OpenGL (should be built in with your compiler)
	OSVR libraries and headers:
		osvr (osvr.org)
		libfunctionality (included with OSVR)
	jsoncpp (https://github.com/open-source-parsers/jsoncpp)
	VRPN (vrpn.org)
	quatlib (comes with VRPN)
	GLEW (http://glew.sourceforge.net/)
	SDL2 (https://www.libsdl.org/index.php)
	
Once the above libraries have been installed, you need to point CMake at each of them, using either cmake-gui or ccmake.

The build has, as of release 0.6.18, only been tested on Windows with the 64-bit Windows binaries for osvrRenderManager.  This requires 64-bit binaries for the other libraries as well.

You should be able to build your own programs by copying the CMakeLists.txt file, the cmake/ directory, and one of the source files to a new directory.
Then you can rename the file and modify CMakeLists.txt to build that application and remove the others.

Once the programs have been compiled, the associated DLLs need to be copied into the path, or into the same directory as the executables, before you can run them.


