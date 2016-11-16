/** @file
    @brief Header for internal usage in making tooling.

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

#ifndef INCLUDED_ToolingArguments_h_GUID_743CB846_DBFB_489B_AD6D_3A65A426225A
#define INCLUDED_ToolingArguments_h_GUID_743CB846_DBFB_489B_AD6D_3A65A426225A

// Internal Includes
#include "osvr/RenderKit/VendorIdTools.h" // pnpidToHex
#include "osvr/RenderKit/CleanPNPIDString.h"
#include "osvr/RenderKit/DirectModeVendors.h" // formatAsHexString

// Library/third-party includes
// - none

// Standard includes
#include <string>
#include <iostream>

static const auto HELP_FLAG = "-h";
static const auto NO_WAIT_FLAG = "--no-wait";
static bool g_waitAtExit = true;
static std::string g_customPNPID;

static inline int Usage(const char* name) {
    std::cerr << "Usage:\n";
    std::cerr << "\t" << name << " [" << HELP_FLAG << "]\n";
    std::cerr << "\t" << name << " [" << NO_WAIT_FLAG << "] [<3-letter Vendor PNPID>]\n";
    std::cerr << std::endl;
    std::cerr << "Options:\n";
    std::cerr << "\t" << HELP_FLAG << "\n\t\tDisplay this usage information.\n";
    std::cerr << "\t" << NO_WAIT_FLAG << "\n\t\tDo not wait for user input "
                                         "before exiting (for scripting "
                                         "use).\n";
    std::cerr << "\t"
              << "<3-letter Vendor PNPID>"
              << "\n\t\tOptionally use a vendor ID for HMD monitor "
                 "vendors not used by default.\n";
    return -1;
}

/// Waits at exit (conditionally)
static inline void waitAtExit() {
    if (g_waitAtExit) {
        std::cout << "Press enter to exit..." << std::endl;
        std::cin.ignore();
    } else {
        std::cout << "Was passed " << NO_WAIT_FLAG << ", exiting without waiting." << std::endl;
    }
}

static inline int toolingParseArgs(int argc, char* argv[]) {
    if (argc == 1) {
        return 0;
    }
    using namespace osvr::renderkit;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == HELP_FLAG) {
            return Usage(argv[0]);
        }
        if (arg == NO_WAIT_FLAG) {
            g_waitAtExit = false;
            continue;
        }

        /// OK, not a known flag, so we'll assume it's a PNPID.
        if (!g_customPNPID.empty()) {
            std::cerr << "Extra vendor ID already specified - unrecognized "
                         "command line argument."
                      << std::endl;
            return Usage(argv[0]);
        }
        g_customPNPID = vendorid::cleanPotentialPNPID(arg);
        if (g_customPNPID.empty()) {
            std::cerr << "custom pnpid wrong size or character in custom pnpid "
                         "not in [A-Z]\n"
                      << std::endl;

            return Usage(argv[0]);
        }
        auto hexPNPID = pnpidToFlippedHex(g_customPNPID);
        std::cout << "Will include custom PNPID from command line '" << g_customPNPID << "' [hex "
                  << vendorid::formatAsHexString(hexPNPID) << "]" << std::endl;
    }
    return 0;
}

#endif // INCLUDED_ToolingArguments_h_GUID_743CB846_DBFB_489B_AD6D_3A65A426225A
