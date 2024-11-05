#include "emu.h"
#include "smf.h"
#include "wav.h"
#include "path_util.h"
#include "command_line.h"
#include "audio.h"
#include "cast.h"
#include "math_util.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <cstdio>
#include <cinttypes>
#include <source_location>
#include <thread>
#include <condition_variable>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

using namespace std::chrono_literals;

enum class R_EndBehavior
{
    // Cut the track at the last MIDI event.
    Cut,
    // Keep rendering until the emulator produces silence.
    Release,
};

struct R_Parameters
{
    std::string_view input_filename;
    std::string_view output_filename;
    bool help = false;
    size_t instances = 1;
    EMU_SystemReset reset = EMU_SystemReset::NONE;
    std::filesystem::path rom_directory;
    AudioFormat output_format = AudioFormat::S16;
    bool output_stdout = false;
    bool disable_oversampling = false;
    std::string_view romset_name;
    bool debug = false;
    R_EndBehavior end_behavior = R_EndBehavior::Cut;
};

enum class R_ParseError
{
    Success,
    NoInput,
    NoOutput,
    MultipleInputs,
    InstancesInvalid,
    InstancesOutOfRange,
    UnexpectedEnd,
    RomDirectoryNotFound,
    FormatInvalid,
    EndInvalid,
};

const char* R_ParseErrorStr(R_ParseError err)
{
    switch (err)
    {
        case R_ParseError::Success:
            return "Success";
        case R_ParseError::NoInput:
            return "No input file specified";
        case R_ParseError::NoOutput:
            return "No output file specified (pass -o)";
        case R_ParseError::MultipleInputs:
            return "Multiple input files";
        case R_ParseError::InstancesInvalid:
            return "Instances couldn't be parsed (should be 1-16)";
        case R_ParseError::InstancesOutOfRange:
            return "Instances out of range (should be 1-16)";
        case R_ParseError::UnexpectedEnd:
            return "Expected another argument";
        case R_ParseError::RomDirectoryNotFound:
            return "Rom directory doesn't exist";
        case R_ParseError::FormatInvalid:
            return "Output format invalid";
        case R_ParseError::EndInvalid:
            return "End behavior invalid";
    }
    return "Unknown error";
}

