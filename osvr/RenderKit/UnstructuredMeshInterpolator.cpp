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
#include "UnstructuredMeshInterpolator.h"

// Library/third-party includes
#include <quat.h>

// Standard includes
#include <map>
#include <cmath>
#include <array>

namespace osvr {
namespace renderkit {

    /// Used to determine if we have three 2D points that are almost
    /// in the same line.  If so, they are not good for use as a
    /// basis for interpolation.
    static bool nearly_collinear(std::array<double, 2> const& p1,
                                 std::array<double, 2> const& p2,
                                 std::array<double, 2> const& p3) {
        double dx1 = p2[0] - p1[0];
        double dy1 = p2[1] - p1[1];
        double dx2 = p3[0] - p1[0];
        double dy2 = p3[1] - p1[1];
        double len1 = sqrt(dx1 * dx1 + dy1 * dy1);
        double len2 = sqrt(dx2 * dx2 + dy2 * dy2);

        // If either vector is zero length, they are collinear
        if (len1 * len2 == 0) {
            return true;
        }

        // Normalize the vectors
        dx1 /= len1;
        dy1 /= len1;
        dx2 /= len2;
        dy2 /= len2;

        // See if the magnitude of their dot products is close to 1.
        double dot = dx1 * dx2 + dy1 * dy2;
        return fabs(dot) > 0.8;
    }

    static double pointDistance(double x1, double y1, double x2, double y2) {
        return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    }

    /// Interpolates the values at three 2D points to the
    /// location of a third point.
    static double interpolate(double p1X, double p1Y, double val1, double p2X,
                              double p2Y, double val2, double p3X, double p3Y,
                              double val3, double pointX, double pointY) {
        // Fit a plane to three points, using their values as the
        // third dimension.
        q_vec_type p1, p2, p3;
        q_vec_set(p1, p1X, p1Y, val1);
        q_vec_set(p2, p2X, p2Y, val2);
        q_vec_set(p3, p3X, p3Y, val3);

        // The normalized cross product of the vectors from the first
        // point to each of the other two is normal to this plane.
        q_vec_type v1, v2;
        q_vec_subtract(v1, p2, p1);
        q_vec_subtract(v2, p3, p1);
        q_vec_type ABC;
        q_vec_cross_product(ABC, v1, v2);
        if (q_vec_magnitude(ABC) == 0) {
            // We can't get a normal, degenerate points, just return the first
            // value.
            return val1;
        }
        q_vec_normalize(ABC, ABC);

        // Solve for the D associated with the plane by filling back
        // in one of the points.  This is done by taking the dot product
        // of the ABC vector with the first point.  We then solve for D.
        // AX + BY + CZ + D = 0; D = -(AX + BY + CZ)
        double D = -q_vec_dot_product(ABC, p1);

        // Evaluate the plane equations at our input point, which will interpolate
        // or extrapolate our values.
        // We're solving for Z in this case, so we get
        // CZ = -(AX + BY + D); Z = -(AX + BY + D)/C;
        return -(ABC[0] * pointX + ABC[1] * pointY + D) / ABC[2];
    }

    UnstructuredMeshInterpolator::UnstructuredMeshInterpolator(
        const MonoPointDistortionMeshDescription& points, int numSamplesX,
        int numSamplesY)
        : m_points(points), m_numSamplesX(numSamplesX),
          m_numSamplesY(numSamplesY) {
        // Construct and fill in the grid of nearby points that is used by the
        // interpolation function to accelerate the search for the three
        // nearest non-collinear points.
        std::vector<MonoPointDistortionMeshDescription> ySet;
        MonoPointDistortionMeshDescription empty;
        for (size_t y = 0; y < m_numSamplesY; y++) {
            ySet.emplace_back(empty);
        }
        for (size_t x = 0; x < m_numSamplesX; x++) {
            m_grid.emplace_back(ySet);
        }

        // Go through each point in the unstructured grid and insert its index
        // into all grid elements that are within 1/4th (rounded up) of the
        // total span of the grid from its normalized location.
        int xHalfSpan =
            static_cast<int>(0.9 + (1.0 / 4.0) * 0.5 * m_numSamplesX);
        int yHalfSpan =
            static_cast<int>(0.9 + (1.0 / 4.0) * 0.5 * m_numSamplesY);
        for (size_t i = 0; i < points.size(); i++) {
            int xIndex, yIndex;
            if (getIndex(points[i][0][0], points[i][0][1], xIndex, yIndex)) {
                // Get the range of locations to insert
                int xMin = xIndex - xHalfSpan;
                if (xMin < 0) {
                    xMin = 0;
                }
                int xMax = xIndex + xHalfSpan;
                if (xMax >= m_numSamplesX) {
                    xMax = m_numSamplesX - 1;
                }
                int yMin = yIndex - yHalfSpan;
                if (yMin < 0) {
                    yMin = 0;
                }
                int yMax = yIndex + yHalfSpan;
                if (yMax >= m_numSamplesY) {
                    yMax = m_numSamplesY - 1;
                }

                // Insert this point into each of these locations.
                for (int x = xMin; x <= xMax; x++) {
                    for (int y = yMin; y <= yMax; y++) {
                        m_grid[x][y].push_back(points[i]);
                    }
                }
            }
        }
    }

