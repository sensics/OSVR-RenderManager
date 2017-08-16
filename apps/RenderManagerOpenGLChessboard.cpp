/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and the OpenGL Core profile to render a scene that has lots
           of polygons.  This can be used to do speed tests on various
           cards, or regression tests on new versions.

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
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/RenderKit/RenderManager.h>

// just where this header happens to be.
#include <osvr/Server/RegisterShutdownHandler.h>

// Library/third-party includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/glew.h>

// Standard includes
#include <iostream>
#include <string>
#include <chrono>
#include <stdlib.h> // For exit()

// This must come after we include <GL/gl.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>

// normally you'd load the shaders from a file, but in this case, let's
// just keep things simple and load from memory.
static const GLchar* vertexShader = "#version 330 core\n"
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
            checkShaderError(vertexShaderId, "Vertex shader compilation failed.");

            // fragment shader
            glShaderSource(fragmentShaderId, 1, &fragmentShader, NULL);
            glCompileShader(fragmentShaderId);
            checkShaderError(fragmentShaderId, "Fragment shader compilation failed.");

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

inline size_t checkerBoard(size_t i, size_t j) { return (i % 2) ^ (j % 2); }

class MeshCube {
  public:
    MeshCube(GLfloat scale, size_t numTriangles = 6 * 2 * 15 * 15) {
        // Figure out how many quads we have per edge.  There
        // is a minimum of 1.
        size_t numQuads = numTriangles / 2;
        size_t numQuadsPerFace = numQuads / 6;
        size_t numQuadsPerEdge = static_cast<size_t>(sqrt(numQuadsPerFace));
        if (numQuadsPerEdge < 1) {
            numQuadsPerEdge = 1;
        }
        static const auto MAX_INTENSITY = 0.7f;
        // Construct a white square with the specified number of
        // quads as the +Z face of the cube.  We'll copy this and
        // then multiply by the correct face color, and we'll
        // adjust the coordinates by rotation to match each face.
        std::vector<GLfloat> whiteBufferData;
        std::vector<GLfloat> faceBufferData;
        auto getXorY = [&](size_t index) {
            GLfloat val = -(2 * scale) + index * (4 * scale) / numQuadsPerEdge;
            return val;
        };
        for (size_t i = 0; i < numQuadsPerEdge; i++) {
            for (size_t j = 0; j < numQuadsPerEdge; j++) {

                // Modulate the color of each quad by a random luminance,
                // leaving all vertices the same color.
                GLfloat color = checkerBoard(i, j) * MAX_INTENSITY;
                const size_t numTris = 2;
                const size_t numColors = 3;
                const size_t numVerts = 3;
                for (size_t c = 0; c < numColors * numTris * numVerts; c++) {
                    whiteBufferData.push_back(color);
                }

                // Send the two triangles that make up this quad, where the
                // quad covers the appropriate fraction of the face from
                // -scale to scale in X and Y.
                GLfloat Z = scale;
                GLfloat minX = getXorY(i);
                GLfloat maxX = getXorY(i + 1);
                GLfloat minY = getXorY(j);
                GLfloat maxY = getXorY(j + 1);

                faceBufferData.push_back(minX);
                faceBufferData.push_back(maxY);
                faceBufferData.push_back(Z);

                faceBufferData.push_back(minX);
                faceBufferData.push_back(minY);
                faceBufferData.push_back(Z);

                faceBufferData.push_back(maxX);
                faceBufferData.push_back(minY);
                faceBufferData.push_back(Z);

                faceBufferData.push_back(maxX);
                faceBufferData.push_back(maxY);
                faceBufferData.push_back(Z);

                faceBufferData.push_back(minX);
                faceBufferData.push_back(maxY);
                faceBufferData.push_back(Z);

                faceBufferData.push_back(maxX);
                faceBufferData.push_back(minY);
                faceBufferData.push_back(Z);
            }
        }

        std::array<GLfloat, 3> whiteModColor = {1.0f, 1.0f, 1.0f};
        // Make a copy of the vertices for each face, then modulate
        // the color by the face color and rotate the coordinates to
        // put them on the correct cube face.

        // -Z is in the opposite size from the
        // original face (mirror all 3).
        {
            std::vector<GLfloat> myBufferData = colorModulate(whiteBufferData, whiteModColor);

            // X = -X, Y = -Y, Z = -Z
            std::array<GLfloat, 3> scales = {-1.0f, -1.0f, -1.0f};
            std::array<size_t, 3> indices = {0, 1, 2};
            std::vector<GLfloat> myFaceBufferData = vertexRotate(faceBufferData, indices, scales);

            // Catenate the colors onto the end of the
            // color buffer.
            colorBufferData.insert(colorBufferData.end(), myBufferData.begin(), myBufferData.end());

            // Catenate the vertices onto the end of the
            // vertex buffer.
            vertexBufferData.insert(vertexBufferData.end(), myFaceBufferData.begin(), myFaceBufferData.end());
        }
    }

