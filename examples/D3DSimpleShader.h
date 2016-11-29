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

#include <DirectXMath.h>
#include <wrl\client.h>

class SimpleShader {
  public:
    SimpleShader() {}

    void init(ID3D11Device* device, ID3D11DeviceContext* context) {
        if (!initialized) {
            // Setup vertex shader
            auto hr = device->CreateVertexShader(g_vs, sizeof(g_vs), nullptr,
                                                 vertexShader.GetAddressOf());
            if (FAILED(hr)) {
                throw std::runtime_error("Could not create vertex shader.");
            }

            // Setup pixel shader
            hr = device->CreatePixelShader(g_ps, sizeof(g_ps), nullptr,
                                           pixelShader.GetAddressOf());
            if (FAILED(hr)) {
                throw std::runtime_error("Could not create pixel shader.");
            }

            // Setup constant buffer (used for passing projection/view/world
            // matrices to the vertex shader)
            D3D11_BUFFER_DESC constantBufferDesc = {};
            constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            constantBufferDesc.ByteWidth = sizeof(cbPerObject);
            constantBufferDesc.CPUAccessFlags = 0;
            constantBufferDesc.MiscFlags = 0;
            constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;

            if (FAILED(
                    device->CreateBuffer(&constantBufferDesc, nullptr,
                                         cbPerObjectBuffer.GetAddressOf()))) {
                throw std::runtime_error(
                    "couldn't create the cbPerObject constant buffer.");
            }
            initialized = true;
        }
    }

    void use(ID3D11Device* device, ID3D11DeviceContext* context,
             const DirectX::XMMATRIX& projection, const DirectX::XMMATRIX& view,
             const DirectX::XMMATRIX& world) {
        cbPerObject wvp = {projection, view, world};

        init(device, context);
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        context->UpdateSubresource(cbPerObjectBuffer.Get(), 0, nullptr, &wvp, 0,
                                   0);
        context->VSSetConstantBuffers(0, 1, cbPerObjectBuffer.GetAddressOf());
    }

  private:
    SimpleShader(const SimpleShader&) = delete;
    SimpleShader& operator=(const SimpleShader&) = delete;
    struct cbPerObject {
        DirectX::XMMATRIX projection;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX world;
    };

    bool initialized = false;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cbPerObjectBuffer;
};
