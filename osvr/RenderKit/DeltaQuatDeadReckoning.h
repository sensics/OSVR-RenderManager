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

#ifndef INCLUDED_DeltaQuatDeadReckoning_h_GUID_6BFD4A2F_F612_4EB6_7A69_05EC20C6D9AF
#define INCLUDED_DeltaQuatDeadReckoning_h_GUID_6BFD4A2F_F612_4EB6_7A69_05EC20C6D9AF

// Internal Includes
// - none

// Library/third-party includes
#include <osvr/Util/EigenCoreGeometry.h>

// Standard includes
// - none

namespace osvr {
namespace util {
    inline Eigen::Quaterniond applyQuatDeadReckoning(Eigen::Quaterniond const& initialOrientation, double angVelDt,
                                                     Eigen::Quaterniond const& velocityDeltaQuat,
                                                     double predictionDistance) {
        Eigen::Quaterniond ret = initialOrientation;
        // Determine the number of integer multiples of our deltaquat needed.
        int multiples = static_cast<int>(predictionDistance / angVelDt);

        // Determine the fractional (slerp) portion to apply after that.
        auto predictionRemainder = predictionDistance - (multiples * angVelDt);
        auto remainderAsFractionOfDt = predictionRemainder / angVelDt;

        Eigen::Quaterniond fractionalDeltaQuat =
            Eigen::Quaterniond::Identity().slerp(remainderAsFractionOfDt, velocityDeltaQuat);

        // Actually perform the application of the prediction.
        for (int i = 0; i < multiples; ++i) {
            ret = velocityDeltaQuat * ret;
        }
        ret = fractionalDeltaQuat * ret;
        return ret;
    }
} // namespace util
} // namespace osvr

#endif // INCLUDED_DeltaQuatDeadReckoning_h_GUID_6BFD4A2F_F612_4EB6_7A69_05EC20C6D9AF