    Float2 UnstructuredMeshInterpolator::interpolateNearestPoints(float xN,
                                                                  float yN) {
        Float2 ret = {xN, yN};

        // Look in the spatial-acceleration grid to see if we can
        // find three points without having to search the entire set of
        // points.
        int xIndex, yIndex;
        if (!getIndex(xN, yN, xIndex, yIndex)) {
            return ret;
        }
        MonoPointDistortionMeshDescription points;
        points = getNearestPoints(xN, yN, m_grid[xIndex][yIndex]);

        // If we didn't get enough points from the acceleration
        // structure, look in the whole points array
        if (points.size() < 3) {
            points = getNearestPoints(xN, yN, m_points);
        }

        // If we didn't get three points, just return the output of
        // the first point we found.

        if (points.size() < 3) {
            float xNew = static_cast<float>(points[0][1][0]);
            float yNew = static_cast<float>(points[0][1][1]);
            ret[0] = xNew;
            ret[1] = yNew;
            return ret;
        }

        // Found three points -- interpolate them.
        float xNew = static_cast<float>(interpolate(
            points[0][0][0], points[0][0][1], points[0][1][0], points[1][0][0],
            points[1][0][1], points[1][1][0], points[2][0][0], points[2][0][1],
            points[2][1][0], xN, yN));
        float yNew = static_cast<float>(interpolate(
            points[0][0][0], points[0][0][1], points[0][1][1], points[1][0][0],
            points[1][0][1], points[1][1][1], points[2][0][0], points[2][0][1],
            points[2][1][1], xN, yN));
        ret[0] = xNew;
        ret[1] = yNew;
        return ret;
    }

    MonoPointDistortionMeshDescription
    UnstructuredMeshInterpolator::getNearestPoints(
        float xN, float yN, const MonoPointDistortionMeshDescription& points) {
        MonoPointDistortionMeshDescription ret;

        // Find the three non-collinear points in the mesh that are nearest
        // to the normalized point we are trying to look up.  We start by
        // sorting the points based on distance from our location, selecting
        // the first two, and then looking through the rest until we find
        // one that is not collinear with the first two (normalized dot
        // product magnitude far enough from 1).  If we don't find such
        // points, we just go with the values from the closest point.
        typedef std::multimap<double, size_t> PointDistanceIndexMap;
        PointDistanceIndexMap map;

        //if (points.size() < 460 && points.size() > 0)
        //    std::cout << "XXX Sorting mesh of size " << points.size() << std::endl;
        for (size_t i = 0; i < points.size(); i++) {
            // Insertion into the multimap sorts them by distance.
            map.insert(std::make_pair(
                pointDistance(xN, yN, points[i][0][0], points[i][0][1]), i));
        }

        PointDistanceIndexMap::const_iterator it = map.begin();
        size_t first = it->second;
        it++;
        size_t second = it->second;
        it++;
        size_t third = first;
        while (it != map.end()) {
            if (!nearly_collinear(points[first][0], points[second][0],
                                  points[it->second][0])) {
                third = it->second;
                break;
            }
            it++;
        }

        // Push back all of the points we found, which may not include
        // a third point if the first is the same as the third.
        if (map.size() >= 1) {
            ret.push_back(points[first]);
        }
        if (map.size() >= 2) {
            ret.push_back(points[second]);
        }
        if (third != first) {
            ret.push_back(points[third]);
        }

        return ret;
    }

