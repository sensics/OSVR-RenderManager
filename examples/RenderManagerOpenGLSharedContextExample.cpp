/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and an OpenGL shared context to render a scene with low latency.

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

// Internal Includes
#include <osvr/RenderKit/RenderManager.h>
#include <osvr/RenderKit/RenderManagerSDLInitQuit.h>
#include <osvr/ClientKit/Context.h>

// Library/third-party includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/glew.h>

// We are going to use SDL to get our OpenGL context for us.
// Unfortunately, SDL.h has #define main    SDL_main in it, so
// we need to undefine main again so we can make our own below.
#include <SDL.h>
#include <SDL_opengl.h>
#undef main

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// This must come after we include <GL/gl.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>

//==========================================================================
// Toolkit object to handle our window creation needs.  We pass it down to
// the RenderManager and it is to make windows in the same context that
// we are making them in.  RenderManager will call its functions to make them.
//  NOTE: The operative line for our purposes is the one that always asks
// to share the context: SDL_GL_SHARE_WITH_CURRENT_CONTEXT.  In the built-
// in RenderManager code, this context is only shared for multiple displays.
// Our overriding the standard toolkit lets us do this.
//  NOTE: If you are using a rendering engine, you would replace the methods
// here to make it use the rendering engine to construct windows in its own
// context.  The addOpenGLContext() function is the one that should be
// overridden to do this.

class SDLToolkitImpl {
  OSVR_OpenGLToolkitFunctions toolkit;

  static void createImpl(void* data) {
  }
  static void destroyImpl(void* data) {
    delete ((SDLToolkitImpl*)data);
  }
  static OSVR_CBool addOpenGLContextImpl(void* data, const OSVR_OpenGLContextParams* p) {
    return ((SDLToolkitImpl*)data)->addOpenGLContext(p);
  }
  static OSVR_CBool removeOpenGLContextsImpl(void* data) {
    return ((SDLToolkitImpl*)data)->removeOpenGLContexts();
  }
  static OSVR_CBool makeCurrentImpl(void* data, size_t display) {
    return ((SDLToolkitImpl*)data)->makeCurrent(display);
  }
  static OSVR_CBool swapBuffersImpl(void* data, size_t display) {
    return ((SDLToolkitImpl*)data)->swapBuffers(display);
  }
  static OSVR_CBool setVerticalSyncImpl(void* data, OSVR_CBool verticalSync) {
    return ((SDLToolkitImpl*)data)->setVerticalSync(verticalSync == OSVR_TRUE);
  }
  static OSVR_CBool handleEventsImpl(void* data) {
    return ((SDLToolkitImpl*)data)->handleEvents();
  }

  // Classes and structures needed to do our rendering.
  class DisplayInfo {
  public:
    SDL_Window* m_window = nullptr; //< The window we're rendering into
  };
  std::vector<DisplayInfo> m_displays;

  SDL_GLContext
    m_GLContext; //< The context we use to render to all displays

public:
  SDLToolkitImpl() {
    memset(&toolkit, 0, sizeof(toolkit));
    toolkit.size = sizeof(toolkit);
    toolkit.data = this;

    toolkit.create = createImpl;
    toolkit.destroy = destroyImpl;
    toolkit.addOpenGLContext = addOpenGLContextImpl;
    toolkit.removeOpenGLContexts = removeOpenGLContextsImpl;
    toolkit.makeCurrent = makeCurrentImpl;
    toolkit.swapBuffers = swapBuffersImpl;
    toolkit.setVerticalSync = setVerticalSyncImpl;
    toolkit.handleEvents = handleEventsImpl;
  }

  ~SDLToolkitImpl() {
  }

  const OSVR_OpenGLToolkitFunctions* getToolkit() const { return &toolkit; }

