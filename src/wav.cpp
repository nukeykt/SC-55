// We use fopen()
#define _CRT_SECURE_NO_WARNINGS

#include "wav.h"
#include "cast.h"

#include <bit>
#include <cassert>
#include <cstring>

// Constants from rfc2361
enum class WaveFormat : uint16_t
{
    PCM        = 0x0001,
    IEEE_FLOAT = 0x0003,
};

void WAV_WriteBytes(FILE* output, const char* bytes, size_t len)
{
    fwrite(bytes, 1, len, output);
}

void WAV_WriteCString(FILE* output, const char* s)
{
    WAV_WriteBytes(output, s, strlen(s));
}

void WAV_WriteU16LE(FILE* output, uint16_t value)
{
    if constexpr (std::endian::native == std::endian::big)
    {
        value = std::byteswap(value);
    }
    WAV_WriteBytes(output, (const char*)&value, sizeof(uint16_t));
}

void WAV_WriteU32LE(FILE* output, uint32_t value)
{
    if constexpr (std::endian::native == std::endian::big)
    {
        value = std::byteswap(value);
    }
    WAV_WriteBytes(output, (const char*)&value, sizeof(uint32_t));
}

void WAV_WriteF32LE(FILE* output, float value)
{
    // byteswap is only implemented for integral types, so forward the call to
    // the U32 implementation
    WAV_WriteU32LE(output, std::bit_cast<uint32_t>(value));
}

WAV_Handle::~WAV_Handle()
{
    Close();
}

WAV_Handle::WAV_Handle(WAV_Handle&& rhs) noexcept
{
    m_output         = rhs.m_output;
    rhs.m_output     = nullptr;
    m_format         = rhs.m_format;
    m_sample_rate    = rhs.m_sample_rate;
    m_frames_written = rhs.m_frames_written;
}

WAV_Handle& WAV_Handle::operator=(WAV_Handle&& rhs) noexcept
{
    Close();
    m_output         = rhs.m_output;
    rhs.m_output     = nullptr;
    m_format         = rhs.m_format;
    m_sample_rate    = rhs.m_sample_rate;
    m_frames_written = rhs.m_frames_written;
    return *this;
}

void WAV_Handle::SetSampleRate(uint32_t sample_rate)
{
    m_sample_rate = sample_rate;
}

void WAV_Handle::OpenStdout(AudioFormat format)
{
    m_format = format;
    m_output = stdout;
}

void WAV_Handle::Open(const char* filename, AudioFormat format)
{
    Open(std::filesystem::path(filename), format);
}

void WAV_Handle::Open(const std::filesystem::path& filename, AudioFormat format)
{
    m_format = format;
    m_output = fopen(filename.generic_string().c_str(), "wb");
    fseek(m_output, format == AudioFormat::F32 ? 58 : 44, SEEK_SET);
}

void WAV_Handle::Close()
{
    if (m_output && m_output != stdout)
    {
        fclose(m_output);
    }
    m_output = nullptr;
}

void WAV_Handle::Write(const AudioFrame<int16_t>& frame)
{
    WAV_WriteU16LE(m_output, std::bit_cast<uint16_t>(frame.left));
    WAV_WriteU16LE(m_output, std::bit_cast<uint16_t>(frame.right));
    ++m_frames_written;
}

void WAV_Handle::Write(const AudioFrame<int32_t>& frame)
{
    WAV_WriteU32LE(m_output, std::bit_cast<uint32_t>(frame.left));
    WAV_WriteU32LE(m_output, std::bit_cast<uint32_t>(frame.right));
    ++m_frames_written;
}

void WAV_Handle::Write(const AudioFrame<float>& frame)
{
    WAV_WriteF32LE(m_output, frame.left);
    WAV_WriteF32LE(m_output, frame.right);
    ++m_frames_written;
}

void WAV_Handle::Finish()
{
    // we wrote raw samples, nothing to do
    if (m_output == stdout)
    {
        return;
    }

    // go back and fill in the header
    fseek(m_output, 0, SEEK_SET);

    switch (m_format)
    {
    case AudioFormat::S16: {
        const uint32_t data_size = RangeCast<uint32_t>(m_frames_written * sizeof(AudioFrame<int16_t>));

        // RIFF header
        WAV_WriteCString(m_output, "RIFF");
        WAV_WriteU32LE(m_output, 36 + data_size);
        WAV_WriteCString(m_output, "WAVE");
        // fmt
        WAV_WriteCString(m_output, "fmt ");
        WAV_WriteU32LE(m_output, 16);
        WAV_WriteU16LE(m_output, (uint16_t)WaveFormat::PCM);
        WAV_WriteU16LE(m_output, AudioFrame<int16_t>::channel_count);
        WAV_WriteU32LE(m_output, m_sample_rate);
        WAV_WriteU32LE(m_output, m_sample_rate * sizeof(AudioFrame<int16_t>));
        WAV_WriteU16LE(m_output, sizeof(AudioFrame<int16_t>));
        WAV_WriteU16LE(m_output, 8 * sizeof(int16_t));
        // data
        WAV_WriteCString(m_output, "data");
        WAV_WriteU32LE(m_output, data_size);

        assert(ftell(m_output) == 44);

        break;
    }
    case AudioFormat::S32: {
        const uint32_t data_size = RangeCast<uint32_t>(m_frames_written * sizeof(AudioFrame<int32_t>));

        // RIFF header
        WAV_WriteCString(m_output, "RIFF");
        WAV_WriteU32LE(m_output, 36 + data_size);
        WAV_WriteCString(m_output, "WAVE");
        // fmt
        WAV_WriteCString(m_output, "fmt ");
        WAV_WriteU32LE(m_output, 16);
        WAV_WriteU16LE(m_output, (uint16_t)WaveFormat::PCM);
        WAV_WriteU16LE(m_output, AudioFrame<int32_t>::channel_count);
        WAV_WriteU32LE(m_output, m_sample_rate);
        WAV_WriteU32LE(m_output, m_sample_rate * sizeof(AudioFrame<int32_t>));
        WAV_WriteU16LE(m_output, sizeof(AudioFrame<int32_t>));
        WAV_WriteU16LE(m_output, 8 * sizeof(int32_t));
        // data
        WAV_WriteCString(m_output, "data");
        WAV_WriteU32LE(m_output, data_size);

        assert(ftell(m_output) == 44);

        break;
    }
    case AudioFormat::F32: {
        const uint32_t data_size = RangeCast<uint32_t>(m_frames_written * sizeof(AudioFrame<float>));

        // RIFF header
        WAV_WriteCString(m_output, "RIFF");
        WAV_WriteU32LE(m_output, 50 + data_size);
        WAV_WriteCString(m_output, "WAVE");
        // fmt
        WAV_WriteCString(m_output, "fmt ");
        WAV_WriteU32LE(m_output, 18);
        WAV_WriteU16LE(m_output, (uint16_t)WaveFormat::IEEE_FLOAT);
        WAV_WriteU16LE(m_output, AudioFrame<float>::channel_count);
        WAV_WriteU32LE(m_output, m_sample_rate);
        WAV_WriteU32LE(m_output, m_sample_rate * sizeof(AudioFrame<float>));
        WAV_WriteU16LE(m_output, sizeof(AudioFrame<float>));
        WAV_WriteU16LE(m_output, 8 * sizeof(float));
        WAV_WriteU16LE(m_output, 0);
        // fact
        WAV_WriteCString(m_output, "fact");
        WAV_WriteU32LE(m_output, 4);
        WAV_WriteU32LE(m_output, RangeCast<uint32_t>(m_frames_written));
        // data
        WAV_WriteCString(m_output, "data");
        WAV_WriteU32LE(m_output, data_size);

        assert(ftell(m_output) == 58);

        break;
    }
    }

    Close();
}
