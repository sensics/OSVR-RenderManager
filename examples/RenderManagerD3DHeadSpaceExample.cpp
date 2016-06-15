/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and D3D to render a scene with low latency.

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
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <quat.h>

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// This must come after we include <d3d11.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryD3D11.h>

// Includes from our own directory
#include "pixelshader3d.h"
#include "vertexshader3d.h"

using namespace DirectX;

#include "D3DCube.h"
#include "D3DSimpleShader.h"

// Set to true when it is time for the application to quit.
// Handlers below that set it to true when the user causes
// any of a variety of events so that we shut down the system
// cleanly.  This only works on Windows, but so does D3D...
static bool quit = false;
static Cube roomCube(5.0f, true);
static Cube handCube(0.05f, false);
static Cube headSpaceCube(0.005f, false);
static Cube extraCube(0.1f, false);
static SimpleShader simpleShader;

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

void SetupRendering(osvr::renderkit::GraphicsLibrary library) {
    // Nothing to do here yet. SimpleShader and Cube initialize themselves
    // lazily.
}

// Callback to set up for rendering into a given display (which may have on or
// more eyes).
void SetupDisplay(
    void* userData //< Passed into SetViewProjectionCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr << "SetupDisplay: No D3D11 GraphicsLibrary" << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr << "SetupDisplay: No D3D11 RenderBuffer" << std::endl;
        return;
    }

    auto context = library.D3D11->context;
    auto renderTargetView = buffers.D3D11->colorBufferView;
    auto depthStencilView = buffers.D3D11->depthStencilView;

    // Set up to render to the textures for this eye
    // RenderManager will have already set our render target to this
    // eye's buffer, so we don't need to do that here.

    // Perform a random colorfill.  This does not have to be random, but
    // random draws attention if we leave any background showing.
    FLOAT red = static_cast<FLOAT>((double)rand() / (double)RAND_MAX);
    FLOAT green = static_cast<FLOAT>((double)rand() / (double)RAND_MAX);
    FLOAT blue = static_cast<FLOAT>((double)rand() / (double)RAND_MAX);
    FLOAT colorRgba[4] = {0.3f * red, 0.3f * green, 0.3f * blue, 1.0f};
    context->ClearRenderTargetView(renderTargetView, colorRgba);
    context->ClearDepthStencilView(
        depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

// Callback to set up for rendering into a given eye (viewpoint and projection).
void SetupEye(
    void* userData //< Passed into SetViewProjectionCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ,
    osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport set by RenderManager
    ,
    osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    ,
    size_t whichEye //< Which eye are we setting up for?
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr
            << "SetupEye: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr << "SetupEye: No D3D11 RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    // RenderManager will have already set our viewport, so we don't need to
    // do anything here.
}

// Callbacks to draw things in world space, head space, left-hand space, and
// right-hand
// space.
void DrawWorld(
    void* userData //< Passed into AddRenderCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ,
    osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    ,
    OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    ,
    osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    ,
    OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr
            << "DrawWorld: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr << "DrawWorld: No D3D11 RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    auto context = library.D3D11->context;
    auto device = library.D3D11->device;
    float projectionD3D[16];
    float viewD3D[16];
    XMMATRIX identity = XMMatrixIdentity();

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, pose);
    osvr::renderkit::OSVR_Projection_to_D3D(projectionD3D, projection);

    XMMATRIX xm_projectionD3D(projectionD3D), xm_viewD3D(viewD3D);

    // draw room
    simpleShader.use(device, context, xm_projectionD3D, xm_viewD3D, identity);
    roomCube.draw(device, context);

    // We want to draw another cube 1 meter along the -Z axis
    q_vec_type deltaZ = { 0, 0, -1.0 };
    q_vec_type deltaZWorld;
    q_type rot;
    rot[Q_X] = osvrQuatGetX(&pose.rotation);
    rot[Q_Y] = osvrQuatGetY(&pose.rotation);
    rot[Q_Z] = osvrQuatGetZ(&pose.rotation);
    rot[Q_W] = osvrQuatGetW(&pose.rotation);
    q_xform(deltaZWorld, rot, deltaZ);
    pose.translation.data[0] += deltaZWorld[0];
    pose.translation.data[1] += deltaZWorld[1];
    pose.translation.data[2] += deltaZWorld[2];

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, pose);

    XMMATRIX xm_viewCubeD3D(viewD3D);

    // draw a small cube
    simpleShader.use(device, context, xm_projectionD3D, xm_viewCubeD3D, identity);
    extraCube.draw(device, context);
}

