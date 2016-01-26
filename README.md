# OSVR-RenderManager

This repository holds the open-source code for the OSVR RenderManager developed by Sensics.  It is licensed under the Apache-2 license.

It has a set of submodules that point to sets of non-open source code to build vendor-specific extensions to support direct rendering.  These were developed under non-disclosure agreements with the vendors.

This code is built using CMake, and as of 1/26/2016 compiled on Windows, Linux (tested on Ubuntu) and Mac-OSX.  The Linux and Mac ports only support OpenGL and do not support direct-to-display (DirectMode) rendering.

As of 1/26/2016, the mac port does not run because of the lack of a compatibility OpenGL library.  It needs to be modified to use the Core library (needed for the distortion-correction shaders) and to not include the legacy OpenGL examples.

