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

//=========================================================================
// Routines to turn the OSVR_PoseState into ModelView matrices for OpenGL
// and
// Direct3D.  Done in such a way that we don't require the inclusion of the
// native API header files (since most apps will not include all of the
// libraries).

/// @brief Produce OpenGL ModelView transform from OSVR_PoseState
bool OSVR_RENDERMANAGER_EXPORT OSVR_PoseState_to_OpenGL(
    double* OpenGL_out, const OSVR_PoseState& state_in);
/// @brief Produce D3D ModelView transform from OSVR_PoseState
bool OSVR_RENDERMANAGER_EXPORT
OSVR_PoseState_to_D3D(float D3D_out[16], const OSVR_PoseState& state_in);

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