    ~MeshCube() {
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
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertexBufferData[0]) * vertexBufferData.size(), &vertexBufferData[0],
                         GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Color buffer
            glGenBuffers(1, &colorBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(colorBufferData[0]) * colorBufferData.size(), &colorBufferData[0],
                         GL_STATIC_DRAW);
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
        { glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexBufferData.size())); }
        glBindVertexArray(0);
    }

  private:
    MeshCube(const MeshCube&) = delete;
    MeshCube& operator=(const MeshCube&) = delete;
    bool initialized = false;
    GLuint colorBuffer = 0;
    GLuint vertexBuffer = 0;
    GLuint vertexArrayId = 0;
    std::vector<GLfloat> colorBufferData;
    std::vector<GLfloat> vertexBufferData;

    // Multiply each triple of colors by the specified color.
    static std::vector<GLfloat> colorModulate(std::vector<GLfloat> const& inVec, std::array<GLfloat, 3> const& clr) {
        std::vector<GLfloat> out;
        size_t elements = inVec.size() / 3;
        if (elements * 3 != inVec.size()) {
            // We don't have an even multiple of 3 elements, so bail.
            return out;
        }
        out = inVec;
        for (size_t i = 0; i < elements; i++) {
            for (size_t c = 0; c < 3; c++) {
                out[3 * i + c] *= clr[c];
            }
        }
        return out;
    }

    // Swizzle each triple of coordinates by the specified
    // index and then multiply by the specified scale.  This
    // lets us implement a poor-man's rotation matrix, where
    // we pick which element (0-2) and which polarity (-1 or
    // 1) to use.
    static std::vector<GLfloat> vertexRotate(std::vector<GLfloat> const& inVec, std::array<size_t, 3> const& indices,
                                             std::array<GLfloat, 3> const& scales) {
        std::vector<GLfloat> out;
        size_t elements = inVec.size() / 3;
        if (elements * 3 != inVec.size()) {
            // We don't have an even multiple of 3 elements, so bail.
            return out;
        }
        out.resize(inVec.size());
        for (size_t i = 0; i < elements; i++) {
            for (size_t p = 0; p < 3; p++) {
                out[3 * i + p] = inVec[3 * i + indices[p]] * scales[p];
            }
        }
        return out;
    }
};

static std::unique_ptr<MeshCube> roomCube;

// Set to true when it is time for the application to quit.
typedef struct QuitStruct { volatile bool quit = false; } QuitStruct;
static QuitStruct quit;

// Note: On Windows, this runs in a different thread from
// the main application.
static void CtrlHandler() { quit.quit = true; }

