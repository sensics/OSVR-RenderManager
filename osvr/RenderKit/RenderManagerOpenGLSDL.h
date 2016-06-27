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
#include "RenderManagerOpenGL.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL.h>
#ifndef RM_USE_OPENGLES20
  #include <SDL_opengl.h>
#endif

namespace osvr {
namespace renderkit {

    class RenderManagerOpenGLSDL : public RenderManagerOpenGL {
      public:
        virtual ~RenderManagerOpenGLSDL();

      protected:
        /// Construct an OpenGL render manager.
        RenderManagerOpenGLSDL(
            OSVR_ClientContext context,
            ConstructorParameters p);

        bool addOpenGLContext(GLContextParams p) override;
        bool removeOpenGLContexts() override;

        bool makeCurrent(size_t display) override;

        bool swapBuffers(size_t display) override;

        bool setVerticalSync(bool verticalSync) override;

        bool handleEvents() override;

        // Classes and structures needed to do our rendering.
        class DisplayInfo {
          public:
            SDL_Window* m_window = nullptr; //< The window we're rendering into
        };
        std::vector<DisplayInfo> m_displays;

        SDL_GLContext
            m_GLContext; //< The context we use to render to all displays

        friend RenderManager OSVR_RENDERMANAGER_EXPORT*
        createRenderManager(OSVR_ClientContext context,
                            const std::string& renderLibraryName,
                            GraphicsLibrary graphicsLibrary);
    };

} // namespace renderkit
} // namespace osvr
