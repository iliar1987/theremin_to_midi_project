#pragma once

#include <Windows.h>
#include <mmsystem.h>

#include <string>

#define USE_MIDI_OUT 1

namespace midis
{

	typedef unsigned long midi_err_t;
	typedef BYTE controller_num_t;
	typedef BYTE controller_val_t;
	typedef unsigned int device_num_t;

	std::string GetMidiErrorMessage(midi_err_t err);

	class MidiOutStream
	{
		MidiOutStream(const MidiOutStream&);
	protected:
#if !USE_MIDI_OUT
		HMIDISTRM my_midi_stream = 0;
#else
		HMIDIOUT my_midi_stream=0;
#endif
	public:
		midi_err_t OpenStream(device_num_t midi_device_number);
#if !USE_MIDI_OUT
		virtual midi_err_t StartStream();
		virtual midi_err_t StopStream();
#endif
		midi_err_t CloseStream();
		virtual midi_err_t SendController(controller_num_t controller_num,
			controller_val_t controller_val, BYTE channel_number);
		MidiOutStream();
		virtual ~MidiOutStream();
		
	};

}