    bool makeUnstructuredMeshInterpolators(
      const DistortionParameters &params,
      size_t eye,
      std::vector< std::unique_ptr<UnstructuredMeshInterpolator> >
      &interpolators)
    {
      // Clear the vector of interpolators we are going to write to,
      // deleting any objects pointed to, so we dont hand back existing
      // interpolators.
      interpolators.clear();

      // If we have a distortion type that involves one or more meshes,
      // construct the meshes and append them to the vector.
      if (params.m_type ==
        DistortionParameters::mono_point_samples) {
        if (params.m_monoPointSamples.size() != 2) {
          std::cerr << "makeInterpolatorsForParameters: Need 2 "
            "meshes, found "
            << params.m_monoPointSamples.size() << std::endl;
          return false;
        }
        // Add a new interpolator to be used when we're finding
        // mesh coordinates.
        if (params.m_monoPointSamples[eye].size() < 3) {
          std::cerr << "makeInterpolatorsForParameters: Need "
            "3+ points, found "
            << params.m_monoPointSamples[eye].size()
            << std::endl;
          return false;
        }
        for (size_t i = 0; i < params.m_monoPointSamples[eye].size();
          i++) {
          if (params.m_monoPointSamples[eye][i].size() != 2) {
            std::cerr
              << "makeInterpolatorsForParameters: Need 2 "
              << "points in the mesh, found "
              << params.m_monoPointSamples[eye][i].size()
              << std::endl;
            return false;
          }
          if (params.m_monoPointSamples[eye][i][0].size() != 2) {
            std::cerr
              << "makeInterpolatorsForParameters: Need 2 "
              << "values in input point, found "
              << params.m_monoPointSamples[eye][i][0].size()
              << std::endl;
            return false;
          }
          if (params.m_monoPointSamples[eye][i][1].size() != 2) {
            std::cerr
              << "makeInterpolatorsForParameters: Need 2 "
              << "values in output point, found "
              << params.m_monoPointSamples[eye][i][1].size()
              << std::endl;
            return false;
          }
        }
        // Add a new interpolator to be used when we're finding
        // mesh coordinates.
        interpolators.emplace_back(new
          UnstructuredMeshInterpolator(params.m_monoPointSamples[eye]));
      }
      else if (params.m_type ==
        DistortionParameters::rgb_point_samples) {
        if (params.m_rgbPointSamples.size() != 3) {
          std::cerr << "makeInterpolatorsForParameters: Need 3 "
            "color meshes, found "
            << params.m_rgbPointSamples.size() << std::endl;
          return false;
        }
        for (size_t clr = 0; clr < 3; clr++) {
          if (params.m_rgbPointSamples[clr].size() != 2) {
            std::cerr << "makeInterpolatorsForParameters: Need 2 "
              "eye meshes, found "
              << params.m_rgbPointSamples[clr].size()
              << std::endl;
            return false;
          }
          // Add a new interpolator to be used when we're finding
          // mesh coordinates.
          if (params.m_rgbPointSamples[clr][eye].size() < 3) {
            std::cerr
              << "makeInterpolatorsForParameters: Need "
              "3+ points, found "
              << params.m_rgbPointSamples[clr][eye].size()
              << std::endl;
            return false;
          }
          for (size_t i = 0;
            i < params.m_rgbPointSamples[clr][eye].size(); i++) {
            if (params.m_rgbPointSamples[clr][eye][i].size() !=
              2) {
              std::cerr
                << "makeInterpolatorsForParameters: Need "
                "2 points in the mesh, found "
                << params.m_rgbPointSamples[clr][eye][i].size()
                << std::endl;
              return false;
            }
            if (params.m_rgbPointSamples[clr][eye][i][0].size() !=
              2) {
              std::cerr
                << "makeInterpolatorsForParameters: Need "
                "2 values in input point, found "
                << params.m_rgbPointSamples[clr][eye][i][0]
                .size()
                << std::endl;
              return false;
            }
            if (params.m_rgbPointSamples[clr][eye][i][1].size() !=
              2) {
              std::cerr
                << "makeInterpolatorsForParameters: Need "
                "2 values in output point, found "
                << params.m_rgbPointSamples[clr][eye][i][1]
                .size()
                << std::endl;
              return false;
            }
          }

          // Add a new interpolator to be used when we're finding
          // mesh coordinates, one per eye.
          interpolators.emplace_back(new
            UnstructuredMeshInterpolator(params.m_rgbPointSamples[clr][eye]));
        }
      }

      return true;
    }


} // namespace renderkit
} // namespace osvr