// This callback sets a boolean value whose pointer is passed in to
// the state of the button that was pressed.  This lets the callback
// be used to handle any button press that just needs to update state.
void myButtonCallback(void* userdata, const OSVR_TimeValue* /*timestamp*/, const OSVR_ButtonReport* report) {
    QuitStruct* result = static_cast<QuitStruct*>(userdata);
    result->quit = (report->state != 0);
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
void SetupDisplay(void* userData,                           //< Passed into SetDisplayCallback
                  osvr::renderkit::GraphicsLibrary library, //< Graphics library context to use
                  osvr::renderkit::RenderBuffer buffers     //< Buffers to use
                  ) {

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    // Clear the screen to black and clear depth
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Callback to set up for rendering into a given eye (viewpoint and projection).
void SetupEye(void* userData,                                     //< Passed into SetViewProjectionCallback
              osvr::renderkit::GraphicsLibrary library,           //< Graphics library context to use
              osvr::renderkit::RenderBuffer buffers,              //< Buffers to use
              osvr::renderkit::OSVR_ViewportDescription viewport, //< Viewport set by RenderManager
              osvr::renderkit::OSVR_ProjectionMatrix projection,  //< Projection matrix set by RenderManager
              size_t whichEye                                     //< Which eye are we setting up for?
              ) {

    // Set the viewport
    glViewport(static_cast<GLint>(viewport.left), static_cast<GLint>(viewport.lower),
               static_cast<GLint>(viewport.width), static_cast<GLint>(viewport.height));
}

// Callback to draw things in world space.
void DrawWorld(void* userData, //< Passed into AddRenderCallback
              osvr::renderkit::GraphicsLibrary library,           //< Graphics library context to use
              osvr::renderkit::RenderBuffer buffers,              //< Buffers to use
              osvr::renderkit::OSVR_ViewportDescription viewport, //< Viewport we're rendering into
              OSVR_PoseState pose,                                //< OSVR ModelView matrix set by RenderManager
              osvr::renderkit::OSVR_ProjectionMatrix projection,  //< Projection matrix set by RenderManager
              OSVR_TimeValue deadline                             //< When the frame should be sent to the screen
              ) {

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    GLdouble projectionGL[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projectionGL, projection);

    GLdouble viewGL[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(viewGL, pose);

    /// Draw a cube with a 5-meter radius as the room we are floating in.
    roomCube->draw(projectionGL, viewGL);
}

void Usage(std::string name) {
    std::cerr << "Usage: " << name << " [TrianglesPerSide]" << std::endl;
    std::cerr << "       Default triangles per cube face = 1e3" << std::endl;

    exit(-1);
}

int main(int argc, char* argv[]) {
    // Parse the command line
    double trianglesPerSide = 1e3;
    int realParams = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            Usage(argv[0]);
        } else {
            switch (++realParams) {
            case 1:
                trianglesPerSide = atof(argv[i]);
                break;
            default:
                Usage(argv[0]);
            }
        }
    }
    if (realParams > 1) {
        Usage(argv[0]);
    }
    size_t triangles = static_cast<size_t>(trianglesPerSide * 6);
    roomCube.reset(new MeshCube(10.0, triangles));
    std::cout << "Rendering " << trianglesPerSide << " triangles per cube face" << std::endl;
    std::cout << "Rendering " << triangles << " triangles total" << std::endl;

    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context("org.osvr.renderManager.openGLChessboard");

    // Open OpenGL and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    std::unique_ptr<osvr::renderkit::RenderManager> render(
        osvr::renderkit::createRenderManager(context.get(), "OpenGL"));

    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    // Set callback to handle setting up rendering in an eye
    render->SetViewProjectionCallback(SetupEye);

    // Set callback to handle setting up rendering in a display
    render->SetDisplayCallback(SetupDisplay);

    // Register callback to render things in world space.
    render->AddRenderCallback("/", DrawWorld);

    // Set up a handler to cause us to exit cleanly.
    osvr::server::registerShutdownHandler<&CtrlHandler>();

    // Open the display and make sure this worked.
    osvr::renderkit::RenderManager::OpenResults ret = render->OpenDisplay();
    if (ret.status == osvr::renderkit::RenderManager::OpenStatus::FAILURE) {
        std::cerr << "Could not open display" << std::endl;
        return 2;
    }
    if (ret.library.OpenGL == nullptr) {
        std::cerr << "Attempted to run an OpenGL program with a config file "
                  << "that specified a different rendering library." << std::endl;
        return 3;
    }

    // Set up the rendering state we need.
    if (!SetupRendering(ret.library)) {
        return 3;
    }

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n" << std::endl;
        return -1;
    }
    // Clear any GL error that Glew caused.  Apparently on Non-Windows
    // platforms, this can cause a spurious  error 1280.
    glGetError();

    // Frame timing
    size_t countFrames = 0;

    // Always render from the identity pose, not allowing head tracking to operate.
    osvr::renderkit::RenderManager::RenderParams params;
    OSVR_PoseState identity;
    osvrPose3SetIdentity(&identity);
    params.roomFromHeadReplace = &identity;

    // Continue rendering until it is time to quit.
    using ourClock = std::chrono::high_resolution_clock;
    auto start = ourClock::now();
    while (!quit.quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        if (!render->Render(params)) {
          std::cerr << "Render() returned false, maybe because it was asked to quit" << std::endl;
            quit.quit = true;
        }

        // Print timing info
        auto now = ourClock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
        countFrames++;
        if (duration >= 2.0) {
            std::cout << "Rendering at " << countFrames / duration << " fps" << std::endl;
            start = now;
            countFrames = 0;
        }
    }

    return 0;
}
