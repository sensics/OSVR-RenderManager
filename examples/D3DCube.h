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

class Cube {
  public:
    Cube(float scale, bool reverse = false) {
        // This is a left-handed world, so we need to invert the Z
        // values with respect to the right-handed coordinates used
        // in the OpenGL sample program
        vertices = {// VERTEX                   COLOR
                    {{scale, scale, -scale}, {1.0, 0.0, 0.0, 1.0f}}, // +X
                    {{scale, -scale, scale}, {1.0, 0.0, 0.0, 1.0f}},
                    {{scale, scale, scale}, {1.0, 0.0, 0.0, 1.0f}},

                    {{scale, -scale, scale}, {1.0, 0.0, 0.0, 1.0f}},
                    {{scale, scale, -scale}, {1.0, 0.0, 0.0, 1.0f}},
                    {{scale, -scale, -scale}, {1.0, 0.0, 0.0, 1.0f}},

                    {{-scale, -scale, scale}, {1.0, 0.0, 1.0, 1.0f}}, // -X
                    {{-scale, -scale, -scale}, {1.0, 0.0, 1.0, 1.0f}},
                    {{-scale, scale, -scale}, {1.0, 0.0, 1.0, 1.0f}},

                    {{-scale, -scale, scale}, {1.0, 0.0, 1.0, 1.0f}},
                    {{-scale, scale, -scale}, {1.0, 0.0, 1.0, 1.0f}},
                    {{-scale, scale, scale}, {1.0, 0.0, 1.0, 1.0f}},

                    {{scale, scale, -scale}, {0.0, 1.0, 0.0, 1.0f}}, // +Y
                    {{scale, scale, scale}, {0.0, 1.0, 0.0, 1.0f}},
                    {{-scale, scale, scale}, {0.0, 1.0, 0.0, 1.0f}},

                    {{scale, scale, -scale}, {0.0, 1.0, 0.0, 1.0f}},
                    {{-scale, scale, scale}, {0.0, 1.0, 0.0, 1.0f}},
                    {{-scale, scale, -scale}, {0.0, 1.0, 0.0, 1.0f}},

                    {{scale, -scale, -scale}, {1.0, 1.0, 0.0, 1.0f}}, // -Y
                    {{-scale, -scale, scale}, {1.0, 1.0, 0.0, 1.0f}},
                    {{scale, -scale, scale}, {1.0, 1.0, 0.0, 1.0f}},

                    {{scale, -scale, -scale}, {1.0, 1.0, 0.0, 1.0f}},
                    {{-scale, -scale, -scale}, {1.0, 1.0, 0.0, 1.0f}},
                    {{-scale, -scale, scale}, {1.0, 1.0, 0.0, 1.0f}},

                    {{-scale, scale, -scale}, {0.0, 0.0, 1., 1.0f}}, // -Z
                    {{-scale, -scale, -scale}, {0.0, 0.0, 1., 1.0f}},
                    {{scale, -scale, -scale}, {0.0, 0.0, 1., 1.0f}},

                    {{scale, scale, -scale}, {0.0, 0.0, 1., 1.0f}},
                    {{-scale, scale, -scale}, {0.0, 0.0, 1., 1.0f}},
                    {{scale, -scale, -scale}, {0.0, 0.0, 1., 1.0f}},

                    {{scale, scale, scale}, {0.0, 1.0, 1.0, 1.0f}}, // +Z
                    {{-scale, -scale, scale}, {0.0, 1.0, 1.0, 1.0f}},
                    {{-scale, scale, scale}, {0.0, 1.0, 1.0, 1.0f}},

                    {{scale, scale, scale}, {0.0, 1.0, 1.0, 1.0f}},
                    {{scale, -scale, scale}, {0.0, 1.0, 1.0, 1.0f}},
                    {{-scale, -scale, scale}, {0.0, 1.0, 1.0, 1.0f}}};
        // This is a left-handed world, so we need to invert the
        // indices with respect to the OpenGL example so that our
        // front-facing side remains the same.
        indices = {47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36,

                   35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,

                   23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12,

                   11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};

        // if you want to render the inside of the box with backface culling
        // turned on
        // then pass true for reverse.
        if (reverse) {
            std::reverse(std::begin(indices), std::end(indices));
        }
    }

    void init(ID3D11Device* device, ID3D11DeviceContext* context) {
        if (!initialized) {
            // Create the index buffer
            CD3D11_BUFFER_DESC indexBufferDesc(
                static_cast<UINT>(sizeof(WORD) * indices.size()),
                D3D11_BIND_INDEX_BUFFER);
            D3D11_SUBRESOURCE_DATA indexResData = {&indices[0], 0, 0};
            auto hr = device->CreateBuffer(&indexBufferDesc, &indexResData,
                                           indexBuffer.GetAddressOf());
            if (FAILED(hr)) {
                throw std::runtime_error("Couldn't create index buffer");
            }
            context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT,
                                      0);

            // Create the vertex buffer
            CD3D11_BUFFER_DESC vertexBufferDesc(
                static_cast<UINT>(sizeof(SimpleVertex) * vertices.size()),
                D3D11_BIND_VERTEX_BUFFER);
            D3D11_SUBRESOURCE_DATA subResData = {&vertices[0], 0, 0};
            hr = device->CreateBuffer(&vertexBufferDesc, &subResData,
                                      vertexBuffer.GetAddressOf());
            if (FAILED(hr)) {
                throw std::runtime_error("Couldn't create vertex buffer");
            }

            UINT stride = sizeof(SimpleVertex);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(),
                                        &stride, &offset);

            // Create the input layout
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                 offsetof(SimpleVertex, Position), D3D11_INPUT_PER_VERTEX_DATA,
                 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                 offsetof(SimpleVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
            };
            hr = device->CreateInputLayout(layout, _countof(layout), g_vs,
                                           sizeof(g_vs),
                                           vertexLayout.GetAddressOf());
            if (FAILED(hr)) {
                throw std::runtime_error("Could not create input layout.");
            }
            context->IASetInputLayout(vertexLayout.Get());
            context->IASetPrimitiveTopology(
                D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            initialized = true;
        }
    }

    void draw(ID3D11Device* device, ID3D11DeviceContext* context) {
        UINT stride = sizeof(SimpleVertex);
        UINT offset = 0;

        init(device, context);
        context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride,
                                    &offset);
        context->IASetInputLayout(vertexLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);
    }

  private:
    Cube(const Cube&) = delete;
    Cube& operator=(const Cube&) = delete;

    struct SimpleVertex {
        XMFLOAT3 Position;
        XMFLOAT4 Color;
    };
    bool initialized = false;
    std::vector<SimpleVertex> vertices;
    std::vector<WORD> indices;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> vertexLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
};
