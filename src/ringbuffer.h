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

#include "audio.h"
#include "math_util.h"
#include <memory>
#include <span>

// This type has reference semantics.
class GenericBuffer
{
public:
    GenericBuffer() = default;

    ~GenericBuffer()
    {
        Free();
    }

    GenericBuffer(const GenericBuffer&)            = delete;
    GenericBuffer& operator=(const GenericBuffer&) = delete;

    GenericBuffer(GenericBuffer&&)            = delete;
    GenericBuffer& operator=(GenericBuffer&&) = delete;

    bool Init(size_t size_bytes)
    {
        size_t alloc_size = 64 + size_bytes;

        m_alloc_base = malloc(alloc_size);
        if (!m_alloc_base)
        {
            return false;
        }

        m_buffer      = m_alloc_base;
        m_buffer_size = size_bytes;
        if (!std::align(64, size_bytes, m_buffer, alloc_size))
        {
            Free();
            return false;
        }

        return true;
    }

    void Free()
    {
        if (m_alloc_base)
        {
            free(m_alloc_base);
        }
        m_buffer      = nullptr;
        m_buffer_size = 0;
        m_alloc_base  = nullptr;
    }

    void* DataFirst()
    {
        return std::assume_aligned<64>(m_buffer);
    }

    void* DataLast()
    {
        return (uint8_t*)DataFirst() + m_buffer_size;
    }

    size_t GetByteLength() const
    {
        return m_buffer_size;
    }

private:
    void*  m_buffer      = nullptr;
    size_t m_buffer_size = 0;
    void*  m_alloc_base  = nullptr;
};

template <typename ElemT>
class RingbufferView
{
public:
    RingbufferView() = default;

    explicit RingbufferView(GenericBuffer& buffer)
        : m_buffer((uint8_t*)buffer.DataFirst(), (uint8_t*)buffer.DataLast())
    {
        m_read_head  = 0;
        m_write_head = 0;
        m_elem_count = buffer.GetByteLength() / sizeof(ElemT);
    }

    void UncheckedWriteOne(const ElemT& value)
    {
        *GetWritePtr() = value;
        m_write_head   = (m_write_head + 1) % m_elem_count;
    }

    void UncheckedReadOne(ElemT& dest)
    {
        dest        = *GetReadPtr();
        m_read_head = (m_read_head + 1) % m_elem_count;
    }

    size_t GetReadableCount() const
    {
        if (m_read_head <= m_write_head)
        {
            return m_write_head - m_read_head;
        }
        else
        {
            return m_elem_count - (m_read_head - m_write_head);
        }
    }

    size_t GetWritableCount() const
    {
        if (m_read_head <= m_write_head)
        {
            return m_elem_count - (m_write_head - m_read_head) - 1;
        }
        else
        {
            return m_read_head - m_write_head - 1;
        }
    }

private:
    ElemT* GetWritePtr()
    {
        return (ElemT*)m_buffer.data() + m_write_head;
    }

    const ElemT* GetReadPtr() const
    {
        return (ElemT*)m_buffer.data() + m_read_head;
    }

private:
    std::span<uint8_t> m_buffer;
    size_t             m_read_head  = 0;
    size_t             m_write_head = 0;
    size_t             m_elem_count = 0;
};

inline void MixFrame(AudioFrame<int16_t>& dest, const AudioFrame<int16_t>& src)
{
    dest.left  = SaturatingAdd(dest.left, src.left);
    dest.right = SaturatingAdd(dest.right, src.right);
}

inline void MixFrame(AudioFrame<int32_t>& dest, const AudioFrame<int32_t>& src)
{
    dest.left  = SaturatingAdd(dest.left, src.left);
    dest.right = SaturatingAdd(dest.right, src.right);
}

inline void MixFrame(AudioFrame<float>& dest, const AudioFrame<float>& src)
{
    dest.left  += src.left;
    dest.right += src.right;
}

// Reads up to `frame_count` frames and returns the number of frames
// actually read. Mixes samples into dest by adding and clipping.
template <typename SampleT>
size_t ReadMix(RingbufferView<AudioFrame<SampleT>>& rb, AudioFrame<SampleT>* dest, size_t frame_count)
{
    const size_t have_count = rb.GetReadableCount();
    const size_t read_count = Min(have_count, frame_count);
    for (size_t i = 0; i < read_count; ++i)
    {
        AudioFrame<SampleT> src;
        rb.UncheckedReadOne(src);
        MixFrame(dest[i], src);
    }
    return read_count;
}
