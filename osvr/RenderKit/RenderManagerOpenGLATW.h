/** @file
    @brief Header

    @date 2017

    @author Jeremy Bell
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2017 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_RenderManagerOpenGLATW_h_GUID_47EFB3F7_EA50_4C9A_EC19_7C7DDCA0A243
#define INCLUDED_RenderManagerOpenGLATW_h_GUID_47EFB3F7_EA50_4C9A_EC19_7C7DDCA0A243

#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>

#include <osvr/Util/Logger.h>

#include "RenderManagerOpenGL.h"
#include "GraphicsLibraryOpenGL.h"

#include <EGL/egl.h>

#include <vrpn_Shared.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <map>
#include <set>

#if !_WIN32
#include <pthread.h>
#endif

namespace osvr {
namespace renderkit {

    class RenderManagerOpenGLATW : public RenderManagerOpenGL {
      private:

        // EGL extension definitions (use a _ suffix if you add more, so they don't conflict
        // with the official ones).
        const static GLuint EGL_CONTEXT_PRIORITY_LEVEL_IMG_ = 0x3100;
        const static GLuint EGL_CONTEXT_PRIORITY_HIGH_IMG_ = 0x3101;
        const static GLuint EGL_CONTEXT_PRIORITY_MEDIUM_IMG_ = 0x3102;
        const static GLuint EGL_CONTEXT_PRIORITY_LOW_IMG_ = 0x3103;

        const static GLuint EGL_SYNC_FLUSH_COMMANDS_BIT_KHR_ = 0x0001;
        const static GLuint EGL_TIMEOUT_EXPIRED_KHR_ = 0x30F5;
        const static GLuint EGL_SYNC_FENCE_KHR_ = 0x30F9;

        typedef void* EGLSyncKHR_;
        typedef uint64_t EGLTimeKHR_;
        const static EGLTimeKHR_ EGL_FOREVER_KHR_ = 0xFFFFFFFFFFFFFFFFull;

        typedef EGLSyncKHR_ (*eglCreateSyncKHRFunc)(
            EGLDisplay dpy,
            EGLenum type,
            const EGLint *attrib_list);
            eglCreateSyncKHRFunc eglCreateSyncKHR_;

        typedef EGLBoolean (*eglDestroySyncKHRFunc)(
            EGLDisplay dpy,
            EGLSyncKHR_ sync);
            eglDestroySyncKHRFunc eglDestroySyncKHR_;

        typedef EGLint (*eglClientWaitSyncKHRFunc)(
            EGLDisplay dpy,
            EGLSyncKHR_ sync,
            EGLint flags,
            EGLTimeKHR_ timeout);
            eglClientWaitSyncKHRFunc eglClientWaitSyncKHR_;

        constexpr static EGLSyncKHR_ EGL_NO_SYNC_KHR_ = ((EGLSyncKHR_)0);

        bool mEGLFenceExtensionAvailable = false;

        bool IsEGLExtensionSupported(const std::string& extensionName) {
            const char* extensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
            if(!extensions) {
                m_log->error() << "RenderManagerOpenGLATW::IsEGLExtensionSupported: eglQueryString failed. Maybe there is a problem with the current EGLDisplay?";
                return false;
            }

            std::string extensionsStr(extensions);
            return extensionsStr.find(extensionName) != std::string::npos;
        }

        bool LoadEGLExtensions() {
            if(mEGLFenceExtensionAvailable) {
                // We've already loaded extensions, quit.
                m_log->info() << "RenderManagerOpenGLATW::LoadEGLExtensions: extensions already loaded.";
                return true;
            }
            m_log->info() << "RenderManagerOpenGLATW::LoadEGLExtensions: Loading EGL extensions.";
            mEGLFenceExtensionAvailable = IsEGLExtensionSupported("EGL_KHR_fence_sync");
            if(!mEGLFenceExtensionAvailable) {
                m_log->error() << "RenderManagerOpenGLATW::LoadEGLExtensions: EGL_KHR_fence_sync extension is not available.";
            } else {
                m_log->info() << "RenderManagerOpenGLATW::LoadEGLExtensions: SyncKHR extension supported. Loading procs...";

                if(!eglCreateSyncKHR_) {
                    eglCreateSyncKHR_ = reinterpret_cast<eglCreateSyncKHRFunc>(
                        eglGetProcAddress("eglCreateSyncKHR"));

                    if(!eglCreateSyncKHR_) {
                        m_log->error() << "RenderManagerOpenGLATW::LoadEGLExtensions: could not load eglCreateSyncKHR proc";
                    }
                }

                if(!eglDestroySyncKHR_) {
                    eglDestroySyncKHR_ = reinterpret_cast<eglDestroySyncKHRFunc>(
                        eglGetProcAddress("eglDestroySyncKHR"));

                    if(!eglDestroySyncKHR_) {
                        m_log->error() << "RenderManagerOpenGLATW::LoadEGLExtensions: could not load eglDestroySyncKHR proc";
                    }
                }

                if(!eglClientWaitSyncKHR_) {
                    eglClientWaitSyncKHR_ = reinterpret_cast<eglClientWaitSyncKHRFunc>(
                        eglGetProcAddress("eglClientWaitSyncKHR"));
                    if(!eglClientWaitSyncKHR_) {
                        m_log->error() << "RenderManagerOpenGLATW::LoadEGLExtensions: could not load the eglClientWaitSyncKHR proc";
                    }
                }

                mEGLFenceExtensionAvailable =
                    eglCreateSyncKHR_ &&
                    eglDestroySyncKHR_ &&
                    eglClientWaitSyncKHR_;

                if(mEGLFenceExtensionAvailable) {
                    m_log->info() << "RenderManagerOpenGLATW::LoadEGLExtensions: fence extension supported and procs loaded successfully!";
                } else {
                    m_log->error() << "RenderManagerOpenGLATW::LoadEGLExtensions: fence extension not supported or procs failed to load.";
                }
            }
            return mEGLFenceExtensionAvailable;
        }

        /// Holds the information needed to handle locking and unlocking of
        /// buffers and also the copying of buffers in the case where we have
        /// our own internal copy.
        // typedef struct RenderBufferATWInfo {
        //     osvr::renderkit::RenderBuffer atwBuffer;
        //     GLuint textureCopy; ///< nullptr if no copy needed.
        // } RenderBufferATWInfo;
        //std::map<GLuint, RenderBufferATWInfo> mBufferMap;

        // Our RegisterRenderBuffersInternal does nothing but forward
        // the arguments to the ATW thread.
        std::vector<RenderBuffer> mRegisteredRenderBuffers;
        bool mAppWillNotOverwriteBeforeNewPresent;

        std::mutex mMutex;
        std::condition_variable mThreadInitializedCV;
        std::condition_variable mPresentFinishedCV;
        std::shared_ptr<std::thread> mThread = nullptr;

        // We harness a D3D11 DirectMode renderer to do our
        // DirectMode work and to handle the timing.
        RenderManagerOpenGL* mRenderManager = nullptr;

        EGLDisplay mDisplay = 0;
        EGLSurface mSurface = 0;
        EGLSurface mPlaceholderPbufferSurface = 0;
        EGLContext mRenderContext = 0;
        EGLContext mATWThreadContext = 0;
        EGLConfig mConfig = 0;
        EGLSyncKHR_ mFenceSync = nullptr;

        /// Holds information about the buffers to be used by the next rendering
        /// pass.  This is filled in by PresentRenderBuffersInternal() and used
        /// by the ATW thread.  Access should be guarded using the mMutex to prevent
        /// simultaneous use in both threads.
        struct {
            std::vector<osvr::renderkit::RenderBufferOpenGL> renderBuffers;
            std::vector<osvr::renderkit::RenderInfo> renderInfo;
            std::vector<OSVR_ViewportDescription> normalizedCroppingViewports;
            RenderParams renderParams;
            bool flipInY;
        } mNextFrameInfo;
        bool mNextFrameAvailable = false;

        bool mQuit = false;
        bool mStarted = false;
        bool mFirstFramePresented = false;
        bool mATWThreadInitializationAttempted = false;
        bool mATWThreadInitialized = false;

      public:
        /**
        * Construct an D3D ATW wrapper around an existing D3D render
        * manager. Takes ownership of the passed in render manager
        * and deletes it when the wrapper is deleted.
        */
        RenderManagerOpenGLATW(OSVR_ClientContext context, ConstructorParameters p)
            : RenderManagerOpenGL(context, p) {
            // mRenderManager.reset(renderManagerToHarness);
        }

        virtual ~RenderManagerOpenGLATW() {
            if (mThread) {
                stop();
                mThread->join();
            }
            // Delete textures and views that we allocated or otherwise opened
            // std::map<GLuint, RenderBufferATWInfo>::iterator i;
            // for (i = mBufferMap.begin(); i != mBufferMap.end(); i++) {
            // TODO: delete resources in i->second.atwBuffer.OpenGL

            //   i->second.atwBuffer.D3D11->colorBuffer->Release();
            //   // We don't release the colorBufferView because we didn't create one.
            //   if (i->second.textureCopy != nullptr) {
            //     i->second.textureCopy->Release();
            //   }
            // }

            if (eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
                m_log->error()
                    << "RenderManagerOpenGLATW::~RenderManagerOpenGLATW: failed to make the temporary pbuffer current.";
            } else {
                if (eglDestroySurface(mDisplay, mPlaceholderPbufferSurface) == EGL_FALSE) {
                    m_log->error()
                        << "RenderManagerOpenGLATW::~RenderManagerOpenGLATW: failed to destroy temporary pbuffer.";
                }
            }

            // mATWThreadContext = 0;
        }

        OpenResults OpenDisplay() override {
            std::unique_lock<std::mutex> lock(mMutex);

            OpenResults ret;

            // Assuming OpenDisplay is called on the render thread with
            // a current context
            mDisplay = eglGetCurrentDisplay();
            if (mDisplay == EGL_NO_DISPLAY) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: no current "
                               << "EGLDisplay.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            mSurface = eglGetCurrentSurface(EGL_DRAW);
            if (mSurface == EGL_NO_SURFACE) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: no current "
                               << "EGLSurface.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            mRenderContext = eglGetCurrentContext();
            if (mRenderContext == EGL_NO_CONTEXT) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: no current "
                               << "EGLContext.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            EGLint configID;
            if (!eglQueryContext(mDisplay, mRenderContext, EGL_CONFIG_ID, &configID)) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: couldn't get EGL_CONFIG_ID"
                               << " of the current display and context.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            EGLConfig configList[1024];
            EGLint numConfigs = 0;
            if (eglGetConfigs(mDisplay, configList, 1024, &numConfigs) == EGL_FALSE) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: couldn't get "
                               << "the EGLConfigs for the current display.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            for (int i = 0; i < numConfigs; i++) {
                EGLint curConfigID = 0;
                eglGetConfigAttrib(mDisplay, configList[i], EGL_CONFIG_ID, &curConfigID);
                if (curConfigID == configID) {
                    mConfig = configList[i];
                    break;
                }
            }

            if (mConfig == nullptr) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: couldn't find an EGLConfig for "
                               << "config ID " << configID;
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            if(!LoadEGLExtensions()) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: Failed to load EGL extensions.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            const EGLint placeholderPbufferSurfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
            mPlaceholderPbufferSurface = eglCreatePbufferSurface(mDisplay, mConfig, placeholderPbufferSurfaceAttribs);

            // Set the device and context we're going to use
            // but don't open an additional display -- we're
            // going to pass all display-related things down to
            // our render thread.
            m_log->info() << "RenderManagerOpenGLATW::OpenDisplay: Calling SetDeviceAndContext()";
            if (eglMakeCurrent(mDisplay, mPlaceholderPbufferSurface, mPlaceholderPbufferSurface, mRenderContext) == EGL_FALSE) {
                m_log->error() << "RenderManagerOpenGLATW::OpenDisplay: Could not "
                                  "set the temporary Pbuffer surface as current.";
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

            //======================================================
            // Start our ATW sub-thread.
            m_log->info("RenderManagerOpenGLATW::OpenDisplay: Starting ATW thread and waiting for it to initialize.");
            start();

            // Wait for the ATW thread to initialize its own resources and
            // call OpenDisplay on the harnessed RenderManagerOpenGL
            mThreadInitializedCV.wait(lock, [this] { return mATWThreadInitialized; });
            m_log->info(
                "RenderManagerOpenGLATW::OpenDisplay: Finished waiting for ATW thread to initialize. Returning...");

            // if not, some failure happened during ATW thread initialization
            if (!mATWThreadInitialized) {
                m_log->error("RenderManagerOpenGLATW::OpenDisplay: ATW thread initialization failed.");
                ret.status = OpenStatus::FAILURE;
                return ret;
            }

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
        bool OSVR_RENDERMANAGER_EXPORT GetTimingInfo(size_t whichEye, OSVR_RenderTimingInfo& info) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mRenderManager) {
                m_log->error() << "RenderManagerOpenGLATW::GetTimingInfo: attempted to call before internal "
                                  "RenderManager is created.";
                return RenderManagerOpenGL::GetTimingInfo(whichEye, info);
            }
            return mRenderManager->GetTimingInfo(whichEye, info);
        };

        //===================================================================
        // Overloaded render functions required from the base class.
        bool PresentFrameFinalize() override { return true; }
        bool PresentDisplayFinalize(size_t display) override { return true; }
        bool RenderEyeFinalize(size_t eye) override { return true; }

        bool PresentEye(PresentEyeParameters params) override { return true; }

        bool RenderFrameInitialize() override { return true; }
        bool RenderDisplayInitialize(size_t display) override { return true; }
        bool RenderEyeInitialize(size_t eye) override { return true; }

        bool PresentDisplayInitialize(size_t display) override { return true; }

      protected:
        bool PresentRenderBuffersInternal(const std::vector<RenderBuffer>& renderBuffers,
                                          const std::vector<RenderInfo>& renderInfoUsed,
                                          const RenderParams& renderParams = RenderParams(),
                                          const std::vector<OSVR_ViewportDescription>& normalizedCroppingViewports =
                                              std::vector<OSVR_ViewportDescription>(),
                                          bool flipInY = false) override {

            if (!m_renderBuffersRegistered) {
                m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: "
                               << "Render buffers not yet registered, ignoring present request.";
                return true;
            }

#if 1
            glFinish();
#else
            // This code is disabled because of a potential bug in the Tegra drivers with eglClientWaitSync
            // crashing. For now, using a glFinish()
            if(this->mEGLFenceExtensionAvailable && eglGetCurrentContext() != EGL_NO_CONTEXT) {
                std::lock_guard<std::mutex> lock(mMutex);
                if(mFenceSync) {
                    m_log->info() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglDestroySyncKHR";
                    if(eglDestroySyncKHR_ && eglDestroySyncKHR_(mDisplay, mFenceSync) == EGL_FALSE) {
                        m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglDestroySyncKHR return EGL_FALSE. "
                            << "Maybe something is wrong with the extension support on this platform?";
                    // ignore failure for now, but maybe we need to find
                    // some way to drop back to non-ATW when we can't load the sync
                    // and high-priority context extensions.
                    } else {
                        m_log->info() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: destroyed the previous SyncKHR";
                    }
                    mFenceSync = nullptr;
                }

                if(eglCreateSyncKHR_) {
                    m_log->info() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglCreateSyncKHR";
                    mFenceSync = eglCreateSyncKHR_(mDisplay, EGL_SYNC_FENCE_KHR_, nullptr);
                    if(mFenceSync == EGL_NO_SYNC_KHR_) {
                        m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglCreateSyncKHR returned EGL_NO_SYNC_KHR. "
                            << "Maybe something is wrong with the extension support on this platform?";
                        // ignore failure for now, but maybe we need to find
                        // some way to drop back to non-ATW when we can't load the sync
                        // and high-priority context extensions.
                    } else {
                        m_log->info() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: created a new SyncKHR";
                    }
                } else {
                    m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglCreateSyncKHR not initialized.";
                }

                if(eglClientWaitSyncKHR_) {
                    m_log->info() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglClientWaitSyncKHR";
                    //const EGLTimeKHR_ tenMs = 1e+7;
                    EGLint waitResult = eglClientWaitSyncKHR_(mDisplay, mFenceSync,
                        EGL_SYNC_FLUSH_COMMANDS_BIT_KHR_, EGL_FOREVER_KHR_);

                    if(waitResult == EGL_TIMEOUT_EXPIRED_KHR_) {
                        m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: got an EGL_TIMEOUT_EXPIRED_KHR when waiting for the sync fence.";
                    } else if(waitResult == EGL_FALSE) {
                        m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: got an EGL_FALSE returned from eglClientWaitSyncKHR. Something might be wrong.";
                    } else {
                        m_log->info() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: successfully waited on mFenceSync";
                    }
                } else {
                    m_log->error() << "RenderManagerOpenGLATW::PresentRenderBuffersInternal: eglClientWaitSyncKHR_ not initialized";
                }
            }

#endif

            { // Adding block to scope the lock_guard.
                // Lock our mutex so we don't adjust the buffers while rendering is happening.
                // This lock is automatically released when we're done with this function.
                std::lock_guard<std::mutex> lock(mMutex);
                mNextFrameInfo.renderBuffers.clear();
                for(size_t i = 0; i < renderBuffers.size(); i++) {
                    if(renderBuffers[i].OpenGL) {
                        mNextFrameInfo.renderBuffers.push_back(*renderBuffers[i].OpenGL);
                    }
                }

                mNextFrameInfo.renderInfo = renderInfoUsed;
                mNextFrameInfo.flipInY = flipInY;
                mNextFrameInfo.renderParams = renderParams;
                mNextFrameInfo.normalizedCroppingViewports = normalizedCroppingViewports;
                mFirstFramePresented = true;
                mNextFrameAvailable = true;
            }

            //m_log->info() << "RenderManagerOpenGLATW::PresentFrameInternal: Queued next frame info, waiting for it to be presented...";
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mPresentFinishedCV.wait(lock, [this] { return !mNextFrameAvailable; });
            }
            //m_log->info() << "RenderManagerOpenGLATW::PresentFrameInternal: Finished waiting for the frame to be presented.";

            return true;
        }

        void start() {
            if (mStarted) {
                m_log->error() << "RenderManagerThread::start() - thread loop already started.";
            } else {
                mThread.reset(new std::thread(std::bind(&RenderManagerOpenGLATW::threadFunc, this)));
// Set the scheduling priority of this thread to time-critical.
#ifdef _WIN32
                HANDLE h = mThread->native_handle();
                if (!SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL)) {
                    m_log->error() << "RenderManagerOpenGLATW::start():"
                                      " Could not set ATW thread priority";
                }
#else
                // This does NOT currently work on Android because you have
                // to be root for it to work, and apps won't be rooted.

                // sched_param sch;
                // int policy;
                // pthread_getschedparam(mThread->native_handle(), &policy, &sch);
                // sch.sched_priority = 20;
                // int setschedRC = pthread_setschedparam(mThread->native_handle(), SCHED_FIFO, &sch);
                // if (setschedRC) {
                //     switch(setschedRC) {
                //         case ESRCH:
                //             m_log->error() << "RenderManagerOpenGLATW::start: got ESRCH from pthread_setschedparam";
                //             break;
                //         case EINVAL:
                //             m_log->error() << "RenderManagerOpenGLATW::start: got EINVAL from pthread_setschedparam";
                //             break;
                //         case EPERM:
                //             m_log->error() << "RenderManagerOpenGLATW::start: got EPERM from pthread_setschedparam";
                //             break;
                //         default:
                //             m_log->error() << "RenderManagerOpenGLATW::start: got unknown error from pthread_setschedparam: "
                //                 << std::strerror(errno);
                //             break;
                //     }
                // }
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
            struct timeval lastFrameTime = {};
            bool quit = getQuit();
            size_t iteration = 0;

            EGLint atwContextAttribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL_CONTEXT_PRIORITY_LEVEL_IMG_, EGL_CONTEXT_PRIORITY_HIGH_IMG_,
                EGL_NONE
            };

            {
                std::lock_guard<std::mutex> lock(mMutex);
                mATWThreadInitialized = true;

                mATWThreadContext = eglCreateContext(mDisplay, mConfig, mRenderContext, atwContextAttribs);
                if (mATWThreadContext == EGL_NO_CONTEXT) {
                    m_log->error() << "RenderManagerOpenGLATW::threadFunc: "
                                   << "could not create ATW thread EGL Context";
                    mATWThreadInitialized = false;
                }

                if (eglMakeCurrent(mDisplay, mSurface, mSurface, mATWThreadContext) == EGL_FALSE) {
                    m_log->error() << "RenderManagerOpenGLATW::threadFunc: "
                                   << "could not make the ATW thread context current.";
                    mATWThreadInitialized = false;
                }

                m_log->info() << "RenderManagerOpenGLATW::threadFunc: Creating harnessed RenderManagerOpenGL";
                ConstructorParameters atwParams = m_params;
                atwParams.m_verticalSync = false;
                atwParams.m_verticalSyncBlocksRendering = false;
                atwParams.m_maxMSBeforeVsyncTimeWarp = 0.0f;

                mRenderManager = new RenderManagerOpenGL(m_context, atwParams);
                mRenderManager->m_storeClientGLState = false;

                m_log->info() << "RenderManagerOpenGLATW::threadFunc: Calling harnessed OpenDisplay()";
                auto ret = mRenderManager->OpenDisplay();
                if (ret.status == OpenStatus::FAILURE) {
                    m_log->error() << "RenderManagerOpenGLATW::threadFunc: Could not "
                                      "open display in harnessed RenderManager";
                    mATWThreadInitialized = false;
                }

                mATWThreadInitializationAttempted = true;
            }

            // signal the main thread that we are done initializing
            mThreadInitializedCV.notify_all();

            if (!mATWThreadInitialized) {
                m_log->error() << "RenderManagerOpenGLATW::threadFunc: ATW thread initialization "
                               << "failed. Exiting ATW thread...";
                quit = true;
            }

            bool harnessedRenderBuffersRegistered = false;
            while (!quit) {
                
                // Wait until it is time to present the render buffers.
                // If we've got a specified maximum time before vsync,
                // we use that.  Otherwise, we set the threshold to 1ms
                // to give us some time to swap things out before vsync.
                bool timeToPresent = false;

                // Convert from milliseconds to seconds
                float thresholdF = m_params.m_maxMSBeforeVsyncTimeWarp / 1e3f;
                if (thresholdF == 0) {
                    thresholdF = 1e-3f;
                }
                OSVR_TimeValue threshold;
                threshold.seconds = static_cast<OSVR_TimeValue_Seconds>(thresholdF);
                thresholdF -= threshold.seconds;
                threshold.microseconds = static_cast<OSVR_TimeValue_Microseconds>(thresholdF * 1e6);

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
                    osvrTimeValueDifference(&nextRetrace, &timing.timeSincelastVerticalRetrace);
                    if (osvrTimeValueGreater(&threshold, &nextRetrace)) {
                        timeToPresent = true;
                    }
                    expectedFrameInterval = static_cast<double>(timing.hardwareDisplayInterval.seconds +
                                                                timing.hardwareDisplayInterval.microseconds / 1e6);
                } else {
                    // if we can't get timing info, we're probably in extended mode.
                    // in this case, render as often as possible.
                    timeToPresent = true;
                }

                if (timeToPresent) {

                    // eglClientWaitSyncKHR is broken on Tegra, causes a signal 7,
                    // disable this for now and do a glFlush instead.
                    glFlush();
                    // {
                    //     std::lock_guard<std::mutex> lock(mMutex);
                    //     if(mFenceSync && eglGetCurrentContext() != EGL_NO_CONTEXT) {
                    //         m_log->info() << "RenderManagerOpenGLATW::threadFunc: eglClientWaitSyncKHR";
                    //         EGLint waitResult = eglClientWaitSyncKHR_(mDisplay, mFenceSync,
                    //             EGL_SYNC_FLUSH_COMMANDS_BIT_KHR_, EGL_FOREVER_KHR_);

                    //         if(waitResult == EGL_TIMEOUT_EXPIRED_KHR_) {
                    //             m_log->info() << "RenderManagerOpenGLATW::threadFunc: got an EGL_TIMEOUT_EXPIRED_KHR when waiting for the sync fence.";
                    //         }
                    //         if(waitResult == EGL_FALSE) {
                    //             m_log->info() << "RenderManagerOpenGLATW::threadFunc: got an EGL_FALSE returned from eglClientWaitSyncKHR. Something might be wrong.";
                    //         }
                    //     }
                    // }
                    {
                        // Lock our mutex so that we're not rendering while new buffers are
                        // being presented.
                        std::lock_guard<std::mutex> lock(mMutex);

                        if(!m_renderBuffersRegistered) {
                            continue;
                        }

                        if(!harnessedRenderBuffersRegistered) {
                            m_log->info() << "RenderManagerOpenGLATW::threadFunc: "
                                          << "Registering render buffers to the harnessed "
                                          << "RenderManagerOpenGL";

                            if (!mRenderManager->RegisterRenderBuffers(
                                mRegisteredRenderBuffers, mAppWillNotOverwriteBeforeNewPresent)) {
                                m_log->error() << "RenderManagerOpenGLATW::threadFunc: "
                                               << "Could not Register render "
                                               << "buffers on harnessed RenderManager";
                                setDoingOkay(false);
                                mQuit = true;
                            } else {
                                harnessedRenderBuffersRegistered = true;
                            }
                        }

                        if (mFirstFramePresented && harnessedRenderBuffersRegistered) {
                            // Update the context so we get our callbacks called and
                            // update tracker state, which will be read during the
                            // time-warp calculation in our harnessed RenderManager.
                            osvrClientUpdate(mRenderManager->m_context);

                            //m_log->info() << "RenderManagerOpenGLATW::threadFunc: presenting frame to internal backend.";

                            std::vector<osvr::renderkit::RenderBuffer> renderBuffers;
                            for(size_t i = 0; i < mNextFrameInfo.renderBuffers.size(); i++) {
                                osvr::renderkit::RenderBuffer buffer;
                                buffer.OpenGL = new osvr::renderkit::RenderBufferOpenGL();
                                buffer.OpenGL->colorBufferName = mNextFrameInfo.renderBuffers[i].colorBufferName;
                                buffer.OpenGL->depthStencilBufferName = mNextFrameInfo.renderBuffers[i].depthStencilBufferName;
                                renderBuffers.push_back(buffer);
                            }

                            // Send the rendered results to the screen, using the
                            // RenderInfo that was handed to us by the client the last
                            // time they gave us some images.
                            if (!mRenderManager->PresentRenderBuffers(
                                    renderBuffers, mNextFrameInfo.renderInfo, mNextFrameInfo.renderParams,
                                    mNextFrameInfo.normalizedCroppingViewports, mNextFrameInfo.flipInY)) {
                                /// @todo if this might be intentional (expected) - shouldn't be an error...
                                m_log->error()
                                    << "PresentRenderBuffers() returned false, maybe because it was asked to quit";
                                setDoingOkay(false);
                                mQuit = true;
                            }

                            for(size_t i = 0; i < renderBuffers.size(); i++) {
                                delete renderBuffers[i].OpenGL;
                            }

                            struct timeval now;
                            vrpn_gettimeofday(&now, nullptr);
                            if (expectedFrameInterval >= 0 && lastFrameTime.tv_sec != 0) {
                                double frameInterval = vrpn_TimevalDurationSeconds(now, lastFrameTime);
                                if (frameInterval > expectedFrameInterval * 1.9) {
                                    m_log->info() << "RenderManagerThread::threadFunc(): Missed"
                                                    " 1+ frame at "
                                                << iteration << ", expected interval " << expectedFrameInterval * 1e3
                                                << "ms but got " << frameInterval * 1e3;
                                    m_log->info() << "  (PresentRenderBuffers took "
                                                << mRenderManager->timePresentRenderBuffers * 1e3 << "ms)";
                                    m_log->info()
                                        << "  (FrameInit " << mRenderManager->timePresentFrameInitialize * 1e3
                                        << ", WaitForSync " << mRenderManager->timeWaitForSync * 1e3 << ", DisplayInit "
                                        << mRenderManager->timePresentDisplayInitialize * 1e3 << ", PresentEye "
                                        << mRenderManager->timePresentEye * 1e3 << ", DisplayFinal "
                                        << mRenderManager->timePresentDisplayFinalize * 1e3 << ", FrameFinal "
                                        << mRenderManager->timePresentFrameFinalize * 1e3 << ")";
                                }
                            }
                            lastFrameTime = now;

                            iteration++;

                            mNextFrameAvailable = false;
                        }
                    }
                    mPresentFinishedCV.notify_all();
                }

                quit = mQuit;
            }

            // RenderManagerOpenGL makes OpenGL calls in its destructor, so
            // it has to be deleted on the ATW thread.
            delete mRenderManager;
            mRenderManager = nullptr;

            // Detatch the ATW thread surface/context
            if (eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
                m_log->error() << "RenderManagerOpenGLATW::threadFunc: failed to detatch ATW thread context to current "
                                  "window surface.";
            }

            // Destroy the ATW thread mutex
            if (mATWThreadContext != 0 && mATWThreadContext != EGL_NO_CONTEXT &&
                eglDestroyContext(mDisplay, mATWThreadContext) == EGL_FALSE) {
                m_log->error() << "RenderManagerOpenGLATW::threadFunc: failed to destroy ATW thread context.";
            }
        }

        bool SolidColorEye(size_t eye, const RGBColorf& color) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if(!mRenderManager) {
                m_log->error() << "RenderManagerOpenGLATW::SolidColorEye: Called before successful OpenDisplay";
                return false;
            }
            // Stop the rendering thread from overwriting with warped
            // versions of the most recently presented buffers.
            mFirstFramePresented = false;
            return mRenderManager->SolidColorEye(eye, color);
        }

        //===================================================================
        // The distortion mesh is applied after time warp, so needs to be
        // passed on down to the harnessed RenderManager to handle it.
        OSVR_RENDERMANAGER_EXPORT bool
        UpdateDistortionMeshesInternal(DistortionMeshType type, std::vector<DistortionParameters> const& distort) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if(!mRenderManager) {
                m_log->error() << "RenderManagerOpenGLATW::SolidColorEye: Called before successful OpenDisplay";
                return false;
            }
            return mRenderManager->UpdateDistortionMeshesInternal(type, distort);
        }

        bool RegisterRenderBuffersInternal(const std::vector<RenderBuffer>& buffers,
                                           bool appWillNotOverwriteBeforeNewPresent = false) override {

            // They may be using one buffer for two eyes or one buffer
            // per eye, so we can't check the number of buffers.  Also,
            // we should support letting them register the render buffers
            // in batches, not all at once.
            std::unique_lock<std::mutex> lock(mMutex);
// this was a bug. client should be able to register render buffers after calling OpenDisplay.
#if 0
            if(mRenderManager) {
                m_log->error() << "RenderManagerOpenGLATW::RegisterRenderBuffersInternal: "
                    << "OpenDisplay has already been called. Cannot register new render buffers.";
                return false;
            }
#endif
            mRegisteredRenderBuffers = buffers;
            mAppWillNotOverwriteBeforeNewPresent = appWillNotOverwriteBeforeNewPresent;

            // We're done -- call the base-class function to notify that we've
            // registered our buffers
            return RenderManager::RegisterRenderBuffersInternal(buffers, appWillNotOverwriteBeforeNewPresent);
        }

        friend RenderManager OSVR_RENDERMANAGER_EXPORT* createRenderManager(OSVR_ClientContext context,
                                                                            const std::string& renderLibraryName,
                                                                            GraphicsLibrary graphicsLibrary);
    };
}
}

#endif // INCLUDED_RenderManagerOpenGLATW_h_GUID_47EFB3F7_EA50_4C9A_EC19_7C7DDCA0A243
