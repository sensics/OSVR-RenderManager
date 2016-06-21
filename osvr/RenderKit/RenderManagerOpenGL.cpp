/** d@file
@brief Source file implementing OSVR rendering interface for OpenGL

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

#include <RenderManagerBackends.h>
#ifdef RM_USE_OPENGLES20
  #define glDeleteVertexArrays glDeleteVertexArraysOES
  #define glGenVertexArrays glGenVertexArraysOES
  #define glBindVertexArray glBindVertexArrayOES
#else
  #include <GL/glew.h>
  #ifdef _WIN32
    #include <GL/wglew.h>
  #endif
#endif
#include "RenderManagerOpenGL.h"
#include "GraphicsLibraryOpenGL.h"
#include "RenderManagerSDLInitQuit.h"
#include "ComputeDistortionMesh.h"
#include <iostream>
#include <Eigen/Core>
#include <Eigen/Geometry>

//==========================================================================
// Vertex and fragment shaders to perform our combination of asynchronous
// time warp and distortion correction.
static const GLchar* distortionVertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec4 position;\n"
    "layout(location = 1) in vec2 textureCoordinateR;\n"
    "layout(location = 2) in vec2 textureCoordinateG;\n"
    "layout(location = 3) in vec2 textureCoordinateB;\n"
    "out vec2 warpedCoordinateR;\n"
    "out vec2 warpedCoordinateG;\n"
    "out vec2 warpedCoordinateB;\n"
    "uniform mat4 projectionMatrix;\n"
    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 textureMatrix;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projectionMatrix * modelViewMatrix * position;\n"
    "   warpedCoordinateR = vec2(textureMatrix * "
    "      vec4(textureCoordinateR,0,1));\n"
    "   warpedCoordinateG = vec2(textureMatrix * "
    "      vec4(textureCoordinateG,0,1));\n"
    "   warpedCoordinateB = vec2(textureMatrix * "
    "      vec4(textureCoordinateB,0,1));\n"
    "}\n";

static const GLchar* distortionFragmentShader =
    "#version 330 core\n"
    "uniform sampler2D tex;\n"
    "in vec2 warpedCoordinateR;\n"
    "in vec2 warpedCoordinateG;\n"
    "in vec2 warpedCoordinateB;\n"
    "out vec3 color;\n"
    "void main()\n"
    "{\n"
    "    color.r = texture(tex, warpedCoordinateR).r;\n"
    "    color.g = texture(tex, warpedCoordinateG).g;\n"
    "    color.b = texture(tex, warpedCoordinateB).b;\n"
    "}\n";

static bool checkShaderError(GLuint shaderId) {
    GLint result = GL_FALSE;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(shaderId, GL_INFO_LOG_LENGTH, &maxLength);
        if (maxLength > 1) {
          std::unique_ptr<GLchar> infoLog(new GLchar[maxLength + 1]);
          glGetProgramInfoLog(shaderId, maxLength, NULL, infoLog.get());
          std::cerr << "osvr::renderkit::RenderManager::RenderManagerOpenGL"
            << "::checkShaderError: Message returned from shader compiler: "
            << infoLog.get() << std::endl;
        } else {
          std::cerr << "osvr::renderkit::RenderManager::RenderManagerOpenGL"
            << "::checkShaderError: Empty error message from shader compiler."
            << std::endl;
        }
        return false;
    }
    return true;
}

static bool checkProgramError(GLuint programId) {
    GLint result = GL_FALSE;
    glGetProgramiv(programId, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        int infoLength = 0;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 1) {
          std::unique_ptr<GLchar> infoLog(new GLchar[infoLength + 1]);
          glGetProgramInfoLog(programId, infoLength, NULL, infoLog.get());
          std::cerr << "osvr::renderkit::RenderManager::RenderManagerOpenGL"
            << "::checkProgramError: Message returned from shader compiler: "
            << infoLog.get() << std::endl;
        } else {
          std::cerr << "osvr::renderkit::RenderManager::RenderManagerOpenGL"
            << "::checkProgramError: Empty error message from shader compiler."
            << std::endl;
        }
        return false;
    }
    return true;
}

namespace osvr {
namespace renderkit {

    /// @todo Make this compile to no-op when debugging is off.
    bool RenderManagerOpenGL::checkForGLError(const std::string& message) {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cout << message << ": OpenGL error " << err << std::endl;
        }
        return (err != GL_NO_ERROR);
    }

    RenderManagerOpenGL::RenderManagerOpenGL(
        OSVR_ClientContext context,
        ConstructorParameters p)
        : RenderManager(context, p) {
        // Initialize all of the variables that don't have to be done in the
        // list above, so we don't get warnings about out-of-order
        // initialization if they are re-ordered in the header file.
        m_doingOkay = true;
        m_displayOpen = false;
        m_GLContext = nullptr;
        m_programId = 0;

        // Construct the appropriate GraphicsLibrary pointer.
        m_library.OpenGL = new GraphicsLibraryOpenGL;
        m_buffers.OpenGL = new RenderBufferOpenGL;
    }

    RenderManagerOpenGL::~RenderManagerOpenGL() {
        removeOpenGLContexts();

        if (m_displayOpen) {

            glDeleteFramebuffers(1, &m_frameBuffer);
            size_t numEyes = GetNumEyes();
            // @todo Handle the case of multiple displays per eye
            for (size_t i = 0; i < m_colorBuffers.size(); i++) {
                glDeleteTextures(1, &m_colorBuffers[i].OpenGL->colorBufferName);
                delete m_colorBuffers[i].OpenGL;
                glDeleteRenderbuffers(1, &m_depthBuffers[i]);
            }

            m_distortionMeshBuffer.clear();

            /// @todo Clean up anything else we need to

            m_displayOpen = false;
        }
        delete m_buffers.OpenGL;
        delete m_library.OpenGL;
    }

    bool RenderManagerOpenGL::RenderPathSetup() {
      //======================================================
      // Construct the present buffers we're going to use when in Render()
      // mode, to wrap the PresentMode interface.
      if (!constructRenderBuffers()) {
        std::cerr << "RenderManagerOpenGL::RenderPathSetup: Could not "
          "construct present buffers to wrap Render() path"
          << std::endl;
        return false;
      }
      return true;
    }

    bool RenderManagerOpenGL::constructRenderBuffers() {
        //======================================================
        // Create the framebuffer which regroups 0, 1,
        // or more textures, and 0 or 1 depth buffer.
        // It gets bound to the appropriate buffer for each eye
        // during rendering.
        m_frameBuffer = 0;
        glGenFramebuffers(1, &m_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);

        //======================================================
        // Create the render textures (and Z buffer textures) we're going
        // to use to render into before presenting them as buffers to be
        // displayed.  We make one per eye.  We'll set up to render into
        // each of these before calling the render callbacks.
        size_t numEyes = GetNumEyes();
        for (size_t i = 0; i < numEyes; i++) {

            // The color buffer for this eye
            GLuint colorBufferName = 0;
            glGenTextures(1, &colorBufferName);
            RenderBuffer rb;
            rb.OpenGL = new RenderBufferOpenGL;
            rb.OpenGL->colorBufferName = colorBufferName;
            m_colorBuffers.push_back(rb);

            // "Bind" the newly created texture : all future texture functions
            // will modify this texture glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBufferName);

            // Determine the appropriate size for the frame buffer to be used
            // for this eye.
            OSVR_ViewportDescription v;
            ConstructViewportForRender(i, v);
            int width = static_cast<int>(v.width);
            int height = static_cast<int>(v.height);

            // Give an empty image to OpenGL ( the last "0" means "empty" )
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, 0);

            // The depth buffer
            GLuint depthrenderbuffer;
            glGenRenderbuffers(1, &depthrenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width,
                                  height);
            m_depthBuffers.push_back(depthrenderbuffer);
        }

        // Register the render buffers we're going to use to present
        return RegisterRenderBuffersInternal(m_colorBuffers);
    }

    bool RenderManagerOpenGL::addOpenGLContext(GLContextParams p) {
        // Initialize the SDL video subsystem.
        if (!osvr::renderkit::SDLInitQuit()) {
            std::cerr << "RenderManagerOpenGL::addOpenGLContext: Could not "
                          "initialize SDL"
                      << std::endl;
            return false;
        }

        // Figure out the flags we want
        Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (p.fullScreen) {
            //        flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
            flags |= SDL_WINDOW_BORDERLESS;
        }
        if (p.visible) {
            flags |= SDL_WINDOW_SHOWN;
        } else {
            flags |= SDL_WINDOW_HIDDEN;
        }

        // Set the OpenGL attributes we want before opening the window
        if (p.numBuffers > 1) {
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        }
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, p.bitsPerPixel);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, p.bitsPerPixel);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, p.bitsPerPixel);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, p.bitsPerPixel);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
            SDL_GL_CONTEXT_PROFILE_CORE);
#endif

        // For now, append the display ID to the title.
        /// @todo Make a different title for each window in the config file
        char displayId = '0' + static_cast<char>(m_displays.size());
        std::string windowTitle = m_params.m_windowTitle + displayId;

        // For now, move the X position of the second display to the
        // right of the entire display for the left one.
        /// @todo Make the config-file entry a vector and read both
        /// from it.
        p.xPos += p.width * static_cast<int>(m_displays.size());

        // If this is not the first display, or if the configuration
        // includes a graphics library that says to share, we re-use
        // the existing context.
        if ((m_displays.size() > 0) ||
          ((m_params.m_graphicsLibrary.OpenGL != nullptr) &&
          (m_params.m_graphicsLibrary.OpenGL->shareOpenGLContext == true))) {

          // Share the current context
          SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        } else {
          // Replace the current context
          SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
        }

        // Push back a new window and context.
        m_displays.push_back(DisplayInfo());
        m_displays.back().m_window = SDL_CreateWindow(
            windowTitle.c_str(), p.xPos, p.yPos, p.width, p.height, flags);
        if (m_displays.back().m_window == nullptr) {
            std::cerr
                << "RenderManagerOpenGL::addOpenGLContext: Could not get window"
                << std::endl;
            return false;
        }

        m_GLContext = SDL_GL_CreateContext(m_displays.back().m_window);
        if (m_GLContext == nullptr) {
            std::cerr << "RenderManagerOpenGL::addOpenGLContext: Could not get "
                         "OpenGL context"
                      << std::endl;
            return false;
        }

        return true;
    }

    bool RenderManagerOpenGL::removeOpenGLContexts() {
        if (m_programId != 0) {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }
        if (m_GLContext) {
            SDL_GL_DeleteContext(m_GLContext);
            m_GLContext = 0;
        }
        while (m_displays.size() > 0) {
            if (m_displays.back().m_window == nullptr) {
                std::cerr << "RenderManagerOpenGL::closeOpenGLContext: No "
                             "window pointer"
                          << std::endl;
                return false;
            }
            SDL_DestroyWindow(m_displays.back().m_window);
            m_displays.back().m_window = nullptr;
            m_displays.pop_back();
        }
        return true;
    }

    RenderManager::OpenResults RenderManagerOpenGL::OpenDisplay(void) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        OpenResults ret;
        ret.library = m_library;
        ret.status = COMPLETE; // Until we hear otherwise
        if (!doingOkay()) {
            ret.status = FAILURE;
            return ret;
        }

        /// @todo How to handle window resizing?

        //======================================================
        // Use SDL to get us an OpenGL context.
        GLContextParams p;
        p.windowTitle = m_params.m_windowTitle;
        p.fullScreen = m_params.m_windowFullScreen;
        // @todo Pull this calculation out into the base class and
        // store a separate virtual-screen and actual-screen size.
        // If we've rotated the screen by 90 or 270, then the window
        // we ask for on the screen has swapped aspect ratios.
        if ((m_params.m_displayRotation ==
             ConstructorParameters::Display_Rotation::Ninety) ||
            (m_params.m_displayRotation ==
             ConstructorParameters::Display_Rotation::TwoSeventy)) {
            p.width = m_displayHeight;
            p.height = m_displayWidth;
        } else {
            p.width = m_displayWidth;
            p.height = m_displayHeight;
        }
        p.xPos = m_params.m_windowXPosition;
        p.yPos = m_params.m_windowYPosition;
        p.bitsPerPixel = m_params.m_bitsPerColor;
        p.numBuffers = m_params.m_numBuffers;
        p.visible = true;
        for (size_t display = 0; display < GetNumDisplays(); display++) {
            if (!addOpenGLContext(p)) {
                std::cerr << "RenderManagerOpenGL::OpenDisplay: Cannot get GL "
                             "context "
                          << "for display " << display << std::endl;
                ret.status = FAILURE;
                return ret;
            }
        }

        checkForGLError("RenderManagerOpenGL::OpenDisplay after context creation");

#ifndef RM_USE_OPENGLES20
        //======================================================
        // We need to call glewInit() so that we have access to
        // the extensions needed below.
        glewExperimental = true; // Needed for core profile
        if (glewInit() != GLEW_OK) {
            std::cerr
                << "RenderManagerOpenGL::OpenDisplay: Can't initialize GLEW"
                << std::endl;
            removeOpenGLContexts();
            ret.status = FAILURE;
            return ret;
        }
        // Clear any GL error that Glew caused.  Apparently on Non-Windows
        // platforms, this can cause a spurious  error 1280.
        glGetError();
#endif

        //======================================================
        // Set vertical sync behavior.
        if (m_params.m_verticalSync) {
            if (SDL_GL_SetSwapInterval(1) != 0) {
                std::cerr << "RenderManagerOpenGL::OpenDisplay: Warning: Could "
                             "not set vertical retrace on"
                          << std::endl;
            }
        } else {
            if (SDL_GL_SetSwapInterval(0) != 0) {
                std::cerr << "RenderManagerOpenGL::OpenDisplay: Warning: Could "
                             "not set vertical retrace off"
                          << std::endl;
            }
        }

        checkForGLError("RenderManagerOpenGL::OpenDisplay after vsync setting");

        //======================================================
        // Construct the shaders and program we'll use to present things
        // handling time warp/distortion.
        GLuint vertexShaderId;   //< Vertex shader for time warp/distortion
        GLuint fragmentShaderId; //< Fragment shader for time warp/distortion

        vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShaderId, 1, &distortionVertexShader, nullptr);
        glCompileShader(vertexShaderId);
        if (!checkShaderError(vertexShaderId)) {
            GLint infoLogLength;
            glGetShaderiv(vertexShaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
            GLchar* strInfoLog = new GLchar[infoLogLength + 1];
            glGetShaderInfoLog(vertexShaderId, infoLogLength, NULL, strInfoLog);

            removeOpenGLContexts();
            std::cerr << "RenderManagerOpenGL::OpenDisplay: Could not "
                         "construct vertex shader:"
                      << std::endl << strInfoLog << std::endl;
            ret.status = FAILURE;
            return ret;
        }

        fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShaderId, 1, &distortionFragmentShader, nullptr);
        glCompileShader(fragmentShaderId);
        if (!checkShaderError(fragmentShaderId)) {
            GLint infoLogLength;
            glGetShaderiv(fragmentShaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
            GLchar* strInfoLog = new GLchar[infoLogLength + 1];
            glGetShaderInfoLog(fragmentShaderId, infoLogLength, NULL, strInfoLog);

            removeOpenGLContexts();
            std::cerr << "RenderManagerOpenGL::OpenDisplay: Could not "
                         "construct fragment shader:"
                      << std::endl << strInfoLog << std::endl;
            ret.status = FAILURE;
            return ret;
        }

        checkForGLError("RenderManagerOpenGL::OpenDisplay after shader compile");

        m_programId = glCreateProgram();
        glAttachShader(m_programId, vertexShaderId);
        glAttachShader(m_programId, fragmentShaderId);
        glLinkProgram(m_programId);
        if (!checkProgramError(m_programId)) {
            removeOpenGLContexts();
            std::cerr << "RenderManagerOpenGL::OpenDisplay: Could not link "
                         "shader program "
                      << std::endl;
            ret.status = FAILURE;
            return ret;
        }
        checkForGLError("RenderManagerOpenGL::OpenDisplay after program link");

        m_projectionUniformId =
            glGetUniformLocation(m_programId, "projectionMatrix");
        m_modelViewUniformId =
            glGetUniformLocation(m_programId, "modelViewMatrix");
        m_textureUniformId = glGetUniformLocation(m_programId, "textureMatrix");

        // Now that they are linked, we don't need to keep them around.
        glDeleteShader(vertexShaderId);
        glDeleteShader(fragmentShaderId);

        if (!UpdateDistortionMeshesInternal(SQUARE,
                                            m_params.m_distortionParameters)) {
            removeOpenGLContexts();
            std::cerr << "RenderManagerOpenGL::OpenDisplay: Could not "
                         "construct distortion mesh"
                      << std::endl;
            ret.status = FAILURE;
            return ret;
        }

        //======================================================
        // Fill in our library with the things the application may need to
        // use to do its graphics state set-up.
        ret.library = m_library;

        checkForGLError("RenderManagerOpenGL::OpenDisplay end");

        //======================================================
        // Done, we now have an open window to use.
        m_displayOpen = true;
        return ret;
    }

    bool RenderManagerOpenGL::RenderDisplayInitialize(size_t display) {
        checkForGLError("RenderManagerOpenGL::RenderDisplayInitialize start");

        // Make our OpenGL context current
        SDL_GL_MakeCurrent(m_displays[display].m_window, m_GLContext);
        checkForGLError("RenderManagerOpenGL::RenderDisplayInitialize end");
        return true;
    }

    bool RenderManagerOpenGL::RenderEyeInitialize(size_t eye) {
        checkForGLError("RenderManagerOpenGL::RenderEyeInitialize starting");

        // Render to our framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        if (checkForGLError(
                "RenderManagerOpenGL::RenderEyeInitialize glBindFrameBuffer")) {
            return false;
        }

        // Set color and depth buffers for the frame buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             m_colorBuffers[eye].OpenGL->colorBufferName, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_depthBuffers[eye]);
        if (checkForGLError(
                "RenderManagerOpenGL::RenderEyeInitialize Setting textures")) {
            return false;
        }

        // Always check that our framebuffer is ok
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "RenderManagerOpenGL::RenderEyeInitialize: Incomplete "
                         "Framebuffer"
                      << std::endl;
            return false;
        }

        // Call the display set-up callback for each eye, because they each
        // have their own frame buffer whether or not they actually end up
        // in different windows.
        if (m_displayCallback.m_callback != nullptr) {
            m_displayCallback.m_callback(m_displayCallback.m_userData,
                                         m_library, m_buffers);
        }

        if (checkForGLError("RenderManagerOpenGL::RenderEyeInitialize")) {
            return false;
        }
        return true;
    }

    bool RenderManagerOpenGL::RenderSpace(
        size_t whichSpace //< Index into m_callbacks vector
        , size_t whichEye //< Which eye are we rendering for?
        , OSVR_PoseState pose //< ModelView transform to use
        , OSVR_ViewportDescription viewport //< Viewport to use
        , OSVR_ProjectionMatrix projection //< Projection to use
        ) {
        /// @todo Fill in the timing information
        OSVR_TimeValue deadline;
        deadline.microseconds = 0;
        deadline.seconds = 0;

        checkForGLError(
          "RenderManagerOpenGL::RenderSpace: Before calling user callback");
        RenderCallbackInfo& cb = m_callbacks[whichSpace];
        cb.m_callback(cb.m_userData, m_library, m_buffers, viewport, pose,
                      projection, deadline);
        checkForGLError(
          "RenderManagerOpenGL::RenderSpace: After calling user callback");

        /// @todo Keep track of timing information

        return true;
    }

    RenderManagerOpenGL::DistortionMeshBuffer::DistortionMeshBuffer()
        : VAO(0)
        , vertexBuffer(0)
        , indexBuffer(0)
    {   }

    RenderManagerOpenGL::DistortionMeshBuffer::DistortionMeshBuffer(
        DistortionMeshBuffer && rhs) {
        VAO = std::move(rhs.VAO);
        vertexBuffer = std::move(rhs.vertexBuffer);
        indexBuffer = std::move(rhs.indexBuffer);
        vertices = std::move(rhs.vertices);
        indices = std::move(rhs.indices);
    }

    RenderManagerOpenGL::DistortionMeshBuffer::~DistortionMeshBuffer() {
        Clear();
    }

    RenderManagerOpenGL::DistortionMeshBuffer &
        RenderManagerOpenGL::DistortionMeshBuffer::operator = (
        DistortionMeshBuffer && rhs) {
        if (&rhs != this) {
            Clear();
            VAO = std::move(rhs.VAO);
            vertexBuffer = std::move(rhs.vertexBuffer);
            indexBuffer = std::move(rhs.indexBuffer);
            vertices = std::move(rhs.vertices);
            indices = std::move(rhs.indices);
        }
        return *this;
    }

    void RenderManagerOpenGL::DistortionMeshBuffer::Clear() {
        if (VAO) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (vertexBuffer) {
            glDeleteBuffers(1, &vertexBuffer);
            vertexBuffer = 0;
        }
        if (indexBuffer) {
            glDeleteBuffers(1, &indexBuffer);
            indexBuffer = 0;
        }
        vertices.clear();
        indices.clear();
    }

    bool RenderManagerOpenGL::UpdateDistortionMeshesInternal(
        DistortionMeshType type //< Type of mesh to produce
        ,
        std::vector<DistortionParameters> const&
            distort //< Distortion parameters
        ) {

        // Clear the triangle and quad buffers if we have created them before.
        m_distortionMeshBuffer.clear();

        // Construct the data buffer that will hold the vertices and texture
        // coordinates for R,G,B distortion mapping.

        size_t const numEyes = GetNumEyes();
        if (numEyes > distort.size()) {
            std::cerr << "RenderManagerD3D11Base::UpdateDistortionMesh: "
                "Not enough distortion parameters for all eyes" << std::endl;
            removeOpenGLContexts();
            return false;
        }

        m_distortionMeshBuffer.resize(numEyes);
        for (size_t eye = 0; eye < numEyes; eye++) {

            auto & meshBuffer = m_distortionMeshBuffer[eye];

            // Compute the distortion mesh
            DistortionMesh mesh = ComputeDistortionMesh(eye, type, distort[eye], m_params.m_renderOverfillFactor);
            if (mesh.vertices.empty()) {
                std::cerr << "RenderManagerOpenGL::UpdateDistortionMesh: Could "
                             "not create mesh "
                          << "for eye " << eye << std::endl;
                removeOpenGLContexts();
                return false;
            }

            // Transcribe the vertex data into the correct format
            meshBuffer.vertices.resize(mesh.vertices.size());
            for (size_t i = 0; i < meshBuffer.vertices.size(); ++i) {
                auto & meshVert = meshBuffer.vertices[i];
                auto const & v = mesh.vertices[i];
                meshVert.pos[0] = v.m_pos[0];
                meshVert.pos[1] = v.m_pos[1];
                meshVert.pos[2] = 0; // Z = 0
                meshVert.pos[3] = 1; // Homogeneous coordinate = 1

                meshVert.texRed[0] = v.m_texRed[0];
                meshVert.texRed[1] = v.m_texRed[1];

                meshVert.texGreen[0] = v.m_texGreen[0];
                meshVert.texGreen[1] = v.m_texGreen[1];

                meshVert.texBlue[0] = v.m_texBlue[0];
                meshVert.texBlue[1] = v.m_texBlue[1];
            }

            // Copy the index data
            meshBuffer.indices = mesh.indices;

            // Construct the geometry we're going to render into the eyes
            glGenVertexArrays(1, &meshBuffer.VAO);
            glBindVertexArray(meshBuffer.VAO);
            
            glGenBuffers(1, &meshBuffer.vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, meshBuffer.vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                sizeof(DistortionVertex) * meshBuffer.vertices.size(),
                &meshBuffer.vertices[0], GL_STATIC_DRAW);

            size_t const stride = sizeof(DistortionVertex);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride,
                (void*)offsetof(DistortionVertex,pos));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                (void*)offsetof(DistortionVertex, texRed));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                (void*)offsetof(DistortionVertex, texGreen));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
                (void*)offsetof(DistortionVertex, texBlue));
            glEnableVertexAttribArray(3);

            glGenBuffers(1, &meshBuffer.indexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshBuffer.indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                sizeof(decltype(meshBuffer.indices[0])) * meshBuffer.indices.size(),
                &meshBuffer.indices[0], GL_STATIC_DRAW);

            glBindVertexArray(0);
        }

        return true;
    }

    bool RenderManagerOpenGL::RenderFrameInitialize() {
        return PresentFrameInitialize();
    }

    bool RenderManagerOpenGL::RenderFrameFinalize() {
        checkForGLError(
          "RenderManagerOpenGL::RenderFramaFinalize: start");
        if (!PresentRenderBuffersInternal(m_colorBuffers, m_renderInfoForRender,
                                          m_renderParamsForRender)) {
            std::cerr << "RenderManagerD3D11OpenGL::RenderFrameFinalize: Could "
                         "not present render buffers"
                      << std::endl;
            return false;
        }
        return true;
    }

    bool RenderManagerOpenGL::PresentDisplayInitialize(size_t display) {
        if (display >= GetNumDisplays()) {
            return false;
        }
        checkForGLError(
          "RenderManagerOpenGL::PresentDisplayInitialize: start");

        // Make our OpenGL context current
        SDL_GL_MakeCurrent(m_displays[display].m_window, m_GLContext);
        checkForGLError(
          "RenderManagerOpenGL::PresentDisplayInitialize: after making GL current");
        return true;
    }

    bool RenderManagerOpenGL::PresentDisplayFinalize(size_t display) {
        if (display >= GetNumDisplays()) {
            return false;
        }

        SDL_GL_SwapWindow(m_displays[display].m_window);
        return true;
    }

    bool RenderManagerOpenGL::PresentFrameFinalize() {
        // Let SDL handle any system events that it needs to.
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            // If SDL has been given a quit event, what should we do?
            // We return false to let the app know that something went wrong.
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                return false;
            }
        }

        return true;
    }

    bool RenderManagerOpenGL::PresentEye(PresentEyeParameters params) {
        if (checkForGLError(
                "RenderManagerOpenGL::PresentEye start")) {
            return false;
        }
        if (params.m_buffer.OpenGL == nullptr) {
            std::cerr
                << "RenderManagerOpenGL::PresentEye(): NULL buffer pointer"
                << std::endl;
            return false;
        }

        // Construct the OpenGL viewport based on which eye this is.
        OSVR_ViewportDescription viewportDesc;
        if (!ConstructViewportForPresent(
                params.m_index, viewportDesc,
                m_params.m_displayConfiguration.getSwapEyes())) {
            std::cerr << "RenderManagerOpenGL::PresentEye(): Could not "
                         "construct viewport"
                      << std::endl;
            return false;
        }
        // Adjust the viewport based on how much the display window is
        // rotated with respect to the rendering window.
        viewportDesc = RotateViewport(viewportDesc);
        glViewport(static_cast<GLint>(viewportDesc.left),
                   static_cast<GLint>(viewportDesc.lower),
                   static_cast<GLsizei>(viewportDesc.width),
                   static_cast<GLsizei>(viewportDesc.height));
        if (checkForGLError(
          "RenderManagerOpenGL::PresentEye after glViewport")) {
          return false;
        }

        // Figure out which display we're rendering to for this eye.
        /// @todo This will need to be generalized when we have multiple
        /// displays per eye.
        size_t display = GetDisplayUsedByEye(params.m_index);

        /// Switch to our vertex/shader programs
        /// Store the user program so we can put it back again before
        /// returning.
        GLint userProgram;
        glGetIntegerv(GL_CURRENT_PROGRAM, &userProgram);
        checkForGLError("RenderManagerOpenGL::PresentEye after get user program");
        glUseProgram(m_programId);
        if (checkForGLError(
          "RenderManagerOpenGL::PresentEye after use program")) {
          return false;
        }

        // Set up a Projection matrix that undoes the scale factor applied
        // due to our rendering overfill factor.  This will put only the part
        // of the geometry that should be visible inside the viewing frustum.
        // @todo think about how we get square pixels, to properly handle
        // distortion correction.
        GLfloat myScale = m_params.m_renderOverfillFactor;
        GLfloat scaleProj[16] = { myScale, 0, 0, 0, 0, myScale, 0, 0,
          0, 0, 1, 0, 0, 0, 0, 1 };
        glUniformMatrix4fv(m_projectionUniformId, 1, GL_FALSE, scaleProj);
        if (checkForGLError("RenderManagerOpenGL::PresentEye after projection "
          "matrix setting")) {
          return false;
        }

        // Set up a ModelView matrix that handles rotating and flipping the
        // geometry as needed to match the display scan-out circuitry and/or
        // any changes needed by the inversion of window coordinates when
        // switching between graphics systems (OpenGL and Direct3D, for
        // example).
        // @todo think about how we get square pixels, to properly handle
        // distortion correction.
        matrix16 modelView;
        if (!ComputeDisplayOrientationMatrix(
          static_cast<float>(params.m_rotateDegrees), params.m_flipInY,
          modelView)) {
          std::cerr << "RenderManagerOpenGL::PresentEye(): "
            "ComputeDisplayOrientationMatrix failed"
            << std::endl;
          return false;
        }
        glUniformMatrix4fv(m_modelViewUniformId, 1, GL_FALSE, modelView.data);
        if (checkForGLError("RenderManagerOpenGL::PresentEye after modelView "
          "matrix setting")) {
          return false;
        }

        //=========================================================
        // Asynchronous Time Warp.
        // Set up the texture matrix to handle asynchronous time warp

        float textureMat[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        if (params.m_timeWarp != nullptr) {
          // Because the matrix was built in compliance with the OpenGL
          // spec, we can just directly use it.
          memcpy(textureMat, params.m_timeWarp->data, 15 * sizeof(float));
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
        // We read in, multiply, and write out textureMat.
        matrix16 crop;
        ComputeRenderBufferCropMatrix(params.m_normalizedCroppingViewport,
          crop);
        Eigen::Map<Eigen::MatrixXf> textureEigen(textureMat, 4, 4);
        Eigen::Map<Eigen::MatrixXf> cropEigen(crop.data, 4, 4);
        Eigen::MatrixXf full(4, 4);
        full = textureEigen * cropEigen;
        memcpy(textureMat, full.data(), 16 * sizeof(float));

        glUniformMatrix4fv(m_textureUniformId, 1, GL_FALSE, textureMat);
        if (checkForGLError("RenderManagerOpenGL::PresentEye after texture "
          "matrix setting")) {
          return false;
        }

        // Render the geometry to fill the viewport, with the texture
        // mapped onto it.

        // Render to the 0th frame buffer, which is the screen.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Bind the texture that we're going to use to render into the
        // frame buffer.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, params.m_buffer.OpenGL->colorBufferName);

        // Bilinear filtering and clamp to the edge of the texture.
        const GLfloat border[] = { 0, 0, 0, 0 };
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

        // NOTE: No need to clear the buffer in color or depth; we're
        // always overwriting the whole thing.  We do need to store the
        // value of the depth-test bit and restore it, turning it off for
        // our use here.
        // Store the initial values of rendering state that we set, so we
        // can restore it below.
        // Disable depth testing.
        // Enable 2D texturing.
        // Disable face culling (in case client switched
        // front-face).

        GLboolean depthTest, cullFace;
        glGetBooleanv(GL_DEPTH_TEST, &depthTest);
        glGetBooleanv(GL_CULL_FACE, &cullFace);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        if (checkForGLError(
          "RenderManagerOpenGL::PresentEye after environment setting")) {
          return false;
        }

        // @todo save and later restore the state telling which texture is bound
        // and which vertex attributes are set

        if (checkForGLError(
          "RenderManagerOpenGL::PresentEye after texture bind")) {
          return false;
        }

        auto const & meshBuffer = m_distortionMeshBuffer[params.m_index];
        glBindVertexArray(meshBuffer.VAO);
        glDrawElements(GL_TRIANGLES,
          static_cast<GLsizei>(meshBuffer.indices.size()),
          GL_UNSIGNED_SHORT, 0);

        // Put rendering parameters back the way they were before we set them
        // above.
        if (depthTest) {
          glEnable(GL_DEPTH_TEST);
        } else {
          glDisable(GL_DEPTH_TEST);
        }

        if (cullFace) {
          glEnable(GL_CULL_FACE);
        } else {
          glDisable(GL_CULL_FACE);
        }

        if (checkForGLError("RenderManagerOpenGL::PresentEye end")) {
            return false;
        }

        // Put back the user's vertex/shader program.
        glUseProgram(userProgram);

        return true;
    }

    bool RenderManagerOpenGL::SolidColorEye(
          size_t eye, const RGBColorf &color) {

      // Construct the OpenGL viewport based on which eye this is.
      OSVR_ViewportDescription viewportDesc;
      if (!ConstructViewportForPresent(
        eye, viewportDesc,
        m_params.m_displayConfiguration.getSwapEyes())) {
        std::cerr << "RenderManagerOpenGL::SolidColorEye(): Could not "
          "construct viewport"
          << std::endl;
        return false;
      }
      // Adjust the viewport based on how much the display window is
      // rotated with respect to the rendering window.
      viewportDesc = RotateViewport(viewportDesc);
      glViewport(static_cast<GLint>(viewportDesc.left),
        static_cast<GLint>(viewportDesc.lower),
        static_cast<GLsizei>(viewportDesc.width),
        static_cast<GLsizei>(viewportDesc.height));
      if (checkForGLError(
        "RenderManagerOpenGL::SolidColorEye after glViewport")) {
        return false;
      }

      // Clear to the specified color
      glClearColor(color.r, color.g, color.b, 1);
      glClear(GL_COLOR_BUFFER_BIT);

      return true;
    }


} // namespace renderkit
} // namespace osvr
