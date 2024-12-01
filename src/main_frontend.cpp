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
#include "emu.h"
#include "midi.h"
#include "ringbuffer.h"
#include "path_util.h"
#include "command_line.h"
#include "audio.h"
#include "cast.h"
#include "pcm.h"
#include "serial.h"
#include <SDL.h>
#include <cinttypes>
#include <optional>

#ifdef _WIN32
#include <Windows.h>
#endif

// Workaround until all compilers implement CWG 2518 (at time of writing, MSVC
// doesn't accept a static_assert(false) in a constexpr conditional branch)
template <typename>
constexpr bool DependentFalse = false;

struct FE_Instance
{
    Emulator emu;

    GenericBuffer sample_buffer;

    RingbufferView<AudioFrame<int16_t>> view_s16;
    RingbufferView<AudioFrame<int32_t>> view_s32;
    RingbufferView<AudioFrame<float>>   view_f32;

    std::thread thread;
    bool        running;
    AudioFormat format;

    // Statically selects the correct ringbuffer field into based on SampleT.
    template <typename SampleT>
    RingbufferView<AudioFrame<SampleT>>& StaticSelectBuffer()
    {
        if constexpr (std::is_same_v<SampleT, int16_t>)
        {
            return view_s16;
        }
        else if constexpr (std::is_same_v<SampleT, int32_t>)
        {
            return view_s32;
        }
        else if constexpr (std::is_same_v<SampleT, float>)
        {
            return view_f32;
        }
        else
        {
            static_assert(DependentFalse<SampleT>, "No valid case for SampleT");
        }
    }
};

const size_t FE_MAX_INSTANCES = 16;

struct FE_Application {
    FE_Instance instances[FE_MAX_INSTANCES];
    size_t instances_in_use = 0;

    uint32_t audio_buffer_size = 0;
    uint32_t audio_page_size = 0;

    SDL_AudioDeviceID sdl_audio = 0;

    bool running = false;
};

struct FE_Parameters
{
    bool help = false;
    std::string midiin_device;
    std::string midiout_device;
    std::string serial_type;
    std::string serial_port;
    std::string audio_device;
    uint32_t page_size = 512;
    uint32_t page_num = 32;
    bool autodetect = true;
    EMU_SystemReset reset = EMU_SystemReset::NONE;
    size_t instances = 1;
    Romset romset = Romset::MK2;
    std::optional<std::filesystem::path> rom_directory;
    AudioFormat output_format = AudioFormat::S16;
    bool no_lcd = false;
    bool disable_oversampling = false;
};

bool FE_AllocateInstance(FE_Application& container, FE_Instance** result)
{
    if (container.instances_in_use == FE_MAX_INSTANCES)
    {
        return false;
    }

    FE_Instance& fe = container.instances[container.instances_in_use];
    ++container.instances_in_use;

    if (result)
    {
        *result = &fe;
    }

    return true;
}

void FE_SendMIDI(FE_Application& fe, size_t n, std::span<const uint8_t> bytes)
{
    fe.instances[n].emu.PostMIDI(bytes);
}

void FE_BroadcastMIDI(FE_Application& fe, std::span<const uint8_t> bytes)
{
    for (size_t i = 0; i < fe.instances_in_use; ++i)
    {
        FE_SendMIDI(fe, i, bytes);
    }
}

void FE_RouteMIDI(FE_Application& fe, std::span<const uint8_t> bytes)
{
    if (bytes.size() == 0)
    {
        return;
    }

    uint8_t first = bytes[0];

    if (first < 0x80)
    {
        fprintf(stderr, "FE_RouteMIDI received data byte %02x\n", first);
        return;
    }

    const bool is_sysex = first == 0xF0;
    const uint8_t channel = first & 0x0F;

    if (is_sysex)
    {
        FE_BroadcastMIDI(fe, bytes);
    }
    else
    {
        FE_SendMIDI(fe, channel % fe.instances_in_use, bytes);
    }
}

