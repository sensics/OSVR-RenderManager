/** @file
@brief Header file describing the OSVR direct-to-device rendering interface

@date 2016

@author
Sensics, Inc.
<http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
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

#pragma once
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include "RenderManagerD3DBase.h"
#include "RenderManagerOpenGL.h"
#include "GraphicsLibraryD3D11.h"

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <map>

namespace osvr {
    namespace renderkit {

        class RenderManagerD3D11ATW : public RenderManagerD3D11Base {
        private:
            // Indices into the keys for the Render Thread
            const UINT rtAcqKey = 0;
            const UINT rtRelKey = 1;
            // Indices into the keys for the ATW thread
            const UINT acqKey = 1;
            const UINT relKey = 0;

            typedef struct {
                osvr::renderkit::RenderBuffer rtBuffer;
                osvr::renderkit::RenderBuffer atwBuffer;
                IDXGIKeyedMutex* rtMutex;
                IDXGIKeyedMutex* atwMutex;
                HANDLE sharedResourceHandle;
            } RenderBufferATWInfo;

            std::mutex mLock;
            std::shared_ptr<std::thread> mThread = nullptr;
            std::map<osvr::renderkit::RenderBufferD3D11*, RenderBufferATWInfo> mBufferMap;
            GraphicsLibraryD3D11* mRTGraphicsLibrary;

            struct {
                std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
                std::vector<osvr::renderkit::RenderInfo> renderInfo;
                std::vector<OSVR_ViewportDescription> normalizedCroppingViewports;
                RenderParams renderParams;
                bool flipInY;
            } mNextFrameInfo;

            bool mQuit = false;
            bool mStarted = false;
            bool mFirstFramePresented = false;

        public:
            /**
            * Construct an D3D ATW wrapper around an existing D3D render
            * manager. Takes ownership of the passed in render manager
            * and deletes it when the wrapper is deleted.
            */
            RenderManagerD3D11ATW(
                std::shared_ptr<osvr::clientkit::ClientContext> context,
                ConstructorParameters p, RenderManagerD3D11Base* D3DToHarness)
                : RenderManagerD3D11Base(context, p) {
                mRTGraphicsLibrary = p.m_graphicsLibrary.D3D11;
                mRenderManager.reset(D3DToHarness);
            }

            virtual ~RenderManagerD3D11ATW() {
                if (mThread) {
                    stop();
                    mThread->join();
                }
            }

            OpenResults OpenDisplay() override {
                //std::lock_guard<std::mutex> lock(mLock);
                OpenResults ret = mRenderManager->OpenDisplay();
                if (ret.status != OpenStatus::FAILURE) {
                    start();
                }
                return ret;
            }

            bool OSVR_RENDERMANAGER_EXPORT
            RegisterRenderBuffers(const std::vector<RenderBuffer>& buffers,
                bool appWillNotOverwriteBeforeNewPresent = false) override {
                // @todo: check that we haven't started the thread yet (i.e. OpenDisplay called)

                HRESULT hr;
                auto rtDevice = mRTGraphicsLibrary->device;

                std::vector<osvr::renderkit::RenderInfo> renderInfo = mRenderManager->GetRenderInfo();
                std::vector<osvr::renderkit::RenderBuffer> renderBuffers;

                for (size_t i = 0; i < buffers.size(); i++) {
                    RenderBufferATWInfo newInfo;

                    //
                    // Let's start by getting some resources from the render thread's ID3D11Device
                    //
                    newInfo.rtBuffer = buffers[i];


                    // we need to get the shared resource HANDLE for the ID3D11Texture2D, but in order to
                    // get that, we need to get the IDXGIResource* first
                    {
                        IDXGIResource* dxgiResource = NULL;
                        hr = buffers[i].D3D11->colorBuffer->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&dxgiResource);
                        if (FAILED(hr)) {
                            std::cerr << "Can't get the IDXGIResource for the texture resource." << std::endl;
                            return false;
                        }

                        // now get the shared HANDLE
                        hr = dxgiResource->GetSharedHandle(&newInfo.sharedResourceHandle);
                        if (FAILED(hr)) {
                            std::cerr << "Can't get the shared handle from the dxgiResource." << std::endl;
                            return false;
                        }
                        dxgiResource->Release(); // we don't need this anymore
                    }

                    // now get the IDXGIKeyedMutex for the render thread's ID3D11Texture2D
                    hr = buffers[i].D3D11->colorBuffer->QueryInterface(
                        __uuidof(IDXGIKeyedMutex), (LPVOID*)&newInfo.rtMutex);

                    if (FAILED(hr) || newInfo.rtMutex == nullptr) {
                        std::cerr << "Can't get the IDXGIKeyedMutex from the texture resource." << std::endl;
                        return false;
                    }
                    
                    // Start with all render thread's IDXGIKeyedMutex locked
                    //std::cerr << "Locking buffer " << (size_t)buffers[i].D3D11 << " for the render thread." << std::endl;
                    hr = newInfo.rtMutex->AcquireSync(rtAcqKey, INFINITE);
                    if (FAILED(hr)) {
                        std::cerr << "Could not AcquireSync on a game render target's IDXGIKeyedMutex during registration." << std::endl;
                        return false;
                    }

                    //
                    // Next, some resources for the ATW thread's ID3D11Device
                    //

                    // OK, now we need to open the shared resource on the ATW thread's ID3D11Device.
                    {
                        auto atwDevice = renderInfo[i].library.D3D11->device;
                        ID3D11Texture2D *texture2D = nullptr;
                        hr = atwDevice->OpenSharedResource(newInfo.sharedResourceHandle, __uuidof(ID3D11Texture2D),
                            (LPVOID*)&texture2D);
                        if (FAILED(hr)) {
                            std::cerr << "RenderManagerThread::RegisterRenderBuffers() - failed to open shared resource." << std::endl;
                            return false;
                        }

                        // And get the IDXGIKeyedMutex for the ATW thread's ID3D11Texture2D
                        hr = texture2D->QueryInterface(__uuidof(IDXGIKeyedMutex), (LPVOID*)&newInfo.atwMutex);
                        if (FAILED(hr) || newInfo.atwMutex == nullptr) {
                            std::cerr << "RenderManagerThread::RegisterRenderBuffers() - failed to create keyed mutex." << std::endl;
                            return false;
                        }

                        // We can't use the render thread's ID3D11RenderTargetView. Create one from
                        // the ATW's ID3D11Texture2D handle.

                        // Fill in the resource view for your render texture buffer here
                        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
                        memset(&renderTargetViewDesc, 0, sizeof(renderTargetViewDesc));
                        // This must match what was created in the texture to be rendered
                        // @todo Figure this out by introspection on the texture?
                        //renderTargetViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                        renderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                        renderTargetViewDesc.Texture2D.MipSlice = 0;

                        // Create the render target view.
                        ID3D11RenderTargetView *renderTargetView; //< Pointer to our render target view
                        hr = atwDevice->CreateRenderTargetView(texture2D, &renderTargetViewDesc, &renderTargetView);
                        if (FAILED(hr)) {
                            std::cerr << "Could not create render target for eye " << i
                                << std::endl;
                            return false;
                        }

                        newInfo.atwBuffer.D3D11 = new osvr::renderkit::RenderBufferD3D11();
                        newInfo.atwBuffer.D3D11->colorBuffer = texture2D;
                        newInfo.atwBuffer.D3D11->colorBufferView = renderTargetView;
                        renderBuffers.push_back(newInfo.atwBuffer);
                    }

                    mBufferMap[buffers[i].D3D11] = newInfo;
                }

                return mRenderManager->RegisterRenderBuffers(renderBuffers, appWillNotOverwriteBeforeNewPresent);
            }

            bool OSVR_RENDERMANAGER_EXPORT
            PresentRenderBuffers(const std::vector<RenderBuffer>& renderBuffers,
                const std::vector<RenderInfo>& renderInfoUsed,
                const RenderParams& renderParams = RenderParams(),
                const std::vector<OSVR_ViewportDescription>& normalizedCroppingViewports = std::vector<OSVR_ViewportDescription>(),
                bool flipInY = false) override {

                {
                    std::lock_guard<std::mutex> lock(mLock);
                    HRESULT hr;

                    // For all the buffers that have been given to the ATW thread,
                    // release them there and acquire them back for the render thread.
                    // This starts us with the render thread owning all of the buffers.
                    // Then clear the buffer list that is owned by the ATW thread.
                    for (size_t i = 0; i < mNextFrameInfo.renderBuffers.size(); i++) {
                      auto key = mNextFrameInfo.renderBuffers[i].D3D11;
                      auto bufferInfoItr = mBufferMap.find(key);
                      if (bufferInfoItr == mBufferMap.end()) {
                        std::cerr << "No Buffer info for key " << (size_t)key << std::endl;
                        mQuit = true;
                      }
                      auto bufferInfo = bufferInfoItr->second;
                      //std::cerr << "releasing atwMutex " << (size_t)key << std::endl;
                      hr = bufferInfoItr->second.atwMutex->ReleaseSync(relKey);
                      if (FAILED(hr)) {
                        std::cerr << "Could not ReleaseSync in the render manager thread." << std::endl;
                        mQuit = true;
                      }
                      //std::cerr << "acquiring rtMutex " << (size_t)key << std::endl;
                      hr = bufferInfoItr->second.rtMutex->AcquireSync(rtAcqKey, INFINITE);
                      if (FAILED(hr)) {
                        std::cerr << "Could not lock the render thread's mutex" << std::endl;
                        mQuit = true;
                      }
                    }
                    mNextFrameInfo.renderBuffers.clear();

                    // For all of the buffers we're getting ready to hand to the ATW thread,
                    // we release our lock and lock them for that thread and then push them
                    // onto the vector for use by that thread.
                    for (size_t i = 0; i < renderBuffers.size(); i++) {
                        // We need to unlock the render thread's mutex (remember they're locked initially)
                        auto key = renderBuffers[i].D3D11;
                        //std::cerr << "releasing rtMutex " << (size_t)key << std::endl;
                        auto bufferInfoItr = mBufferMap.find(key);
                        if (bufferInfoItr == mBufferMap.end()) {
                            std::cerr << "Could not find buffer info for RenderBuffer " << (size_t)key << std::endl;
                            return false;
                        }
                        hr = bufferInfoItr->second.rtMutex->ReleaseSync(rtRelKey);
                        if (FAILED(hr)) {
                            std::cerr << "Could not ReleaseSync on a client render target's IDXGIKeyedMutex during present." << std::endl;
                            return false;
                        }
                        // and lock the ATW thread's mutex
                        //std::cerr << "locking atwMutex " << (size_t)key << std::endl;
                        hr = bufferInfoItr->second.atwMutex->AcquireSync(rtRelKey, INFINITE);
                        if (FAILED(hr)) {
                            std::cerr << "Could not AcquireSync on the atw IDXGIKeyedMutex during present.";
                            return false;
                        }
                        mNextFrameInfo.renderBuffers.push_back(bufferInfoItr->second.rtBuffer);
                    }
                    mFirstFramePresented = true;
                    mNextFrameInfo.renderInfo = renderInfoUsed;
                    mNextFrameInfo.flipInY = flipInY;
                    mNextFrameInfo.renderParams = renderParams;
                    mNextFrameInfo.normalizedCroppingViewports = normalizedCroppingViewports;
                }
                return true;
            }

        protected:

            ///**
            // * Construct an D3D ATW wrapper around an existing D3D render
            // * manager. Takes ownership of the passed in render manager
            // * and deletes it when the wrapper is deleted.
            // */
            //RenderManagerD3D11ATW(
            //    std::shared_ptr<osvr::clientkit::ClientContext> context,
            //    ConstructorParameters p, std::unique_ptr<RenderManagerD3D11Base> && D3DToHarness)
            //    : RenderManager(context, p) {
            //    mRTGraphicsLibrary = p.m_graphicsLibrary.D3D11;
            //}

            // @todo do we need these?
            bool m_doingOkay;   //< Are we doing okay?
            bool m_displayOpen; //< Has our display been opened?

            void start() {
                if (mStarted) {
                    std::cerr << "RenderManagerThread::start() - thread loop already started." << std::endl;
                }
                else {
                    mThread.reset(new std::thread(std::bind(&RenderManagerD3D11ATW::threadFunc, this)));
                }
            }

            void stop() {
                std::lock_guard<std::mutex> lock(mLock);
                if (!mStarted) {
                    std::cerr << "RenderManagerThread::stop() - thread loop not already started." << std::endl;
                }
                mQuit = true;
            }

            bool getQuit() {
                std::lock_guard<std::mutex> lock(mLock);
                return mQuit;
            }

            void threadFunc() {
                bool quit = getQuit();
                size_t iteration = 0;
                while (!quit) {
                    auto renderInfo = mRenderManager->GetRenderInfo();
                    bool timeToPresent = false;
                    const int presentWindow = 15; // in microseconds
                    for (size_t i = 0; i < renderInfo.size(); i++) {
                        osvr::renderkit::RenderTimingInfo timing;
                        if (!mRenderManager->GetTimingInfo(i, timing)) {
                            std::cerr << "RenderManagerThread::threadFunc() = couldn't get timing info for eye " << i << std::endl;
                        }
                        auto time = timing.timeUntilNextPresentRequired.microseconds;
                        timeToPresent = time >= 0 && time < presentWindow;
                        if (timeToPresent) {
                            break;
                        }
                    }

                    if (timeToPresent) {
                        std::lock_guard<std::mutex> lock(mLock);
                        if (mFirstFramePresented) {
                            // Update the context so we get our callbacks called and
                            // update tracker state.
                            mRenderManager->m_context->update();

                            {
                                // make a new RenderBuffers array with the atw thread's buffers
                                std::vector<osvr::renderkit::RenderBuffer> atwRenderBuffers;
                                for (size_t i = 0; i < mNextFrameInfo.renderBuffers.size(); i++) {
                                    auto key = mNextFrameInfo.renderBuffers[i].D3D11;
                                    auto bufferInfoItr = mBufferMap.find(key);
                                    if (bufferInfoItr == mBufferMap.end()) {
                                        std::cerr << "No buffer info for key " << (size_t)key << std::endl;
                                        throw std::runtime_error("No buffer info for given key.");
                                    }
                                    atwRenderBuffers.push_back(bufferInfoItr->second.atwBuffer);
                                }

                                // Send the rendered results to the screen
                                if (!mRenderManager->PresentRenderBuffers(
                                    atwRenderBuffers,
                                    mNextFrameInfo.renderInfo,
                                    mNextFrameInfo.renderParams,
                                    mNextFrameInfo.normalizedCroppingViewports,
                                    mNextFrameInfo.flipInY)) {
                                    std::cerr << "PresentRenderBuffers() returned false, maybe because it was asked to quit" << std::endl;
                                    mQuit = true;
                                }
                            }

                            iteration++;
                        }
                    }
                    quit = mQuit;
                }
            }

            // We harness a D3D11 DirectMode renderer to do our
            // DirectMode work and to handle the timing.
            std::unique_ptr<RenderManagerD3D11Base> mRenderManager;

            //===================================================================
            // Overloaded render functions from the base class.
            bool RenderDisplayInitialize(size_t display) override { return true; }
            bool RenderEyeFinalize(size_t eye) override { return true; }

            bool PresentDisplayInitialize(size_t display) override { return true; }
            bool PresentDisplayFinalize(size_t display) override { return true; }
            bool PresentFrameFinalize() override { return true; }

            // The distortion mesh is applied after time warp, so needs to be
            // passed on down to the harnessed RenderManager to handle it.
            OSVR_RENDERMANAGER_EXPORT bool UpdateDistortionMeshesInternal(
                DistortionMeshType type,
                std::vector<DistortionParameters> const& distort) override {
                // @todo lock?
                return mRenderManager->UpdateDistortionMeshesInternal(type,
                    distort);
            }

            bool RegisterRenderBuffersInternal(
                const std::vector<RenderBuffer>& buffers,
                bool appWillNotOverwriteBeforeNewPresent = false) override {

                // @todo lock?

                // If they are promising to not present the same buffers
                // each frame, make sure they have registered twice as
                // many as there are viewports for them to render.
                if (appWillNotOverwriteBeforeNewPresent) {
                  if (buffers.size() != LatchRenderInfo() * 2) {
                    std::cerr << "RenderManagerD3D11ATW::"
                      << "RegisterRenderBuffersInternal: Promised"
                      << " not to re-use buffers, but number registered ("
                      << buffers.size() << " != twice number of viewports ("
                      << LatchRenderInfo() << std::endl;
                    return false;
                  }
                }

                // @todo If not promising to not overwrite, we need to
                // construct a set of copy buffers that we'll pull the
                // data into and render from.

                return mRenderManager->RegisterRenderBuffersInternal(
                    buffers, appWillNotOverwriteBeforeNewPresent);
            }

            friend RenderManager OSVR_RENDERMANAGER_EXPORT*
                createRenderManager(OSVR_ClientContext context,
                const std::string& renderLibraryName,
                GraphicsLibrary graphicsLibrary);
        };
    }
}
