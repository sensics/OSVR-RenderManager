/** @file
    @brief Header

    Must be c-safe!

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
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
*/

#ifndef INCLUDED_RenderManagerC_h_GUID_F235863C_3572_439D_C8A0_D15AE74A22D8
#define INCLUDED_RenderManagerC_h_GUID_F235863C_3572_439D_C8A0_D15AE74A22D8

/* Internal Includes */
#include <osvr/RenderKit/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/StdInt.h>
#include <osvr/Util/ClientReportTypesC.h>
#include <osvr/Util/ClientOpaqueTypesC.h>
#include <osvr/Util/BoolC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */
#include <stdlib.h>

OSVR_EXTERN_C_BEGIN

// @todo These typedefs might be better off in a separate header.

// @todo implement proper opaque types?
typedef void* OSVR_RenderManager;
typedef void* OSVR_RenderManagerPresentState;
typedef void* OSVR_RenderManagerRegisterBufferState;
typedef void* OSVR_RenderInfoCollection;
typedef size_t OSVR_RenderInfoCount;

// @todo could we use this for the C++ API as well?
typedef struct OSVR_RenderParams {
    // not sure why the original struct had pointers here
    OSVR_PoseState* worldFromRoomAppend; //< Room space to insert
    OSVR_PoseState* roomFromHeadReplace; //< Overrides head space
    double nearClipDistanceMeters;
    double farClipDistanceMeters;
} OSVR_RenderParams;

//=========================================================================
/// Description needed to construct an off-axes projection matrix
typedef struct OSVR_ProjectionMatrix {
    double left;
    double right;
    double top;
    double bottom;
    double nearClip; //< Cannot name "near" because Visual Studio keyword
    double farClip;
} OSVR_ProjectionMatrix;

//=========================================================================
/// Viewport description with lower-left corner of the screen as (0,0)
typedef struct OSVR_ViewportDescription {
    double left;   //< Left side of the viewport in pixels
    double lower;  //< First pixel in the viewport at the bottom.
    double width;  //< Last pixel in the viewport at the top
    double height; //< Last pixel on the right of the viewport in pixels
} OSVR_ViewportDescription;

// ========================================================================
// Float representation of an rgb color (without alpha)
typedef struct OSVR_RGB {
    float r;
    float g;
    float b;
} OSVR_RGB_FLOAT;

typedef enum {
    OSVR_OPEN_STATUS_FAILURE,
    OSVR_OPEN_STATUS_PARTIAL,
    OSVR_OPEN_STATUS_COMPLETE
} OSVR_OpenStatus;

// @todo OSVR_RenderTimingInfo

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrDestroyRenderManager(OSVR_RenderManager renderManager);

/// DEPRECATED - use the RenderInfoCollection API for your specific graphics API instead.
/// This function reads all of the rendering parameters from the
/// underlying RenderManager.  It caches this information locally
/// until the next call, and returns the number that it has cached.
/// Use the graphics-library-specific osvrRenderManagerGetRenderInfo
/// function (for example, osvrRenderManagerGetRenderInfoD3D11) to
/// read each entry that has been cached.
/// @brief Reads all of the RenderInfos and caches them.
/// @return Number of RenderInfos cached.
/// @todo Make this actually cache, for now it does not.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrRenderManagerGetNumRenderInfo(
    OSVR_RenderManager renderManager, OSVR_RenderParams renderParams,
    OSVR_RenderInfoCount* numRenderInfoOut);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetDoingOkay(OSVR_RenderManager renderManager);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetDefaultRenderParams(OSVR_RenderParams* renderParamsOut);

/// This function is used to bracket the start of the presentation of render
/// buffers for a single frame.  Between this function and the associated
/// Finish call below, graphics-library-specific Present calls should be made
/// (for example, osvrRenderManagerPresentRenderBufferD3D11). All buffers
/// must be registered before they are presented.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerStartPresentRenderBuffers(
    OSVR_RenderManagerPresentState* presentStateOut);

/// This function is used to bracket the end of the presentation of render
/// buffers for a single frame.  This sequence starts with the Start function.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerFinishPresentRenderBuffers(
    OSVR_RenderManager renderManager,
    OSVR_RenderManagerPresentState presentState, OSVR_RenderParams renderParams,
    OSVR_CBool shouldFlipY);

/// This function is used to bracket the start of the registration of render
/// buffers.  Between this function and the associated Finish call below,
/// graphics-library-specific Register calls should be made
/// (for example, osvrRenderManagerRegisterRenderBufferD3D11). All buffers
/// must be registered before they are presented.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerStartRegisterRenderBuffers(
    OSVR_RenderManagerRegisterBufferState* registerBufferStateOut);

/// This function is used to bracket the end of the registration of render
/// buffers for a single frame.  This sequence starts with the Start function.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerFinishRegisterRenderBuffers(
    OSVR_RenderManager renderManager,
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_CBool appWillNotOverwriteBeforeNewPresent);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerPresentSolidColorf(
    OSVR_RenderManager renderManager,
    OSVR_RGB_FLOAT rgb);

/// This function gets all of the RenderInfo collection in one atomic call.
/// Use osvrRenderManagerGetNumRenderInfoInCollection to get the size of the
/// collection, and API-specific methods to get a given render info for that
/// graphics API. Finally, use osvrRenderManagerReleaseRenderInfoCollection
/// when you're done.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetRenderInfoCollection(
    OSVR_RenderManager renderManager,
    OSVR_RenderParams renderParams,
    OSVR_RenderInfoCollection* renderInfoCollectionOut);

/// Releases the OSVR_RenderInfoCollection.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerReleaseRenderInfoCollection(
    OSVR_RenderInfoCollection renderInfoCollection);

/// Get the size of the OSVR_RenderInfoCollection.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetNumRenderInfoInCollection(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount* countOut);

OSVR_EXTERN_C_END

#endif
