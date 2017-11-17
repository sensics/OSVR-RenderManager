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

#include <osvr/Util/Logger.h>

#include "RenderManagerD3DBase.h"
#include "RenderManagerOpenGL.h"
#include "GraphicsLibraryD3D11.h"

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <map>
#include <set>

#include <vrpn_Shared.h>

namespace osvr {
    namespace renderkit {

        class RenderManagerD3D11ATW : public RenderManagerD3D11Base {
        private:

            /// Holds the information needed to handle locking and unlocking of
            /// buffers and also the copying of buffers in the case where we have
            /// our own internal copy.
            typedef struct {
                osvr::renderkit::RenderBuffer atwBuffer;
                HANDLE sharedResourceHandle;
                ID3D11Texture2D *textureCopy;   ///< nullptr if no copy needed.
            } RenderBufferATWInfo;
            std::map<ID3D11Texture2D*, RenderBufferATWInfo> mBufferMap;

            std::mutex mMutex;
			std::condition_variable mPresentFinishedCV;
            std::shared_ptr<std::thread> mThread = nullptr;

            /// Holds information about the buffers to be used by the next rendering
            /// pass.  This is filled in by PresentRenderBuffersInternal() and used
            /// by the ATW thread.  Access should be guarded using the mMutex to prevent
            /// simultaneous use in both threads.
            struct {
                std::vector<ID3D11Texture2D*> colorBuffers;
                std::vector<osvr::renderkit::RenderInfo> renderInfo;
                std::vector<OSVR_ViewportDescription> normalizedCroppingViewports;
                RenderParams renderParams;
                bool flipInY;
            } mNextFrameInfo;
			bool mNextFrameAvailable = false;

            bool mQuit = false;
            bool mStarted = false;
            bool mFirstFramePresented = false;

          public:
            /**
            * Construct an D3D ATW wrapper around an existing D3D render
            * manager. Takes ownership of the passed in render manager
            * and deletes it when the wrapper is deleted.
            */
            RenderManagerD3D11ATW(OSVR_ClientContext context, ConstructorParameters p,
                                  RenderManagerD3D11Base* D3DToHarness)
                : RenderManagerD3D11Base(context, p) {
                mRenderManager.reset(D3DToHarness);
            }

            virtual ~RenderManagerD3D11ATW() {
                if (mThread) {
                    stop();
                    mThread->join();
                }
                // Delete textures and views that we allocated or otherwise opened
                std::map<ID3D11Texture2D*, RenderBufferATWInfo>::iterator i;
                for (i = mBufferMap.begin(); i != mBufferMap.end(); i++) {
                  i->second.atwBuffer.D3D11->colorBuffer->Release();
                  // We don't release the colorBufferView because we didn't create one.
                  if (i->second.textureCopy != nullptr) {
                    i->second.textureCopy->Release();
                  }
                }
            }

            OpenResults OpenDisplay() override {
                std::lock_guard<std::mutex> lock(mMutex);

                OpenResults ret;

                // Set the device and context we're going to use
                // but don't open an additional display -- we're
                // going to pass all display-related things down to
                // our render thread.
				m_log->info() << "RenderManagerD3D11ATW::OpenDisplay: Calling SetDeviceAndContext()";
				if (!SetDeviceAndContext()) {
                    m_log->error() << "RenderManagerD3D11ATW::OpenDisplay: Could not "
                                      "create device and context";
                    ret.status = OpenStatus::FAILURE;
                    return ret;
                }

                // Try to open the display in the harnessed
                // RenderManager.  Return false if it fails.
                if (!mRenderManager) {
                  m_log->error() << "RenderManagerD3D11ATW::OpenDisplay: No "
                    "harnessed RenderManager";
                  ret.status = OpenStatus::FAILURE;
                  return ret;
                }
				m_log->info() << "RenderManagerD3D11ATW::OpenDisplay: Calling harnessed OpenDisplay()";
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
                // Start our ATW sub-thread.
                m_log->info("RenderManagerD3D11ATW: Starting ATW thread");
                start();

                //======================================================
                // Fill in our library, rather than that of
                // the harnessed RenderManager, since this is the one
                // that the client will deal with.
                ret.library = m_library;
                return ret;
            }

            // Calls the wrapped RenderManager's GetTimingInfo() call so that
            // the application can find out the frame rate info.  The other
            // entries will not be as useful, given that ATW is being used to
            // interpolate, but they are also returned.
            bool OSVR_RENDERMANAGER_EXPORT
              GetTimingInfo(size_t whichEye, OSVR_RenderTimingInfo& info) override {
                return mRenderManager->GetTimingInfo(whichEye, info);
              };

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
                // @todo Enable overlapped rendering on one frame while presentation
                // of the previous by doing this waiting on another thread.
                WaitForRenderCompletion();

