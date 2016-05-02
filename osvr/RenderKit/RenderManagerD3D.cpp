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

#include "RenderManagerD3D.h"
#include "GraphicsLibraryD3D11.h"
#include <iostream>
#include "SDL_syswm.h"
#include <d3d11.h>
#include <DirectXMath.h>

namespace osvr {
namespace renderkit {

    RenderManagerD3D11::RenderManagerD3D11(
        OSVR_ClientContext context,
        ConstructorParameters p)
        : RenderManagerD3D11Base(context, p) {}

    RenderManagerD3D11::~RenderManagerD3D11() {
        for (size_t i = 0; i < m_displays.size(); i++) {
            if (m_displays[i].m_window != nullptr) {
                SDL_DestroyWindow(m_displays[i].m_window);
            }
        }
        if (m_displayOpen) {
            /// @todo Clean up anything else we need to
            m_displayOpen = false;
        }
        SDL_Quit();
    }

    RenderManager::OpenResults RenderManagerD3D11::OpenDisplay(void) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        HRESULT hr;
        OpenResults ret;
        ret = RenderManagerD3D11Base::OpenDisplay();
        if (ret.status != COMPLETE) {
            return ret;
        }

        auto withFailure = [&] {
            m_doingOkay = false;
            ret.status = FAILURE;
            return ret;
        };

        /// @todo How to handle window resizing?

        //======================================================
        // Use SDL to get us a window.

        // Initialize the SDL video subsystem.
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            m_log->error() << "RenderManagerD3D11::openD3D11Context: Could not "
                         "initialize SDL";
            /// @todo should this be return withFailure() ?
            return ret;
        }

        // Figure out the flags we want
        Uint32 flags = SDL_WINDOW_RESIZABLE;
        if (m_params.m_windowFullScreen) {
            //        flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
            flags |= SDL_WINDOW_BORDERLESS;
        }
        if (true) {
            flags |= SDL_WINDOW_SHOWN;
        } else {
            flags |= SDL_WINDOW_HIDDEN;
        }

        // @todo Pull this calculation out into the base class and
        // store a separate virtual-screen and actual-screen size.
        // If we've rotated the screen by 90 or 270, then the window
        // we ask for on the screen has swapped aspect ratios.
        int heightRotated, widthRotated;
        if ((m_params.m_displayRotation ==
             ConstructorParameters::Display_Rotation::Ninety) ||
            (m_params.m_displayRotation ==
             ConstructorParameters::Display_Rotation::TwoSeventy)) {
            widthRotated = m_displayHeight;
            heightRotated = m_displayWidth;
        } else {
            widthRotated = m_displayWidth;
            heightRotated = m_displayHeight;
        }

        // Obtain DXGI factory from device (since we may have used nullptr for
        // pAdapter earlier)
        auto dxgiFactory = getDXGIFactory();
        if (!dxgiFactory) {
            std::cerr
                << "RenderManagerD3D11::OpenDisplay: Could not get dxgiFactory"
                << std::endl;
            return withFailure();
        }

