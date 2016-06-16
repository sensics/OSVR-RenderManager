/** @file
@brief Header file describing the OSVR graphics transformations interface

@date 2016

@author
Russ Taylor <russ@sensics.com>
<http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
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

#pragma once

// Internal Includes
#include <osvr/RenderKit/Export.h>
#include <osvr/RenderKit/RenderManagerC.h>

// Library/third-party includes
#include <osvr/Util/ClientReportTypesC.h>

// Standard includes
#include <memory>

extern "C" {

  //=========================================================================
  // Routines to turn the OSVR_PoseState into ModelView matrices for OpenGL
  // and
  // Direct3D.  Done in such a way that we don't require the inclusion of the
  // native API header files (since most apps will not include all of the
  // libraries).

  ///XXXX Update to match non-C version.
  /// Produce an OpenGL ModelView matrix from an OSVR_PoseState.
  /// Assumes that the world is described in a right-handed fashion and
  /// that we're going to use a right-handed projection matrix.
  /// @brief Produce OpenGL ModelView transform from OSVR_PoseState
  /// @param state_in Input state from RenderManager.
  /// @param OpenGL_out Pointer to 16-element double array that has
  ///        been allocated by the caller.
  /// @return True on success, false on failure (null pointer).
  bool OSVR_RENDERMANAGER_EXPORT OSVR_PoseState_to_OpenGL(
      double* OpenGL_out, const OSVR_PoseState& state_in);

  /// Produce a D3D ModelView matrix from an OSVR_PoseState.
  /// Handles transitioning from the right-handed OSVR coordinate
  /// system to the left-handed projection matrix that is typical
  /// for D3D applications.
  /// @brief Produce D3D ModelView transform from OSVR_PoseState
  /// @param state_in Input state from RenderManager.
  /// @param OpenGL_out Pointer to 16-element double array that has
  ///        been allocated by the caller.
  /// @return True on success, false on failure (null pointer).
  bool OSVR_RENDERMANAGER_EXPORT
  OSVR_PoseState_to_D3D(float D3D_out[16], const OSVR_PoseState& state_in);
  /// Modify the OSVR_PoseState from OSVR to be appropriate for use
  /// in a Unity application.  OSVR's world is right handed, and Unity's
  /// is left handed.
  /// @brief Modify OSVR_PoseState for use by Unity.
  /// @param state_in Input state from RenderManager.
  /// @param state_out Ouput state for use by Unity
  /// @return True on success, false on failure (null pointer).
  bool OSVR_RENDERMANAGER_EXPORT OSVR_PoseState_to_Unity(
    OSVR_PoseState& state_out, const OSVR_PoseState& state_in);

  //=========================================================================
  // Routines to turn the 4x4 projection matrices returned as part of the
  // RenderCallback class into Projection matrices for OpenGL and
  // Direct3D.  Done in such a way that we don't require the inclusion of the
  // native API header files (since most apps will not include all of the
  // libraries).

  /// @brief Produce OpenGL Projection matrix from 4x4 projection matrix
  bool OSVR_RENDERMANAGER_EXPORT OSVR_Projection_to_OpenGL(
      double* OpenGL_out, const OSVR_ProjectionMatrix& projection_in);
  /// @brief Produce Direct3D Projection matrix from 4x4 projection matrix
  bool OSVR_RENDERMANAGER_EXPORT OSVR_Projection_to_D3D(
      float D3D_out[16], const OSVR_ProjectionMatrix& projection_in);

}
