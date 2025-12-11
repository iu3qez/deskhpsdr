/*
 * Windows MIDI support for pihpsdr
 *
 * Implements MIDI functionality using Windows Multimedia API (mmsystem.h)
 */

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

#include "midi.h"
#include "midi_menu.h" 
#include "message.h" 

MIDI_DEVICE midi_devices[MAX_MIDI_DEVICES];
int n_midi_devices = 0;

static HMIDIIN midi_handles[MAX_MIDI_DEVICES];
static gboolean configure = FALSE;

void configure_midi_device(gboolean state) {
  configure = state;
}

// Forward declaration
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

void get_midi_devices() {
    UINT numDevs = midiInGetNumDevs();
    t_print("MIDI: Found %d system devices\n", numDevs);
    if (numDevs > MAX_MIDI_DEVICES) numDevs = MAX_MIDI_DEVICES;
    
    n_midi_devices = 0;
    MIDIINCAPSA caps; // Explicitly use ANSI version

    for (UINT i = 0; i < numDevs; i++) {
        MMRESULT res = midiInGetDevCapsA(i, &caps, sizeof(caps));
        if (res == MMSYSERR_NOERROR) {
            t_print("MIDI: Device %d: %s\n", i, caps.szPname);
            // Check if name changed for existing slot
             if (midi_devices[n_midi_devices].name) {
                 if (strcmp(midi_devices[n_midi_devices].name, caps.szPname) != 0) {
                     g_free(midi_devices[n_midi_devices].name);
                     midi_devices[n_midi_devices].name = g_strdup(caps.szPname);
                     
                     // If it was active, close it because device changed
                     if (midi_devices[n_midi_devices].active) {
                         close_midi_device(n_midi_devices);
                     }
                 }
             } else {
                 midi_devices[n_midi_devices].name = g_strdup(caps.szPname);
             }
             
             n_midi_devices++;
        } else {
            t_print("MIDI: midiInGetDevCapsA failed for %d: error %d\n", i, res);
        }
    }
}

void register_midi_device(int index) {
    if (index < 0 || index >= n_midi_devices) return;
    
    if (midi_devices[index].active) return; // Already active

    MMRESULT result = midiInOpen(&midi_handles[index], index, (DWORD_PTR)MidiInProc, (DWORD_PTR)index, CALLBACK_FUNCTION);
    if (result == MMSYSERR_NOERROR) {
        midiInStart(midi_handles[index]);
        midi_devices[index].active = 1;
        t_print("MIDI Device %d opened: %s\n", index, midi_devices[index].name);
    } else {
        t_print("Failed to open MIDI device %d error %d\n", index, result);
    }
}

void close_midi_device(int index) {
    if (index < 0 || index >= MAX_MIDI_DEVICES) return;
    if (!midi_devices[index].active) return;

    midiInStop(midi_handles[index]);
    midiInReset(midi_handles[index]);
    midiInClose(midi_handles[index]);
    
    midi_devices[index].active = 0;
    midi_handles[index] = NULL;
}

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    // dwInstance contains the index passed in midiInOpen
    int index = (int)dwInstance;
    (void)index; // Silence unused variable warning 
    
    if (wMsg == MIM_DATA) {
        // Short MIDI message
        // dwParam1: Status (lowest byte), Data1, Data2, Unused
        unsigned char status = dwParam1 & 0xFF;
        unsigned char data1 = (dwParam1 >> 8) & 0xFF;
        unsigned char data2 = (dwParam1 >> 16) & 0xFF;
        
        int chan = status & 0x0F;
        int cmd = status & 0xF0;

        switch (cmd) {
            case 0x80: // Note Off
                if (configure) NewMidiConfigureEvent(MIDI_NOTE, chan, data1, 0);
                else NewMidiEvent(MIDI_NOTE, chan, data1, 0);
                break;
                
            case 0x90: // Note On
                if (data2 == 0) { // Velocity 0 = Note Off
                    if (configure) NewMidiConfigureEvent(MIDI_NOTE, chan, data1, 0);
                    else NewMidiEvent(MIDI_NOTE, chan, data1, 0);
                } else {
                    if (configure) NewMidiConfigureEvent(MIDI_NOTE, chan, data1, 1);
                    else NewMidiEvent(MIDI_NOTE, chan, data1, 1);
                }
                break;
                
            case 0xB0: // Control Change
                 // Ignore pairs logic matching alsa_midi.c
                 if (!midiIgnoreCtrlPairs || data1 < 32 || data1 >= 64) {
                     if (configure) NewMidiConfigureEvent(MIDI_CTRL, chan, data1, data2);
                     else NewMidiEvent(MIDI_CTRL, chan, data1, data2);
                 }
                 break;
                 
            case 0xE0: // Pitch Bend
                 // Value is 14 bit: data1 (LSB) + data2 (MSB) * 128
                 if (configure) NewMidiConfigureEvent(MIDI_PITCH, chan, 0, data1 + 128 * data2);
                 else NewMidiEvent(MIDI_PITCH, chan, 0, data1 + 128 * data2);
                 break;
        }
    }
}

#endif // _WIN32

