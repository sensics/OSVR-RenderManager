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

// Internal Includes
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/RenderKit/RenderManager.h>

// Needed for render buffer calls.  OSVR will have called glewInit() for us
// when we open the display.
#include <GL/glew.h>

// Library/third-party includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// Standard includes
#include <iostream>
#include <string>
#include <chrono>
#include <stdlib.h> // For exit()

// This must come after we include <GL/gl.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>

// Forward declarations of rendering functions defined below.
void draw_cube(double radius);

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

bool SetupRendering(osvr::renderkit::GraphicsLibrary library) {
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr << "SetupRendering: No OpenGL GraphicsLibrary, this should "
                     "not happen"
                  << std::endl;
        return false;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    // Turn on depth testing, so we get correct ordering.
    glEnable(GL_DEPTH_TEST);

    return true;
}

// Render the world from the specified point of view.
void RenderView(
    size_t eye, //< Which eye are we rendering
    const osvr::renderkit::RenderInfo& renderInfo, //< Info needed to render
    GLuint frameBuffer, //< Frame buffer object to bind our buffers to
    GLuint colorBuffer, //< Color buffer to render into
    GLuint depthBuffer  //< Depth buffer to render into
    ) {
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (renderInfo.library.OpenGL == nullptr) {
        std::cerr
            << "RenderView: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }

    // Render to our framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

    // Set color and depth buffers for the frame buffer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthBuffer);

    // Set the list of draw buffers.
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

    // Always check that our framebuffer is ok
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "RenderView: Incomplete Framebuffer" << std::endl;
        return;
    }

    // Set the viewport to cover the fraction of our render buffer that
    // this eye is responsible for.  This is always the same width and
    // height but shifts over by one width for each eye.
    glViewport(static_cast<GLsizei>(eye * renderInfo.viewport.width), 0,
               static_cast<GLsizei>(renderInfo.viewport.width),
               static_cast<GLsizei>(renderInfo.viewport.height));

    // Set the OpenGL projection matrix
    GLdouble projection[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projection,
                                               renderInfo.projection);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMultMatrixd(projection);

    /// Put the transform into the OpenGL ModelView matrix
    GLdouble modelView[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(modelView, renderInfo.pose);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMultMatrixd(modelView);

    // Only on the first eye, clear the screen to black and clear depth.
    // glClear() does not respect the viewport, so will clear all earlier
    // eyes on the later renderings if we call it then.
    if (eye == 0) {
        glClearColor(0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // =================================================================
    // This is where we draw our world and hands and any other objects.
    // We're in World Space.  To find out about where to render objects
    // in OSVR spaces (like left/right hand space) we need to query the
    // interface and handle the coordinate tranforms ourselves.

    // Draw a cube with a 5-meter radius as the room we are floating in.
    draw_cube(5.0);
}

void Usage(std::string name) {
    std::cerr << "Usage: " << name << " millisecondRenderingDelay" << std::endl;
    exit(-1);
}

int main(int argc, char* argv[]) {
    // Parse the command line
    int delayMilliSeconds = 0;
    int realParams = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            Usage(argv[0]);
        } else
            switch (++realParams) {
            case 1:
                delayMilliSeconds = atoi(argv[i]);
                break;
            default:
                Usage(argv[0]);
            }
    }
    if (realParams > 1) {
        Usage(argv[0]);
    }

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

    // Open OpenGL and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    osvr::renderkit::RenderManager* render =
        osvr::renderkit::createRenderManager(context.get(), "OpenGL");

    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

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

    // Set up the rendering state we need.
    if (!SetupRendering(ret.library)) {
        return 3;
    }

    // Do a call to get the information we need to construct our
    // color and depth render-to-texture buffers.
    std::vector<osvr::renderkit::RenderInfo> renderInfo;
    context.update();
    renderInfo = render->GetRenderInfo();

    // Make sure that all of the viewports in the eyes are
    // the same, so that we can generate a single buffer to
    // consolidate them all by just abutting them side by
    // side.
    for (size_t i = 1; i < renderInfo.size(); i++) {
        if (renderInfo[0].viewport != renderInfo[i].viewport) {
            std::cerr << "Viewport " << i << " != Viewport 0" << std::endl;
            return 4;
        }
    }

    // Construct the buffer we're going to need for our render-to-texture
    // code.  We're just going to make one buffer, but make it wide enough
    // to handle all of the eyes we need.
    osvr::renderkit::RenderBuffer colorBuffer;
    GLuint depthBuffer; //< Depth/stencil buffer to render into
    GLuint frameBuffer; //< Groups a color buffer and a depth buffer
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

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
    glGenRenderbuffers(1, &colorBufferName);
    osvr::renderkit::RenderBuffer rb;
    rb.OpenGL = new osvr::renderkit::RenderBufferOpenGL;
    rb.OpenGL->colorBufferName = colorBufferName;
    colorBuffer = rb;

    // "Bind" the newly created texture : all future texture
    // functions will modify this texture glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBufferName);

    // Determine the appropriate size for the frame buffer to be used for
    // all eyes when placed horizontally size by side.
    int width =
        static_cast<int>(renderInfo[0].viewport.width * renderInfo.size());
    int height = static_cast<int>(renderInfo[0].viewport.height);

    // Give an empty image to OpenGL ( the last "0" means "empty" )
    // Note that whether or not the second GL_RGBA is turned into
    // GL_BGRA, the first one should remain GL_RGBA -- it is specifying
    // the size.  If the second is changed to GL_RGB or GL_BGR, then
    // the first should become GL_RGB.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, 0);

    // Bilinear filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // The depth buffer
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

    // Construct our vector of color buffers, which will all point to
    // the same one.
    // Also construct our vector of normalized viewports that we will
    // pass to PresentRenderBuffers.  This describes the region of
    // the normalized texture coordinates (0,0) to (1,1) that is
    // handled by each eye.  The eyes are stacked left to right
    // in the same buffer.
    std::vector<osvr::renderkit::RenderBuffer> colorBuffers;
    double fraction = 1.0 / renderInfo.size();
    std::vector<osvr::renderkit::OSVR_ViewportDescription> NVCPs;
    for (size_t i = 0; i < renderInfo.size(); i++) {
        colorBuffers.push_back(colorBuffer);

        osvr::renderkit::OSVR_ViewportDescription v;
        v.left = fraction * i;
        v.lower = 0.0;
        v.width = fraction;
        v.height = 1;
        NVCPs.push_back(v);
    }

    // Register our constructed buffer so that we can use it for
    // presentation.  It is okay to have multiple instances of
    // the same buffer.  If we wanted, we could have constructed
    // a vector with our single buffer in it and only registered
    // that.
    if (!render->RegisterRenderBuffers(colorBuffers)) {
        std::cerr << "RegisterRenderBuffers() returned false, cannot continue"
                  << std::endl;
        quit = true;
    }

    // Continue rendering until it is time to quit.
    while (!quit) {

        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        renderInfo = render->GetRenderInfo();

        // Render into each buffer using the specified information.
        for (size_t i = 0; i < renderInfo.size(); i++) {
            RenderView(i, renderInfo[i], frameBuffer,
                       colorBuffer.OpenGL->colorBufferName, depthBuffer);
        }

        // Delay the requested length of time.
        // Busy-wait so we don't get swapped out longer than we wanted.
        auto end = std::chrono::high_resolution_clock::now() +
                   std::chrono::milliseconds(delayMilliSeconds);
        do {
        } while (std::chrono::high_resolution_clock::now() < end);

        // Send the rendered results to the screen
        if (!render->PresentRenderBuffers(
                colorBuffers, renderInfo,
                osvr::renderkit::RenderManager::RenderParams(), NVCPs)) {
            std::cerr << "PresentRenderBuffers() returned false, maybe because "
                         "it was asked to quit"
                      << std::endl;
            quit = true;
        }
    }

    // Clean up after ourselves.
    glDeleteFramebuffers(1, &frameBuffer);
    glDeleteTextures(1, &colorBuffer.OpenGL->colorBufferName);
    delete colorBuffer.OpenGL;
    glDeleteRenderbuffers(1, &depthBuffer);

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}

static GLfloat matspec[4] = {0.5, 0.5, 0.5, 0.0};
static float red_col[] = {1.0, 0.0, 0.0};
static float grn_col[] = {0.0, 1.0, 0.0};
static float blu_col[] = {0.0, 0.0, 1.0};
static float yel_col[] = {1.0, 1.0, 0.0};
static float lightblu_col[] = {0.0, 1.0, 1.0};
static float pur_col[] = {1.0, 0.0, 1.0};

void draw_cube(double radius) {
    GLfloat matspec[4] = {0.5, 0.5, 0.5, 0.0};
    glPushMatrix();
    glScaled(radius, radius, radius);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matspec);
    glMaterialf(GL_FRONT, GL_SHININESS, 64.0);
    glBegin(GL_POLYGON);
    glColor3fv(lightblu_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, lightblu_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, lightblu_col);
    glNormal3f(0.0, 0.0, -1.0);
    glVertex3f(1.0, 1.0, -1.0);
    glVertex3f(1.0, -1.0, -1.0);
    glVertex3f(-1.0, -1.0, -1.0);
    glVertex3f(-1.0, 1.0, -1.0);
    glEnd();
    glBegin(GL_POLYGON);
    glColor3fv(blu_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, blu_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, blu_col);
    glNormal3f(0.0, 0.0, 1.0);
    glVertex3f(-1.0, 1.0, 1.0);
    glVertex3f(-1.0, -1.0, 1.0);
    glVertex3f(1.0, -1.0, 1.0);
    glVertex3f(1.0, 1.0, 1.0);
    glEnd();
    glBegin(GL_POLYGON);
    glColor3fv(yel_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, yel_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, yel_col);
    glNormal3f(0.0, -1.0, 0.0);
    glVertex3f(1.0, -1.0, 1.0);
    glVertex3f(-1.0, -1.0, 1.0);
    glVertex3f(-1.0, -1.0, -1.0);
    glVertex3f(1.0, -1.0, -1.0);
    glEnd();
    glBegin(GL_POLYGON);
    glColor3fv(grn_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, grn_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, grn_col);
    glNormal3f(0.0, 1.0, 0.0);
    glVertex3f(1.0, 1.0, 1.0);
    glVertex3f(1.0, 1.0, -1.0);
    glVertex3f(-1.0, 1.0, -1.0);
    glVertex3f(-1.0, 1.0, 1.0);
    glEnd();
    glBegin(GL_POLYGON);
    glColor3fv(pur_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, pur_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pur_col);
    glNormal3f(-1.0, 0.0, 0.0);
    glVertex3f(-1.0, 1.0, 1.0);
    glVertex3f(-1.0, 1.0, -1.0);
    glVertex3f(-1.0, -1.0, -1.0);
    glVertex3f(-1.0, -1.0, 1.0);
    glEnd();
    glBegin(GL_POLYGON);
    glColor3fv(red_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, red_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, red_col);
    glNormal3f(1.0, 0.0, 0.0);
    glVertex3f(1.0, -1.0, 1.0);
    glVertex3f(1.0, -1.0, -1.0);
    glVertex3f(1.0, 1.0, -1.0);
    glVertex3f(1.0, 1.0, 1.0);
    glEnd();
    glPopMatrix();
}
