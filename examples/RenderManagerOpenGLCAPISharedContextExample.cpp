/** @file
@brief Example program that uses the OSVR direct-to-display interface
and OpenGL to render a scene with low latency.

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

// Needed for render buffer calls.  OSVR will have called glewInit() for us
// when we open the display.
#include <GL/glew.h>

// We are going to use SDL to get our OpenGL context for us.
// Unfortunately, SDL.h has #define main    SDL_main in it, so
// we need to undefine main again so we can make our own below.
#include <osvr/RenderKit/RenderManagerSDLInitQuit.h>
#include <SDL.h>
#include <SDL_opengl.h>
#undef main

// Internal Includes
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/RenderKit/RenderManagerC.h>
#include <osvr/RenderKit/RenderManagerOpenGLC.h>
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>

// Library/third-party includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// @todo There shouldn't be two OSVR_ProjectionMatrix types in the API.
inline osvr::renderkit::OSVR_ProjectionMatrix ConvertProjectionMatrix(::OSVR_ProjectionMatrix matrix)
{
    osvr::renderkit::OSVR_ProjectionMatrix ret = { 0 };
    ret.bottom = matrix.bottom;
    ret.top = matrix.top;
    ret.left = matrix.left;
    ret.right = matrix.right;
    ret.nearClip = matrix.nearClip;
    ret.farClip = matrix.farClip;
    return ret;
}

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
    colorBufferData = { 1.0, 0.0, 0.0, // +X
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

      0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0 };

    vertexBufferData = { scale, scale, scale, // +X
      scale, -scale, -scale, scale, scale, -scale,

      scale, -scale, -scale, scale, scale, scale,
      scale, -scale, scale,

      -scale, -scale, -scale, // -X
      -scale, -scale, scale, -scale, scale, scale,

      -scale, -scale, -scale, -scale, scale, scale,
      -scale, scale, -scale,

      scale, scale, scale, // +Y
      scale, scale, -scale, -scale, scale, -scale,

      scale, scale, scale, -scale, scale, -scale,
      -scale, scale, scale,

      scale, -scale, scale, // -Y
      -scale, -scale, -scale, scale, -scale, -scale,

      scale, -scale, scale, -scale, -scale, scale,
      -scale, -scale, -scale,

      -scale, scale, scale, // +Z
      -scale, -scale, scale, scale, -scale, scale,

      scale, scale, scale, -scale, scale, scale,
      scale, -scale, scale,

      scale, scale, -scale, // -Z
      -scale, -scale, -scale, -scale, scale, -scale,

      scale, scale, -scale, scale, -scale, -scale,
      -scale, -scale, -scale };
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

// Set to true when it is time for the application to quit.
// Handlers below that set it to true when the user causes
// any of a variety of events so that we shut down the system
// cleanly.  This only works on Windows, but so does D3D...
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

bool SetupRendering() {

    // Turn on depth testing, so we get correct ordering.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    return true;
}

// Render the world from the specified point of view.
void RenderView(
    const OSVR_RenderInfoOpenGL& renderInfo, //< Info needed to render
    GLuint frameBuffer, //< Frame buffer object to bind our buffers to
    GLuint colorBuffer, //< Color buffer to render into
    GLuint depthBuffer  //< Depth buffer to render into
    ) {

    // Render to our framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    GLenum err = glGetError();

    // Set color and depth buffers for the frame buffer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, depthBuffer);

    // Set the list of draw buffers.
    GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

    // Always check that our framebuffer is ok
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "RenderView: Incomplete Framebuffer" << std::endl;
        return;
    }

    // Set the viewport
    glViewport(static_cast<GLint>(renderInfo.viewport.left),
      static_cast<GLint>(renderInfo.viewport.lower),
      static_cast<GLint>(renderInfo.viewport.width),
      static_cast<GLint>(renderInfo.viewport.height));

    // Clear the screen to black and clear depth
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    osvr::renderkit::OSVR_ProjectionMatrix projection =
      ConvertProjectionMatrix(renderInfo.projection);
    GLdouble projectionGL[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projectionGL, projection);

    GLdouble viewGL[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(viewGL, renderInfo.pose);

    /// Draw a cube with a 5-meter radius as the room we are floating in.
    roomCube.draw(projectionGL, viewGL);
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
    // Note: This window is not the one that will be used for rendering
    // the OSVR display, but one that will be cleared to a slowly-changing
    // constant color so we can see that we're able to render to both
    // contexts.
    if (!osvr::renderkit::SDLInitQuit()) {
      std::cerr << "Could not initialize SDL"
        << std::endl;
      return 100;
    }
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

    // Open OpenGL and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    OSVR_GraphicsLibraryOpenGL library;
    library.toolkit = nullptr;
    OSVR_RenderManager render;
    OSVR_RenderManagerOpenGL renderOGL;
    if (OSVR_RETURN_SUCCESS != osvrCreateRenderManagerOpenGL(
        context.get(), "OpenGL", library, &render, &renderOGL)) {
        std::cerr << "Could not create the RenderManager" << std::endl;
        return 1;
    }

    // Set up a handler to cause us to exit cleanly.
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    // Open the display and make sure this worked.
    OSVR_OpenResultsOpenGL openResults;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerOpenDisplayOpenGL(
        renderOGL, &openResults)) ||
        (openResults.status == OSVR_OPEN_STATUS_FAILURE)) {

        std::cerr << "Could not open display" << std::endl;
		osvrDestroyRenderManager(render);
		return 2;
    }

    // Set up the rendering state we need.
    if (!SetupRendering()) {
		osvrDestroyRenderManager(render);
		return 3;
    }

    // Do a call to get the information we need to construct our
    // color and depth render-to-texture buffers.
    context.update();

    OSVR_RenderParams renderParams;
    osvrRenderManagerGetDefaultRenderParams(&renderParams);
    OSVR_RenderInfoCollection renderInfoCollection;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoCollection(
        render, renderParams, &renderInfoCollection))) {
        std::cerr << "Could not get render info" << std::endl;
		osvrDestroyRenderManager(render);
		return 5;
    }

    OSVR_RenderInfoCount numRenderInfo;
    osvrRenderManagerGetNumRenderInfoInCollection(renderInfoCollection, &numRenderInfo);

    std::vector<OSVR_RenderBufferOpenGL> colorBuffers;
    std::vector<GLuint> depthBuffers; //< Depth/stencil buffers to render into

    // Initialize the sample shader with our window's context open,
    // so that our shaders will be associated with it.
    // NOTE: When the RenderManager internals are changed so that it
    // does not share an OpenGL context with the application, this
    // causes the display to be rendered black.  Because it now defaults
    // to doing this, we see the display in the window.
    SDL_GL_MakeCurrent(myWindow, myGLContext);

    // Construct the buffers we're going to need for our render-to-texture
    // code.
    GLuint frameBuffer; //< Groups a color buffer and a depth buffer
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

    OSVR_RenderManagerRegisterBufferState registerBufferState;
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerStartRegisterRenderBuffers(
        &registerBufferState))) {
        std::cerr << "Could not start registering render buffers" << std::endl;
		osvrDestroyRenderManager(render);
		return -4;
    }

    for (size_t i = 0; i < numRenderInfo; i++) {
        // Get the current render info
        OSVR_RenderInfoOpenGL renderInfo = { 0 };

        if (OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoFromCollectionOpenGL(
            renderInfoCollection, i, &renderInfo)) {
            std::cerr << "Could not get render info " << i << std::endl;
			osvrDestroyRenderManager(render);
			return 1;
        }

        // Determine the appropriate size for the frame buffer to be used for
        // this eye.
        int width = static_cast<int>(renderInfo.viewport.width);
        int height = static_cast<int>(renderInfo.viewport.height);

        // The color buffer for this eye.  We need to put this into
        // a generic structure for the Present function, but we only need
        // to fill in the OpenGL portion.
        //  Note that this must be used to generate a RenderBuffer, not just
        // a texture, if we want to be able to present it to be rendered
        // via Direct3D for DirectMode.  This is selected based on the
        // config file value, so we want to be sure to use the more general
        // case.
        //  Note that this texture format must be RGBA and unsigned byte,
        // so that we can present it to Direct3D for DirectMode
        GLuint colorBufferName = 0;
        if (OSVR_RETURN_SUCCESS
              != osvrRenderManagerCreateColorBufferOpenGL(width, height, GL_RGBA,
              &colorBufferName)) {
            std::cerr << "Could not create color buffer." << std::endl;
			osvrDestroyRenderManager(render);
			return -5;
        }

        OSVR_RenderBufferOpenGL rb;
        rb.colorBufferName = colorBufferName;
        colorBuffers.push_back(rb);

        // The depth buffer
        GLuint depthrenderbuffer;
        if (OSVR_RETURN_SUCCESS
            != osvrRenderManagerCreateDepthBufferOpenGL(width, height, &depthrenderbuffer)) {
            std::cerr << "Could not create depth buffer." << std::endl;
			osvrDestroyRenderManager(render);
			return -5;
        }
        rb.depthStencilBufferName = depthrenderbuffer;
        depthBuffers.push_back(depthrenderbuffer);

        if (OSVR_RETURN_SUCCESS != osvrRenderManagerRegisterRenderBufferOpenGL(
            registerBufferState, rb)) {
            std::cerr << "Could not register render buffer " << i << std::endl;
			osvrDestroyRenderManager(render);
			return -5;
        }
    }
    osvrRenderManagerReleaseRenderInfoCollection(renderInfoCollection);

    // Register our constructed buffers so that we can use them for
    // presentation.
    if ((OSVR_RETURN_SUCCESS != osvrRenderManagerFinishRegisterRenderBuffers(
        render, registerBufferState, false))) {
        std::cerr << "Could not start finish registering render buffers" << std::endl;
        quit = true;
    }

    // Continue rendering until it is time to quit.
    while (!quit) {

        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        //renderInfo = render->GetRenderInfo();

        OSVR_RenderInfoCollection renderInfoCollection = { 0 };
        if (OSVR_RETURN_SUCCESS != osvrRenderManagerGetRenderInfoCollection(
            render, renderParams, &renderInfoCollection)) {
            std::cerr << "Could not get render info in the main loop" << std::endl;
			osvrDestroyRenderManager(render);
			return -1;
        }

        osvrRenderManagerGetNumRenderInfoInCollection(renderInfoCollection, &numRenderInfo);

        // Render into each buffer using the specified information.
        for (size_t i = 0; i < numRenderInfo; i++) {
            OSVR_RenderInfoOpenGL renderInfo = { 0 };
            osvrRenderManagerGetRenderInfoFromCollectionOpenGL(
                renderInfoCollection, i, &renderInfo);

            RenderView(renderInfo, frameBuffer,
                colorBuffers[i].colorBufferName,
                depthBuffers[i]);
        }
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
          std::cout << "After OSVR rendering: OpenGL error " << err << std::endl;
        }

        OSVR_RenderManagerPresentState presentState;
        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerStartPresentRenderBuffers(
            &presentState))) {
            std::cerr << "Could not start presenting render buffers" << std::endl;
			osvrDestroyRenderManager(render);
			return 201;
        }
        OSVR_ViewportDescription fullView;
        fullView.left = fullView.lower = 0;
        fullView.width = fullView.height = 1;
        for (size_t i = 0; i < numRenderInfo; i++) {
            OSVR_RenderInfoOpenGL renderInfo = { 0 };
            osvrRenderManagerGetRenderInfoFromCollectionOpenGL(
                renderInfoCollection, i, &renderInfo);

            if ((OSVR_RETURN_SUCCESS != osvrRenderManagerPresentRenderBufferOpenGL(
                presentState, colorBuffers[i], renderInfo, fullView))) {
                std::cerr << "Could not present render buffer " << i << std::endl;
                quit = true;
            }
        }

        if ((OSVR_RETURN_SUCCESS != osvrRenderManagerFinishPresentRenderBuffers(
            render, presentState, renderParams, false))) {
            std::cerr << "Could not finish presenting render buffers" << std::endl;
            quit = true;
        }

        osvrRenderManagerReleaseRenderInfoCollection(renderInfoCollection);

        // Draw something in our window, just looping the background color.
        // Note that we need to bind the correct framebuffer (0 in this case)
        // because we're binding a different one in our draw calls.
        static GLfloat bg = 0;
        SDL_GL_MakeCurrent(myWindow, myGLContext);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    // Clean up after ourselves.
    glDeleteFramebuffers(1, &frameBuffer);
    for (size_t i = 0; i < colorBuffers.size(); i++) {
        glDeleteTextures(1, &colorBuffers[i].colorBufferName);
        glDeleteRenderbuffers(1, &depthBuffers[i]);
    }

    // Close the Renderer interface cleanly.
    osvrDestroyRenderManager(render);

    return 0;
}
