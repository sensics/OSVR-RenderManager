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

            /// Holds the information needed to handle locking and unlocking of
            /// buffers and also the copying of buffers in the case where we have
            /// our own internal copy.
            typedef struct {
                osvr::renderkit::RenderBuffer atwBuffer;
                IDXGIKeyedMutex* rtMutex;
                IDXGIKeyedMutex* atwMutex;
                HANDLE sharedResourceHandle;
                ID3D11Texture2D *textureCopy;   //< nullptr if no copy needed.
            } RenderBufferATWInfo;
            std::map<osvr::renderkit::RenderBufferD3D11*, RenderBufferATWInfo> mBufferMap;

            std::mutex mLock;
            std::shared_ptr<std::thread> mThread = nullptr;

            /// Holds information about the buffers to be used by the next rendering
            /// pass.  This is filled in by PresentRenderBuffersInternal() and used
            /// by the ATW thread.  Access should be guarded using the mLock to prevent
            /// simultaneous use in both threads.
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

            /// Used to keep track of when rendering has completed so we can hand
            /// our buffers over to the ATW thread.
            ID3D11Query* m_completionQuery = nullptr;

        public:
            /**
            * Construct an D3D ATW wrapper around an existing D3D render
            * manager. Takes ownership of the passed in render manager
            * and deletes it when the wrapper is deleted.
            */
            RenderManagerD3D11ATW(
                OSVR_ClientContext context,
                ConstructorParameters p, RenderManagerD3D11Base* D3DToHarness)
                : RenderManagerD3D11Base(context, p) {
                mRenderManager.reset(D3DToHarness);
            }

            virtual ~RenderManagerD3D11ATW() {
                if (mThread) {
                    stop();
                    mThread->join();
                }
                // Delete textures and views that we allocated or otherwise opened
                std::map<osvr::renderkit::RenderBufferD3D11*, RenderBufferATWInfo>::iterator i;
                for (i = mBufferMap.begin(); i != mBufferMap.end(); i++) {
                  i->second.atwBuffer.D3D11->colorBuffer->Release();
                  // We don't release the colorBufferView because we didn't create one.
                  if (i->second.textureCopy != nullptr) {
                    i->second.textureCopy->Release();
                  }
                }
                if (m_completionQuery) {
                  m_completionQuery->Release();
                  m_completionQuery = nullptr;
                }
            }

            OpenResults OpenDisplay() override {
                std::lock_guard<std::mutex> lock(mLock);

                OpenResults ret;

                // Set the device and context we're going to use
                // but don't open an additional display -- we're
                // going to pass all display-related things down to
                // our render thread.
                if (!SetDeviceAndContext()) {
                  m_log->error() << "RenderManagerD3D11ATW::OpenDisplay: Could not "
                    "create device and context";
                  ret.status = OpenStatus::FAILURE;
                  return ret;
                }

                // Try to open the display in the harnessed
                // RenderManager.  Return false if it fails.
                ret = mRenderManager->OpenDisplay();
                if (ret.status == OpenStatus::FAILURE) {
                  m_log->error() << "RenderManagerD3D11ATW::OpenDisplay: Could not "
                    "open display in harnessed RenderManager";
                  return ret;
                }

                //======================================================
                // Fill in our library with the things the application may need to
                // use to do its graphics state set-up.
                m_library.D3D11->device = m_D3D11device;
                m_library.D3D11->context = m_D3D11Context;

                //======================================================
                // Construct our completion query that will be used to
                // wait for rendering completion.
                {
                  D3D11_QUERY_DESC desc = {};
                  desc.Query = D3D11_QUERY_EVENT;
                  HRESULT hr = m_D3D11device->CreateQuery(&desc, &m_completionQuery);
                  if (FAILED(hr)) {
                    m_log->error() << "RenderManagerD3D11ATW::OpenDisplay: "
                      "Warning: Failed to create completion event query: code "
                      << hr;
                    m_completionQuery = nullptr;
                  }
                }

                //======================================================
                // Start our ATW sub-thread.
                start();

                //======================================================
                // Fill in our library, rather than that of
                // the harnessed RenderManager, since this is the one
                // that the client will deal with.
                ret.library = m_library;
                return ret;
            }

        protected:

          bool PresentRenderBuffersInternal(const std::vector<RenderBuffer>& renderBuffers,
                const std::vector<RenderInfo>& renderInfoUsed,
                const RenderParams& renderParams = RenderParams(),
                const std::vector<OSVR_ViewportDescription>& normalizedCroppingViewports =
                  std::vector<OSVR_ViewportDescription>(),
                bool flipInY = false) override {

                  if (!m_renderBuffersRegistered) {
                    m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal: "
                      << "Render buffers not yet registered, ignoring present request.";
                    return true;
                  }

                  // We use a D3D query placed right at the end of rendering to make
                  // sure we wait until rendering has finished on our buffers before
                  // handing them over to the ATW thread.  We flush our queue so that
                  // rendering will get moving right away.
                  if (m_completionQuery) {
                    m_D3D11Context->End(m_completionQuery);
                    m_D3D11Context->Flush();
                    while (S_FALSE ==
                      m_D3D11Context->GetData(m_completionQuery, nullptr, 0, 0)) {
                      // We don't want to miss the completion because Windows has
                      // swapped us out, so we busy-wait here on the completion
                      // event.
                    }
                  }

                  // Lock our mutex so we don't adjust the buffers while rendering is happening.
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
                      m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                        << "No Buffer info for key " << (size_t)key;
                      m_doingOkay = false;
                      mQuit = true;
                    }
                    auto bufferInfo = bufferInfoItr->second;
                    hr = bufferInfoItr->second.atwMutex->ReleaseSync(relKey);
                    if (FAILED(hr)) {
                      m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                        << "Could not ReleaseSync in the render manager thread.";
                      m_doingOkay = false;
                      mQuit = true;
                    }
                    hr = bufferInfoItr->second.rtMutex->AcquireSync(rtAcqKey, INFINITE);
                    if (FAILED(hr)) {
                      m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                        << "Could not lock the render thread's mutex";
                      m_doingOkay = false;
                      mQuit = true;
                    }
                  }
                  mNextFrameInfo.renderBuffers.clear();

                  // If we have non-NULL texture-copy pointers in any of the buffers
                  // associated with the presented buffers, copy the texture into
                  // the associated buffer.  This is to handle the case where the client
                  // did not promise not to overwrite the texture before it is presented.
                  for (size_t i = 0; i < renderBuffers.size(); i++) {
                    auto key = renderBuffers[i].D3D11;
                    auto bufferInfoItr = mBufferMap.find(key);
                    if (bufferInfoItr == mBufferMap.end()) {
                      m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                        << "Could not find buffer info for RenderBuffer " << (size_t)key;
                      m_log->error() << "  (Be sure to register buffers before presenting them)";
                      m_doingOkay = false;
                      return false;
                    }

                    if (bufferInfoItr->second.textureCopy != nullptr) {
                      m_D3D11Context->CopyResource(bufferInfoItr->second.textureCopy,
                        renderBuffers[i].D3D11->colorBuffer);
                    }
                  }

                  // For all of the buffers we're getting ready to hand to the ATW thread,
                  // we release our lock and lock them for that thread and then push them
                  // onto the vector for use by that thread.
                  for (size_t i = 0; i < renderBuffers.size(); i++) {
                      // We need to unlock the render thread's mutex (remember they're locked initially)
                      auto key = renderBuffers[i].D3D11;
                      auto bufferInfoItr = mBufferMap.find(key);
                      if (bufferInfoItr == mBufferMap.end()) {
                          m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                            << "Could not find buffer info for RenderBuffer " << (size_t)key;
                          m_log->error() << "  (Be sure to register buffers before presenting them)";
                          m_doingOkay = false;
                          return false;
                      }
                      hr = bufferInfoItr->second.rtMutex->ReleaseSync(rtRelKey);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                            << "Could not ReleaseSync on a client render target's IDXGIKeyedMutex during present.";
                          m_doingOkay = false;
                          return false;
                      }
                      // and lock the ATW thread's mutex
                      hr = bufferInfoItr->second.atwMutex->AcquireSync(rtRelKey, INFINITE);
                      if (FAILED(hr)) {
                          m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                            << "Could not AcquireSync on the atw IDXGIKeyedMutex during present.";
                          m_doingOkay = false;
                          return false;
                      }
                      mNextFrameInfo.renderBuffers.push_back(renderBuffers[i]);
                  }
                  mFirstFramePresented = true;
                  mNextFrameInfo.renderInfo = renderInfoUsed;
                  mNextFrameInfo.flipInY = flipInY;
                  mNextFrameInfo.renderParams = renderParams;
                  mNextFrameInfo.normalizedCroppingViewports = normalizedCroppingViewports;
                  return true;
            }

            void start() {
                if (mStarted) {
                  m_log->error() << "RenderManagerThread::start() - thread loop already started.";
                } else {
                    mThread.reset(new std::thread(std::bind(&RenderManagerD3D11ATW::threadFunc, this)));
                }
                mStarted = true;
            }

            void stop() {
                std::lock_guard<std::mutex> lock(mLock);
                if (!mStarted) {
                  m_log->error() << "RenderManagerThread::stop() - thread loop not already started.";
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

                    // Wait until it is time to present the render buffers.
                    // If we've got a specified maximum time before vsync,
                    // we use that.  Otherwise, we set the threshold to 1ms
                    // to give us some time to swap things out before vsync.
                    bool timeToPresent = false;

                    // Convert from milliseconds to seconds
                    float thresholdF = m_params.m_maxMSBeforeVsyncTimeWarp / 1e3f;
                    if (thresholdF == 0) { thresholdF = 1e-3f; }
                    OSVR_TimeValue threshold;
                    threshold.seconds = static_cast<OSVR_TimeValue_Seconds>(thresholdF);
                    thresholdF -= threshold.seconds;
                    threshold.microseconds =
                      static_cast<OSVR_TimeValue_Microseconds>(thresholdF * 1e6);

                    // We use the timing info from the first display to
                    // determine when it is time to present.
                    // @todo Need one thread per display if we have displays
                    // that are not gen-locked.
                    // @todo Consider making a function that both the RenderManagerBase.cpp
                    // and this code calls to check if we're within range.
                    osvr::renderkit::RenderTimingInfo timing;
                    if (!mRenderManager->GetTimingInfo(0, timing)) {
                      m_log->error() << "RenderManagerThread::threadFunc() = couldn't get timing info";
                    }
                    OSVR_TimeValue nextRetrace = timing.hardwareDisplayInterval;
                    osvrTimeValueDifference(&nextRetrace,
                      &timing.timeSincelastVerticalRetrace);
                    if (osvrTimeValueGreater(&threshold, &nextRetrace)) {
                      timeToPresent = true;
                    }

                    if (timeToPresent) {
                        // Lock our mutex so that we're not rendering while new buffers are
                        // being presented.
                        std::lock_guard<std::mutex> lock(mLock);
                        if (mFirstFramePresented) {
                            // Update the context so we get our callbacks called and
                            // update tracker state, which will be read during the
                            // time-warp calculation in our harnessed RenderManager.
                            osvrClientUpdate(mRenderManager->m_context);

                            {
                                // make a new RenderBuffers array with the atw thread's buffers
                                std::vector<osvr::renderkit::RenderBuffer> atwRenderBuffers;
                                for (size_t i = 0; i < mNextFrameInfo.renderBuffers.size(); i++) {
                                    auto key = mNextFrameInfo.renderBuffers[i].D3D11;
                                    auto bufferInfoItr = mBufferMap.find(key);
                                    if (bufferInfoItr == mBufferMap.end()) {
                                      m_log->error() << "No buffer info for key " << (size_t)key;
                                        m_doingOkay = false;
                                        mQuit = true;
                                    }
                                    atwRenderBuffers.push_back(bufferInfoItr->second.atwBuffer);
                                }

                                // Send the rendered results to the screen, using the
                                // RenderInfo that was handed to us by the client the last
                                // time they gave us some images.
                                if (!mRenderManager->PresentRenderBuffers(
                                    atwRenderBuffers,
                                    mNextFrameInfo.renderInfo,
                                    mNextFrameInfo.renderParams,
                                    mNextFrameInfo.normalizedCroppingViewports,
                                    mNextFrameInfo.flipInY)) {
                                    m_log->error() << "PresentRenderBuffers() returned false, maybe because it was asked to quit";
                                    m_doingOkay = false;
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
            // Overloaded render functions required from the base class.
            bool RenderDisplayInitialize(size_t display) override { return true; }
            bool RenderEyeFinalize(size_t eye) override { return true; }

            bool PresentDisplayInitialize(size_t display) override { return true; }
            bool PresentDisplayFinalize(size_t display) override { return true; }
            bool PresentFrameFinalize() override { return true; }

            //===================================================================
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

                // They may be using one buffer for two eyes or one buffer
                // per eye, so we can't check the number of buffers.  Also,
                // we should support letting them register the render buffers
                // in batches, not all at once.

                HRESULT hr;

                std::vector<osvr::renderkit::RenderInfo> renderInfo = mRenderManager->GetRenderInfo();
                std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
                size_t numRenderInfos = renderInfo.size();

                for (size_t i = 0; i < buffers.size(); i++) {
                  RenderBufferATWInfo newInfo;
                  newInfo.textureCopy = nullptr; // We don't yet have a place to copy the texture.

                  // OK, now we need to open the shared resource on the ATW thread's ID3D11Device.
                  // We assume that the buffers for the eyes repeat, so that we modulo the number
                  // of buffers to find the correct index.
                  // @todo Specify this requirement in the API
                  {
                    auto atwDevice = renderInfo[i % numRenderInfos].library.D3D11->device;
                    ID3D11Texture2D *texture2D = nullptr;
                    m_log->flush();

                    if (appWillNotOverwriteBeforeNewPresent) {
                      // we need to get the shared resource HANDLE for the ID3D11Texture2D, but in order to
                      // get that, we need to get the IDXGIResource* first
                      IDXGIResource* dxgiResource = NULL;
                      hr = buffers[i].D3D11->colorBuffer->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&dxgiResource);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Can't get the IDXGIResource for the texture resource.";
                        m_doingOkay = false;
                        return false;
                      }

                      // now get the shared HANDLE
                      hr = dxgiResource->GetSharedHandle(&newInfo.sharedResourceHandle);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Can't get the shared handle from the dxgiResource.";
                        m_doingOkay = false;
                        return false;
                      }
                      dxgiResource->Release(); // we don't need this anymore

                      // Now get the IDXGIKeyedMutex for the render thread's ID3D11Texture2D
                      // The application should have already locked these mutexes, so we
                      // don't attempt to lock them here.
                      hr = buffers[i].D3D11->colorBuffer->QueryInterface(
                        __uuidof(IDXGIKeyedMutex), (LPVOID*)&newInfo.rtMutex);
                      if (FAILED(hr) || newInfo.rtMutex == nullptr) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Can't get the IDXGIKeyedMutex from the texture resource.";
                        m_doingOkay = false;
                        return false;
                      }

                      // The application is maintaining two sets of buffers, so we don't
                      // need to make a copy of this texture when it is presented.  We just
                      // get a shared handle to it and re-use the existing texture.
                      hr = atwDevice->OpenSharedResource(newInfo.sharedResourceHandle, __uuidof(ID3D11Texture2D),
                        (LPVOID*)&texture2D);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: - failed to open shared resource.";
                        m_doingOkay = false;
                        return false;
                      }
                    } else {
                      // The application is not maintaining two sets of buffers, so we
                      // need to make a copy of this texture when it is presented.  Here
                      // we allocate a place to put it.  We have to allocate a shared
                      // resource, so it can be used by both threads.  It is allocated on
                      // the render thread's device.  We need to introspect the texture
                      // to find its size and we need to make sure that we don't make
                      // two copies of the same buffer.
                      // @todo Handle the case where the client sends us the same buffer
                      // twice or more, with multiple eyes packed into the same one.  We
                      // don't want to duplicate that buffer more than once.

                      D3D11_TEXTURE2D_DESC info;
                      buffers[i].D3D11->colorBuffer->GetDesc(&info);

                      D3D11_TEXTURE2D_DESC textureDesc = {};
                      textureDesc.Width = info.Width;
                      textureDesc.Height = info.Height;
                      textureDesc.MipLevels = 1;
                      textureDesc.ArraySize = 1;
                      textureDesc.Format = info.Format;
                      textureDesc.SampleDesc.Count = 1;
                      textureDesc.SampleDesc.Quality = 0;
                      textureDesc.Usage = D3D11_USAGE_DEFAULT;
                      // We need it to be both a render target and a shader resource
                      textureDesc.BindFlags =
                        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
                      textureDesc.CPUAccessFlags = 0;
                      textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
                      hr = m_D3D11device->CreateTexture2D(&textureDesc, NULL, &texture2D);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: - Can't create copy texture for buffer "
                          << i;
                        m_doingOkay = false;
                        return false;
                      }

                      // We need to get the shared resource HANDLE for the ID3D11Texture2D,
                      //  but in order to get that, we need to get the IDXGIResource* first
                      IDXGIResource* dxgiResource = NULL;
                      hr = texture2D->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&dxgiResource);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Can't get the IDXGIResource for created texture resource.";
                        m_doingOkay = false;
                        return false;
                      }

                      // now get the shared HANDLE
                      hr = dxgiResource->GetSharedHandle(&newInfo.sharedResourceHandle);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Can't get the shared handle from the dxgiResource from the created texture.";
                        m_doingOkay = false;
                        return false;
                      }
                      dxgiResource->Release(); // we don't need this anymore

                      // Now get the IDXGIKeyedMutex for the render thread's ID3D11Texture2D
                      // Then lock the mutex for the render thread.
                      hr = texture2D->QueryInterface(
                        __uuidof(IDXGIKeyedMutex), (LPVOID*)&newInfo.rtMutex);
                      if (FAILED(hr) || newInfo.rtMutex == nullptr) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Can't get the IDXGIKeyedMutex from the created texture resource.";
                        m_doingOkay = false;
                        return false;
                      }
                      hr = newInfo.rtMutex->AcquireSync(0, INFINITE);
                      if (FAILED(hr)) {
                        m_log->error() << "RenderManagerD3D11ATW::"
                          << "RegisterRenderBuffersInternal: Could not acquire mutex";
                        m_doingOkay = false;
                        return false;
                      }

                      // Record the place to copy incoming textures to.
                      newInfo.textureCopy = texture2D;
                    }

                    // And get the IDXGIKeyedMutex for the ATW thread's ID3D11Texture2D
                    hr = texture2D->QueryInterface(__uuidof(IDXGIKeyedMutex), (LPVOID*)&newInfo.atwMutex);
                    if (FAILED(hr) || newInfo.atwMutex == nullptr) {
                      m_log->error() << "RenderManagerD3D11ATW::"
                        << "RegisterRenderBuffersInternal: - failed to create keyed mutex.";
                      m_doingOkay = false;
                      return false;
                    }

                    // We can't use the render thread's ID3D11RenderTargetView. Create one from
                    // the ATW's ID3D11Texture2D handle.

                    // Find out the format of the texture by looking at its info.
                    D3D11_TEXTURE2D_DESC info;
                    texture2D->GetDesc(&info);

                    // Fill in the resource view for your render texture buffer here
                    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
                    memset(&renderTargetViewDesc, 0, sizeof(renderTargetViewDesc));
                    // This must match what was created in the texture to be rendered
                    renderTargetViewDesc.Format = info.Format;
                    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                    renderTargetViewDesc.Texture2D.MipSlice = 0;

                    newInfo.atwBuffer.D3D11 = new osvr::renderkit::RenderBufferD3D11();
                    newInfo.atwBuffer.D3D11->colorBuffer = texture2D;

                    // We do not need a render target view for the ATW thread -- it will
                    // only be reading from the buffer, not rendering into it.  Our base
                    // class will create our RenderTargetView the first time the app calls
                    // Render().
                    newInfo.atwBuffer.D3D11->colorBufferView = nullptr; // We don't need this.

                    renderBuffers.push_back(newInfo.atwBuffer);
                  }

                  mBufferMap[buffers[i].D3D11] = newInfo;
                }

                if (!mRenderManager->RegisterRenderBuffers(renderBuffers,
                    appWillNotOverwriteBeforeNewPresent)) {
                  m_log->error() << "RenderManagerD3D11ATW::"
                    << "RegisterRenderBuffersInternal: Could not Register render"
                    << " buffers on harnessed RenderManager";
                  m_doingOkay = false;
                  return false;
                }
                
                // We're done -- call the base-class function to notify that we've
                // registered our buffers
                return RenderManager::RegisterRenderBuffersInternal(buffers,
                  appWillNotOverwriteBeforeNewPresent);
            }

            friend RenderManager OSVR_RENDERMANAGER_EXPORT*
                createRenderManager(OSVR_ClientContext context,
                const std::string& renderLibraryName,
                GraphicsLibrary graphicsLibrary);
        };
    }
}
