/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and D3D to render a scene with low latency.

    @date 2015

    @author
    Russ Taylor working through ReliaSolve.com for Sensics, Inc.
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

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// This must come after we include <d3d11.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryD3D11.h>

// Includes from our own directory
#include "pixelshader.h"
#include "vertexshader.h"

// Static global variables we use for rendering.
static Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
static Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;

// Set to true when it is time for the application to quit.
// Handlers below that set it to true when the user causes
// any of a variety of events so that we shut down the system
// cleanly.  This only works on Windows, but so does D3D...
static bool quit = false;

// Vertex buffer to use to render triangle.
struct XMFLOAT3 {
    float x;
    float y;
    float z;
};
struct SimpleVertex {
    XMFLOAT3 Pos;
};
ID3D11Buffer* g_vertexBuffer = nullptr;
ID3D11DepthStencilState* g_depthStencilState = nullptr;

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

bool SetupRendering(osvr::renderkit::GraphicsLibrary library) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr << "SetupRendering: No D3D11 GraphicsLibrary, this should "
                     "not happen"
                  << std::endl;
        return false;
    }

    ID3D11Device* device = library.D3D11->device;
    ID3D11DeviceContext* context = library.D3D11->context;

    // Setup vertex shader
    auto hr = device->CreateVertexShader(g_triangle_vs, sizeof(g_triangle_vs),
                                         nullptr, vertexShader.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // Setup pixel shader
    hr = device->CreatePixelShader(g_triangle_ps, sizeof(g_triangle_ps),
                                   nullptr, pixelShader.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // Set the input layout
    ID3D11InputLayout* vertexLayout;
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = device->CreateInputLayout(layout, _countof(layout), g_triangle_vs,
                                   sizeof(g_triangle_vs), &vertexLayout);
    if (SUCCEEDED(hr)) {
        context->IASetInputLayout(vertexLayout);
        vertexLayout->Release();
        vertexLayout = nullptr;
    }

    // Create vertex buffer
    SimpleVertex vertices[3];
    vertices[0].Pos.x = 0.0f;
    vertices[0].Pos.y = 0.5f;
    vertices[0].Pos.z = 0.5f;
    vertices[1].Pos.x = 0.5f;
    vertices[1].Pos.y = -0.5f;
    vertices[1].Pos.z = 0.5f;
    vertices[2].Pos.x = -0.5f;
    vertices[2].Pos.y = -0.5f;
    vertices[2].Pos.z = 0.5f;
    CD3D11_BUFFER_DESC bufferDesc(sizeof(SimpleVertex) * _countof(vertices),
                                  D3D11_BIND_VERTEX_BUFFER);
    D3D11_SUBRESOURCE_DATA subResData = {vertices, 0, 0};
    hr = device->CreateBuffer(&bufferDesc, &subResData, &g_vertexBuffer);

    // Describe how depth and stencil tests should be performed.
    // In particular, that they should not be for this 2D example
    // where we want to render a triangle no matter what.
    D3D11_DEPTH_STENCIL_DESC depthStencilDescription = {};
    depthStencilDescription.DepthEnable = false;
    depthStencilDescription.StencilEnable = false;

    // Create depth stencil state and set it.
    hr = device->CreateDepthStencilState(&depthStencilDescription,
                                         &g_depthStencilState);
    if (FAILED(hr)) {
        std::cerr << "SetupRendering: Could not create depth/stencil state"
                  << std::endl;
        return false;
    }

    return true;
}

// Callback to set up for rendering into a given display; clear the display.
void SetupDisplay(
    void* userData //< Passed into SetViewProjectionCallback
    ,
    osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    ,
    osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.D3D11 == nullptr) {
        std::cerr
            << "SetupDisplay: No D3D11 GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.D3D11 == nullptr) {
        std::cerr
            << "SetupDisplay: No D3D11 RenderBuffer, this should not happen"
            << std::endl;
        return;
    }

    ID3D11DeviceContext* context = library.D3D11->context;
    ID3D11RenderTargetView* renderTargetView = buffers.D3D11->colorBufferView;

    // Perform a random colorfill
    FLOAT red = static_cast<FLOAT>((double)rand() / (double)RAND_MAX);
    FLOAT green = static_cast<FLOAT>((double)rand() / (double)RAND_MAX);
    FLOAT blue = static_cast<FLOAT>((double)rand() / (double)RAND_MAX);
    FLOAT colorRgba[4] = {0.3f * red, 0.3f * green, 0.3f * blue, 1.0f};
    context->ClearRenderTargetView(renderTargetView, colorRgba);
}

// Callbacks to draw things in world space, left-hand space, and right-hand
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

    ID3D11DeviceContext* context = library.D3D11->context;
    ID3D11RenderTargetView* renderTargetView = buffers.D3D11->colorBufferView;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &g_vertexBuffer, &stride, &offset);

    // Set primitive topology
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set depth/stencil state
    context->OMSetDepthStencilState(g_depthStencilState, 1);

    // Draw a triangle using the simple shaders
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
    context->Draw(3, 0);
}

int main(int argc, char* argv[]) {
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

    // Set callback to handle setting up rendering in an eye
    render->SetDisplayCallback(SetupDisplay);

    // Register callbacks to render things in left hand, right
    // hand, and world space.
    render->AddRenderCallback("/", DrawWorld);

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

    // Set up the rendering state we need.
    if (!SetupRendering(ret.library)) {
        return 4;
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

    g_vertexBuffer->Release();

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
