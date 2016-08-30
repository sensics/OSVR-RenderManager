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

#ifndef INCLUDED_DirectModeToolSkeleton_h_GUID_03A2F8B3_098E_448B_1B25_19A4EAA5DFA7
#define INCLUDED_DirectModeToolSkeleton_h_GUID_03A2F8B3_098E_448B_1B25_19A4EAA5DFA7

// Internal Includes
#include "osvr/RenderKit/VendorIdTools.h" // pnpidToHex
#include "osvr/RenderKit/CleanPNPIDString.h"
#include "osvr/RenderKit/DirectModeVendors.h"

// Library/third-party includes
#include <iostream>

// Standard includes
#include <stdint.h>

bool VendorAction(std::uint16_t flippedHexPNPID);

static int Usage(const char* name) {
    std::cerr << "Usage: " << name << " [PNPID]" << std::endl;
    std::cerr << "If given, the custom PNPID must be exactly 3 letters A-Z long." << std::endl;
    return -1;
}

int main(int argc, char* argv[]) {
    if (argc > 2) {
        return Usage(argv[0]);
    }

    using namespace osvr::renderkit;
    using namespace osvr;

    std::string customPNPID;
    if (argc > 1) {
        customPNPID = vendorid::cleanPotentialPNPID(argv[1]);
        if (customPNPID.empty()) {

            std::cerr << "custom pnpid wrong size or character in custom pnpid "
                         "not in [A-Z]\n"
                      << std::endl;
            return Usage(argv[0]);
        }
    }

    bool gotOne = false;

    if (!customPNPID.empty()) {
        auto hexPNPID = pnpidToFlippedHex(customPNPID);
        std::cout << "Trying custom PNPID from command line '" << customPNPID << "' [hex "
                  << vendorid::formatAsHexString(hexPNPID) << "]" << std::endl;
        if (VendorAction(hexPNPID)) {
            std::cout << "  Success!" << std::endl;
            gotOne = true;
        }
        std::cout << std::endl;
    }

    // Try all of the vendor IDs we're familiar with.
    auto& pnpidsWithDescriptions = getDefaultPNPIDsWithDescriptions();
    for (auto& pnpidGroup : pnpidsWithDescriptions) {
        auto hexPNPID = pnpidGroup.getFlippedHexPNPID();
        std::cout << "Trying PNPID '" << pnpidGroup.getPNPIDCString() << "' [hex "
                  << vendorid::formatAsHexString(hexPNPID) << "] : " << pnpidGroup.getDescriptionsJoined(", ")
                  << std::endl;
        if (VendorAction(hexPNPID)) {
            std::cout << "  Success!" << std::endl;
            gotOne = true;
        }
        std::cout << std::endl;
    }

    // Unsure if this is a feature or annoying. If no calls result in success,
    // it waits for an enter hit.
    if (!gotOne) {
        std::cout << "Press enter to exit..." << std::endl;
        std::cin.ignore();
    }

    return 0;
}

#endif // INCLUDED_DirectModeToolSkeleton_h_GUID_03A2F8B3_098E_448B_1B25_19A4EAA5DFA7
