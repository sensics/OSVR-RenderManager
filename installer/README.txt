IMPORTANT: This installer only includes sample applications and the libraries and source code needed to compile applications.  If you are running a pre-compiled Unity, Unreal, or other demo this will not update the version of RenderManager it is linked against; you must either obtain an updated demo or (if you're lucky) copy the RenderManager.dll file from this installer into appropriate location inside the game folder.

IMPORTANT: Using DirectMode requires
	nVidia driver version 364.64 or higher (364.47 and 364.51 have a bug preventing it from working).
	or AMD Crimson drivers (tested with version 16.1).

FAQ: If you have multiple GPUs not in SLI mode, you need to select "Activate all displays" instead of "SLI Disabled" in the nVidia Control Panel under "Configure SLI, Surround, PhysX" to make DirectMode work.

Step 1: Run an OSVR server (obtained from an OSVR Core install) with a compatible configuration file.
  (If you do not have this installed, you can run one of the servers using the installed shortcuts;
   these are older-format files run against an older server, but should still work.)
  osvr_server_nondirectmode_window: Displays in a window that can be moved around.
  osvr_server_*: Configurations for various HMDs and modes
  NOTE: As of version 6.36, AMD DirectMode will only work in Landscape mode.
        The only current configuration file that works with AMD is OSVR HDK 1.2 landscape directmode.
        You can also edit the other configuration files to make the rotation 0 rather than 90.

Step 2: Run one of the example programs
  RenderManager*: Various graphics libraries and modes of operation.
  AdjustableRenderingDelayD3D: Shows the impact of timewarp with 500ms rendering delay.
  RenderManagerOpenGLHeadSpaceExample: Displays a small cube in head space to debug backwards eyes.
  SpinCubeD3D provides a smoothly-rotating cube to test update consistency.
  SpinCubeOpenGL provides a smoothly-rotating cube with frame ID displayed to test update consistency.  Timing info expects 60Hz DirectMode display device that blocks the app for vsync.

If DirectMode fails, you can run the DirectModeDebugging program to list available displays on nVidia.
You can also run EnableOSVRDirectModeNVidia or EnableOSVRDirectModeAMD, depending on which type of graphics card you are using.

NOTE: As of version 0.6.42, nVidia DirectMode only works with a driver that has been modified to white-list the display that you are using.  This should be already in the driver versions above 361.43 for OSVR and Vuzix displays.

An osvr_server.exe must be running to open a display (this is where it gets information about the distortion correction and other system parameters).  The osvr_server.exe must use a configuration that defines /me/head (implicitly or explicitly) for head tracking to work.  There are shortcuts to run the server with various configuration files.  You can leave the server running and run multiple different clients one after the other.  All clients should work with all servers.

Since version 0.6.12, the configuration information for both the display and the RenderManager pipeline are read on the server.  This file can be edited to change the behavior of the display, including turning direct_mode on and off and turning vertical sync on (set vertical_sync_block_rendering true).

Since version 0.6.3, Time Warp works on both OpenGL and Direct3D, and in version 0.6.8 a feature was added to ask it to wait until just before vsync (reducing latency by reading new tracker reports right up until vsync).  The DirectMode examples have this capability turned on.

Since version 0.6.3, distortion correction works on both OpenGL and Direct3D.  Since version 0.6.8, distortion correction can use an arbitrary polynomial distortion with respect to the distance from the center of projection on the screen.

Source code for RenderManager, including example programs, is available at:
  https://github.com/sensics/OSVR-RenderManager

Version notes:
0.6.43:
 *ftr Asynchronous time warp is working on all DirectRender configurations
 *ftr Client-side per-eye prediction added to reduce judder and latency
 *bug Multi-GPU support fixed (Intel + nVidia, with HDMI on nVidia)
  ftr Smaller GPU memory use when not using Render() path, and OpenGL/D3D
  ftr Now get black borders during time warp rather than texture clamping
  ftr Added juddered-rendering example program to test time warp
  ftr Added example program that does double-buffered ATW to reduce copies.
  bug Removed unused (and un-filled) return structure from D3D open
0.6.42:
 ISSUE Asynchronous Time Warp is broken (time warp is working)
 ISSUE Multi-GPU support is broken (Intel + nVidia on same computer)
 *ftr  Handles a change in the nVidia DirectMode driver regarding HDCP
  ftr  Added DirectMode check for HTC Vive using vendor name HTC
0.6.41:
 ISSUE Asynchronous Time Warp is broken (time warp is working)
 ISSUE Multi-GPU support is broken (Intel + nVidia on same computer)
  ftr  Better config file defaults to avoid tearing, build improvements
0.6.40:
 ISSUE Asynchronous Time Warp is broken (time warp is working)
 ISSUE Multi-GPU support is broken (Intel + nVidia on same computer)
  ftr  Added ability to use built-in distortion mesh for OSVR HDK 1.3
0.6.39:
 ISSUE Asynchronous Time Warp is broken (time warp is working)
 ISSUE Multi-GPU support is broken (Intel + nVidia on same computer)
 *ftr  Much more rapid unstructured grid distortion correction loading
 *ftr  Mac OpenGL Core profile support is now working
 *ftr  Builds and links (not yet tested) on Android
  ftr  Installer stored shortcuts in sub-folders by type
  ftr  Added GLES 2.0 sample program
  ftr  Removed all legacy OpenGL function calls from RenderManager
  bug  Fixed #define collision on Linux/X11
0.6.38:
 *bug  Added header files needed to compile the example code
  bug  Updated the CMakeLists.txt file included with example code
  bug  Updated the cmake directory shipped with example code
  bug  Changed the meshdistort configuration to use relative paths
0.6.37:
 *ftr  Refactoring code to prepare for open-source release
 *bug  Fixed bug with OpenGL non-DirectMode introduced as part of mac port
0.6.36:
 *ftr  Added DirectMode (D2D) on AMD cards for both Direct3D11 and OpenGL
 *ftr  Asynchronous Time Warp (currently only in a single example program)
  ftr  Code compiles and links on mac (still needs fixing for OpenGL/Core)
 *ftr  DirectMode works on nVidia cards co-installed with an Intel card
 *ftr  DirectModeDebug tool added for nVidia DirectMode
 *ftr  Code compiles and runs on Linux with all non-DirectMode features
0.6.35:
 *ftr  Adding ability to read RGB unstructured distortion meshes
 *ftr  Includes DirectMode debugging program
0.6.34:
 *ftr  Direct mode working for machines with Intel + nVidia graphics cards
  ftr  Avoid forcing direct/nondirect mode when it is already set
  bug  Fixed uninitialized translation in D3D spinning-cube demo
0.6.33:
 *ftr  Added Oversample parameter to trade finer textures or faster renders
0.6.32:
  ftr  Added calls to set and clear room rotation from head orientation
  ftr  Added error checking and warning messages in Enable/Disable DirectMode
0.6.31:
 *bug  Fixes the mesh-creation code so that all paths denormalize coordinates
  ftr  Distortion mesh interpolator requires more-orthogonal triplets
0.6.30:
  bug  Handles systems with only the HDK 1.3 plugged in with new nVidia driver
0.6.29:
  bug  Uses version 1.1 of nVidia DirectMode, fixing DirectMode on HDKs
0.6.28:
  ftr  Display configuration parser that can handle more general files
0.6.27:
 *bug  Fixed oversized field of view introduced earlier (release 0.6.23?)
0.6.26: [Bad, do not use]
 *ftr  Enables the specification of a general angle-based distortion mesh
  ftp  Added DirectMode for additional HMD
  bug  Improved determination of number of displays (works for new configs)
0.6.25: [Bad, do not use]
 *ftr  Handles off-axis projection based on the display COP parameters
  ftr  Application can specify IPD when rendering
  ftr  Added C-API header files into the include directory
 *bug  Reverted texture-format introspection that caused problems with Unity
 *bug  Fixed the whitelist entries for SVR and SEN, they were backwards
0.6.24: [Bad, do not use]
  ftr  Added initial distortion correction for the OSVR HDK 1.3
  ftr  Added ability to update distortion mesh while running
  ftr  Using release 1.0 version of nVidia DirectMode API
0.6.23: [Bad, do not use]
 *ftr  Added support for dSight dual-display mode in DirectMode and non
 *bug  Fixed the vertical field of view on OSVR configuration files
  bug  Fixed the aspect ratio for the windowed example config
0.6.22:
  ftr  Added per-frame time testing in SpinCubeOpenGL (assumes 60 Hz)
 *bug  Fixed bug where D3D render/callback path was rendering at half rate
 *bug  Fixed DirectMode display glitches
  bug  Viewport set-up now handles overlap_percent entry in config file
  bug  Handles window quit events from SDL
 *ftr  Alpha support for dual DirectMode displays (1-buffer rendering only)
  ftr  Adds support for two-window, two-display HMDs
  ftr  Enables updating of distortion mesh during run from D3D
  ftr  Per-eye timing information
0.6.21:
 *ftr  The application now passes the RenderInfo and Render params back to present
  ftr  Applications can now specify near and far clipping planes
  bug  Turning parameters into const references where possible
  bug  Render() callbacks not called for spaces that do not exist
 *ftr  Adding mutex to make RenderManager more thread-safe
  ftr  Making a separate client context for RenderManager (thread safety)
  ftr  Enabling the application to double-buffer to avoid copies in ATW
  bug  Checks for zero-sized buffers to enable error report rather than crash
0.6.20:
  ftr  Removed frame delay when specifying both sync and app-block sync
 *bug  Check for extra vsync to reduce incidence of screen tearing
 *bug  Fixed vendor ID for Vuzix display, now working with iWear
  bug  Removed double-width buffer sizes for rendering buffers
  bug  Fixed the 2D D3D example program so it shows triangles again
  ftr  Powers off DirectMode displays when we close them
  bug  Busy-waits rather than sleeps in demos to avoid O/S delays
0.6.19:
 *bug  Adds predictive tracking to reduce perceived latency
  ftr  Adds configuration file and white list for alpha support of Vuzix display
0.6.18:
 *ftr  Added CMakeLists.txt and instructions for compiling.
  ftr  Fixed Render() callback-based approach in Direct3D, added example.
  ftr  Added program to demonstrate drawing a small cube in head space
 *bug  Fixed Direct3D programs to render correct eyes at 90 and 270 rotations
 *bug  The code now reads and responds to the swap_eyes entry in display config
0.6.17:
 *ftr  Applications can now register the same buffer for multiple eyes
0.6.16:
  ftr  Added an OpenGL spinning-cube demo that prints frame ID in a HUD
 *bug  Fixed the Render path in OpenGL so objects get drawn in correct spaces
 *bug  Made the distortion mesh much finer to reduce artifacts
 *bug  Turned back on distortion in the second eye (was off for debugging)
 *bug  The code now responds to time warp enable in config (was not being used)
0.6.15:
  ftr  Added Visual Studio merge modules for CRT and MFC to installer
  ftr  Added new shortcut to spin the rotating cube backwards for smoothness debug
0.6.14:
  ftr  Added configuration file to run in a non-directmode mono window
0.6.13:
  ftr  Added program that spins the cube around the vertical axis
0.6.12:
 *ftr  Moves the configuration files to the server side.
 *ftr  Client application sets the type of rendering to use, not config file
  bug  Render routines check to see if the display has been opened to avoid crashing.
  ftr  Removed depth buffer clearing (and existence) from internal rendering pass
  bug  Failure to obtain high-priority rendering now returns PARTIAL rather than FAILURE
0.6.11:
  bug  Includes the Visual Studio redistributable runtime and Direct3D compiler DLL.
0.6.10:
  bug  Fixed matrix-order problem where flipping in Y did not work for 90/270 screen rotations.
  ftr  Turns on vsync by default for the direct-mode examples to avoid tearing.
