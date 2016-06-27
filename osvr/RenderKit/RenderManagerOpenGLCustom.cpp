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

#include "RenderManagerOpenGLCustom.h"

#include <iostream>

namespace osvr {
namespace renderkit {
    RenderManagerOpenGLCustom::RenderManagerOpenGLCustom(
        OSVR_ClientContext context,
        ConstructorParameters p,
        const OSVR_OpenGLToolkitFunctions& functions)
        : RenderManagerOpenGL(context, p) {
        if (functions.size != sizeof (OSVR_OpenGLToolkitFunctions)) {
            m_doingOkay = false;
            std::cerr << "osvr::renderkit::RenderManagerOpenGLCustom:"
                      << "Unknown size for OSVR_OpenGLToolkitFunctions struct" << std::endl;
            return;
        }

        this->functions = functions;
        this->functions.create(this->functions.data);
    }

    RenderManagerOpenGLCustom::~RenderManagerOpenGLCustom() {
        removeOpenGLContexts();
        this->functions.destroy(this->functions.data);
    }

    bool RenderManagerOpenGLCustom::addOpenGLContext(GLContextParams p) {
        OSVR_OpenGLContextParams params;
        params.windowTitle = p.windowTitle.c_str();
        params.displayIndex = p.displayIndex;
        params.fullScreen = p.fullScreen;
        params.width = p.width;
        params.height = p.height;
        params.xPos = p.xPos;
        params.yPos = p.yPos;
        params.bitsPerPixel = p.bitsPerPixel;
        params.numBuffers = p.numBuffers;
        params.visible = p.visible;

        return functions.addOpenGLContext(functions.data, &params);
    }

    bool RenderManagerOpenGLCustom::removeOpenGLContexts() {
        return functions.removeOpenGLContexts(functions.data);
    }

    bool RenderManagerOpenGLCustom::makeCurrent(size_t display) {
        return functions.makeCurrent(functions.data, display);
    }

    bool RenderManagerOpenGLCustom::swapBuffers(size_t display) {
        return functions.swapBuffers(functions.data, display);
    }

    bool RenderManagerOpenGLCustom::setVerticalSync(bool verticalSync) {
        return functions.setVerticalSync(functions.data, verticalSync);
    }

    bool RenderManagerOpenGLCustom::handleEvents() {
        return functions.handleEvents(functions.data);
    }


} // namespace renderkit
} // namespace osvr
