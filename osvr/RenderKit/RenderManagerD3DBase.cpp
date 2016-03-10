/** @file
@brief Source file implementing nVidia-based OSVR direct-to-device rendering
interface

@date 2015

@author
Sensics, Inc.
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

#include "ComputeATW.h"
#include "RenderManagerD3DBase.h"
#include "GraphicsLibraryD3D11.h"
#include <boost/assert.hpp>
#include <iostream>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <Eigen/Core>
#include <Eigen/Geometry>

static const char* distortionVertexShader =
    "cbuffer cbPerObject"
    "{"
    "  matrix projectionMatrix;"
    "  matrix modelViewMatrix;"
    "  matrix textureMatrix;"
    "}"
    ""
    "struct VS_Input"
    "{"
    "  float4 position : POSITION;"
    "  float2 texR : TEXCOORDR;"
    "  float2 texG : TEXCOORDG;"
    "  float2 texB : TEXCOORDB;"
    "};"
    ""
    "struct VS_Output"
    "{"
    "  float4 position : SV_POSITION;"
    "  float2 texR : TEXCOORDR;"
    "  float2 texG : TEXCOORDG;"
    "  float2 texB : TEXCOORDB;"
    "};"
    ""
    "VS_Output triangle_vs(VS_Input input)"
    "{"
    "  /* still pass through, no projection/view. */"
    "  VS_Output ret;"
    "  matrix wvp = mul(projectionMatrix, modelViewMatrix);"
    "  ret.position = mul(wvp, input.position);"
    "  /* Adjust the texture coordinates using the texture matrix */"
    "  ret.texR = mul(float4(input.texR, 0.0f, 1.0f), textureMatrix);"
    "  ret.texG = mul(float4(input.texG, 0.0f, 1.0f), textureMatrix);"
    "  ret.texB = mul(float4(input.texB, 0.0f, 1.0f), textureMatrix);"
    "  return ret;"
    "}";

static const char* distortionPixelShader =
    "Texture2D shaderTexture;"
    "SamplerState sampleState;"
    ""
    "struct PS_Input"
    "{"
    "  float4 position : SV_POSITION;"
    "  float2 texR : TEXCOORDR;"
    "  float2 texG : TEXCOORDG;"
    "  float2 texB : TEXCOORDB;"
    "};"
    ""
    "float4 triangle_ps(PS_Input input) : SV_Target"
    "{"
    "  /* @todo Look up the distortion correction */"
    "  float4 outColor;"
    "  outColor.r = shaderTexture.Sample(sampleState, input.texR).r;"
    "  outColor.g = shaderTexture.Sample(sampleState, input.texG).g;"
    "  outColor.b = shaderTexture.Sample(sampleState, input.texB).b;"
    "  outColor.a = 1.0f;"
    "  return outColor;"
    "}";

static std::string StringFromD3DError(HRESULT hr) {
    switch (hr) {
    case D3D11_ERROR_FILE_NOT_FOUND:
        return "D3D11_ERROR_FILE_NOT_FOUND";
    case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS:
        return "D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS";
    case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS:
        return "D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS";
    case D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD:
        return "D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD";
    case DXGI_ERROR_INVALID_CALL:
        return "DXGI_ERROR_INVALID_CALL";
    case DXGI_ERROR_WAS_STILL_DRAWING:
        return "DXGI_ERROR_WAS_STILL_DRAWING";
    case E_FAIL:
        return "E_FAIL: Attempted to create a device with the debug layer "
               "enabled and the layer is not installed";
    case E_INVALIDARG:
        return "E_INVALIDARG";
    case E_OUTOFMEMORY:
        return "E_OUTOFMEMORY";
    case E_NOTIMPL:
        return "E_NOTIMPL: The method call isn't implemented with the passed "
               "parameter combination";
    case S_FALSE:
        return "S_FALSE: Alternate success value, indicating a successful but "
               "nonstandard completion (the precise meaning depends on "
               "context)";
    case S_OK:
        return "S_OK: No error occurred";
    default:
        return "Unrecognized return code";
    }
}

namespace osvr {
namespace renderkit {

