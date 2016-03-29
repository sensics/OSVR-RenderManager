/** @file
@brief Implementation of the OSVR direct-to-device rendering interface

@date 2015

@author
Russ Taylor working through ReliaSolve.com for Sensics, Inc.
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
#include "RenderManager.h"
#include <RenderManagerBackends.h>

#ifdef RM_USE_D3D11
#include "RenderManagerD3D.h"
#include "RenderManagerD3D11ATW.h"
#endif

#ifdef RM_USE_NVIDIA_DIRECT_D3D11
#include "RenderManagerNVidiaD3D.h"
#endif

#ifdef RM_USE_NVIDIA_DIRECT_D3D11_OPENGL
#include "RenderManagerD3DOpenGL.h"
#endif

#ifdef RM_USE_AMD_DIRECT_D3D11
#include "RenderManagerAMDD3D.h"
#endif

#ifdef RM_USE_OPENGL
#include "RenderManagerOpenGL.h"
#endif

#include "VendorIdTools.h"

// OSVR Includes
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/ClientKit/DisplayC.h>
#include <osvr/Common/ClientContext.h>
#include <osvr/Client/RenderManagerConfig.h>
#include <osvr/ClientKit/TransformsC.h>
#include <osvr/Common/IntegerByteSwap.h>
#include <osvr/ClientKit/ParametersC.h>
#include <osvr/Util/QuatlibInteropC.h>

// Library/third-party includes
#include <Eigen/Core>
#include <Eigen/Geometry>

#include <quat.h>

#include <json/value.h>
#include <json/reader.h>

// Standard includes
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <memory>
#include <map>
#include <algorithm>


// @todo Consider pulling this function into Core.
/// @brief Predict a future pose based on initial and velocity.
// Predict the future pose of the head based on the velocity
// information and how long we should predict.  Check the
// linear and angular velocity terms to see if we should be
// using each.  Replace the pose with the predicted pose.
/// @param[in] poseIn The initial pose used for prediction.
/// @param[in] vel The pose velocity used to move the pose
///  forward in time.  This function respects the valid
///  flags to make sure not to make use of parts of the state
///  that have not been filled in.
/// @param[in] predictionIntervalSec How long to integrate
///  the velocity to move the pose forward in time.
/// @param[out] poseOut The place to store the predicted
///  pose.  May be a reference to the same structure as
///  poseIn.
static void PredictFuturePose(
  const OSVR_PoseState &poseIn,
  const OSVR_VelocityState &vel,
  double predictionIntervalSec,
  OSVR_PoseState &poseOut)
{
  // Make a copy of the pose state so that we can handle the
  // case where the out and in pose are the same.
  OSVR_PoseState out = poseIn;

  // If we have a change in orientation, make it.
  if (vel.angularVelocityValid) {

    // Start out the new orientation at the original one
    // from OSVR.
    q_type newOrientation;
    osvrQuatToQuatlib(newOrientation, &poseIn.rotation);

    // Rotate it by the amount to rotate once for every integral multiple
    // of the rotation time we've been asked to go.
    q_type rotationAmount;
    osvrQuatToQuatlib(rotationAmount,
      &vel.angularVelocity.incrementalRotation);

    // @todo
    double remaining = predictionIntervalSec;
    while (remaining > vel.angularVelocity.dt) {
      q_mult(newOrientation, rotationAmount, newOrientation);
      remaining -= vel.angularVelocity.dt;
    }

    // Then rotate it by the remaining fractional amount.
    double fractionTime = remaining / vel.angularVelocity.dt;
    q_type identity = { 0, 0, 0, 1 };
    q_type fractionRotation;
    q_slerp(fractionRotation, identity, rotationAmount, fractionTime);
    q_mult(newOrientation, fractionRotation, newOrientation);

    // Then put it back into OSVR format in the output pose.
    osvrQuatFromQuatlib(&out.rotation, newOrientation);
  }

  // If we have a linear velocity, apply it.
  if (vel.linearVelocityValid) {
    out.translation.data[0] += vel.linearVelocity.data[0]
      * predictionIntervalSec;
    out.translation.data[1] += vel.linearVelocity.data[1]
      * predictionIntervalSec;
    out.translation.data[2] += vel.linearVelocity.data[2]
      * predictionIntervalSec;
  }

  // @todo Make sure the above transformation happens in
  // the correct space, so we're rotating about the object
  // center and translating in external (not object-local)
  // space.

  // Copy the resulting pose.
  poseOut = out;
}

/// Used to determine if we have three 2D points that are almost
/// in the same line.  If so, they are not good for use as a
/// basis for interpolation.
static bool nearly_collinear(std::array<double, 2> const& p1,
                             std::array<double, 2> const& p2,
                             std::array<double, 2> const& p3) {
    double dx1 = p2[0] - p1[0];
    double dy1 = p2[1] - p1[1];
    double dx2 = p3[0] - p1[0];
    double dy2 = p3[1] - p1[1];
    double len1 = sqrt(dx1 * dx1 + dy1 * dy1);
    double len2 = sqrt(dx2 * dx2 + dy2 * dy2);

    // If either vector is zero length, they are collinear
    if (len1 * len2 == 0) {
        return true;
    }

    // Normalize the vectors
    dx1 /= len1;
    dy1 /= len1;
    dx2 /= len2;
    dy2 /= len2;

    // See if the magnitude of their dot products is close to 1.
    double dot = dx1 * dx2 + dy1 * dy2;
    return fabs(dot) > 0.8;
}

/// Interpolates the values at three 2D points to the
/// location of a third point.
static double interpolate(double p1X, double p1Y, double val1, double p2X,
                          double p2Y, double val2, double p3X, double p3Y,
                          double val3, double pointX, double pointY) {
    // Fit a plane to three points, using their values as the
    // third dimension.
    q_vec_type p1, p2, p3;
    q_vec_set(p1, p1X, p1Y, val1);
    q_vec_set(p2, p2X, p2Y, val2);
    q_vec_set(p3, p3X, p3Y, val3);

    // The normalized cross product of the vectors from the first
    // point to each of the other two is normal to this plane.
    q_vec_type v1, v2;
    q_vec_subtract(v1, p2, p1);
    q_vec_subtract(v2, p3, p1);
    q_vec_type ABC;
    q_vec_cross_product(ABC, v1, v2);
    if (q_vec_magnitude(ABC) == 0) {
      // We can't get a normal, degenerate points, just return the first
      // value.
      return val1;
    }
    q_vec_normalize(ABC, ABC);

    // Solve for the D associated with the plane by filling back
    // in one of the points.  This is done by taking the dot product
    // of the ABC vector with the first point.  We then solve for D.
    // AX + BY + CZ + D = 0; D = -(AX + BY + CZ)
    double D = -q_vec_dot_product(ABC, p1);

    // Evaluate the plane equations at our input point, which will interpolate
    // or extrapolate our values.
    // We're solving for Z in this case, so we get
    // CZ = -(AX + BY + D); Z = -(AX + BY + D)/C;
    return -(ABC[0] * pointX + ABC[1] * pointY + D) / ABC[2];
}

/// @brief Static helper function to make the identity xform
/// @todo Remove this once we use Eigen code below.
static void makeIdentity(q_xyz_quat_type& xform) {
    xform.xyz[Q_X] = xform.xyz[Q_Y] = xform.xyz[Q_Z] = 0;
    xform.quat[Q_X] = xform.quat[Q_Y] = xform.quat[Q_Z] = 0;
    xform.quat[Q_W] = 1;
}

/// @brief Static helper function to make the identity xform
/// @todo Remove this once we use Eigen code below.
static void q_from_OSVR(q_xyz_quat_type& xform, const OSVR_PoseState& pose) {
    xform.xyz[Q_X] = pose.translation.data[0];
    xform.xyz[Q_Y] = pose.translation.data[1];
    xform.xyz[Q_Z] = pose.translation.data[2];
    xform.quat[Q_X] = osvrQuatGetX(&pose.rotation);
    xform.quat[Q_Y] = osvrQuatGetY(&pose.rotation);
    xform.quat[Q_Z] = osvrQuatGetZ(&pose.rotation);
    xform.quat[Q_W] = osvrQuatGetW(&pose.rotation);
}

/// @brief Static helper function to make the identity xform
/// @todo Remove this once we use Eigen code below.
static void OSVR_from_q(OSVR_PoseState& pose, const q_xyz_quat_type& xform) {
    pose.translation.data[0] = xform.xyz[Q_X];
    pose.translation.data[1] = xform.xyz[Q_Y];
    pose.translation.data[2] = xform.xyz[Q_Z];
    osvrQuatSetX(&pose.rotation, xform.quat[Q_X]);
    osvrQuatSetY(&pose.rotation, xform.quat[Q_Y]);
    osvrQuatSetZ(&pose.rotation, xform.quat[Q_Z]);
    osvrQuatSetW(&pose.rotation, xform.quat[Q_W]);
}

namespace osvr {
namespace renderkit {

    RenderManager::RenderManager(
        OSVR_ClientContext context,
        const ConstructorParameters& p)
        : m_context(context) {
        // Initialize all of the variables that don't have to be done in the
        // list above, so we don't get warnings about out-of-order
        // initialization if they are re-ordered in the header file.
        m_params = p;

        /// Clear the callback for display, so it will
        /// not be present until set
        m_displayCallback.m_callback = nullptr;
        m_displayCallback.m_userData = nullptr;

        /// Clear the callback for viewport/projection, so it will
        /// not be present until set
        m_viewCallback.m_callback = nullptr;
        m_viewCallback.m_userData = nullptr;

        /// If we have been passed an empty headFromRoomName, then we
        /// try /me/head.  If that doesn't work, or if we can't get the
        /// space we're asked for, print an error and the headFromWorld
        /// transform will remain the identity transform.
        std::string headSpaceName = "/me/head";
        if (p.m_roomFromHeadName.size() > 0) {
            headSpaceName = p.m_roomFromHeadName;
        }

        m_displayWidth = m_params.m_displayConfiguration.getDisplayWidth();
        m_displayHeight = m_params.m_displayConfiguration.getDisplayHeight();

        if (osvrClientGetInterface(m_context, headSpaceName.c_str(),
                                   &m_roomFromHeadInterface) ==
            OSVR_RETURN_FAILURE) {
            std::cerr << "RenderManager::RenderManager(): Can't get interface "
                      << headSpaceName << std::endl;
            throw std::runtime_error("Can't get head interface.");
        }
        osvrPose3SetIdentity(&m_roomFromHead);

        // We haven't yet registered our render buffers, so can't present them
        m_renderBuffersRegistered = false;
    }

    bool RenderManager::SetDisplayCallback(DisplayCallback callback,
                                           void* userData) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        // Make sure we have valid data
        if (callback == nullptr) {
            std::cerr
                << "RenderManager::SetDisplayCallback: NULL callback handler"
                << std::endl;
            return false;
        }

        m_displayCallback.m_callback = callback;
        m_displayCallback.m_userData = userData;

        return true;
    }

    bool
    RenderManager::SetViewProjectionCallback(ViewProjectionCallback callback,
                                             void* userData) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        // Make sure we have valid data
        if (callback == nullptr) {
            std::cerr << "RenderManager::SetViewProjectionCallback: NULL "
                         "callback handler"
                      << std::endl;
            return false;
        }

        m_viewCallback.m_callback = callback;
        m_viewCallback.m_userData = userData;

        return true;
    }

    bool RenderManager::AddRenderCallback(const std::string& interfaceName,
                                          RenderCallback callback,
                                          void* userData) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        // Make sure we have valid data
        if (callback == nullptr) {
            std::cerr
                << "RenderManager::AddRenderCallback: NULL callback handler"
                << std::endl;
            return false;
        }

        // Create a new callback structure and fill it in.  Make the
        // pose be the identity pose until we hear otherwise.
        RenderCallbackInfo cb;
        cb.m_callback = callback;
        cb.m_userData = userData;
        cb.m_interfaceName = interfaceName;
        cb.m_interface = nullptr;
        osvrPose3SetIdentity(&cb.m_state);

        // If this is not world space, construct an interface
        // description so we can render objects here.
        if ((interfaceName.size() > 0) && (interfaceName != "/")) {
            if (osvrClientGetInterface(m_context, interfaceName.c_str(),
                                       &cb.m_interface) ==
                OSVR_RETURN_FAILURE) {
                std::cerr << "RenderManager::AddRenderCallback(): Can't get "
                             "interface "
                          << interfaceName << std::endl;
            }
        }

        // Add this to the list of render callbacks.
        m_callbacks.push_back(cb);
        return true;
    }

    bool RenderManager::RemoveRenderCallback(const std::string& interfaceName,
                                             RenderCallback callback,
                                             void* userData) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        // Look up an entry matching all three paramaters.  If we
        // find one, remove it from the list after removing its
        // callback handler by freeing its interface object.
        // If this callback does not have an interface, we don't
        // free the interface.
        for (size_t i = 0; i < m_callbacks.size(); i++) {
            RenderCallbackInfo& ci = m_callbacks[i];
            if ((interfaceName == ci.m_interfaceName) &&
                (callback == ci.m_callback) && (userData == ci.m_userData)) {
                if (ci.m_interface != nullptr) {
                    if (osvrClientFreeInterface(m_context,
                                                ci.m_interface) ==
                        OSVR_RETURN_FAILURE) {
                        std::cerr
                            << "RenderManager::RemoveRenderCallback(): Could "
                               "not free the interface for this callback."
                            << std::endl;
                        return false;
                    }
                }
                m_callbacks.erase(m_callbacks.begin() + i);
                return true;
            }
        }
        // We didn't fine one!
        return false;
    }

    RenderManager::~RenderManager() {
        // Unregister any remaining callback handlers for devices that
        // are set to update our transformation matrices.
        while (m_callbacks.size() > 0) {
            RenderCallbackInfo& cb = m_callbacks.front();
            RemoveRenderCallback(cb.m_interfaceName, cb.m_callback,
                                 cb.m_userData);
        }
    }

    bool RenderManager::Render(const RenderParams& params) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        // Make sure we're doing okay.
        if (!doingOkay()) {
            std::cerr << "RenderManager::Render(): Display not opened."
                      << std::endl;
            return false;
        }

        // Update the transformations so that we have the most-recent
        // state in them.
        if (osvrClientUpdate(m_context) == OSVR_RETURN_FAILURE) {
            std::cerr
                << "RenderManager::Render(): client context update failed."
                << std::endl;
            return false;
        }

        // Read the transformations
        m_renderParamsForRender = params;
        m_renderInfoForRender = GetRenderInfoInternal(params);

        /// @todo Use acceleration and velocity to predict viewpoint at the
        // time we expect to render.

        // Initialize the rendering for the whole frame.
        if (!RenderFrameInitialize()) {
            return false;
        }

        // One of the RenderDisplayInitialize() or RenderEyeInitialize()
        // will call the client display-callback method, whichever is
        // appropriate for the way it is rendering.  This is because some
        // renderers will do a different texture for each eye, so each is
        // effectively its own display.
        for (size_t display = 0; display < GetNumDisplays(); display++) {

            if (!RenderDisplayInitialize(display)) {
                std::cerr
                    << "RenderManager::Render(): Could not initialize display "
                    << display << std::endl;
                return false;
            }

            // Render for each eye, setting up the appropriate projection matrix
            // and viewport.
            for (size_t eyeInDisplay = 0; eyeInDisplay < GetNumEyesPerDisplay();
                 eyeInDisplay++) {

                // Figure out which overall eye this is.
                size_t eye = eyeInDisplay + display * GetNumEyesPerDisplay();

                // Initialize the projection matrix and viewport.
                // Then call any user callback to handle whatever else
                // needs doing (clearing the screen, for example).
                // Compute and set the viewport and projection matrix.
                // Use our internal member variables to store them so
                // they will be available for the render space callbacks
                // as well.
                // Every eye is on its own display now, so we need to
                // initialize and finalize the displays as well.
                if (!RenderEyeInitialize(eye)) {
                    std::cerr
                        << "RenderManager::Render(): Could not initialize eye."
                        << std::endl;
                    return false;
                }
                if (m_viewCallback.m_callback != nullptr) {
                    m_viewCallback.m_callback(
                        m_viewCallback.m_userData, m_library, m_buffers,
                        m_renderInfoForRender[eye].viewport,
                        m_renderInfoForRender[eye].projection, eye);
                }

                /// @todo Consider adding a shear to do with current
                /// head velocity to the transform.  Probably in
                /// the RenderParams structure passed in.

                // Render objects in the callback spaces.
                for (size_t i = 0; i < m_callbacks.size(); i++) {

                    /// Construct the ModelView transform to use and then render
                    /// the
                    /// space.  We can't just re-use the ones we got above
                    /// because
                    /// they are all done in world space.
                    ///  If we don't get a modelview matrix for a particular
                    ///  space,
                    /// we just don't render anything into that space.  For
                    /// example,
                    /// the example demos look for left and right hands and they
                    /// might
                    /// not be defined.
                    OSVR_PoseState pose;
                    if (!ConstructModelView(i, eye, params, pose)) {
                        continue;
                    }
                    if (!RenderSpace(i, eye, pose,
                                     m_renderInfoForRender[eye].viewport,
                                     m_renderInfoForRender[eye].projection)) {
                        return false;
                    }
                }

                // Done with this eye.
                if (!RenderEyeFinalize(eye)) {
                    std::cerr
                        << "RenderManager::Render(): Could not finalize eye."
                        << std::endl;
                    return false;
                }
            }

            if (!RenderDisplayFinalize(display)) {
                std::cerr
                    << "RenderManager::Render(): Could not finalize display "
                    << display << std::endl;
                return false;
            }
        }

        // Finalize the rendering for the whole frame.
        if (!RenderFrameFinalize()) {
            return false;
        }

        // Keep track of the timing information.
        /// @todo

        return true;
    }

    size_t RenderManager::LatchRenderInfo(const RenderParams& params) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        m_latchedRenderInfo = GetRenderInfoInternal(params);
        return m_latchedRenderInfo.size();
    }

    RenderInfo RenderManager::GetRenderInfo(size_t index) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        RenderInfo ret;
        if (index < m_latchedRenderInfo.size()) {
            ret = m_latchedRenderInfo[index];
        }
        return ret;
    }

    std::vector<RenderInfo>
    RenderManager::GetRenderInfoInternal(const RenderParams& params) {
        // Start with an empty vector, which will be returned as such on
        // failure.
        std::vector<RenderInfo> ret;

        // Make sure we're doing okay.
        if (!doingOkay()) {
            std::cerr << "RenderManager::GetRenderInfo(): Display not opened."
                      << std::endl;
            ret.clear();
            return ret;
        }

        // Update the transformations so that we have the most-recent
        // state in them.
        if (osvrClientUpdate(m_context) == OSVR_RETURN_FAILURE) {
            std::cerr << "RenderManager::GetRenderInfo(): client context "
                         "update failed."
                      << std::endl;
            ret.clear();
            return ret;
        }

        // Determine parameters for each eye, filling in all relevant
        // parameters.
        size_t numEyes = GetNumEyes();
        for (size_t eye = 0; eye < numEyes; eye++) {
            RenderInfo info;
            info.library = m_library;

            // Compute the viewport.
            // NOTE: The viewport needs to start at 0 and include the
            // overfill border on all sides because this is the user's
            // RenderTexture we are describing, not the final output
            // screen.
            OSVR_ViewportDescription v;
            if (!ConstructViewportForRender(eye, v)) {
                ret.clear();
                return ret;
            }
            info.viewport = v;

            // Construct the projection matrix.
            if (!ConstructProjection(eye, params.nearClipDistanceMeters,
                                     params.farClipDistanceMeters,
                                     info.projection)) {
                ret.clear();
                return ret;
            }

            // Construct a ModelView transform for world space.
            // By passing m_callbacks.size(), we guarantee world space.
            if (!ConstructModelView(m_callbacks.size(), eye, params,
                                    info.pose)) {
                std::cerr << "RenderManagerBase::GetRenderInfo(): Could not "
                             "ConstructModelView"
                          << std::endl;
                ret.clear();
                return ret;
            }

            // Add this to the list of eyes to be rendered.
            ret.push_back(info);
        }

        return ret;
    }

    bool RenderManager::RegisterRenderBuffers(
        const std::vector<RenderBuffer>& buffers,
        bool appWillNotOverwriteBeforeNewPresent) {
      // All public methods that use internal state should be guarded
      // by a mutex.
      std::lock_guard<std::mutex> lock(m_mutex);
      return RegisterRenderBuffersInternal(
            buffers, appWillNotOverwriteBeforeNewPresent);
    }

    bool RenderManager::RegisterRenderBuffersInternal(
        const std::vector<RenderBuffer>& buffers,
        bool /* appWillNotOverwriteBeforeNewPresent */) {
        // Record that we registered our render buffers.
        m_renderBuffersRegistered = true;
        return true;
    }

    bool RenderManager::PresentRenderBuffers(
        const std::vector<RenderBuffer>& buffers,
        const std::vector<RenderInfo>& renderInfoUsed,
        const RenderParams& renderParams,
        const std::vector<OSVR_ViewportDescription>&
            normalizedCroppingViewports,
        bool flipInY) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        return PresentRenderBuffersInternal(
            buffers, renderInfoUsed, renderParams, normalizedCroppingViewports,
            flipInY);
    }

    bool RenderManager::PresentRenderBuffersInternal(
        const std::vector<RenderBuffer>& buffers,
        const std::vector<RenderInfo>& renderInfoUsed,
        const RenderParams &renderParams,
        const std::vector<OSVR_ViewportDescription>&
                                       normalizedCroppingViewports,
        bool flipInY) {
        // Make sure we're doing okay.
        if (!doingOkay()) {
            std::cerr
                << "RenderManager::PresentRenderBuffers(): Display not opened."
                << std::endl;
            return false;
        }

        // Make sure we've registered some render buffers
        if (!m_renderBuffersRegistered) {
            std::cerr << "RenderManager::PresentRenderBuffers(): Buffers not "
                         "registered."
                      << std::endl;
            return false;
        }

        // Initialize the presentation for the whole frame.
        if (!PresentFrameInitialize()) {
            std::cerr << "RenderManager::PresentRenderBuffers(): "
                         "PresentFrameInitialize() failed."
                      << std::endl;
            return false;
        }

        // If we're doing Time Warp and we have a positive maximum
        // milliseconds until vsync, and we are able to read the timing
        // information needed to determine how far ahead of vsync we
        // are, then we continue to update our context state until we're
        // within the required threshold.

        if (m_params.m_enableTimeWarp &&
            (m_params.m_maxMSBeforeVsyncTimeWarp > 0)) {
            int count = 0;

            // Compute the threshold interval we need to be below.
            // Convert from milliseconds to seconds
            float thresholdF = m_params.m_maxMSBeforeVsyncTimeWarp / 1e3f;
            OSVR_TimeValue threshold;
            threshold.seconds = static_cast<OSVR_TimeValue_Seconds>(thresholdF);
            thresholdF -= threshold.seconds;
            threshold.microseconds =
                static_cast<OSVR_TimeValue_Microseconds>(thresholdF * 1e6);

            bool proceed;
            do {
                // Go ahead unless something stops us.
                proceed = true;

                // Update the client context so we keep getting all required
                // callbacks called during our busy-wait.
                if (osvrClientUpdate(m_context) == OSVR_RETURN_FAILURE) {
                    std::cerr << "RenderManager::PresentRenderBuffers(): "
                                 "client context update failed."
                              << std::endl;
                    return false;
                }

                // Check to see if we are able to determine the timing info.
                // If so, see if we're within the threshold.  If not, don't
                // proceed.
                // We use the first eye in the system and assume that all of the
                // others are synchronized to it.
                // @todo Consider what happens for non-genlocked displays
                RenderTimingInfo info;
                if (GetTimingInfo(0, info)) {
                    OSVR_TimeValue nextRetrace = info.hardwareDisplayInterval;
                    osvrTimeValueDifference(&nextRetrace,
                                            &info.timeSincelastVerticalRetrace);
                    if (osvrTimeValueGreater(&nextRetrace, &threshold)) {
                        proceed = false;
                    }
                }

                ++count;
            } while (!proceed);
        }

        // Store the previous matrices we returned to the client and a new set
        // using
        // the same transformation hierarchy.  This is a side effect of the
        // GetRenderInfo function.  Use these parameters to construct info
        // needed
        // to perform Asynchronous Time Warp.
        std::vector<RenderInfo> currentRenderInfo =
            GetRenderInfoInternal(renderParams);
        // @todo make the depth for ATW a parameter?
        if (m_params.m_enableTimeWarp) {
            if (!ComputeAsynchronousTimeWarps(renderInfoUsed, currentRenderInfo,
                                              2.0f)) {
                std::cerr << "RenderManager::PresentRenderBuffers: Could not "
                             "compute ATWs"
                          << std::endl;
                return false;
            }
        }

        // Render into each display, setting up the display beforehand and
        // finalizing
        // it after.
        for (size_t display = 0; display < GetNumDisplays(); display++) {

            // Set up the appropriate display before setting up its eye(s).
            if (!PresentDisplayInitialize(display)) {
                std::cerr << "RenderManager::PresentRenderBuffers(): "
                             "PresentDisplayInitialize() failed."
                          << std::endl;
                return false;
            }

            // Render for each eye, setting up the appropriate projection
            // and viewport.
            for (size_t eyeInDisplay = 0; eyeInDisplay < GetNumEyesPerDisplay();
                 eyeInDisplay++) {

                // Figure out which overall eye this is.
                size_t eye = eyeInDisplay + display * GetNumEyesPerDisplay();

                /// @todo Consider adding a shear to do with current
                /// head velocity to the transform.  Probably in
                /// the RenderParams structure passed in.

                // See if we need to rotate by 90 or 180 degrees about Z.  If
                // so, do so
                // NOTE: This would adjust the distortion center of projection,
                // but it is
                // assumed that we're doing this to make scan-out circuitry
                // behave
                // rather than to change where the actual pixel location of the
                // center
                // of projection is.
                float rotate_pixels_degrees = 0;
                if (m_params.m_displayConfiguration.getEyes()[eye]
                        .m_rotate180 != 0) {
                    rotate_pixels_degrees = 180;
                }

                // If we have display scan-out rotation, we add it to the amount
                // of
                // rotation we've already been asked to do.
                switch (m_params.m_displayRotation) {
                case ConstructorParameters::Display_Rotation::Ninety:
                    rotate_pixels_degrees += 90.0;
                    break;
                case ConstructorParameters::Display_Rotation::OneEighty:
                    rotate_pixels_degrees += 180.0;
                    break;
                case ConstructorParameters::Display_Rotation::TwoSeventy:
                    rotate_pixels_degrees += 270.0;
                    break;
                default:
                    // Nothing to do here.
                    break;
                }

                /// Pass rotate_pixels_degrees
                PresentEyeParameters p;
                p.m_index = eye;
                p.m_rotateDegrees = rotate_pixels_degrees;
                if (buffers.size() <= eye) {
                    std::cerr << "RenderManager::PresentRenderBuffers: Given "
                              << GetNumEyes() << " eyes, but only "
                              << buffers.size() << " buffers" << std::endl;
                    return false;
                }
                p.m_buffer = buffers[eye];
                p.m_flipInY = flipInY;

                // Pass in a pointer to the Asynchronous Time Warp matrix to
                // use, or nullptr (default) if there is not one.
                if (m_params.m_enableTimeWarp) {
                    // Apply the asynchronous time warp matrix for this eye.
                    if (m_asynchronousTimeWarps.size() <= eye) {
                        std::cerr << "RenderManager::PresentRenderBuffers: "
                                     "Required Asynchronous Time "
                                  << "Warp matrix not available" << std::endl;
                        return false;
                    }
                    p.m_ATW = &m_asynchronousTimeWarps[eye];
                }

                // Fill in the region to image within the buffer.  If the client
                // has
                // mapped multiple eyes into the same texture, we need to aim at
                // a subset of it for each according to the viewports they
                // passed in.
                // If they didn't pass anything, use the full buffer.
                OSVR_ViewportDescription bufferCrop;
                if (eye < normalizedCroppingViewports.size()) {
                    bufferCrop = normalizedCroppingViewports[eye];
                } else {
                    bufferCrop.left = 0;
                    bufferCrop.lower = 0;
                    bufferCrop.width = 1;
                    bufferCrop.height = 1;
                }
                p.m_normalizedCroppingViewport = bufferCrop;

                if (!PresentEye(p)) {
                    std::cerr << "RenderManager::PresentRenderBuffers(): "
                                 "PresentEye failed."
                              << std::endl;
                    return false;
                }
            }

            // We're done with this display.
            if (!PresentDisplayFinalize(display)) {
                std::cerr << "RenderManager::PresentRenderBuffers(): "
                             "PresentDisplayFinalize failed."
                          << std::endl;
                return false;
            }
        }

        // Finalize the rendering for the whole frame.
        if (!PresentFrameFinalize()) {
            std::cerr << "RenderManager::PresentRenderBuffers(): "
                         "PresentFrameFinalize failed."
                      << std::endl;
            return false;
        }

        // Keep track of the timing information.
        /// @todo

        return true;
    }

    bool RenderManager::UpdateDistortionMeshes(
        DistortionMeshType type //< Type of mesh to produce
        ,
        std::vector<DistortionParameters> const&
            distort //< Distortion parameters
        ) {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        return UpdateDistortionMeshesInternal(type, distort);
    }

    void RenderManager::SetRoomRotationUsingHead() {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        osvrClientSetRoomRotationUsingHead(m_context);
    }

    void RenderManager::ClearRoomToWorldTransform() {
        // All public methods that use internal state should be guarded
        // by a mutex.
        std::lock_guard<std::mutex> lock(m_mutex);

        osvrClientClearRoomToWorldTransform(m_context);
    }

    size_t RenderManager::GetNumEyes() {
        return m_params.m_displayConfiguration.getEyes().size();
    }

    size_t RenderManager::GetNumDisplays() {
        switch (m_params.m_displayConfiguration.getEyes().size()) {
        case 1:
            return 1;
        case 2:
            if (m_params.m_displayConfiguration.getDisplayMode() ==
                OSVRDisplayConfiguration::DisplayMode::FULL_SCREEN) {
                return 2;
            }
            return 1;
        default:
            std::cerr << "RenderManager::GetNumDisplays(): Unrecognized value: "
                      << m_params.m_displayConfiguration.getEyes().size()
                      << std::endl;
        }
        return 1;
    }

    size_t RenderManager::GetNumEyesPerDisplay() {
        if (GetNumDisplays() == 0) {
            return 0;
        }
        return GetNumEyes() / GetNumDisplays();
    }

    size_t RenderManager::GetDisplayUsedByEye(size_t eye) {
        if (GetNumDisplays() == 0) {
            return 0;
        }
        return eye / GetNumEyesPerDisplay();
    }

    bool RenderManager::ConstructProjection(size_t whichEye,
                                            double nearClipDistanceMeters,
                                            double farClipDistanceMeters,
                                            OSVR_ProjectionMatrix& projection) {
        // Make sure that we have as many eyes as were asked for
        if (whichEye >= GetNumEyes()) {
            return false;
        }

        //--------------------------------------------------------------------
        // Configure a projection transform based on the characteristics of the
        // display we are using.

        // Scale the unit X and Y parameters based on the near
        // plane to make the field of view match what we expect.
        // The tangent of the view angle in either axis is the
        // in-plane distance (left, right, top, or bottom) divided
        // by the distance to the near clipping plane.  We have
        // the angle specified and for now we assume a unit distance
        // to the window (we will adjust that later).  Given
        // this, we solve for the tangent of half the angle
        // (each of left and right provide half, as do top and
        // bottom).
        double right =
            tan(osvr::util::getRadians(
                    m_params.m_displayConfiguration.getHorizontalFOV()) /
                2.0);
        double left = -right;
        double top = tan(osvr::util::getRadians(
                             m_params.m_displayConfiguration.getVerticalFOV()) /
                         2.0);
        double bottom = -top;

        // Scale the in-plane positions based on the near plane to put
        // the virtual viewing window on the near plane with the eye at the
        // origin.
        left *= nearClipDistanceMeters;
        right *= nearClipDistanceMeters;
        top *= nearClipDistanceMeters;
        bottom *= nearClipDistanceMeters;

        // Incorporate the center-of-projection information for this
        // eye, shifting so that 0.5,0.5 is in the center of the screen.
        // We need to do this before we scale the viewport to add the amount
        // needed for overfill, so we don't end up shifting past the edge of
        // the actual screen.
        double width = right - left;
        double height = top - bottom;
        double xCOP =
            m_params.m_displayConfiguration.getEyes()[whichEye].m_CenterProjX;
        double yCOP =
            m_params.m_displayConfiguration.getEyes()[whichEye].m_CenterProjY;
        double xOffset = (0.5 - xCOP) * width;
        double yOffset = (0.5 - yCOP) * height;
        left += xOffset;
        right += xOffset;
        top += yOffset;
        bottom += yOffset;

        // Incorporate pitch_tilt (degrees, positive is downwards)
        // We assume that this results in a shearing of the image that leaves
        // the plane of the screen the same.
        auto pitchTilt = m_params.m_displayConfiguration.getPitchTilt();
        if (pitchTilt != 0 * util::radians) {
            /// @todo
        }

        // Add in the extra space needed to handle the rendering overfill used
        // to provide margin for distortion correction and Time
        // Warp.  We do in a way that can handle off-center projection by
        // adding a margin (which is half of the total overfill) to each edge.
        double xMargin = width / 2 * (m_params.m_renderOverfillFactor - 1);
        double yMargin = height / 2 * (m_params.m_renderOverfillFactor - 1);
        left -= xMargin;
        right += xMargin;
        top += yMargin;
        bottom -= yMargin;

        // We handle rotation of the pixels on the way to the screen,
        // due to the scan-out circuitry, in the code that reprojects the
        // rendered texture into the screen.  We don't need to handle
        // it here (and should not, because it will rotate bitmapped
        // textures).

        // Make sure that things won't blow up in the math below.
        if ((nearClipDistanceMeters <= 0) || (farClipDistanceMeters <= 0) ||
            (nearClipDistanceMeters == farClipDistanceMeters) ||
            (left == right) || (top == bottom)) {
            return false;
        }

        /// @todo Figure out interactions between the above shifts and
        /// distortions
        /// and make sure to do them in the right order, or to adjust as needed
        /// to
        /// make them consistent when they are composed.

        // Set the values in the projection matrix
        projection.left = left;
        projection.right = right;
        projection.top = top;
        projection.bottom = bottom;
        projection.nearClip = nearClipDistanceMeters;
        projection.farClip = farClipDistanceMeters;

        return true;
    }

    bool RenderManager::ConstructViewportForRender(
        size_t whichEye, OSVR_ViewportDescription& viewport) {
        // Zero the viewpoint to start with.
        viewport.left = viewport.lower = viewport.width = viewport.height = 0;

        // Make sure that we have as many eyes as were asked for
        if (whichEye >= GetNumEyes()) {
            return false;
        }

        // Figure out the fraction of the display we're rendering to based
        // on the display mode.
        // Set up the viewport based on the display resolution and the
        // display configuration.
        double xFactor = 1, yFactor = 1;
        switch (m_params.m_displayConfiguration.getDisplayMode()) {
        case OSVRDisplayConfiguration::DisplayMode::FULL_SCREEN:
            // Already set.
            break;
        case OSVRDisplayConfiguration::DisplayMode::HORIZONTAL_SIDE_BY_SIDE:
            xFactor = 0.5;
            break;
        case OSVRDisplayConfiguration::DisplayMode::VERTICAL_SIDE_BY_SIDE:
            yFactor = 0.5;
            break;
        default:
            std::cerr << "RenderManager::ConstructViewportForRender: "
                         "Unrecognized Display Mode"
                      << m_params.m_displayConfiguration.getDisplayMode()
                      << std::endl;
            return false;
        }

        // We always want to render to a full window, with non-rotated view,
        // with overfill factor and the oversampling factor applied.  This
        // is the size of the buffer constructed for use in the Render pass.
        viewport.width = xFactor * m_displayWidth *
                         m_params.m_renderOverfillFactor *
                         m_params.m_renderOversampleFactor;
        viewport.height = yFactor * m_displayHeight *
                          m_params.m_renderOverfillFactor *
                          m_params.m_renderOversampleFactor;

        return true;
    }

    bool RenderManager::ConstructViewportForPresent(
        size_t whichEye, OSVR_ViewportDescription& viewport, bool swapEyes) {
        // Zero the viewpoint to start with.
        viewport.left = viewport.lower = viewport.width = viewport.height = 0;

        // Make sure that we have as many eyes as were asked for
        if (whichEye >= GetNumEyes()) {
            return false;
        }

        // If we've been asked to swap the eyes, and we have an even
        // number of eyes, we adjust the asked-for eye to have the
        // opposite polarity.
        if (swapEyes) {
            if (GetNumEyes() % 2 == 0) {
                whichEye = 2 * (whichEye / 2) + (1 - (whichEye % 2));
            }
        }

        // Set up the viewport based on the display resolution and the
        // display configuration.
        switch (m_params.m_displayConfiguration.getDisplayMode()) {
        case OSVRDisplayConfiguration::DisplayMode::FULL_SCREEN:
            viewport.lower = viewport.left = 0;
            viewport.width = m_displayWidth;
            viewport.height = m_displayHeight;
            break;
        case OSVRDisplayConfiguration::DisplayMode::HORIZONTAL_SIDE_BY_SIDE:
            viewport.lower = 0;
            viewport.height = m_displayHeight;
            viewport.width = m_displayWidth / 2;
            // Zeroeth eye at left, first eye starts in the middle.
            viewport.left = whichEye * viewport.width;
            break;
        case OSVRDisplayConfiguration::DisplayMode::VERTICAL_SIDE_BY_SIDE:
            viewport.left = 0;
            viewport.width = m_displayWidth;
            viewport.height = m_displayHeight / 2;
            // Zeroeth eye in the top half, first eye at the bottom.
            if (whichEye == 0) {
                viewport.lower = viewport.height;
            } else {
                viewport.lower = 0;
            }
            break;
        default:
            std::cerr << "RenderManager::ConstructViewportForPresent: "
                         "Unrecognized Display Mode"
                      << m_params.m_displayConfiguration.getDisplayMode()
                      << std::endl;
            return false;
        }

        return true;
    }

    OSVR_ViewportDescription
    RenderManager::RotateViewport(const OSVR_ViewportDescription& viewport) {

        // If we are not rotating, just return the viewport
        // unchanged.
        if (m_params.m_displayRotation ==
            ConstructorParameters::Display_Rotation::Zero) {
            return viewport;
        }

        // Compute the terms we'll need to construct the
        // new viewpoint.
        double radians; //< How much to rotate?
        double scaleX;  //< How much to scale in X in the new space?
        double scaleY;  //< How much to scale in Y in the new space?
        switch (m_params.m_displayRotation) {
        case ConstructorParameters::Display_Rotation::Ninety:
            radians = 90.0 * M_PI / 180.0;
            // Flip width and height
            scaleX = m_displayHeight;
            scaleY = m_displayWidth;
            break;
        case ConstructorParameters::Display_Rotation::OneEighty:
            radians = 180.0 * M_PI / 180.0;
            // Keep width and height the same
            scaleX = m_displayWidth;
            scaleY = m_displayHeight;
            break;
        case ConstructorParameters::Display_Rotation::TwoSeventy:
            radians = 270.0 * M_PI / 180.0;
            // Flip width and height
            scaleX = m_displayHeight;
            scaleY = m_displayWidth;
            break;
        default:
            // The default is to do nothing, because there is no rotation.
            return viewport;
        }

        // Construct the four vertices of the viewport.  We
        // need to rotate all of them and then test which
        // is the new lower-left corner and what the new width
        // and height are.
        std::vector<double> LL = {viewport.left, viewport.lower};
        std::vector<double> UL = {viewport.left + viewport.width,
                                  viewport.lower};
        std::vector<double> UU = {viewport.left + viewport.width,
                                  viewport.lower + viewport.height};
        std::vector<double> LU = {viewport.left,
                                  viewport.lower + viewport.height};

        std::vector<std::vector<double> > verts = {LL, UL, UU, LU};

        // Normalize the vertex coordinates to the range -1..1
        for (auto& vert : verts) {
            vert[0] = -1.0 + 2.0 * (vert[0] / m_displayWidth);
            vert[1] = -1.0 + 2.0 * (vert[1] / m_displayHeight);
        }

        // Rotate the vertices using the 2D rotation matrix
        // matrix:
        //   X' = X * cos(angle) - Y * sin(angle)
        //   Y' = X * sin(angle) + Y * cos(angle)
        double cR = cos(radians);
        double sR = sin(radians);
        std::vector<std::vector<double> > vertsRot = verts;
        for (size_t i = 0; i < verts.size(); i++) {
            vertsRot[i][0] = verts[i][0] * cR - verts[i][1] * sR;
            vertsRot[i][1] = verts[i][0] * sR + verts[i][1] * cR;
        }

        // Denormalize the vertices into the new viewport size,
        // which may be the same (for 180 degree rotation) or
        // swapped (for 90 degree rotation).
        for (size_t i = 0; i < vertsRot.size(); i++) {
            vertsRot[i][0] = ((vertsRot[i][0] + 1.0) / 2.0) * scaleX;
            vertsRot[i][1] = ((vertsRot[i][1] + 1.0) / 2.0) * scaleY;
        }

        // Find the min and max X and Y and use them to construct
        // a new viewport.
        OSVR_ViewportDescription out;
        double left = vertsRot[0][0];
        double lower = vertsRot[0][1];
        double right = left;
        double upper = lower;
        for (size_t i = 1; i < vertsRot.size(); i++) {
            double x = vertsRot[i][0];
            double y = vertsRot[i][1];
            if (x < left) {
                left = x;
            }
            if (x > right) {
                right = x;
            }
            if (y < lower) {
                lower = y;
            }
            if (y > upper) {
                upper = y;
            }
        }
        out.left = left;
        out.lower = lower;
        out.width = right - left;
        out.height = upper - lower;

        return out;
    }

    bool RenderManager::ConstructModelView(size_t whichSpace, size_t whichEye,
                                           RenderParams params,
                                           OSVR_PoseState& eyeFromSpace) {
        /// Set the identity transformation to start with, in case
        /// we have to bail out with an error condition below.
        osvrPose3SetIdentity(&eyeFromSpace);

        // Make sure that we have as many eyes as were asked for.
        if (whichEye >= GetNumEyes()) {
            std::cerr << "RenderManager::ConstructModelView(): Eye index "
                      << "out of bounds" << std::endl;
            return false;
        }

        /// @todo Replace all of the quatlib math with Eigen math below,
        /// or with direct calls to the core library.

        /// We need to determine the transformation that takes points
        /// in the space we're going to render in and moves them into
        /// the space described by the projection matrix.  That space
        /// is eye space, which has the eye at the origin, X to the
        /// right, Y up, and Z pointing into the camera (opposite to
        /// the viewing direction).

        /// Include the impact of rotating the screen around the
        // eye location for HMDs who have this feature.  This is
        // computed in terms of the percent overlap of the screen.
        // We rotate each eye away from the other by half of the
        // amount they should not overlap.  NOTE: This assumes
        // that both eyes are at the same location w.r.t. the
        // overlap percent.
        // @todo Verify this assumption.
        q_xyz_quat_type q_rotatedEyeFromEye;
        makeIdentity(q_rotatedEyeFromEye);
        double rotateEyesApart = 0;
        double overlapFrac =
            m_params.m_displayConfiguration.getOverlapPercent();
        if (overlapFrac < 1.) {
            const auto hfov =
                m_params.m_displayConfiguration.getHorizontalFOV();
            const auto angularOverlap = hfov * overlapFrac;
            rotateEyesApart = util::getDegrees((hfov - angularOverlap) / 2.);
        }
        // Right eyes should rotate the other way.
        if (whichEye % 2 != 0) {
            rotateEyesApart *= -1;
        }
        rotateEyesApart = Q_DEG_TO_RAD(rotateEyesApart);
        q_from_axis_angle(q_rotatedEyeFromEye.quat, 0, 1, 0, rotateEyesApart);

        /// Include the impact of the eyeFromHead matrix.
        // This is a translation along the X axis in head space by
        // the IPD, or its negation, depending on the eye.
        // We assume that even eyes are left eyes and odd eyes are
        // right eyes.  We further assume that head space is between
        // the two eyes.  If the display descriptor wants us to swap
        // eyes, we do so by inverting the offset for each eye.
        q_xyz_quat_type q_headFromRotatedEye;
        makeIdentity(q_headFromRotatedEye);
        if (whichEye % 2 == 0) {
            // Left eye
            q_headFromRotatedEye.xyz[Q_X] -= params.IPDMeters / 2;
        } else {
            // Right eye
            q_headFromRotatedEye.xyz[Q_X] += params.IPDMeters / 2;
        }
        q_xyz_quat_type q_headFromEye;
        q_xyz_quat_compose(&q_headFromEye, &q_headFromRotatedEye,
                           &q_rotatedEyeFromEye);

        /// Include the impact of RenderParams.headFromRoom
        /// (which will override m_headFromRoom) or of m_headFromRoom.
        q_xyz_quat_type q_roomFromHead;
        if (params.roomFromHeadReplace != nullptr) {
            /// Use the params.m_headFromRoom as our transform
            q_from_OSVR(q_roomFromHead, *params.roomFromHeadReplace);
        } else {
            /// Use the state interface to read the most-recent
            /// location of the head.  It will have been updated
            /// by the most-recent call to update() on the context.
            /// DO NOT update the client here, so that we're using the
            /// same state for all eyes.
            OSVR_TimeValue timestamp;
            if (osvrGetPoseState(m_roomFromHeadInterface, &timestamp,
                                 &m_roomFromHead) == OSVR_RETURN_FAILURE) {
                // This it not an error -- they may have put in an invalid
                // state name for the head; we just ignore that case.
            }

            // Do prediction of where this eye will be when it is presented
            // if client-side prediction is enabled.
            if (m_params.m_clientPredictionEnabled) {
              // Get information about how long we have until the next present.
              // If we can't get timing info, we just set its offset to 0.
              float msUntilPresent = 0;
              RenderTimingInfo timing;
              if (GetTimingInfo(whichEye, timing)) {
                msUntilPresent +=
                  (timing.timeUntilNextPresentRequired.seconds * 1e3f) +
                  (timing.timeUntilNextPresentRequired.microseconds / 1e3f);
              }

              // Adjust the time at which the most-recent tracking info was
              // set based on whether we're supposed to override it with "now".
              // If not, find out how long ago it was.
              float msSinceTrackerReport = 0;
              if (!m_params.m_clientPredictionLocalTimeOverride) {
                OSVR_TimeValue now;
                osvrTimeValueGetNow(&now);
                msSinceTrackerReport = static_cast<float>(
                  osvrTimeValueDurationSeconds(&now, &timestamp) * 1e3
                  );
              }

              // The delay before rendering for each
              // eye will be different because they are at different delays past
              // the next vsync.  The static delay common to both eyes has
              // already been added into their offset.
              float predictionIntervalms = msSinceTrackerReport +
                msUntilPresent;
              if (whichEye < m_params.m_eyeDelaysMS.size()) {
                predictionIntervalms += m_params.m_eyeDelaysMS[whichEye];
              }
              float predictionIntervalSec = predictionIntervalms / 1e3f;

              // Find out the pose velocity information, if available.
              // Set the valid flags to false so that if to call to get
              // velocity fails, we will not try and use the info.
              OSVR_VelocityState vel;
              vel.linearVelocityValid = false;
              vel.angularVelocityValid = false;
              if (osvrGetVelocityState(m_roomFromHeadInterface, &timestamp,
                  &vel) != OSVR_RETURN_SUCCESS) {
                // We're okay with failure here, we just use a zero
                // velocity to predict.
              }

              // Predict the future pose of the head based on the velocity
              // information and how long we should predict.  Check the
              // linear and angular velocity terms to see if we should be
              // using each.  Replace the pose with the predicted pose.
              PredictFuturePose(m_roomFromHead, vel,
                predictionIntervalSec, m_roomFromHead);
            }

            // Bring the pose into quatlib world.
            q_from_OSVR(q_roomFromHead, m_roomFromHead);
        }
        q_xyz_quat_type q_roomFromEye;
        q_xyz_quat_compose(&q_roomFromEye, &q_roomFromHead, &q_headFromEye);

        // See if we are making a transform for world space.
        // If don't have a callback defined for this space, we're in
        // world space.  This is used by GetRenderInfo() and
        // PresentRenderBuffers() to get its world-space matrix.
        // If we have a NULL interface pointer, we are
        // in world space.
        bool inWorldSpace = (whichSpace >= m_callbacks.size()) ||
                            (m_callbacks[whichSpace].m_interface == nullptr);

        /// Include the impact of roomFromWorld, if it is specified.
        /// If we are not going into world space, but rather into one
        /// of the other OSVR spaces, then just leave this as the identity
        /// transform so we don't need to undo it again on the way
        /// back from room space.
        q_xyz_quat_type q_worldFromRoom;
        makeIdentity(q_worldFromRoom);
        if (inWorldSpace && (params.worldFromRoomAppend != nullptr)) {
            q_from_OSVR(q_worldFromRoom, *params.worldFromRoomAppend);
        }
        q_xyz_quat_type q_worldFromEye;
        q_xyz_quat_compose(&q_worldFromEye, &q_worldFromRoom, &q_roomFromEye);

        /// Invert the above matrices, to produce eyeFromWorld.
        q_xyz_quat_type q_eyeFromWorld;
        q_xyz_quat_invert(&q_eyeFromWorld, &q_worldFromEye);

        /// Include the impact of the space we're rendering to.
        /// This is spaceFromRoom; put on the right and multiply it on
        /// the left by the above inverted matrix.  (If we are going
        /// into one of these spaces, worldFromRoom will be the
        /// identity so we don't need to invert and reapply it.)
        q_xyz_quat_type q_worldFromSpace;
        if (inWorldSpace) {
            makeIdentity(q_worldFromSpace);
        } else {
            OSVR_TimeValue timestamp;
            if (osvrGetPoseState(
                    m_callbacks[whichSpace].m_interface, &timestamp,
                    &m_callbacks[whichSpace].m_state) == OSVR_RETURN_FAILURE) {
                // They asked for a space that does not exist.  Return false to
                // let them know we didn't get the one they wanted.
                return false;
            }
            q_from_OSVR(q_worldFromSpace, m_callbacks[whichSpace].m_state);
        }
        q_xyz_quat_type q_eyeFromSpace;
        q_xyz_quat_compose(&q_eyeFromSpace, &q_eyeFromWorld, &q_worldFromSpace);

        /// Store the result into the output pose
        OSVR_from_q(eyeFromSpace, q_eyeFromSpace);
        return true;
    }

    bool RenderManager::ComputeAsynchronousTimeWarps(
        std::vector<RenderInfo> usedRenderInfo,
        std::vector<RenderInfo> currentRenderInfo, float assumedDepth) {
        // Empty out the ATW vector until we fill it again below.
        m_asynchronousTimeWarps.clear();

        size_t numEyes = GetNumEyes();
        if (assumedDepth <= 0) {
            return false;
        }
        if ((currentRenderInfo.size() < numEyes) ||
            (usedRenderInfo.size() < numEyes)) {
            return false;
        }

        for (size_t eye = 0; eye < numEyes; eye++) {
            /// @todo For CAVE displays and fish-tank VR, the projection matrix
            /// will not be the same between frames.  Make sure we're not
            /// assuming
            /// here that it is.

            // Compute the scale to use during forward transform.
            // Scale the coordinates in X and Y so that they match the width and
            // height of a window at the specified distance from the origin.
            // We divide by the near clip distance to make the result match that
            // at
            // a unit distance and then multiply by the assumed depth.
            float xScale = static_cast<float>(
                (usedRenderInfo[eye].projection.right -
                 usedRenderInfo[eye].projection.left) /
                usedRenderInfo[eye].projection.nearClip * assumedDepth);
            float yScale = static_cast<float>(
                (usedRenderInfo[eye].projection.top -
                 usedRenderInfo[eye].projection.bottom) /
                usedRenderInfo[eye].projection.nearClip * assumedDepth);

            // Compute the translation to use during forward transform.
            // Translate the points so that their center lies in the middle of
            // the
            // view frustum pushed out to the specified distance from the
            // origin.
            // We take the mean coordinate of the two edges as the center that
            // is
            // to be moved to, and we move the space origin to there.
            // We divide by the near clip distance to make the result match that
            // at
            // a unit distance and then multiply by the assumed depth.
            // This assumes the default r texture coordinate of 0.
            float xTrans = static_cast<float>(
                (usedRenderInfo[eye].projection.right +
                 usedRenderInfo[eye].projection.left) /
                2.0 / usedRenderInfo[eye].projection.nearClip * assumedDepth);
            float yTrans = static_cast<float>(
                (usedRenderInfo[eye].projection.top +
                 usedRenderInfo[eye].projection.bottom) /
                2.0 / usedRenderInfo[eye].projection.nearClip * assumedDepth);
            float zTrans = static_cast<float>(-assumedDepth);

            // NOTE: These operations occur from the right to the left, so later
            // actions on the list actually occur first because we're
            // post-multiplying.

            // Translate the points back to a coordinate system with the
            // center at (0,0);
            Eigen::Affine3f postTranslation(
                Eigen::Translation3f(0.5f, 0.5f, 0.0f));

            /// Scale the points so that they will fit into the range
            /// (-0.5,-0.5)
            // to (0.5,0.5) (the inverse of the scale below).
            Eigen::Affine3f postScale(
                Eigen::Scaling(1.0f / xScale, 1.0f / yScale, 1.0f));

            /// Translate the points so that the projection center will lie on
            // the -Z axis (inverse of the translation below).
            Eigen::Affine3f postProjectionTranslate(
                Eigen::Translation3f(-xTrans, -yTrans, -zTrans));

            /// Compute the forward last ModelView matrix.
            OSVR_PoseState lastModelOSVR = usedRenderInfo[eye].pose;
            Eigen::Quaternionf lastModelViewRotation(
                static_cast<float>(osvrQuatGetW(&lastModelOSVR.rotation)),
                static_cast<float>(osvrQuatGetX(&lastModelOSVR.rotation)),
                static_cast<float>(osvrQuatGetY(&lastModelOSVR.rotation)),
                static_cast<float>(osvrQuatGetZ(&lastModelOSVR.rotation)));
            Eigen::Affine3f lastModelViewTranslation(Eigen::Translation3f(
                static_cast<float>(osvrVec3GetX(&lastModelOSVR.translation)),
                static_cast<float>(osvrVec3GetY(&lastModelOSVR.translation)),
                static_cast<float>(osvrVec3GetZ(&lastModelOSVR.translation))));
            // Pull the translation out from above and then plop in the rotation
            // matrix parts by hand.
            Eigen::Matrix3f lastRot3 = lastModelViewRotation.toRotationMatrix();
            Eigen::Matrix4f lastModelView = lastModelViewTranslation.matrix();
            for (size_t i = 0; i < 3; i++) {
                for (size_t j = 0; j < 3; j++) {
                    lastModelView(i, j) = lastRot3(i, j);
                }
            }
            Eigen::Projective3f lastModelViewTransform(lastModelView);

            /// Compute the inverse of the current ModelView matrix.
            OSVR_PoseState currentModelOSVR = currentRenderInfo[eye].pose;
            Eigen::Quaternionf currentModelViewRotation(
                static_cast<float>(osvrQuatGetW(&currentModelOSVR.rotation)),
                static_cast<float>(osvrQuatGetX(&currentModelOSVR.rotation)),
                static_cast<float>(osvrQuatGetY(&currentModelOSVR.rotation)),
                static_cast<float>(osvrQuatGetZ(&currentModelOSVR.rotation)));
            Eigen::Affine3f currentModelViewTranslation(Eigen::Translation3f(
                static_cast<float>(osvrVec3GetX(&currentModelOSVR.translation)),
                static_cast<float>(osvrVec3GetY(&currentModelOSVR.translation)),
                static_cast<float>(
                    osvrVec3GetZ(&currentModelOSVR.translation))));
            // Pull the translation out from above and then plop in the rotation
            // matrix parts by hand.
            // @todo turn this into a transform catenation in the proper order.
            Eigen::Matrix3f curRot3 =
                currentModelViewRotation.toRotationMatrix();
            Eigen::Matrix4f currentModelView =
                currentModelViewTranslation.matrix();
            for (size_t i = 0; i < 3; i++) {
                for (size_t j = 0; j < 3; j++) {
                    currentModelView(i, j) = curRot3(i, j);
                }
            }
            Eigen::Matrix4f currentModelViewInverse =
                currentModelView.inverse();
            Eigen::Projective3f currentModelViewInverseTransform(
                currentModelViewInverse);

            /// Translate the origin to the center of the projected rectangle
            Eigen::Affine3f preProjectionTranslate(
                Eigen::Translation3f(xTrans, yTrans, zTrans));

            /// Scale from (-0.5,-0.5)/(0.5,0.5) to the actual frustum size
            Eigen::Affine3f preScale(Eigen::Scaling(xScale, yScale, 1.0f));

            // Translate the points from a coordinate system that has (0.5,0.5)
            // as the origin to one that has (0,0) as the origin.
            Eigen::Affine3f preTranslation(
                Eigen::Translation3f(-0.5f, -0.5f, 0.0f));

            /// Compute the full matrix by multiplying the parts.
            Eigen::Projective3f full =
                postTranslation * postScale * postProjectionTranslate *
                lastModelView * currentModelViewInverse *
                preProjectionTranslate * preScale * preTranslation;

            // Store the result.
            matrix16 ATW;
            memcpy(ATW.data, full.matrix().data(), sizeof(ATW.data));
            m_asynchronousTimeWarps.push_back(ATW);
        }
        return true;
    }

    bool RenderManager::ComputeDisplayOrientationMatrix(
        float rotateDegrees //< Rotation in degrees around Z
        ,
        bool flipInY //< Flip in Y after rotating?
        ,
        matrix16& outMatrix //< Matrix to use.
        ) {
        // NOTE: These operations occur from the right to the left, so later
        // actions on the list actually occur first because we're
        // post-multiplying.

        /// Scale the points to flip the Y axis if that is called for.
        float yScale = 1;
        if (flipInY) {
            yScale = -1;
        }
        Eigen::Affine3f preScale(Eigen::Scaling(1.0f, yScale, 1.0f));

        // Rotate by the specified number of degrees.
        Eigen::Vector3f zAxis(0, 0, 1);
        float rotateRadians = static_cast<float>(rotateDegrees * M_PI / 180.0);
        Eigen::Affine3f rotate(Eigen::AngleAxisf(rotateRadians, zAxis));

        /// Compute the full matrix by multiplying the parts.
        Eigen::Projective3f full = rotate * preScale;

        // Store the result.
        memcpy(outMatrix.data, full.matrix().data(), sizeof(outMatrix.data));

        return true;
    }

    bool RenderManager::ComputeRenderBufferCropMatrix(
        OSVR_ViewportDescription normalizedCroppingViewport,
        matrix16& outMatrix) {
        // NOTE: These operations occur from the right to the left, so later
        // actions on the list actually occur first because we're
        // post-multiplying.

        /// Translate so that the origin moves to the location of
        /// the lower-left corner.
        Eigen::Affine3f translate(Eigen::Translation3f(
            static_cast<float>(normalizedCroppingViewport.left),
            static_cast<float>(normalizedCroppingViewport.lower), 0.0f));

        /// Scale the points around the origin to reduce the
        /// range in X and Y to match what was passed in.
        Eigen::Affine3f scale(Eigen::Scaling(
            static_cast<float>(normalizedCroppingViewport.width),
            static_cast<float>(normalizedCroppingViewport.height), 1.0f));

        /// Compute the full matrix by multiplying the parts.
        Eigen::Projective3f full = translate * scale;

        // Store the result.
        memcpy(outMatrix.data, full.matrix().data(), sizeof(outMatrix.data));

        return true;
    }

    static double pointDistance(double x1, double y1, double x2, double y2) {
        return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    }

    RenderManager::UnstructuredMeshInterpolator::UnstructuredMeshInterpolator(
        const MonoPointDistortionMeshDescription& points,
        int numSamplesX, int numSamplesY
        ) : m_points(points), m_numSamplesX(numSamplesX),
        m_numSamplesY(numSamplesY) {

      // Construct and fill in the grid of nearby points that is used by the
      // interpolation function to accelerate the search for the three
      // nearest non-collinear points.
      std::vector< MonoPointDistortionMeshDescription> ySet;
      MonoPointDistortionMeshDescription empty;
      for (size_t y = 0; y < m_numSamplesY; y++) {
        ySet.emplace_back(empty);
      }
      for (size_t x = 0; x < m_numSamplesX; x++) {
        m_grid.emplace_back(ySet);
      }
      
      // Go through each point in the unstructured grid and insert its index
      // into all grid elements that are within 1/4th (rounded up) of the
      // total span of the grid from its normalized location.
      int xHalfSpan = static_cast<int>(0.9 + (1.0/4.0)*0.5 * m_numSamplesX);
      int yHalfSpan = static_cast<int>(0.9 + (1.0/4.0)*0.5 * m_numSamplesY);
      for (size_t i = 0; i < points.size(); i++) {
        int xIndex, yIndex;
        if (getIndex(points[i][0][0], points[i][0][1], xIndex, yIndex)) {

          // Get the range of locations to insert
          int xMin = xIndex - xHalfSpan;
          if (xMin < 0) { xMin = 0; }
          int xMax = xIndex + xHalfSpan;
          if (xMax >= m_numSamplesX) { xMax = m_numSamplesX - 1; }
          int yMin = yIndex - yHalfSpan;
          if (yMin < 0) { yMin = 0; }
          int yMax = yIndex + yHalfSpan;
          if (yMax >= m_numSamplesY) { yMax = m_numSamplesY - 1; }

          // Insert this point into each of these locations.
          for (int x = xMin; x <= xMax; x++) {
            for (int y = yMin; y <= yMax; y++) {
              m_grid[x][y].push_back(points[i]);
            }
          }
        }
      }
    }

    Float2
    RenderManager::UnstructuredMeshInterpolator::interpolateNearestPoints(
            float xN, float yN) {
        Float2 ret = {};

        // Look in the spatial-acceleration grid to see if we can
        // find three points without having to search the entire set of
        // points.
        int xIndex, yIndex;
        if (!getIndex(xN, yN, xIndex, yIndex)) {
          return ret;
        }
        MonoPointDistortionMeshDescription points;
        points = getNearestPoints(xN, yN, m_grid[xIndex][yIndex]);

        // If we didn't get enough points from the acceleration
        // structure, look in the whole points array
        if (points.size() < 3) {
          points = getNearestPoints(xN, yN, m_points);
        }

        // If we didn't get three points, just return the output of
        // the first point we found.

        if (points.size() < 3) {
          float xNew = static_cast<float>(points[0][1][0]);
          float yNew = static_cast<float>(points[0][1][1]);
          ret[0] = xNew;
          ret[1] = yNew;
          return ret;
        }

        // Found three points -- interpolate them.
        float xNew = static_cast<float>(interpolate(
          points[0][0][0], points[0][0][1], points[0][1][0],
          points[1][0][0], points[1][0][1], points[1][1][0],
          points[2][0][0], points[2][0][1], points[2][1][0], xN,
          yN));
        float yNew = static_cast<float>(interpolate(
          points[0][0][0], points[0][0][1], points[0][1][1],
          points[1][0][0], points[1][0][1], points[1][1][1],
          points[2][0][0], points[2][0][1], points[2][1][1], xN,
          yN));
        ret[0] = xNew;
        ret[1] = yNew;
        return ret;
    }

    MonoPointDistortionMeshDescription
      RenderManager::UnstructuredMeshInterpolator::getNearestPoints(
          float xN, float yN,
          const MonoPointDistortionMeshDescription &points) {

      MonoPointDistortionMeshDescription ret;

      // Find the three non-collinear points in the mesh that are nearest
      // to the normalized point we are trying to look up.  We start by
      // sorting the points based on distance from our location, selecting
      // the first two, and then looking through the rest until we find
      // one that is not collinear with the first two (normalized dot
      // product magnitude far enough from 1).  If we don't find such
      // points, we just go with the values from the closest point.
      typedef std::multimap<double, size_t> PointDistanceIndexMap;
      PointDistanceIndexMap map;
      //if (points.size() < 460 && points.size() > 0) std::cout << "XXX Sorting mesh of size " << points.size() << std::endl;
      for (size_t i = 0; i < points.size(); i++) {
        // Insertion into the multimap sorts them by distance.
        map.insert(std::make_pair(
          pointDistance(xN, yN, points[i][0][0], points[i][0][1]), i));
      }

      PointDistanceIndexMap::const_iterator it = map.begin();
      size_t first = it->second;
      it++;
      size_t second = it->second;
      it++;
      size_t third = first;
      while (it != map.end()) {
        if (!nearly_collinear(points[first][0], points[second][0],
          points[it->second][0])) {
          third = it->second;
          break;
        }
        it++;
      }

      // Push back all of the points we found, which may not include
      // a third point if the first is the same as the third.
      if (map.size() >= 1) { ret.push_back(points[first]); }
      if (map.size() >= 2) { ret.push_back(points[second]); }
      if (third != first) {
        ret.push_back(points[third]);
      }
      return ret;
    }

    Float2 RenderManager::DistortionCorrectTextureCoordinate(
        size_t eye //< Which eye?
        , Float2 const& inCoords //< Coordinates to modify
        , DistortionParameters distort //< Distortion parameters
        , size_t color //< 0 = red, 1 = green, 2 = blue
        ) {
        Float2 ret = inCoords;

        // Check for invalid parameters
        if (color > 2) {
            return ret;
        }
        using Eigen::Vector2f;
        // Convert from coordinates in the overfilled texture to coordinates
        // that will cover the range (0,0) to (1,1) on the screen.  This is
        // done by scaling around (0.5,0.5) to push the edges of the screen
        // out to the (0,0) and (1,1) boundaries.
        Eigen::Map<Vector2f> retMap(ret.data());
        Eigen::Map<const Vector2f> inMap(inCoords.data());

        Vector2f xyN = (inMap - Vector2f::Constant(0.5f)) *
                           m_params.m_renderOverfillFactor +
                       Vector2f::Constant(0.5f);
        /// these are for mono point that hasn't been ported to eigen yet.
        float x = inCoords[0];
        float y = inCoords[1];
        float xN = xyN.x();
        float yN = xyN.y();

        switch (distort.m_type) {
        case osvr::renderkit::RenderManager::DistortionParameters::
            rgb_symmetric_polynomials: {
            if (distort.m_distortionPolynomialRed.size() < 2) {
                return ret;
            }
            if (distort.m_distortionPolynomialGreen.size() < 2) {
                return ret;
            }
            if (distort.m_distortionPolynomialBlue.size() < 2) {
                return ret;
            }
            if (distort.m_distortionCOP.size() != 2) {
                return ret;
            }
            if (distort.m_distortionD.size() != 2) {
                return ret;
            }
            if (distort.m_distortionD[0] <= 0) {
                return ret;
            }
            if (distort.m_distortionD[1] <= 0) {
                return ret;
            }

            // Convert from normalized range to (D[0], D[1]) range.  Here,
            // both coordinate systems share a common (0,0) boundary so we
            // can just scale around the origin.
            Vector2f xyD =
                xyN.cwiseProduct(Vector2f::Map(distort.m_distortionD.data()));

            // Compute the distance from the COP in D space
            // (direction and squared magnitude)
            Vector2f xyDDiff =
                xyD - Vector2f::Map(distort.m_distortionCOP.data());
            float rMag2 = xyDDiff.squaredNorm();
            if (rMag2 == 0) { // We're at the center -- no distortion
                ret = inCoords;
                return ret;
            }
            float rMag = sqrt(rMag2);
            Vector2f xyDNorm = xyDDiff / rMag;

            // Compute the new location in D space based on the distortion
            // parameters
            std::vector<float> const& params =
                color == 0 ? distort.m_distortionPolynomialRed
                           : color == 1 ? distort.m_distortionPolynomialGreen
                                        : distort.m_distortionPolynomialBlue;

            // The distance scaling factor needs to be applied as many times
            // as the degree of the polynomial we are using.  For the constant
            // term, the factor is 1.
            float rFactor = 1;
            float rNew = params[0];

            auto n = params.size();
            using index_type = decltype(n);
            for (index_type i = 1; i < n; i++) {
                rFactor *= rMag;
                rNew += params[i] * rFactor;
            }
            Vector2f xyDNew =
                Vector2f::Map(distort.m_distortionCOP.data()) + rNew * xyDNorm;

            // Convert from D space back to unit space
            Vector2f xyNNew = xyDNew.cwiseQuotient(
                Vector2f::Map(distort.m_distortionD.data()));

            Vector2f::Map(ret.data()) = xyNNew;
        } break;
        case osvr::renderkit::RenderManager::DistortionParameters::
            mono_point_samples: {
            if (eye >= distort.m_monoPointSamples.size()) {
                return ret;
            }

            MonoPointDistortionMeshDescription& points =
                distort.m_monoPointSamples[eye];
            if (points.size() < 3) {
                return ret;
            }

            // Find the three non-collinear points in the mesh that are nearest
            // to the normalized point we are trying to look up.  We start by
            // sorting the points based on distance from our location, selecting
            // the first two, and then looking through the rest until we find
            // one that is not collinear with the first two (normalized dot
            // product magnitude far enough from 1).  If we don't find such
            // points, we just go with the values from the closest point.
            ret = m_interpolators[0]->interpolateNearestPoints(xN, yN);
        } break;
        case osvr::renderkit::RenderManager::DistortionParameters::
            rgb_point_samples: {
            if (color >= 3) {
                return ret;
            }
            if (eye >= distort.m_rgbPointSamples[color].size()) {
                return ret;
            }

            MonoPointDistortionMeshDescription& points =
                distort.m_rgbPointSamples[color][eye];
            if (points.size() < 3) {
                return ret;
            }

            // Find the three non-collinear points in the mesh that are nearest
            // to the normalized point we are trying to look up.  We start by
            // sorting the points based on distance from our location, selecting
            // the first two, and then looking through the rest until we find
            // one that is not collinear with the first two (normalized dot
            // product magnitude far enough from 1).  If we don't find such
            // points, we just go with the values from the closest point.
            ret = m_interpolators[color]->interpolateNearestPoints(xN, yN);
        } break;
        default:
            break;
        }

        // Convert from unit (normalized) space back into overfill space.
        ret[0] = (ret[0] - 0.5f) / m_params.m_renderOverfillFactor + 0.5f;
        ret[1] = (ret[1] - 0.5f) / m_params.m_renderOverfillFactor + 0.5f;

        return ret;
    }

    std::vector<RenderManager::DistortionMeshVertex>
    RenderManager::ComputeDistortionMesh(
        size_t eye //< Which eye?
        , DistortionMeshType type //< Type of mesh to produce
        , DistortionParameters distort //< Distortion parameters
        ) {
        std::vector<RenderManager::DistortionMeshVertex> ret;

        // Clear any created interpolators, freeing up their memory
        // first.  These may have been left behind by a failed
        // mesh-creation from before.
        for (size_t clr = 0; clr < m_interpolators.size(); clr++) {
            delete m_interpolators[clr];
        }
        m_interpolators.clear();

        // Check the validity of the parameters, based on the ones we're
        // using.
        if (distort.m_type ==
            RenderManager::DistortionParameters::rgb_symmetric_polynomials) {
            if (distort.m_distortionPolynomialRed.size() < 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2+ "
                             "red polynomial coefficients, found "
                          << distort.m_distortionPolynomialRed.size()
                          << std::endl;
                return ret;
            }
            if (distort.m_distortionPolynomialGreen.size() < 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2+ "
                             "green polynomial coefficients, found "
                          << distort.m_distortionPolynomialGreen.size()
                          << std::endl;
                return ret;
            }
            if (distort.m_distortionPolynomialBlue.size() < 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2+ "
                             "blue polynomial coefficients, found "
                          << distort.m_distortionPolynomialBlue.size()
                          << std::endl;
                return ret;
            }
            if (distort.m_distortionD.size() != 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2 "
                             "distortion coefficients, found "
                          << distort.m_distortionD.size() << std::endl;
                return ret;
            }
        } else if (distort.m_type ==
                   RenderManager::DistortionParameters::mono_point_samples) {
            if (distort.m_monoPointSamples.size() != 2) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 2 "
                             "meshes, found "
                          << distort.m_monoPointSamples.size() << std::endl;
                return ret;
            }
            // Add a new interpolator to be used when we're finding
            // mesh coordinates.
            if (distort.m_monoPointSamples[eye].size() < 3) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need "
                              "3+ points, found "
                          << distort.m_monoPointSamples[eye].size()
                          << std::endl;
                return ret;
            }
            for (size_t i = 0; i < distort.m_monoPointSamples[eye].size();
                  i++) {
                if (distort.m_monoPointSamples[eye][i].size() != 2) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need 2 "
                        << "points in the mesh, found "
                        << distort.m_monoPointSamples[eye][i].size()
                        << std::endl;
                    return ret;
                }
                if (distort.m_monoPointSamples[eye][i][0].size() != 2) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need 2 "
                        << "values in input point, found "
                        << distort.m_monoPointSamples[eye][i][0].size()
                        << std::endl;
                    return ret;
                }
                if (distort.m_monoPointSamples[eye][i][1].size() != 2) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need 2 "
                        << "values in output point, found "
                        << distort.m_monoPointSamples[eye][i][1].size()
                        << std::endl;
                    return ret;
                }
            }
            // Add a new interpolator to be used when we're finding
            // mesh coordinates.
            m_interpolators.emplace_back(new
              UnstructuredMeshInterpolator(distort.m_monoPointSamples[eye]));
        }
        else if (distort.m_type ==
                   RenderManager::DistortionParameters::rgb_point_samples) {
            if (distort.m_rgbPointSamples.size() != 3) {
                std::cerr << "RenderManager::ComputeDistortionMesh: Need 3 "
                             "color meshes, found "
                          << distort.m_rgbPointSamples.size() << std::endl;
                return ret;
            }
            for (size_t clr = 0; clr < 3; clr++) {
                if (distort.m_rgbPointSamples[clr].size() != 2) {
                    std::cerr << "RenderManager::ComputeDistortionMesh: Need 2 "
                                 "eye meshes, found "
                              << distort.m_rgbPointSamples[clr].size()
                              << std::endl;
                    return ret;
                }
                // Add a new interpolator to be used when we're finding
                // mesh coordinates.
                if (distort.m_rgbPointSamples[clr][eye].size() < 3) {
                    std::cerr
                        << "RenderManager::ComputeDistortionMesh: Need "
                            "3+ points, found "
                        << distort.m_rgbPointSamples[clr][eye].size()
                        << std::endl;
                    return ret;
                }
                for (size_t i = 0;
                      i < distort.m_rgbPointSamples[clr][eye].size(); i++) {
                    if (distort.m_rgbPointSamples[clr][eye][i].size() !=
                        2) {
                        std::cerr
                            << "RenderManager::ComputeDistortionMesh: Need "
                                "2 "
                            << "points in the mesh, found "
                            << distort.m_rgbPointSamples[clr][eye][i].size()
                            << std::endl;
                        return ret;
                    }
                    if (distort.m_rgbPointSamples[clr][eye][i][0].size() !=
                        2) {
                        std::cerr
                            << "RenderManager::ComputeDistortionMesh: Need "
                                "2 "
                            << "values in input point, found "
                            << distort.m_rgbPointSamples[clr][eye][i][0]
                                    .size()
                            << std::endl;
                        return ret;
                    }
                    if (distort.m_rgbPointSamples[clr][eye][i][1].size() !=
                        2) {
                        std::cerr
                            << "RenderManager::ComputeDistortionMesh: Need "
                                "2 "
                            << "values in output point, found "
                            << distort.m_rgbPointSamples[clr][eye][i][1]
                                    .size()
                            << std::endl;
                        return ret;
                    }
                }

                // Add a new interpolator to be used when we're finding
                // mesh coordinates, one per eye.
                m_interpolators.emplace_back(new
                  UnstructuredMeshInterpolator(distort.m_rgbPointSamples[clr][eye]));
            }
        } else {
            std::cerr << "RenderManager::ComputeDistortionMesh: Unrecognized "
                      << "distortion parameter type" << std::endl;
            return ret;
        }

        // See what kind of mesh we're supposed to produce.  Make the
        // appropriate one.
        switch (type) {
        case SQUARE: {
            // Figure out how many quads we should use in each dimension.  The
            // minimum is 1.  We have an even number in each.  There are two
            // triangles per quad.
            int quadsPerSide =
                static_cast<int>(sqrt(distort.m_desiredTriangles / 2));
            if (quadsPerSide < 1) {
                quadsPerSide = 1;
            }

            // Figure out how large each quad will be.  Recall that we're
            // covering a range of 2 (from -1 to 1) in each dimension, so the
            // quads will all be square in texture space.
            float quadSide = 2.0f / quadsPerSide;
            float quadTexSide = 1.0f / quadsPerSide;

            // Generate a pair of triangles for each quad, wound
            // counter-clockwise, with appropriate spatial location and texture
            // coordinates.
            // Compute distorted texture coordinates and use those for each
            // vertex.

            // total of quadsPerSide * quadsPerSide * 6 vertices added: reserve
            // that space to avoid excess copying during mesh generation.
            ret.reserve(quadsPerSide * quadsPerSide * 6);

            for (int x = 0; x < quadsPerSide; x++) {
                float xLow = -1 + x * quadSide;
                float xHigh = -1 + (x + 1) * quadSide;
                float xTexLow = x * quadTexSide;
                float xTexHigh = (x + 1) * quadTexSide;

                for (int y = 0; y < quadsPerSide; y++) {
                    /// 6 vertices per inner loop
                    float yLow = -1 + y * quadSide;
                    float yHigh = -1 + (y + 1) * quadSide;
                    float yTexLow = y * quadTexSide;
                    float yTexHigh = (y + 1) * quadTexSide;

                    Float2 posLL = { xLow, yLow };
                    Float2 posLH = { xLow, yHigh };
                    Float2 posHL = { xHigh, yLow };
                    Float2 posHH = { xHigh, yHigh };

                    Float2 texLL = {xTexLow, yTexLow};
                    Float2 texLH = {xTexLow, yTexHigh};
                    Float2 texHL = {xTexHigh, yTexLow};
                    Float2 texHH = {xTexHigh, yTexHigh};

                    // First triangle - emplace DistortionMeshVertex
                    ret.emplace_back(posLL, DistortionCorrectTextureCoordinate(
                                                eye, texLL, distort, 0),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texLL, distort, 1),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texLL, distort, 2));
                    ret.emplace_back(posHL, DistortionCorrectTextureCoordinate(
                                                eye, texHL, distort, 0),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texHL, distort, 1),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texHL, distort, 2));
                    ret.emplace_back(posHH, DistortionCorrectTextureCoordinate(
                                                eye, texHH, distort, 0),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texHH, distort, 1),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texHH, distort, 2));

                    // Second triangle
                    ret.emplace_back(posLL, DistortionCorrectTextureCoordinate(
                                                eye, texLL, distort, 0),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texLL, distort, 1),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texLL, distort, 2));
                    ret.emplace_back(posHH, DistortionCorrectTextureCoordinate(
                                                eye, texHH, distort, 0),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texHH, distort, 1),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texHH, distort, 2));
                    ret.emplace_back(posLH, DistortionCorrectTextureCoordinate(
                                                eye, texLH, distort, 0),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texLH, distort, 1),
                                     DistortionCorrectTextureCoordinate(
                                         eye, texLH, distort, 2));
                }
            }
        } break;
        case RADIAL: {
            std::cerr
                << "RenderManager::ComputeDistortionMesh: Radial mesh type "
                << "not yet implemented" << std::endl;

            // @todo Scale the aspect ratio of the rings around the center of
            // projection so that they will be round in the visible display.
        } break;
        default:
            std::cerr << "RenderManager::ComputeDistortionMesh: Unsupported "
                         "mesh type: "
                      << type << std::endl;
        }

        // Clear any created interpolators, freeing up their memory
        // first.
        for (size_t clr = 0; clr < m_interpolators.size(); clr++) {
            delete m_interpolators[clr];
        }
        m_interpolators.clear();

        return ret;
    }

    static std::string osvrRenderManagerGetString(OSVR_ClientContext context,
                                                  const std::string& path) {
        size_t len;
        if (osvrClientGetStringParameterLength(context, path.c_str(), &len) ==
            OSVR_RETURN_FAILURE) {
            std::string msg =
                std::string("Couldn't get osvr string length for path ") + path;
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }

        // struct TempBuffer {
        //    char *buffer;
        //    TempBuffer(size_t len) { buffer = new char[len + 1]; }
        //    ~TempBuffer() { delete[] buffer; }
        //} tempBuffer(len);
        std::vector<char> tempBuffer(len + 1);

        if (osvrClientGetStringParameter(context, path.c_str(),
                                         tempBuffer.data(),
                                         len + 1) == OSVR_RETURN_FAILURE) {
            std::string msg =
                std::string("Couldn't get osvr string buffer for path ") + path;
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }
        return std::string(tempBuffer.data(), len);
    }

    void
    RenderManager::ConstructorParameters::addCandidatePNPID(const char* pnpid) {
        auto id = std::string{pnpid};
        if (id.size() != 3) {
            std::cerr << "RenderManager::ConstructorParameters::"
                         "addCandidatePNPID: Error: given '"
                      << pnpid << "' which cannot be valid due to its size!"
                      << std::endl;
            return;
        }
        // Make the string all uppercase - not because pnpidToHex needs it, but
        // because we might want it all caps for our own usage as a string.
        // While we're iterating we can also check the validity.
        bool valid = true;
        std::transform(begin(id), end(id), begin(id), [&valid](char& c) {
            auto ret = std::toupper(c);
            if (ret > 'Z' || ret < 'A') {
                valid = false;
            }
            return ret;
        });

        if (!valid) {
            std::cerr << "RenderManager::ConstructorParameters::"
                         "addCandidatePNPID: Error: given '"
                      << pnpid
                      << "' which cannot be valid due to an invalid character!"
                      << std::endl;
            return;
        }
        // Add the hex version
        m_directVendorIds.push_back(common::integerByteSwap(pnpidToHex(pnpid)));
        // Now add the string version
        m_pnpIds.emplace_back(std::move(id));
    }

