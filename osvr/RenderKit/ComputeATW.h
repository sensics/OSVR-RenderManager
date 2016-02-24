/** @file
    @brief Header

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
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

#ifndef INCLUDED_ComputeATW_h_GUID_D936674E_AD5A_4AA8_86C0_C9E7D8D6F089
#define INCLUDED_ComputeATW_h_GUID_D936674E_AD5A_4AA8_86C0_C9E7D8D6F089

// Internal Includes
#include "RenderManager.h"

// Library/third-party includes
#include <osvr/Util/EigenInterop.h>
#include <osvr/Util/EigenCoreGeometry.h>

// Standard includes
// - none

namespace ei = osvr::util::eigen_interop;

namespace osvr {
namespace renderkit {
    inline Eigen::Projective3f computeATW(RenderInfo const& usedRenderInfo,
                                          RenderInfo const& currentRenderInfo,
                                          float assumedDepth) {

        /// @todo For CAVE displays and fish-tank VR, the projection matrix
        /// will not be the same between frames.  Make sure we're not
        /// assuming here that it is.

        // Compute the scale to use during forward transform.
        // Scale the coordinates in X and Y so that they match the width and
        // height of a window at the specified distance from the origin.
        // We divide by the near clip distance to make the result match that
        // at a unit distance and then multiply by the assumed depth.
        float xScale = static_cast<float>(
            (usedRenderInfo.projection.right - usedRenderInfo.projection.left) /
            usedRenderInfo.projection.nearClip * assumedDepth);
        float yScale = static_cast<float>(
            (usedRenderInfo.projection.top - usedRenderInfo.projection.bottom) /
            usedRenderInfo.projection.nearClip * assumedDepth);

        // Compute the translation to use during forward transform.
        // Translate the points so that their center lies in the middle of
        // the view frustum pushed out to the specified distance from the
        // origin.
        // We take the mean coordinate of the two edges as the center that
        // is to be moved to, and we move the space origin to there.
        // We divide by the near clip distance to make the result match that
        // at a unit distance and then multiply by the assumed depth.
        // This assumes the default r texture coordinate of 0.
        float xTrans = static_cast<float>(
            (usedRenderInfo.projection.right + usedRenderInfo.projection.left) /
            2.0 / usedRenderInfo.projection.nearClip * assumedDepth);
        float yTrans = static_cast<float>(
            (usedRenderInfo.projection.top + usedRenderInfo.projection.bottom) /
            2.0 / usedRenderInfo.projection.nearClip * assumedDepth);
        float zTrans = static_cast<float>(-assumedDepth);

        // NOTE: These operations occur from the right to the left, so later
        // actions on the list actually occur first because we're
        // post-multiplying.

        // Translate the points back to a coordinate system with the
        // center at (0,0);
        Eigen::Isometry3f postTranslation(
            Eigen::Translation3f(0.5f, 0.5f, 0.0f));

        // Scale the points so that they will fit into the range
        // (-0.5,-0.5)
        // to (0.5,0.5) (the inverse of the scale below).
        Eigen::Isometry3f postScale(
            Eigen::Scaling(1.0f / xScale, 1.0f / yScale, 1.0f));

        // Translate the points so that the projection center will lie on
        // the -Z axis (inverse of the translation below).
        Eigen::Isometry3f postProjectionTranslate(
            Eigen::Translation3f(-xTrans, -yTrans, -zTrans));

        // Compute the forward last ModelView matrix.
        OSVR_PoseState lastModelOSVR = usedRenderInfo.pose;
        Eigen::Isometry3f lastModelViewTransform =
            ei::map(lastModelOSVR).transform().cast<float>();

        /// Compute the inverse of the current ModelView matrix.
        OSVR_PoseState currentModelOSVR = currentRenderInfo.pose;

        Eigen::Isometry3f currentModelViewInverseTransform =
            ei::map(currentModelOSVR).transform().cast<float>().inverse();

        /// Translate the origin to the center of the projected rectangle
        Eigen::Isometry3f preProjectionTranslate(
            Eigen::Translation3f(xTrans, yTrans, zTrans));

        /// Scale from (-0.5,-0.5)/(0.5,0.5) to the actual frustum size
        Eigen::Isometry3f preScale(Eigen::Scaling(xScale, yScale, 1.0f));

        // Translate the points from a coordinate system that has (0.5,0.5)
        // as the origin to one that has (0,0) as the origin.
        Eigen::Isometry3f preTranslation(
            Eigen::Translation3f(-0.5f, -0.5f, 0.0f));

        /// Compute the full matrix by multiplying the parts.
        /// @todo Does this have to be projective? it looks like combining
        /// isometries...
        Eigen::Projective3f full =
            postTranslation * postScale * postProjectionTranslate *
            lastModelViewTransform * currentModelViewInverseTransform *
            preProjectionTranslate * preScale * preTranslation;
        return full;
    }

} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_ComputeATW_h_GUID_D936674E_AD5A_4AA8_86C0_C9E7D8D6F089
