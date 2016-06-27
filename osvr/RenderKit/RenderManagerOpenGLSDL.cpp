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

#include "RenderManagerOpenGLSDL.h"
#include <RenderManagerBackends.h>
#include "GraphicsLibraryOpenGL.h"
#include "RenderManagerSDLInitQuit.h"

namespace osvr {
namespace renderkit {

    RenderManagerOpenGLSDL::RenderManagerOpenGLSDL(
        OSVR_ClientContext context,
        ConstructorParameters p)
        : RenderManagerOpenGL(context, p) {
        m_GLContext = nullptr;
    }

    RenderManagerOpenGLSDL::~RenderManagerOpenGLSDL() {
        removeOpenGLContexts();
    }

    bool RenderManagerOpenGLSDL::addOpenGLContext(GLContextParams p) {
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

    bool RenderManagerOpenGLSDL::removeOpenGLContexts() {
        deleteProgram();
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

    bool RenderManagerOpenGLSDL::makeCurrent(size_t display) {
        SDL_GL_MakeCurrent(m_displays[display].m_window, m_GLContext);
        return true;
    }

    bool RenderManagerOpenGLSDL::swapBuffers(size_t display) {
        SDL_GL_SwapWindow(m_displays[display].m_window);
        return true;
    }

    bool RenderManagerOpenGLSDL::setVerticalSync(bool verticalSync) {
        if (verticalSync) {
            if (SDL_GL_SetSwapInterval(1) != 0) {
                std::cerr << "RenderManagerOpenGL::OpenDisplay: Warning: Could "
                             "not set vertical retrace on"
                          << std::endl;
                return false;
            }
        } else {
            if (SDL_GL_SetSwapInterval(0) != 0) {
                std::cerr << "RenderManagerOpenGL::OpenDisplay: Warning: Could "
                             "not set vertical retrace off"
                          << std::endl;
                return false;
            }
        }
        return true;
    }

    bool RenderManagerOpenGLSDL::handleEvents() {
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


} // namespace renderkit
} // namespace osvr