template <typename SampleT>
void FE_ReceiveSample(void* userdata, const AudioFrame<int32_t>& in)
{
    FE_Instance& fe = *(FE_Instance*)userdata;

    AudioFrame<SampleT> out;
    Normalize(in, out);

    fe.StaticSelectBuffer<SampleT>().UncheckedWriteOne(out);
}

template <typename SampleT>
void FE_AudioCallback(void* userdata, Uint8* stream, int len)
{
    FE_Application& frontend = *(FE_Application*)userdata;

    const size_t num_frames = (size_t)len / sizeof(AudioFrame<SampleT>);
    memset(stream, 0, (size_t)len);

    size_t renderable_count = num_frames;
    for (size_t i = 0; i < frontend.instances_in_use; ++i)
    {
        renderable_count = Min(
            renderable_count,
            frontend.instances[i].StaticSelectBuffer<SampleT>().GetReadableCount()
        );
    }

    for (size_t i = 0; i < frontend.instances_in_use; ++i)
    {
        ReadMix<SampleT>(frontend.instances[i].StaticSelectBuffer<SampleT>(), (AudioFrame<SampleT>*)stream, renderable_count);
    }
}

static const char* FE_AudioFormatStr(SDL_AudioFormat format)
{
    switch(format)
    {
    case AUDIO_S8:
        return "S8";
    case AUDIO_U8:
        return "U8";
    case AUDIO_S16MSB:
        return "S16MSB";
    case AUDIO_S16LSB:
        return "S16LSB";
    case AUDIO_U16MSB:
        return "U16MSB";
    case AUDIO_U16LSB:
        return "U16LSB";
    case AUDIO_S32MSB:
        return "S32MSB";
    case AUDIO_S32LSB:
        return "S32LSB";
    case AUDIO_F32MSB:
        return "F32MSB";
    case AUDIO_F32LSB:
        return "F32LSB";
    }
    return "UNK";
}

enum class FE_PickOutputResult
{
    WantMatchedName,
    WantDefaultDevice,
    NoOutputDevices,
    NoMatchingName,
};

FE_PickOutputResult FE_PickOutputDevice(std::string_view preferred_name, std::string& out_device_name)
{
    out_device_name.clear();

    const int num_audio_devs = SDL_GetNumAudioDevices(0);
    if (num_audio_devs == 0)
    {
        return FE_PickOutputResult::NoOutputDevices;
    }

    if (preferred_name.size() == 0)
    {
        return FE_PickOutputResult::WantDefaultDevice;
    }

    for (int i = 0; i < num_audio_devs; ++i)
    {
        if (SDL_GetAudioDeviceName(i, 0) == preferred_name)
        {
            out_device_name = SDL_GetAudioDeviceName(i, 0);
            return FE_PickOutputResult::WantMatchedName;
        }
    }

    // maybe we have an index instead of a name
    if (int out_device_id; TryParse(preferred_name, out_device_id))
    {
        if (out_device_id >= 0 && out_device_id < num_audio_devs)
        {
            out_device_name = SDL_GetAudioDeviceName(out_device_id, 0);
            return FE_PickOutputResult::WantMatchedName;
        }
    }

    out_device_name = preferred_name;
    return FE_PickOutputResult::NoMatchingName;
}

