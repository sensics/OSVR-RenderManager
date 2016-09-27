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

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()
#include <chrono>

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

    // Set up the viewport we're going to draw into.
    CD3D11_VIEWPORT viewport(static_cast<float>(renderInfo.viewport.left),
                             static_cast<float>(renderInfo.viewport.lower),
                             static_cast<float>(renderInfo.viewport.width),
                             static_cast<float>(renderInfo.viewport.height));
    context->RSSetViewports(1, &viewport);

    // Make a grey background
    FLOAT colorRgba[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    context->ClearRenderTargetView(renderTargetView, colorRgba);
    context->ClearDepthStencilView(
        depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    osvr::renderkit::OSVR_PoseState_to_D3D(viewD3D, renderInfo.pose);
    osvr::renderkit::OSVR_Projection_to_D3D(projectionD3D,
                                            renderInfo.projection);

    XMMATRIX xm_projectionD3D(projectionD3D), xm_viewD3D(viewD3D);

    // draw room
    simpleShader.use(device, context, xm_projectionD3D, xm_viewD3D, identity);
    roomCube.draw(device, context);
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
                Usage(argv[0]);
            }
    }
    if (realParams != 0) {
        Usage(argv[0]);
    }

    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context(
        "osvr.RenderManager.D3DPresentExample3D");

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

    // Set up the vector of textures to render to and any framebuffer
    // we need to group them.
    std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
    std::vector<ID3D11Texture2D*> depthStencilTextures;
    std::vector<ID3D11DepthStencilView*> depthStencilViews;

    HRESULT hr;
    for (size_t i = 0; i < renderInfo.size(); i++) {

        // The color buffer for this eye.  We need to put this into
        // a generic structure for the Present function, but we only need
        // to fill in the Direct3D portion.
        //  Note that this texture format must be RGBA and unsigned byte,
        // so that we can present it to Direct3D for DirectMode.
        ID3D11Texture2D* D3DTexture = nullptr;
        unsigned width = static_cast<int>(renderInfo[i].viewport.width);
        unsigned height = static_cast<int>(renderInfo[i].viewport.height);

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
        hr = renderInfo[i].library.D3D11->device->CreateTexture2D(
            &textureDesc, nullptr, &D3DTexture);
        if (FAILED(hr)) {
            std::cerr << "Can't create texture for eye " << i << std::endl;
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
        hr = renderInfo[i].library.D3D11->device->CreateRenderTargetView(
            D3DTexture, &renderTargetViewDesc, &renderTargetView);
        if (FAILED(hr)) {
            std::cerr << "Could not create render target for eye " << i
                      << std::endl;
            return -2;
        }

        // Push the filled-in RenderBuffer onto the vector.
        osvr::renderkit::RenderBufferD3D11* rbD3D =
            new osvr::renderkit::RenderBufferD3D11;
        rbD3D->colorBuffer = D3DTexture;
        rbD3D->colorBufferView = renderTargetView;
        osvr::renderkit::RenderBuffer rb;
        rb.D3D11 = rbD3D;
        renderBuffers.push_back(rb);

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
        ID3D11Texture2D* depthStencilBuffer;
        hr = renderInfo[i].library.D3D11->device->CreateTexture2D(
            &textureDescription, NULL, &depthStencilBuffer);
        if (FAILED(hr)) {
            std::cerr << "Could not create depth/stencil texture for eye " << i
                      << std::endl;
            return -4;
        }
        depthStencilTextures.push_back(depthStencilBuffer);

        // Create the depth/stencil view description
        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription = {};
        depthStencilViewDescription.Format = textureDescription.Format;
        depthStencilViewDescription.ViewDimension =
            D3D11_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDescription.Texture2D.MipSlice = 0;

        ID3D11DepthStencilView* depthStencilView;
        hr = renderInfo[i].library.D3D11->device->CreateDepthStencilView(
            depthStencilBuffer, &depthStencilViewDescription,
            &depthStencilView);
        if (FAILED(hr)) {
            std::cerr << "Could not create depth/stencil view for eye " << i
                      << std::endl;
            return -5;
        }
        depthStencilViews.push_back(depthStencilView);
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

    // Register our constructed buffers so that we can use them for
    // presentation.
    if (!render->RegisterRenderBuffers(renderBuffers)) {
        std::cerr << "RegisterRenderBuffers() returned false, cannot continue"
                  << std::endl;
        quit = true;
    }

    // Timing of frame rates
    size_t count = 0;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

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
            RenderView(renderInfo[i], renderBuffers[i].D3D11->colorBufferView,
                       depthStencilViews[i]);
        }

        // Send the rendered results to the screen
        if (!render->PresentRenderBuffers(renderBuffers, renderInfo)) {
            std::cerr << "PresentRenderBuffers() returned false, maybe because "
                         "it was asked to quit"
                      << std::endl;
            quit = true;
        }

        // Timing information
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_sec = end - start;
        if (elapsed_sec.count() >= 2) {
            std::chrono::duration<double, std::micro> elapsed_usec =
                end - start;
            double usec = elapsed_usec.count();
            std::cout << "Rendering at " << count / (usec * 1e-6) << " fps"
                      << std::endl;
            start = end;
            count = 0;
        }
        count++;
    }

    // Clean up after ourselves.
    // @todo

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
