/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           to render solid colors to each display.  This example program
           uses no graphics-system specific calls, so in that sense it is
           rendering-system independent.  You can set the rendering system
           to use on the command line or use the default.

    @date 2015

    @author
    Russ Taylor <russ@sensics.com>
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/RenderKit/RenderManager.h>

// Library/third-party includes
#ifdef _WIN32
#include <windows.h>
#include <initguid.h>
#endif
#include <vrpn_Shared.h>
#include <quat.h>

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// Set to true when it is time for the application to quit.
// Handlers below that set it to true when the user causes
// any of a variety of events so that we shut down the system
// cleanly.  This only works on Windows, but so does D3D...
static bool quit = false;

#ifdef _WIN32
// Note: On Windows, this runs in a different thread from
// the main application.
static BOOL CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
    // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        quit = true;
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

// This callback sets a boolean value whose pointer is passed in to
// the state of the button that was pressed.  This lets the callback
// be used to handle any button press that just needs to update state.
void myButtonCallback(void* userdata, const OSVR_TimeValue* /*timestamp*/,
                      const OSVR_ButtonReport* report) {
    bool* result = static_cast<bool*>(userdata);
    *result = (report->state != 0);
}

void Usage(std::string name) {
    std::cerr << "Usage: " << name << "[graphicslibrary]" << std::endl;
    std::cerr << "       Default graphics library is OpenGL" << std::endl;
    exit(-1);
}

int main(int argc, char* argv[]) {
    // Parse the command line
    double colorRateCyclesPerSecond = 0.5;
    std::string graphicsLibrary = "OpenGL";
    int realParams = 0;
    for (int i = 1; i < argc; i++) {
#if 0
        if (argv[i][0] == '-') {
            Usage(argv[0]);
        }
        else
#endif
        switch (++realParams) {
        case 1:
            graphicsLibrary = argv[i];
            break;
        default:
            Usage(argv[0]);
        }
    }
    if (realParams > 1) {
        Usage(argv[0]);
    }

    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context("org.RenderManager.SolidColor");

    // Construct button devices and connect them to a callback
    // that will set the "quit" variable to true when it is
    // pressed.  Use button "1" on the left-hand or
    // right-hand controller.
    osvr::clientkit::Interface leftButton1 =
        context.getInterface("/controller/left/1");
    leftButton1.registerCallback(&myButtonCallback, &quit);

    osvr::clientkit::Interface rightButton1 =
        context.getInterface("/controller/right/1");
    rightButton1.registerCallback(&myButtonCallback, &quit);

    // Open Direct3D and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    osvr::renderkit::RenderManager* render =
        osvr::renderkit::createRenderManager(context.get(),
        graphicsLibrary.c_str());
    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

// Set up a handler to cause us to exit cleanly.
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    // Open the display and make sure this worked.
    osvr::renderkit::RenderManager::OpenResults ret = render->OpenDisplay();
    if (ret.status == osvr::renderkit::RenderManager::OpenStatus::FAILURE) {
        std::cerr << "Could not open display" << std::endl;
        return 2;
    }

    // Do a call to get the information we need to construct our
    // color vector.
    std::vector<osvr::renderkit::RenderInfo> renderInfo;
    context.update();
    renderInfo = render->GetRenderInfo();

    // Continue rendering until it is time to quit.
    struct timeval start;
    vrpn_gettimeofday(&start, nullptr);
    while (!quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        // Figure out the color to use, which cycles from black up
        // through white
        struct timeval now;
        vrpn_gettimeofday(&now, nullptr);
        double loops =
            colorRateCyclesPerSecond * vrpn_TimevalDurationSeconds(now, start);
        float c = static_cast<float>(loops - floor(loops));

        // Set up the vector of colors to render
        osvr::renderkit::RGBColorf color = { c, c, c };

        // Color each buffer using the specified information.
        if (!render->PresentSolidColor(color)) {
            std::cerr << "PresentSolidColors() returned false, maybe because "
                         "it was asked to quit"
                      << std::endl;
            quit = true;
        }
    }

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