                { // Adding block to scope the lock_guard.
                  // Lock our mutex so we don't adjust the buffers while rendering is happening.
                  // This lock is automatically released when we're done with this function.
                  std::lock_guard<std::mutex> lock(mMutex);
                  HRESULT hr;

                  mNextFrameInfo.colorBuffers.clear();

                  // If we have non-NULL texture-copy pointers in any of the buffers
                  // associated with the presented buffers, copy the texture into
                  // the associated buffer.  This is to handle the case where the client
                  // did not promise not to overwrite the texture before it is presented.
                  for (size_t i = 0; i < renderBuffers.size(); i++) {
                    auto key = renderBuffers[i].D3D11->colorBuffer;
                    auto bufferInfoItr = mBufferMap.find(key);
                    if (bufferInfoItr == mBufferMap.end()) {
                      m_log->error() << "RenderManagerD3D11ATW::PresentRenderBuffersInternal "
                        << "Could not find buffer info for RenderBuffer " << (size_t)key;
                      m_log->error() << "  (Be sure to register buffers before presenting them)";
                      setDoingOkay(false);
                      return false;
                    }

                    if (bufferInfoItr->second.textureCopy != nullptr) {
                      // If we've already copied this buffer as part of an earlier
                      // renderBuffer, then skip copying it this time.
                      bool alreadyCopied = false;
                      for (size_t j = 0; j < i; j++) {
                        if (renderBuffers[j].D3D11->colorBuffer ==
                          renderBuffers[i].D3D11->colorBuffer) {
                          alreadyCopied = true;
                        }
                      }
                      if (alreadyCopied) {
                        continue;
                      }

                      // Lock the mutex, copy, and then release it.
                      IDXGIKeyedMutex* mutex = nullptr;
                      hr = bufferInfoItr->second.textureCopy->QueryInterface(__uuidof(IDXGIKeyedMutex), (LPVOID*)&mutex);
                      if (!FAILED(hr) && (mutex != nullptr)) {
                        hr = mutex->AcquireSync(0, 500); // ignore failure
                      }

                      m_D3D11Context->CopyResource(bufferInfoItr->second.textureCopy,
                        renderBuffers[i].D3D11->colorBuffer);

                      if (mutex) {
                        hr = mutex->ReleaseSync(0);
                      }
                    }
                  }

                  for (size_t i = 0; i < renderBuffers.size(); i++) {
                    mNextFrameInfo.colorBuffers.push_back(renderBuffers[i].D3D11->colorBuffer);
                  }
                  mNextFrameInfo.renderInfo = renderInfoUsed;
                  mNextFrameInfo.flipInY = flipInY;
                  mNextFrameInfo.renderParams = renderParams;
                  mNextFrameInfo.normalizedCroppingViewports = normalizedCroppingViewports;
                  mFirstFramePresented = true;
				  mNextFrameAvailable = true;
                }

				//m_log->info() << "RenderManagerD3D11ATW::PresentFrameInternal: Queued next frame info, waiting for it to be presented...";
				{
					std::unique_lock<std::mutex> lock(mMutex);
					mPresentFinishedCV.wait(lock, [this] { return !mNextFrameAvailable; });
				}
				//m_log->info() << "RenderManagerD3D11ATW::PresentFrameInternal: Finished waiting for the frame to be presented.";

                return true;
            }

            void start() {
                if (mStarted) {
                    m_log->error() << "RenderManagerThread::start() - thread loop already started.";
                } else {
                    mThread.reset(new std::thread(std::bind(&RenderManagerD3D11ATW::threadFunc, this)));
                    // Set the scheduling priority of this thread to time-critical.
#ifdef _WIN32
                    HANDLE h = mThread->native_handle();
                    if (!SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL)) {
                      m_log->error() << "RenderManagerD3D11ATW::start():"
                        " Could not set ATW thread priority";
                    }
#endif
                }
                mStarted = true;
            }