#ifdef RM_USE_D3D11
    /// Used to open a Direct3D DirectRender RenderManager based on
    /// what kind of graphics card is installed in the machine.
    static RenderManagerD3D11Base *openRenderManagerDirectMode(
      OSVR_ClientContext context
      , RenderManager::ConstructorParameters
      params
      )
    {
      RenderManagerD3D11Base *ret = nullptr;
#if defined(RM_USE_NVIDIA_DIRECT_D3D11) || defined(RM_USE_AMD_DIRECT_D3D11)
  #if defined(RM_USE_AMD_DIRECT_D3D11)
      if (ret == nullptr) {
        // See if we have an AMD card.  This is done by
        // creating an instance of an AMD RenderManager, which will only
        // be doing okay if it could load the libraries it needs.
        ret = new RenderManagerAMDD3D11(context, params);
        if (!ret->doingOkay()) {
          delete ret;
          ret = nullptr;
        }
      }
  #endif
  #if defined(RM_USE_NVIDIA_DIRECT_D3D11)
      if (ret == nullptr) {
        ret = new RenderManagerNVidiaD3D11(context, params);
        if (!ret->doingOkay()) {
          delete ret;
          ret = nullptr;
        }
      }
  #endif
#else
      std::cerr << "openRenderManagerDirectMode: No DirectRender libraries "
        "compiled into RenderManager."
        << std::endl;
#endif
      return ret;
    }
