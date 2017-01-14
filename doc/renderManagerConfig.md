# OSVR RenderManager Configuration

This document describes the values found in the **renderManagerConfig** portion of the OSVR server's Json configuration file and how RenderManager uses them and other entries to do its rendering.

## Introduction

RenderManager either constructs render buffers for the user (in callback mode, which is seldom used) or describes to the application what format those buffers should be in.
It also provides descriptions of the viewports, projection matrices, and modelview matrices needed for rendering into those buffers to support all operations.  These must all be used together for effective rendering.
It includes options for overfill (useful for enabling time warp without black borders) and oversampling (useful for increasing rendering rate or for increased resolution in areas where the optics magnify the screen).

RenderManager provides a description of a texture that is upright when viewed in the HMD, regardless of whether the scan-out circuitry or operating system rotates that viewport before presentation.  (This enables the use of bitmap sprites and other rendering techniques that rely on texture orientation.)
It then internally adjusts the projection geometry and texture coordinates during its warping pass to provide any rotation or flipping required by the hardware, graphics library interop, or other parameter on the way to the final texure that is handed to graphics system.

Individual entries, and their meanings, are described below.  They are nested in the configuration file.

## renderManagerConfig

This entry is actually itself nested, there is an external one with a **meta** tag that has "schemeVersion": 1 and then another **renderManagerConfig** entry within it.  The entries within the inner nesting are described here, with the ones directly under it first and then the nestings as subheadings:

* directModeEnabled: If true, RenderManager will attempt to open a DirectMode display using any of the compiled-in interfaces.  As of 11/16/2016, this includes nVidia VRWorks DirectMode, AMD Liquid VR, and an upcoming Microsoft API that is not yet released.  If it cannot find a display with the resolution specfied at the appropriate orientation (see **renderManagerConfig/display/rotation** and **display/hmd/resolutions**) then it will fail and return an error condition.

* directDisplayIndex: [Ignored if not in DirectMode] The index of the available displays matching the requested resolution, starting with 0.  If there are multiple displays being used (full\_screen and 2 eyes), the indices of the following displays will be incremented by 1.

* directHighPriorityEnabled: [Ignored if not in DirectMode] For video drivers that support high-priority rendering threads (only nVidia Pascal series are supported as of 11/16/2016), makes a high-priority thread for the DirectMode rendering thread.  If using ATW, this is the separate ATW thread.

* numBuffers: The number of buffers per swap chain, 1 = front-buffer rendering (not recommended), 2 = double buffered (which does not increase latency in DirectMode if waiting for vsync), 3+ = superfluous.

* verticalSyncEnabled: The presentation of the surfaces will wait for vertical sync, preventing image "earing".

* verticalSyncBlockRenderingEnabled: [Ignored if not in DirectMode] When the application presents a set of render buffers, this will be a blocking call that will not return until after the surfaces have been presented to the eye.  Useful to throttle applications with rapid rendering times.  It avoids overtaxing the GPU but busy-waits, so still uses an entire CPU core.

* renderOverfillFactor: Described in the *Overfill* section of the document at https://github.com/OSVR/OSVR-Docs/blob/master/Configuring/distortion.md  This parameter affects both the viewport size and the projection matrix, requesting a larger field of view than is requires for final rendering when >1.

* renderOversampleFactor: This parameter controls the ratio of texture pixels to display pixels.  Setting it larger than 1 results in finer-scaled rendering that will still provide full detail in regions of the display that the lenses magnify.  Setting it lower than 1 results in potentially faster rendering rates at the expense of visual detail in the rendered images.  This affects the viewport but not the projection matrix, since it is rendering the same region but doing so at a different resolution.

### window

This section describes the window created by RenderManager when it is in extended mode.  It is ignored when using DirectMode, as there is no window to be managed.

* title: The OS title for the window.

* fullScreenEnabled: Should the window take up the full screen?  If so, the **display/hmd/resolutions** are ignored.

* xPosition: Location of the upper-left corner in the virtual screen coordinates of the window manager.

* yPosition: Location of the upper-left corner in the virtual screen coordinates of the window manager.

### display

This section describes more features of the display.

