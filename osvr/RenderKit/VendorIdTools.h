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

#ifndef INCLUDED_VendorIdTools_h_GUID_D1D76390_D7F9_419D_709C_2893EB16704C
#define INCLUDED_VendorIdTools_h_GUID_D1D76390_D7F9_419D_709C_2893EB16704C

// Internal Includes
// - none

// Library/third-party includes
// - none

// Standard includes
#include <cstdint>
#include <cstddef>
#include <cctype>
#include <array>

namespace osvr {
namespace renderkit {
    namespace vendorid {
        static const char BASE_LETTER = 'A';
        static const char BASE_VALUE = 0x01;

        /// Left-shift amount for the first letter in the ID
        static const std::size_t BIT_OFFSET_0 = 10;
        /// Left-shift amount for the second letter in the ID
        static const std::size_t BIT_OFFSET_1 = 5;
        /// Left-shift amount for the third letter in the ID - dummy just for
        /// parallel code structure.
        static const std::size_t BIT_OFFSET_2 = 0;

        static const std::uint16_t LETTER_MASK =
            2 * 2 * 2 * 2 * 2 -
            1; /// 5 bits allocated for each letter, so mask is 2^5 -1

        /// Convert one A-Z letter to a hex value, unshifted.
        inline std::uint16_t charToHex(char const letter) {
            /// @todo is the mask helpful? It's theoretically redundant, but it
            /// ensures we don't mess up anyone else's spot.
            return (std::toupper(letter) - BASE_LETTER + BASE_VALUE) &
                   LETTER_MASK;
        }

        /// Convert one unshifted hex value back to an A-Z letter.
        inline char hexToChar(std::uint16_t isolatedLetter) {
            return static_cast<char>(isolatedLetter & LETTER_MASK) -
                   BASE_VALUE + BASE_LETTER;
        }

        /// Convert something stringy of length 3, intended to be a PNPID,
        /// into the hex equivalent.
        /// Note that NVIDIA likes the byte order flipped.
        template <typename T> inline std::uint16_t pnpidToHex(T const& pnpid) {
            return (charToHex(pnpid[0]) << BIT_OFFSET_0) |
                   (charToHex(pnpid[1]) << BIT_OFFSET_1) |
                   (charToHex(pnpid[2]) << BIT_OFFSET_2);
        }

        /// Convert the full two-byte hex VID into a null-terminated
        /// 3-character PNP ID.
        /// Note that NVIDIA likes the byte order flipped.
        inline std::array<char, 4> fullHexVidToPnp(std::uint16_t vid) {
            return std::array<char, 4>{hexToChar(vid >> BIT_OFFSET_0),
                                       hexToChar(vid >> BIT_OFFSET_1),
                                       hexToChar(vid >> BIT_OFFSET_2), '\0'};
        }
    } // namespace vendorid
    using vendorid::pnpidToHex;

} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_VendorIdTools_h_GUID_D1D76390_D7F9_419D_709C_2893EB16704C
