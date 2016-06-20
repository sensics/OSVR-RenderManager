/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and D3D to render a scene with adjustable rendering latency.

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

// Callbacks to draw things in world space, left-hand space, and right-hand
// space.
void RenderView(
    size_t eye //< Which eye we are rendering
    ,
    const osvr::renderkit::RenderInfo& renderInfo //< Info needed to render
    ,
    ID3D11RenderTargetView* renderTargetView,
    ID3D11DepthStencilView* depthStencilView) {
    // Make sure our pointers are filled in correctly.
    if (renderInfo.library.D3D11 == nullptr) {
        std::cerr
            << "SetupDisplay: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }

    auto context = renderInfo.library.D3D11->context;
    auto device = renderInfo.library.D3D11->device;
    float projectionD3D[16];
    float viewD3D[16];
    XMMATRIX identity = XMMatrixIdentity();

    // Set up to render to the textures for this eye
    context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

    // Set the viewport to cover the fraction of our render buffer that
    // this eye is responsible for.  This is always the same width and
    // height but shifts over by one width for each eye.
    CD3D11_VIEWPORT viewport(
        static_cast<float>(eye * renderInfo.viewport.width), 0,
        static_cast<float>(renderInfo.viewport.width),
        static_cast<float>(renderInfo.viewport.height));
    context->RSSetViewports(1, &viewport);

    // Make a grey background.  Only clear for the first eye,
    // because clear in DirectX 10 and 11 does not respect the
    // boundaries of the viewport.
    FLOAT colorRgba[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    if (eye == 0) {
        context->ClearRenderTargetView(renderTargetView, colorRgba);
        context->ClearDepthStencilView(
            depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, renderInfo.pose);
    osvr::renderkit::OSVR_Projection_to_D3D(projectionD3D,
                                            renderInfo.projection);

    XMMATRIX _projectionD3D(projectionD3D), _viewD3D(viewD3D);

    // draw room
    simpleShader.use(device, context, _projectionD3D, _viewD3D, identity);
    roomCube.draw(device, context);
}

void Usage(std::string name) {
    std::cerr << "Usage: " << name << " [millisecondRenderingDelay]" << std::endl;
    exit(-1);
}

int main(int argc, char* argv[]) {
    // Parse the command line
    int delayMilliSeconds = 0;
    int realParams = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            Usage(argv[0]);
        } else
            switch (++realParams) {
            case 1:
                delayMilliSeconds = atoi(argv[i]);
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
        return 3;
    }

    // Do a call to get the information we need to construct our
    // color and depth render-to-texture buffers.
    std::vector<osvr::renderkit::RenderInfo> renderInfo;
    context.update();
    renderInfo = render->GetRenderInfo();

    // Make sure that all of the viewports in the eyes are
    // the same, so that we can generate a single buffer to
    // consolidate them all by just abutting them side by
    // side.
    for (size_t i = 1; i < renderInfo.size(); i++) {
        if (renderInfo[0].viewport != renderInfo[i].viewport) {
            std::cerr << "Viewport " << i << " != Viewport 0" << std::endl;
            return 4;
        }
    }

    // Set up the texture to render to and any framebuffer
    // we need to group them.  We're just going to make one buffer,
    // but make it wide enough
    // to handle all of the eyes we need.
    osvr::renderkit::RenderBuffer renderBuffer;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11DepthStencilView* depthStencilView;

    // The color buffer.  We need to put this into
    // a generic structure for the Present function, but we only need
    // to fill in the Direct3D portion.
    //  Note that this texture format must be RGBA and unsigned byte,
    // so that we can present it to Direct3D for DirectMode.
    HRESULT hr;
    ID3D11Texture2D* D3DTexture = nullptr;
    unsigned width =
        static_cast<int>(renderInfo[0].viewport.width * renderInfo.size());
    unsigned height = static_cast<int>(renderInfo[0].viewport.height);

    // Initialize a new render target texture description.
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    // We need it to be both a render target and a shader resource
    textureDesc.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    // Create a new render target texture to use.
    hr = renderInfo[0].library.D3D11->device->CreateTexture2D(
        &textureDesc, nullptr, &D3DTexture);
    if (FAILED(hr)) {
        std::cerr << "Can't create color texture" << std::endl;
        return -1;
    }

    // Fill in the resource view for your render texture buffer here
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
    // This must match what was created in the texture to be rendered
    renderTargetViewDesc.Format = textureDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    // Create the render target view.
    ID3D11RenderTargetView*
        renderTargetView; //< Pointer to our render target view
    hr = renderInfo[0].library.D3D11->device->CreateRenderTargetView(
        D3DTexture, &renderTargetViewDesc, &renderTargetView);
    if (FAILED(hr)) {
        std::cerr << "Could not create render target view" << std::endl;
        return -2;
    }

    // Push the filled-in RenderBuffer onto the stack.
    osvr::renderkit::RenderBufferD3D11* rbD3D =
        new osvr::renderkit::RenderBufferD3D11;
    rbD3D->colorBuffer = D3DTexture;
    rbD3D->colorBufferView = renderTargetView;
    renderBuffer.D3D11 = rbD3D;

    //==================================================================
    // Create a depth buffer

    // Make the depth/stencil texture.
    D3D11_TEXTURE2D_DESC textureDescription = {};
    textureDescription.SampleDesc.Count = 1;
    textureDescription.SampleDesc.Quality = 0;
    textureDescription.Usage = D3D11_USAGE_DEFAULT;
    textureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.CPUAccessFlags = 0;
    textureDescription.MiscFlags = 0;
    /// @todo Make this a parameter
    textureDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    hr = renderInfo[0].library.D3D11->device->CreateTexture2D(
        &textureDescription, nullptr, &depthStencilTexture);
    if (FAILED(hr)) {
        std::cerr << "Could not create depth/stencil texture" << std::endl;
        return -4;
    }

    // Create the depth/stencil view description
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription = {};
    depthStencilViewDescription.Format = textureDescription.Format;
    depthStencilViewDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDescription.Texture2D.MipSlice = 0;

    hr = renderInfo[0].library.D3D11->device->CreateDepthStencilView(
        depthStencilTexture, &depthStencilViewDescription, &depthStencilView);
    if (FAILED(hr)) {
        std::cerr << "Could not create depth/stencil view" << std::endl;
        return -5;
    }

    // Create depth stencil state.
    // Describe how depth and stencil tests should be performed.
    D3D11_DEPTH_STENCIL_DESC depthStencilDescription = {};

    depthStencilDescription.DepthEnable = true;
    depthStencilDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDescription.DepthFunc = D3D11_COMPARISON_LESS;

    depthStencilDescription.StencilEnable = true;
    depthStencilDescription.StencilReadMask = 0xFF;
    depthStencilDescription.StencilWriteMask = 0xFF;

    // Front-facing stencil operations
    depthStencilDescription.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDescription.FrontFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_INCR;
    depthStencilDescription.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDescription.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    // Back-facing stencil operations
    depthStencilDescription.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDescription.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthStencilDescription.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDescription.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    ID3D11DepthStencilState* depthStencilState;
    hr = renderInfo[0].library.D3D11->device->CreateDepthStencilState(
        &depthStencilDescription, &depthStencilState);
    if (FAILED(hr)) {
        std::cerr << "Could not create depth/stencil state" << std::endl;
        return -3;
    }

    // Construct our vector of color buffers, which will all point to
    // the same one.
    // Also construct our vector of normalized viewports that we will
    // pass to PresentRenderBuffers.  This describes the region of
    // the normalized texture coordinates (0,0) to (1,1) that is
    // handled by each eye.  The eyes are stacked left to right
    // in the same buffer.
    std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
    double fraction = 1.0 / renderInfo.size();
    std::vector<osvr::renderkit::OSVR_ViewportDescription> NVCPs;
    for (size_t i = 0; i < renderInfo.size(); i++) {
        renderBuffers.push_back(renderBuffer);

        osvr::renderkit::OSVR_ViewportDescription v;
        v.left = fraction * i;
        v.lower = 0.0;
        v.width = fraction;
        v.height = 1;
        NVCPs.push_back(v);
    }

    // Register our constructed buffer so that we can use it for
    // presentation.
    if (!render->RegisterRenderBuffers(renderBuffers)) {
        std::cerr << "RegisterRenderBuffers() returned false, cannot continue"
                  << std::endl;
        quit = true;
    }

    // Continue rendering until it is time to quit.
    while (!quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        renderInfo = render->GetRenderInfo();

        // Render into each buffer using the specified information.
        for (size_t i = 0; i < renderInfo.size(); i++) {
            renderInfo[i].library.D3D11->context->OMSetDepthStencilState(
                depthStencilState, 1);
            RenderView(i, renderInfo[i],
                       renderBuffers[i].D3D11->colorBufferView,
                       depthStencilView);
        }

        // Delay the requested length of time.
        // Busy-wait so we don't get swapped out longer than we wanted.
        auto end = std::chrono::high_resolution_clock::now() +
                   std::chrono::milliseconds(delayMilliSeconds);
        do {
        } while (std::chrono::high_resolution_clock::now() < end);

        // Send the rendered results to the screen
        if (!render->PresentRenderBuffers(
                renderBuffers, renderInfo,
                osvr::renderkit::RenderManager::RenderParams(), NVCPs)) {
            std::cerr << "PresentRenderBuffers() returned false, maybe because "
                         "it was asked to quit"
                      << std::endl;
            quit = true;
        }
    }

    // Clean up after ourselves.
    // @todo

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
