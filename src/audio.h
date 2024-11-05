#pragma once

#include "math_util.h"
#include <cstddef>
#include <cstdint>

enum class AudioFormat
{
    S16,
    S32,
    F32,
};

template <typename T>
struct AudioFrame
{
    T left;
    T right;

    static constexpr size_t channel_count = 2;
};

inline void Normalize(const AudioFrame<int32_t>& in, AudioFrame<int16_t>& out)
{
    out.left  = (int16_t)Clamp<int32_t>(in.left >> 15, INT16_MIN, INT16_MAX);
    out.right = (int16_t)Clamp<int32_t>(in.right >> 15, INT16_MIN, INT16_MAX);
}

inline void Normalize(const AudioFrame<int32_t>& in, AudioFrame<int32_t>& out)
{
    out.left  = (int32_t)Clamp<int64_t>((int64_t)in.left << 1, INT32_MIN, INT32_MAX);
    out.right = (int32_t)Clamp<int64_t>((int64_t)in.right << 1, INT32_MIN, INT32_MAX);
}

inline void Normalize(const AudioFrame<int32_t>& in, AudioFrame<float>& out)
{
    constexpr float DIV_REC = 1.0f / 536870912.0f;

    out.left  = (float)in.left * DIV_REC;
    out.right = (float)in.right * DIV_REC;
}
