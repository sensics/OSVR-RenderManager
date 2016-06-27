/** @file
    @brief Header

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
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_RenderManagerOpenGLC_h_GUID_362705F9_1D6B_468E_3532_B813F7AB50C6
#define INCLUDED_RenderManagerOpenGLC_h_GUID_362705F9_1D6B_468E_3532_B813F7AB50C6

// Internal Includes
#include <osvr/RenderKit/RenderManagerC.h>

// Library/third-party includes
//#include <GL/GL.h>

// Standard includes
// - none

OSVR_EXTERN_C_BEGIN

typedef void* OSVR_RenderManagerOpenGL;

typedef struct OSVR_OpenGLContextParams {
    const char* windowTitle;
    int displayIndex;
    OSVR_CBool fullScreen;
    int width;
    int height;
    int xPos;
    int yPos;
    int bitsPerPixel;
    unsigned numBuffers;
    OSVR_CBool visible;
} OSVR_OpenGLContextParams;

typedef struct OSVR_OpenGLToolkitFunctions {
    // Should be set to sizeof(OSVR_OpenGLToolkitFunctions) to allow the library
    // to detect when the client was compiled against an older version which has
    // fewer members in this struct.
    size_t size;

    // Pointer which will be passed to all the functions
    void* data;

    // Functions implementing the toolkit functionality
    void (*create)(void* data);
    void (*destroy)(void* data);
    OSVR_CBool (*addOpenGLContext)(void* data, const OSVR_OpenGLContextParams* p);
    OSVR_CBool (*removeOpenGLContexts)(void* data);
    OSVR_CBool (*makeCurrent)(void* data, size_t display);
    OSVR_CBool (*swapBuffers)(void* data, size_t display);
    OSVR_CBool (*setVerticalSync)(void* data, OSVR_CBool verticalSync);
    OSVR_CBool (*handleEvents)(void* data);
} OSVR_OpenGLToolkitFunctions;

typedef struct OSVR_GraphicsLibraryOpenGL {
    OSVR_OpenGLToolkitFunctions* toolkit;
    int unused;  // C does not allow empty structures
} OSVR_GraphicsLibraryOpenGL;

typedef struct OSVR_RenderBufferOpenGL {
    // GLuint colorBufferName;
    // GLuint depthStencilBufferName;
    int unused;  // C does not allow empty structures
} OSVR_RenderBufferOpenGL;

typedef struct OSVR_RenderInfoOpenGL {
    OSVR_GraphicsLibraryOpenGL library;
    OSVR_ViewportDescription viewport;
    OSVR_PoseState pose;
    OSVR_ProjectionMatrix projection;
} OSVR_RenderInfoOpenGL;

typedef struct OSVR_OpenResultsOpenGL {
    OSVR_OpenStatus status;
    OSVR_GraphicsLibraryOpenGL library;
    OSVR_RenderBufferOpenGL buffers;
} OSVR_OpenResultsOpenGL;

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrCreateRenderManagerOpenGL(
    OSVR_ClientContext clientContext, const char graphicsLibraryName[],
    OSVR_GraphicsLibraryOpenGL graphicsLibrary,
    OSVR_RenderManager* renderManagerOut,
    OSVR_RenderManagerOpenGL* renderManagerOpenGLOut);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrRenderManagerGetRenderInfoOpenGL(
    OSVR_RenderManagerOpenGL renderManager,
    OSVR_RenderInfoCount renderInfoIndex, OSVR_RenderParams renderParams,
    OSVR_RenderInfoOpenGL* renderInfoOut);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerOpenDisplayOpenGL(OSVR_RenderManagerOpenGL renderManager,
                                   OSVR_OpenResultsOpenGL* openResultsOut);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerPresentRenderBufferOpenGL(
    OSVR_RenderManagerPresentState presentState, OSVR_RenderBufferOpenGL buffer,
    OSVR_RenderInfoOpenGL renderInfoUsed,
    OSVR_ViewportDescription normalizedCroppingViewport);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerRegisterRenderBufferOpenGL(
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_RenderBufferOpenGL renderBuffer);

/// Gets a given OSVR_RenderInfoOpenGL from an OSVR_RenderInfoCollection.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrRenderManagerGetRenderInfoFromCollectionOpenGL(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount index,
    OSVR_RenderInfoOpenGL* renderInfoOut);

OSVR_EXTERN_C_END

#endif // INCLUDED_RenderManagerOpenGLC_h_GUID_362705F9_1D6B_468E_3532_B813F7AB50C6
