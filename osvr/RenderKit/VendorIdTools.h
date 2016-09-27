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
#include <type_traits>

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
            2 * 2 * 2 * 2 * 2 - 1; /// 5 bits allocated for each letter, so mask is 2^5 -1

        /// Convert one A-Z letter to a hex value, unshifted.
        inline std::uint16_t charToHex(char const letter) {
            /// @todo is the mask helpful? It's theoretically redundant, but it
            /// ensures we don't mess up anyone else's spot.
            return (std::toupper(letter) - BASE_LETTER + BASE_VALUE) & LETTER_MASK;
        }

        /// Convert one unshifted hex value back to an A-Z letter.
        inline char hexToChar(std::uint16_t isolatedLetter) {
            return static_cast<char>(isolatedLetter & LETTER_MASK) - BASE_VALUE + BASE_LETTER;
        }

        /// Convert something stringy of length 3, intended to be a PNPID,
        /// into the hex equivalent.
        /// Note that NVIDIA likes the byte order flipped.
        template <typename T> inline std::uint16_t pnpidToHex(T const& pnpid) {
            return (charToHex(pnpid[0]) << BIT_OFFSET_0) | //
                   (charToHex(pnpid[1]) << BIT_OFFSET_1) | //
                   (charToHex(pnpid[2]) << BIT_OFFSET_2);
        }

        /// This is explicitly a reference to a const char array of length 4 (intended for 3 characters and a null
        /// terminator as a string literal) - this is the tidiest/least gross way to declare that type
        using PNPIDStringLiteralType = std::add_lvalue_reference<const char[4]>::type;

        /// This is the preferred alternative to PNPIDStringLiteralType's referred-to type since it is easily copied and
        /// compared, doesn't decay to a pointer type, but is otherwise the same in size and performance: a std::array
        /// of char with 4 elements (3 non-null and a null terminator)
        using PNPIDNullTerminatedStdArray = std::array<char, 4>;

        /// Create a PNPIDNullTerminatedStdArray given a string literal (const char [4])
        ///
        /// Direct construction of PNPIDNullTerminatedStdArray from a const char [4], even wrapped in curly braces, is a
        /// bit hit or miss - where misses include important environments like GCC 4.9, hence this helper function,
        /// which also ensures null termination (but not any of the other invariants/requirements of a PNPID!)
        inline PNPIDNullTerminatedStdArray stringLiteralPNPIDToArray(PNPIDStringLiteralType pnpid) {
            return PNPIDNullTerminatedStdArray{pnpid[0], pnpid[1], pnpid[2], '\0'};
        }

        /// Convert the full two-byte hex VID into a null-terminated
        /// 3-character PNP ID.
        /// Note that NVIDIA likes the byte order flipped.
        inline PNPIDNullTerminatedStdArray fullHexVidToPnp(std::uint16_t vid) {
            return PNPIDNullTerminatedStdArray{
                hexToChar(vid >> BIT_OFFSET_0), //< shift off to put first character in bits 0-7 then decode
                hexToChar(vid >> BIT_OFFSET_1), //< shift off for second character then decode
                hexToChar(vid >> BIT_OFFSET_2), //< shift off for third character then decode
                '\0'                            //< always null terminated
            };
        }
    } // namespace vendorid
    using vendorid::pnpidToHex;

} // namespace renderkit
} // namespace osvr
#endif // INCLUDED_VendorIdTools_h_GUID_D1D76390_D7F9_419D_709C_2893EB16704C