void FE_PrintAudioDevices()
{
    // we may want to print this information without initializing all of SDL
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "Failed to init audio: %s\n", SDL_GetError());
        return;
    }

    const int num_audio_devs = SDL_GetNumAudioDevices(0);
    if (num_audio_devs == 0)
    {
        fprintf(stderr, "No output devices found.\n");
    }

    fprintf(stderr, "\nKnown output devices:\n\n");

    for (int i = 0; i < num_audio_devs; ++i)
    {
        fprintf(stderr, "  %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
    }

    fprintf(stderr, "\n");

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool FE_OpenAudio(FE_Application& fe, const FE_Parameters& params)
{
    SDL_AudioSpec spec = {};
    SDL_AudioSpec spec_actual = {};

    fe.audio_page_size = (params.page_size / 2) * 2; // must be even
    fe.audio_buffer_size = fe.audio_page_size * params.page_num;

    switch (params.output_format)
    {
        case AudioFormat::S16:
            spec.format = AUDIO_S16SYS;
            spec.callback = FE_AudioCallback<int16_t>;
            break;
        case AudioFormat::S32:
            spec.format = AUDIO_S32SYS;
            spec.callback = FE_AudioCallback<int32_t>;
            break;
        case AudioFormat::F32:
            spec.format = AUDIO_F32SYS;
            spec.callback = FE_AudioCallback<float>;
            break;
    }
    spec.freq = RangeCast<int>(PCM_GetOutputFrequency(fe.instances[0].emu.GetPCM()));
    spec.channels = 2;
    spec.userdata = &fe;
    spec.samples = RangeCast<Uint16>(fe.audio_page_size / 4);

    std::string output_device_name;
    FE_PickOutputResult output_result = FE_PickOutputDevice(params.audio_device, output_device_name);
    switch (output_result)
    {
    case FE_PickOutputResult::WantMatchedName:
        fe.sdl_audio = SDL_OpenAudioDevice(output_device_name.c_str(), 0, &spec, &spec_actual, 0);
        break;
    case FE_PickOutputResult::WantDefaultDevice:
        output_device_name = "Default device";
        fe.sdl_audio = SDL_OpenAudioDevice(NULL, 0, &spec, &spec_actual, 0);
        break;
    case FE_PickOutputResult::NoOutputDevices:
        // in some cases this may still work
        fprintf(stderr, "No output devices found; attempting to open default device\n");
        output_device_name = "Default device";
        fe.sdl_audio = SDL_OpenAudioDevice(NULL, 0, &spec, &spec_actual, 0);
        break;
    case FE_PickOutputResult::NoMatchingName:
        // in some cases SDL cannot list all audio devices so we should still try
        fprintf(stderr, "No output device named '%s'; attempting to open it anyways...\n", params.audio_device.c_str());
        fe.sdl_audio = SDL_OpenAudioDevice(output_device_name.c_str(), 0, &spec, &spec_actual, 0);
        break;
    }

    if (!fe.sdl_audio)
    {
        fprintf(stderr, "Failed to open audio device: %s\n", SDL_GetError());
        // if we failed for this reason the user might want to know what valid devices exist
        if (output_result == FE_PickOutputResult::NoMatchingName)
        {
            FE_PrintAudioDevices();
        }
        return false;
    }

    fprintf(stderr, "Audio device: %s\n", output_device_name.c_str());

    fprintf(stderr, "Audio Requested: F=%s, C=%d, R=%d, B=%d\n",
           FE_AudioFormatStr(spec.format),
           spec.channels,
           spec.freq,
           spec.samples);

    fprintf(stderr, "Audio Actual: F=%s, C=%d, R=%d, B=%d\n",
           FE_AudioFormatStr(spec_actual.format),
           spec_actual.channels,
           spec_actual.freq,
           spec_actual.samples);
    fflush(stderr);

    SDL_PauseAudioDevice(fe.sdl_audio, 0);

    return true;
}

template <typename SampleT>
bool FE_IsBufferFull(FE_Instance& instance)
{
    // MCU_Step will always produce 1 or 2 samples. It's simplest to always leave enough space for oversampling because
    // then we don't need to re-align the write head every time.
    return instance.StaticSelectBuffer<SampleT>().GetWritableCount() < 2;
}

template <typename SampleT>
void FE_RunInstance(FE_Instance& instance)
{
    MCU_WorkThread_Lock(instance.emu.GetMCU());
    while (instance.running)
    {
        if (FE_IsBufferFull<SampleT>(instance))
        {
            MCU_WorkThread_Unlock(instance.emu.GetMCU());
            while (FE_IsBufferFull<SampleT>(instance))
            {
                SDL_Delay(1);
            }
            MCU_WorkThread_Lock(instance.emu.GetMCU());
        }

        MCU_Step(instance.emu.GetMCU());
    }
    MCU_WorkThread_Unlock(instance.emu.GetMCU());
}

bool FE_HandleGlobalEvent(FE_Application& fe, const SDL_Event& ev)
{
    switch (ev.type)
    {
        case SDL_QUIT:
            fe.running = false;
            return true;
        default:
            return false;
    }
}

void FE_EventLoop(FE_Application& fe)
{
    while (fe.running)
    {
        for (size_t i = 0; i < fe.instances_in_use; ++i)
        {
            if (fe.instances[i].emu.IsLCDEnabled())
            {
                if (LCD_QuitRequested(fe.instances[i].emu.GetLCD()))
                {
                    fe.running = false;
                }

                LCD_Update(fe.instances[i].emu.GetLCD());
            }
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (FE_HandleGlobalEvent(fe, ev))
            {
                // not directed at any particular window; don't let LCDs
                // handle this one
                continue;
            }

            for (size_t i = 0; i < fe.instances_in_use; ++i)
            {
                if (fe.instances[i].emu.IsLCDEnabled())
                {
                    LCD_HandleEvent(fe.instances[i].emu.GetLCD(), ev);
                }
            }
        }

        SDL_Delay(15);
    }
}

void FE_Run(FE_Application& fe)
{
    fe.running = true;

    for (size_t i = 0; i < fe.instances_in_use; ++i)
    {
        fe.instances[i].running = true;
        switch (fe.instances[i].format)
        {
            case AudioFormat::S16:
                fe.instances[i].thread = std::thread(FE_RunInstance<int16_t>, std::ref(fe.instances[i]));
                break;
            case AudioFormat::S32:
                fe.instances[i].thread = std::thread(FE_RunInstance<int32_t>, std::ref(fe.instances[i]));
                break;
            case AudioFormat::F32:
                fe.instances[i].thread = std::thread(FE_RunInstance<float>, std::ref(fe.instances[i]));
                break;
        }
    }

    FE_EventLoop(fe);

    for (size_t i = 0; i < fe.instances_in_use; ++i)
    {
        fe.instances[i].running = false;
        fe.instances[i].thread.join();
    }
}

#ifdef _WIN32
// On Windows we install a Ctrl-C handler to make sure that the event loop always receives an SDL_QUIT event. This
// is what normally happens on other platforms but only some Windows environments (for instance, a mingw64 shell).
// If the program is run from cmd or Windows explorer, SDL_QUIT is never sent and the program hangs.
BOOL WINAPI FE_CtrlCHandler(DWORD dwCtrlType)
{
    (void)dwCtrlType;
    SDL_Event quit_event{};
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
    return TRUE;
}
#endif

bool FE_Init()
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
    {
        fprintf(stderr, "FATAL ERROR: Failed to initialize the SDL2: %s.\n", SDL_GetError());
        fflush(stderr);
        return false;
    }

#ifdef _WIN32
    SetConsoleCtrlHandler(FE_CtrlCHandler, TRUE);
#endif

    return true;
}

