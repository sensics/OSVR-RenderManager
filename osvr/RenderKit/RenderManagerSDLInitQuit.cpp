/** @file
@brief Source file implementing SDL Init/Quit call as a singleton across the library
       and application.

@date 2016

@author
Russ Taylor <russ@sensics.com>
Sensics, Inc.
<http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
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

#include <memory>
#include <iostream>
#include <SDL.h>
#include "RenderManagerSDLInitQuit.h"

namespace osvr {
namespace renderkit {

  /// Singleton class that calls SDL_Init() when constructed and SDL_Quit()
  /// when destroyed.  Only one instance of this should be made, and it should
  /// be assigned to a static std::unique_ptr so that it will be destroyed when
  /// the program quits.
  class SDLInitQuitClass {
  public:
    SDLInitQuitClass() {
      if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        m_working = false;
      }
    }

    ~SDLInitQuitClass() {
      if (m_working) {
        //  Oddly enough, when we called SDL_Quit() inside the destructor of a
        // RenderManager that had constructed it in OpenDisplay(), it did not hang.
        // But when we call it at program shutdown, it hangs.
        //   According to one online report, SDL_Quit() can get stuck inside a
        // call to SDL_DestroyWindow() when
        // we've already destroyed all of the windows we created, or when we don't
        // create any windows.  So unfortunately, we can't call it.  Presumably, it
        // will clean itself up when the program finishes exiting or the DLL completes
        // its unloading.  If this behavior stops happing in the future, we can
        // put it back in.
        //   @todo If this starts working again on all architectures in a future version
        // of SDL, put it back in.
        //SDL_Quit();
      }
    }
    bool m_working = true;
  };
  static std::unique_ptr<SDLInitQuitClass> myPtr = nullptr;

  bool SDLInitQuit()
  {
    if (myPtr == nullptr) {
      myPtr.reset(new SDLInitQuitClass);
    }
    return myPtr->m_working;
 }

} // namespace renderkit
} // namespace osvr
