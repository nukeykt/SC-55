#include <stdio.h>
#include "mcu.h"
#include "emu.h"
#include "midi.h"
#include "cast.h"
#include "command_line.h"
#include <RtMidi.h>

static RtMidiIn *s_midi_in = nullptr;

static FE_Application* midi_frontend = nullptr;

void FE_RouteMIDI(FE_Application& fe, std::span<const uint8_t> bytes);

static void MidiOnReceive(double, std::vector<uint8_t> *message, void *)
{
    FE_RouteMIDI(*midi_frontend, *message);
}

static void MidiOnError(RtMidiError::Type, const std::string &errorText, void *)
{
    fprintf(stderr, "RtMidi: Error has occured: %s\n", errorText.c_str());
    fflush(stderr);
}

// rtmidi will append a space and the port number to the port name which makes it useless as a stable string identifier
void MIDI_StripRtmidiPortNumber(std::string& port_name)
{
    if (port_name.size() == 0)
    {
        return;
    }

    if (port_name.back() < '0' || port_name.back() > '9')
    {
        return;
    }

    auto last_space = port_name.rfind(' ');
    if (last_space != std::string_view::npos)
    {
        port_name.resize(last_space);
    }
}

void MIDI_PrintDevices()
{
    try
    {
        std::unique_ptr<RtMidiIn> midi = std::make_unique<RtMidiIn>();

        const unsigned int num_devices = midi->getPortCount();

        if (num_devices == 0)
        {
            fprintf(stderr, "No midi devices found.\n");
        }

        fprintf(stderr, "Known midi devices:\n\n");

        for (unsigned int i = 0; i < num_devices; ++i)
        {
            std::string friendly_name = midi->getPortName(i);
            MIDI_StripRtmidiPortNumber(friendly_name);

            fprintf(stderr, "  %d: %s\n", i, friendly_name.c_str());
        }
    }
    catch (const RtMidiError& err)
    {
        // this exception shouldn't escape to the caller since it's not exception-aware
        fprintf(stderr, "Failed to enumerate midi devices: %s\n", err.getMessage().c_str());
    }
}

struct MIDI_PickedDevice
{
    unsigned int device_id;
    std::string  device_name;
};

// throws RtMidiError
bool MIDI_PickInputDevice(RtMidiIn& midi, std::string_view preferred_name, MIDI_PickedDevice& out_picked)
{
    const unsigned int num_devices = midi.getPortCount();

    if (num_devices == 0)
    {
        fprintf(stderr, "No midi input\n");
        return false;
    }

    if (preferred_name.size() == 0)
    {
        // default to first device
        out_picked.device_id   = 0;
        out_picked.device_name = midi.getPortName(0);
        return true;
    }

    for (unsigned int i = 0; i < num_devices; ++i)
    {
        std::string name = midi.getPortName(i);
        if (preferred_name == name)
        {
            out_picked.device_id   = i;
            out_picked.device_name = std::move(name);
            return true;
        }

        MIDI_StripRtmidiPortNumber(name);
        if (preferred_name == name)
        {
            out_picked.device_id   = i;
            out_picked.device_name = std::move(name);
            return true;
        }
    }

    // user provided a number
    if (unsigned int device_id; TryParse(preferred_name, device_id))
    {
        if (device_id < num_devices)
        {
            out_picked.device_id   = device_id;
            out_picked.device_name = midi.getPortName(device_id);
            return true;
        }
    }

    fprintf(stderr, "No input device named '%s'\n", std::string(preferred_name).c_str());
    return false;
}

bool MIDI_Init(FE_Application& fe, std::string_view port_name_or_id)
{
    if (s_midi_in)
    {
        fprintf(stderr, "MIDI already running\n");
        return false; // Already running
    }

    midi_frontend = &fe;

    s_midi_in = new RtMidiIn(RtMidi::UNSPECIFIED, "Nuked SC55", 1024);
    s_midi_in->ignoreTypes(false, false, false); // SysEx disabled by default
    s_midi_in->setCallback(&MidiOnReceive, nullptr); // FIXME: (local bug) Fix the linking error
    s_midi_in->setErrorCallback(&MidiOnError, nullptr);

    MIDI_PickedDevice picked_device;

    try
    {
        if (!MIDI_PickInputDevice(*s_midi_in, port_name_or_id, picked_device))
        {
            fprintf(stderr, "Failed to initialize RtMidi\n");
            return false;
        }
    }
    catch (const RtMidiError& err)
    {
        fprintf(stderr, "Failed to initialize RtMidi: %s\n", err.getMessage().c_str());
    }

    s_midi_in->openPort(picked_device.device_id, "Nuked SC55");
    fprintf(stderr, "Opened midi port: %s\n", picked_device.device_name.c_str());

    return true;
}

void MIDI_Quit()
{
    if (s_midi_in)
    {
        s_midi_in->closePort();
        delete s_midi_in;
        s_midi_in = nullptr;
        midi_frontend = nullptr;
    }
}