void DrawHeadSpace(
    void* userData //< Passed into AddRenderCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ,
    osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    ,
    OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    ,
    osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    ,
    OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr
            << "DrawHeadSpace: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr
            << "DrawHeadSpace: No D3D11 RenderBuffer, this should not happen"
            << std::endl;
        return;
    }

    auto context = library.D3D11->context;
    auto device = library.D3D11->device;
    auto renderTargetView = buffers.D3D11->colorBufferView;
    float projectionD3D[16];
    float viewD3D[16];
    XMMATRIX identity = XMMatrixIdentity();

    // We want to draw our head space out in front of the user, which is
    // along -Z.  We move things 0.25 meters along the Z axis.  Note that
    // the Z axis may be rotated with respect to the head-space Z axis
    // because the display may not be aligned with head space.
    // NOTE: This code uses quatlib to handle the rotation, but any math
    // library can be used.
    q_vec_type deltaZ = {0, 0, -0.25};
    q_vec_type deltaZWorld;
    q_type rot;
    rot[Q_X] = osvrQuatGetX(&pose.rotation);
    rot[Q_Y] = osvrQuatGetY(&pose.rotation);
    rot[Q_Z] = osvrQuatGetZ(&pose.rotation);
    rot[Q_W] = osvrQuatGetW(&pose.rotation);
    q_xform(deltaZWorld, rot, deltaZ);
    pose.translation.data[0] += deltaZWorld[0];
    pose.translation.data[1] += deltaZWorld[1];
    pose.translation.data[2] += deltaZWorld[2];

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, pose);
    osvr::renderkit::OSVR_Projection_to_D3D(projectionD3D, projection);

    XMMATRIX xm_projectionD3D(projectionD3D), xm_viewD3D(viewD3D);

    // draw a small cube
    simpleShader.use(device, context, xm_projectionD3D, xm_viewD3D, identity);
    headSpaceCube.draw(device, context);
}

void DrawLeftHand(
    void* userData //< Passed into AddRenderCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ,
    osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    ,
    OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    ,
    osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    ,
    OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr
            << "DrawLeftHand: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr
            << "DrawLeftHand: No D3D11 RenderBuffer, this should not happen"
            << std::endl;
        return;
    }

    auto context = library.D3D11->context;
    auto device = library.D3D11->device;
    auto renderTargetView = buffers.D3D11->colorBufferView;
    float projectionD3D[16];
    float viewD3D[16];
    XMMATRIX identity = XMMatrixIdentity();

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, pose);
    osvr::renderkit::OSVR_Projection_to_D3D(projectionD3D, projection);

    XMMATRIX _projectionD3D(projectionD3D), _viewD3D(viewD3D);

    // draw left hand
    simpleShader.use(device, context, _projectionD3D, _viewD3D, identity);
    handCube.draw(device, context);
}

void DrawRightHand(
    void* userData //< Passed into AddRenderCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ,
    osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    ,
    OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    ,
    osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    ,
    OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr
            << "DrawRightHand: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr
            << "DrawRightHand: No D3D11 RenderBuffer, this should not happen"
            << std::endl;
        return;
    }

    auto context = library.D3D11->context;
    auto device = library.D3D11->device;
    auto renderTargetView = buffers.D3D11->colorBufferView;
    float projectionD3D[16];
    float viewD3D[16];
    XMMATRIX identity = XMMatrixIdentity();

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, pose);
    osvr::renderkit::OSVR_Projection_to_D3D(projectionD3D, projection);

    XMMATRIX _projectionD3D(projectionD3D), _viewD3D(viewD3D);

    // draw right hand
    simpleShader.use(device, context, _projectionD3D, _viewD3D, identity);
    handCube.draw(device, context);
}

void Usage(std::string name) {
    std::cerr << "Usage: " << name << std::endl;
    exit(-1);
}

int main(int argc, char* argv[]) {
    // Parse the command line
    int realParams = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            Usage(argv[0]);
        } else
            switch (++realParams) {
            case 1:
            default:
                std::cerr << "Too many arguments" << std::endl;
                Usage(argv[0]);
            }
    }
    if (realParams != 0) {
        std::cerr << "Wrong number of arguments (" << realParams << ")"
                  << std::endl;
        Usage(argv[0]);
    }

    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context(
        "org.opengoggles.exampleclients.TrackerCallback");

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
        osvr::renderkit::createRenderManager(context.get(), "Direct3D11");

    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    // Set callback to handle setting up rendering in a display
    render->SetDisplayCallback(SetupDisplay);

    // Set callback to handle setting up rendering in an eye
    render->SetViewProjectionCallback(SetupEye);

    // Register callbacks to render things in left hand, right
    // hand, and world space.
    render->AddRenderCallback("/", DrawWorld);
    render->AddRenderCallback("/me/head", DrawHeadSpace);
    render->AddRenderCallback("/me/hands/left", DrawLeftHand);
    render->AddRenderCallback("/me/hands/right", DrawRightHand);

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
    if (ret.library.D3D11 == nullptr) {
        std::cerr << "Attempted to run a Direct3D11 program with a config file "
                  << "that specified a different rendering library."
                  << std::endl;
        return 10;
    }

    // Set up the rendering state we need.
    try {
        SetupRendering(ret.library);
    } catch (const std::runtime_error& error) {
        std::cerr << "Error during SetupRendering: " << error.what();
        return 3;
    }

    // Continue rendering until it is time to quit.
    while (!quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        if (!render->Render()) {
            std::cerr
                << "Render() returned false, maybe because it was asked to quit"
                << std::endl;
            quit = true;
        }
    }

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
