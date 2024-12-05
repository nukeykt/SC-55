#include <stdio.h>
#include "mcu.h"
#include "midi.h"
#include <RtMidi.h>

static RtMidiIn *s_midi_in = nullptr;
static RtMidiOut *s_midi_out = nullptr;

static void MidiOnReceive(double, std::vector<uint8_t> *message, void *)
{
    uint8_t *beg = message->data();
    uint8_t *end = message->data() + message->size();

    MCU_Midi_Lock();
    while(beg < end)
        MCU_PostUART(*beg++);
    MCU_Midi_Unlock();
}

static void MidiOnError(RtMidiError::Type, const std::string &errorText, void *)
{
    fprintf(stderr, "RtMidi: Error has occured: %s\n", errorText.c_str());
    fflush(stderr);
}

int MIDI_Init(int inport, int outport)
{
    if (inport >= 0) {
        if (s_midi_in)
        {
            printf("MIDI already running\n");
            return 0; // Already running
        }

        s_midi_in = new RtMidiIn(RtMidi::UNSPECIFIED, "Nuked SC55", 1024);
        s_midi_in->ignoreTypes(false, false, false); // SysEx disabled by default
        s_midi_in->setCallback(&MidiOnReceive, nullptr); // FIXME: (local bug) Fix the linking error
        s_midi_in->setErrorCallback(&MidiOnError, nullptr);

        unsigned count = s_midi_in->getPortCount();

        if (count == 0)
        {
            printf("No midi input\n");
            delete s_midi_in;
            s_midi_in = nullptr;
            return 0;
        }

        if (inport < 0 || inport >= count)
        {
            printf("Out of range midi port is requested. Defaulting to port 0\n");
            inport = 0;
        }

        s_midi_in->openPort(inport, "Nuked SC55");
    }
    if (outport >= 0) {
        if (s_midi_out)
        {
            printf("MIDI already running\n");
            return 0; // Already running
        }

        s_midi_out = new RtMidiOut(RtMidi::UNSPECIFIED, "Nuked SC55");\
        s_midi_out->setErrorCallback(&MidiOnError, nullptr);

        unsigned count = s_midi_in->getPortCount();

        if (count == 0)
        {
            printf("No midi output\n");
            delete s_midi_out;
            s_midi_out = nullptr;
            return 0;
        }

        if (outport < 0 || outport >= count)
        {
            printf("Out of range midi port is requested. Defaulting to port 0\n");
            outport = 0;
        }

        s_midi_in->openPort(outport, "Nuked SC55");
    }

    return 1;
}

void MIDI_Quit()
{
    if (s_midi_in)
    {
        s_midi_in->closePort();
        delete s_midi_in;
        s_midi_in = nullptr;
    }
    if (s_midi_out)
    {
        s_midi_out->closePort();
        delete s_midi_out;
        s_midi_out = nullptr;
    }
}

void MIDI_PostShortMessge(uint8_t *message, int len) {
    if (s_midi_out) {
        s_midi_out->sendMessage(message, len);
    }
}
void MIDI_PostSysExMessge(uint8_t *message, int len) {
    if (s_midi_out) {
        s_midi_out->sendMessage(message, len);
    }
}

int MIDI_GetMidiInDevices(char* devices) {
    auto rtmidi = new RtMidiIn(RtMidi::UNSPECIFIED, "Nuked SC55", 1024);
    int numDevices = rtmidi->getPortCount();
    int length = 0;
    for (int i = 0; i < numDevices; i++) {
        std::string pname = rtmidi->getPortName(i);
        const char* name = pname.c_str();
        int len = pname.length();
        if (devices != NULL) {
            memcpy(devices + length, name, len);
            *(devices + length + len) = 0;
        }
        length += len + 1;
    }
    delete rtmidi;
    return length;
}

int MIDI_GetMidiOutDevices(char* devices) {
    auto rtmidi = new RtMidiOut(RtMidi::UNSPECIFIED, "Nuked SC55");
    int numDevices = rtmidi->getPortCount();
    int length = 0;
    for (int i = 0; i < numDevices; i++) {
        std::string pname = rtmidi->getPortName(i);
        const char* name = pname.c_str();
        int len = pname.length();
        if (devices != NULL) {
            memcpy(devices + length, name, len);
            *(devices + length + len) = 0;
        }
        length += len + 1;
    }
    delete rtmidi;
    return length;
}