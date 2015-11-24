#pragma once

#include <Windows.h>
#include <mmsystem.h>

#include <string>

#define USE_MIDI_OUT 1

#include <exception>
#include <iostream>

namespace midis
{
	static const short PITCH_BEND_MIN = -0x2000;
	static const short PITCH_BEND_MAX = 0x1fff;
	static const unsigned short PITCH_BEND_ABSOULTE_MIN = 0x0000;
	static const unsigned short PITCH_BEND_ABSOULTE_MAX = 0x3fff;
	static const unsigned short PITCH_BEND_ABSOULTE_ZERO = 0x2000;
	static const BYTE CONTROLLER_MIN = 0;
	static const BYTE CONTROLLER_MAX = 127;

	class MidiStreamerNoSuchDevice_Exception : std::runtime_error
	{
		std::string device_name;
	public:
		std::string GetDeviceName() { return device_name; }
		MidiStreamerNoSuchDevice_Exception(std::string _device_name)
			: runtime_error(std::string("No such device: ") + _device_name)
			, device_name(_device_name)
		{
			std::cerr << "No such device: " << device_name << std::endl;
		}
	};

	typedef unsigned long midi_err_t;
	typedef BYTE controller_num_t;
	typedef BYTE controller_val_t;
	typedef unsigned int device_num_t;

	int GetOutDeviceId(const std::string &device_name);
	std::string GetOutDeviceName(int device_id);

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
		virtual midi_err_t SendPitchBend(short pitchbend_value,
			BYTE channel_number);
		MidiOutStream();
		virtual ~MidiOutStream();
		
	};

	void PrintAvailableOutputDevices(std::ostream &out);

}