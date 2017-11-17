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
#include <chrono>
#include <thread>
#include <stdlib.h> // For exit()

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
    size_t eye, //< Which eye we are rendering
    const OSVR_RenderInfoD3D11& renderInfo, //< Info needed to render
    ID3D11RenderTargetView* renderTargetView,
    ID3D11DepthStencilView* depthStencilView,
    ID3D11DeviceContext* context,
    ID3D11Device* device) {

    float projectionD3D[16];
    float viewD3D[16];
    XMMATRIX identity = XMMatrixIdentity();

    // Set up to render to the textures for this eye
    context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

    // Set up the viewport we're going to draw into.
    CD3D11_VIEWPORT viewport(
        static_cast<float>(eye * renderInfo.viewport.width),
        0.0f,
        static_cast<float>(renderInfo.viewport.width),
        static_cast<float>(renderInfo.viewport.height));
    context->RSSetViewports(1, &viewport);

    OSVR_PoseState_to_D3D(viewD3D, renderInfo.pose);
    OSVR_Projection_to_D3D(projectionD3D,
        renderInfo.projection);

    XMMATRIX xm_projectionD3D(projectionD3D), xm_viewD3D(viewD3D);

    // draw room
    simpleShader.use(device, context, xm_projectionD3D, xm_viewD3D, identity);
	//for (size_t i = 0; i < 10000; i++) {
		roomCube.draw(device, context);
	//}
}

void Usage(std::string name) {
  std::cerr << "Usage: " << name << " [millisecondRenderingDelay]" << std::endl;
  exit(-1);
}

struct FrameInfo {
    // Set up the vector of textures to render to and any framebuffer
    // we need to group them.
    OSVR_RenderBufferD3D11 renderBuffer;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11DepthStencilView* depthStencilView;
    IDXGIKeyedMutex* keyedMutex;
};

