# OSVR-RenderManager

[![Join the chat at https://gitter.im/sensics/OSVR-RenderManager](https://badges.gitter.im/sensics/OSVR-RenderManager.svg)](https://gitter.im/sensics/OSVR-RenderManager?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This repository holds the open-source code for the OSVR RenderManager developed by
Sensics.  It is licensed under the Apache-2 license.  

RenderManager is an API for rendering and presenting graphics for virtual reality. There is a detailed description below, but in short, it is a codebase that contains:
- Rendering code that performs time-warp, distortion correction, (per-eye) client side prediction, for multiple graphics APIs
- Distortion mesh creation from a variety of distortion descriptions
- An output backend for rendering in extended or direct mode on a range of platforms and drivers

It has a set of submodules that point to sets of non-open source code to build
vendor-specific extensions to support direct rendering.  These were developed
under non-disclosure agreements with the vendors.

Most Windows users will want to install the precompiled binaries (which include
the DirectMode interfaces for nVidia, AMD, and Intel graphics cards) using the
installers provided at the [developer downloads site](http://osvr.github.io/using/).

Those who don't have access to the NDA repositories get using:

```
git clone https://github.com/sensics/OSVR-RenderManager.git
cd OSVR-RenderManager
git submodule init vendor/vrpn
git submodule update
```

Sensics internal users, who have access to the NDA repos, get using:

```
git clone --recursive git@github.com:sensics/OSVR-RenderManager.git
```

This code is built using CMake, and as of 2/23/2016 compiled on Windows, Linux
(tested on Ubuntu) and Mac-OSX.  It makes use of SDL2 to construct windows and
GLEW to set OpenGL settings, so install those packages before compiling.  It
also requires Eigen3, which is built as part of OSVR or can be installed
separately.  The Linux and Mac ports only support OpenGL and
do not yet support direct-to-display (DirectMode) rendering.  The Android compile
is done using the [OSVR-Android-Build](https://github.com/OSVR/OSVR-Android-Build)
project.

## What RenderManager Provides

RenderManager provides a number of functions beyond the OSVR-Core library in support
of VR rendering.  It wraps the Core functions in an easy-to-use interface that
implements many VR-specific needs.

**DirectMode:** On platforms that support it, RenderManager implements direct
rendering to the display, bypassing operating-system delays and enabling front-
buffer rendering.  On Windows, this is implemented using nVidia's *VR Direct
Mode* rendering, AMD's *Direct-to-Display* rendering, and on Intel cards via
a new Windows rendering interface that supports DirectMode.  These share a common
interface in RenderManager and plans are underway to extend these to new operating
systems as they become available.  DirectMode supports both D3D11 and OpenGL (core
and legacy) on Windows except that OpenGL is not yet supported on Intel cards
because they do not yet support the OpenGL/D3D interop calls.

The following capabilities are provided on all supported platforms:

* **Distortion correction:** This enables undistortion of HMD lenses.
Configuration files can be used to specify the type of
distortion correction used, from several choices: *rgb polynomial away from a center*,
*monochromatic unstructured mesh*, and *rgb unstructured mesh*.  RenderManager
includes distortion-mesh-construction for all of its rendering libraries based on
all of the above input types.  See RenderManager.h for more information.

* **Time Warp:** Synchronous time warp is provided on all platforms.  This is done
at the same time as the distortion-correction render pass by reading the latest
tracking information and adjusting the viewing transformation using the texture
matrix to fix up changes due to motion between the start of rendering and its
completion.  This warping is geometrically correct for strict rotations around
the center of projection and is approximated by a 2-meter distance for translations.

* **Asynchronous Time Warp** is supported under DirectMode configurations.  This
mode is enabled by a configuration-file setting and does not require code change.
It produces a separate rendering thread that re-warps and re-renders images at
full rate even when the application renders too slowly to present a new image each frame.
Applications that want to avoid an extra copy of their render buffer to the
ATW thread can do internal double-buffered rendering and present alternate sets
of buffers.  An example D3D11 program doing this ATW double-buffered rendering
is available in the examples directory.

* **Client-side prediction:** When used with trackers that report velocity or
angular velocity, RenderManager predicts the time each eye will be rendered based
on the next upcoming vertical retrace.  The transform associated with each eye
is adjusted to suit the time at which it will be rendered.  This prediction
happens for both time-warped and non-time-warped presentation and can use a
different delay for each eye (supporting systems in portrait mode).  This
capability is enabled using a setting in the configuration file.

* **Rendering state:** RenderManager produces graphics-language-specific conversion
functions to describe the number and size of required textures, the viewports,
projection and ModelView matrices needed to configure rendering for scenes.
Configuration files specify the number of eyes, whether they are in a single screen
or multiple screens, and their relative orientations.  RenderManager takes all
viewports and textures in their canonical (up is up) orientation and internally
maps to the correct orientation, enabling the use of bitmap fonts and other
rendering effects that require canonical orientation.  An optional, callback-based
rendering path provides these transformations for arbitrary spaces within the
OSVR configuration space (head space, hand space, room space, etc.).

* **Window creation:** RenderManager uses SDL on Windows, Linux, and Mac systems
to construct windows of the appropriate size for a given HMD or on-screen display.
Configuration file entries describe window size, placement, and orientation.  For
non-DirectMode operation, these show up within the operating system's virtual screen and can
be either full-screen or windowed.  For DirectMode operation, they provide full-
screen operation on one or more displays.

* **OverFill & Oversampling:** To enable time warp to work, the rendered view must
be larger than the image to be presented on a given frame.  This provides a border
around the image that can be pulled in as the user's viewport rotates.  Also, the
distortion caused by lenses in VR systems can cause a magnification of the screen
that requires the application to render pixels at a higher density than the physical
display.  RenderManager handles both of these capabilities internally, hiding them
from the application.  Configuration file entries can adjust these; trading rendering
speed for performance at run time without changes to the code.

## Coming Soon

**Android** support is under development.  As of 2/23/2016, the OpenGL internal
code is all compatible with OpenGL ES 2.0 and there is an OpenGLES example
application that build and links (not yet tested).  Work is underway to port RenderManager
to Android on top of the existing OSVR-Core port.

**DirectMode/Linux** is planned as graphics-card vendors finish drivers
to enable it on this platform.  It is being designed to use the same RenderManager
interface and configuration files as the current Windows implementations.

## Two RenderManager Interfaces

RenderManager provides two different interfaces, a *Get/Present* interface and a
*Callback* interface.  Example applications are provided that use each.  The
Callback interface provides the ability to easily render objects in multiple
spaces (head space, hand space, etc.).  The Get/Present interface lets the
application have complete control over render-buffer construction.

## Example programs

There are a number of example programs that highlight the different RenderManager
interfaces and features.

## Optimizing timing

See the [Rendering Optimization](./doc/renderingOptimization.md) document for more information on how to optimize rendering performance using RenderManager.
