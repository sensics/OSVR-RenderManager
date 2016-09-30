/** @file
    @brief Header

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

#ifndef INCLUDED_RenderManagerImpl_h_GUID_D55DD6BB_14DA_4082_4340_C987D7F22660
#define INCLUDED_RenderManagerImpl_h_GUID_D55DD6BB_14DA_4082_4340_C987D7F22660

// Internal Includes
#include <osvr/RenderKit/RenderManagerC.h>
#include <osvr/RenderKit/RenderManager.h>

// Library/third-party includes
// - none

// Standard includes
// - none

namespace {
typedef struct RenderManagerPresentState {
    std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
    std::vector<osvr::renderkit::OSVR_ViewportDescription>
        normalizedCroppingViewports;
    std::vector<osvr::renderkit::RenderInfo> renderInfoUsed;
} RenderManagerPresentState;

typedef struct RenderManagerRegisterBufferState {
    std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
} RenderManagerRegisterBufferState;

typedef struct RenderManagerRenderInfoCollection {
    std::vector<osvr::renderkit::RenderInfo> renderInfo;
} RenderManagerRenderInfoCollection;

inline void
ConvertViewport(const OSVR_ViewportDescription& viewport,
                osvr::renderkit::OSVR_ViewportDescription& viewportOut) {
    viewportOut.height = viewport.height;
    viewportOut.width = viewport.width;
    viewportOut.left = viewport.left;
    viewportOut.lower = viewport.lower;
}

inline void
ConvertViewport(const osvr::renderkit::OSVR_ViewportDescription viewport,
                OSVR_ViewportDescription& viewportOut) {
    viewportOut.height = viewport.height;
    viewportOut.width = viewport.width;
    viewportOut.left = viewport.left;
    viewportOut.lower = viewport.lower;
}

inline void
ConvertProjection(const OSVR_ProjectionMatrix& projection,
                  osvr::renderkit::OSVR_ProjectionMatrix& projectionOut) {
    projectionOut.bottom = projection.bottom;
    projectionOut.farClip = projection.farClip;
    projectionOut.left = projection.left;
    projectionOut.right = projection.right;
    projectionOut.top = projection.top;
    projectionOut.nearClip = projection.nearClip;
}

inline void
ConvertProjection(const osvr::renderkit::OSVR_ProjectionMatrix& projection,
                  OSVR_ProjectionMatrix& projectionOut) {
    projectionOut.bottom = projection.bottom;
    projectionOut.farClip = projection.farClip;
    projectionOut.left = projection.left;
    projectionOut.right = projection.right;
    projectionOut.top = projection.top;
    projectionOut.nearClip = projection.nearClip;
}

inline void ConvertRenderParams(
    const OSVR_RenderParams& renderParams,
    osvr::renderkit::RenderManager::RenderParams& renderParamsOut) {
    renderParamsOut.farClipDistanceMeters = renderParams.farClipDistanceMeters;
    renderParamsOut.nearClipDistanceMeters =
        renderParams.nearClipDistanceMeters;
    renderParamsOut.roomFromHeadReplace = renderParams.roomFromHeadReplace;
    renderParamsOut.worldFromRoomAppend = renderParams.worldFromRoomAppend;
}

template <class OSVR_GraphicsLibraryType, class OSVR_RenderManagerType>
OSVR_ReturnCode
osvrCreateRenderManagerImpl(OSVR_ClientContext clientContext,
                            const char graphicsLibraryName[],
                            OSVR_GraphicsLibraryType graphicsLibrary,
                            OSVR_RenderManager* renderManagerOut,
                            OSVR_RenderManagerType* renderManagerSubtypeOut) {
    osvr::renderkit::GraphicsLibrary _graphicsLibrary;
    ConvertGraphicsLibrary(graphicsLibrary, _graphicsLibrary);

    osvr::renderkit::RenderManager* rm = osvr::renderkit::createRenderManager(
        clientContext, graphicsLibraryName, _graphicsLibrary);

    // @todo implement proper opaque type for OSVR_RenderManager so this cast
    // can be safer.
    *renderManagerOut = reinterpret_cast<OSVR_RenderManager*>(rm);
    // @todo should this be a static cast? we're pure virtual now but we may not
    // be in the future
    *renderManagerSubtypeOut = reinterpret_cast<OSVR_RenderManagerType*>(rm);
    return OSVR_RETURN_SUCCESS;
}

template <class OSVR_RenderInfoType>
OSVR_ReturnCode osvrRenderManagerGetRenderInfoImpl(
      OSVR_RenderManager renderManager, OSVR_RenderInfoCount renderInfoIndex,
      OSVR_RenderParams renderParams, OSVR_RenderInfoType* renderInfoOut) {
    osvr::renderkit::RenderManager::RenderParams _renderParams;
    ConvertRenderParams(renderParams, _renderParams);
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
    auto ri = rm->GetRenderInfo(_renderParams);
    if (renderInfoIndex >= ri.size()) {
        std::cerr << "[OSVR] renderInfoIndex is out of range" << std::endl;
        return OSVR_RETURN_FAILURE;
    }
    auto curRenderInfo = ri[renderInfoIndex];

    auto& _renderInfoOut = *renderInfoOut;
    ConvertGraphicsLibrary(curRenderInfo.library, _renderInfoOut.library);

    ConvertRenderInfo(curRenderInfo, _renderInfoOut);

    return OSVR_RETURN_SUCCESS;
}

template <class OSVR_RenderManagerType, class OSVR_OpenResultsType>
OSVR_ReturnCode
osvrRenderManagerOpenDisplayImpl(OSVR_RenderManagerType renderManager,
                                 OSVR_OpenResultsType* openResultsOut) {
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
    auto results = rm->OpenDisplay();
    if (openResultsOut) {
        auto& _openResultsOut = *openResultsOut;
        switch (results.status) {
        case osvr::renderkit::RenderManager::FAILURE:
            _openResultsOut.status = OSVR_OPEN_STATUS_FAILURE;
            break;
        case osvr::renderkit::RenderManager::PARTIAL:
            _openResultsOut.status = OSVR_OPEN_STATUS_PARTIAL;
            break;
        case osvr::renderkit::RenderManager::COMPLETE:
            _openResultsOut.status = OSVR_OPEN_STATUS_COMPLETE;
            break;
        }
        ConvertGraphicsLibrary(results.library, _openResultsOut.library);
    }
    return results.status == osvr::renderkit::RenderManager::FAILURE
               ? OSVR_RETURN_FAILURE
               : OSVR_RETURN_SUCCESS;
}

template <class OSVR_BufferType, class OSVR_RenderInfoType>
OSVR_ReturnCode osvrRenderManagerPresentRenderBufferImpl(
    OSVR_RenderManagerPresentState presentState, OSVR_BufferType buffer,
    OSVR_RenderInfoType renderInfoUsed,
    OSVR_ViewportDescription normalizedCroppingViewport) {
    RenderManagerPresentState* state =
        reinterpret_cast<RenderManagerPresentState*>(presentState);

    osvr::renderkit::RenderBuffer newBuffer;
    ConvertRenderBuffer(buffer, newBuffer);
    state->renderBuffers.push_back(newBuffer);

    osvr::renderkit::OSVR_ViewportDescription newViewport;
    ConvertViewport(normalizedCroppingViewport, newViewport);
    state->normalizedCroppingViewports.push_back(newViewport);

    osvr::renderkit::RenderInfo newRenderInfoUsed;
    ConvertRenderInfo(renderInfoUsed, newRenderInfoUsed);
    state->renderInfoUsed.push_back(newRenderInfoUsed);
    return OSVR_RETURN_SUCCESS;
}

template <class OSVR_RenderBufferType>
OSVR_ReturnCode osvrRenderManagerRegisterRenderBufferImpl(
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_RenderBufferType renderBuffer) {

    auto state = reinterpret_cast<RenderManagerRegisterBufferState*>(
        registerBufferState);
    osvr::renderkit::RenderBuffer newRenderBuffer;
    ConvertRenderBuffer(renderBuffer, newRenderBuffer);
    state->renderBuffers.push_back(newRenderBuffer);
    return OSVR_RETURN_SUCCESS;
}

template <class OSVR_RenderInfoType>
OSVR_ReturnCode osvrRenderManagerGetRenderInfoFromCollectionImpl(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount index,
    OSVR_RenderInfoType* renderInfoOut) {

    auto ri = reinterpret_cast<RenderManagerRenderInfoCollection*>(renderInfoCollection);
    if (renderInfoCollection && index >= 0 && index < ri->renderInfo.size() && renderInfoOut) {
        ConvertRenderInfo(ri->renderInfo[index], *renderInfoOut);
        return OSVR_RETURN_SUCCESS;
    }
    return OSVR_RETURN_FAILURE;
}

}
#endif // INCLUDED_RenderManagerImpl_h_GUID_D55DD6BB_14DA_4082_4340_C987D7F22660
