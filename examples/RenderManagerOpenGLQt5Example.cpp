/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and OpenGL to render a scene with low latency, using a custom
           OpenGL window library (in this case, Qt5) rather than the built-in
           SDL2 library.

    @date 2016

    @author
    Steffen Kiess
    <http://sensics.com/osvr>
*/

// Copyright 2016 Steffen Kiess
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

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <QtGui/QOpenGLFunctions_3_2_Compatibility>

#include <QtOpenGL/QGLWidget>

#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>

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
#include <stdlib.h> // For exit()

// This must come after we include <GL/gl.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>

void myButtonCallback(void* userdata, const OSVR_TimeValue* /*timestamp*/,
                      const OSVR_ButtonReport* report) {
    QApplication::instance()->quit();
}

static GLfloat matspec[4] = {0.5, 0.5, 0.5, 0.0};
static float red_col[] = {1.0, 0.0, 0.0};
static float grn_col[] = {0.0, 1.0, 0.0};
static float blu_col[] = {0.0, 0.0, 1.0};
static float yel_col[] = {1.0, 1.0, 0.0};
static float lightblu_col[] = {0.0, 1.0, 1.0};
static float pur_col[] = {1.0, 0.0, 1.0};

//==========================================================================
// Toolkit object to handle our window creation needs.  We pass it down to
// the RenderManager and it is to make windows in the same context that
// we are making them in.  RenderManager will call its functions to make them.
// @todo This code crashes when it is used to create two windows.  The
// standard SDL code is able to create and render to both windows.

class Qt5ToolkitImpl {
    OSVR_OpenGLToolkitFunctions toolkit;

    static void createImpl(void* data) {
    }
    static void destroyImpl(void* data) {
        delete ((Qt5ToolkitImpl*)data);
    }
    static OSVR_CBool addOpenGLContextImpl(void* data, const OSVR_OpenGLContextParams* p) {
        return ((Qt5ToolkitImpl*)data)->addOpenGLContext(p);
    }
    static OSVR_CBool removeOpenGLContextsImpl(void* data) {
        return ((Qt5ToolkitImpl*)data)->removeOpenGLContexts();
    }
    static OSVR_CBool makeCurrentImpl(void* data, size_t display) {
        return ((Qt5ToolkitImpl*)data)->makeCurrent(display);
    }
    static OSVR_CBool swapBuffersImpl(void* data, size_t display) {
        return ((Qt5ToolkitImpl*)data)->swapBuffers(display);
    }
    static OSVR_CBool setVerticalSyncImpl(void* data, OSVR_CBool verticalSync) {
        return ((Qt5ToolkitImpl*)data)->setVerticalSync(verticalSync);
    }
    static OSVR_CBool handleEventsImpl(void* data) {
        return ((Qt5ToolkitImpl*)data)->handleEvents();
    }

    QList<QGLWidget*> glwidgets;
    QList<QWidget*> widgets;