bool FE_CreateInstance(FE_Application& container, const std::filesystem::path& base_path, const FE_Parameters& params)
{
    FE_Instance* fe = nullptr;

    if (!FE_AllocateInstance(container, &fe))
    {
        fprintf(stderr, "ERROR: Failed to allocate instance.\n");
        return false;
    }

    fe->format = params.output_format;

    if (!fe->emu.Init(EMU_Options { .enable_lcd = !params.no_lcd, .serial_type = params.serial_type }))
    {
        fprintf(stderr, "ERROR: Failed to init emulator.\n");
        return false;
    }

    switch (fe->format)
    {
        case AudioFormat::S16:
            fe->emu.SetSampleCallback(FE_ReceiveSample<int16_t>, fe);
            break;
        case AudioFormat::S32:
            fe->emu.SetSampleCallback(FE_ReceiveSample<int32_t>, fe);
            break;
        case AudioFormat::F32:
            fe->emu.SetSampleCallback(FE_ReceiveSample<float>, fe);
            break;
    }

    LCD_LoadBack(fe->emu.GetLCD(), base_path / "back.data");

    if (!fe->emu.LoadRoms(params.romset, *params.rom_directory))
    {
        fprintf(stderr, "ERROR: Failed to load roms.\n");
        return false;
    }
    fe->emu.Reset();
    fe->emu.GetPCM().disable_oversampling = params.disable_oversampling;

    if (!params.no_lcd && !LCD_CreateWindow(fe->emu.GetLCD()))
    {
        fprintf(stderr, "ERROR: Failed to create window.\n");
        return false;
    }

    return true;
}

