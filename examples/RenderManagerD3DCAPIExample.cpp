/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and D3D to render a scene with low latency.  This uses the C
           API to RenderManager.

    @date 2016

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
#include <osvr/RenderKit/RenderManagerC.h>
#include <osvr/RenderKit/RenderManagerD3D11C.h>

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
    const OSVR_RenderInfoD3D11& renderInfo //< Info needed to render
    ,
    ID3D11RenderTargetView* renderTargetView,
    ID3D11DepthStencilView* depthStencilView) {

    auto context = renderInfo.library.context;
    auto device = renderInfo.library.device;
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

    OSVR_PoseState_to_D3D(viewD3D, renderInfo.pose);
    OSVR_Projection_to_D3D(projectionD3D,
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
    OSVR_GraphicsLibraryD3D11 library;
    library.context = nullptr;
    library.device = nullptr;
    OSVR_RenderManager render;
    OSVR_RenderManagerD3D11 renderD3D;
    if (OSVR_RETURN_SUCCESS != osvrCreateRenderManagerD3D11(
        context.get(), "Direct3D11", library, &render, &renderD3D)) {
      std::cerr << "Could not create RenderManager" << std::endl;
      return 1;
    }

// Set up a handler to cause us to exit cleanly.
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    // Open the display and make sure this worked.
    OSVR_OpenResultsD3D11 openResults;
    if ( (OSVR_RETURN_SUCCESS != osvrRenderManagerOpenDisplayD3D11(
        renderD3D, &openResults)) || 
        (openResults.status == OSVR_OPEN_STATUS_FAILURE) ) {
      std::cerr << "Could not open display" << std::endl;
      return 2;
    }
    if (openResults.library.device == nullptr) {
        std::cerr << "Could not get device when opening display."
                  << std::endl;
        return 3;
    }
    if (openResults.library.context == nullptr) {
        std::cerr << "Could not get context when opening display."
          << std::endl;
        return 4;
    }

    // Do a call to get the information we need to construct our
    // color and depth render-to-texture buffers.  This involves
    // getting rendering info for each buffer we're going to create.
    // We first find out the number of buffers, which pulls in the
    // info internally and stores it until we can query each one.
    OSVR_RenderInfoCount numRenderInfo;
    OSVR_RenderParams renderParams;
    osvrRenderManagerGetDefaultRenderParams(&renderParams);
    std::vector<OSVR_RenderInfoD3D11> renderInfo;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetNumRenderInfo(
        render, renderParams, &numRenderInfo))) {
      std::cerr << "Could not get context number of render infos."
        << std::endl;
      return 5;
    }
    renderInfo.clear();
    for (OSVR_RenderInfoCount i = 0; i < numRenderInfo; i++) {
      OSVR_RenderInfoD3D11 info;
      if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoD3D11(
          renderD3D, i, renderParams, &info))) {
        std::cerr << "Could not get render info " << i
          << std::endl;
        return 6;
      }
      renderInfo.push_back(info);
    }

    // Set up the vector of textures to render to and any framebuffer
    // we need to group them.
    std::vector<OSVR_RenderBufferD3D11> renderBuffers;
    std::vector<ID3D11Texture2D*> depthStencilTextures;
    std::vector<ID3D11DepthStencilView*> depthStencilViews;

    HRESULT hr;
    for (size_t i = 0; i < numRenderInfo; i++) {

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
        hr = renderInfo[i].library.device->CreateTexture2D(
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
        hr = renderInfo[i].library.device->CreateRenderTargetView(
            D3DTexture, &renderTargetViewDesc, &renderTargetView);
        if (FAILED(hr)) {
            std::cerr << "Could not create render target for eye " << i
                      << std::endl;
            return -2;
        }

        // Push the filled-in RenderBuffer onto the vector.
        OSVR_RenderBufferD3D11 rbD3D;
        rbD3D.colorBuffer = D3DTexture;
        rbD3D.colorBufferView = renderTargetView;
        renderBuffers.push_back(rbD3D);

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
        hr = renderInfo[i].library.device->CreateTexture2D(
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
        hr = renderInfo[i].library.device->CreateDepthStencilView(
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
    hr = renderInfo[0].library.device->CreateDepthStencilState(
        &depthStencilDescription, &depthStencilState);
    if (FAILED(hr)) {
        std::cerr << "Could not create depth/stencil state" << std::endl;
        return -3;
    }

    // Register our constructed buffers so that we can use them for
    // presentation.
    OSVR_RenderManagerRegisterBufferState registerBufferState;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerStartRegisterRenderBuffers(
      &registerBufferState))) {
      std::cerr << "Could not start registering render buffers" << std::endl;
      return -4;
    }
    for (size_t i = 0; i < numRenderInfo; i++) {
      if ((OSVR_RETURN_SUCCESS != osvrRenderManagerRegisterRenderBufferD3D11(
        registerBufferState, renderBuffers[i]))) {
        std::cerr << "Could not register render buffer " << i << std::endl;
        return -5;
      }
    }
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerFinishRegisterRenderBuffers(
      render, registerBufferState, false))) {
      std::cerr << "Could not start finish registering render buffers" << std::endl;
      return -6;
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

        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetNumRenderInfo(
          render, renderParams, &numRenderInfo))) {
          std::cerr << "Could not get context number of render infos."
            << std::endl;
          return 105;
        }
        renderInfo.clear();
        for (OSVR_RenderInfoCount i = 0; i < numRenderInfo; i++) {
          OSVR_RenderInfoD3D11 info;
          if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoD3D11(
            renderD3D, i, renderParams, &info))) {
            std::cerr << "Could not get render info " << i
              << std::endl;
            return 106;
          }
          renderInfo.push_back(info);
        }

        // Render into each buffer using the specified information.
        for (size_t i = 0; i < renderInfo.size(); i++) {
            renderInfo[i].library.context->OMSetDepthStencilState(
                depthStencilState, 1);
            RenderView(renderInfo[i], renderBuffers[i].colorBufferView,
                       depthStencilViews[i]);
        }

        // Every other second, we show a black screen to test how
        // a game engine might blank it between scenes.  Every even
        // second, we display the video.
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_sec = end - start;
        int secs = static_cast<int>(elapsed_sec.count());

        if (secs % 2 == 0) {
          // Send the rendered results to the screen
          OSVR_RenderManagerPresentState presentState;
          if ((OSVR_RETURN_SUCCESS != osvrRenderManagerStartPresentRenderBuffers(
            &presentState))) {
            std::cerr << "Could not start presenting render buffers" << std::endl;
            return 201;
          }
          OSVR_ViewportDescription fullView;
          fullView.left = fullView.lower = 0;
          fullView.width = fullView.height = 1;
          for (size_t i = 0; i < numRenderInfo; i++) {
            if ((OSVR_RETURN_SUCCESS != osvrRenderManagerPresentRenderBufferD3D11(
              presentState, renderBuffers[i], renderInfo[i], fullView))) {
              std::cerr << "Could not present render buffer " << i << std::endl;
              return 202;
            }
          }
          if ((OSVR_RETURN_SUCCESS != osvrRenderManagerFinishPresentRenderBuffers(
            render, presentState, renderParams, false))) {
            std::cerr << "Could not finish presenting render buffers" << std::endl;
            return 203;
          }
        } else {
          // send a black screen.
          OSVR_RGB_FLOAT black;
          black.r = black.g = black.b = 0;
          osvrRenderManagerPresentSolidColorf(render, black);
        }

        // Timing information
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

    return 0;
}
