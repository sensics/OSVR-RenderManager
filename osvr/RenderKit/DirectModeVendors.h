/** @file
    @brief Header

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
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

#ifndef INCLUDED_DirectModeVendors_h_GUID_DFCDE7B9_78B9_44B1_85A5_2E661722FF93
#define INCLUDED_DirectModeVendors_h_GUID_DFCDE7B9_78B9_44B1_85A5_2E661722FF93

// Internal Includes
#include <osvr/RenderKit/VendorIdTools.h>

// Library/third-party includes
#include <osvr/Common/IntegerByteSwap.h>

// Standard includes
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <sstream>
#include <iomanip>
#include <utility>
#include <assert.h>

namespace osvr {
namespace renderkit {
    namespace vendorid {

        /// This is in here, rather than VendorIdTools, since it uses an OSVR header, and that file doesn't need
        /// anything but some simple standard headers.
        template <typename T> inline std::uint16_t pnpidToFlippedHex(T&& pnpid) {
            return common::integerByteSwap(pnpidToHex(std::forward<T>(pnpid)));
        }

        /// formats a 16-bit uint (like a hex PNPID) like 0x0000
        inline std::string formatAsHexString(std::uint16_t v) {
            std::ostringstream os;
            os << "0x" << std::hex << std::setw(4) << std::setfill('0') << v;
            return os.str();
        }

        /// @brief A class storing an association between a PNPID vendor ID as found in EDID data, a "Vendor" name as
        /// found in OSVR display descriptor files (schema v1), and an optional user-friendly description.
        ///
        /// Used primarily in a static data table processed at runtime (replacing extensive conditionals and ad-hoc code
        /// repetition) by various apps and libraries.
        class DirectModeVendorEntry {
          public:
            /// @name Constructors
            /// @brief Used to create entries in the table of vendors, Identically-named parameters perform identically
            /// between constructors.
            /// @{
            /// @brief Minimal/brief/stealth entry - just PNPID. Sets the display descriptor vendor to be the same as
            /// the PNPID.
            /// @param pnpid A 3-character, all caps string literal matching /^[A-Z]+$/ . See getPNPNIDCharArray() for
            /// details. WARNING: For speed of initialization, your input here is not validated to ensure it is
            /// all-caps, A-Z only!
            explicit DirectModeVendorEntry(PNPIDStringLiteralType pnpid) : DirectModeVendorEntry(pnpid, pnpid) {}
            /// @brief Basic/common entry
            /// @param pnpid See above
            /// @param dispDescVend A string literal used as a "Vendor" string in an OSVR Display Descriptor (schema v1)
            /// that should trigger searching for devices with this PNPID.
            DirectModeVendorEntry(PNPIDStringLiteralType pnpid, const char* dispDescVend)
                : pnpid_(stringLiteralPNPIDToArray(pnpid)), displayDescriptorVendor(dispDescVend) {}
            /// @brief Elaborated entry
            /// @param pnpid See above
            /// @param dispDescVend See above
            /// @param desc A human-readable description of devices with this vendor and PNPID. For instance, when a
            /// single vendor has multiple devices with PNPIDs, and/or when multiple vendors share a PNPID.
            DirectModeVendorEntry(PNPIDStringLiteralType pnpid, const char* dispDescVend, const char* desc)
                : pnpid_(stringLiteralPNPIDToArray(pnpid)), displayDescriptorVendor(dispDescVend), description(desc) {}
            /// @}

            /// @brief Returns the PNPID as a std::array of chars including the null terminator:
            ///
            /// 3 character, all-caps, in A-Z, PNPID, preferably registered through the UEFI registry. These should
            /// technically be registered through http://www.uefi.org/PNP_ACPI_Registry (at least respecting assignments
            /// made there) and must match the vendor ID reported in your EDID data (see Windows "Hardware IDs")
            PNPIDNullTerminatedStdArray const& getPNPIDCharArray() const { return pnpid_; }

            /// @brief Convenience method to access the data of getPNPIDCharArray() as a null-terminated C string.
            const char* getPNPIDCString() const { return pnpid_.data(); }

            /// @brief Converts the PNPID to two hex bytes for use in an EDID, per the formula established by Microsoft,
            /// then swaps the bytes as seems to be required by most consumers of this data.
            std::uint16_t getFlippedHexPNPID() const {
                /// @todo will this only work on little-endian systems? Need to figure out why the byte swap is needed.
                return pnpidToFlippedHex(getPNPIDCharArray());
            }

            /// @brief Gets the byte-flipped hex PNPID in display-ready 0x0000 string format.
            std::string getFlippedHexPNPIDAsHexString() const { return formatAsHexString(getFlippedHexPNPID()); }

            /// @brief Returns the string as provided in the constructor.
            std::string const& getDisplayDescriptorVendor() const { return displayDescriptorVendor; }

            /// @brief Returns the string provided in constructor, if any - if not, it returns the same as
            /// getDisplayDescriptorVendor()
            std::string const& getDescription() const {
                return description.empty() ? displayDescriptorVendor : description;
            }

          private:
            /// PNPID as a std::array of chars including the null terminator.
            /// std::array is used for bounds/type-safety and protection from decay-to-pointer as well as operator
            /// overloads: it is easily compared, copied, etc.
            PNPIDNullTerminatedStdArray pnpid_;
            /// Vendor string to match in display descriptor.
            std::string displayDescriptorVendor;
            /// Description - if left empty, getDescription will return displayDescriptorVendor instead.
            std::string description;
        };

        class PNPIDWithDescriptions {
          public:
            /// Construct from a single vendor entry.
            explicit PNPIDWithDescriptions(DirectModeVendorEntry const& vendorEntry)
                : pnpid_(vendorEntry.getPNPIDCharArray()) {
                descs_.push_back(vendorEntry.getDescription());
            }

            /// Compares PNPID in the given vendor entry to our own.
            bool isPNPIDMatch(DirectModeVendorEntry const& vendorEntry) const {
                return vendorEntry.getPNPIDCharArray() == pnpid_;
            }

            /// Compares PNPID in the given vendor entry to our own, and if it is a match, adds the vendor entry's
            /// description to our own list of descriptions.
            bool addIfMatch(DirectModeVendorEntry const& vendorEntry) {
                if (!isPNPIDMatch(vendorEntry)) {
                    return false;
                }
                descs_.push_back(vendorEntry.getDescription());
                return true;
            }

            /// @brief Returns the PNPID as a std::array of chars including the null terminator:
            ///
            /// 3 character, all-caps, in A-Z, PNPID, preferably registered through the UEFI registry. These should
            /// technically be registered through http://www.uefi.org/PNP_ACPI_Registry (at least respecting assignments
            /// made there) and must match the vendor ID reported in your EDID data (see Windows "Hardware IDs")
            PNPIDNullTerminatedStdArray const& getPNPIDCharArray() const { return pnpid_; }

            /// @brief Convenience method to access the data of getPNPIDCharArray() as a null-terminated C string.
            const char* getPNPIDCString() const { return pnpid_.data(); }

            /// @brief Converts the PNPID to two hex bytes for use in an EDID, per the formula established by Microsoft,
            /// then swaps the bytes as seems to be required by most consumers of this data.
            std::uint16_t getFlippedHexPNPID() const {
                /// @todo will this only work on little-endian systems? Need to figure out why the byte swap is needed.
                return pnpidToFlippedHex(getPNPIDCharArray());
            }

            /// @brief Gets the byte-flipped hex PNPID in display-ready 0x0000 string format.
            std::string getFlippedHexPNPIDAsHexString() const { return formatAsHexString(getFlippedHexPNPID()); }

            using DescriptionList = std::vector<std::string>;

            DescriptionList const& getDescriptionList() const { return descs_; }

            /// Return a string with all the descriptions joined with the given separator between them.
            std::string getDescriptionsJoined(const char* separator) const {
                std::ostringstream os;
                auto it = getDescriptionList().begin();
                auto e = getDescriptionList().end();
                os << *it;
                for (++it; it != e; ++it) {
                    os << separator << *it;
                }
                return os.str();
            }

          private:
            PNPIDNullTerminatedStdArray pnpid_;

            DescriptionList descs_;
        };

        using DirectModeVendors = std::vector<DirectModeVendorEntry>;
        using PNPIDsWithDescriptions = std::vector<PNPIDWithDescriptions>;

        static inline PNPIDsWithDescriptions generatePNPIDsWithDescriptions(DirectModeVendors const& vendors) {
            PNPIDsWithDescriptions ret;
            for (auto& entry : vendors) {
                auto matchCount = std::count_if(ret.begin(), ret.end(), [&](PNPIDWithDescriptions& existing) {
                    return existing.addIfMatch(entry);
                });
                if (matchCount > 1) {
                    assert(0 && "Should not happen!");
                }
                if (0 == matchCount) {
                    // didn't match any existing ones, must add a new one.
                    ret.emplace_back(entry);
                }
            }
            return ret;
        }
    } // namespace vendorid

    using vendorid::pnpidToFlippedHex;
    using vendorid::DirectModeVendors;

    /// Returns the list of vendors that it's always safe to command to enter/exit direct mode.
    static DirectModeVendors const& getDefaultVendors() {
        using Vendor = vendorid::DirectModeVendorEntry;
        static DirectModeVendors vendors = DirectModeVendors{
            Vendor{"OVR", "Oculus"},
            Vendor{"SEN", "Sensics", "Some Sensics professional displays"},
            Vendor{"SVR", "Sensics", "Some Sensics professional displays"},
            Vendor{"SEN", "OSVR", "Some OSVR HDK units with early firmware/EDID data"},
            Vendor{"SVR", "OSVR", "Most OSVR HDK units"},
            Vendor{"AUO", "OSVR", "Some OSVR HDK2 firmware versions"},
            Vendor{"VVR"},
            Vendor{"IWR", "Vuzix"},
            Vendor{"HVR", "HTC"},
            Vendor{"AVR"},
            Vendor{"VRG", "VRGate"},
            Vendor{"TSB", "VRGate"},
            Vendor{"VRV", "Vrvana"},
            Vendor{"TVR", "TotalVision"},
            /* add new vendors here - keep grouped by display descriptor vendor */
        };
        return vendors;
    }

    /// Returns the list of vendors that can be checked for direct mode, but that shouldn't be put into/out of direct
    /// mode automatically.
    static DirectModeVendors const& getNonDefaultVendors() {
        using Vendor = vendorid::DirectModeVendorEntry;
        static DirectModeVendors vendors = DirectModeVendors{
            Vendor{"SAM", "Samsung"},
            /* add new vendors here - keep grouped by display descriptor vendor */
        };
        return vendors;
    }

    using vendorid::PNPIDsWithDescriptions;
    static PNPIDsWithDescriptions const& getDefaultPNPIDsWithDescriptions() {
        static PNPIDsWithDescriptions vendorsByPNPID = vendorid::generatePNPIDsWithDescriptions(getDefaultVendors());
        return vendorsByPNPID;
    }
} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_DirectModeVendors_h_GUID_DFCDE7B9_78B9_44B1_85A5_2E661722FF93