void FE_DestroyInstance(FE_Instance& fe)
{
    fe.running = false;
}

void FE_Quit(FE_Application& container)
{
    // Important to close audio devices first since this will stop the SDL
    // audio thread. Otherwise we might get a UAF destroying ringbuffers
    // while they're still in use.
    SDL_CloseAudioDevice(container.sdl_audio);
    for (size_t i = 0; i < container.instances_in_use; ++i)
    {
        FE_DestroyInstance(container.instances[i]);
    }
    MIDI_Quit();
    SDL_Quit();
}

enum class FE_ParseError
{
    Success,
    InstancesInvalid,
    InstancesOutOfRange,
    UnexpectedEnd,
    PageSizeInvalid,
    PageCountInvalid,
    UnknownArgument,
    RomDirectoryNotFound,
    FormatInvalid,
    SerialTypeInvalid,
};

const char* FE_ParseErrorStr(FE_ParseError err)
{
    switch (err)
    {
        case FE_ParseError::Success:
            return "Success";
        case FE_ParseError::InstancesInvalid:
            return "Instances couldn't be parsed (should be 1-16)";
        case FE_ParseError::InstancesOutOfRange:
            return "Instances out of range (should be 1-16)";
        case FE_ParseError::UnexpectedEnd:
            return "Expected another argument";
        case FE_ParseError::PageSizeInvalid:
            return "Page size invalid";
        case FE_ParseError::PageCountInvalid:
            return "Page count invalid";
        case FE_ParseError::UnknownArgument:
            return "Unknown argument";
        case FE_ParseError::RomDirectoryNotFound:
            return "Rom directory doesn't exist";
        case FE_ParseError::FormatInvalid:
            return "Output format invalid";
        case FE_ParseError::SerialTypeInvalid:
            return "Serial Type Invalid";
    }
    return "Unknown error";
}

