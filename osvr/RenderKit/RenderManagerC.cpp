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
#include <osvr/RenderKit/RenderManager.h>
#include <osvr/RenderKit/RenderManagerImpl.h>

// Library/third-party includes
/* none */

// Standard includes
#include <iostream>
#include <vector>

OSVR_ReturnCode osvrDestroyRenderManager(OSVR_RenderManager renderManager) {
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
    delete rm;
    return OSVR_RETURN_SUCCESS;
}

OSVR_ReturnCode
osvrRenderManagerGetNumRenderInfo(OSVR_RenderManager renderManager,
                                  OSVR_RenderParams renderParams,
                                  OSVR_RenderInfoCount* numRenderInfoOut) {
    osvr::renderkit::RenderManager::RenderParams _renderParams;
    ConvertRenderParams(renderParams, _renderParams);
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
    auto ri = rm->GetRenderInfo(_renderParams);
    *numRenderInfoOut = ri.size();
    return OSVR_RETURN_SUCCESS;
}

OSVR_ReturnCode
osvrRenderManagerGetDoingOkay(OSVR_RenderManager renderManager) {
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
    return rm->doingOkay() ? OSVR_RETURN_SUCCESS : OSVR_RETURN_FAILURE;
}

OSVR_ReturnCode
osvrRenderManagerGetDefaultRenderParams(OSVR_RenderParams* renderParamsOut) {
    auto& _renderParamsOut = *renderParamsOut;
    _renderParamsOut.nearClipDistanceMeters = 0.1;
    _renderParamsOut.farClipDistanceMeters = 100.0f;
    _renderParamsOut.worldFromRoomAppend = nullptr;
    _renderParamsOut.roomFromHeadReplace = nullptr;
    return OSVR_RETURN_SUCCESS;
}

OSVR_ReturnCode osvrRenderManagerStartPresentRenderBuffers(
    OSVR_RenderManagerPresentState* presentStateOut) {
    RenderManagerPresentState* presentState = new RenderManagerPresentState();
    (*presentStateOut) =
        reinterpret_cast<OSVR_RenderManagerPresentState*>(presentState);
    return OSVR_RETURN_SUCCESS;
}

OSVR_ReturnCode osvrRenderManagerFinishPresentRenderBuffers(
    OSVR_RenderManager renderManager,
    OSVR_RenderManagerPresentState presentState, OSVR_RenderParams renderParams,
    OSVR_CBool shouldFlipY) {
    auto state = reinterpret_cast<RenderManagerPresentState*>(presentState);
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);

    osvr::renderkit::RenderManager::RenderParams _renderParams;
    ConvertRenderParams(renderParams, _renderParams);

    rm->PresentRenderBuffers(state->renderBuffers, state->renderInfoUsed,
                             _renderParams, state->normalizedCroppingViewports,
                             shouldFlipY == OSVR_TRUE);

    return OSVR_RETURN_SUCCESS;
}

OSVR_ReturnCode osvrRenderManagerStartRegisterRenderBuffers(
    OSVR_RenderManagerRegisterBufferState* registerBufferStateOut) {
    RenderManagerRegisterBufferState* ret =
        new RenderManagerRegisterBufferState();
    *registerBufferStateOut =
        reinterpret_cast<OSVR_RenderManagerRegisterBufferState>(ret);
    return OSVR_RETURN_SUCCESS;
}

OSVR_ReturnCode osvrRenderManagerFinishRegisterRenderBuffers(
    OSVR_RenderManager renderManager,
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_CBool appWillNotOverwriteBeforeNewPresent) {
    auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
    auto state = reinterpret_cast<RenderManagerRegisterBufferState*>(
        registerBufferState);
    bool success = rm->RegisterRenderBuffers(
        state->renderBuffers, appWillNotOverwriteBeforeNewPresent == OSVR_TRUE);
    delete state;
    return success ? OSVR_RETURN_SUCCESS : OSVR_RETURN_FAILURE;
}

OSVR_ReturnCode osvrRenderManagerPresentSolidColor(
  OSVR_RenderManager renderManager,
  float rgb[3]) {
  auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);

  std::array<float, 3> color = { rgb[0], rgb[1], rgb[2] };
  bool success = rm->PresentSolidColor(color);

  return success ? OSVR_RETURN_SUCCESS : OSVR_RETURN_FAILURE;
}

