/** @file
    @brief OSVR display configuration

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com>

*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// 	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include "osvr_display_configuration.h"

// Library/third-party includes
#include <boost/units/io.hpp>
#include <json/reader.h>
#include <json/value.h>

// Standard includes
#include <cassert>
#include <fstream>
#include <iostream>
#include <utility>

// Included files that define built-in distortion meshes.
#include "osvr_display_config_built_in_osvr_hdks.h"

struct BuiltInKeysAndData {
    const char* key;
    const char* dataString;
};

static const std::initializer_list<BuiltInKeysAndData>
    BUILT_IN_MONO_POINT_SAMPLES = {
        {"OSVR_HDK_13_V1", osvr_display_config_built_in_osvr_hdk13_v1},
        {"OSVR_HDK_13_V2", osvr_display_config_built_in_osvr_hdk13_v2},
        {"OSVR_HDK_20_V1", osvr_display_config_built_in_osvr_hdk20_v1}};

OSVRDisplayConfiguration::OSVRDisplayConfiguration() {
    // do nothing
}

OSVRDisplayConfiguration::OSVRDisplayConfiguration(
    const std::string& display_description) {
    parse(display_description);
}

struct ExternalDistortionReturnValue {
    ExternalDistortionReturnValue() : distortion(Json::nullValue) {}
    ExternalDistortionReturnValue(Json::Value const& val, std::string&& fn)
        : distortion(val), filename(std::move(fn)) {}

    /// returns whether we have a valid return value in here.
    explicit operator bool() const { return !distortion.isNull(); }

    /// Distortion object from descriptor: valid iff not null.
    Json::Value distortion;

    /// Filename loaded from, if valid.
    std::string filename;
};

inline ExternalDistortionReturnValue
getExternalDistortionFile(const char* distortionTypeName, Json::Reader& reader,
                          Json::Value const& distortionObject,
                          const char* externalFileKey) {
    auto withError = [&] { return ExternalDistortionReturnValue{}; };
    Json::Value const& externalFile = distortionObject[externalFileKey];
    if ((!externalFile.isNull()) && (externalFile.isString())) {
        // Read a Json value from the external file, then replace the distortion
        // mesh with that from the file.

        Json::Value externalData;
        std::string fn = externalFile.asString();
        std::ifstream fs{fn};
        if (!fs) {
            std::cerr << "OSVRDisplayConfiguration::parse(): Warning: Couldn't "
                         "open external "
                      << distortionTypeName << " file " << fn << "!\n";
            return withError();
        }
        std::cout << "OSVRDisplayConfiguration::parse(): Reading and parsing "
                     "JSON in external "
                  << distortionTypeName << " file " << fn << std::endl;
        if (!reader.parse(fs, externalData, false)) {
            std::cerr << "OSVRDisplayConfiguration::parse(): Warning: Couldn't "
                         "parse external "
                      << distortionTypeName << " file " << fn << "!\n";
            std::cerr << "Errors: " << reader.getFormattedErrorMessages();
            return withError();
        }

        std::cout << "OSVRDisplayConfiguration::parse(): File read and JSON "
                     "parse complete."
                  << std::endl;
        return ExternalDistortionReturnValue{
            externalData["display"]["hmd"]["distortion"], std::move(fn)};
    }
    return withError();
}

/// Given the distortion object of the json config, tries to turn it into a mesh
/// description (without loading external or built-ins - all it cares about is
/// "mono_point_samples").
///
/// If it succeeds, it returns a non-empty value. If it fails, it will squawk a
/// little but just return an empty object: carry on and try your next approach.
inline osvr::renderkit::MonoPointDistortionMeshDescriptions
jsonToMonoPointMesh(Json::Value const& distortion, const char* dataSource) {

    std::cout << "OSVRDisplayConfiguration::parse(): Processing JSON data into "
                 "mono point samples description structure."
              << std::endl;
    osvr::renderkit::MonoPointDistortionMeshDescriptions mesh;
    Json::Value const& eyeArray = distortion["mono_point_samples"];
    auto withError = [&] {
        mesh.clear();
        return mesh;
    };
    if (eyeArray.isNull() || eyeArray.empty()) {
        std::cerr
            << "OSVRDisplayConfiguration::parse(): Warning: Couldn't find "
               "non-empty distortion mono point distortion in data from "
            << dataSource << "!\n";
        return withError();
    }

    for (auto& pointArray : eyeArray) {
        osvr::renderkit::MonoPointDistortionMeshDescription eye;
        if (pointArray.empty()) {
            std::cerr << "OSVRDisplayConfiguration::parse(): Warning: Empty "
                      << " distortion mono point distortion list for eye, in "
                         "data from "
                      << dataSource << "!\n";
            return withError();
        }
        using Double2 = std::array<double, 2>;
        using Double2x2 = std::array<Double2, 2>;
        for (auto& elt : pointArray) {
            Double2x2 point;
            if ((elt.size() != 2) || (elt[0].size() != 2) ||
                (elt[1].size() != 2)) {
                std::cerr
                    << "OSVRDisplayConfiguration::parse(): Warning: Malformed"
                    << " distortion mono point distortion list entry, in data "
                       "from "
                    << dataSource << "!\n";
                return withError();
            }
            Double2 in, out;
            in[0] = (elt[0][0].asDouble());
            in[1] = (elt[0][1].asDouble());
            out[0] = (elt[1][0].asDouble());
            out[1] = (elt[1][1].asDouble());
            point[0] = (in);
            point[1] = (out);
            eye.push_back(point);
        }
        mesh.push_back(eye);
    }
    std::cout << "OSVRDisplayConfiguration::parse(): Initial processing "
                 "complete. Loaded mono point samples data with "
              << mesh[0].size() << " and " << mesh[1].size()
              << " samples per eye, respectively.\n";
    return mesh;
}

/// Function split out of parseDistortionMonoPointMeshes, just so that we can
/// use return false as an easy early-out.
inline bool parseBuiltInDistortionMonoPointMeshes(
    Json::Reader& reader, Json::Value const& distortion,
    osvr::renderkit::MonoPointDistortionMeshDescriptions& mesh) {

    Json::Value const& builtIn = distortion["mono_point_samples_built_in"];
    if (builtIn.isNull() || (!builtIn.isString())) {
        return false;
    }
    // Read a Json value from the built-in config, then replace the
    // distortion mesh with that from the file.
    const std::string builtInKey = builtIn.asString();
    bool found = false;
    std::string builtInString;
    /// Check against each entry in the known built ins (registered in a table
    /// at the top of this file)
    for (auto& knownEntry : BUILT_IN_MONO_POINT_SAMPLES) {
        if (builtInKey == knownEntry.key) {
            builtInString = knownEntry.dataString;
            found = true;
            break;
        }
    }
    if (!found) {
        // didn't find a match
        std::cerr << "OSVRDisplayConfiguration::parse(): Warning: Unrecognized "
                     "mono_point_samples_built_in value: "
                  << builtInKey << "!\n";
        return false;
    }

    Json::Value builtInData;
    if (!reader.parse(builtInString, builtInData, false)) {
        std::cerr << "OSVRDisplayConfiguration::parse(): Warning: Couldn't "
                     "parse built-in configuration "
                  << builtInKey << "!\n";
        std::cerr << "Errors: " << reader.getFormattedErrorMessages();
        return false;
    }
    auto const& myDistortion = builtInData["display"]["hmd"]["distortion"];
    if (myDistortion.isNull()) {
        return false;
    }
    auto newMesh =
        jsonToMonoPointMesh(myDistortion, "mono_point_samples_built_in");
    if (newMesh.empty()) {
        /// Something couldn't be turned into a mesh, that routine already
        /// blabbed about it, just return false without touching anything else.
        return false;
    }
    /// OK, we've got a winner!
    std::cout << "OSVRDisplayConfiguration::parse(): Using distortion method "
                 "\"mono_point_samples_built_in\": \""
              << builtInKey << "\"" << std::endl;
    mesh = std::move(newMesh);
    return true;
}

inline void parseDistortionMonoPointMeshes(
    Json::Value const& distortion,
    osvr::renderkit::MonoPointDistortionMeshDescriptions& mesh) {
    Json::Reader reader;

    // See if we have the name of a built-in config to parse.  If so, we open it
    // and grab its values to parse, replacing the ones that they sent
    // in.
    {
        auto success =
            parseBuiltInDistortionMonoPointMeshes(reader, distortion, mesh);
        if (success) {
            // If we loaded a built-in successfully, no need to look any
            // further.
            return;
        }
    }

    // See if we have the name of an external file to parse.  If so, we open it
    // and grab its values to parse, replacing the ones that they sent
    // in.
    {
        auto externalDistortion =
            getExternalDistortionFile("mono point", reader, distortion,
                                      "mono_point_samples_external_file");
        if (externalDistortion) {
            /// Successfully loaded external distortion JSON
            auto newMesh =
                jsonToMonoPointMesh(externalDistortion.distortion,
                                    "mono_point_samples_external_file");

            if (!newMesh.empty()) {
                /// and sucessfully turned that JSON into a mesh - return it
                /// through the out param and get out of here!

                std::cout << "OSVRDisplayConfiguration::parse(): Using "
                             "distortion method "
                             "\"mono_point_samples_external_file\": \""
                          << externalDistortion.filename << "\"" << std::endl;
                mesh = std::move(newMesh);
                return;
            }
        }
    }

    // Finally, just try loading the plain-old object itself.
    {
        auto newMesh = jsonToMonoPointMesh(distortion, "mono_point_samples");

        if (newMesh.empty()) {
            /// @todo A proper "no-op" default should be placed here, instead of
            /// erroring out. Not sure what that would be here, since this is
            /// the "last resort" option: what happens if this function returns
            /// without changing mesh?
            throw DisplayConfigurationParseException("Could not create mono "
                                                     "point distortion mesh "
                                                     "from data provided.");
        }
        std::cout << "OSVRDisplayConfiguration::parse(): Using "
                     "distortion method \"mono_point_samples\""
                  << std::endl;
        mesh = std::move(newMesh);
    }
}

inline void parseDistortionRGBPointMeshes(
    Json::Value const& distortion,
    osvr::renderkit::RGBPointDistortionMeshDescriptions& mesh) {
    Json::Value myDistortion = distortion;
    Json::Reader reader;

    // See if we have the name of an external file to parse.  If so, we open it
    // and grab its values to parse.  Otherwise, we parse the ones that they
    // sent in.
    auto externalDistortion =
        getExternalDistortionFile("RGB point samples", reader, distortion,
                                  "rgb_point_samples_external_file");
    if (externalDistortion) {
        myDistortion = externalDistortion.distortion;
    }

    std::array<std::string, 3> names = {
        "red_point_samples", "green_point_samples", "blue_point_samples"};

    for (size_t clr = 0; clr < 3; clr++) {
        const Json::Value eyeArray = myDistortion[names[clr].c_str()];
        if (eyeArray.isNull() || eyeArray.empty()) {
            /// @todo A proper "no-op" default should be placed here, instead of
            /// erroring out.
            std::cerr
                << "OSVRDisplayConfiguration::parse(): ERROR: Couldn't find "
                   "non-empty distortion rgb point distortion for "
                << names[clr] << std::endl;
            throw DisplayConfigurationParseException(
                "Couldn't find non-empty rgb point distortion.");
        }

        mesh[clr].clear();
        for (auto& pointArray : eyeArray) {
            osvr::renderkit::MonoPointDistortionMeshDescription eye;
            if (pointArray.empty()) {
                /// @todo A proper "no-op" default should be placed here,
                /// instead of
                /// erroring out.
                std::cerr << "OSVRDisplayConfiguration::parse(): ERROR: Empty "
                          << " distortion rgb point distortion list for eye!"
                          << std::endl;
                throw DisplayConfigurationParseException(
                    "Empty rgb point distortion list for eye.");
            }
            for (auto& elt : pointArray) {
                std::array<std::array<double, 2>, 2> point;
                if ((elt.size() != 2) || (elt[0].size() != 2) ||
                    (elt[1].size() != 2)) {
                    /// @todo A proper "no-op" default should be placed here,
                    /// instead of erroring out.
                    std::cerr
                        << "OSVRDisplayConfiguration::parse(): ERROR: Malformed"
                        << " distortion rgb point distortion list entry!"
                        << std::endl;
                    throw DisplayConfigurationParseException(
                        "Malformed rgb point distortion list entry.");
                }
                std::array<double, 2> in, out;
                in[0] = (elt[0][0].asDouble());
                in[1] = (elt[0][1].asDouble());
                out[0] = (elt[1][0].asDouble());
                out[1] = (elt[1][1].asDouble());
                point[0] = (in);
                point[1] = (out);
                eye.push_back(point);
            }
            mesh[clr].push_back(eye);
        }
    }
}

inline void parseDistortionPolynomial(Json::Value const& distortion,
                                      std::string const& color,
                                      std::vector<float>& polynomial) {
    const Json::Value distortArray = distortion["polynomial_coeffs_" + color];
    if (distortArray.isNull() || distortArray.empty()) {
        /// @todo A proper "no-op" default should be placed here, instead of
        /// erroring out.
        std::cerr << "OSVRDisplayConfiguration::parse(): ERROR: Couldn't find "
                     "non-empty "
                  << color << " distortion coefficient array!\n";
        throw DisplayConfigurationParseException(
            "Couldn't find non-empty " + color +
            " distortion coefficient array.");
    }
    polynomial.clear();
    for (auto& elt : distortArray) {
        polynomial.push_back(elt.asFloat());
    }
}

/// Returns a resolution and if it should swap eyes.
inline std::pair<OSVRDisplayConfiguration::Resolution, bool>
parseResolution(Json::Value const& resolution) {
    // if there is more than 1 input, display descriptor right now
    // specifies one resolution value for both inputs. that may be
    // changed in the future
    OSVRDisplayConfiguration::Resolution res;
    res.video_inputs = resolution.get("video_inputs", 1).asInt();

    // Window bounds
    res.width = resolution["width"].asInt();
    res.height = resolution["height"].asInt();

    // Display mode - Default to horiz side by side unless we have
    // multiple video inputs, then default to full screen (? seems
    // logical but not strictly what the json schema specifies)
    res.display_mode =
        (res.video_inputs > 1
             ? OSVRDisplayConfiguration::FULL_SCREEN
             : OSVRDisplayConfiguration::HORIZONTAL_SIDE_BY_SIDE);

    auto const& display_mode = resolution["display_mode"];
    if (display_mode.isString()) {
        const std::string display_mode_str = display_mode.asString();
        if ("horz_side_by_side" == display_mode_str) {
            res.display_mode =
                OSVRDisplayConfiguration::HORIZONTAL_SIDE_BY_SIDE;
        } else if ("vert_side_by_side" == display_mode_str) {
            res.display_mode = OSVRDisplayConfiguration::VERTICAL_SIDE_BY_SIDE;
        } else if ("full_screen" == display_mode_str) {
            res.display_mode = OSVRDisplayConfiguration::FULL_SCREEN;
        } else {
            std::cerr << "OSVRDisplayConfiguration::parse(): WARNING: "
                         "Unknown display mode string: "
                      << display_mode_str << " (using default)\n";
        }
    }
    auto const& eyeSwap = resolution["swap_eyes"];
    auto doEyeSwap = !eyeSwap.isNull() && eyeSwap.asBool();
    return std::make_pair(std::move(res), doEyeSwap);
}

void OSVRDisplayConfiguration::parse(const std::string& display_description) {
    Json::Value root;
    {
        Json::Reader reader;
        if (!reader.parse(display_description, root, false)) {
            std::cerr << "OSVRDisplayConfiguration::parse(): ERROR: Couldn't "
                         "parse the display descriptor as JSON!\n";
            throw DisplayConfigurationParseException(
                "Couldn't parse the display descriptor as JSON.");
        }
    }
    using namespace osvr;

    auto const& hmd = root["hmd"];
    {
        auto const& fov = hmd["field_of_view"];
        // Field of view
        m_monocularHorizontalFOV =
            util::Angle(fov["monocular_horizontal"].asDouble() * util::degrees);
        m_monocularVerticalFOV =
            util::Angle(fov["monocular_vertical"].asDouble() * util::degrees);
        m_overlapPercent = fov.get("overlap_percent", 100).asDouble() / 100.0;
        m_pitchTilt =
            util::Angle(fov.get("pitch_tilt", 0).asDouble() * util::degrees);
    }
    {
        // Device properties
        auto processDeviceData = [&](Json::Value const& devprops) {
            m_vendor = devprops["vendor"].asString();
            m_model = devprops["model"].asString();
            m_version = devprops["Version"].asString();
            m_note = devprops["Note"].asString();
        };

        // In the canonical schema, this is where those properties live.
        // However, in some legacy directmode config files, they are in a
        // sub-object called "properties". Thus, we'll call the above lambda
        // with only one of the two.
        auto const& devprops = hmd["device"];
        auto const& subproperties = devprops["properties"];
        if (!subproperties.isNull() && !subproperties.empty()) {
            // legacy descriptor

            std::cerr << "OSVRDisplayConfiguration::parse(): WARNING: Your "
                         "display descriptor is outdated (using an outdated "
                         "schema) and may not work with all applications. "
                         "Please check with your vendor or the OSVR community "
                         "to get an updated one matching the most recent JSON "
                         "Schema for display descriptors.\n";
            processDeviceData(subproperties);
        } else {
            processDeviceData(devprops);
        }
    }
    {
        auto const& resolutions = hmd["resolutions"];
        if (resolutions.isNull()) {
            std::cerr << "OSVRDisplayConfiguration::parse(): ERROR: Couldn't "
                         "find resolutions array!\n";
            throw DisplayConfigurationParseException(
                "Couldn't find resolutions array.");
        }

        bool first = true;
        for (auto const& resolution : resolutions) {
            Resolution res;
            bool swapEyes;
            std::tie(res, swapEyes) = parseResolution(resolution);
            m_resolutions.push_back(res);
            if (first) {
                m_swapEyes = swapEyes;
                first = false;
            }
        }

        if (m_resolutions.empty()) {
            // We couldn't find any appropriate resolution entries

            std::cerr << "OSVRDisplayConfiguration::parse(): ERROR: Couldn't "
                         "find any appropriate resolutions.\n";
            throw DisplayConfigurationParseException(
                "Couldn't find resolutions array or any appropriate "
                "resolutions.");
            return;
        }
    }

    {
        auto const& rendering = hmd["rendering"];
        m_rightRoll = rendering.get("right_roll", 0).asDouble();
        m_leftRoll = rendering.get("left_roll", 0).asDouble();
    }
    {
        auto const& distortion = hmd["distortion"];

        /// We will detect distortion type based on either the explicitly
        /// specified string or the presence of essential object members.
        m_distortionTypeString = distortion["type"].asString();
        if (m_distortionTypeString == "rgb_symmetric_polynomials" ||
            distortion.isMember("polynomial_coeffs_red")) {

            std::cout << "OSVRDisplayConfiguration::parse(): Using polynomial "
                         "distortion.\n";
            m_distortionType = RGB_SYMMETRIC_POLYNOMIALS;
            m_distortionDistanceScaleX =
                distortion.get("distance_scale_x", 1.f).asFloat();
            m_distortionDistanceScaleY =
                distortion.get("distance_scale_y", 1.f).asFloat();
            parseDistortionPolynomial(distortion, "red",
                                      m_distortionPolynomialRed);
            parseDistortionPolynomial(distortion, "green",
                                      m_distortionPolynomialGreen);
            parseDistortionPolynomial(distortion, "blue",
                                      m_distortionPolynomialBlue);
        } else if (m_distortionTypeString == "mono_point_samples" ||
                   distortion.isMember("mono_point_samples") ||
                   distortion.isMember("mono_point_samples_built_in") ||
                   distortion.isMember("mono_point_samples_external_file")) {

            std::cout << "OSVRDisplayConfiguration::parse(): Using mono point "
                         "sample distortion.\n";
            m_distortionType = MONO_POINT_SAMPLES;
            parseDistortionMonoPointMeshes(distortion,
                                           m_distortionMonoPointMesh);

        } else if (m_distortionTypeString == "rgb_point_samples" ||
                   distortion.isMember("rgb_point_samples") ||
                   distortion.isMember("rgb_point_samples_external_file")) {

            std::cout << "OSVRDisplayConfiguration::parse(): Using rgb point "
                         "sample distortion.\n";
            m_distortionType = RGB_POINT_SAMPLES;
            parseDistortionRGBPointMeshes(distortion, m_distortionRGBPointMesh);

        } else if (m_distortionTypeString == "rgb_k1_coefficients") {
#if 0
            // Simple distortion params ignored by RenderManager
            /// @todo how to use these, or use them if non-zero, to set up
            /// distortion?
            double k1_red = distortion.get("k1_red", 0).asDouble();
            double k1_green = distortion.get("k1_green", 0).asDouble();
            double k1_blue = distortion.get("k1_blue", 0).asDouble();
#endif
            m_distortionType = RGB_SYMMETRIC_POLYNOMIALS;
            m_distortionDistanceScaleX = 1.f;
            m_distortionDistanceScaleY = 1.f;
            m_distortionPolynomialRed = {0.f, 1.f};
            m_distortionPolynomialGreen = {0.f, 1.f};
            m_distortionPolynomialBlue = {0.f, 1.f};
        } else if (!m_distortionTypeString.empty()) {
            std::cerr
                << "OSVRDisplayConfiguration::parse(): ERROR: Unrecognized "
                   "distortion type: "
                << m_distortionTypeString << std::endl;
            throw DisplayConfigurationParseException(
                "Unrecognized distortion type: " + m_distortionTypeString);

        } else {
            std::cout << "OSVRDisplayConfiguration::parse(): No "
                         "RenderManager-compatible distortion parameters "
                         "found, falling back to an identity polynomial.\n";
            m_distortionType = RGB_SYMMETRIC_POLYNOMIALS;
            m_distortionDistanceScaleX = 1.f;
            m_distortionDistanceScaleY = 1.f;
            m_distortionPolynomialRed = {0.f, 1.f};
            m_distortionPolynomialGreen = {0.f, 1.f};
            m_distortionPolynomialBlue = {0.f, 1.f};
        }
    }
    {
        auto const& eyes = hmd["eyes"];
        if (eyes.isNull()) {
            std::cerr << "OSVRDisplayConfiguration::parse(): ERROR: "
                         "Couldn't find eyes array!\n";
            throw DisplayConfigurationParseException(
                "Couldn't find eyes array.");
        }
        for (auto const& eye : eyes) {
            EyeInfo e;
            e.m_CenterProjX = eye.get("center_proj_x", 0.5).asDouble();
            e.m_CenterProjY = eye.get("center_proj_y", 0.5).asDouble();
            if (eye.isMember("rotate_180")) {
                auto const& rot = eye["rotate_180"];
                if (rot.isBool()) {
                    e.m_rotate180 = rot.asBool();
                } else {
                    e.m_rotate180 = (rot.asInt() != 0);
                }
            }
            m_eyes.push_back(e);
        }
    }
}

void OSVRDisplayConfiguration::print() const {
    std::cout << "Monocular horizontal FOV: " << m_monocularHorizontalFOV
              << std::endl;
    std::cout << "Monocular vertical FOV: " << m_monocularVerticalFOV
              << std::endl;
    std::cout << "Overlap percent: " << m_overlapPercent << "%" << std::endl;
    std::cout << "Pitch tilt: " << m_pitchTilt << std::endl;
    std::cout << "Resolution: " << activeResolution().width << " x "
              << activeResolution().height << std::endl;
    std::cout << "Video inputs: " << activeResolution().video_inputs
              << std::endl;
    std::cout << "Display mode: " << activeResolution().display_mode
              << std::endl;
    std::cout << "Right roll: " << m_rightRoll << std::endl;
    std::cout << "Left roll: " << m_leftRoll << std::endl;
    std::cout << "Number of eyes: " << m_eyes.size() << std::endl;
    for (std::vector<EyeInfo>::size_type i = 0; i < m_eyes.size(); ++i) {
        std::cout << "Eye " << i << ": " << std::endl;
        m_eyes[i].print();
    }
}

std::string OSVRDisplayConfiguration::getVendor() const { return m_vendor; }

std::string OSVRDisplayConfiguration::getModel() const { return m_model; }

std::string OSVRDisplayConfiguration::getVersion() const { return m_version; }

std::string OSVRDisplayConfiguration::getNote() const { return m_note; }

int OSVRDisplayConfiguration::getNumDisplays() const {
    if (m_eyes.size() < 2) {
        return 1;
    }
    // OK, so two eyes now.
    if (activeResolution().display_mode == DisplayMode::FULL_SCREEN) {
        assert(activeResolution().video_inputs == 2);
        return 2;
    }
    return 1;
}

int OSVRDisplayConfiguration::getDisplayTop() const { return 0; }

int OSVRDisplayConfiguration::getDisplayLeft() const { return 0; }

int OSVRDisplayConfiguration::getDisplayWidth() const {
    return activeResolution().width;
}

int OSVRDisplayConfiguration::getDisplayHeight() const {
    return activeResolution().height;
}

OSVRDisplayConfiguration::DisplayMode
OSVRDisplayConfiguration::getDisplayMode() const {
    return activeResolution().display_mode;
}

osvr::util::Angle OSVRDisplayConfiguration::getVerticalFOV() const {
    return m_monocularVerticalFOV;
}

osvr::util::Angle OSVRDisplayConfiguration::getHorizontalFOV() const {
    return m_monocularHorizontalFOV;
}
#if 0
double OSVRDisplayConfiguration::getVerticalFOV() const {
    return m_MonocularVerticalFOV;
}

double OSVRDisplayConfiguration::getVerticalFOVRadians() const {
    return m_MonocularVerticalFOV * M_PI / 180.0;
}

double OSVRDisplayConfiguration::getHorizontalFOV() const {
    return m_MonocularHorizontalFOV;
}

double OSVRDisplayConfiguration::getHorizontalFOVRadians() const {
    return m_MonocularHorizontalFOV * M_PI / 180.0;
}
#endif

double OSVRDisplayConfiguration::getOverlapPercent() const {
    return m_overlapPercent;
}

osvr::util::Angle OSVRDisplayConfiguration::getPitchTilt() const {
    return m_pitchTilt;
}

double OSVRDisplayConfiguration::getIPDMeters() const { return m_IPDMeters; }

bool OSVRDisplayConfiguration::getSwapEyes() const { return m_swapEyes; }

std::string OSVRDisplayConfiguration::getDistortionTypeString() const {
    return m_distortionTypeString;
}

osvr::renderkit::MonoPointDistortionMeshDescriptions
OSVRDisplayConfiguration::getDistortionMonoPointMeshes() const {
    return m_distortionMonoPointMesh;
}

osvr::renderkit::RGBPointDistortionMeshDescriptions
OSVRDisplayConfiguration::getDistortionRGBPointMeshes() const {
    return m_distortionRGBPointMesh;
}

float OSVRDisplayConfiguration::getDistortionDistanceScaleX() const {
    return m_distortionDistanceScaleX;
}

float OSVRDisplayConfiguration::getDistortionDistanceScaleY() const {
    return m_distortionDistanceScaleY;
}

std::vector<float> const&
OSVRDisplayConfiguration::getDistortionPolynomalRed() const {
    return m_distortionPolynomialRed;
}

std::vector<float> const&
OSVRDisplayConfiguration::getDistortionPolynomalGreen() const {
    return m_distortionPolynomialGreen;
}

std::vector<float> const&
OSVRDisplayConfiguration::getDistortionPolynomalBlue() const {
    return m_distortionPolynomialBlue;
}

std::vector<OSVRDisplayConfiguration::EyeInfo> const&
OSVRDisplayConfiguration::getEyes() const {
    return m_eyes;
}

void OSVRDisplayConfiguration::EyeInfo::print() const {
    std::cout << "Center of projection (X): " << m_CenterProjX << std::endl;
    std::cout << "Center of projection (Y): " << m_CenterProjY << std::endl;
    std::cout << "Rotate by 180: " << m_rotate180 << std::endl;
}
