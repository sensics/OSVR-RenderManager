/** @file
@brief Header file describing the OSVR direct-to-device rendering interface for
OpenGL

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

#pragma once
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include "RenderManager.h"
#include <RenderManagerBackends.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef RM_USE_OPENGLES20
  // @todo This presumes we're compiling on Android.
  #include <GLES2/gl2.h>
  #include <GLES2/gl2ext.h>

  // Bind the vertex-array extensions from the DLL
  #include <dlfcn.h>
  PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES;
  PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES;
  PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES;
  class CalledBeforeCodeRuns {
    public:
      CalledBeforeCodeRuns() {
	void *libhandle = dlopen("libGLESv2.so", RTLD_LAZY);

	glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC)
				dlsym(libhandle,
				"glBindVertexArrayOES");
	glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC)
				dlsym(libhandle,
				"glDeleteVertexArraysOES");
	glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC)
				dlsym(libhandle,
				"glGenVertexArraysOES");
    }
  };
  static CalledBeforeCodeRuns getFunctionPointers;

#else
  #ifdef __APPLE__
    #include <OpenGL/gl3.h>
  #else
    #include <GL/gl.h>
  #endif
#endif

#include <stdlib.h>

#include <vector>
#include <string>

namespace osvr {
namespace renderkit {

    class RenderManagerOpenGL : public RenderManager {
      public:
        virtual ~RenderManagerOpenGL();

        // Is the renderer currently working?
        bool doingOkay() override { return m_doingOkay; }

        // Opens the D3D renderer we're going to use.
        OpenResults OpenDisplay() override;

      protected:
        /// Construct an OpenGL render manager.
        RenderManagerOpenGL(
            OSVR_ClientContext context,
            ConstructorParameters p);

        OSVR_RENDERMANAGER_EXPORT bool UpdateDistortionMeshesInternal(
            DistortionMeshType type //< Type of mesh to produce
            ,
            std::vector<DistortionParameters> const&
                distort //< Distortion parameters
            ) override;

        bool m_doingOkay;   //< Are we doing okay?
        bool m_displayOpen; //< Has our display been opened?

        // Methods to open and close a window, used to get
        // the GL contexts established.  We have these outside
        // the OpenDisplay() call so that child classes can
        // use them to make and destroy contexts as needed.
        class GLContextParams {
          public:
            std::string windowTitle; //< Window title
            int displayIndex;        //< Which display to use (-1 = any)?
            bool fullScreen;         //< Do we want full screen?
            int width;               //< If not full screen, how wide?
            int height;              //< If not full screen, how high?
            int xPos;                //< Where on the virtual desktop?
            int yPos;                //< Where on the virtual desktop?
            int bitsPerPixel;        //< How many bits per pixel?
            unsigned numBuffers;     //< How many buffers (2 = double buffering)
            bool visible;            //< Should the window be initially visible?
            GLContextParams() {
                windowTitle = "OSVR";
                displayIndex = -1;
                fullScreen = false;
                width = 640;
                height = 480;
                xPos = 0;
                yPos = 0;
                bitsPerPixel = 8;
                numBuffers = 2;
                visible = true;
            }
        };
        virtual bool addOpenGLContext(GLContextParams p) = 0;
        virtual bool removeOpenGLContexts() = 0;

        /// Make the context for display the current context.
        virtual bool makeCurrent(size_t display) = 0;

        /// Update window after rendering.
        virtual bool swapBuffers(size_t display) = 0;

        /// Set vertical sync behavior.
        virtual bool setVerticalSync(bool verticalSync) = 0;

        /// Set vertical sync behavior.
        virtual bool handleEvents() = 0;

        /// Delete m_programId in destructor.
        void deleteProgram();

        /// Construct the buffers we're going to use in Render() mode, which
        /// we use to actually use the Presentation mode.  This gives us the
        /// main Presentation path as the basic approach which we can build on
        /// top of, and also lets us make the intermediate buffers the correct
        /// size we need for Asychronous Time Warp and distortion, and keeps
        /// them from being in the same window and so bleeding together.
        bool constructRenderBuffers();

        // Special vertex/fragment shader information for our shader that
        // handles
        // asynchronous time warp and/or distortion.
        GLuint m_programId;           //< Groups the shaders for time warp/distortion
        GLuint m_projectionUniformId; //< Pointer to projection matrix, vertex
                                      /// shader
        GLuint
            m_modelViewUniformId; //< Pointer to modelView matrix, vertex shader
        GLuint m_textureUniformId; //< Pointer to texture matrix, vertex shader
        GLuint m_frameBuffer;      //< Groups a color buffer and a depth buffer

        std::vector<RenderBuffer>
            m_colorBuffers; //< Color buffers to hand to render callbacks
        std::vector<GLuint> m_depthBuffers; //< Depth/stencil buffers to hand to
                                            /// render callbacks

        struct DistortionVertex {
            GLfloat pos[4];
            GLfloat texRed[2];
            GLfloat texGreen[2];
            GLfloat texBlue[2];
        };

        struct DistortionMeshBuffer {
            GLuint VAO;
            GLuint vertexBuffer;
            GLuint indexBuffer;
            std::vector<DistortionVertex> vertices;
            std::vector<uint16_t> indices;

            DistortionMeshBuffer();
            DistortionMeshBuffer(DistortionMeshBuffer && rhs);
            ~DistortionMeshBuffer();
            DistortionMeshBuffer & operator=(DistortionMeshBuffer && rhs);

            void Clear();
        };

        // Vertex/texture coordinate buffer to render into final windows, one
        // per eye
        // @todo One per eye/display combination in case of multiple displays
        // per eye
        std::vector<DistortionMeshBuffer> m_distortionMeshBuffer;

        //===================================================================
        // Overloaded render functions from the base class.
        bool RenderPathSetup() override;
        bool RenderFrameInitialize() override;
        bool RenderDisplayInitialize(size_t display) override;
        bool RenderEyeInitialize(size_t eye) override;
        bool RenderSpace(size_t whichSpace //< Index into m_callbacks vector
                         ,
                         size_t whichEye //< Which eye are we rendering for?
                         ,
                         OSVR_PoseState pose //< ModelView transform to use
                         ,
                         OSVR_ViewportDescription viewport //< Viewport to use
                         ,
                         OSVR_ProjectionMatrix projection //< Projection to use
                         ) override;
        bool RenderEyeFinalize(size_t eye) override { return true; }
        bool RenderDisplayFinalize(size_t display) override { return true; }
        bool RenderFrameFinalize() override;

        bool PresentFrameInitialize() override { return true; }
        bool PresentDisplayInitialize(size_t display) override;
        bool PresentEye(PresentEyeParameters params) override;
        bool SolidColorEye(size_t eye, const RGBColorf &color) override;
        bool PresentDisplayFinalize(size_t display) override;
        bool PresentFrameFinalize() override;

        /// See if we had an OpenGL error
        /// @return True if there is an error, false if not.
        /// @param [in] message Message to print if there is an error
        static bool checkForGLError(const std::string& message);

        friend RenderManager OSVR_RENDERMANAGER_EXPORT*
        createRenderManager(OSVR_ClientContext context,
                            const std::string& renderLibraryName,
                            GraphicsLibrary graphicsLibrary);
    };

} // namespace renderkit
} // namespace osvr