* rotation: This describes how both the texture presented by the application and the physical display surface should be rotated.  It should be 0, 90, 180, or 270.  For 90 and 270, the **display/hmd/resolutions/width** and **../height** will be swapped to determine the aspect ration of the window to open or the DirectMode display to open.  For example: The portait configuration file for the OSVR HDK 1.3 has **display/hmd/resolutions/width**=1920 and height=1080 with a rotation of 90.  Assuming overfill=1 and oversample=1 and horizontal side-by-side presentation, this results in a render target that is 1920x1080 but a window (or DirectMode display) of 1080x1920.  The landscape configuration, which has rotation=0 has both a render target and a window/display resolution of 1920x1080.

* bitsPerColor: Should always be 8.

### prediction

This section describes the client-side prediction to be applied during rendering, both in DirectMode and in Extended mode.  This adds on to any server-side prediction, so it is recommended that when it is enabled, server-side prediction be set to predict to the time that the measurement was sent (and the time it is tagged at) by the server.

* enabled: If true, client-side prediction is enabled.  If false, it is not.

* staticDelayMS: Number of milliseconds to predict ahead for both eyes, in addition to the actual time required before rendering for a given frame.  In DirectMode, this should only include fixed delays (uncompensated tracker latency, for example) whereas in non-DirectMode, it should include the estimated time between rendering completion and presentation (which may be >1 frame for pipelined rendering).

* leftEyeDelayMS: For displays that scan out side to side, the left and right eyes may not be scanned out at the same time.  This parameter tells how long the left eye starts to scan out after the entire display is presented for the case of 2 eyes per display.

* rightEyeDelayMS: For displays that scan out side to side, the left and right eyes may not be scanned out at the same time.  This parameter tells how long the right eye starts to scan out after the entire display is presented for the case of 2 eyes per display.

* localTimeOverride: If true, the local time of arrival for a tracker message is taken as its actual time.  This is a hack to work around the fact that before Visual Studio 2015 VRPN did not support synchronized clocks across process boundaries on Windows.

### timeWarp

This section describes the operation of time warp, which re-projects textures from new viewports based on (potentially predicted) tracker position changes received after rendering started but before image scan-out.

* enabled: True to enable time warp, false to disable it.

* asynchronous: If time warp is enabled, true to enable asynchronous time warp and false to disable it.  As of 11/16/2016, asynchronous time warp is only available on DirectMode displays and is only effective on nVidia Pascal-series cards (Geforce 10-series).  This launches a separate display thread that will re-present a previous frame in case the application does not finish presenting a new one in time to warp and present before vsync.

* maxMsBeforeVsync: If >0, this causes surface presentation to block until at most this many milliseconds before vsync.  It is primarily useful for ATW, in which case it describes additional padding before vsync for the time-warp thread.  Values around 3-5 have proven to be optimal on some displays and applications as of 11/16/2016, but this is an active area of development and optimization.

## Fields from the display config

RenderManager also makes use of some fields from the **"display"/"hmd"** description during rendering:

* field\_of\_view: The entries here are used to describe the field of view and angular tilt of the HMD.

### resolutions

* width/height: These are used along with **renderManagerConfig/renderOverfillFactor** and **renderMaangerConfig/renderOversampleFactor** to determine the resolution and aspect ratio of the texture provided to the application to render into.  If **rendeManager/display/rotation** is 90 or 270, the aspect ratio of the window asked for from the display system (or the display opened in DirectMode) will have its aspect ratio swapped.

* display\_mode: This is used to determine how many eyes to pack into each display, and what the relative locations of these eyes are.

* swap\_eyes: If this is 1, the texture that would normally go to the left eye is sent to the right eyes, and vice-versa.  This impacts both the case where there are two eyes in a display and the case where there is a display per eye.

### distortion

Described in the *Example: Using a screen-to-angle table* section of the document at https://github.com/OSVR/OSVR-Docs/blob/master/Configuring/distortion.md

### rendering

Described in the *Example: Using a screen-to-angle table* section of the document at https://github.com/OSVR/OSVR-Docs/blob/master/Configuring/distortion.md

### eyes

Described in the *Example: Using a screen-to-angle table* section of the document at https://github.com/OSVR/OSVR-Docs/blob/master/Configuring/distortion.md

