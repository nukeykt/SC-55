/*
 * Copyright (C) 2021, 2024 nukeykt
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <cstdint>

template <typename T>
inline T Min(T a, T b)
{
    return a < b ? a : b;
}

template <typename T>
inline T Clamp(T value, T min, T max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

inline int16_t SaturatingAdd(int16_t a, int16_t b)
{
    int32_t result = (int32_t)a + (int32_t)b;
    return (int16_t)Clamp<int32_t>(result, INT16_MIN, INT16_MAX);
}

inline int32_t SaturatingAdd(int32_t a, int32_t b)
{
    int64_t result = (int64_t)a + (int64_t)b;
    return (int32_t)Clamp<int64_t>(result, INT32_MIN, INT32_MAX);
}

// Auto vectorizes in clang at -O2, gcc at -O3
inline void HorizontalSatAddI16(int16_t* dest, int16_t* src_first, int16_t* src_last)
{
    while (src_first != src_last)
    {
        *dest = SaturatingAdd(*dest, *src_first);
        ++src_first;
        ++dest;
    }
}

inline void HorizontalSatAddI32(int32_t* dest, int32_t* src_first, int32_t* src_last)
{
    while (src_first != src_last)
    {
        *dest = SaturatingAdd(*dest, *src_first);
        ++src_first;
        ++dest;
    }
}

inline void HorizontalAddF32(float* dest, float* src_first, float* src_last)
{
    while (src_first != src_last)
    {
        *dest = *dest + *src_first;
        ++src_first;
        ++dest;
    }
}
