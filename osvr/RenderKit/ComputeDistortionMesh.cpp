/** @file
    @brief Implementation

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
#include "ComputeDistortionMesh.h"
#include "UnstructuredMeshInterpolator.h"
#include "DistortionCorrectTextureCoordinate.h"

// Library/third-party includes
// - none

// Standard includes
#include <iostream>
#include <cmath>

namespace osvr {
namespace renderkit {

    DistortionMesh ComputeDistortionMesh(size_t eye, DistortionMeshType type, DistortionParameters distort, float overfillFactor) {
        DistortionMesh ret;

        std::vector<UnstructuredMeshInterpolator*> interpolators;

        // Check the validity of the parameters, based on the ones we're
        // using.
        if (distort.m_type ==
            DistortionParameters::rgb_symmetric_polynomials) {
            if (distort.m_distortionPolynomialRed.size() < 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2+ "
                    "red polynomial coefficients, found "
                    << distort.m_distortionPolynomialRed.size()
                    << std::endl;
                return ret;
            }
            if (distort.m_distortionPolynomialGreen.size() < 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2+ "
                    "green polynomial coefficients, found "
                    << distort.m_distortionPolynomialGreen.size()
                    << std::endl;
                return ret;
            }
            if (distort.m_distortionPolynomialBlue.size() < 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2+ "
                    "blue polynomial coefficients, found "
                    << distort.m_distortionPolynomialBlue.size()
                    << std::endl;
                return ret;
            }
            if (distort.m_distortionD.size() != 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2 "
                    "distortion coefficients, found "
                    << distort.m_distortionD.size() << std::endl;
                return ret;
            }
        } else if (distort.m_type ==
                   DistortionParameters::mono_point_samples) {
            if (distort.m_monoPointSamples.size() != 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2 "
                    "meshes, found "
                    << distort.m_monoPointSamples.size() << std::endl;
                return ret;
            }
            // Add a new interpolator to be used when we're finding
            // mesh coordinates.
            if (distort.m_monoPointSamples[eye].size() < 3) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need "
                    "3+ points, found "
                    << distort.m_monoPointSamples[eye].size()
                    << std::endl;
                return ret;
            }
            for (size_t i = 0; i < distort.m_monoPointSamples[eye].size();
                 i++) {
                if (distort.m_monoPointSamples[eye][i].size() != 2) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need 2 "
                        << "points in the mesh, found "
                        << distort.m_monoPointSamples[eye][i].size()
                        << std::endl;
                    return ret;
                }
                if (distort.m_monoPointSamples[eye][i][0].size() != 2) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need 2 "
                        << "values in input point, found "
                        << distort.m_monoPointSamples[eye][i][0].size()
                        << std::endl;
                    return ret;
                }
                if (distort.m_monoPointSamples[eye][i][1].size() != 2) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need 2 "
                        << "values in output point, found "
                        << distort.m_monoPointSamples[eye][i][1].size()
                        << std::endl;
                    return ret;
                }
            }
            // Add a new interpolator to be used when we're finding
            // mesh coordinates.
            interpolators.emplace_back(new
                                         UnstructuredMeshInterpolator(distort.m_monoPointSamples[eye]));
        }
        else if (distort.m_type ==
                 DistortionParameters::rgb_point_samples) {
            if (distort.m_rgbPointSamples.size() != 3) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 3 "
                    "color meshes, found "
                    << distort.m_rgbPointSamples.size() << std::endl;
                return ret;
            }
            for (size_t clr = 0; clr < 3; clr++) {
                if (distort.m_rgbPointSamples[clr].size() != 2) {
                    std::cerr << "RenderManager::ComputeDistortionMesh: Need 2 "
                        "eye meshes, found "
                        << distort.m_rgbPointSamples[clr].size()
                        << std::endl;
                    return ret;
                }
                // Add a new interpolator to be used when we're finding
                // mesh coordinates.
                if (distort.m_rgbPointSamples[clr][eye].size() < 3) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need "
                        "3+ points, found "
                        << distort.m_rgbPointSamples[clr][eye].size()
                        << std::endl;
                    return ret;
                }
                for (size_t i = 0;
                     i < distort.m_rgbPointSamples[clr][eye].size(); i++) {
                    if (distort.m_rgbPointSamples[clr][eye][i].size() !=
                        2) {
                        std::cerr
                            << "RenderManager::ComputeDistortionMesh: Need "
                            "2 "
                            << "points in the mesh, found "
                            << distort.m_rgbPointSamples[clr][eye][i].size()
                            << std::endl;
                        return ret;
                    }
                    if (distort.m_rgbPointSamples[clr][eye][i][0].size() !=
                        2) {
                        std::cerr
                            << "RenderManager::ComputeDistortionMesh: Need "
                            "2 "
                            << "values in input point, found "
                            << distort.m_rgbPointSamples[clr][eye][i][0]
                            .size()
                            << std::endl;
                        return ret;
                    }
                    if (distort.m_rgbPointSamples[clr][eye][i][1].size() !=
                        2) {
                        std::cerr
                            << "RenderManager::ComputeDistortionMesh: Need "
                            "2 "
                            << "values in output point, found "
                            << distort.m_rgbPointSamples[clr][eye][i][1]
                            .size()
                            << std::endl;
                        return ret;
                    }
                }

                // Add a new interpolator to be used when we're finding
                // mesh coordinates, one per eye.
                interpolators.emplace_back(new
                                             UnstructuredMeshInterpolator(distort.m_rgbPointSamples[clr][eye]));
            }
        } else {
            std::cerr << "RenderManager::ComputeDistortionMesh: Unrecognized "
                << "distortion parameter type" << std::endl;
            return ret;
        }

        // See what kind of mesh we're supposed to produce.  Make the
        // appropriate one.
        switch (type) {
        case SQUARE: {
                         // Figure out how many quads we should use in each dimension.  The
                         // minimum is 1.  We have an even number in each.  There are two
                         // triangles per quad.
                         int quadsPerSide =
                             static_cast<int>(std::sqrt(distort.m_desiredTriangles / 2));
                         if (quadsPerSide < 1) {
                             quadsPerSide = 1;
                         }

                         // Figure out how large each quad will be.  Recall that we're
                         // covering a range of 2 (from -1 to 1) in each dimension, so the
                         // quads will all be square in texture space.
                         float quadSide = 2.0f / quadsPerSide;
                         float quadTexSide = 1.0f / quadsPerSide;

                         // Compute distorted texture coordinates and use those for each
                         // vertex, with appropriate spatial location and texture
                         // coordinates.

                         auto const numVertsPerSide = quadsPerSide + 1;
                         auto const numVertices = numVertsPerSide*numVertsPerSide;
                         ret.vertices.reserve(numVertices);

                         // Generate a grid of vertices with distorted texture coordinates
                         for (int x = 0; x < numVertsPerSide; x++) {
                             float xPos = -1 + x * quadSide;
                             float xTex = x * quadTexSide;

                             for (int y = 0; y < numVertsPerSide; y++) {
                                 float yPos = -1 + y * quadSide;
                                 float yTex = y * quadTexSide;

                                 Float2 pos = { xPos, yPos };
                                 Float2 tex = { xTex, yTex };

                                 ret.vertices.emplace_back(pos,
                                                           DistortionCorrectTextureCoordinate(eye, tex, distort, 0, overfillFactor, interpolators),
                                                           DistortionCorrectTextureCoordinate(eye, tex, distort, 1, overfillFactor, interpolators),
                                                           DistortionCorrectTextureCoordinate(eye, tex, distort, 2, overfillFactor, interpolators));
                             }
                         }

                         // Generate a pair of triangles for each quad, wound
                         // counter-clockwise from the mesh grid

                         // total of quadsPerSide * quadsPerSide * 6 vertices added: reserve
                         // that space to avoid excess copying during mesh generation.
                         ret.indices.reserve(quadsPerSide * quadsPerSide * 6);
                         for (int x = 0; x < quadsPerSide; x++) {
                             for (int y = 0; y < quadsPerSide; y++) {
                                 // Grid generated above is in column-major order
                                 int indexLL = x*numVertsPerSide + y;
                                 int indexHL = indexLL + numVertsPerSide;
                                 int indexHH = indexLL + numVertsPerSide + 1;
                                 int indexLH = indexLL + 1;

                                 // Triangle 1
                                 ret.indices.emplace_back(indexLL);
                                 ret.indices.emplace_back(indexHL);
                                 ret.indices.emplace_back(indexHH);

                                 // Triangle 2
                                 ret.indices.emplace_back(indexLL);
                                 ret.indices.emplace_back(indexHH);
                                 ret.indices.emplace_back(indexLH);
                             }
                         }
                     } break;
        case RADIAL: {
                         std::cerr
                             << "RenderManager::ComputeDistortionMesh: Radial mesh type "
                             << "not yet implemented" << std::endl;

                         // @todo Scale the aspect ratio of the rings around the center of
                         // projection so that they will be round in the visible display.
                     } break;
        default:
                     std::cerr << "RenderManager::ComputeDistortionMesh: Unsupported "
                         "mesh type: "
                         << type << std::endl;
        }

        // Clear any created interpolators, freeing up their memory
        // first.
        for (size_t clr = 0; clr < interpolators.size(); clr++) {
            delete interpolators[clr];
        }
        interpolators.clear();

        return ret;
    }

} // end namespace renderkit
} // end namespace osvr

