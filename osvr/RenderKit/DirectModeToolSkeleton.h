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
#include "osvr/RenderKit/DirectModeVendors.h"
#include "osvr/RenderKit/ToolingArguments.h"

// Library/third-party includes
#include <iostream>

// Standard includes
#include <cstdint>

bool VendorAction(std::uint16_t flippedHexPNPID);

int main(int argc, char* argv[]) {
    auto argParseRet = toolingParseArgs(argc, argv);
    if (argParseRet != 0) {
        /// failure during argument parsing.
        return argParseRet;
    }

    using namespace osvr::renderkit;
    using namespace osvr;

    bool gotOne = false;

    if (!g_customPNPID.empty()) {
        auto hexPNPID = pnpidToFlippedHex(g_customPNPID);
        std::cout << "Trying custom PNPID from command line '" << g_customPNPID << "' [hex "
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

    // If no calls result in success and --no-wait wasn't specified, it waits for an enter hit before exiting.
    if (!gotOne) {
        waitAtExit();
    }

    return 0;
}

#endif // INCLUDED_DirectModeToolSkeleton_h_GUID_03A2F8B3_098E_448B_1B25_19A4EAA5DFA7