  bool addOpenGLContext(const OSVR_OpenGLContextParams* p) {
    // Initialize the SDL video subsystem.
    if (!osvr::renderkit::SDLInitQuit()) {
      std::cerr << "RenderManagerOpenGL::addOpenGLContext: Could not "
        "initialize SDL"
        << std::endl;
      return false;
    }

    // Figure out the flags we want
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (p->fullScreen) {
      //        flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
      flags |= SDL_WINDOW_BORDERLESS;
    }
    if (p->visible) {
      flags |= SDL_WINDOW_SHOWN;
    }
    else {
      flags |= SDL_WINDOW_HIDDEN;
    }

    // Set the OpenGL attributes we want before opening the window
    if (p->numBuffers > 1) {
      SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, p->bitsPerPixel);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, p->bitsPerPixel);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, p->bitsPerPixel);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, p->bitsPerPixel);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
      SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    // Re-use the same context for all created windows.
    // ***** This is the line that makes it share the context. *****
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    // Push back a new window and context.
    m_displays.push_back(DisplayInfo());
    m_displays.back().m_window = SDL_CreateWindow(
      p->windowTitle, p->xPos, p->yPos, p->width, p->height, flags);
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
  bool removeOpenGLContexts() {
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
  bool makeCurrent(size_t display) {
    SDL_GL_MakeCurrent(m_displays[display].m_window, m_GLContext);
    return true;
  }
  bool swapBuffers(size_t display) {
    SDL_GL_SwapWindow(m_displays[display].m_window);
    return true;
  }
  bool setVerticalSync(bool verticalSync) {
    if (verticalSync) {
      if (SDL_GL_SetSwapInterval(1) != 0) {
        std::cerr << "RenderManagerOpenGL::OpenDisplay: Warning: Could "
          "not set vertical retrace on"
          << std::endl;
        return false;
      }
    }
    else {
      if (SDL_GL_SetSwapInterval(0) != 0) {
        std::cerr << "RenderManagerOpenGL::OpenDisplay: Warning: Could "
          "not set vertical retrace off"
          << std::endl;
        return false;
      }
    }
    return true;
  }
  bool handleEvents() {
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
};

// normally you'd load the shaders from a file, but in this case, let's
// just keep things simple and load from memory.
static const GLchar* vertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec3 position;\n"
    "layout(location = 1) in vec3 vertexColor;\n"
    "out vec3 fragmentColor;\n"
    "uniform mat4 modelView;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * modelView * vec4(position,1);\n"
    "   fragmentColor = vertexColor;\n"
    "}\n";

static const GLchar* fragmentShader = "#version 330 core\n"
                                      "in vec3 fragmentColor;\n"
                                      "out vec3 color;\n"
                                      "void main()\n"
                                      "{\n"
                                      "    color = fragmentColor;\n"
                                      "}\n";

class SampleShader {
  public:
    SampleShader() {}

    ~SampleShader() {
        if (initialized) {
            glDeleteProgram(programId);
        }
    }

    void init() {
        if (!initialized) {
            GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
            GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

            // vertex shader
            glShaderSource(vertexShaderId, 1, &vertexShader, NULL);
            glCompileShader(vertexShaderId);
            checkShaderError(vertexShaderId,
                             "Vertex shader compilation failed.");

            // fragment shader
            glShaderSource(fragmentShaderId, 1, &fragmentShader, NULL);
            glCompileShader(fragmentShaderId);
            checkShaderError(fragmentShaderId,
                             "Fragment shader compilation failed.");

            // linking program
            programId = glCreateProgram();
            glAttachShader(programId, vertexShaderId);
            glAttachShader(programId, fragmentShaderId);
            glLinkProgram(programId);
            checkProgramError(programId, "Shader program link failed.");

            // once linked into a program, we no longer need the shaders.
            glDeleteShader(vertexShaderId);
            glDeleteShader(fragmentShaderId);

            projectionUniformId = glGetUniformLocation(programId, "projection");
            modelViewUniformId = glGetUniformLocation(programId, "modelView");
            initialized = true;
        }
    }

    void useProgram(const GLdouble projection[], const GLdouble modelView[]) {
        init();
        glUseProgram(programId);
        GLfloat projectionf[16];
        GLfloat modelViewf[16];
        convertMatrix(projection, projectionf);
        convertMatrix(modelView, modelViewf);
        glUniformMatrix4fv(projectionUniformId, 1, GL_FALSE, projectionf);
        glUniformMatrix4fv(modelViewUniformId, 1, GL_FALSE, modelViewf);
    }

  private:
    SampleShader(const SampleShader&) = delete;
    SampleShader& operator=(const SampleShader&) = delete;
    bool initialized = false;
    GLuint programId = 0;
    GLuint projectionUniformId = 0;
    GLuint modelViewUniformId = 0;

    void checkShaderError(GLuint shaderId, const std::string& exceptionMsg) {
        GLint result = GL_FALSE;
        int infoLength = 0;
        glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLength);
        if (result == GL_FALSE) {
            std::vector<GLchar> errorMessage(infoLength + 1);
            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
            std::cerr << &errorMessage[0] << std::endl;
            throw std::runtime_error(exceptionMsg);
        }
    }

    void checkProgramError(GLuint programId, const std::string& exceptionMsg) {
        GLint result = GL_FALSE;
        int infoLength = 0;
        glGetProgramiv(programId, GL_LINK_STATUS, &result);
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLength);
        if (result == GL_FALSE) {
            std::vector<GLchar> errorMessage(infoLength + 1);
            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
            std::cerr << &errorMessage[0] << std::endl;
            throw std::runtime_error(exceptionMsg);
        }
    }

    void convertMatrix(const GLdouble source[], GLfloat dest_out[]) {
        if (nullptr == source || nullptr == dest_out) {
            throw new std::logic_error("source and dest_out must be non-null.");
        }
        for (int i = 0; i < 16; i++) {
            dest_out[i] = (GLfloat)source[i];
        }
    }
};
static SampleShader sampleShader;