    RenderManagerD3D11Base::RenderManagerD3D11Base(
        OSVR_ClientContext context,
        ConstructorParameters p)
        : RenderManager(context, p) {
        // Initialize all of the variables that don't have to be done in the
        // list above, so we don't get warnings about out-of-order
        // initialization if they are re-ordered in the header file.
        m_doingOkay = true;
        m_displayOpen = false;
        m_D3D11device = nullptr;
        m_D3D11Context = nullptr;

        // Construct the appropriate GraphicsLibrary pointer.
        m_library.D3D11 = new GraphicsLibraryD3D11;
        m_buffers.D3D11 = new RenderBufferD3D11;
        m_depthStencilStateForRender = nullptr;

        //======================================================
        // Create the D3D11 context that is used to draw things into the window
        // unless these have already been filled in
        HRESULT hr;
        if (m_params.m_graphicsLibrary.D3D11 != nullptr) {
          m_D3D11device = m_params.m_graphicsLibrary.D3D11->device;
          m_D3D11Context = m_params.m_graphicsLibrary.D3D11->context;
        } else {
          D3D_FEATURE_LEVEL acceptibleAPI = D3D_FEATURE_LEVEL_11_0;
          D3D_FEATURE_LEVEL foundAPI;

          UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
          // If we pass a non-null adapter (like if we're being used by NVIDIA
          // direct mode), then we need to pass driver type unknown.
          auto driverType = nullptr == m_adapter ? D3D_DRIVER_TYPE_HARDWARE
            : D3D_DRIVER_TYPE_UNKNOWN;
          hr = D3D11CreateDevice(m_adapter.Get(), driverType, nullptr,
            createDeviceFlags, &acceptibleAPI, 1,
            D3D11_SDK_VERSION, &m_D3D11device, &foundAPI,
            &m_D3D11Context);
          if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::RenderManagerD3D11Base: Could not "
              "create D3D11 device"
              << std::endl;
            m_doingOkay = false;
          }
        }
    }

    RenderManagerD3D11Base::~RenderManagerD3D11Base() {
        // Release any prior buffers we allocated
        for (size_t i = 0; i < m_quadVertexBuffer.size(); i++) {
            m_quadVertexBuffer[i]->Release();
        }
        for (size_t i = 0; i < m_triangleBuffer.size(); i++) {
            delete[] m_triangleBuffer[i];
        }

        for (size_t i = 0; i < m_renderBuffers.size(); i++) {
            m_renderBuffers[i].D3D11->colorBuffer->Release();
            m_renderBuffers[i].D3D11->colorBufferView->Release();
            m_renderBuffers[i].D3D11->depthStencilBuffer->Release();
            m_renderBuffers[i].D3D11->depthStencilView->Release();
            delete m_renderBuffers[i].D3D11;
        }
        if (m_depthStencilStateForRender != nullptr) {
          m_depthStencilStateForRender->Release();
        }

        delete m_buffers.D3D11;
        delete m_library.D3D11;
    }

