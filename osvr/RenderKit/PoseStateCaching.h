/** @file
    @brief Header

    @date 2017

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2017 Sensics, Inc.
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

#ifndef INCLUDED_PoseStateCaching_h_GUID_0424A36E_4123_45A3_6DB8_3A6E4B90665B
#define INCLUDED_PoseStateCaching_h_GUID_0424A36E_4123_45A3_6DB8_3A6E4B90665B

// Internal Includes
// - none

// Library/third-party includes
#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/Util/TimeValue.h>
#include <osvr/Util/Pose3C.h>

// Standard includes
// - none

namespace osvr {
namespace renderkit {
    /// A class that handles stashing poses with optionally overridden timestamps.
    ///
    /// The associated client context must outlive it.
    class PoseStateCaching {
      public:
        /// Constructor, taking a client context, semantic path, and a value indicating whether (true) or not (false) to
        /// override timestamps coming with the reports by the timestamp at the time the callback is invoked.
        PoseStateCaching(OSVR_ClientContext clientContext, const char path[], bool overrideTime)
            : ctx_(clientContext), overrideTime_(overrideTime) {
            osvrClientGetInterface(ctx_, path, &iface_);
            if (!iface_) {
                return;
            }
            osvrRegisterPoseCallback(iface_, &handleReport, this);
        }

        /// Destructor: cleans up the client interface object.
        ~PoseStateCaching() {
            if (ctx_ && iface_) {
                osvrClientFreeInterface(ctx_, iface_);
                ctx_ = nullptr;
                iface_ = nullptr;
            }
        }

        /// Non-copyable.
        PoseStateCaching(PoseStateCaching const&) = delete;
        /// Non-assignable.
        PoseStateCaching& operator=(PoseStateCaching const&) = delete;

        /// Indicates whether this object has observed a pose report yet.
        bool hasReport() const { return hasPose_; }

        /// Retrieve the last report, if one exists.
        /// If none exists, out params are unchanged.
        ///
        /// @param [out] tv Timestamp associated with pose.
        /// @param [out] pose The pose itself.
        ///
        /// @return true if there was a report observed and returned in the out params.
        bool getLastReport(util::time::TimeValue& tv, OSVR_Pose3& pose) const {
            if (!hasPose_) {
                return false;
            }

            tv = lastTimestamp_;
            pose = pose_;
            return true;
        }

      private:
        static void handleReport(void* userdata, const struct OSVR_TimeValue* timestamp,
                                 const struct OSVR_PoseReport* report) {
            auto self = static_cast<PoseStateCaching*>(userdata);
            self->hasPose_ = true;
            self->pose_ = report->pose;
            if (self->overrideTime_) {
                osvrTimeValueGetNow(&self->lastTimestamp_);
            } else {
                self->lastTimestamp_ = *timestamp;
            }
        }
        OSVR_ClientContext ctx_;
        bool overrideTime_;
        OSVR_ClientInterface iface_ = nullptr;
        bool hasPose_ = false;
        util::time::TimeValue lastTimestamp_;
        OSVR_Pose3 pose_;
    };

} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_PoseStateCaching_h_GUID_0424A36E_4123_45A3_6DB8_3A6E4B90665B
