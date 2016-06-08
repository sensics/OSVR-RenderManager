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
        double projection[4][4];
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                projection[i][j] = 0.0;
            }
        }

        // Convert from "left, right, bottom top, near, far" to the 4x4
        // transform.
        // See https://www.opengl.org/sdk/docs/man2/xhtml/glFrustum.xml
        // NOTE: There is actually a bug in the documentation.  If you
        // call glFrustum() and print out the results and compare them,
        // the value D from that page holds -1 and the value where there
        // is a -1 is what holds D.  This error is also copied to the
        // Microsoft page describing this function.  These are elements
        // [2][3] and [3][2], which are swapped.
        /*
        projection[0][0] = (2 * projection_in.nearClip) / (projection_in.right -
        projection_in.left);
        projection[0][2] = (projection_in.right + projection_in.left) /
        (projection_in.right - projection_in.left);
        projection[1][1] = (2 * projection_in.nearClip) / (projection_in.top -
        projection_in.bottom);
        projection[1][2] = (projection_in.top + projection_in.bottom) /
        (projection_in.top - projection_in.bottom);
        projection[2][2] = -(projection_in.farClip + projection_in.nearClip) /
        (projection_in.farClip - projection_in.nearClip);
        // Elements below swapped w.r.t. the web page so that they
        // match the actual behavior of glFrustum()
        projection[3][2] = -(2 * projection_in.farClip * projection_in.nearClip)
        / (projection_in.farClip - projection_in.nearClip);
        projection[2][3] = -1;
        */

        // We use the same projection matrix calculation here that we use below
        // in the D3D matrix.
        // @todo Seems like this should not be the case, since one is right
        // handed and the other left?
        projection[0][0] = (2 * projection_in.nearClip) /
                           (projection_in.right - projection_in.left);
        projection[1][1] = (2 * projection_in.nearClip) /
                           (projection_in.top - projection_in.bottom);
        projection[2][0] = (projection_in.right + projection_in.left) /
                           (projection_in.right - projection_in.left);
        projection[2][2] = projection_in.farClip /
                           (projection_in.nearClip - projection_in.farClip);
        projection[2][3] = -1;
        projection[3][2] = -(projection_in.farClip * projection_in.nearClip) /
                           (projection_in.farClip - projection_in.nearClip);

        // We store the projection matrix in the same order that OpenGL expects
        // it,
        // so we just need to copy the values.  There are 16 of them.
        /// @todo remove this step by writing to the matrix to start with
        memcpy(OpenGL_out, projection, 16 * sizeof(double));

        return true;
    }

    bool OSVR_Projection_to_D3D(float D3D_out[16], const OSVR_ProjectionMatrix& proj) {
        /// @todo Check that this order is correct.

        // Here, we need to make sure that we're putting elements into the
        // matrix in the order that C expects the elements when we index using
        // the [row][col] notation, with the matrix stored in memory in
        // row-major
        // order (all columns from the first row come first).
        float temp[16];
        for (size_t r = 0; r < 4; r++) {
            for (size_t c = 0; c < 4; c++) {
                temp[(r * 4) + c] = 0;
            }
        }

        // Construct the matrix using the formula desribed for the DirectX
        // off-axis
        // projection, for a right-handled matrix.
        temp[(0 * 4) + 0] =
            static_cast<float>(2 * proj.nearClip / (proj.right - proj.left));

        temp[(1 * 4) + 1] =
            static_cast<float>(2 * proj.nearClip / (proj.top - proj.bottom));

        temp[(2 * 4) + 0] = static_cast<float>((proj.left + proj.right) /
                                               (proj.right - proj.left));
        // This term was not in the online example for a left-handed matrix.
        // temp[(2 * 4) + 1] = static_cast<float>(
        //    (proj.top + proj.bottom) / (proj.top - proj.bottom)
        //     );
        temp[(2 * 4) + 2] =
            static_cast<float>(proj.farClip / (proj.nearClip - proj.farClip));
        // Making this 1 and negating the term above should produce a
        // left-handed matrix
        // https://msdn.microsoft.com/en-us/library/windows/desktop/bb205350%28v=vs.85%29.aspx
        temp[(2 * 4) + 3] = -1;

        temp[(3 * 4) + 2] = static_cast<float>((proj.nearClip * proj.farClip) /
                                               (proj.nearClip - proj.farClip));

        // Copy the matrix into the output matrix
        for (size_t r = 0; r < 4; r++) {
            for (size_t c = 0; c < 4; c++) {
                D3D_out[(r * 4) + c] = temp[(r * 4) + c];
            }
        }

        // Per Microsoft documentation: "In D3DX, the _34 element of a
        // projection matrix
        // cannot be a negative number. If your application needs to use a
        // negative value
        // in this location, it should scale the entire projection matrix by -1
        // instead."
        // We once got a working matrix if we multiply all by the last row of
        // the matrix by -1.
        // We later realized that we don't need to do this check at all.
        // Perhaps something
        // has changed in the API since then.
        /*
        if (D3D_out[11] < 0) {
            for (size_t r = 0; r < 3; r++) {
                for (size_t c = 0; c < 4; c++) {
                    D3D_out[(r * 4) + c] *= -1;
                }
            }
        }
        */

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
        q_xyz_quat_type info;
        info.xyz[0] = state_in.translation.data[0];
        info.xyz[1] = state_in.translation.data[1];
        info.xyz[2] = state_in.translation.data[2];
        info.quat[Q_X] = osvrQuatGetX(&state_in.rotation);
        info.quat[Q_Y] = osvrQuatGetY(&state_in.rotation);
        info.quat[Q_Z] = osvrQuatGetZ(&state_in.rotation);
        info.quat[Q_W] = osvrQuatGetW(&state_in.rotation);

        double rightHandedMatrix[16];
        q_xyz_quat_to_ogl_matrix(&rightHandedMatrix[0], &info);

        // Negate Z to switch the handedness of the matrix
        // so that we can use a right-handed projection matrix above.
        // @todo switch the above matrix to left handed and remove this.
        for (size_t i = 0; i < 4; i++) {
            rightHandedMatrix[2 * 4 + i] *= -1;
        }

        // Copy to the output matrix
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                D3D_out[i * 4 + j] =
                    static_cast<float>(rightHandedMatrix[i * 4 + j]);
            }
        }

        return true;
    }

} // namespace renderkit
} // namespace osvr
