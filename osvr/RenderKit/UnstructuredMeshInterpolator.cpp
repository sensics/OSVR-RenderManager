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
#include <iostream>

namespace osvr {
namespace renderkit {

    /// Used to determine if we have three 2D points that are almost
    /// in the same line.  If so, they are not good for use as a
    /// basis for interpolation.
    static bool nearly_collinear(std::array<double, 2> const& p1,
                                 std::array<double, 2> const& p2,
                                 std::array<double, 2> const& p3,
                                 double threshold = 0.8) {
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
        return fabs(dot) > threshold;
    }

    /// Used to determine if two new points are almost in the
    /// same direction relative to a test point.
    /// If so, they are not good for use as a
    /// basis for interpolation.
    static bool same_direction(std::array<double, 2> const& test,
      std::array<double, 2> const& p2,
      std::array<double, 2> const& p3,
      double threshold = 0.8) {
      double dx1 = p2[0] - test[0];
      double dy1 = p2[1] - test[1];
      double dx2 = p3[0] - test[0];
      double dy2 = p3[1] - test[1];
      double len1 = sqrt(dx1 * dx1 + dy1 * dy1);
      double len2 = sqrt(dx2 * dx2 + dy2 * dy2);

      // If either vector is zero length, they not are in the same direction
      if (len1 * len2 == 0) {
        return false;
      }

      // Normalize the vectors
      dx1 /= len1;
      dy1 /= len1;
      dx2 /= len2;
      dy2 /= len2;

      // See if the magnitude of their dot products is close to 1.
      double dot = dx1 * dx2 + dy1 * dy2;
      return dot > threshold;
    }

    /// Determines whether points a and b are on the same side of the line
    /// from p1 to p2, where all are assumed to lie in the Z=0 plane.
    /// We check that the cross product of the vector from a to p1 with
    /// the vector from a to b is in the same direction as the cross
    /// product of the vector from a to p2 with the vector from a to b.
    /// If either lines on the line, then both are considered to lie on
    /// the same side of the line.
    static bool sameSide(q_vec_type a, q_vec_type b, q_vec_type p1, q_vec_type p2) {
      q_vec_type aToB, aToP1, aToP2;
      q_vec_subtract(aToB, b, a);
      q_vec_subtract(aToP1, p1, a);
      q_vec_subtract(aToP2, p2, a);

      q_vec_type cp1, cp2;
      q_vec_cross_product(cp1, aToB, aToP1);
      q_vec_cross_product(cp2, aToB, aToP2);

      // See if the cross product of both are the same sign, indicating
      // that they are on the same side of the line.  If the cross product
      // is zero, this indicates that one or both of them was on the line,
      // which also counts as on the same side.
      return q_vec_dot_product(cp1, cp2) >= 0;
    }

    /// Used to determine if the three 2D points surround the
    /// specified location, so that it will be an interpolation
    /// rather than an extrapolation to determine its value.
    static bool contains(double x, double y,
      std::array<double, 2> const& p1,
      std::array<double, 2> const& p2,
      std::array<double, 2> const& p3) {

      q_vec_type v1, v2, v3;
      q_vec_set(v1, p1[0], p1[1], 0);
      q_vec_set(v2, p2[0], p2[1], 0);
      q_vec_set(v3, p3[0], p3[1], 0);
      q_vec_type test;
      q_vec_set(test, x, y, 0);

      // The point should lie on the same side of the line between any
      // pair of points as the third point lies.  We test all three
      // pairs of points to ensure this.
      if ((sameSide(v1, v2, test, v3)) &&
          (sameSide(v1, v3, test, v2)) &&
          (sameSide(v2, v3, test, v1))) {
        return true;
      }
      return false;
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
            // We can't get a normal: degenerate points, just return the first
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
        for (int y = 0; y < m_numSamplesY; y++) {
            ySet.emplace_back(empty);
        }
        for (int x = 0; x < m_numSamplesX; x++) {
            m_grid.emplace_back(ySet);
        }

        // Go through each point in the unstructured grid and insert its index
        // into all grid elements that are within 1/4th (rounded up) of the
        // total span of the grid from its normalized location.
        int xHalfSpan =
            static_cast<int>(ceil(1.0 / 4.0) * 0.5 * m_numSamplesX);
        int yHalfSpan =
            static_cast<int>(ceil(1.0 / 4.0) * 0.5 * m_numSamplesY);
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

        // Find the nearest point.  We'll always use it.
        PointDistanceIndexMap::const_iterator it = map.begin();
        size_t first = it->second;
        it++;

        // Keep looking for a second one that is not in the same direction
        // from our test point as the first one.  Assuming we find one, it
        // will be our second point.
        std::array<double, 2> me = { xN, yN };
        size_t second;
        do {
          second = it->second;
          it++;
        } while (same_direction(me, points[first][0], points[second][0],0.95) &&
                 (it != map.end()));

        // For now, fill in the third with the first in case we
        // don't find a better one.
        size_t third = first;

        // Look for a set of three points that interpolate the coordinate
        // we are seeking.  That is, the point lies within the triangle
        // formed by the three points.
        // Keep track of where we should restart in case we don't find
        // what we're looking for.
        PointDistanceIndexMap::const_iterator restart = it;
        while (it != map.end()) {

          // If the three points are collinear, then we don't want to
          // use them.
          if (nearly_collinear(points[first][0], points[second][0],
                               points[it->second][0], 0.99)) {
            it++;
            continue;
          }

          // See if the point lies inside the triangle formed by the three points.
          // If so, use this point.
          if (contains(xN, yN, points[first][0], points[second][0],
                       points[it->second][0])) {
            third = it->second;
            break;
          }
          it++;
        }

        // If we did not yet find a third point as an interpolator, then
        // continue looking to see if we can find one as an extrapolator.
        // This will happen past the edge of the mesh.
        if (first == third) {
          // Reset to where we should start
          it = restart;
          while (it != map.end()) {
            if (!nearly_collinear(points[first][0], points[second][0],
              points[it->second][0], 0.8)) {
              third = it->second;
              break;
            }
            it++;
          }
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