    bool RenderManagerD3D11Base::constructRenderBuffers() {
        HRESULT hr;
        for (size_t i = 0; i < GetNumEyes(); i++) {

            OSVR_ViewportDescription v;
            ConstructViewportForRender(i, v);
            unsigned width = static_cast<unsigned>(v.width);
            unsigned height = static_cast<unsigned>(v.height);

            // The color buffer for this eye.  We need to put this into
            // a generic structure for the Present function, but we only need
            // to fill in the Direct3D portion.
            //  Note that this texture format must be RGBA and unsigned byte,
            // so that we can present it to Direct3D for DirectMode.
            ID3D11Texture2D* D3DTexture = nullptr;

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
            textureDesc.MiscFlags = 0;

            // Create a new render target texture to use.
            hr =
                m_D3D11device->CreateTexture2D(&textureDesc, NULL, &D3DTexture);
            if (FAILED(hr)) {
                std::cerr << "RenderManagerD3D11Base::constructRenderBuffers: "
                             "Can't create texture for eye "
                          << i << std::endl;
                std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                          << std::endl;
                return false;
            }

            // Fill in the resource view for your render texture buffer here
            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
            // This must match what was created in the texture to be rendered
            renderTargetViewDesc.Format = textureDesc.Format;
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            renderTargetViewDesc.Texture2D.MipSlice = 0;

            // Create the render target view.
            ID3D11RenderTargetView* renderTargetView = nullptr;
            hr = m_D3D11device->CreateRenderTargetView(
                D3DTexture, &renderTargetViewDesc, &renderTargetView);
            if (FAILED(hr)) {
                std::cerr << "RenderManagerD3D11Base::constructRenderBuffers: "
                             "Could not create render target for eye "
                          << i << std::endl;
                std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                          << std::endl;
                return false;
            }

            // Push the filled-in RenderBuffer onto the vector.
            osvr::renderkit::RenderBufferD3D11* rbD3D =
                new osvr::renderkit::RenderBufferD3D11;
            rbD3D->colorBuffer = D3DTexture;
            rbD3D->colorBufferView = renderTargetView;
            osvr::renderkit::RenderBuffer rb;
            rb.D3D11 = rbD3D;
            m_renderBuffers.push_back(rb);

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
            hr = m_D3D11device->CreateTexture2D(&textureDescription, NULL,
                                                &depthStencilBuffer);
            if (FAILED(hr)) {
                std::cerr << "RenderManagerD3D11Base::constructRenderBuffers: "
                             "Could not create depth/stencil texture for eye "
                          << i << std::endl;
                std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                          << std::endl;
                return false;
            }
            m_renderBuffers[i].D3D11->depthStencilBuffer = depthStencilBuffer;

            // Create the depth/stencil view description
            D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription;
            memset(&depthStencilViewDescription, 0,
                   sizeof(depthStencilViewDescription));
            depthStencilViewDescription.Format = textureDescription.Format;
            depthStencilViewDescription.ViewDimension =
                D3D11_DSV_DIMENSION_TEXTURE2D;
            depthStencilViewDescription.Texture2D.MipSlice = 0;

            ID3D11DepthStencilView* depthStencilView;
            hr = m_D3D11device->CreateDepthStencilView(
                depthStencilBuffer, &depthStencilViewDescription,
                &depthStencilView);
            if (FAILED(hr)) {
                std::cerr << "RenderManagerD3D11Base::constructRenderBuffers: "
                             "Could not create depth/stencil view for eye "
                          << i << std::endl;
                std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                          << std::endl;
                return false;
            }
            m_renderBuffers[i].D3D11->depthStencilView = depthStencilView;
        }

        // Create depth stencil state for the render path.
        // Describe how depth and stencil tests should be performed.
        D3D11_DEPTH_STENCIL_DESC depthStencilDescription = {};

        depthStencilDescription.DepthEnable = true;
        depthStencilDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthStencilDescription.DepthFunc = D3D11_COMPARISON_LESS;

        depthStencilDescription.StencilEnable = true;
        depthStencilDescription.StencilReadMask = 0xFF;
        depthStencilDescription.StencilWriteMask = 0xFF;

        // Front-facing stencil operations (draw front faces)
        depthStencilDescription.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depthStencilDescription.FrontFace.StencilDepthFailOp =
            D3D11_STENCIL_OP_INCR;
        depthStencilDescription.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthStencilDescription.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        // Back-facing stencil operations (cull back faces)
        depthStencilDescription.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depthStencilDescription.BackFace.StencilDepthFailOp =
            D3D11_STENCIL_OP_DECR;
        depthStencilDescription.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthStencilDescription.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;

        hr = m_D3D11device->CreateDepthStencilState(
            &depthStencilDescription, &m_depthStencilStateForRender);
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::constructRenderBuffers: "
                         "Could not create depth/stencil state"
                      << std::endl;
            std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                      << std::endl;
            return false;
        }

        // Register the render buffers we're going to use to present
        return RegisterRenderBuffersInternal(m_renderBuffers);