#endif

    //=======================================================================
    // Factory to create a specific instance of a RenderManager is below.
    // It determines which type to construct based on the configuration
    // files.

    RenderManager* createRenderManager(OSVR_ClientContext contextIgnored,
                                       const std::string& renderLibraryName,
                                       GraphicsLibrary graphicsLibrary) {
        // Null pointer return in case we can't open one.
        std::unique_ptr<RenderManager> ret;

        /// @todo Clone the passed-in context rather than creating our own, when
        // this function is added to Core.
        // Construct the context we're going to pass it.
        // @todo Destroy the context at the appropriate time.  Need to make sure
        // this happens only once, so we'd like to use a shared_ptr() on the
        // basic struct that is the underlying pointer and register the deletion
        // function.
        OSVR_ClientContext context = osvrClientInit("com.osvr.renderManager");

        // Wait until we get a connection to a display object, from which we
        // will
        // read information that we need about display device resolutions and
        // distortion correction parameters.  Once we hear from the display
        // device, we presume that we will also be able to read our
        // RenderManager parameters.  Complain as we don't hear from the
        // display device.
        // @todo Verify that waiting for the display is sufficient to be
        // sure we'll get the RenderManager string.
        OSVR_ReturnCode displayReturnCode;
        OSVR_DisplayConfig display;
        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();
        do {
            osvrClientUpdate(context);
            displayReturnCode = osvrClientGetDisplay(context, &display);
            end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed = end - start;
            if (elapsed.count() >= 1) {
                std::cerr << "RenderManager::createRenderManager(): Waiting to "
                             "get Display from server..."
                          << std::endl;
                start = end;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } while (displayReturnCode == OSVR_RETURN_FAILURE);
        std::cerr << "RenderManager::createRenderManager(): Got Display info "
                     "from server "
                     "(ignore earlier errors that occured while we were "
                     "waiting to connect)"
                  << std::endl;

        // Check the information in the pipeline configuration to determine
        // what kind of renderer to instantiate.  Also fill in the parameters
        // to pass to the renderer.
        RenderManager::ConstructorParameters p;
        p.m_graphicsLibrary = graphicsLibrary;

        osvr::client::RenderManagerConfigPtr pipelineConfig;
        try {
            // @todo
            // this should be a temporary workaround to an issue with
            // RenderManagerConfig APIin osvrClient. I think it's a
            // C++ cross-dll boundary issue, and making it
            // a header-only lib might fix it, but we're moving the code here
            // for now.
            std::string configString = osvrRenderManagerGetString(context,
              "/renderManagerConfig");
            osvr::client::RenderManagerConfigPtr cfg(
                new osvr::client::RenderManagerConfig(configString));
            pipelineConfig = cfg;

            // this is what the code should be doing:
            // pipelineConfig =
            // osvr::client::RenderManagerConfigFactory::createShared(context->get());
        } catch (std::exception& /*e*/) {
            std::cerr << "createRenderManager: Could not parse "
                         "/render_manager_parameters string from server."
                      << std::endl;
            return nullptr;
        }
        if (pipelineConfig == nullptr) {
            std::cerr << "createRenderManager: Could not parse "
                         "/render_manager_parameters string from server (NULL "
                         "pipelineconfig)."
                      << std::endl;
            return nullptr;
        }
        p.m_directMode = pipelineConfig->getDirectMode();
        p.m_directDisplayIndex = pipelineConfig->getDisplayIndex();
        p.m_directHighPriority = pipelineConfig->getDirectHighPriority();
        p.m_numBuffers = static_cast<unsigned>(pipelineConfig->getNumBuffers());
        p.m_verticalSync = pipelineConfig->getVerticalSync();
        p.m_verticalSyncBlocksRendering =
            pipelineConfig->getVerticalSyncBlockRendering();
        p.m_renderLibrary = renderLibraryName;

        p.m_windowTitle = pipelineConfig->getWindowTitle();
        p.m_windowFullScreen = pipelineConfig->getWindowFullScreen();
        p.m_windowXPosition = pipelineConfig->getWindowXPosition();
        p.m_windowYPosition = pipelineConfig->getWindowYPosition();
        auto rotation = pipelineConfig->getDisplayRotation();
        switch (rotation) {
        case 0:
            p.m_displayRotation =
                RenderManager::ConstructorParameters::Display_Rotation::Zero;
            break;
        case 90:
            p.m_displayRotation =
                RenderManager::ConstructorParameters::Display_Rotation::Ninety;
            break;
        case 180:
            p.m_displayRotation = RenderManager::ConstructorParameters::
                Display_Rotation::OneEighty;
            break;
        case 270:
            p.m_displayRotation = RenderManager::ConstructorParameters::
                Display_Rotation::TwoSeventy;
            break;
        default:
            std::cerr << "createRenderManager: Unrecognized display rotation ("
                      << rotation << ") in rendermanager config file"
                      << std::endl;
            return nullptr;
        }
        p.m_bitsPerColor = pipelineConfig->getBitsPerColor();
        p.m_asynchronousTimeWarp = pipelineConfig->getAsynchronousTimeWarp();
        p.m_enableTimeWarp = pipelineConfig->getEnableTimeWarp();
        p.m_maxMSBeforeVsyncTimeWarp =
            pipelineConfig->getMaxMSBeforeVsyncTimeWarp();
        p.m_renderOverfillFactor = pipelineConfig->getRenderOverfillFactor();
        p.m_renderOversampleFactor =
            pipelineConfig->getRenderOversampleFactor();
        p.m_clientPredictionEnabled =
          pipelineConfig->getclientPredictionEnabled();
        p.m_eyeDelaysMS.push_back(pipelineConfig->getStaticDelayMS() +
          pipelineConfig->getLeftEyeDelayMS());
        p.m_eyeDelaysMS.push_back(pipelineConfig->getStaticDelayMS() +
          pipelineConfig->getRightEyeDelayMS());
        p.m_clientPredictionLocalTimeOverride =
          pipelineConfig->getclientPredictionLocalTimeOverride();

        std::string jsonString;
        try {
            std::string jsonString =
                osvrRenderManagerGetString(context, "/display");
            OSVRDisplayConfiguration displayConfig(jsonString);
            p.m_displayConfiguration = displayConfig;
        } catch (std::exception& /*e*/) {
            std::cerr << "createRenderManager: Could not parse /display string "
                         "from server."
                      << std::endl;
            return nullptr;
        }

        // Determine the appropriate display VendorIds based on the name of the
        // display device.  Don't push any back if we don't recognize the vendor
        // name.
        if (p.m_displayConfiguration.getVendor() == "Oculus") {
            p.addCandidatePNPID("OVR"); // 0xD23E
        } else if (p.m_displayConfiguration.getVendor() == "OSVR" ||
                   p.m_displayConfiguration.getVendor() == "Sensics") {
            // There are two possible vendor IDs for OSVR.  We push them both
            // here.
            p.addCandidatePNPID("SVR"); // 0xD24E
            p.addCandidatePNPID("SEN"); // 0xAE4C
        } else if (p.m_displayConfiguration.getVendor() == "Dell") {
            p.addCandidatePNPID("DEL"); // 0xAC10 - for testing
        } else if (p.m_displayConfiguration.getVendor() == "VVR") {
            p.addCandidatePNPID("VVR"); // 0xD25A
        } else if (p.m_displayConfiguration.getVendor() == "Vuzix") {
            p.addCandidatePNPID("IWR"); // 0xF226
        } else if (p.m_displayConfiguration.getVendor() == "HTC") {
          p.addCandidatePNPID("HVR"); // 0xD222
        }
        p.m_directModeIndex = -1; // -1 means select based on resolution

#if 0
    // Construct the distortion parameters based on information from the
    // Core display class.

    /// @todo Implement multiple viewers.
    /// Get the number of viewers.
    OSVR_ViewerCount viewers;
    osvrClientGetNumViewers(display, &viewers);
    if (viewers != 1) {
      std::cerr << "RenderManager::createRenderManager(): Multiple viewers not "
        << "yet implemented" << std::endl;
      return nullptr;
    }
    OSVR_ViewerCount viewer;
    for (viewer = 0; viewer < viewers; ++viewer) {
      OSVR_EyeCount eyes;
      osvrClientGetNumEyesForViewer(display, viewer, &eyes);

      /// @todo Handle multiple display surfaces per eye.
      OSVR_SurfaceCount surfaces;
      osvrClientGetNumSurfacesForViewerEye(display, viewer, 0 /* eye */,
        &surfaces);
      if (surfaces != 1) {
        std::cerr << "RenderManager::createRenderManager(): Multiple surfaces "
          << "per eye not yet implemented" << std::endl;
        return nullptr;
      }

      /// Get any radial distortion parameters for each eye.
      /// If an eye does not have them, use the default parameters
      /// that don't do distortion and turn off the request for
      /// distortion.
      OSVR_EyeCount eye;
      p.m_distortionCorrection = false;
      for (eye = 0; eye < eyes; ++eye) {
        OSVR_RadialDistortionParameters rDP;
        OSVR_ReturnCode ret;
        ret = osvrClientGetViewerEyeSurfaceRadialDistortion(display,
          viewer, eye, 0 /*surface*/, &rDP);
        if (ret == OSVR_RETURN_SUCCESS) {
          // Turn on distortion correction and set the parameters for
          // this eye.  We assume that the parameters are in the space
          // that is the size of the display.
          RenderManager::DistortionParameters dP;
          dP.m_desiredTriangles = 800;
          std::vector<float> COP;
          COP.push_back(static_cast<float>(rDP.centerOfProjection.data[0]));
          COP.push_back(static_cast<float>(rDP.centerOfProjection.data[1]));
          dP.m_distortionCOP = COP;
          std::vector<float> K1s;
          K1s.push_back(static_cast<float>(rDP.k1.data[0]));
          K1s.push_back(static_cast<float>(rDP.k1.data[1]));
          K1s.push_back(static_cast<float>(rDP.k1.data[2]));
          dP.m_distortionK1s = K1s;
          // Scale the D's so that the K unit for width is with respect
          // to a unity D and the K value for height is adjusted by
          // the aspect ratio.
          std::vector<float> Ds;
          // @todo Fill in Ds based on display size from Core.
          //  Get viewport from ViewerEye, getViewerEyeSurface()
          //  Don't divide by 2 in this case, since the viewport
          //  is sized for each eye.
          // @todo Figure out how to handle single vs. multiple eyes
          //   in the same display.
          Ds.push_back(1.0f);
          Ds.push_back(1.0f);
          //Ds.push_back(static_cast<float>(
          //  p.m_displayConfiguration.getDisplayWidth()/2));
          //Ds.push_back(static_cast<float>(
          //  p.m_displayConfiguration.getDisplayHeight()));
          dP.m_distortionD = Ds;
          p.m_distortionParameters.push_back(dP);
          p.m_distortionCorrection = true;
        } else {
          // Push back an empty set of parameters for this eye
          p.m_distortionParameters.push_back(RenderManager::DistortionParameters());
        }
      }
    }

#else
        // Construct the distortion parameters based on the local display
        // class.
        // @todo Remove once we get a general polynomial from Core.

        RenderManager::DistortionParameters distortion;
        distortion.m_desiredTriangles = 200 * 64;

        if (p.m_displayConfiguration.getDistortionType() ==
            OSVRDisplayConfiguration::RGB_SYMMETRIC_POLYNOMIALS) {
            distortion.m_type = osvr::renderkit::RenderManager::
                DistortionParameters::Type::rgb_symmetric_polynomials;
            std::vector<float> Ds;
            Ds.push_back(
                p.m_displayConfiguration.getDistortionDistanceScaleX());
            Ds.push_back(
                p.m_displayConfiguration.getDistortionDistanceScaleY());
            distortion.m_distortionD = Ds;
            distortion.m_distortionPolynomialRed =
                p.m_displayConfiguration.getDistortionPolynomalRed();
            distortion.m_distortionPolynomialGreen =
                p.m_displayConfiguration.getDistortionPolynomalGreen();
            distortion.m_distortionPolynomialBlue =
                p.m_displayConfiguration.getDistortionPolynomalBlue();
            for (size_t i = 0; i < p.m_displayConfiguration.getEyes().size();
                 i++) {
                std::vector<float> COP = {
                    static_cast<float>(
                        p.m_displayConfiguration.getEyes()[i].m_CenterProjX),
                    static_cast<float>(
                        p.m_displayConfiguration.getEyes()[i].m_CenterProjY)};
                distortion.m_distortionCOP = COP;
                p.m_distortionParameters.push_back(distortion);
            }
        } else if (p.m_displayConfiguration.getDistortionType() ==
                   OSVRDisplayConfiguration::MONO_POINT_SAMPLES) {
            distortion.m_type = osvr::renderkit::RenderManager::
                DistortionParameters::Type::mono_point_samples;
            distortion.m_monoPointSamples =
                p.m_displayConfiguration.getDistortionMonoPointMeshes();
            // Push back the same parameter set for both eyes; the parameter has
            // a vector that has info for each eye, and the code that uses this
            // will select the right one.
            // @todo Seems like it might make more sense to pull out the
            // individual parameter sets here and push back one for each eye,
            // rather than sending both sets in twice.
            p.m_distortionParameters.assign(
                p.m_displayConfiguration.getEyes().size(), distortion);
        } else if (p.m_displayConfiguration.getDistortionType() ==
                   OSVRDisplayConfiguration::RGB_POINT_SAMPLES) {
            distortion.m_type = osvr::renderkit::RenderManager::
                DistortionParameters::Type::rgb_point_samples;
            distortion.m_rgbPointSamples =
                p.m_displayConfiguration.getDistortionRGBPointMeshes();
            // Push back the same parameter set for both eyes; the parameter has
            // a vector that has info for each eye, and the code that uses this
            // will select the right one.
            // @todo Seems like it might make more sense to pull out the
            // individual parameter sets here and push back one for each eye,
            // rather than sending both sets in twice.
            p.m_distortionParameters.assign(
                p.m_displayConfiguration.getEyes().size(), distortion);
        } else {
            std::cerr << "createRenderManager: Unrecognized distortion "
                         "correction type ("
                      << p.m_displayConfiguration.getDistortionTypeString()
                      << ") in display config file" << std::endl;
        }

#endif

        // @todo Read the info we need from Core.

        // If DirectMode is turned off and we've got a valid vendor ID, then we
        // construct a vendor-specific device to disable it.  This is a side
        // effect of trying to open the display on that vendor's device.
        // @todo Rethink how this should work now that we have an external
        // program to bring devices back out of DirectMode.  We should at least
        // check to see if it is already in this mode.  This will require doing
        // the check on both nVidia and AMD.
        if (!p.m_directMode && (p.m_directVendorIds.size() > 0)) {
#ifdef RM_USE_NVIDIA_DIRECT_D3D11
            // Turn off parameters that would cause a lot of code to run.
            RenderManager::ConstructorParameters pTemp = p;
            pTemp.m_distortionCorrection = false;
            pTemp.m_enableTimeWarp = false;
            pTemp.m_distortionParameters.clear();
            {
                std::unique_ptr<RenderManager> temp(
                    new RenderManagerNVidiaD3D11(context, pTemp));
                temp->OpenDisplay();
            }
#endif
        }

        // Open the appropriate render manager based on the rendering library
        // and DirectMode selected.
        if (p.m_renderLibrary == "Direct3D11") {
#ifdef RM_USE_D3D11
            if (p.m_directMode) {
              // If we've been asked for asynchronous time warp, we layer
              // the request on top of a request for a DirectRender instance
              // to harness.  @todo This should be doable on top of a non-
              // DirectMode interface as well.
              if (p.m_asynchronousTimeWarp) {
                // @todo See about making this fall-back not be required.
                if (p.m_graphicsLibrary.D3D11 == nullptr) {
                  std::cerr << "Async timewarp enabled, but D3D11 graphics"
                    << " library not passed in. Falling back to"
                    << " synchronous timewarp." << std::endl;
                  ret.reset(openRenderManagerDirectMode(context, p));
                } else {
                  // the wrapped RenderManager should create its own graphics device
                  RenderManager::ConstructorParameters pTemp = p;
                  pTemp.m_graphicsLibrary.D3D11 = nullptr;
                  auto wrappedRm = openRenderManagerDirectMode(context, pTemp);
                  auto atwRm = new RenderManagerD3D11ATW(context, p, wrappedRm);
                  ret.reset(atwRm);
                }
              } else {
                // Try each available DirectRender library to see if we can
                // get a pointer to a RenderManager that has access to the
                // DirectMode display we want to use.
                ret.reset(openRenderManagerDirectMode(context, p));
              }
              if (ret == nullptr) {
                std::cerr << "createRenderManager: Could not open the"
                  << " requested DirectMode display" << std::endl;
              }
            } else {
              ret.reset(new RenderManagerD3D11(context, p));
            }
#else
              std::cerr << "createRenderManager: D3D11 render library "
                "not compiled in"
                << std::endl;
              return nullptr;
#endif
        } else if (p.m_renderLibrary == "OpenGL") {
            if (p.m_directMode) {
// nVidia DirectMode is currently only implemented under Direct3D11,
// so we wrap this with an OpenGL renderer.
#if defined(RM_USE_NVIDIA_DIRECT_D3D11_OPENGL) && !defined(RM_USE_OPENGLES20)
                // Set the parameters on the harnessed renderer to not apply the
                // rendering fixes that we're applying.  Also set its render
                // library
                // to match.
                RenderManager::ConstructorParameters p2 = p;
                p2.m_renderLibrary = "Direct3D11";
                p2.m_directMode = true;

                // @todo This needs to be fixed elsewhere, and generalized to
                // work with all forms of distortion correction.
                // Flip y on the center of projection on the distortion
                // correction, because we're going to be rendering in OpenGL
                // but distorting in D3D, and they use a different texture
                // orientation.
                // When we do this, we need to take into account the D scaling
                // factor being applied to the center of projection; first
                // scaling back into unity, then flipping, then rescaling.
                for (size_t eye = 0; eye < p.m_distortionParameters.size();
                     ++eye) {
                    if (p2.m_distortionParameters[eye].m_distortionCOP.size() <
                        2) {
                        std::cerr << "createRenderManager: Insufficient "
                                     "distortion parameters"
                                  << std::endl;
                        return nullptr;
                    }
                    if (p2.m_distortionParameters[eye].m_distortionD.size() <
                        2) {
                        std::cerr << "createRenderManager: Insufficient "
                                     "distortion parameters"
                                  << std::endl;
                        return nullptr;
                    }
                    float original =
                        p2.m_distortionParameters[eye].m_distortionCOP[1];
                    float normalized =
                        original /
                        p2.m_distortionParameters[eye].m_distortionD[1];
                    float flipped = 1.0f - normalized;
                    float scaled =
                        flipped *
                        p2.m_distortionParameters[eye].m_distortionD[1];
                    p2.m_distortionParameters[eye].m_distortionCOP[1] = scaled;
                }

                std::unique_ptr<RenderManagerD3D11Base> host = nullptr;
                host.reset(openRenderManagerDirectMode(context, p2));
                if (host == nullptr) {
                  std::cerr << "createRenderManager: Could not open the"
                    << " requested harnessed DirectMode display" << std::endl;
                }

                ret.reset(
                    new RenderManagerD3D11OpenGL(context, p, std::move(host)));
#else
                std::cerr
                    << "createRenderManager: D3D11OpenGL render library not "
                       "compiled in"
                    << std::endl;
                return nullptr;
#endif
            } else {
#ifdef RM_USE_OPENGL
                ret.reset(new RenderManagerOpenGL(context, p));
#else
                std::cerr << "createRenderManager: OpenGL render library not "
                             "compiled in"
                          << std::endl;
                return nullptr;
#endif
            }
        } else {
            std::cerr << "createRenderManager: Unrecognized render library: "
                      << p.m_renderLibrary << std::endl;
            return nullptr;
        }

        // Check and see if the render manager is doing okay.  If not, return
        // nullptr after deleting it.
        if (!ret->doingOkay()) {
            return nullptr;
        }

        // Return the render manager.
        return ret.release();
    }

} // namespace renderkit
} // namespace osvr
