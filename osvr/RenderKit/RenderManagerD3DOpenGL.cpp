
/** @file
@brief Source file implementing OpenGL-over-D3D-based OSVR direct-to-device
rendering interface

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

#include <GL/glew.h>
#include <GL/wglew.h>
#include "RenderManagerD3DOpenGL.h"
#include "GraphicsLibraryD3D11.h"
#include "GraphicsLibraryOpenGL.h"
#include <osvr/Util/Logger.h>
#include <iostream>
#include <utility>

namespace osvr {
namespace renderkit {

    RenderManagerD3D11OpenGL::RenderManagerD3D11OpenGL(
        OSVR_ClientContext context,
        ConstructorParameters p,
        std::unique_ptr<RenderManagerD3D11Base>&& D3DToHarness)
        : RenderManagerOpenGL(context, p),
          m_D3D11Renderer(std::move(D3DToHarness)) {

        // Initialize all of the variables that don't have to be done in the
        // list above, so we don't get warnings about out-of-order
        // initialization if they are re-ordered in the header file.
        m_displayOpen = false;
        m_library.OpenGL = nullptr;

        if (!m_D3D11Renderer) {
            m_log->error() << "RenderManagerD3D11OpenGL::RenderManagerD3D11OpenGL: "
                           << "NULL pointer to D3D Renderer to harness.";
            return;
        }

        // Construct the appropriate GraphicsLibrary pointer.
        m_library.OpenGL = new GraphicsLibraryOpenGL;
        m_buffers.OpenGL = new RenderBufferOpenGL;
    }

    RenderManagerD3D11OpenGL::~RenderManagerD3D11OpenGL() {
        cleanupGL();

        if (m_displayOpen) {
            /// @todo Add anything else we need to clean up
            m_displayOpen = false;
        }

        // @todo Clean up all of the mm_oglToD3D entries we have made.
    }

    void RenderManagerD3D11OpenGL::cleanupGL() {
        if (m_glD3DHandle) {
            for (size_t i = 0; i < m_oglToD3D.size(); i++) {
                wglDXUnregisterObjectNV(m_glD3DHandle,
                                        m_oglToD3D[i].glColorHandle);
            }
            wglDXCloseDeviceNV(m_glD3DHandle);
            m_glD3DHandle = nullptr;
        }
        delete m_buffers.OpenGL;
        m_buffers.OpenGL = nullptr;
        delete m_library.OpenGL;
        m_library.OpenGL = nullptr;
        if (m_toolkit.removeOpenGLContexts) {
          m_toolkit.removeOpenGLContexts(m_toolkit.data);
        }
    }

    RenderManager::OpenResults RenderManagerD3D11OpenGL::OpenDisplay(void) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        OpenResults ret;
        ret.status = FAILURE;
        if (!doingOkay()) {
            return ret;
        }

        //======================================================
        // We need to call glewInit() so that we have access to
        // the wgl extensions needed below.  This means that we
        // need to get an OpenGL context open.  We use methods
        // in our OpenGL parent class to open the desired context.
        // We leave the context open so that we can get our
        // RenderBuffer names.
        GLContextParams p;
        p.visible = false; // Make the context invisible, to not distract
        OSVR_OpenGLContextParams pC;
        ConvertContextParams(p, pC);
        if (!m_toolkit.addOpenGLContext ||
            !m_toolkit.addOpenGLContext(m_toolkit.data, &pC)) {
            m_log->error() << "RenderManagerD3D11OpenGL::OpenDisplay: Can't open GL "
                              "context";
            return ret;
        }
        ReleaseContextParams(pC);
        glewExperimental = true; // Needed for core profile
        if (glewInit() != GLEW_OK) {
          if (m_toolkit.removeOpenGLContexts) {
            m_toolkit.removeOpenGLContexts(m_toolkit.data);
          }
          m_log->error() << "RenderManagerD3D11OpenGL::OpenDisplay: Can't "
                            "initialize GLEW";
          return ret;
        }

        //======================================================
        // Open the D3D display we're going to use and get a handle
        // on it to use to map our OpenGL objects.
        ret = m_D3D11Renderer->OpenDisplay();
        if (ret.status == FAILURE) {
            m_log->error() << "RenderManagerD3D11OpenGL::OpenDisplay: Can't open "
                              "D3D11 display";
            setDoingOkay(false);
            return ret;
        }
        m_glD3DHandle = wglDXOpenDeviceNV(ret.library.D3D11->device);
        if (m_glD3DHandle == nullptr) {
            m_log->error() << "RenderManagerD3D11OpenGL::OpenDisplay: Can't get GL "
                              "device handle";
            ret.status = FAILURE;
            return ret;
        }

        // Fill in our library with the things the application may need to
        // use to do its graphics state set-up.
        ret.library = m_library;
        m_displayOpen = true;
        return ret;
    }

    bool RenderManagerD3D11OpenGL::RenderFrameInitialize() {
        RenderManagerOpenGL::RenderFrameInitialize();
        if (m_D3D11Renderer == nullptr) {
            return false;
        }
        return m_D3D11Renderer->RenderFrameInitialize();
    }

    bool RenderManagerD3D11OpenGL::RenderDisplayInitialize(size_t display) {
        if (!RenderManagerOpenGL::RenderDisplayInitialize(display)) {
          return false;
        }
        if (m_D3D11Renderer == nullptr) {
            return false;
        }
        // @todo Do we need this?
        return m_D3D11Renderer->RenderDisplayInitialize(display);
    }

    bool RenderManagerD3D11OpenGL::RenderEyeInitialize(size_t eye) {
        checkForGLError(
            "RenderManagerD3D11OpenGL::RenderEyeInitialize beginning");

        if (!m_toolkit.makeCurrent ||
          !m_toolkit.makeCurrent(m_toolkit.data, GetDisplayUsedByEye(eye))) {
          return false;
        }

        // Attach the Direct3D buffers to our framebuffer object
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffers[GetDisplayUsedByEye(eye)]);
        checkForGLError(
            "RenderManagerD3D11OpenGL::RenderEyeInitialize BindFrameBuffer");
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             m_colorBuffers[eye].OpenGL->colorBufferName, 0);
        checkForGLError(
            "RenderManagerD3D11OpenGL::RenderEyeInitialize Bind color buffer");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_depthBuffers[eye]);
        checkForGLError(
            "RenderManagerD3D11OpenGL::RenderEyeInitialize Bind depth buffer");

        // Set the list of draw buffers.
        GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
        checkForGLError(
            "RenderManagerD3D11OpenGL::RenderEyeInitialize DrawBuffers");

        // Always check that our framebuffer is ok
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE) {
            m_log->error() << "RenderManagerD3D11OpenGL::RenderEyeInitialize: "
                              "Incomplete Framebuffer";
            return false;
        }

        // Call the display set-up callback, which needs to be done now for each
        // eye
        if (m_displayCallback.m_callback != nullptr) {
            m_displayCallback.m_callback(m_displayCallback.m_userData,
                                         m_library, m_colorBuffers[eye]);
        }
        checkForGLError("RenderManagerD3D11OpenGL::RenderEyeInitialize after "
                        "display user callback");

        return true;
    }

    bool RenderManagerD3D11OpenGL::PresentDisplayInitialize(size_t display) {
        if (!RenderManagerOpenGL::PresentDisplayInitialize(display)) {
          return false;
        }
        if (m_D3D11Renderer == nullptr) {
            return false;
        }
        return m_D3D11Renderer->PresentDisplayInitialize(display);
    }

    bool RenderManagerD3D11OpenGL::PresentDisplayFinalize(size_t display) {
        if (m_D3D11Renderer == nullptr) {
            return false;
        }
        return m_D3D11Renderer->PresentDisplayFinalize(display);
    }

    bool RenderManagerD3D11OpenGL::PresentFrameInitialize() {
        if (m_D3D11Renderer == nullptr) {
            return false;
        }
        return m_D3D11Renderer->PresentFrameInitialize();
    }

    bool RenderManagerD3D11OpenGL::PresentFrameFinalize() {
        if (!RenderManagerOpenGL::PresentFrameFinalize()) {
          return false;
        }
        if (m_D3D11Renderer == nullptr) {
            return false;
        }
        return m_D3D11Renderer->PresentFrameFinalize();
    }

    bool RenderManagerD3D11OpenGL::RegisterRenderBuffersInternal(
        const std::vector<RenderBuffer>& buffers,
        bool appWillNotOverwriteBeforeNewPresent) {
        // Make sure we're doing okay.
        if (!doingOkay()) {
            m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers(): "
                              "Display not opened.";
            return false;
        }

        // Make sure we don't have more eyes than buffers.  We can have fewer
        // because the client may have consolidated a number of eyes onto
        // one buffer.
        size_t numEyes = GetNumEyes();
        if (buffers.size() > numEyes) {
            m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                              "Wrong number of buffers: "
                           << buffers.size() << ", need " << numEyes;
            return false;
        }

        // Delete any previously-registered buffers.
        for (size_t i = 0; i < m_oglToD3D.size(); i++) {
            wglDXUnregisterObjectNV(m_glD3DHandle, m_oglToD3D[i].glColorHandle);
            m_oglToD3D[i].D3DBuffer.colorBufferView->Release();
            m_oglToD3D[i].D3DBuffer.colorBuffer->Release();
        }
        m_oglToD3D.clear();

        // Allocate D3D buffers to be used and tie them to the OpenGL buffers.
        for (size_t i = 0; i < buffers.size(); i++) {
            // Make our context current.
            // @todo The i variable may not match the eye in all cases?
            if (!m_toolkit.makeCurrent ||
              !m_toolkit.makeCurrent(m_toolkit.data, GetDisplayUsedByEye(i))) {
              m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                "Coud not set context for buffer: "
                << i;
              return false;
            }

            // If we have already mapped this buffer, we go ahead and skip it,
            // so that we don't tie the same OpenGL buffer to multiple D3D
            // buffers.  It is not an error to map the same buffer twice.
            bool found = false;
            for (size_t j = 0; j < i; j++) {
                if (buffers[i].OpenGL->colorBufferName ==
                    buffers[j].OpenGL->colorBufferName) {
                    found = true;
                }
            }
            if (found) {
                continue;
            }

            // Figure out how large the buffer should be by binding this texture
            // and querying the size of the 0th mipmap level.
            GLint width = 0, height = 0;
            glBindTexture(GL_TEXTURE_2D, buffers[i].OpenGL->colorBufferName);
            const GLint mipLevel = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, mipLevel, GL_TEXTURE_WIDTH,
                                     &width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, mipLevel, GL_TEXTURE_HEIGHT,
                                     &height);
            if ((width == 0) || (height == 0)) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                                  "Zero-sized buffer for buffer: "
                               << i;
                return false;
            }

            // Initialize a new render target texture description.
            // Make it a shared texture so that we can use it with OpenGL
            // Interop.
            D3D11_TEXTURE2D_DESC textureDesc = {};
            textureDesc.Width = width;
            textureDesc.Height = height;
            textureDesc.MipLevels = 1;
            textureDesc.ArraySize = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Usage = D3D11_USAGE_DEFAULT;
            // We need it to be both a render target and a shader resource
            textureDesc.BindFlags =
                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            textureDesc.CPUAccessFlags = 0;
            textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

            // Create a new render target texture to use.
            ID3D11Texture2D* D3DTexture = nullptr;
            HRESULT hr = m_D3D11Renderer->m_D3D11device->CreateTexture2D(
                &textureDesc, NULL, &D3DTexture);
            if (FAILED(hr)) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                                  "Can't create texture";
                return false;
            }

            // Fill in the resource view for your render texture buffer here
            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
            // This must match what was created in the texture to be rendered
            renderTargetViewDesc.Format = textureDesc.Format;
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            renderTargetViewDesc.Texture2D.MipSlice = 0;

            // Create the render target view.
            ID3D11RenderTargetView*
                renderTargetView; //< Pointer to our render target view
            hr = m_D3D11Renderer->m_D3D11device->CreateRenderTargetView(
                D3DTexture, &renderTargetViewDesc, &renderTargetView);
            if (FAILED(hr)) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                                  "Could not create render target";
                return false;
            }

            // Create a share handle for the texture, to enable it to be shared
            // between multiple devices.  Then register the share handle with
            // interop.
            // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476531(v=vs.85).aspx
            // https://www.opengl.org/registry/specs/NV/DX_interop.txt
            IDXGIResource* pOtherResource = nullptr;
            hr = D3DTexture->QueryInterface(__uuidof(IDXGIResource),
                                            (void**)&pOtherResource);
            HANDLE sharedHandle;
            hr = pOtherResource->GetSharedHandle(&sharedHandle);
            if (FAILED(hr)) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                                  "Could not get shared handle";
                return false;
            }
            if (wglDXSetResourceShareHandleNV(D3DTexture, sharedHandle) !=
                TRUE) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers()"
                                  ": Could not share resource";
                return false;
            }

            // Prepare the things we need for wrapping Direct3D
            // objects. Information on how to do this comes from
            // the DX_interop2.txt file from opengl.org and from the
            // secondstory/ofDxSharedTextureExample project on Github.
            // Bind the OpenGL texture to the D3D texture for rendering
            HANDLE glColorHandle = wglDXRegisterObjectNV(
                m_glD3DHandle, D3DTexture, buffers[i].OpenGL->colorBufferName,
                GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
            if (glColorHandle == nullptr) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                                  "Can't get Color buffer handle"
                               << " (error " << GetLastError() << "):";
                switch (GetLastError()) {
                case ERROR_INVALID_HANDLE:
                    m_log->error() << "  (Invalid handle)";
                    break;
                case ERROR_INVALID_DATA:
                    m_log->error() << "  (Invalid data)";
                    break;
                case ERROR_OPEN_FAILED:
                    m_log->error() << "  (Could not open Direct3D resource)";
                    break;
                default:
                    m_log->error() << "  (Unexpected error code)";
                }
                return false;
            }

            // New object to fill in
            OglToD3DTexture map;
            map.OpenGLTexture = buffers[i].OpenGL->colorBufferName;
            map.glColorHandle = glColorHandle;
            map.D3DBuffer.colorBuffer = D3DTexture;
            map.D3DBuffer.colorBufferView = renderTargetView;
            m_oglToD3D.push_back(map);

            // Lock the render target for OpenGL access
            if (!wglDXLockObjectsNV(m_glD3DHandle, 1, &map.glColorHandle)) {
                m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                                  "Can't lock Color buffer";
                return false;
            }
        }

        // Make a vector of the newly-created buffers and register them
        // on our harnessed RenderManager.
        std::vector<RenderBuffer> newBuffers;
        for (size_t i = 0; i < m_oglToD3D.size(); i++) {
          RenderBuffer rb;
          rb.D3D11 = &m_oglToD3D[i].D3DBuffer;
          newBuffers.push_back(rb);
        }
        if (!m_D3D11Renderer->RegisterRenderBuffers(newBuffers,
            appWillNotOverwriteBeforeNewPresent)) {
            m_log->error() << "RenderManagerD3D11OpenGL::RegisterRenderBuffers: "
                              "Could not register buffers with harnessed RenderManager";
            return false;
        }

        // We're done -- call the base-class function to notify that we've
        // registered our buffers
        return RenderManager::RegisterRenderBuffersInternal(buffers,
          appWillNotOverwriteBeforeNewPresent);
    }

    bool RenderManagerD3D11OpenGL::PresentRenderBuffersInternal(
      const std::vector<RenderBuffer>& renderBuffers,
      const std::vector<RenderInfo>& renderInfoUsed,
      const RenderParams& renderParams,
      const std::vector<OSVR_ViewportDescription>& normalizedCroppingViewports,
      bool flipInY) {

      // We need to flip the projection information back to normal so that
      // when the time warp calculations are running they are using the
      // expected projection matrix, rather than the one we modified to give
      // to the caller.  We're assuming here that they didn't do yet another
      // projection inversion.
      std::vector<RenderInfo> adjustedRenderInfo = renderInfoUsed;
      for (size_t i = 0; i < adjustedRenderInfo.size(); i++) {
        std::swap(adjustedRenderInfo[i].projection.bottom,
          adjustedRenderInfo[i].projection.top);
      }

      // Verify that we have registered all of these buffers.
      // Also add each to the vector we will send to the wrapped
      // RenderManager.
      std::vector<RenderBuffer> myD3DBuffers;
      for (size_t b = 0; b < renderBuffers.size(); b++) {
        if (!m_toolkit.makeCurrent ||
          !m_toolkit.makeCurrent(m_toolkit.data, GetDisplayUsedByEye(b))) {
          m_log->error() << "RenderManagerD3D11OpenGL::PresentRenderBuffersInternal(): "
            "Coud not set context for buffer: "
            << b;
          return false;
        }

        OglToD3DTexture* oglMap = nullptr;
        for (size_t i = 0; i < m_oglToD3D.size(); i++) {
          if (m_oglToD3D[i].OpenGLTexture ==
            renderBuffers[b].OpenGL->colorBufferName) {
            oglMap = &m_oglToD3D[i];
          }
        }
        if (oglMap == nullptr) {
          m_log->error()
            << "RenderManagerD3D11OpenGL::PresentRenderBuffersInternal(): Unregistered buffer"
            << " (call RegisterRenderBuffers before presenting)";
          return false;
        }
        if (renderBuffers[b].OpenGL->colorBufferName != oglMap->OpenGLTexture) {
          m_log->error()
            << "RenderManagerD3D11OpenGL::PresentRenderBuffersInternal(): Mis-matched buffer"
            << " (call RegisterRenderBuffers whenever a new render-texture "
            "is created)";
          return false;
        }
        RenderBuffer rb;
        rb.D3D11 = &(oglMap->D3DBuffer);
        myD3DBuffers.push_back(rb);
      }

      // Make sure to finish up our rendering and complete the buffers before
      // presenting them to D3D.
      glFinish();

      // Unlock all of the render buffers we know about.
      for (size_t i = 0; i < m_oglToD3D.size(); i++) {
        if (!wglDXUnlockObjectsNV(m_glD3DHandle, 1, &m_oglToD3D[i].glColorHandle)) {
            m_log->error() << "RenderManagerD3D11OpenGL::PresentRenderBuffersInternal: Can't unlock "
                              "Color buffer";
            return false;
        }
      }

      // Present the buffers using our wrapped renderer.
      bool ret = m_D3D11Renderer->PresentRenderBuffers(
        myD3DBuffers, adjustedRenderInfo, renderParams,
        normalizedCroppingViewports, flipInY);

      // Lock all of the render targets for OpenGL access again
      // so that the application can draw to them.
      for (size_t i = 0; i < m_oglToD3D.size(); i++) {
        if (!wglDXLockObjectsNV(m_glD3DHandle, 1, &m_oglToD3D[i].glColorHandle)) {
            m_log->error() << "RenderManagerD3D11OpenGL::PresentRenderBuffersInternal: Can't lock "
                              "Color buffer";
            return false;
        }
      }

      return true;
    }

    std::vector<RenderInfo> RenderManagerD3D11OpenGL::GetRenderInfoInternal(
      const RenderParams& params) {

      std::vector<RenderInfo> ret;
      ret = RenderManager::GetRenderInfoInternal(params);

      // We need to flip the projection information so that our output
      // images match those used by Direct3D, so we don't need to (1)
      // flip the textures and (2) Modify the time warp calculations.
      for (size_t i = 0; i < ret.size(); i++) {
        double temp;
        temp = ret[i].projection.bottom;
        ret[i].projection.bottom = ret[i].projection.top;
        ret[i].projection.top = temp;
      }

      return ret;
    }
} // namespace renderkit
} // namespace osvr
