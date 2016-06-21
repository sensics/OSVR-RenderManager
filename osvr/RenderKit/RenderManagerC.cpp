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
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>

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

OSVR_ReturnCode osvrRenderManagerPresentSolidColorf(
  OSVR_RenderManager renderManager,
  OSVR_RGB_FLOAT rgb) {
  auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);

  osvr::renderkit::RGBColorf color;
  color.r = rgb.r;
  color.g = rgb.g;
  color.b = rgb.b;
  bool success = rm->PresentSolidColor(color);
  return success ? OSVR_RETURN_SUCCESS : OSVR_RETURN_FAILURE;
}

OSVR_ReturnCode osvrRenderManagerGetRenderInfoCollection(
    OSVR_RenderManager renderManager,
    OSVR_RenderParams renderParams,
    OSVR_RenderInfoCollection* renderInfoCollectionOut) {

    if (renderManager && renderInfoCollectionOut) {
        osvr::renderkit::RenderManager::RenderParams _renderParams;
        ConvertRenderParams(renderParams, _renderParams);
        auto rm = reinterpret_cast<osvr::renderkit::RenderManager*>(renderManager);
        RenderManagerRenderInfoCollection* ret = new RenderManagerRenderInfoCollection();
        ret->renderInfo = rm->GetRenderInfo(_renderParams);
        (*renderInfoCollectionOut) =
            reinterpret_cast<OSVR_RenderInfoCollection*>(ret);
        return OSVR_RETURN_SUCCESS;
    }
    return OSVR_RETURN_FAILURE;
}

OSVR_ReturnCode osvrRenderManagerReleaseRenderInfoCollection(
    OSVR_RenderInfoCollection renderInfoCollection) {

    if (renderInfoCollection) {
        auto ri = reinterpret_cast<RenderManagerRenderInfoCollection*>(renderInfoCollection);
        delete ri;
        return OSVR_RETURN_SUCCESS;
    }
    return OSVR_RETURN_FAILURE;
}

OSVR_ReturnCode osvrRenderManagerGetNumRenderInfoInCollection(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount* countOut) {

    if (renderInfoCollection && countOut) {
        auto ri = reinterpret_cast<RenderManagerRenderInfoCollection*>(renderInfoCollection);
        (*countOut) = ri->renderInfo.size();
        return OSVR_RETURN_SUCCESS;
    }
    return OSVR_RETURN_FAILURE;
}

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_PoseState_to_OpenGL(
  double* OpenGL_out, OSVR_PoseState state_in)
{
  if (!osvr::renderkit::OSVR_PoseState_to_OpenGL(
    OpenGL_out, state_in)) {
    return OSVR_RETURN_FAILURE;
  }
  return OSVR_RETURN_SUCCESS;
}

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_PoseState_to_D3D(
  float D3D_out[16], OSVR_PoseState state_in)
{
  if (!osvr::renderkit::OSVR_PoseState_to_D3D(
    D3D_out, state_in)) {
    return OSVR_RETURN_FAILURE;
  }
  return OSVR_RETURN_SUCCESS;
}

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_PoseState_to_Unity(
  OSVR_PoseState* state_out, OSVR_PoseState state_in)
{
  if (!state_out) { return OSVR_RETURN_FAILURE; }
  if (!osvr::renderkit::OSVR_PoseState_to_Unity(
    *state_out, state_in)) {
    return OSVR_RETURN_FAILURE;
  }
  return OSVR_RETURN_SUCCESS;
}

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_Projection_to_OpenGL(
  double* OpenGL_out, OSVR_ProjectionMatrix projection_in)
{
  osvr::renderkit::OSVR_ProjectionMatrix proj;
  ConvertProjection(projection_in, proj);
  if (!osvr::renderkit::OSVR_Projection_to_OpenGL(
    OpenGL_out, proj)) {
    return OSVR_RETURN_FAILURE;
  }
  return OSVR_RETURN_SUCCESS;
}

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_Projection_to_D3D(
  float D3D_out[16], OSVR_ProjectionMatrix projection_in)
{
  osvr::renderkit::OSVR_ProjectionMatrix proj;
  ConvertProjection(projection_in, proj);
  if (!osvr::renderkit::OSVR_Projection_to_D3D(
    D3D_out, proj)) {
    return OSVR_RETURN_FAILURE;
  }
  return OSVR_RETURN_SUCCESS;
}

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_Projection_to_Unreal(
  float Unreal_out[16], OSVR_ProjectionMatrix projection_in)
{
  osvr::renderkit::OSVR_ProjectionMatrix proj;
  ConvertProjection(projection_in, proj);
  if (!osvr::renderkit::OSVR_Projection_to_Unreal(
    Unreal_out, proj)) {
    return OSVR_RETURN_FAILURE;
  }
  return OSVR_RETURN_SUCCESS;
}
