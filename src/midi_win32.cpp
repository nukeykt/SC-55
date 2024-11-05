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
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>
#include <span>
#include <cstdint>
#include <string>
#include "command_line.h"
#include "midi.h"

static HMIDIIN midi_in_handle;
static HMIDIOUT midi_out_handle;
static MIDIHDR midi_buffer;

static uint8_t midi_in_buffer[1024];

static FE_Application* midi_frontend = nullptr;

void FE_RouteMIDI(FE_Application& fe, std::span<const uint8_t> bytes);

void CALLBACK MIDIIN_Callback(
    HMIDIIN   hMidiIn,
    UINT      wMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{
    (void)hMidiIn;
    (void)dwInstance;
    (void)dwParam2;

    switch (wMsg)
    {
        case MIM_OPEN:
            break;
        case MIM_DATA:
        {
            uint8_t b1 = dwParam1 & 0xff;
            switch (b1 & 0xf0)
            {
                case 0x80:
                case 0x90:
                case 0xa0:
                case 0xb0:
                case 0xe0:
                    {
                        uint8_t buf[3] = {
                            (uint8_t)b1,
                            (uint8_t)((dwParam1 >> 8) & 0xff),
                            (uint8_t)((dwParam1 >> 16) & 0xff),
                        };
                        FE_RouteMIDI(*midi_frontend, buf);
                    }
                    break;
                case 0xc0:
                case 0xd0:
                    {
                        uint8_t buf[2] = {
                            (uint8_t)b1,
                            (uint8_t)((dwParam1 >> 8) & 0xff),
                        };
                        FE_RouteMIDI(*midi_frontend, buf);
                    }
                    break;
            }
            break;
        }
        case MIM_LONGDATA:
        case MIM_LONGERROR:
        {
            MMRESULT result = midiInUnprepareHeader(midi_in_handle, &midi_buffer, sizeof(MIDIHDR));
            if (result == MMSYSERR_INVALHANDLE)
            {
                // If this happens, the frontend probably called MIDI_Quit and
                // midi_frontend is no longer valid. We got here because this
                // callback is running in a separate thread and might be called
                // after MIDI_Quit.
                break;
            }

            if (wMsg == MIM_LONGDATA)
            {
                FE_RouteMIDI(*midi_frontend, std::span(midi_in_buffer, midi_buffer.dwBytesRecorded));
            }

            midiInPrepareHeader(midi_in_handle, &midi_buffer, sizeof(MIDIHDR));
            midiInAddBuffer(midi_in_handle, &midi_buffer, sizeof(MIDIHDR));

            break;
        }
        default:
            fprintf(stderr, "hmm");
            break;
    }
}

void MIDI_PrintDevices()
{
    const UINT num_devices = midiInGetNumDevs();

    if (num_devices == 0)
    {
        fprintf(stderr, "No midi devices found.\n");
    }

    MMRESULT result;
    MIDIINCAPSA device_caps;

    fprintf(stderr, "Known midi devices:\n\n");

    for (UINT i = 0; i < num_devices; ++i)
    {
        result = midiInGetDevCapsA(i, &device_caps, sizeof(MIDIINCAPSA));
        if (result == MMSYSERR_NOERROR)
        {
            fprintf(stderr, "  %d: %s\n", i, device_caps.szPname);
        }
    }
}

struct MIDI_PickedDevice
{
    UINT         device_in_id;
    MIDIINCAPSA  device_in_caps;
    UINT         device_out_id;
    MIDIOUTCAPSA device_out_caps;
};

bool MIDI_PickDevice(std::string_view preferred_in_name, std::string_view preferred_out_name, MIDI_PickedDevice& out_picked)
{
    const UINT num_devices = midiInGetNumDevs();
    const UINT num_out_devices = midiOutGetNumDevs();

    if (num_devices == 0)
    {
        fprintf(stderr, "No midi input\n");
        return false;
    }

    MMRESULT result;

    if (preferred_in_name.size() == 0)
    {
        // default to first device
        result = midiInGetDevCapsA(0, &out_picked.device_in_caps, sizeof(MIDIINCAPSA));
        if (result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiInGetDevCapsA failed\n");
            return false;
        }
        out_picked.device_in_id = 0;
        out_picked.device_out_id = 0;
        return true;
    }

    for (UINT i = 0; i < num_devices; ++i)
    {
        result = midiInGetDevCapsA(i, &out_picked.device_in_caps, sizeof(MIDIINCAPSA));
        if (result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiInGetDevCapsA failed\n");
            return false;
        }
        if (std::string_view(out_picked.device_in_caps.szPname) == preferred_in_name)
        {
            out_picked.device_in_id = i;
            for (UINT j = 0; j < num_out_devices; ++j)
            {
                result = midiOutGetDevCapsA(j, &out_picked.device_out_caps, sizeof(MIDIOUTCAPSA));
                if (result != MMSYSERR_NOERROR)
                {
                    fprintf(stderr, "midiOutGetDevCapsA failed\n");
                    break;
                }
                if (std::string_view(out_picked.device_out_caps.szPname) == preferred_out_name)
                {
                    out_picked.device_out_id = j;
                }
            }
            
            return true;
        }
    }

    // user provided a number
    if (UINT device_in_id; TryParse(preferred_in_name, device_in_id))
    {
        if (device_in_id < num_devices)
        {
            result = midiInGetDevCaps(device_in_id, &out_picked.device_in_caps, sizeof(MIDIINCAPSA));
            if (result != MMSYSERR_NOERROR)
            {
                fprintf(stderr, "midiInGetDevCapsA failed\n");
                return false;
            }
            out_picked.device_in_id = device_in_id;
            if(UINT device_out_id; TryParse(preferred_out_name, device_out_id))
            {
                result = midiOutGetDevCaps(device_out_id, &out_picked.device_out_caps, sizeof(MIDIOUTCAPSA));
                if (result != MMSYSERR_NOERROR)
                {
                    fprintf(stderr, "midiOutGetDevCapsA failed\n");
                    return false;
                }
                out_picked.device_out_id = device_out_id;
            }
            return true;
        }
    }

    fprintf(stderr, "No input device named '%s'\n", std::string(preferred_in_name).c_str());
    return false;
}

bool MIDI_Init(FE_Application& fe, std::string_view in_port_name_or_id, std::string_view out_port_name_or_id)
{
    midi_frontend = &fe;

    MIDI_PickedDevice picked_device;
    if (!MIDI_PickDevice(in_port_name_or_id, out_port_name_or_id, picked_device))
    {
        return false;
    }

    MMRESULT resultin = midiInOpen(&midi_in_handle, picked_device.device_in_id, (DWORD_PTR)MIDIIN_Callback, 0, CALLBACK_FUNCTION);
    if (resultin != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed\n");
        return false;
    }

    fprintf(stderr, "Opened midi input port: %s\n", picked_device.device_in_caps.szPname);

    midi_buffer.lpData = (LPSTR)midi_in_buffer;
    midi_buffer.dwBufferLength = sizeof(midi_in_buffer);

    resultin = midiInPrepareHeader(midi_in_handle, &midi_buffer, sizeof(MIDIHDR));
    if (resultin != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInPrepareHeader failed\n");
        return false;
    }

    resultin = midiInAddBuffer(midi_in_handle, &midi_buffer, sizeof(MIDIHDR));
    if (resultin != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInAddBuffer failed\n");
        return false;
    }

    resultin = midiInStart(midi_in_handle);
    if (resultin != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInStart failed\n");
        return false;
    }

    MMRESULT resultout = midiOutOpen(&midi_out_handle, picked_device.device_out_id, (DWORD_PTR)NULL, 0, CALLBACK_NULL);
    if(resultout == MMSYSERR_NOERROR)
        fprintf(stderr, "Opened midi output port: %s\n", picked_device.device_in_caps.szPname);

    return true;
}

void MIDI_Quit()
{
    if (midi_in_handle)
    {
        midiInStop(midi_in_handle);
        midiInClose(midi_in_handle);
        midi_in_handle = 0;
    }
    if (midi_out_handle)
    {
        midiOutClose(midi_out_handle);
        midi_out_handle = 0;
    }
    midi_frontend = nullptr;
}

void MIDI_PostShortMessage(uint8_t *message, int len) {
    if (midi_out_handle) {
        DWORD msg = 0;
        for (int i = 0; i < len; i++) {
            msg |= *message++ << i * 8;
        }
        midiOutShortMsg(midi_out_handle, msg);
    }
}
void MIDI_PostSysExMessage(uint8_t *message, int len) {
    if (midi_out_handle) {
        MIDIHDR buffer;
        memset(&buffer, 0, sizeof buffer);
        buffer.dwBufferLength = len;
        buffer.lpData = (LPSTR) message;
        midiOutPrepareHeader(midi_out_handle, &buffer, sizeof buffer);
        midiOutLongMsg(midi_out_handle, &buffer, sizeof buffer);
        midiOutUnprepareHeader(midi_out_handle, &buffer, sizeof buffer);
    }
}