R_ParseError R_ParseCommandLine(int argc, char* argv[], R_Parameters& result)
{
    CommandLineReader reader(argc, argv);

    while (reader.Next())
    {
        if (reader.Any("-o"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            result.output_filename = reader.Arg();
        }
        else if (reader.Any("-h", "--help", "-?"))
        {
            result.help = true;
            return R_ParseError::Success;
        }
        else if (reader.Any("--debug"))
        {
            result.debug = true;
        }
        else if (reader.Any("-n", "--instances"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            if (!reader.TryParse(result.instances))
            {
                return R_ParseError::InstancesInvalid;
            }

            if (result.instances < 1 || result.instances > 16)
            {
                return R_ParseError::InstancesOutOfRange;
            }
        }
        else if (reader.Any("-r", "--reset"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            if (reader.Arg() == "gm")
            {
                result.reset = EMU_SystemReset::GM_RESET;
            }
            else if (reader.Arg() == "gs")
            {
                result.reset = EMU_SystemReset::GS_RESET;
            }
            else
            {
                result.reset = EMU_SystemReset::NONE;
            }
        }
        else if (reader.Any("-d", "--rom-directory"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            result.rom_directory = reader.Arg();
            if (!std::filesystem::exists(result.rom_directory))
            {
                return R_ParseError::RomDirectoryNotFound;
            }
        }
        else if (reader.Any("-d", "--romset"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            result.romset_name = reader.Arg();
        }
        else if (reader.Any("-f", "--format"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            if (reader.Arg() == "s16")
            {
                result.output_format = AudioFormat::S16;
            }
            else if (reader.Arg() == "s32")
            {
                result.output_format = AudioFormat::S32;
            }
            else if (reader.Arg() == "f32")
            {
                result.output_format = AudioFormat::F32;
            }
            else
            {
                return R_ParseError::FormatInvalid;
            }
        }
        else if (reader.Any("--stdout"))
        {
            result.output_stdout = true;
        }
        else if (reader.Any("--disable-oversampling"))
        {
            result.disable_oversampling = true;
        }
        else if (reader.Any("--end"))
        {
            if (!reader.Next())
            {
                return R_ParseError::UnexpectedEnd;
            }

            if (reader.Arg() == "cut")
            {
                result.end_behavior = R_EndBehavior::Cut;
            }
            else if (reader.Arg() == "release")
            {
                result.end_behavior = R_EndBehavior::Release;
            }
            else
            {
                return R_ParseError::EndInvalid;
            }
        }
        else
        {
            if (result.input_filename.size())
            {
                return R_ParseError::MultipleInputs;
            }
            result.input_filename = reader.Arg();
        }
    }

    if (result.input_filename.size() == 0)
    {
        return R_ParseError::NoInput;
    }

    if (result.output_filename.size() == 0 && !result.output_stdout)
    {
        return R_ParseError::NoOutput;
    }

    return R_ParseError::Success;
}

void R_Panic(const char* msg, const std::source_location where = std::source_location::current())
{
    fprintf(stderr, "%s:%d: in %s: %s", where.file_name(), (int)where.line(), where.function_name(), msg);
    exit(1);
}

// Audio frame chunk. Points to a header followed by a dynamically sized buffer containing audio data. The buffer
// contains audio data. This type has reference semantics and represents unowned memory like a bare pointer, so take
// care making copies of it.
class R_FrameChunk
{
public:
    [[nodiscard]]
    static R_FrameChunk Alloc(size_t size_bytes)
    {
        R_FrameChunk c;

        // TODO: use a better allocator. We allocate a bit more space because malloc has weak alignment guarantees, and
        // we want the buffer to be 64-byte aligned for max SIMD compatibility.
        size_t alloc_size = 64 + sizeof(Header) + size_bytes;

        void* ptr = malloc(alloc_size);
        if (!ptr)
        {
            return c;
        }

        // This should never fail or adjust ptr so passing the aligned pointer back to free should be ok.
        if (!std::align(alignof(Header), sizeof(Header), ptr, alloc_size))
        {
            R_Panic("failed to align header");
        }

        Header* h = new (ptr) Header();
        ptr = (uint8_t*)ptr + sizeof(Header);

        if (!std::align(64, size_bytes, ptr, alloc_size))
        {
            R_Panic("failed to align buffer");
        }

        h->buffer      = ptr;
        c.m_alloc      = h;
        c.m_alloc->cap = size_bytes;

        return c;
    }

    static void Free(R_FrameChunk c)
    {
        free(c.m_alloc);
        c.m_alloc = nullptr;
    }

    [[nodiscard]]
    size_t GetBufferLength() const
    {
        return m_alloc->len;
    }

    [[nodiscard]]
    void* DataFirst()
    {
        return std::assume_aligned<64>(m_alloc->buffer);
    }

    [[nodiscard]]
    void* DataLast()
    {
        // We cannot assume the end is 64-byte aligned because audio frames may be much smaller.
        return (uint8_t*)DataFirst() + m_alloc->len;
    }

    void Write(const void* src, size_t src_len)
    {
        memcpy(DataLast(), src, src_len);
        m_alloc->len += src_len;
    }

    [[nodiscard]]
    bool IsNull() const
    {
        return m_alloc == nullptr;
    }

    void SetNext(R_FrameChunk c)
    {
        m_alloc->next = c.m_alloc;
    }

    [[nodiscard]]
    R_FrameChunk GetNext()
    {
        R_FrameChunk next;
        next.m_alloc = m_alloc->next;
        return next;
    }

    [[nodiscard]]
    bool IsBufferFull() const
    {
        return m_alloc->len == m_alloc->cap;
    }

private:
    struct Header
    {
        Header* next = nullptr;
        size_t  len  = 0;
        size_t  cap  = 0;
        void*   buffer;
    };

private:
    Header* m_alloc = nullptr;
};

// Manages a chunk and frees it when it goes out of scope, similar to unique_ptr. Code outside of the chunk queue should
// only use this type.
class R_OwnedChunk
{
public:
    R_OwnedChunk() = default;

    explicit R_OwnedChunk(R_FrameChunk c)
        : m_chunk(c)
    {
    }

    ~R_OwnedChunk()
    {
        Free();
    }

    // Non-copyable
    R_OwnedChunk(const R_OwnedChunk&)            = delete;
    R_OwnedChunk& operator=(const R_OwnedChunk&) = delete;

    // Moveable
    R_OwnedChunk(R_OwnedChunk&& rhs)
    {
        m_chunk = rhs.Unmanage();
    }

    R_OwnedChunk& operator=(R_OwnedChunk&& rhs)
    {
        Free();
        m_chunk = rhs.Unmanage();
        return *this;
    }

    void Free()
    {
        R_FrameChunk::Free(m_chunk);
        m_chunk = R_FrameChunk();
    }

    void Manage(R_FrameChunk rhs)
    {
        Free();
        m_chunk = rhs;
    }

    [[nodiscard]]
    R_FrameChunk Unmanage()
    {
        R_FrameChunk ptr = m_chunk;
        m_chunk          = R_FrameChunk();
        return ptr;
    }

    void Write(const void* src, size_t src_len)
    {
        m_chunk.Write(src, src_len);
    }

    [[nodiscard]]
    bool IsBufferFull() const
    {
        return m_chunk.IsBufferFull();
    }

    [[nodiscard]]
    void* DataFirst()
    {
        return m_chunk.DataFirst();
    }

    [[nodiscard]]
    void* DataLast()
    {
        return m_chunk.DataLast();
    }

    [[nodiscard]]
    bool IsNull() const
    {
        return m_chunk.IsNull();
    }

    [[nodiscard]]
    size_t GetBufferLength() const
    {
        return m_chunk.GetBufferLength();
    }

private:
    R_FrameChunk m_chunk;
};

// Threadsafe queue for chunks of audio. The intent is that emulators should be able to expand the queue as fast as
// possible while it may drain at a different rate. Contention is kept low by creating chunks of data and enqueuing or
// dequeuing chunks at a time. For a buffer size of ~64k, expect each thread to take a lock once per second.
class R_ChunkQueue
{
public:
    R_ChunkQueue() = default;

    void Enqueue(R_OwnedChunk chunk)
    {
        std::scoped_lock lk(m_mutex);

        R_FrameChunk raw = chunk.Unmanage();
        if (!m_last.IsNull())
        {
            m_last.SetNext(raw);
            m_last = raw;
        }
        else
        {
            m_first = raw;
            m_last  = raw;
        }
        ++m_count;
    }

    void Dequeue(R_OwnedChunk& chunk)
    {
        std::scoped_lock lk(m_mutex);

        if (!m_first.IsNull())
        {
            chunk.Manage(m_first);
            m_first = m_first.GetNext();
            if (m_first.IsNull())
            {
                m_last = m_first;
            }
        }
        else
        {
            R_Panic("empty queue");
        }
        --m_count;
    }

    size_t ChunkCount() const
    {
        return m_count;
    }

private:
    std::mutex          m_mutex;
    R_FrameChunk        m_first;
    R_FrameChunk        m_last;
    std::atomic<size_t> m_count = 0;
};

class R_Mixer
{
public:
    // Blocks the calling thread until there's enough data in queues to mix.
    void WaitForWork()
    {
        std::unique_lock lk(m_mutex);
        m_cond.wait(lk, [this]() { return GetReadyChunkCount() > 0; });
    }

    // Returns chunk size in frame count.
    size_t GetChunkSize() const
    {
        return m_chunk_size;
    }

    // Sets number of queues and prepares a chunk builder for each.
    // precondition: 0 <= count <= QUEUE_COUNT
    template <typename T>
    void SetQueueCount(size_t count)
    {
        m_queues_in_use = count;
        for (size_t i = 0; i < count; ++i)
        {
            m_chunks[i] = AllocChunk<T>();
        }
    }

    // Writes a frame to the chunk currently being built for queue_id. If the chunk becomes full, it is moved into its
    // queue and a new chunk becomes available.
    template <typename T>
    void SubmitFrame(size_t queue_id, const AudioFrame<T>& frame)
    {
        m_chunks[queue_id].Write(&frame, sizeof(frame));
        if (m_chunks[queue_id].IsBufferFull())
        {
            m_queues[queue_id].Enqueue(std::move(m_chunks[queue_id]));
            m_cond.notify_one();
            m_chunks[queue_id] = AllocChunk<T>();
        }
    }

    // Enqueues whatever data is left in the chunk builder for queue_id and marks it as complete. After this call, no
    // more data may be submitted to queue_id.
    void MarkComplete(size_t queue_id)
    {
        m_queue_complete[queue_id] = true;
        m_queues[queue_id].Enqueue(std::move(m_chunks[queue_id]));
        m_cond.notify_one();
    }

    // Returns the number N of chunks that can be dequeued from each queue to call MixFrames N times.
    size_t GetReadyChunkCount()
    {
        size_t count = (size_t)-1;
        for (size_t i = 0; i < m_queues_in_use; ++i)
        {
            // It seems possible to end up in a rare state where e.g. at the end of the midi we have two queues marked
            // as complete but only one has data. Normally this shouldn't happen because the emulators should be stepped
            // for roughly the same amount of time and produce roughly the same number of samples. I have not
            // encountered this problem yet, but multi instance rendering tends to desync slightly. On some midi files,
            // one emulator steps for longer than the other despite the two sharing end-of-track messages with the same
            // SMF tick timestamp. I can only guess this is caused by the integer division converting SMF ticks to
            // microseconds. Until I can find a good test midi or construct one I'm not sure how to solve this, or if
            // it's even a real problem. However, I have observed 400us/28 frame differences.
            //
            // To try to deal with this, we do not consider queues marked as complete and having zero chunks, as they
            // will never receive new data. However, if another queue is incomplete and has zero chunks, the emulator
            // responsible for filling that queue will enqueue one eventually. In that case, MixFrames should still mix
            // samples from that queue without waiting for the complete queue.

            size_t cc = m_queues[i].ChunkCount();
            if (!(m_queue_complete[i] && cc == 0))
            {
                count = Min(count, cc);
            }
        }
        return count;
    }

    // Dequeues a chunk from each queue and mixes the corresponding audio frames from each chunk into a single buffer.
    // precondition: GetReadyChunkCount() > 0
    template <typename T, typename MixFn>
    size_t MixFrames(std::vector<AudioFrame<T>>& output_buffer, MixFn mix)
    {
        output_buffer.clear();

        R_OwnedChunk chunks[QUEUE_COUNT];

        // precalcluate the output buffer size so that we don't need to bounds check or reallocate in the mix loop
        size_t size_requested = 0;

        for (size_t queue_id = 0; queue_id < m_queues_in_use; ++queue_id)
        {
            size_t cc = m_queues[queue_id].ChunkCount();
            if (m_queue_complete[queue_id] && cc == 0)
            {
                // See comment in GetReadyChunkCount.
                fprintf(stderr,
                        "R_Mixer::MixFrames tried to mix a queue that is complete with no data.\n"
                        "Please submit a bug report and attach the midi file that caused this:\n"
                        "\n"
                        "    https://github.com/jcmoyer/Nuked-SC55");
                DebugPrintQueues();
                continue;
            }
            m_queues[queue_id].Dequeue(chunks[queue_id]);
            size_requested = std::max(size_requested, chunks[queue_id].GetBufferLength());
        }

        output_buffer.resize(size_requested / sizeof(AudioFrame<T>));
        for (size_t queue_id = 0; queue_id < m_queues_in_use; ++queue_id)
        {
            if (chunks[queue_id].IsNull())
            {
                // Attempt to deal with errors from the prior loop
                continue;
            }
            mix(output_buffer.data(), chunks[queue_id].DataFirst(), chunks[queue_id].DataLast());
        }

        return size_requested / sizeof(AudioFrame<T>);
    }

    // Returns true when all queues are marked as complete and are empty.
    bool IsFinished() const
    {
        for (size_t i = 0; i < m_queues_in_use; ++i)
        {
            if (!m_queue_complete[i] || m_queues[i].ChunkCount() != 0)
            {
                return false;
            }
        }

        return true;
    }

private:
    template <typename T>
    R_OwnedChunk AllocChunk()
    {
        R_FrameChunk raw = R_FrameChunk::Alloc(m_chunk_size * sizeof(AudioFrame<T>));
        return R_OwnedChunk(raw);
    }

    void DebugPrintQueues()
    {
        for (size_t i = 0; i < m_queues_in_use; ++i)
        {
            fprintf(stderr, "Queue %" PRIu64 " has %" PRIu64 " chunks\n", i, m_queues[i].ChunkCount());
        }
    }

private:
    // a bit less than 1 second of audio
    static constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024;
    // one queue per emulator
    static constexpr size_t QUEUE_COUNT = 16;

    R_ChunkQueue m_queues[QUEUE_COUNT];
    R_OwnedChunk m_chunks[QUEUE_COUNT];
    bool         m_queue_complete[QUEUE_COUNT]{};

    size_t m_queues_in_use = 0;

    // Size of chunks in bytes.
    size_t m_chunk_size = DEFAULT_CHUNK_SIZE;

    // Synchronization between producers/consumer.
    std::mutex              m_mutex;
    std::condition_variable m_cond;
};

struct R_TrackRenderState
{
    Emulator emu;
    R_Mixer* mixer = nullptr;
    size_t queue_id = 0;
    size_t ns_simulated = 0;
    const SMF_Track* track = nullptr;
    std::thread thread;
    std::chrono::high_resolution_clock::duration elapsed;
    size_t num_silent_frames = 0;
    R_EndBehavior end_behavior;

    // these fields are accessed from main thread during render process
    std::atomic<size_t> events_processed = 0;
    std::atomic<bool> done;
};

bool R_IsSilence(const AudioFrame<int32_t>& in_raw)
{
    // The emulator doesn't produce exact zeroes when no notes are playing.
    constexpr int32_t MIN = -0x4000;
    constexpr int32_t MAX = +0x4000;
    return MIN <= in_raw.left && in_raw.left <= MAX && MIN <= in_raw.right && in_raw.right <= MAX;
}

template <typename SampleT>
void R_ReceiveSample(void* userdata, const AudioFrame<int32_t>& in)
{
    R_TrackRenderState* state = (R_TrackRenderState*)userdata;

    AudioFrame<SampleT> out;
    Normalize(in, out);

    if (R_IsSilence(in))
    {
        ++state->num_silent_frames;
    }
    else
    {
        state->num_silent_frames = 0;
    }

    state->mixer->SubmitFrame(state->queue_id, out);
}

void R_RunReset(Emulator& emu, EMU_SystemReset reset)
{
    if (reset == EMU_SystemReset::NONE)
    {
        return;
    }

    emu.PostSystemReset(reset);

    for (size_t i = 0; i < 24'000'000; ++i)
    {
        MCU_Step(emu.GetMCU());
    }
}

void R_PostEvent(Emulator& emu, const SMF_Data& data, const SMF_Event& ev)
{
    emu.PostMIDI(ev.status);
    emu.PostMIDI(ev.GetData(data.bytes));
}

struct R_TrackList
{
    std::vector<SMF_Track> tracks;
};

// Splits a track into `n` tracks, each track can be processed by a single
// emulator instance.
R_TrackList R_SplitTrackModulo(const SMF_Track& merged_track, size_t n)
{
    R_TrackList result;
    result.tracks.resize(n);

    for (auto& event : merged_track.events)
    {
        // System events need to be processed by all emulators
        if (event.IsSystem())
        {
            for (auto& dest : result.tracks)
            {
                dest.events.emplace_back(event);
            }
        }
        else
        {
            auto& dest = result.tracks[event.GetChannel() % n];
            dest.events.emplace_back(event);
        }
    }

    for (auto& track : result.tracks)
    {
        SMF_SetDeltasFromTimestamps(track);
    }

    return result;
}

uint64_t R_NSPerStep(Emulator& emu)
{
    // These are best guesses.
    if (emu.GetMCU().is_mk1)
    {
        return 600;
    }
    else
    {
        return 500;
    }
}

void R_RenderOne(const SMF_Data& data, R_TrackRenderState& state)
{
    uint64_t division = data.header.division;
    uint64_t us_per_qn = 500000;

    const SMF_Track& track = (const SMF_Track&)*state.track;

    const uint64_t ns_per_step = R_NSPerStep(state.emu);

    auto t_start = std::chrono::high_resolution_clock::now();
    for (const SMF_Event& event : track.events)
    {
        const uint64_t this_event_time_ns =
            state.ns_simulated + 1000 * SMF_TicksToUS(event.delta_time, us_per_qn, division);

        while (state.ns_simulated < this_event_time_ns)
        {
            MCU_Step(state.emu.GetMCU());
            state.ns_simulated += ns_per_step;
        }

        if (event.IsTempo(data.bytes))
        {
            us_per_qn = event.GetTempoUS(data.bytes);
        }

        // Fire the event.
        if (!event.IsMetaEvent())
        {
            R_PostEvent(state.emu, data, event);
        }

        ++state.events_processed;
    }

    if (state.end_behavior == R_EndBehavior::Release)
    {
        const uint32_t frequency = PCM_GetOutputFrequency(state.emu.GetPCM());
        // TODO: make this configurable? do we care? currently 100ms
        const size_t silence_time = frequency / 10;
        while (state.num_silent_frames < silence_time)
        {
            MCU_Step(state.emu.GetMCU());
        }
    }
    state.elapsed = std::chrono::high_resolution_clock::now() - t_start;

    state.mixer->MarkComplete(state.queue_id);

    state.done = true;
}

void R_CursorUpLines(int n)
{
    fprintf(stderr, "\x1b[%dF", n);
}

struct R_MixOutState
{
    R_Mixer* mixer = nullptr;

    // Written by mix thread, read by main thread
    std::atomic<size_t> frames_mixed = 0;

    std::vector<AudioFrame<int16_t>> mix_buffer;

    // Eventually we need to abstract over this to stream to other outputs.
    WAV_Handle* output = nullptr;
};

void R_Mix(int16_t* dest, int16_t* src_first, int16_t* src_last)
{
    // TODO more correct to clip *after* summation? we need an intermediate buffer of a larger type?
    HorizontalSatAddI16(dest, src_first, src_last);
}

void R_Mix(int32_t* dest, int32_t* src_first, int32_t* src_last)
{
    HorizontalSatAddI32(dest, src_first, src_last);
}

void R_Mix(float* dest, float* src_first, float* src_last)
{
    HorizontalAddF32(dest, src_first, src_last);
}

template <typename T>
void R_MixOut(R_MixOutState& state)
{
    state.mix_buffer.reserve(state.mixer->GetChunkSize());

    while (!state.mixer->IsFinished())
    {
        state.mixer->WaitForWork();

        state.frames_mixed += state.mixer->MixFrames(state.mix_buffer, [](void* dest, void* src_first, void* src_last) {
            R_Mix((T*)dest, (T*)src_first, (T*)src_last);
        });

        for (auto& frame : state.mix_buffer)
        {
            state.output->Write(frame);
        }
    }

    state.output->Finish();
}

bool R_RenderTrack(const SMF_Data& data, const R_Parameters& params)
{
    const size_t instances = params.instances;
    auto t_start = std::chrono::high_resolution_clock::now();

    // First combine all of the events so it's easier to process
    const SMF_Track merged_track = SMF_MergeTracks(data);
    // Then create a track specifically for each emulator instance
    const R_TrackList split_tracks = R_SplitTrackModulo(merged_track, instances);

    Romset rs;
    if (params.romset_name.size())
    {
        if (!EMU_ParseRomsetName(params.romset_name, rs))
        {
            // interpreting romset_name as a char pointer here is safe because it points into argv
            fprintf(stderr, "Could not parse romset name: `%s`\n", params.romset_name.data());
            return false;
        }
        fprintf(stderr, "Using romset: %s\n", EMU_RomsetName(rs));
    }
    else
    {
        rs = EMU_DetectRomset(params.rom_directory);
        fprintf(stderr, "Detected romset: %s\n", EMU_RomsetName(rs));
    }

    R_Mixer mixer;
    switch (params.output_format)
    {
    case AudioFormat::S16:
        mixer.SetQueueCount<int16_t>(instances);
        break;
    case AudioFormat::S32:
        mixer.SetQueueCount<int32_t>(instances);
        break;
    case AudioFormat::F32:
        mixer.SetQueueCount<float>(instances);
        break;
    }

    R_TrackRenderState render_states[SMF_CHANNEL_COUNT];
    for (size_t i = 0; i < instances; ++i)
    {
        render_states[i].emu.Init(EMU_Options {
            .enable_lcd = false,
        });

        if (!render_states[i].emu.LoadRoms(rs, params.rom_directory))
        {
            return false;
        }

        render_states[i].emu.Reset();
        render_states[i].emu.GetPCM().disable_oversampling = params.disable_oversampling;

        fprintf(stderr, "Running system reset for #%02" PRIu64 "...\n", i);
        R_RunReset(render_states[i].emu, params.reset);

        switch (params.output_format)
        {
        case AudioFormat::S16:
            render_states[i].emu.SetSampleCallback(R_ReceiveSample<int16_t>, &render_states[i]);
            break;
        case AudioFormat::S32:
            render_states[i].emu.SetSampleCallback(R_ReceiveSample<int32_t>, &render_states[i]);
            break;
        case AudioFormat::F32:
            render_states[i].emu.SetSampleCallback(R_ReceiveSample<float>, &render_states[i]);
            break;
        }

        render_states[i].track = &split_tracks.tracks[i];
        render_states[i].mixer = &mixer;
        render_states[i].queue_id = i;
        render_states[i].end_behavior = params.end_behavior;

        render_states[i].thread = std::thread(R_RenderOne, std::cref(data), std::ref(render_states[i]));
    }

    WAV_Handle render_output;
    if (params.output_stdout)
    {
#ifdef _WIN32
        // On Windows, stdout is opened in text mode, which causes newline translation to occur.
        _setmode(_fileno(stdout), O_BINARY);
#endif
        render_output.OpenStdout(params.output_format);
    }
    else
    {
        render_output.Open(params.output_filename, params.output_format);
    }
    render_output.SetSampleRate(PCM_GetOutputFrequency(render_states[0].emu.GetPCM()));

    R_MixOutState mix_out_state;
    mix_out_state.mixer = &mixer;
    mix_out_state.output = &render_output;
    std::thread mix_out_thread;

    switch (params.output_format)
    {
    case AudioFormat::S16:
        mix_out_thread = std::thread(R_MixOut<int16_t>, std::ref(mix_out_state));
        break;
    case AudioFormat::S32:
        mix_out_thread = std::thread(R_MixOut<int32_t>, std::ref(mix_out_state));
        break;
    case AudioFormat::F32:
        mix_out_thread = std::thread(R_MixOut<float>, std::ref(mix_out_state));
        break;
    }

    // Now we wait.
    bool all_done = false;
    while (!all_done)
    {
        all_done = true;

        fprintf(stderr, "Rendered %" PRIu64 " frames\n", mix_out_state.frames_mixed.load());

        for (size_t i = 0; i < instances; ++i)
        {
            if (!render_states[i].done)
            {
                all_done = false;
            }

            const size_t processed    = render_states[i].events_processed;
            const size_t total        = render_states[i].track->events.size();
            const float  percent_done = 100.f * (float)processed / (float)total;

            fprintf(stderr, "#%02" PRIu64 " %6.2f%% [%" PRIu64 " / %" PRIu64 "]\n", i, percent_done, processed, total);
        }

        if (!all_done)
        {
            R_CursorUpLines(RangeCast<int>(1 + instances));
        }

        std::this_thread::sleep_for(1000ms);
    }

    for (size_t i = 0; i < instances; ++i)
    {
        render_states[i].thread.join();
    }

    mix_out_thread.join();

    if (params.debug)
    {
        for (size_t i = 0; i < instances; ++i)
        {
            auto t_instance_sec = (double)render_states[i].elapsed.count() / 1e9;
            fprintf(stderr, "#%02" PRIu64 " took %.2fs\n", i, t_instance_sec);
        }
    }

    auto t_finish = std::chrono::high_resolution_clock::now();
    auto t_diff   = std::chrono::duration_cast<std::chrono::nanoseconds>(t_finish - t_start);
    auto t_sec    = (double)t_diff.count() / 1e9;

    fprintf(stderr, "Done in %.2fs!\n", t_sec);

    return true;
}

void R_Usage()
{
    constexpr const char* USAGE_STR = R"(Renders a standard MIDI file to a WAVE file using nuked-sc55.

Usage: %s [options] -o <output> <input>

General options:
  -? -h, --help                Display this information.
  -o <filename>                Render WAVE file to filename.
  --stdout                     Render raw sample data to stdout. No header

Audio options:
  -f, --format s16|s32|f32     Set output format.
  --disable-oversampling       Halves output frequency.
  --end cut|release            Choose how the end of the track is handled:
        cut (default)              Stop rendering at the last MIDI event
        release                    Continue to render audio after the last MIDI event until silence

Emulator options:
  -r, --reset     gs|gm        Send GS or GM reset before rendering.
  -n, --instances <count>      Number of emulators to use (increases effective polyphony, but
                               takes longer to render)

ROM management options:
  -d, --rom-directory <dir>    Sets the directory to load roms from. Romset will be autodetected when
                               not also passing --romset.
  --romset <name>              Sets the romset to load.
)";

    std::string name = P_GetProcessPath().stem().generic_string();
    fprintf(stderr, USAGE_STR, name.c_str());
}

int main(int argc, char* argv[])
{
    R_Parameters params;
    R_ParseError result = R_ParseCommandLine(argc, argv, params);

    if (result != R_ParseError::Success)
    {
        fprintf(stderr, "error: %s\n", R_ParseErrorStr(result));
        R_Usage();
        return 1;
    }

    if (params.help)
    {
        R_Usage();
        return 0;
    }

    SMF_Data data;
    data = SMF_LoadEvents(params.input_filename);

    if (!R_RenderTrack(data, params))
    {
        fprintf(stderr, "Failed to render track\n");
        return 1;
    }

    return 0;
}