        // Open our window.
        for (size_t display = 0; display < GetNumDisplays(); display++) {

            // Push another display structure on our list to use
            m_displays.push_back(DisplayInfo());

            // For now, append the display ID to the title.
            /// @todo Make a different title for each window in the config file
            char displayId = '0' + static_cast<char>(display);
            std::string windowTitle = m_params.m_windowTitle + displayId;
            // For now, move the X position of the second display to the
            // right of the entire display for the left one.
            /// @todo Make the config-file entry a vector and read both
            /// from it.
            int windowX = static_cast<int>(m_params.m_windowXPosition +
                                           widthRotated * display);

            m_displays[display].m_window = SDL_CreateWindow(
                windowTitle.c_str(), windowX, m_params.m_windowYPosition,
                widthRotated, heightRotated, flags);
            if (m_displays[display].m_window == nullptr) {
                std::cerr
                    << "RenderManagerD3D11::OpenDisplay: Could not get window "
                    << "for display " << display << std::endl;
                return withFailure();
            }
            SDL_SysWMinfo wmInfo;
            SDL_VERSION(&wmInfo.version);
            SDL_GetWindowWMInfo(m_displays[display].m_window, &wmInfo);
            m_displays[display].m_rawWindow = wmInfo.info.win.window;

            //======================================================
            // Find out the size of the window to create.
            RECT rectangle;
            GetClientRect(m_displays[display].m_rawWindow, &rectangle);
            UINT width = rectangle.right - rectangle.left;
            UINT height = rectangle.bottom - rectangle.top;

            //======================================================
            // Create the color buffers to be used to render into.

            DXGI_SWAP_CHAIN_DESC swapChainDescription = {};
            swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDescription.OutputWindow = m_displays[display].m_rawWindow;
            /// @todo Do we need to change this for full-screen?
            /// See
            /// https://msdn.microsoft.com/en-us/library/windows/desktop/bb173075%28v=vs.85%29.aspx
            /// Maybe not if we're just doing borderless
            swapChainDescription.Windowed = TRUE;
            swapChainDescription.BufferCount = m_params.m_numBuffers;
            swapChainDescription.BufferDesc.Width = width;
            swapChainDescription.BufferDesc.Height = height;
            swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapChainDescription.SampleDesc.Count = 1;
            swapChainDescription.SampleDesc.Quality = 0;
            hr = dxgiFactory->CreateSwapChain(m_D3D11device,
                                              &swapChainDescription,
                                              &m_displays[display].m_swapChain);
            if (FAILED(hr)) {
                m_log->error() << "RenderManagerD3D11::OpenDisplay: Could not get "
                             "swapChain for display ";
                return withFailure();
            }

            //==================================================================
            // Get the render target view and depth/stencil view and then set
            // them as the render targets.

            // Create render target views and render targets
            hr = m_displays[display].m_swapChain->GetBuffer(
                0, __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(&m_displays[display].m_renderTarget));
            if (FAILED(hr)) {
                m_log->error() << "RenderManagerD3D11::OpenDisplay: Could not get "
                             "color buffer for display ";
                /// @todo this was missing a m_doingOkay = false line - bug or
                /// intentional?
                return withFailure();
            }

            // Create a render target view for each buffer, so we can swap
            // through them
            hr = m_D3D11device->CreateRenderTargetView(
                m_displays[display].m_renderTarget, nullptr,
                &m_displays[display].m_renderTargetView);
            if (FAILED(hr)) {
              m_log->error() << "RenderManagerD3D11::OpenDisplay: Could not get "
                             "render target view for display ";
                /// @todo this was missing a m_doingOkay = false line - bug or
                /// intentional?
                return withFailure();
            }

            m_D3D11Context->OMSetRenderTargets(
                1, &m_displays[display].m_renderTargetView, nullptr);
        }

        //======================================================
        // Fill in our library with the things the application may need to
        // use to do its graphics state set-up.
        m_library.D3D11->device = m_D3D11device;
        m_library.D3D11->context = m_D3D11Context;

        //======================================================
        // Done, we now have an open window to use.
        m_displayOpen = true;
        ret.library = m_library;
        return ret;
    }

    bool RenderManagerD3D11::PresentDisplayInitialize(size_t display) {
        if (display >= GetNumDisplays()) {
            return false;
        }

        // We want to render to the on-screen display now.  The user will have
        // switched this to their views.
        // Do not need to clear depth/stencil for final display because we
        // set the mode to always overwrite.
        // @todo Put back the original one in PresentDisplayFinalize.
        m_D3D11Context->OMSetRenderTargets(
            1, &m_displays[display].m_renderTargetView, nullptr);
        m_D3D11Context->OMSetDepthStencilState(m_depthStencilStateForPresent,
                                               1);

        // @todo Turn off backface culling in case user has switched the
        // front/back which will keep our quads from being rendered.

        // @todo Save and restore (in Finalize) the current rendering state
        // so we don't clobber end-user settings here.

        return true;
    }

    bool RenderManagerD3D11::PresentDisplayFinalize(size_t display) {
        if (display >= GetNumDisplays()) {
            return false;
        }

        // Forcefully sync device rendering to the shared surface.
        m_D3D11Context->Flush();

        // Present the just-rendered surface, waiting for vertical
        // blank if asked to.
        UINT vblanks = 0;
        if (m_params.m_verticalSync) {
            vblanks = 1;
        }
        m_displays[display].m_swapChain->Present(vblanks, 0);
        return true;
    }

    bool RenderManagerD3D11::PresentFrameFinalize() {
        // Let SDL handle any system events that it needs to.
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            // If SDL has been given a quit event, what should we do?
            // We return false to let the app know that something went wrong.
            if (e.window.event == SDL_QUIT) {
                return false;
            } else if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                return false;
            }
        }

        return true;
    }

} // namespace renderkit
} // namespace osvr
