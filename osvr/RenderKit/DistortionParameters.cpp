/** @file
    @brief C++ body

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com>

*/

// Copyright 2016 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// 	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include "DistortionParameters.h"

// Library/third-party includes
#include "osvr_display_configuration.h"

namespace osvr {
  namespace renderkit {

    OSVR_RENDERMANAGER_EXPORT DistortionParameters::DistortionParameters(
      OSVRDisplayConfiguration& osvrParams,
      size_t eye) : DistortionParameters() {
      m_desiredTriangles = osvrParams.getDesiredDistortionTriangleCount(eye);
      if (osvrParams.getDistortionType(eye) ==
        OSVRDisplayConfiguration::RGB_SYMMETRIC_POLYNOMIALS) {
        m_type = rgb_symmetric_polynomials;
        std::vector<float> Ds;
        Ds.push_back(
          osvrParams.getDistortionDistanceScaleX(eye));
        Ds.push_back(
          osvrParams.getDistortionDistanceScaleY(eye));
        m_distortionD = Ds;
        m_distortionPolynomialRed =
          osvrParams.getDistortionPolynomalRed(eye);
        m_distortionPolynomialGreen =
          osvrParams.getDistortionPolynomalGreen(eye);
        m_distortionPolynomialBlue =
          osvrParams.getDistortionPolynomalBlue(eye);
        std::vector<float> COP = {
          static_cast<float>(
          osvrParams.getEyes()[eye].m_CenterProjX),
          static_cast<float>(
          osvrParams.getEyes()[eye].m_CenterProjY) };
        m_distortionCOP = COP;
      }
      else if (osvrParams.getDistortionType(eye) ==
        OSVRDisplayConfiguration::MONO_POINT_SAMPLES) {
        m_type = mono_point_samples;
        m_monoPointSamples =
          osvrParams.getDistortionMonoPointMeshes(eye);
      }
      else if (osvrParams.getDistortionType(eye) ==
        OSVRDisplayConfiguration::RGB_POINT_SAMPLES) {
        m_type = rgb_point_samples;
        m_rgbPointSamples =
          osvrParams.getDistortionRGBPointMeshes(eye);
      }
      else {
        std::cerr << "DistortionParameters::DistortionParameters(): "
          "Unrecognized distortion correction type ("
          << osvrParams.getDistortionTypeString(eye)
          << "), ignoring" << std::endl;
      }
    }

    OSVR_RENDERMANAGER_EXPORT DistortionParameters::DistortionParameters()
    {
      m_type = rgb_symmetric_polynomials;
      m_distortionCOP = { 0.5f /* X */, 0.5f /* Y */ };
      m_distortionD = { 1 /* DX */, 1 /* DY */ };
      m_distortionPolynomialRed = { 0, 1 };
      m_distortionPolynomialGreen = { 0, 1 };
      m_distortionPolynomialBlue = { 0, 1 };
      m_desiredTriangles = 2;
    };

} // namespace renderkit
} // namespace osvr