FE_ParseError FE_ParseCommandLine(int argc, char* argv[], FE_Parameters& result)
{
    CommandLineReader reader(argc, argv);

    while (reader.Next())
    {
        if (reader.Any("-h", "--help", "-?"))
        {
            result.help = true;
            return FE_ParseError::Success;
        }
        else if (reader.Any("-pi", "--portin"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            result.midiin_device = reader.Arg();
        }
        else if (reader.Any("-po", "--portout"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            result.midiout_device = reader.Arg();
        }
        else if (reader.Any("-st", "--serial_type"))
        {
            if(!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            if(reader.Arg() == "rs422" || reader.Arg() == "rs232c_1" || reader.Arg() == "rs232c_2")
            {
                result.serial_type = reader.Arg();
            }
            else
            {
                return FE_ParseError::SerialTypeInvalid;
            }
        }
        else if(reader.Any("-sp", "--serialport"))
        {
            if(!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            result.serial_port = reader.Arg();
        }
        else if (reader.Any("-a", "--audio-device"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            result.audio_device = reader.Arg();
        }
        else if (reader.Any("-f", "--format"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
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
                return FE_ParseError::FormatInvalid;
            }
        }
        else if (reader.Any("-b", "--buffer-size"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            std::string_view arg = reader.Arg();
            if (size_t colon = arg.find(':'); colon != std::string_view::npos)
            {
                auto page_size_sv = arg.substr(0, colon);
                auto page_num_sv  = arg.substr(colon + 1);

                if (!TryParse(page_size_sv, result.page_size))
                {
                    return FE_ParseError::PageSizeInvalid;
                }

                if (!TryParse(page_num_sv, result.page_num))
                {
                    return FE_ParseError::PageCountInvalid;
                }
            }
            else if (!reader.TryParse(result.page_size))
            {
                return FE_ParseError::PageSizeInvalid;
            }
        }
        else if (reader.Any("-r", "--reset"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
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
        else if (reader.Any("-n", "--instances"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            if (!reader.TryParse(result.instances))
            {
                return FE_ParseError::InstancesInvalid;
            }

            if (result.instances < 1 || result.instances > 16)
            {
                return FE_ParseError::InstancesOutOfRange;
            }
        }
        else if (reader.Any("--no-lcd"))
        {
            result.no_lcd = true;
        }
        else if (reader.Any("--disable-oversampling"))
        {
            result.disable_oversampling = true;
        }
        else if (reader.Any("-d", "--rom-directory"))
        {
            if (!reader.Next())
            {
                return FE_ParseError::UnexpectedEnd;
            }

            result.rom_directory = reader.Arg();

            if (!std::filesystem::exists(*result.rom_directory))
            {
                return FE_ParseError::RomDirectoryNotFound;
            }
        }
        else if (reader.Any("--mk2"))
        {
            result.romset = Romset::MK2;
            result.autodetect = false;
        }
        else if (reader.Any("--st"))
        {
            result.romset = Romset::ST;
            result.autodetect = false;
        }
        else if (reader.Any("--mk1"))
        {
            result.romset = Romset::MK1;
            result.autodetect = false;
        }
        else if (reader.Any("--cm300"))
        {
            result.romset = Romset::CM300;
            result.autodetect = false;
        }
        else if (reader.Any("--jv880"))
        {
            result.romset = Romset::JV880;
            result.autodetect = false;
        }
        else if (reader.Any("--scb55"))
        {
            result.romset = Romset::SCB55;
            result.autodetect = false;
        }
        else if (reader.Any("--rlp3237"))
        {
            result.romset = Romset::RLP3237;
            result.autodetect = false;
        }
        else if (reader.Any("--sc155"))
        {
            result.romset = Romset::SC155;
            result.autodetect = false;
        }
        else if (reader.Any("--sc155mk2"))
        {
            result.romset = Romset::SC155MK2;
            result.autodetect = false;
        }
        else
        {
            return FE_ParseError::UnknownArgument;
        }
    }

    return FE_ParseError::Success;
}

void FE_Usage()
{
    constexpr const char* USAGE_STR = R"(Usage: %s [options]

General options:
  -?, -h, --help                                Display this information.

Audio options:
  -a, --audio-device <device_name_or_number>    Set output audio device.
  -b, --buffer-size  <page_size>[:page_count]   Set Audio Buffer size.
  -f, --format       s16|s32|f32                Set output format.
  --disable-oversampling                        Halves output frequency.

MIDI port options (default, unless set to serial):
  -pi, --portin      <device_name_or_number>    Set MIDI input port.
  -po, --portout     <device_name_or_number>    Set MIDI output port.

Serial Port options:
  -st, --serial_type rs422|rs232c_1|rs232c_2    Set serial connection type
  -sp, --serialport  <serial_io_port>           Set the serial port/named pipe/unix socket for serial I/O.

Emulator options:
  -r, --reset     gs|gm                         Reset system in GS or GM mode.
  -n, --instances <count>                       Set number of emulator instances.
  --no-lcd                                      Run without LCDs.

ROM management options:
  -d, --rom-directory <dir>                     Sets the directory to load roms from.
  --mk2                                         Use SC-55mk2 ROM set.
  --st                                          Use SC-55st ROM set.
  --mk1                                         Use SC-55mk1 ROM set.
  --cm300                                       Use CM-300/SCC-1 ROM set.
  --jv880                                       Use JV-880 ROM set.
  --scb55                                       Use SCB-55 ROM set.
  --rlp3237                                     Use RLP-3237 ROM set.

)";

    std::string name = P_GetProcessPath().stem().generic_string();
    fprintf(stderr, USAGE_STR, name.c_str());
    MIDI_PrintDevices();
    FE_PrintAudioDevices();
}

int main(int argc, char *argv[])
{
    FE_Parameters params;
    FE_ParseError result = FE_ParseCommandLine(argc, argv, params);
    if (result != FE_ParseError::Success)
    {
        fprintf(stderr, "error: %s\n", FE_ParseErrorStr(result));
        return 1;
    }

    if (params.help)
    {
        FE_Usage();
        return 0;
    }

    FE_Application frontend;

    std::filesystem::path base_path = P_GetProcessPath().parent_path();

    if (std::filesystem::exists(base_path / "../share/nuked-sc55"))
        base_path = base_path / "../share/nuked-sc55";

    fprintf(stderr, "Base path is: %s\n", base_path.generic_string().c_str());

    if (!params.rom_directory)
    {
        params.rom_directory = base_path;
    }

    fprintf(stderr, "ROM directory is: %s\n", params.rom_directory->generic_string().c_str());

    if (params.autodetect)
    {
        params.romset = EMU_DetectRomset(*params.rom_directory);
        fprintf(stderr, "ROM set autodetect: %s\n", EMU_RomsetName(params.romset));
    }

    if (!FE_Init())
    {
        fprintf(stderr, "FATAL ERROR: Failed to initialize frontend\n");
        return 1;
    }

    for (size_t i = 0; i < params.instances; ++i)
    {
        if (!FE_CreateInstance(frontend, base_path, params))
        {
            fprintf(stderr, "FATAL ERROR: Failed to create instance %" PRIu64 "\n", i);
            return 1;
        }
    }

    if (!FE_OpenAudio(frontend, params))
    {
        fprintf(stderr, "FATAL ERROR: Failed to open the audio stream.\n");
        fflush(stderr);
        return 1;
    }

    for (size_t i = 0; i < frontend.instances_in_use; ++i)
    {
        FE_Instance& fe = frontend.instances[i];
        switch (fe.format)
        {
        case AudioFormat::S16:
            fe.sample_buffer.Init(sizeof(int16_t) * frontend.audio_buffer_size);
            fe.view_s16 = RingbufferView<AudioFrame<int16_t>>(fe.sample_buffer);
            break;
        case AudioFormat::S32:
            fe.sample_buffer.Init(sizeof(int32_t) * frontend.audio_buffer_size);
            fe.view_s32 = RingbufferView<AudioFrame<int32_t>>(fe.sample_buffer);
            break;
        case AudioFormat::F32:
            fe.sample_buffer.Init(sizeof(float) * frontend.audio_buffer_size);
            fe.view_f32 = RingbufferView<AudioFrame<float>>(fe.sample_buffer);
            break;
        }
    }

    if(!params.serial_type.empty() && !params.serial_port.empty())
    {
        if(!SERIAL_Init(frontend, params.serial_port))
        {
            fprintf(stderr, "ERROR: Failed to initialize the Serial Port.\nWARNING: Continuing without Serial Port...\n");
            fflush(stderr);
        }
    }
    else
    {
        if (!MIDI_Init(frontend, params.midiin_device, params.midiout_device))
        {
            fprintf(stderr, "ERROR: Failed to initialize the MIDI Ports.\nWARNING: Continuing without MIDI Ports...\n");
            fflush(stderr);
        }
    }

    for (size_t i = 0; i < frontend.instances_in_use; ++i)
    {
        frontend.instances[i].emu.PostSystemReset(params.reset);
    }

    FE_Run(frontend);

    FE_Quit(frontend);

    return 0;
}
