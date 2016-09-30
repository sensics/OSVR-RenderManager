/** @file
@brief Implementation of the OSVR direct-to-device rendering interface

@date 2015

@author
Russ Taylor <russ@sensics.com>
<http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include "RenderKitGraphicsTransforms.h"

// Library/third-party includes
#include <quat.h>

// Standard includes
#include <iostream>
#include <string.h>

namespace osvr {
namespace renderkit {

  bool OSVR_PoseState_to_OpenGL(double* OpenGL_out,
    const OSVR_PoseState& state_in) {
    if (OpenGL_out == nullptr) {
      std::cerr << "OSVR_PoseState_to_OpenGL called with NULL pointer"
        << std::endl;
      return false;
    }

    // Move the Pose information into Quatlib data structures so
    // we can operate on it using its functions.
    q_xyz_quat_type info;
    info.xyz[0] = state_in.translation.data[0];
    info.xyz[1] = state_in.translation.data[1];
    info.xyz[2] = state_in.translation.data[2];
    info.quat[Q_X] = osvrQuatGetX(&state_in.rotation);
    info.quat[Q_Y] = osvrQuatGetY(&state_in.rotation);
    info.quat[Q_Z] = osvrQuatGetZ(&state_in.rotation);
    info.quat[Q_W] = osvrQuatGetW(&state_in.rotation);

    q_xyz_quat_to_ogl_matrix(OpenGL_out, &info);
    return true;
  }

  bool OSVR_Projection_to_OpenGL(double* OpenGL_out,
    const OSVR_ProjectionMatrix& projection_in) {
    if (OpenGL_out == nullptr) {
      std::cerr << "OSVR_Projection_to_OpenGL called with NULL pointer"
        << std::endl;
      return false;
    }

    // Zero the entire projection matrix to start with.
    double projection[4][4] = {};

    // Convert from "left, right, bottom top, near, far" to the 4x4
    // transform.
    // The code here follows the presentation in Robinett and
    // Holloway "The Visual Display Transformation for Virtual
    // Reality", http://www.cs.unc.edu/techreports/94-031.pdf
    // It maps X, Y, and (-Z) from -1 to 1 as they range across
    // the bounds of the view frustum (left to right, bottom to
    // top, and near to far).
    projection[0][0] = (2 * projection_in.nearClip) /
      (projection_in.right - projection_in.left);
    projection[1][1] = (2 * projection_in.nearClip) /
      (projection_in.top - projection_in.bottom);
    projection[2][0] = (projection_in.right + projection_in.left) /
      (projection_in.right - projection_in.left);
    projection[2][1] = (projection_in.top + projection_in.bottom) /
      (projection_in.top - projection_in.bottom);
    projection[2][2] = -(projection_in.farClip + projection_in.nearClip) /
      (projection_in.farClip - projection_in.nearClip);
    projection[2][3] = -1;
    projection[3][2] =
      -(2 * projection_in.farClip * projection_in.nearClip) /
      (projection_in.farClip - projection_in.nearClip);

    // We store the projection matrix in the same order that OpenGL expects
    // it, so we just need to copy the values.  There are 16 of them.
    memcpy(OpenGL_out, projection, 16 * sizeof(OpenGL_out[0]));

    return true;
  }

  bool OSVR_Projection_to_D3D(float D3D_out[16], const OSVR_ProjectionMatrix& proj) {

    // Zero all of the elements we're not otherwise filling in.
    memset(D3D_out, 0, 16 * sizeof(D3D_out[0]));

    // Construct the matrix using the formula described for the DirectX
    // off-axis projection, for a left-handled matrix.  This maps X and
    // Y from -1 to 1 as they go from left to right and bottom to top,
    // and it maps Z from 0 to 1 as it goes from near to far.
    D3D_out[(0 * 4) + 0] =
      static_cast<float>(2 * proj.nearClip / (proj.right - proj.left));

    D3D_out[(1 * 4) + 1] =
      static_cast<float>(2 * proj.nearClip / (proj.top - proj.bottom));

    D3D_out[(2 * 4) + 0] = -static_cast<float>((proj.left + proj.right) /
      (proj.right - proj.left));
    D3D_out[(2 * 4) + 1] = -static_cast<float>((proj.top + proj.bottom) /
      (proj.top - proj.bottom));
    D3D_out[(2 * 4) + 2] =
      -static_cast<float>(proj.farClip / (proj.nearClip - proj.farClip));
    D3D_out[(2 * 4) + 3] = 1;

    D3D_out[(3 * 4) + 2] = static_cast<float>((proj.nearClip * proj.farClip) /
      (proj.nearClip - proj.farClip));
    return true;
  }

  bool OSVR_PoseState_to_D3D(float D3D_out[16],
    const OSVR_PoseState& state_in) {
    if (nullptr == D3D_out) {
      std::cerr << "OSVR_PoseState_to_D3D called with NULL pointer"
        << std::endl;
      return false;
    }

    // Move the Pose information into Quatlib data structures so
    // we can operate on it using its functions.
    // To convert from left-handed to right-handed, we negate
    // the Z position value and the X and Y orientation values.
    // This will let us use a right-handed projection matrix.
    q_xyz_quat_type info;
    info.xyz[0] = state_in.translation.data[0];
    info.xyz[1] = state_in.translation.data[1];
    info.xyz[2] = -state_in.translation.data[2];
    info.quat[Q_X] = -osvrQuatGetX(&state_in.rotation);
    info.quat[Q_Y] = -osvrQuatGetY(&state_in.rotation);
    info.quat[Q_Z] = osvrQuatGetZ(&state_in.rotation);
    info.quat[Q_W] = osvrQuatGetW(&state_in.rotation);

    double rightHandedMatrix[16];
    q_xyz_quat_to_ogl_matrix(&rightHandedMatrix[0], &info);

    // Copy to the output matrix
    for (size_t i = 0; i < 4; i++) {
      for (size_t j = 0; j < 4; j++) {
        D3D_out[i * 4 + j] =
          static_cast<float>(rightHandedMatrix[i * 4 + j]);
      }
    }

    return true;
  }

  bool OSVR_PoseState_to_Unity(OSVR_PoseState& state_out,
    const OSVR_PoseState& state_in) {

    // To convert from left-handed to right-handed, we negate
    // the Z position value and the X and Y orientation values.
    // This will let us use a right-handed projection matrix.
    state_out = state_in;
    osvrVec3SetZ(&state_out.translation,
      osvrVec3GetZ(&state_out.translation) * -1);
    osvrQuatSetX(&state_out.rotation,
      osvrQuatGetX(&state_out.rotation) * -1);
    osvrQuatSetY(&state_out.rotation,
      osvrQuatGetY(&state_out.rotation) * -1);

    return true;
  }

  bool OSVR_Projection_to_Unreal(float Unreal_out[16],
    const OSVR_ProjectionMatrix& proj) {

    // Zero all of the elements we're not otherwise filling in.
    memset(Unreal_out, 0, 16 * sizeof(Unreal_out[0]));

    // The X and Y terms for the matrix, and the divide-by-Z term
    // all follow the same approach as the D3D matrix used above.
    // This is a standard left-handed off-axis projection matrix.
    Unreal_out[(0 * 4) + 0] =
      static_cast<float>(2 * proj.nearClip / (proj.right - proj.left));

    Unreal_out[(1 * 4) + 1] =
      static_cast<float>(2 * proj.nearClip / (proj.top - proj.bottom));

    Unreal_out[(2 * 4) + 0] = -static_cast<float>((proj.left + proj.right) /
      (proj.right - proj.left));
    Unreal_out[(2 * 4) + 1] = -static_cast<float>((proj.top + proj.bottom) /
      (proj.top - proj.bottom));

    Unreal_out[(2 * 4) + 3] = 1;

    // The Z terms differ from a standard left-handed projection matrix
    // because (1) they need to range from Z=0 at the far clipping plane
    // to Z=1 at the near clipping plane (the opposite of normal) and
    // (2) There may not actually be a far clipping plane set, indicated
    // by near and far being the same (in which case, we set Z=0 always).
    if (proj.nearClip == proj.farClip) {
      Unreal_out[(2 * 4) + 2] = 0.0f;   // Z value is 0
      Unreal_out[(3 * 4) + 2] = 1.0f;   // Z homogenous coord = 1
    }
    else {
      Unreal_out[(2 * 4) + 2] =
        static_cast<float>(proj.nearClip / (proj.nearClip - proj.farClip));
      Unreal_out[(3 * 4) + 2] =
        -static_cast<float>((proj.nearClip * proj.farClip) /
        (proj.nearClip - proj.farClip));
    }

    return true;
  }

} // namespace renderkit
} // namespace osvr
