/** @file
    @brief Implementation

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include <osvr/RenderKit/RenderManagerOpenGLC.h>
#include <osvr/RenderKit/RenderManager.h>
#include <osvr/RenderKit/RenderManagerImpl.h>
#include <osvr/RenderKit/RenderManagerOpenGL.h>
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>

// Library/third-party includes
// - none

// Standard includes
// - none

inline void
ConvertGraphicsLibrary(const OSVR_GraphicsLibraryOpenGL& graphicsLibrary,
                       osvr::renderkit::GraphicsLibrary& graphicsLibraryOut) {
  // If we have non-NULL entries for either, then we construct a
  // place to put them.  Otherwise, we leave the device with its
  // default NULL pointer so that it will not think the contents
  // are valid.
  if (graphicsLibrary.toolkit != nullptr) {
    osvr::renderkit::GraphicsLibraryOpenGL *glogl = new
      osvr::renderkit::GraphicsLibraryOpenGL();
    glogl->toolkit = graphicsLibrary.toolkit;
    graphicsLibraryOut.OpenGL = glogl;
  }
}

inline void
ConvertGraphicsLibrary(const osvr::renderkit::GraphicsLibrary& graphicsLibrary,
                       OSVR_GraphicsLibraryOpenGL& graphicsLibraryOut) {
    if (graphicsLibrary.OpenGL) {
      graphicsLibraryOut.toolkit = graphicsLibrary.OpenGL->toolkit;
    }
}

inline void
ConvertRenderBuffer(const OSVR_RenderBufferOpenGL& renderBuffer,
                    osvr::renderkit::RenderBuffer& renderBufferOut) {
     renderBufferOut.OpenGL = new osvr::renderkit::RenderBufferOpenGL();
     renderBufferOut.OpenGL->colorBufferName = renderBuffer.colorBufferName;
     renderBufferOut.OpenGL->depthStencilBufferName =
     renderBuffer.depthStencilBufferName;
}

inline void
ConvertRenderBuffer(const osvr::renderkit::RenderBuffer& renderBuffer,
                    OSVR_RenderBufferOpenGL& renderBufferOut) {
    if (renderBuffer.OpenGL) {
         renderBufferOut.colorBufferName =
         renderBuffer.OpenGL->colorBufferName;
         renderBufferOut.depthStencilBufferName =
         renderBuffer.OpenGL->depthStencilBufferName;
    }
}

inline void ConvertRenderInfo(const OSVR_RenderInfoOpenGL& renderInfo,
                              osvr::renderkit::RenderInfo& renderInfoOut) {
    renderInfoOut.pose = renderInfo.pose;
    ConvertProjection(renderInfo.projection, renderInfoOut.projection);
    ConvertViewport(renderInfo.viewport, renderInfoOut.viewport);
    ConvertGraphicsLibrary(renderInfo.library, renderInfoOut.library);
}

inline void ConvertRenderInfo(const osvr::renderkit::RenderInfo& renderInfo,
                              OSVR_RenderInfoOpenGL& renderInfoOut) {
    renderInfoOut.pose = renderInfo.pose;
    ConvertProjection(renderInfo.projection, renderInfoOut.projection);
    ConvertViewport(renderInfo.viewport, renderInfoOut.viewport);
    ConvertGraphicsLibrary(renderInfo.library, renderInfoOut.library);
}

OSVR_ReturnCode osvrCreateRenderManagerOpenGL(
    OSVR_ClientContext clientContext, const char graphicsLibraryName[],
    OSVR_GraphicsLibraryOpenGL graphicsLibrary,
    OSVR_RenderManager* renderManagerOut,
    OSVR_RenderManagerOpenGL* renderManagerOpenGLOut) {
    return osvrCreateRenderManagerImpl(clientContext, graphicsLibraryName,
                                       graphicsLibrary, renderManagerOut,
                                       renderManagerOpenGLOut);
}

OSVR_ReturnCode osvrRenderManagerGetRenderInfoOpenGL(
    OSVR_RenderManager renderManager, OSVR_RenderInfoCount renderInfoIndex,
    OSVR_RenderParams renderParams, OSVR_RenderInfoOpenGL* renderInfoOut) {
    return osvrRenderManagerGetRenderInfoImpl(renderManager, renderInfoIndex,
                                              renderParams, renderInfoOut);
}

OSVR_ReturnCode
osvrRenderManagerOpenDisplayOpenGL(OSVR_RenderManagerOpenGL renderManager,
                                   OSVR_OpenResultsOpenGL* openResultsOut) {
    return osvrRenderManagerOpenDisplayImpl(renderManager, openResultsOut);
}

OSVR_ReturnCode osvrRenderManagerPresentRenderBufferOpenGL(
    OSVR_RenderManagerPresentState presentState, OSVR_RenderBufferOpenGL buffer,
    OSVR_RenderInfoOpenGL renderInfoUsed,
    OSVR_ViewportDescription normalizedCroppingViewport) {
    return osvrRenderManagerPresentRenderBufferImpl(
        presentState, buffer, renderInfoUsed, normalizedCroppingViewport);
}

OSVR_ReturnCode osvrRenderManagerRegisterRenderBufferOpenGL(
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_RenderBufferOpenGL renderBuffer) {
    return osvrRenderManagerRegisterRenderBufferImpl(registerBufferState,
                                                     renderBuffer);
}

OSVR_ReturnCode osvrRenderManagerGetRenderInfoFromCollectionOpenGL(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount index,
    OSVR_RenderInfoOpenGL* renderInfoOut) {
    return osvrRenderManagerGetRenderInfoFromCollectionImpl(
        renderInfoCollection, index, renderInfoOut);
}
