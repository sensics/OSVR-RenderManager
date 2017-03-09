# Overriding methods in OSVR RenderManagerD3D11Base

This document describes how to derive a class from the **RendermanagerD3D11Base** class to provide an implementation that handles its own D3D11 device and context creation, perhaps getting its information from an external Compositor interface.  This is also useful for derived classes that want to implement distortion correction, time warp, or other features of RenderManager in a different way than the base class does.

## Introduction

All of the DirectMode implementations available in OSVR as of 3/9/2017 are based on the Direct3D11Base class.  The DirectMode vendor drivers all derive from this class and the OpenGL DirectMode paths wrap around objects from this base class.

The Direct3D11Base class is fairly heavy-weight, owing to its need to implement the Render() path, and because as much of the common work to support D3D rendering as possible has been implemented in methods in the base class.  This makes it easier to write a thin wrapper class for new DirectMode implementations, but requires overriding methods when the standard functionality is being replaced.

The RenderManagerD3D11 class defined in RenderManagerD3D.cpp provides an example of how to override a subset of the base-class methods to provide an actual interface on top of them.  The required steps are described below.

## Basic implementation of the Present() path

**OpenDisplay()** To provide output to its own real or virtual display, the derived class must override the *OpenDisplay()* method.  This method must set the *m_D3D11device* and *m_D3D11Context* along with constructing these entries in *m_library.D1D11* (including allocating the D3D11 entry itself).

**PresentDisplayInitialize()** must be overridden to call *OMSetRnderTargets()* to point rendering to each display.

**PresentDisplayFinalize()** and/or **PresentFrameFinalize()** must be overridden to cause the swap chain to complete and render the image.  Some DirectMode drivers do this work in one of these calls and some in the other.  External compositors may do no work in these methods, because it will be happening on the presented render buffers in the remote compositor.

**SolidColorEye()** must be overridden to display a solid color in the specified eye.

## Overriding additional capabilities

**PresentRenderBuffersInternal()** is a protected method that can be overridden if the device is going to do its own time warp and buffer orientation correction (rotations by 90, 180, or 270 degrees; flipping buffers).

**GetTimingInfo()** can be overridden to provide information about how long each eye has until it needs to be presented to make the next vertical sync.

**UpdateDistortionMeshesInternal()** If the class implements distortion correction itself, it should override the *UpdateDistortionMeshesInternal()* function and have it simply return true.

**GetRenderInfoInternal()** can be overridden by derived classes that are talking to an external compositor to provide the rendering information.

**RegisterRenderBuffersInternal()** can be overridden by derived classes that have to take a special action when a buffer is registered (for example, the OpenGL/D3D wrapping class has to share the buffer between OpenGL and D3D).

## Render() path

To support the Render() path, the derived class must either call *RenderManagerD3D11Base::OpenDisplay()* in its *OpenDisplay()* routine or else set the device and context and then construct the vertex and shader programs that are to be used in the Render() path.  Before calling this method, it should set the m_params.m_graphicsLibrary.D3D11 device and context values to those that it wants to use, if it is going to create its own.  It must set up the Render-target views that will be used to render into real or virtual displays here.

The derived class must either not override or must properly re-implement a complete Render() approach for the following methods: *RenderPathSetup()*, *RenderSpace()*, *RenderEyeInitialize()*, and *RenderFrameFinalize()*.

It can safely provide overrides that do nothing but return true for the Render functions that are not defined in the base class, including *RenderDisplayInitialize()* and *RenderEyeFinalize()*.