  public:
    Qt5ToolkitImpl() {
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

    ~Qt5ToolkitImpl() {
    }

    const OSVR_OpenGLToolkitFunctions* getToolkit() const { return &toolkit; }

    bool addOpenGLContext(const OSVR_OpenGLContextParams* p) {
        auto widget = new QWidget();

        /*
        qDebug() << "Size:" << p.width << p.height;
        qDebug() << "Pos:" << p.xPos << p.yPos;
        qDebug() << "Dis:" << p.displayIndex << p.fullScreen;
        // */

        widget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        widget->setGeometry(p->xPos, p->yPos, p->width, p->height);
        widget->setWindowTitle(p->windowTitle);
        if (p->fullScreen) {
          widget->setWindowState(Qt::WindowFullScreen);
        }

        auto layout = new QHBoxLayout(widget);
        layout->setMargin(0);
        layout->setSpacing(0);
        QGLWidget *gl;
        // If we're creating multiple windows, share the context from the
        // first one and be able to share display lists and objects with
        // the other widget.
        // @todo This is not working -- the second window is never exposed
        // and never drawn to.  If you keep the windows from being
        // frameless, then the second window's title and region show up but
        // the glwidget is apparently never exposed.
        if (glwidgets.size()) {
          gl = new QGLWidget(nullptr, nullptr, glwidgets.at(0));
          gl->context()->create(glwidgets.at(0)->context());
        }
        else {
          gl = new QGLWidget();
        }
        layout->addWidget(gl);

        if (p->visible) {
          widget->show();
        }

        //qDebug() << widget->geometry();

        glwidgets.push_back(gl);
        widgets.push_back(widget);

        return true;
    }
    bool removeOpenGLContexts() {
        for (auto widget : widgets) {
          widget->deleteLater();
        }
        widgets.clear();
        glwidgets.clear();

        return true;
    }
    bool makeCurrent(size_t display) {
        glwidgets.at(static_cast<int>(display))->makeCurrent();
        return true;
    }
    bool swapBuffers(size_t display) {
        glwidgets.at(static_cast<int>(display))->swapBuffers();
        return true;
    }
    bool setVerticalSync(bool verticalSync) {
      return true;
    }
    bool handleEvents() {
      return true;
    }
};

class RenderManagerOpenGLQt5Example : public QOpenGLFunctions_3_2_Compatibility {
  public:
    bool SetupRendering(osvr::renderkit::GraphicsLibrary library) {
        // Make sure our pointers are filled in correctly.  The config file selects
        // the graphics library to use, and may not match our needs.
        if (library.OpenGL == nullptr) {
            std::cerr << "SetupRendering: No OpenGL GraphicsLibrary, this "
                         "should not happen"
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

        // Set the viewport to cover our entire render texture.
        glViewport(0, 0, static_cast<GLsizei>(renderInfo.viewport.width),
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

        // Clear the screen to black and clear depth
        glClearColor(0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // =================================================================
        // This is where we draw our world and hands and any other objects.
        // We're in World Space.  To find out about where to render objects
        // in OSVR spaces (like left/right hand space) we need to query the
        // interface and handle the coordinate tranforms ourselves.

        // Draw a cube with a 5-meter radius as the room we are floating in.
        draw_cube(5.0);
    }

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
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

#if QTCORE_VERSION >= 0x050300
    // Use desktop OpenGL to make sure that Qt uses the same OpenGL
    // implementation as RenderKit
    app.setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif

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
    leftButton1.registerCallback(&myButtonCallback, nullptr);

    osvr::clientkit::Interface rightButton1 =
        context.getInterface("/controller/right/1");
    rightButton1.registerCallback(&myButtonCallback, nullptr);

    auto toolkit = new Qt5ToolkitImpl();

    osvr::renderkit::GraphicsLibrary myLibrary;
    myLibrary.OpenGL = new osvr::renderkit::GraphicsLibraryOpenGL;
    myLibrary.OpenGL->toolkit = toolkit->getToolkit();

    // Open OpenGL and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    osvr::renderkit::RenderManager* render =
        osvr::renderkit::createRenderManager(context.get(), "OpenGL", myLibrary);

    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    // Open the display and make sure this worked.
    osvr::renderkit::RenderManager::OpenResults ret = render->OpenDisplay();
    if (ret.status == osvr::renderkit::RenderManager::OpenStatus::FAILURE) {
        std::cerr << "Could not open display" << std::endl;
        delete render;
        return 2;
    }

    RenderManagerOpenGLQt5Example example;

    if (!example.initializeOpenGLFunctions()) {
        std::cerr << "Could not open initialize OpenGL functions" << std::endl;
        return 3;
    }

    // Set up the rendering state we need.
    if (!example.SetupRendering(ret.library)) {
        return 4;
    }

    // Do a call to get the information we need to construct our
    // color and depth render-to-texture buffers.
    std::vector<osvr::renderkit::RenderInfo> renderInfo;
    context.update();
    renderInfo = render->GetRenderInfo();
    std::vector<osvr::renderkit::RenderBuffer> colorBuffers;
    std::vector<GLuint> depthBuffers; //< Depth/stencil buffers to render into

    // Construct the buffers we're going to need for our render-to-texture
    // code.
    GLuint frameBuffer; //< Groups a color buffer and a depth buffer
    example.glGenFramebuffers(1, &frameBuffer);
    example.glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

    for (size_t i = 0; i < renderInfo.size(); i++) {

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
        example.glGenRenderbuffers(1, &colorBufferName);
        osvr::renderkit::RenderBuffer rb;
        rb.OpenGL = new osvr::renderkit::RenderBufferOpenGL;
        rb.OpenGL->colorBufferName = colorBufferName;
        colorBuffers.push_back(rb);

        // "Bind" the newly created texture : all future texture
        // functions will modify this texture glActiveTexture(GL_TEXTURE0);
        example.glBindTexture(GL_TEXTURE_2D, colorBufferName);

        // Determine the appropriate size for the frame buffer to be used for
        // this eye.
        int width = static_cast<int>(renderInfo[i].viewport.width);
        int height = static_cast<int>(renderInfo[i].viewport.height);

        // Give an empty image to OpenGL ( the last "0" means "empty" )
        example.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, 0);

        // Bilinear filtering
        example.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        example.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        example.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        example.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // The depth buffer
        GLuint depthrenderbuffer;
        example.glGenRenderbuffers(1, &depthrenderbuffer);
        example.glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
        example.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width,
                              height);
        depthBuffers.push_back(depthrenderbuffer);
    }

    // Register our constructed buffers so that we can use them for
    // presentation.
    if (!render->RegisterRenderBuffers(colorBuffers)) {
        std::cerr << "RegisterRenderBuffers() returned false, cannot continue"
                  << std::endl;
        app.quit();
    }

    QTimer timer;
    // This callback will be called whenever the Qt main loop is idle
    // (until app.quit() is called)
    QObject::connect(&timer, &QTimer::timeout, [&] {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        renderInfo = render->GetRenderInfo();

        // Render into each buffer using the specified information.
        for (size_t i = 0; i < renderInfo.size(); i++) {
            example.RenderView(renderInfo[i], frameBuffer,
                       colorBuffers[i].OpenGL->colorBufferName,
                       depthBuffers[i]);
        }

        // Send the rendered results to the screen
        if (!render->PresentRenderBuffers(colorBuffers, renderInfo)) {
            std::cerr << "PresentRenderBuffers() returned false, maybe because "
                         "it was asked to quit"
                      << std::endl;
            app.quit();
        }
    });
    timer.start();

    int result = app.exec();

    // Clean up after ourselves.
    example.glDeleteFramebuffers(1, &frameBuffer);
    for (size_t i = 0; i < renderInfo.size(); i++) {
        example.glDeleteTextures(1, &colorBuffers[i].OpenGL->colorBufferName);
        delete colorBuffers[i].OpenGL;
        example.glDeleteRenderbuffers(1, &depthBuffers[i]);
    }

    // Close the Renderer interface cleanly.
    delete render;

    return result;
}

// Local Variables:
// mode: c++
// tab-width: 4
// c-basic-offset: 4
// End:
