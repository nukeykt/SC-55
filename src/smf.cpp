#include "smf.h"
#include "cast.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <fstream>
#include <cstring>
#include <cinttypes>

// security: do not call without verifying [ptr,ptr+1] is a readable range
// performance: 16 bit load + rol in clang and gcc, worse in MSVC
[[nodiscard]]
inline uint16_t UncheckedLoadU16BE(const uint8_t* ptr)
{
    return (uint16_t)((uint16_t)(*ptr) << 8) | (uint16_t)(*(ptr + 1));
}

// security: do not call without verifying [ptr,ptr+3] is a readable range
// performance: 32 bit load + bswap in clang and gcc, worse in MSVC
[[nodiscard]]
inline uint32_t UncheckedLoadU32BE(const uint8_t* ptr)
{
    return ((uint32_t)(*(ptr + 0)) << 24) |
           ((uint32_t)(*(ptr + 1)) << 16) |
           ((uint32_t)(*(ptr + 2)) << 8)  |
           ((uint32_t)(*(ptr + 3)) << 0);
}

class SMF_Reader
{
public:
    SMF_Reader(SMF_ByteSpan bytes)
        : m_bytes(bytes)
    {
    }

    [[nodiscard]]
    bool AtEnd() const
    {
        return m_offset == m_bytes.size();
    }

    [[nodiscard]]
    bool PutBack()
    {
        if (m_offset == 0)
        {
            return false;
        }
        --m_offset;
        return true;
    }

    [[nodiscard]]
    bool Skip(size_t count)
    {
        if (count > RemainingBytes())
        {
            return false;
        }
        m_offset += count;
        return true;
    }

    [[nodiscard]]
    size_t RemainingBytes() const
    {
        return m_bytes.size() - m_offset;
    }

    [[nodiscard]]
    bool ReadU8(uint8_t& value)
    {
        if (RemainingBytes() < sizeof(uint8_t))
        {
            return false;
        }
        value = m_bytes[m_offset];
        ++m_offset;
        return true;
    }

    [[nodiscard]]
    bool ReadU16BE(uint16_t& value)
    {
        if (RemainingBytes() < sizeof(uint16_t))
        {
            return false;
        }
        value = UncheckedLoadU16BE(&m_bytes[m_offset]);
        m_offset += sizeof(uint16_t);
        return true;
    }

    [[nodiscard]]
    bool ReadU32BE(uint32_t& value)
    {
        if (RemainingBytes() < sizeof(uint32_t))
        {
            return false;
        }
        value = UncheckedLoadU32BE(&m_bytes[m_offset]);
        m_offset += sizeof(uint32_t);
        return true;
    }

    [[nodiscard]]
    bool ReadBytes(uint8_t* destination, size_t count)
    {
        if (RemainingBytes() < count)
        {
            return false;
        }
        memcpy(destination, &m_bytes[m_offset], count);
        m_offset += count;
        return true;
    }

    size_t GetOffset() const
    {
        return m_offset;
    }

private:
    SMF_ByteSpan m_bytes;
    size_t       m_offset = 0;
};

inline void Check(bool stat, const char* msg)
{
    if (!stat)
    {
        fprintf(stderr, "Panic: %s\n", msg);
        exit(1);
    }
}

#define STR1(x) #x
#define STR2(x) STR1(x)
#define CHECK(expr) Check((expr), __FILE__ ":" STR2(__LINE__) ": " #expr)

static void SMF_ReadHeader(SMF_Reader& reader, SMF_Header& header)
{
    CHECK(reader.ReadU16BE(header.format));
    CHECK(reader.ReadU16BE(header.ntrks));
    CHECK(reader.ReadU16BE(header.division));
}

[[nodiscard]]
static bool SMF_ReadVarint(SMF_Reader& reader, uint32_t& value)
{
    value = 0;
    for (int i = 0; i < 4; ++i)
    {
        uint8_t byte = 0;
        if (!reader.ReadU8(byte))
        {
            return false;
        }
        value = (value << 7) | (uint32_t)(byte & 0x7F);
        if ((byte & 0x80) == 0)
        {
            break;
        }
    }
    return true;
}

void SMF_SetDeltasFromTimestamps(SMF_Track& track)
{
    for (size_t i = 1; i < track.events.size(); ++i)
    {
        track.events[i].delta_time = RangeCast<uint32_t>(
            track.events[i].timestamp - track.events[i - 1].timestamp
        );
    }
}

SMF_Track SMF_MergeTracks(const SMF_Data& data)
{
    SMF_Track merged_track;
    for (const SMF_Track& track : data.tracks)
    {
        merged_track.events.insert(
            merged_track.events.end(),
            track.events.begin(),
            track.events.end());
    }
    std::stable_sort(merged_track.events.begin(), merged_track.events.end(), [](const SMF_Event& left, const SMF_Event& right) {
        if (left.timestamp == right.timestamp)
        {
            return left.seq_id < right.seq_id;
        }
        return left.timestamp < right.timestamp;
    });
    SMF_SetDeltasFromTimestamps(merged_track);
    return merged_track;
}

inline bool SMF_IsStatusByte(uint8_t byte)
{
    return (byte & 0x80) != 0;
}

bool SMF_ReadTrack(SMF_Reader& reader, SMF_Data& result, uint64_t expected_end)
{
    uint8_t running_status = 0;
    uint64_t total_time = 0;

    result.tracks.emplace_back();
    SMF_Track& new_track = result.tracks.back();

    while (reader.GetOffset() < expected_end)
    {
        uint32_t delta_time;
        CHECK(SMF_ReadVarint(reader, delta_time));

        uint8_t event_head;
        CHECK(reader.ReadU8(event_head));

        if (SMF_IsStatusByte(event_head))
        {
            running_status = event_head;
        }
        else
        {
            // Put back, this is actually a data byte. No need to check if this
            // op is valid because we only got here if we already read a status
            // byte.
            (void)reader.PutBack();
        }

        total_time += delta_time;

        new_track.events.emplace_back();
        SMF_Event& new_event = new_track.events.back();
        new_event.seq_id = new_track.events.size();
        new_event.delta_time = delta_time;
        new_event.timestamp = total_time;
        new_event.status = running_status;

        switch (new_event.status & 0xF0)
        {
            // 2 param
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
            case 0xE0:
                new_event.data_first = reader.GetOffset();
                CHECK(reader.Skip(2));
                new_event.data_last = reader.GetOffset();
                break;
            // 1 param
            case 0xC0:
            case 0xD0:
                new_event.data_first = reader.GetOffset();
                CHECK(reader.Skip(1));
                new_event.data_last = reader.GetOffset();
                break;
            // variable length
            case 0xF0:
                {
                    if (new_event.status == 0xF0 || new_event.status == 0xF7)
                    {
                        // Sysex events
                        uint32_t sysex_len;
                        CHECK(SMF_ReadVarint(reader, sysex_len));
                        new_event.data_first = reader.GetOffset();
                        CHECK(reader.Skip(sysex_len));
                        new_event.data_last = reader.GetOffset();
                    }
                    else if (new_event.status == 0xFF)
                    {
                        // Meta events
                        uint32_t meta_len;
                        new_event.data_first = reader.GetOffset();
                        // Skip type byte
                        CHECK(reader.Skip(1));
                        CHECK(SMF_ReadVarint(reader, meta_len));
                        CHECK(reader.Skip(meta_len));
                        new_event.data_last = reader.GetOffset();
                    }
                    else
                    {
                        fprintf(stderr, "Panic: unhandled Fx message: %x\n", new_event.status);
                        exit(1);
                    }
                }
                break;
        }
    }

    if (reader.GetOffset() > expected_end)
    {
        fprintf(stderr, "Read past expected track end\n");
        return false;
    }

    return true;
}

void SMF_PrintStats(const SMF_Data& data)
{
    for (size_t i = 0; i < data.tracks.size(); ++i)
    {
        fprintf(stderr, "Track %02" PRIu64 ": %" PRIu64 " events\n", i, data.tracks[i].events.size());
    }
}

bool SMF_ReadAllBytes(const std::filesystem::path& filename, std::vector<uint8_t>& buffer)
{
    std::ifstream input(filename, std::ios::binary);

    if (!input)
    {
        return false;
    }

    input.seekg(0, std::ios::end);
    std::streamoff byte_count = input.tellg();
    input.seekg(0, std::ios::beg);

    buffer.resize(RangeCast<size_t>(byte_count));

    input.read((char*)buffer.data(), RangeCast<std::streamsize>(byte_count));

    return input.good();
}

bool SMF_ReadChunk(SMF_Reader& reader, SMF_Data& data)
{
    uint64_t chunk_start = reader.GetOffset();

    uint8_t chunk_type[4];
    CHECK(reader.ReadBytes(chunk_type, 4));

    uint32_t chunk_size = 0;
    CHECK(reader.ReadU32BE(chunk_size));

    uint64_t chunk_end = reader.GetOffset() + chunk_size;

    if (memcmp(chunk_type, "MThd", 4) == 0)
    {
        SMF_ReadHeader(reader, data.header);
    }
    else if (memcmp(chunk_type, "MTrk", 4) == 0)
    {
        SMF_ReadTrack(reader, data, chunk_end);
    }
    else
    {
        fprintf(stderr, "Unexpected chunk type at %" PRIu64 "\n", chunk_start);
        return false;
    }

    return true;
}

SMF_Data SMF_LoadEvents(const char* filename)
{
    return SMF_LoadEvents(std::filesystem::path(filename));
}

SMF_Data SMF_LoadEvents(const std::filesystem::path& filename)
{
    SMF_Data data;

    CHECK(SMF_ReadAllBytes(filename, data.bytes));

    SMF_Reader reader(data.bytes);

    while (!reader.AtEnd())
    {
        CHECK(SMF_ReadChunk(reader, data));
    }

    return data;
}

