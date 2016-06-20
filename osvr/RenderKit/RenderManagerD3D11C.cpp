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
#include <osvr/RenderKit/RenderManagerC.h>
#include <osvr/RenderKit/RenderManager.h>
#include <osvr/RenderKit/RenderManagerD3D11C.h>
#include <osvr/RenderKit/RenderManagerImpl.h>
#include <osvr/RenderKit/GraphicsLibraryD3D11.h>

// Library/third-party includes
// - none

// Standard includes
// - none

namespace {
inline void
ConvertGraphicsLibrary(const OSVR_GraphicsLibraryD3D11& graphicsLibrary,
                       osvr::renderkit::GraphicsLibrary& graphicsLibraryOut) {
    // If we have non-NULL entries for either, then we construct a
    // place to put them.  Otherwise, we leave the device with its
    // default NULL pointer so that it will not think the contents
    // are valid.
    if (graphicsLibrary.device != nullptr ||
        graphicsLibrary.context != nullptr) {
      osvr::renderkit::GraphicsLibraryD3D11* gld3d11 =
        new osvr::renderkit::GraphicsLibraryD3D11();
      gld3d11->device = graphicsLibrary.device;
      gld3d11->context = graphicsLibrary.context;
      graphicsLibraryOut.D3D11 = gld3d11;
    }
}

inline void
ConvertGraphicsLibrary(const osvr::renderkit::GraphicsLibrary& graphicsLibrary,
                       OSVR_GraphicsLibraryD3D11& graphicsLibraryOut) {
    if (graphicsLibrary.D3D11) {
        graphicsLibraryOut.device = graphicsLibrary.D3D11->device;
        graphicsLibraryOut.context = graphicsLibrary.D3D11->context;
    } else {
        graphicsLibraryOut.device = nullptr;
        graphicsLibraryOut.context = nullptr;
    }
}

inline void
ConvertRenderBuffer(const OSVR_RenderBufferD3D11& renderBuffer,
                    osvr::renderkit::RenderBuffer& renderBufferOut) {
    renderBufferOut.D3D11 = nullptr;
    renderBufferOut.OpenGL = nullptr;
    renderBufferOut.D3D11 = new osvr::renderkit::RenderBufferD3D11();
    renderBufferOut.D3D11->colorBuffer = renderBuffer.colorBuffer;
    renderBufferOut.D3D11->colorBufferView = renderBuffer.colorBufferView;
    renderBufferOut.D3D11->depthStencilBuffer = renderBuffer.depthStencilBuffer;
#pragma warning(suppress : 6011)
    renderBufferOut.D3D11->depthStencilView = renderBuffer.depthStencilView;
}

inline void
ConvertRenderBuffer(const osvr::renderkit::RenderBuffer& renderBuffer,
                    OSVR_RenderBufferD3D11& renderBufferOut) {
    if (renderBuffer.D3D11) {
        renderBufferOut.colorBuffer = renderBuffer.D3D11->colorBuffer;
        renderBufferOut.colorBufferView = renderBuffer.D3D11->colorBufferView;
        renderBufferOut.depthStencilBuffer =
            renderBuffer.D3D11->depthStencilBuffer;
        renderBufferOut.depthStencilView = renderBuffer.D3D11->depthStencilView;
    }
}

inline void ConvertRenderInfo(const OSVR_RenderInfoD3D11& renderInfo,
                              osvr::renderkit::RenderInfo& renderInfoOut) {
    renderInfoOut.pose = renderInfo.pose;
    ConvertProjection(renderInfo.projection, renderInfoOut.projection);
    ConvertViewport(renderInfo.viewport, renderInfoOut.viewport);
    ConvertGraphicsLibrary(renderInfo.library, renderInfoOut.library);
}

inline void ConvertRenderInfo(const osvr::renderkit::RenderInfo& renderInfo,
                              OSVR_RenderInfoD3D11& renderInfoOut) {
    renderInfoOut.pose = renderInfo.pose;
    ConvertProjection(renderInfo.projection, renderInfoOut.projection);
    ConvertViewport(renderInfo.viewport, renderInfoOut.viewport);
    ConvertGraphicsLibrary(renderInfo.library, renderInfoOut.library);
}
}

OSVR_ReturnCode
osvrCreateRenderManagerD3D11(OSVR_ClientContext clientContext,
                             const char graphicsLibraryName[],
                             OSVR_GraphicsLibraryD3D11 graphicsLibrary,
                             OSVR_RenderManager* renderManagerOut,
                             OSVR_RenderManagerD3D11* renderManagerD3D11Out) {
    return osvrCreateRenderManagerImpl(clientContext, graphicsLibraryName,
                                       graphicsLibrary, renderManagerOut,
                                       renderManagerD3D11Out);
}

/// @todo Make this read from a cache, and remove the need for a renderParams
/// to be passed in.
OSVR_ReturnCode osvrRenderManagerGetRenderInfoD3D11(
    OSVR_RenderManager renderManager, OSVR_RenderInfoCount renderInfoIndex,
    OSVR_RenderParams renderParams, OSVR_RenderInfoD3D11* renderInfoOut) {
    return osvrRenderManagerGetRenderInfoImpl(renderManager, renderInfoIndex,
                                              renderParams, renderInfoOut);
}

OSVR_ReturnCode
osvrRenderManagerOpenDisplayD3D11(OSVR_RenderManagerD3D11 renderManager,
                                  OSVR_OpenResultsD3D11* openResultsOut) {
    return osvrRenderManagerOpenDisplayImpl(renderManager, openResultsOut);
}

OSVR_ReturnCode osvrRenderManagerPresentRenderBufferD3D11(
    OSVR_RenderManagerPresentState presentState, OSVR_RenderBufferD3D11 buffer,
    OSVR_RenderInfoD3D11 renderInfoUsed,
    OSVR_ViewportDescription normalizedCroppingViewport) {
    return osvrRenderManagerPresentRenderBufferImpl(
        presentState, buffer, renderInfoUsed, normalizedCroppingViewport);
}

OSVR_ReturnCode osvrRenderManagerRegisterRenderBufferD3D11(
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_RenderBufferD3D11 renderBuffer) {
    return osvrRenderManagerRegisterRenderBufferImpl(registerBufferState,
                                                     renderBuffer);
}

OSVR_ReturnCode osvrRenderManagerGetRenderInfoFromCollectionD3D11(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount index,
    OSVR_RenderInfoD3D11* renderInfoOut) {
    return osvrRenderManagerGetRenderInfoFromCollectionImpl(
        renderInfoCollection, index, renderInfoOut);
}