class Cube {
  public:
    Cube(GLfloat scale) {
        colorBufferData = {1.0, 0.0, 0.0, // +X
                           1.0, 0.0, 0.0, 1.0, 0.0, 0.0,

                           1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0,

                           1.0, 0.0, 1.0, // -X
                           1.0, 0.0, 1.0, 1.0, 0.0, 1.0,

                           1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,

                           0.0, 1.0, 0.0, // +Y
                           0.0, 1.0, 0.0, 0.0, 1.0, 0.0,

                           0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0,

                           1.0, 1.0, 0.0, // -Y
                           1.0, 1.0, 0.0, 1.0, 1.0, 0.0,

                           1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0,

                           0.0, 0.0, 1.0, // +Z
                           0.0, 0.0, 1.0, 0.0, 0.0, 1.0,

                           0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0,

                           0.0, 1.0, 1.0, // -Z
                           0.0, 1.0, 1.0, 0.0, 1.0, 1.0,

                           0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0};

        vertexBufferData = {scale,  scale,  scale, // +X
                            scale,  -scale, -scale, scale,  scale,  -scale,

                            scale,  -scale, -scale, scale,  scale,  scale,
                            scale,  -scale, scale,

                            -scale, -scale, -scale, // -X
                            -scale, -scale, scale,  -scale, scale,  scale,

                            -scale, -scale, -scale, -scale, scale,  scale,
                            -scale, scale,  -scale,

                            scale,  scale,  scale, // +Y
                            scale,  scale,  -scale, -scale, scale,  -scale,

                            scale,  scale,  scale,  -scale, scale,  -scale,
                            -scale, scale,  scale,

                            scale,  -scale, scale, // -Y
                            -scale, -scale, -scale, scale,  -scale, -scale,

                            scale,  -scale, scale,  -scale, -scale, scale,
                            -scale, -scale, -scale,

                            -scale, scale,  scale, // +Z
                            -scale, -scale, scale,  scale,  -scale, scale,

                            scale,  scale,  scale,  -scale, scale,  scale,
                            scale,  -scale, scale,

                            scale,  scale,  -scale, // -Z
                            -scale, -scale, -scale, -scale, scale,  -scale,

                            scale,  scale,  -scale, scale,  -scale, -scale,
                            -scale, -scale, -scale};
    }

    ~Cube() {
        if (initialized) {
            glDeleteBuffers(1, &vertexBuffer);
            glDeleteVertexArrays(1, &vertexArrayId);
        }
    }

