/** @file
@brief Header file implementing SDL Init/Quit call as a singleton across the library
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

#pragma once
#include <osvr/RenderKit/Export.h>

namespace osvr {
namespace renderkit {

  /// This function should be called by any library function or application
  /// that wants to use SDL, before any SDL function is called.  It handles
  /// calling SDL_Init() exactly once (no matter how many times this function
  /// is called) and handles calling SDL_Quit() at program termination (or
  /// DLL unloading).
  /// @return Returns true on init success, false on init failure.
  bool OSVR_RENDERMANAGER_EXPORT SDLInitQuit();

} // namespace renderkit
} // namespace osvr
