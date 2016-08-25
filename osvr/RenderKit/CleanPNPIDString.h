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

#ifndef INCLUDED_CleanPNPIDString_h_GUID_31D738A4_19B2_49A4_C1A5_12AC5AF30874
#define INCLUDED_CleanPNPIDString_h_GUID_31D738A4_19B2_49A4_C1A5_12AC5AF30874

// Internal Includes
// - none

// Library/third-party includes
#include <boost/algorithm/string/case_conv.hpp>

// Standard includes
#include <string>

namespace osvr {
namespace renderkit {
    namespace vendorid {
        /// Given a character sequence that might be a PNPID, clean it up (all caps, check constraints), and either
        /// return it as a string, or return an empty string if it failed somewhere.
        template <typename T> inline std::string cleanPotentialPNPID(T const& input) {
            auto ret = std::string{};
            std::string pnpid{input};
            boost::algorithm::to_upper(pnpid);
            if (pnpid.size() != 3) {
                /// wrong length
                return ret;
            }

            for (auto c : pnpid) {
                if (c < 'A' || c > 'Z') {
                    /// character out of range
                    return ret;
                }
            }
            ret = std::move(pnpid);
            return ret;
        }
    } // namespace vendorid

} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_CleanPNPIDString_h_GUID_31D738A4_19B2_49A4_C1A5_12AC5AF30874