    void init() {
        if (!initialized) {
            // Vertex buffer
            glGenBuffers(1, &vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(vertexBufferData[0]) * vertexBufferData.size(),
                         &vertexBufferData[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Color buffer
            glGenBuffers(1, &colorBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(colorBufferData[0]) * colorBufferData.size(),
                         &colorBufferData[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Vertex array object
            glGenVertexArrays(1, &vertexArrayId);
            glBindVertexArray(vertexArrayId);
            {
                // color
                glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

                // VBO
                glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
            }
            glBindVertexArray(0);
            initialized = true;
        }
    }

    void draw(const GLdouble projection[], const GLdouble modelView[]) {
        init();

        sampleShader.useProgram(projection, modelView);

        glBindVertexArray(vertexArrayId);
        {
            glDrawArrays(GL_TRIANGLES, 0,
                         static_cast<GLsizei>(vertexBufferData.size()));
        }
        glBindVertexArray(0);
    }

  private:
    Cube(const Cube&) = delete;
    Cube& operator=(const Cube&) = delete;
    bool initialized = false;
    GLuint colorBuffer = 0;
    GLuint vertexBuffer = 0;
    GLuint vertexArrayId = 0;
    std::vector<GLfloat> colorBufferData;
    std::vector<GLfloat> vertexBufferData;
};

static Cube roomCube(5.0f);
static Cube handsCube(0.05f);

// Set to true when it is time for the application to quit.
// Handlers below that set it to true when the user causes
// any of a variety of events so that we shut down the system
// cleanly.  This only works on Windows.
static bool quit = false;

#ifdef _WIN32
// Note: On Windows, this runs in a different thread from
// the main application.
static BOOL CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
    // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        quit = true;
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

// This callback sets a boolean value whose pointer is passed in to
// the state of the button that was pressed.  This lets the callback
// be used to handle any button press that just needs to update state.
void myButtonCallback(void* userdata, const OSVR_TimeValue* /*timestamp*/,
                      const OSVR_ButtonReport* report) {
    bool* result = static_cast<bool*>(userdata);
    *result = (report->state != 0);
}

bool SetupRendering(osvr::renderkit::GraphicsLibrary library) {
    // Make sure our pointers are filled in correctly.
    if (library.OpenGL == nullptr) {
        std::cerr << "SetupRendering: No OpenGL GraphicsLibrary, this should "
                     "not happen"
                  << std::endl;
        return false;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    // Turn on depth testing, so we get correct ordering.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    return true;
}

// Callback to set up a given display, which may have one or more eyes in it
void SetupDisplay(
    void* userData //< Passed into SetDisplayCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    ) {
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "SetupDisplay: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr
            << "SetupDisplay: No OpenGL RenderBuffer, this should not happen"
            << std::endl;
        return;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    // Clear the screen to black and clear depth
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Callback to set up for rendering into a given eye (viewpoint and projection).
void SetupEye(
    void* userData //< Passed into SetViewProjectionCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    , osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport set by RenderManager
    , osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    , size_t whichEye //< Which eye are we setting up for?
    ) {
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "SetupEye: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr << "SetupEye: No OpenGL RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    // Set the viewport
    glViewport(static_cast<GLint>(viewport.left),
      static_cast<GLint>(viewport.lower),
      static_cast<GLint>(viewport.width),
      static_cast<GLint>(viewport.height));
}

// Callbacks to draw things in world space.
void DrawWorld(
    void* userData //< Passed into AddRenderCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    , osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    , OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    , osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    , OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "DrawWorld: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr << "DrawWorld: No OpenGL RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    GLdouble projectionGL[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projectionGL, projection);

    GLdouble viewGL[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(viewGL, pose);

    /// Draw a cube with a 5-meter radius as the room we are floating in.
    roomCube.draw(projectionGL, viewGL);
}

// This is used to draw both hands, but a different callback could be
// provided for each hand if desired.
void DrawHand(
    void* userData //< Passed into AddRenderCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    , osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    , OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    , osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    , OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "DrawHand: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr << "DrawHand: No OpenGL RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    GLdouble projectionGL[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projectionGL, projection);

    GLdouble viewGL[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(viewGL, pose);
    handsCube.draw(projectionGL, viewGL);
}

int main(int argc, char* argv[]) {
    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context(
        "com.osvr.renderManager.openGLExample");

    // Construct button devices and connect them to a callback
    // that will set the "quit" variable to true when it is
    // pressed.  Use button "1" on the left-hand or
    // right-hand controller.
    osvr::clientkit::Interface leftButton1 =
        context.getInterface("/controller/left/1");
    leftButton1.registerCallback(&myButtonCallback, &quit);

    osvr::clientkit::Interface rightButton1 =
        context.getInterface("/controller/right/1");
    rightButton1.registerCallback(&myButtonCallback, &quit);

    // Use SDL to open a window and then get an OpenGL context for us.
    // Note: This window is not the one that will be used for rendering.
    if (!osvr::renderkit::SDLInitQuit()) {
      std::cerr << "Could not initialize SDL"
        << std::endl;
      return 100;
    }
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_Window *myWindow = SDL_CreateWindow(
      "Test window, not used", 30, 30, 300, 100,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (myWindow == nullptr) {
      std::cerr << "SDL window open failed: Could not get window"
        << std::endl;
      return 101;
    }
    SDL_GLContext myGLContext;
    myGLContext = SDL_GL_CreateContext(myWindow);
    if (myGLContext == 0) {
      std::cerr << "RenderManagerOpenGL::addOpenGLContext: Could not get "
        "OpenGL context" << std::endl;
      return 102;
    }

    // Construct a graphics library which will be used both to create our
    // window and to create the RenderManager context window.
    auto toolkit = new SDLToolkitImpl();

    osvr::renderkit::GraphicsLibrary myLibrary;
    myLibrary.OpenGL = new osvr::renderkit::GraphicsLibraryOpenGL;
    myLibrary.OpenGL->toolkit = toolkit->getToolkit();

    // Open OpenGL and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    osvr::renderkit::RenderManager* render =
        osvr::renderkit::createRenderManager(context.get(), "OpenGL",
        myLibrary);

    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    // Set callback to handle setting up rendering in an eye
    render->SetViewProjectionCallback(SetupEye);

    // Set callback to handle setting up rendering in a display
    render->SetDisplayCallback(SetupDisplay);

    // Register callbacks to render things in left hand, right
    // hand, and world space.
    render->AddRenderCallback("/", DrawWorld);
    render->AddRenderCallback("/me/hands/left", DrawHand);
    render->AddRenderCallback("/me/hands/right", DrawHand);

// Set up a handler to cause us to exit cleanly.
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    // Open the display and make sure this worked.
    osvr::renderkit::RenderManager::OpenResults ret = render->OpenDisplay();
    if (ret.status == osvr::renderkit::RenderManager::OpenStatus::FAILURE) {
        std::cerr << "Could not open display" << std::endl;
        delete render;
        return 2;
    }
    if (ret.library.OpenGL == nullptr) {
        std::cerr << "Attempted to run an OpenGL program with a config file "
                  << "that specified a different rendering library."
                  << std::endl;
        return 3;
    }

    // Set up the rendering state we need.
    if (!SetupRendering(ret.library)) {
        return 3;
    }

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed ot initialize GLEW\n" << std::endl;
        return -1;
    }
    // Clear any GL error that Glew caused.  Apparently on Non-Windows
    // platforms, this can cause a spurious  error 1280.
    glGetError();

    // Continue rendering until it is time to quit.
    while (!quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        if (!render->Render()) {
            std::cerr
                << "Render() returned false, maybe because it was asked to quit"
                << std::endl;
            quit = true;
        }

        // Draw something in our window, just looping the background color
        static GLfloat bg = 0;
        SDL_GL_MakeCurrent(myWindow, myGLContext);
        glViewport(static_cast<GLint>(0),
          static_cast<GLint>(0),
          static_cast<GLint>(300),
          static_cast<GLint>(100));
        glClearColor(bg, bg, bg, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SDL_GL_SwapWindow(myWindow);
        bg += 0.003f;
        if (bg > 1) { bg = 0; }
    }

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
