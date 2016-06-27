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
#include <osvr/RenderKit/RenderManagerOpenGLC.h>

namespace osvr {
namespace renderkit {

    class RenderManagerOpenGLCustom : public RenderManagerOpenGL {
      public:
        virtual ~RenderManagerOpenGLCustom();

      protected:
        /// Construct an OpenGL render manager.
        RenderManagerOpenGLCustom(
            OSVR_ClientContext context,
            ConstructorParameters p,
            const OSVR_OpenGLToolkitFunctions& functions);

        bool addOpenGLContext(GLContextParams p) override;
        bool removeOpenGLContexts() override;

        bool makeCurrent(size_t display) override;

        bool swapBuffers(size_t display) override;

        bool setVerticalSync(bool verticalSync) override;

        bool handleEvents() override;

      private:
        OSVR_OpenGLToolkitFunctions functions;

        friend RenderManager OSVR_RENDERMANAGER_EXPORT*
        createRenderManager(OSVR_ClientContext context,
                            const std::string& renderLibraryName,
                            GraphicsLibrary graphicsLibrary);
    };

} // namespace renderkit
} // namespace osvr