            void stop() {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mStarted) {
                    m_log->error() << "RenderManagerThread::stop() - thread loop not already started.";
                }
                mQuit = true;
            }

            bool getQuit() {
                std::lock_guard<std::mutex> lock(mMutex);
                return mQuit;
            }

            void threadFunc() {
                // Used to make sure we don't take too long to render
				m_log->info() << "RenderManagerD3D11ATW::threadFunc thread id: " << std::this_thread::get_id() << std::endl;
                struct timeval lastFrameTime = {};
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
                    OSVR_RenderTimingInfo timing;
                    double expectedFrameInterval = -1;
                    if (mRenderManager->GetTimingInfo(0, timing)) {

                        OSVR_TimeValue nextRetrace = timing.hardwareDisplayInterval;
                        osvrTimeValueDifference(&nextRetrace,
                            &timing.timeSincelastVerticalRetrace);
                        if (osvrTimeValueGreater(&threshold, &nextRetrace)) {
                            timeToPresent = true;
                        }
                        expectedFrameInterval = static_cast<double>(
                            timing.hardwareDisplayInterval.seconds +
                            timing.hardwareDisplayInterval.microseconds / 1e6);
                    } else {
                        //m_log->error() << "RenderManagerThread::threadFunc() = couldn't get timing info";

                        // if we can't get timing info, we're probably in extended mode.
                        // in this case, render as often as possible.
                        timeToPresent = true;
                    }

                    if (timeToPresent) {
                        // Lock our mutex so that we're not rendering while new buffers are
                        // being presented.
                        std::lock_guard<std::mutex> lock(mMutex);
                        if (mFirstFramePresented) {
                            // Update the context so we get our callbacks called and
                            // update tracker state, which will be read during the
                            // time-warp calculation in our harnessed RenderManager.
                            osvrClientUpdate(mRenderManager->m_context);

                            // make a new RenderBuffers array with the atw thread's buffers
                            std::vector<osvr::renderkit::RenderBuffer> atwRenderBuffers;
                            for (size_t i = 0; i < mNextFrameInfo.colorBuffers.size(); i++) {
                                auto key = mNextFrameInfo.colorBuffers[i];
                                auto bufferInfoItr = mBufferMap.find(key);
                                if (bufferInfoItr == mBufferMap.end()) {
                                    m_log->error() << "No buffer info for key " << (size_t)key;
                                    setDoingOkay(false);
                                    mQuit = true;
                                    break;
                                }

                                atwRenderBuffers.push_back(bufferInfoItr->second.atwBuffer);
                            }

							//m_log->info() << "RenderManagerD3D11ATW::threadFunc: presenting frame to internal backend.";

                            // Send the rendered results to the screen, using the
                            // RenderInfo that was handed to us by the client the last
                            // time they gave us some images.
                            if (!mRenderManager->PresentRenderBuffers(
                                atwRenderBuffers,
                                mNextFrameInfo.renderInfo,
                                mNextFrameInfo.renderParams,
                                mNextFrameInfo.normalizedCroppingViewports,
                                mNextFrameInfo.flipInY)) {
                                    /// @todo if this might be intentional (expected) - shouldn't be an error...
                                    m_log->error()
                                        << "PresentRenderBuffers() returned false, maybe because it was asked to quit";
                                    setDoingOkay(false);
                                    mQuit = true;
                            }

							//m_log->info() << "RenderManagerD3D11ATW::threadFunc: finished presenting frame to internal backend.";

                            struct timeval now;
                            vrpn_gettimeofday(&now, nullptr);
                            if (expectedFrameInterval >= 0 && lastFrameTime.tv_sec != 0) {
                                double frameInterval = vrpn_TimevalDurationSeconds(now, lastFrameTime);
                                if (frameInterval > expectedFrameInterval * 1.9) {
                                    m_log->info() << "RenderManagerThread::threadFunc(): Missed"
                                        " 1+ frame at " << iteration <<
                                        ", expected interval " << expectedFrameInterval * 1e3
                                        << "ms but got " << frameInterval * 1e3;
                                    m_log->info() << "  (PresentRenderBuffers took "
                                        << mRenderManager->timePresentRenderBuffers * 1e3
                                        << "ms)";
                                    m_log->info() << "  (FrameInit "
                                        << mRenderManager->timePresentFrameInitialize * 1e3
                                        << ", WaitForSync "
                                        << mRenderManager->timeWaitForSync * 1e3
                                        << ", DisplayInit "
                                        << mRenderManager->timePresentDisplayInitialize * 1e3
                                        << ", PresentEye "
                                        << mRenderManager->timePresentEye * 1e3
                                        << ", DisplayFinal "
                                        << mRenderManager->timePresentDisplayFinalize * 1e3
                                        << ", FrameFinal "
                                        << mRenderManager->timePresentFrameFinalize * 1e3
                                        << ")";
                                }
                            }
                            lastFrameTime = now;

                            iteration++;

							mNextFrameAvailable = false;
                        }
						mPresentFinishedCV.notify_all();
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

            bool SolidColorEye(size_t eye, const RGBColorf &color) override {
              std::lock_guard<std::mutex> lock(mMutex);
              // Stop the rendering thread from overwriting with warped
              // versions of the most recently presented buffers.
              mFirstFramePresented = false;
              return mRenderManager->SolidColorEye(eye, color);
            }

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

                    if (!appWillNotOverwriteBeforeNewPresent) {
                      // The application is not maintaining two sets of buffers, so we'll
                      // need to make a copy of this texture when it is presented.  Here
                      // we allocate a place to put it.  We have to allocate a shared
                      // resource, so it can be used by both threads.  It is allocated on
                      // the render thread's device.  We need to introspect the texture
                      // to find its size and we need to make sure that we don't make
                      // two copies of the same buffer.

                      // If we already have a mapping for this buffer (from an earlier
                      // registration or because they registered the same buffer more than
                      // once), delete the earlier mapping.
                      auto existing = mBufferMap.find(buffers[i].D3D11->colorBuffer);
                      if ((existing != mBufferMap.end() &&
                          (mBufferMap[buffers[i].D3D11->colorBuffer].textureCopy !=
                           nullptr)) ) {
                        mBufferMap[buffers[i].D3D11->colorBuffer].textureCopy->Release();
                      }

                      // Construct the new texture that is to be used for the copy.
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
                      ID3D11Texture2D* textureCopy = nullptr;
                      hr = m_D3D11device->CreateTexture2D(&textureDesc, nullptr, &textureCopy);
                      if (FAILED(hr)) {
                          m_log->error() << "RenderManagerD3D11ATW::"
                                         << "RegisterRenderBuffersInternal: - Can't create copy texture for buffer "
                                         << i;
                          setDoingOkay(false);
                          return false;
                      }

                      // Record the place to copy incoming textures to.
                      newInfo.textureCopy = textureCopy;
                    }

                    // If we made a new texture, we get our info from it.  Otherwise,
                    // from the original texture.
                    ID3D11Texture2D *textureToMap = buffers[i].D3D11->colorBuffer;
                    if (newInfo.textureCopy) {
                      textureToMap = newInfo.textureCopy;
                    }

                    // we need to get the shared resource HANDLE for the ID3D11Texture2D, but in order to
                    // get that, we need to get the IDXGIResource* first.
                    ID3D11Texture2D *texture2D = nullptr;
                    IDXGIResource* dxgiResource = nullptr;
                    hr = textureToMap->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&dxgiResource);
                    if (FAILED(hr)) {
                      m_log->error()
                        << "RenderManagerD3D11ATW::"
                        << "RegisterRenderBuffersInternal: Can't get the IDXGIResource for the texture resource.";
                      setDoingOkay(false);
                      return false;
                    }

                    // now get the shared HANDLE
                    hr = dxgiResource->GetSharedHandle(&newInfo.sharedResourceHandle);
                    if (FAILED(hr)) {
                      m_log->error()
                        << "RenderManagerD3D11ATW::"
                        << "RegisterRenderBuffersInternal: Can't get the shared handle from the dxgiResource.";
                      setDoingOkay(false);
                      return false;
                    }
                    dxgiResource->Release(); // we don't need this anymore

                                             // Get a pointer on our device to the texture we're getting
                                             // from the caller's device.
                    hr = atwDevice->OpenSharedResource(newInfo.sharedResourceHandle, __uuidof(ID3D11Texture2D),
                      (LPVOID*)&texture2D);
                    if (FAILED(hr)) {
                      m_log->error() << "RenderManagerD3D11ATW::"
                        << "RegisterRenderBuffersInternal: - failed to open shared resource "
                        << "for buffer " << i;
                      setDoingOkay(false);
                      return false;
                    }

                    // We do not need a render target view for the ATW thread -- it will
                    // only be reading from the buffer, not rendering into it.  Our base
                    // class will create our RenderTargetView the first time the app calls
                    // Render().
                    newInfo.atwBuffer.D3D11 = new osvr::renderkit::RenderBufferD3D11();
                    newInfo.atwBuffer.D3D11->colorBuffer = texture2D;
                    newInfo.atwBuffer.D3D11->colorBufferView = nullptr; // We don't need this.
                    newInfo.atwBuffer.D3D11->depthStencilBuffer = nullptr;
                    newInfo.atwBuffer.D3D11->depthStencilView = nullptr;
                    renderBuffers.push_back(newInfo.atwBuffer);
                  }
                  { // Adding block to scope the lock_guard.
                    // Lock our mutex so that we're not rendering while new buffers are
                    // being added or old ones modified.
                    std::lock_guard<std::mutex> lock(mMutex);
                    mBufferMap[buffers[i].D3D11->colorBuffer] = newInfo;
                  }
                }

                if (!mRenderManager->RegisterRenderBuffers(renderBuffers,
                    appWillNotOverwriteBeforeNewPresent)) {
                    m_log->error() << "RenderManagerD3D11ATW::"
                                   << "RegisterRenderBuffersInternal: Could not Register render"
                                   << " buffers on harnessed RenderManager";
                    setDoingOkay(false);
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
