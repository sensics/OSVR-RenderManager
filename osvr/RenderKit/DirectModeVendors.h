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

#ifndef INCLUDED_DirectModeVendors_h_GUID_DFCDE7B9_78B9_44B1_85A5_2E661722FF93
#define INCLUDED_DirectModeVendors_h_GUID_DFCDE7B9_78B9_44B1_85A5_2E661722FF93

// Internal Includes
#include <osvr/RenderKit/VendorIdTools.h>

// Library/third-party includes
#include <osvr/Common/IntegerByteSwap.h>

// Standard includes
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <iterator>

namespace osvr {
namespace renderkit {
    namespace vendorid {
        using PNPIDNullTerminatedType = std::array<char, 4>;
        class DirectModeVendorEntry {
          public:
            explicit DirectModeVendorEntry(const PNPIDNullTerminatedType& pnpid)
                : pnpid_(pnpid), displayDescriptorVendor(pnpid.data()) {}
            DirectModeVendorEntry(const PNPIDNullTerminatedType& pnpid, const char* dispDescVend)
                : pnpid_(pnpid), displayDescriptorVendor(dispDescVend) {}
            DirectModeVendorEntry(const PNPIDNullTerminatedType& pnpid, const char* dispDescVend, const char* desc)
                : pnpid_(pnpid), displayDescriptorVendor(dispDescVend), description(desc) {}

            /// 3 character, all-caps, in A-Z, PNPID, preferably registered through the UEFI registry.
            PNPIDNullTerminatedType const& getPNPIDCharArray() const { return pnpid_; }

            const char* getPNPIDCString() const { return pnpid_.data(); }
            std::uint16_t getFlippedHexPNPID() const {
                /// @todo will this only work on little-endian systems? Need to figure out why the byte swap is needed.
                return common::integerByteSwap(pnpidToHex(getPNPIDCharArray()));
            }
            std::string const& getDisplayDescriptorVendor() const { return displayDescriptorVendor; }
            std::string const& getDescription() const {
                return description.empty() ? displayDescriptorVendor : description;
            }

          private:
            PNPIDNullTerminatedType pnpid_;
            /// Vendor string to match in display descriptor.
            std::string displayDescriptorVendor;
            /// Description - if left empty, getDescription will return displayDescriptorVendor instead.
            std::string description;
        };

        using DirectModeVendors = std::vector<DirectModeVendorEntry>;
        static inline std::vector<DirectModeVendors> combineSharedPNPIDs(DirectModeVendors const& vendors) {
            std::vector<DirectModeVendors> ret;
            for (auto& entry : vendors) {
                const auto e = ret.end();
                auto existingEntryIt = std::find_if(ret.begin(), e, [&](DirectModeVendors const& vendorList) {
                    return vendorList.front().getPNPIDCharArray() == entry.getPNPIDCharArray();
                });
                if (e == existingEntryIt) {
                    // no existing entry
                    ret.emplace_back(DirectModeVendors{entry});
                } else {
                    // add to existing entry.
                    existingEntryIt->push_back(entry);
                }
            }
            return ret;
        }
    } // namespace vendorid
    using vendorid::DirectModeVendors;
    static DirectModeVendors const& getDefaultVendors() {
        using Vendor = vendorid::DirectModeVendorEntry;
        static DirectModeVendors vendors =
            DirectModeVendors{Vendor{{"OVR"}, "Oculus"},
                              Vendor{{"SEN"}, "Sensics", "Some Sensics professional displays"},
                              Vendor{{"SVR"}, "Sensics", "Some Sensics professional displays"},
                              Vendor{{"SEN"}, "OSVR", "Some OSVR HDK units with early firmware/EDID data"},
                              Vendor{{"SVR"}, "OSVR", "Most OSVR HDK units"},
                              Vendor{{"AUO"}, "OSVR", "Some OSVR HDK2 firmware versions"},
                              Vendor{{"VVR"}},
                              Vendor{{"IWR"}, "Vuzix"},
                              Vendor{{"HVR"}, "HTC"},
                              Vendor{{"AVR"}},
                              Vendor{{"VRG"}, "VRGate"},
                              Vendor{{"TSB"}, "VRGate"},
                              Vendor{{"VRV"}, "Vrvana"}};
        return vendors;
    }

    static std::vector<DirectModeVendors> const& getDefaultVendorsByPNPID() {
        static std::vector<DirectModeVendors> vendorsByPNPID = vendorid::combineSharedPNPIDs(getDefaultVendors());
        return vendorsByPNPID;
    }
} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_DirectModeVendors_h_GUID_DFCDE7B9_78B9_44B1_85A5_2E661722FF93