int main(int argc, char* argv[]) {
	std::cout << "Render thread id: " << std::this_thread::get_id() << std::endl;

    // Parse the command line
    int delayMilliSeconds = 0;
    int realParams = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            Usage(argv[0]);
        } else {
            switch (++realParams) {
            case 1:
              delayMilliSeconds = atoi(argv[i]);
              break;
            default:
                Usage(argv[0]);
            }
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

    // Create a D3D11 device and context to be used, rather than
    // having RenderManager make one for us.  This is an example
    // of using an external one, which would be needed for clients
    // that already have a rendering pipeline, like Unity.
    ID3D11Device* myDevice = nullptr;         // Fill this in
    ID3D11DeviceContext* myContext = nullptr; // Fill this in.

    // Here, we open the device and context ourselves, but if you
    // are working with a render library that provides them for you,
    // just stick them into the values rather than constructing
    // them.  (This is a bit of a toy example, because we could
    // just let RenderManager do this work for us and use the library
    // it sends back.  However, it does let us set parameters on the
    // device and context construction the way that we want, so it
    // might be useful.  Be sure to get D3D11 and have set
    // D3D11_CREATE_DEVICE_BGRA_SUPPORT in the device/context
    // creation, however it is done).
    D3D_FEATURE_LEVEL acceptibleAPI = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL foundAPI;
    auto hr =
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, &acceptibleAPI, 1,
        D3D11_SDK_VERSION, &myDevice, &foundAPI, &myContext);
    if (FAILED(hr)) {
        std::cerr << "Could not create D3D11 device and context" << std::endl;
        return -1;
    }

    // Put the device and context into a structure to let RenderManager
    // know to use this one rather than creating its own.
    OSVR_GraphicsLibraryD3D11 library;
    library.device = nullptr;
    library.context = nullptr;

    // Open Direct3D and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    OSVR_RenderManager render;
    OSVR_RenderManagerD3D11 renderD3D;
    if (OSVR_RETURN_SUCCESS != osvrCreateRenderManagerD3D11(
        context.get(), "Direct3D11", library, &render, &renderD3D)) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    if (render == nullptr || osvrRenderManagerGetDoingOkay(render) != OSVR_RETURN_SUCCESS) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    // Set up a handler to cause us to exit cleanly.
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    // Open the display and make sure this worked.
    OSVR_OpenResultsD3D11 openResults;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerOpenDisplayD3D11(
        renderD3D, &openResults)) ||
        (openResults.status == OSVR_OPEN_STATUS_FAILURE)) {
        std::cerr << "Could not open display" << std::endl;
        osvrDestroyRenderManager(render);
        return 2;
    }
    if (openResults.library.device == nullptr) {
        std::cerr << "Could not get device when opening display."
            << std::endl;
        osvrDestroyRenderManager(render);
        return 3;
    }
    if (openResults.library.context == nullptr) {
        std::cerr << "Could not get context when opening display."
            << std::endl;
        osvrDestroyRenderManager(render);
        return 4;
    }

    context.update();

    // Do a call to get the information we need to construct our
    // color and depth render-to-texture buffers.
    OSVR_RenderInfoCount numRenderInfo;
    OSVR_RenderParams renderParams;
    osvrRenderManagerGetDefaultRenderParams(&renderParams);
    std::vector<OSVR_RenderInfoD3D11> renderInfo;

    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetNumRenderInfo(
        render, renderParams, &numRenderInfo))) {
        std::cerr << "Could not get context number of render infos."
            << std::endl;
        osvrDestroyRenderManager(render);
        return 5;
    }

    renderInfo.clear();
    for (OSVR_RenderInfoCount i = 0; i < numRenderInfo; i++) {
        OSVR_RenderInfoD3D11 info;
        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoD3D11(
            renderD3D, i, renderParams, &info))) {
            std::cerr << "Could not get render info " << i
                << std::endl;
            osvrDestroyRenderManager(render);
            return 6;
        }
        renderInfo.push_back(info);
    }

    std::vector<FrameInfo> frameInfo(2);
    for (size_t frame = 0; frame < frameInfo.size(); frame++) {

        // The color buffer for this eye.  We need to put this into
        // a generic structure for the Present function, but we only need
        // to fill in the Direct3D portion.
        //  Note that this texture format must be RGBA and unsigned byte,
        // so that we can present it to Direct3D for DirectMode.
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
        // textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        // We need it to be both a render target and a shader resource
        textureDesc.BindFlags =
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        // Create a new render target texture to use.
        hr = myDevice->CreateTexture2D(
            &textureDesc, NULL, &D3DTexture);
        if (FAILED(hr)) {
            std::cerr << "Can't create texture" << std::endl;
            return -1;
        }

        IDXGIKeyedMutex* keyedMutex = nullptr;
        hr = D3DTexture->QueryInterface(
            __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
        if (FAILED(hr) || keyedMutex == nullptr) {
            std::cerr << "Could not get mutex pointer" << std::endl;
            return -2;
        }
        frameInfo[frame].keyedMutex = keyedMutex;

        // Fill in the resource view for your render texture buffer here
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        memset(&renderTargetViewDesc, 0, sizeof(renderTargetViewDesc));
        // This must match what was created in the texture to be rendered
        renderTargetViewDesc.Format = textureDesc.Format;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        // Create the render target view.
        ID3D11RenderTargetView*
            renderTargetView; //< Pointer to our render target view
        hr = myDevice->CreateRenderTargetView(
            D3DTexture, &renderTargetViewDesc, &renderTargetView);
        if (FAILED(hr)) {
            std::cerr << "Could not create render target"
                << std::endl;
            return -2;
        }

        // Push the filled-in RenderBuffer onto the vector.
        OSVR_RenderBufferD3D11 rbD3D;
        rbD3D.colorBuffer = D3DTexture;
        rbD3D.colorBufferView = renderTargetView;
        frameInfo[frame].renderBuffer = rbD3D;

        //==================================================================
        // Create a depth buffer

        // Make the depth/stencil texture.
        D3D11_TEXTURE2D_DESC textureDescription = { 0 };
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
        hr = myDevice->CreateTexture2D(
            &textureDescription, NULL, &depthStencilBuffer);
        if (FAILED(hr)) {
            std::cerr << "Could not create depth/stencil texture"
                << std::endl;
            return -4;
        }
        frameInfo[frame].depthStencilTexture = depthStencilBuffer;

        // Create the depth/stencil view description
        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription;
        memset(&depthStencilViewDescription, 0, sizeof(depthStencilViewDescription));
        depthStencilViewDescription.Format = textureDescription.Format;
        depthStencilViewDescription.ViewDimension =
            D3D11_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDescription.Texture2D.MipSlice = 0;

        ID3D11DepthStencilView* depthStencilView;
        hr = myDevice->CreateDepthStencilView(
            depthStencilBuffer,
            &depthStencilViewDescription,
            &depthStencilView);
        if (FAILED(hr)) {
            std::cerr << "Could not create depth/stencil view"
                << std::endl;
            return -5;
        }
        frameInfo[frame].depthStencilView = depthStencilView;
    }

    // Create depth stencil state.
    // Describe how depth and stencil tests should be performed.
    D3D11_DEPTH_STENCIL_DESC depthStencilDescription = { 0 };

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

    // We only have one depth/stencil state for all displays.
    ID3D11DepthStencilState *depthStencilState;
    hr = myDevice->CreateDepthStencilState(
        &depthStencilDescription,
        &depthStencilState);
    if (FAILED(hr)) {
        std::cerr << "Could not create depth/stencil state" << std::endl;
        return -3;
    }

    // create normalized cropping viewports for side-by-side rendering to a single render target
    std::vector<OSVR_ViewportDescription> NVCPs;
    double fraction = 1.0 / renderInfo.size();
    for (size_t i = 0; i < renderInfo.size(); i++) {
        OSVR_ViewportDescription v;
        v.left = fraction * i;
        v.lower = 0.0;
        v.width = fraction;
        v.height = 1;
        NVCPs.push_back(v);
    }

    // Register our constructed buffers so that we can use them for
    // presentation.
    OSVR_RenderManagerRegisterBufferState registerBufferState;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerStartRegisterRenderBuffers(
        &registerBufferState))) {
        std::cerr << "Could not start registering render buffers" << std::endl;
        osvrDestroyRenderManager(render);
        return -4;
    }
    for (size_t i = 0; i < frameInfo.size(); i++) {
        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerRegisterRenderBufferD3D11(
            registerBufferState, frameInfo[i].renderBuffer))) {
            std::cerr << "Could not register render buffer " << i << std::endl;
            osvrDestroyRenderManager(render);
            return -5;
        }
    }
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerFinishRegisterRenderBuffers(
        render, registerBufferState, true))) {
        std::cerr << "Could not finish registering render buffers" << std::endl;
        osvrDestroyRenderManager(render);
        return -6;
    }

    size_t iteration = 0;
    // Continue rendering until it is time to quit.
    while (!quit) {
        size_t frame = iteration % frameInfo.size();
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetNumRenderInfo(
            render, renderParams, &numRenderInfo))) {
            std::cerr << "Could not get context number of render infos."
                << std::endl;
            osvrDestroyRenderManager(render);
            return 105;
        }
        renderInfo.clear();
        for (OSVR_RenderInfoCount i = 0; i < numRenderInfo; i++) {
            OSVR_RenderInfoD3D11 info;
            if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoD3D11(
                renderD3D, i, renderParams, &info))) {
                std::cerr << "Could not get render info " << i
                    << std::endl;
                osvrDestroyRenderManager(render);
                return 106;
            }
            renderInfo.push_back(info);
        }

        // Grab and lock the mutex, so that we will be able to render
        // to it whether or not RenderManager locks it on our behalf.
        // it will not be auto-locked when we're in the non-ATW case.
        //std::cout << "RenderThread: locking buffer for frame " << frame << " using key " << 0 << std::endl;
        hr = frameInfo[frame].keyedMutex->AcquireSync(0, 500);
        if (FAILED(hr) || hr == E_FAIL || hr == WAIT_ABANDONED || hr == WAIT_TIMEOUT) {
            std::cerr << "RenderThread: could not lock buffer for frame " << frame << std::endl;
            switch (hr) {
            case E_FAIL:
                std::cerr << "RenderThread: error == E_FAIL" << std::endl;
                break;
            case WAIT_ABANDONED:
                std::cerr << "RenderThread: error == WAIT_ABANDONED" << std::endl;
                break;
            case WAIT_TIMEOUT:
                std::cerr << "RenderThread: error == WAIT_TIMEOUT" << std::endl;
                break;
            default:
                std::cerr << "RenderThread: error == (unknown error type: " << hr << ")" << std::endl;
                break;
            }
            osvrDestroyRenderManager(render);
            return 201;
        }
        //std::cout << "RenderThread: buffer for frame " << frame << " locked." << std::endl;

        myContext->OMSetDepthStencilState(depthStencilState, 1);

        // Make a grey background
        FLOAT colorRgba[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
        myContext->ClearRenderTargetView(frameInfo[frame].renderBuffer.colorBufferView, colorRgba);
        myContext->ClearDepthStencilView(
            frameInfo[frame].depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // Render into each buffer using the specified information.
        for (size_t i = 0; i < renderInfo.size(); i++) {
            RenderView(i, renderInfo[i], frameInfo[frame].renderBuffer.colorBufferView,
                frameInfo[frame].depthStencilView, myContext, myDevice);
        }

        // Delay the requested length of time to simulate a long render time.
        // Busy-wait so we don't get swapped out longer than we wanted.
        if (delayMilliSeconds > 0) {
            auto end =
                std::chrono::high_resolution_clock::now() +
                std::chrono::milliseconds(delayMilliSeconds);
            do {
            } while (std::chrono::high_resolution_clock::now() < end);
        }

        // Grab and lock the mutex, so that we will be able to render
        // to it whether or not RenderManager locks it on our behalf.
        // it will not be auto-locked when we're in the non-ATW case.
        //std::cout << "RenderThread: Unlocking buffer for frame " << frame << " using key " << 1 << std::endl;
        hr = frameInfo[frame].keyedMutex->ReleaseSync(0);
        if (FAILED(hr)) {
            std::cerr << "RenderThread: could not unlock buffer for frame " << frame << std::endl;
            osvrDestroyRenderManager(render);
            return 201;
        }
        //std::cout << "RenderThread: Buffer for frame " << frame << " unlocked." << std::endl;

        // Send the rendered results to the screen
        OSVR_RenderManagerPresentState presentState;
        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerStartPresentRenderBuffers(
            &presentState))) {
            std::cerr << "Could not start presenting render buffers" << std::endl;
            osvrDestroyRenderManager(render);
            return 201;
        }
        for (size_t i = 0; i < numRenderInfo; i++) {
            if ((OSVR_RETURN_SUCCESS != osvrRenderManagerPresentRenderBufferD3D11(
                presentState, frameInfo[frame].renderBuffer, renderInfo[i], NVCPs[i]))) {
                std::cerr << "Could not present render buffer " << i << std::endl;
                osvrDestroyRenderManager(render);
                return 202;
            }
        }
        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerFinishPresentRenderBuffers(
            render, presentState, renderParams, false))) {
            std::cerr << "Could not finish presenting render buffers" << std::endl;
            osvrDestroyRenderManager(render);
            return 203;
        }
        iteration++;
    }

    // Close the Renderer interface cleanly.
    delete render;

    // Clean up after ourselves.
    myContext->Release();
    myDevice->Release();

    return 0;
}