        // Store the info about the buffers for the render callbacks.
        // Start with the 0th eye.
        m_buffers.D3D11->colorBuffer = m_renderBuffers[0].D3D11->colorBuffer;
        m_buffers.D3D11->colorBufferView =
            m_renderBuffers[0].D3D11->colorBufferView;
        m_buffers.D3D11->depthStencilBuffer =
            m_renderBuffers[0].D3D11->depthStencilBuffer;
        m_buffers.D3D11->depthStencilView =
            m_renderBuffers[0].D3D11->depthStencilView;
    }

    RenderManager::OpenResults RenderManagerD3D11Base::OpenDisplay() {
        HRESULT hr;
      
        OpenResults ret;
        ret.library = m_library;
        ret.buffers = m_buffers;
        ret.status = COMPLETE; // Until we hear otherwise
        if (!doingOkay()) {
            ret.status = FAILURE;
            return ret;
        }

        //==================================================================
        // Create the vertex buffer we're going to use to render quads in
        // the Present mode and also set up the vertex and shader programs
        // we're going to use to display them.
        ID3D10Blob* compiledShader = nullptr;
        ID3D10Blob* compilerMsgs = nullptr;
        hr = D3DCompile(
            distortionVertexShader, strlen(distortionVertexShader) + 1,
            "triangle_vs", nullptr, nullptr, "triangle_vs", "vs_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &compiledShader, &compilerMsgs);
        if (FAILED(hr)) {
// this is how you're supposed to get the messages, shush /analyze.
#pragma warning(suppress : 6102)
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Vertex shader "
                         "compilation failed: "
                      << static_cast<char*>(compilerMsgs->GetBufferPointer())
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        hr = m_D3D11device->CreateVertexShader(
            compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(),
            nullptr, m_vertexShader.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "create vertex shader"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        // Set the input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
             offsetof(DistortionVertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
            // @todo: replace DXGI_FORMAT_R32G32_FLOAT with the matching format
            // of the texture buffer
            {"TEXCOORDR", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
             offsetof(DistortionVertex, TexR), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORDG", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
             offsetof(DistortionVertex, TexG), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORDB", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
             offsetof(DistortionVertex, TexB), D3D11_INPUT_PER_VERTEX_DATA, 0}};
        hr = m_D3D11device->CreateInputLayout(
            layout, _countof(layout), compiledShader->GetBufferPointer(),
            compiledShader->GetBufferSize(), &m_vertexLayout);
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "create input layout"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }
        compiledShader->Release();

        // Setup pixel shader
        hr = D3DCompile(
            distortionPixelShader, strlen(distortionPixelShader) + 1,
            "triangle_ps", nullptr, nullptr, "triangle_ps", "ps_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &compiledShader, &compilerMsgs);
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Pixel shader "
                         "compilation failed: "
                      << static_cast<char*>(compilerMsgs->GetBufferPointer())
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        hr = m_D3D11device->CreatePixelShader(
            compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(),
            nullptr, m_pixelShader.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "create pixel shader"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }
        compiledShader->Release();

        // Sampler state
        D3D11_SAMPLER_DESC samplerDescription = {};
        samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDescription.MipLODBias = 0;
        samplerDescription.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDescription.MinLOD = 0;
        samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
        hr = m_D3D11device->CreateSamplerState(&samplerDescription,
                                               &m_renderTextureSamplerState);
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "create sampler state"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        // Rasterizer state; turn off back-face culling for the final
        // presentation buffers.
        D3D11_RASTERIZER_DESC rasDesc = {};
        rasDesc.FillMode = D3D11_FILL_SOLID;
        rasDesc.CullMode = D3D11_CULL_NONE;
        hr = m_D3D11device->CreateRasterizerState(
            &rasDesc, m_rasterizerState.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "create rasterizer state"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        // Uniform buffer to be used to pass matrices to the vertex shader.
        // Setup constant buffer (used for passing projection/view/world
        // matrices to the vertex shader)
        D3D11_BUFFER_DESC constantBufferDesc = {};
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.ByteWidth = sizeof(cbPerObject);
        constantBufferDesc.CPUAccessFlags = 0;
        constantBufferDesc.MiscFlags = 0;
        constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;

        if (FAILED(m_D3D11device->CreateBuffer(
                &constantBufferDesc, nullptr,
                m_cbPerObjectBuffer.GetAddressOf()))) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "create uniform buffer"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        // Create distortion meshes for each of the eyes.
        UpdateDistortionMeshesInternal(SQUARE, m_params.m_distortionParameters);

        //==================================================================
        // Describe how depth and stencil tests should be performed
        // for our scan-out buffer, which is not at all.
        D3D11_DEPTH_STENCIL_DESC depthStencilDescription = {};

        depthStencilDescription.DepthEnable = false;
        depthStencilDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthStencilDescription.DepthFunc = D3D11_COMPARISON_LESS;

        depthStencilDescription.StencilEnable = false;
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
        depthStencilDescription.BackFace.StencilDepthFailOp =
            D3D11_STENCIL_OP_DECR;
        depthStencilDescription.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthStencilDescription.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        // Create depth stencil state and set it.
        hr = m_D3D11device->CreateDepthStencilState(
            &depthStencilDescription, &m_depthStencilStateForPresent);
        if (FAILED(hr)) {
            std::cerr << "RenderManagerNVidiaD3D11::OpenDisplay: Could not "
                         "create depth/stencil state"
                      << std::endl;
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        }

        //======================================================
        // Construct the present buffers we're going to use when in Render()
        // mode, to
        // wrap the PresentMode interface.
        if (!constructRenderBuffers()) {
            std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                         "construct present buffers to wrap Render() path"
                      << std::endl;
            ret.status = FAILURE;
            return ret;
        }

        return ret;
    }

    bool RenderManagerD3D11Base::ComputeAsynchronousTimeWarps(
        std::vector<RenderInfo> usedRenderInfo,
        std::vector<RenderInfo> currentRenderInfo, float assumedDepth) {
        /// @todo Make this and the base-class method share code rather than
        /// repeat

        // Empty out the ATW vector until we fill it again below.
        m_asynchronousTimeWarps.clear();

        size_t numEyes = GetNumEyes();
        if (assumedDepth <= 0) {
            return false;
        }
        if ((currentRenderInfo.size() < numEyes) ||
            (usedRenderInfo.size() < numEyes)) {
            return false;
        }

        for (size_t eye = 0; eye < numEyes; eye++) {
            Eigen::Projective3f full = computeATW(
                usedRenderInfo[eye], currentRenderInfo[eye], assumedDepth);
            Eigen::Matrix4f fullMatTranspose = full.matrix().transpose();

            // Store transpose of the result, because Direct3D stores matrices
            // in
            // the opposite order from OpenGL.
            // @todo Figure out how to handle the transpose or the handedness
            // change
            // in Eigen with a method or declaration.
            /// @todo can we do this with eigen::map instead of memcp directly?
            matrix16 ATW;
            memcpy(ATW.data, fullMatTranspose.data(), sizeof(ATW.data));
            m_asynchronousTimeWarps.push_back(ATW);
        }
        return true;
    }

    bool RenderManagerD3D11Base::RenderEyeInitialize(size_t eye) {
        // Bind our render target view to the appropriate one.
        ID3D11RenderTargetView* pRtv =
            m_renderBuffers[eye].D3D11->colorBufferView;
        ID3D11DepthStencilView* pDsv =
            m_renderBuffers[eye].D3D11->depthStencilView;
        m_D3D11Context->OMSetRenderTargets(1, &pRtv, pDsv);
        m_D3D11Context->OMSetDepthStencilState(m_depthStencilStateForRender, 1);
        m_buffers.D3D11->colorBufferView = pRtv;
        m_buffers.D3D11->colorBuffer = m_renderBuffers[eye].D3D11->colorBuffer;
        m_buffers.D3D11->depthStencilView = pDsv;
        m_buffers.D3D11->depthStencilBuffer =
            m_renderBuffers[eye].D3D11->depthStencilBuffer;

        // Set the viewport for rendering to this eye.
        OSVR_ViewportDescription v;
        ConstructViewportForRender(eye, v);
        CD3D11_VIEWPORT viewport(
            static_cast<float>(v.left), static_cast<float>(v.lower),
            static_cast<float>(v.width), static_cast<float>(v.height));
        m_D3D11Context->RSSetViewports(1, &viewport);

        // Call the display set-up callback for each eye, because they each
        // have their own frame buffer.
        if (m_displayCallback.m_callback != nullptr) {
            m_displayCallback.m_callback(m_displayCallback.m_userData,
                                         m_library, m_buffers);
        }

        return true;
    }

    bool RenderManagerD3D11Base::RenderSpace(size_t whichSpace, size_t whichEye,
                                             OSVR_PoseState pose,
                                             OSVR_ViewportDescription viewport,
                                             OSVR_ProjectionMatrix projection) {
        /// @todo Fill in the timing information
        OSVR_TimeValue deadline;
        deadline.microseconds = 0;
        deadline.seconds = 0;

        /// Fill in the information we pass to the render callback.
        RenderCallbackInfo& cb = m_callbacks[whichSpace];
        cb.m_callback(cb.m_userData, m_library, m_buffers, viewport, pose,
                      projection, deadline);

        /// @todo Keep track of timing information

        return true;
    }

    bool RenderManagerD3D11Base::UpdateDistortionMeshesInternal(
        DistortionMeshType type //< Type of mesh to produce
        ,
        std::vector<DistortionParameters> const&
            distort //< Distortion parameters
        ) {
        // Release any prior buffers we already allocated
        for (size_t i = 0; i < m_quadVertexBuffer.size(); i++) {
            m_quadVertexBuffer[i]->Release();
        }
        for (size_t i = 0; i < m_triangleBuffer.size(); i++) {
            delete[] m_triangleBuffer[i];
        }

        // Clear the triangle and quad buffers
        m_numTriangles.clear();
        m_triangleBuffer.clear();
        m_quadVertexBuffer.clear();
        m_quadVertexCount.clear();

        HRESULT hr;

        // Create distortion meshes for each of the eyes.
        size_t numEyes = m_params.m_displayConfiguration.getEyes().size();
        for (size_t eye = 0; eye < distort.size(); eye++) {
            m_numTriangles.push_back(0);
            m_triangleBuffer.push_back(nullptr);

            // Construct a distortion mesh for this eye using the RenderManager
            // standard, which is an OpenGL-compatible mesh.
            std::vector<RenderManager::DistortionMeshVertex> mesh =
                ComputeDistortionMesh(eye, type, distort[eye]);
            m_numTriangles[eye] = mesh.size() / 3;
            if (m_numTriangles[eye] == 0) {
                std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                             "create mesh "
                          << "for eye " << eye << std::endl;
                return false;
            }

            // Allocate a set of vertices and copy the mesh into them.  Remember
            // to adjust the texture Y coordinate compared to OpenGL: we want
            // texture coordinate 0 at Y spatial coordinate 1 and texture
            // coordinate 1 at Y spatial coordinate -1; this is not a simple
            // inversion but  rather a remapping.
            m_triangleBuffer[eye] =
                new DistortionVertex[m_numTriangles[eye] * 3];
            for (size_t tri = 0; tri < m_numTriangles[eye]; tri++) {
                for (size_t vert = 0; vert < 3; vert++) {
                    DistortionVertex& v = m_triangleBuffer[eye][tri * 3 + vert];
                    v.Pos.x = mesh[3 * tri + vert].m_pos[0];
                    v.Pos.y = mesh[3 * tri + vert].m_pos[1];
                    v.Pos.z = 0; // Z = 0, and vertices in mesh only have 2
                                 // coordinates.

                    v.TexR.x = mesh[3 * tri + vert].m_texRed[0];
                    v.TexR.y =
                        RenderManager::DistortionMeshVertex::flipTexCoord(
                            mesh[3 * tri + vert].m_texRed[1]);

                    v.TexG.x = mesh[3 * tri + vert].m_texGreen[0];
                    v.TexG.y =
                        RenderManager::DistortionMeshVertex::flipTexCoord(
                            mesh[3 * tri + vert].m_texGreen[1]);

                    v.TexB.x = mesh[3 * tri + vert].m_texBlue[0];
                    v.TexB.y =
                        RenderManager::DistortionMeshVertex::flipTexCoord(
                            mesh[3 * tri + vert].m_texBlue[1]);
                }
            }

            ID3D11Buffer* quadVertexBuffer;
            CD3D11_BUFFER_DESC bufferDesc(
                static_cast<UINT>(sizeof(DistortionVertex) *
                                  m_numTriangles[eye] * 3),
                D3D11_BIND_VERTEX_BUFFER);
            D3D11_SUBRESOURCE_DATA subResData = {m_triangleBuffer[eye], 0, 0};
            hr = m_D3D11device->CreateBuffer(&bufferDesc, &subResData,
                                             &quadVertexBuffer);
            if (FAILED(hr)) {
                std::cerr << "RenderManagerD3D11Base::OpenDisplay: Could not "
                             "create vertex buffer"
                          << std::endl;
                std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                          << std::endl;
                return false;
            }
            m_quadVertexCount.push_back(
                static_cast<UINT>(m_numTriangles[eye] * 3));
            m_quadVertexBuffer.push_back(quadVertexBuffer);
        }
        return true;
    }

    void RenderManagerD3D11Base::setAdapter(
        Microsoft::WRL::ComPtr<IDXGIAdapter> const& adapter) {
        BOOST_ASSERT_MSG(!m_displayOpen, "Only sensible to set adapter if the "
                                         "display hasn't been opened yet!");
        m_adapter = adapter;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice>
    RenderManagerD3D11Base::getDXGIDevice() {
        Microsoft::WRL::ComPtr<IDXGIDevice> ret;

        auto hr = m_D3D11device->QueryInterface(
            __uuidof(IDXGIDevice),
            reinterpret_cast<void**>(ret.GetAddressOf()));

        if (FAILED(hr)) {
            ret.Reset();
        }
        return ret;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter>
    RenderManagerD3D11Base::getDXGIAdapter() {
        Microsoft::WRL::ComPtr<IDXGIAdapter> ret;
        if (m_adapter) {
            ret = m_adapter;
        } else {
            auto dev = getDXGIDevice();
            if (dev) {
                dev->GetAdapter(ret.GetAddressOf());
            }
            /// @todo should we cache this?
        }
        return ret;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory1>
    RenderManagerD3D11Base::getDXGIFactory() {
        Microsoft::WRL::ComPtr<IDXGIFactory1> ret;
        auto adapter = getDXGIAdapter();
        if (!adapter) { ///@todo indicate error condition?
            return ret;
        }

        auto hr =
            adapter->GetParent(__uuidof(IDXGIFactory1),
                               reinterpret_cast<void**>(ret.GetAddressOf()));
        if (FAILED(hr)) {
            ret.Reset();
        }
        return ret;
    }

    bool RenderManagerD3D11Base::RenderFrameInitialize() {
        return PresentFrameInitialize();
    }

    bool RenderManagerD3D11Base::RenderDisplayFinalize(size_t display) {
        return PresentDisplayFinalize(display);
    }

    bool RenderManagerD3D11Base::RenderFrameFinalize() {
        return PresentRenderBuffersInternal(
            m_renderBuffers, m_renderInfoForRender, m_renderParamsForRender);
    }

    bool RenderManagerD3D11Base::PresentFrameInitialize() {
        /// @todo Construct our vertex and fragment shaders, one per actual
        /// display.

        return true;
    }

    bool RenderManagerD3D11Base::PresentEye(PresentEyeParameters params) {
        HRESULT hr;

        if (params.m_buffer.D3D11 == nullptr) {
            std::cerr << "RenderManagerD3D11::PresentEye(): NULL buffer pointer"
                      << std::endl;
            return false;
        }

        // @todo Record all state we change and re-set it to what it was
        // originally so
        // we don't mess with client rendering.

        // Get the viewport.  This returns a viewport for OpenGL.  To get one
        // for D3D in the case that we have a display that is rotated by 90 or
        // 270, we need to swap the eyes compared to what we've been asked for.
        bool swapEyes = m_params.m_displayConfiguration.getSwapEyes();
        if (static_cast<int>(params.m_rotateDegrees) % 180 != 0) {
            if (GetNumEyes() % 2 == 0) {
                swapEyes = !swapEyes;
            }
        }

        // Construct the viewport based on which eye this is.
        // Set the D3D11 viewport based on the one we computed.
        // As we have eyes at different Y positions, we need to
        // figure out how to subtract the total screen size to find
        // the correct starting location.
        OSVR_ViewportDescription viewportDesc;
        if (!ConstructViewportForPresent(params.m_index, viewportDesc,
                                         swapEyes)) {
            std::cerr << "RenderManagerD3D11::PresentEye(): Could not "
                         "construct viewport"
                      << std::endl;
            return false;
        }
        // Adjust the viewport based on how much the display window is
        // rotated with respect to the rendering window.
        viewportDesc = RotateViewport(viewportDesc);
        CD3D11_VIEWPORT viewport(static_cast<float>(viewportDesc.left),
                                 static_cast<float>(viewportDesc.lower),
                                 static_cast<float>(viewportDesc.width),
                                 static_cast<float>(viewportDesc.height));
        m_D3D11Context->RSSetViewports(1, &viewport);

        // Set primitive topology
        m_D3D11Context->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_D3D11Context->IASetInputLayout(m_vertexLayout);

        m_D3D11Context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_D3D11Context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        //====================================================================
        // Set up matrices to be used for overfill, flipping, and ATW.

        // Set up a Projection matrix that undoes the scale factor applied
        // due to our rendering overfill factor.  This will put only the part
        // of the geometry that should be visible inside the viewing frustum.
        // @todo think about how we get square pixels, to properly handle
        // distortion correction.
        float myScale = m_params.m_renderOverfillFactor;
        float scaleProj[16] = {myScale, 0, 0, 0, 0, myScale, 0, 0,
                               0,       0, 1, 0, 0, 0,       0, 1};
        DirectX::XMMATRIX projection(scaleProj);

        // Set up a ModelView matrix that handles rotating and flipping the
        // geometry as needed to match the display scan-out circuitry and/or
        // any changes needed by the inversion of window coordinates when
        // switching between graphics systems (OpenGL and Direct3D, for
        // example).
        // @todo think about how we get square pixels, to properly handle
        // distortion correction.
        matrix16 modelViewMat;
        if (!ComputeDisplayOrientationMatrix(
                static_cast<float>(params.m_rotateDegrees), params.m_flipInY,
                modelViewMat)) {
            std::cerr << "RenderManagerD3D11Base::PresentEye(): "
                         "ComputeDisplayOrientationMatrix failed"
                      << std::endl;
            return false;
        }
        DirectX::XMMATRIX modelView(modelViewMat.data);

        // Set up the texture matrix to handle asynchronous time warp.
        // We are able to use the matrix directly because it is using
        // the projection math and transforms associated with OSVR's
        // idea of transformations, which matches OpenGL's.
        // Start with the identity matrix and fill in if needed.
        float textureMat[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (params.m_ATW != nullptr) {
            memcpy(textureMat, params.m_ATW->data, 16 * sizeof(float));
        }

        // We now crop to a subregion of the texture.  This is used to handle
        // the
        // case where more than one eye is drawn into the same render texture
        // (for example, as happens in the Unreal game engine).  We base this on
        // the normalized cropping viewport, which will go from 0 to 1 in both
        // X and Y for the full display, but which will be cut in half in in one
        // dimension for the case where two eyes are packed into the same
        // buffer.
        // We scale and translate the texture coordinates by multiplying the
        // texture matrix to map the original range (0..1) to the proper
        // location.
        // We read in, multiply by the transpose of the crop matrix (going from
        // OpenGL form to Direct3D requires a transpose), and write out
        // textureMat.
        matrix16 crop;
        ComputeRenderBufferCropMatrix(params.m_normalizedCroppingViewport,
                                      crop);
        Eigen::Map<Eigen::Matrix4f> textureEi(textureMat);
        textureEi = textureEi * Eigen::Matrix4f::Map(crop.data).transpose();

        DirectX::XMMATRIX texture(textureMat);
        cbPerObject wvp = {projection, modelView, texture};
        m_D3D11Context->UpdateSubresource(m_cbPerObjectBuffer.Get(), 0, nullptr,
                                          &wvp, 0, 0);
        m_D3D11Context->VSSetConstantBuffers(
            0, 1, m_cbPerObjectBuffer.GetAddressOf());

        //====================================================================
        // Set vertex buffer
        UINT stride = sizeof(DistortionVertex);
        UINT offset = 0;
        m_D3D11Context->IASetVertexBuffers(
            0, 1, &m_quadVertexBuffer[params.m_index], &stride, &offset);

        //====================================================================
        // Create the shader resource view.

        /// @todo Is this going to chew up resources?
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
        shaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* renderTextureResourceView;
        hr = m_D3D11device->CreateShaderResourceView(
            params.m_buffer.D3D11->colorBuffer, &shaderResourceViewDesc,
            &renderTextureResourceView);
        if (FAILED(hr)) {
            std::cerr << "RenderManagerD3D11Base::PresentEye(): Could not "
                         "create resource view"
                      << std::endl;
            std::cerr << "  Direct3D error type: " << StringFromD3DError(hr)
                      << std::endl;
            return false;
        }
        m_D3D11Context->PSSetShaderResources(0, 1, &renderTextureResourceView);

        // @todo Record the state and re-set it to what it was originally so
        // we don't mess with client rendering.
        // Turn off back-face culling, so we render the mesh from either side.
        m_D3D11Context->RSSetState(m_rasterizerState.Get());

        //====================================================================
        // Set the sample to use and then draw the quad with the texture
        // on it.
        m_D3D11Context->PSSetSamplers(0, 1, &m_renderTextureSamplerState);
        m_D3D11Context->Draw(m_quadVertexCount[params.m_index], 0);

        // Clean up after ourselves.
        renderTextureResourceView->Release();
        return true;
    }

} // namespace renderkit
} // namespace osvr
