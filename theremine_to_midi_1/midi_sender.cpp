
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <mmsystem.h>

#include "midi_sender.h"

#include <sstream>

using namespace midis;

int midis::GetOutDeviceId(const std::string &device_name)
{
	unsigned int num_devs = midiOutGetNumDevs();
	for (unsigned int i = 0; i < num_devs; i++)
	{
		std::string this_dev_name = GetOutDeviceName(i);
		if (this_dev_name == device_name)
		{
			return i;
		}
	}
	throw MidiStreamerNoSuchDevice_Exception(device_name);
}

std::string midis::GetOutDeviceName(int device_id)
{
	MIDIOUTCAPS midiout_caps;
	midiOutGetDevCaps(device_id, &midiout_caps, sizeof(midiout_caps));
	return std::string(midiout_caps.szPname);
}

std::string midis::GetMidiErrorMessage(unsigned long err)
{
#define BUFFERSIZE 120
	char	buffer[BUFFERSIZE];

	unsigned int err2;
	std::ostringstream str;
	if (!(err2 = midiOutGetErrorText(err, &buffer[0], BUFFERSIZE)))
	{
		str << buffer;
	}
	else if (err2 == MMSYSERR_BADERRNUM)
	{
		str << "Strange MIDI error number returned!";
	}
	else
	{
		str << "MIDI: Specified pointer is invalid!";
	}
	return str.str();
}

midi_err_t midis::MidiOutStream::OpenStream(device_num_t device_number)
{
	midi_err_t err =
#if !USE_MIDI_OUT
		midiStreamOpen(&my_midi_stream, &device_number, 1, 0, 0, CALLBACK_NULL);
#else
		midiOutOpen(&my_midi_stream, device_number, CALLBACK_NULL, 0,0);
#endif
	if (err)
		return err;
	return 0;
}

midi_err_t midis::MidiOutStream::CloseStream()
{
	if (!my_midi_stream)
		return 0;
#if !USE_MIDI_OUT
	return midiStreamClose(my_midi_stream);
#else
	return midiOutClose(my_midi_stream);
#endif
}

#if !USE_MIDI_OUT

midi_err_t midis::MidiOutStream::StartStream()
{
	return midiStreamRestart(my_midi_stream);
}

midi_err_t midis::MidiOutStream::StopStream()
{
	if (!my_midi_stream)
		return 0;
	return midiStreamStop(my_midi_stream);
}
#endif

midis::MidiOutStream::MidiOutStream() {}
midis::MidiOutStream::~MidiOutStream() { CloseStream(); }

midi_err_t midis::MidiOutStream::SendPitchBend(
	short pitchbend_value,
	BYTE channel_number)
{
	midi_err_t err;

	//A better solution is the ShortMsg facility
	union {
		DWORD dwData;
		BYTE bData[4];
	} u;

	// Construct the MIDI message. 

	u.bData[0] = 0xb0 | (midi_channel_number & 0x0f);  // MIDI status byte 
	u.bData[1] = controller_num;   // first MIDI data byte 
	u.bData[2] = controller_val;   // second MIDI data byte 
	u.bData[3] = 0x00;

	// Send the message. 
	err = midiOutShortMsg(hmo, u.dwData);
	return err;
}

midi_err_t midis::MidiOutStream::SendController(
	controller_num_t controller_num
	, controller_val_t controller_val
	, BYTE midi_channel_number)
{
	//MIDIHDR hdr;
#if !USE_MIDI_OUT
	HMIDIOUT hmo = reinterpret_cast<HMIDIOUT>(my_midi_stream);
#else
	HMIDIOUT hmo = my_midi_stream;
#endif

	midi_err_t err;

	//A better solution is the ShortMsg facility
	union {
		DWORD dwData;
		BYTE bData[4];
	} u;

	// Construct the MIDI message. 

	u.bData[0] = 0xb0 | (midi_channel_number&0x0f);  // MIDI status byte 
	u.bData[1] = controller_num;   // first MIDI data byte 
	u.bData[2] = controller_val;   // second MIDI data byte 
	u.bData[3] = 0x00;

	// Send the message. 
	err = midiOutShortMsg(hmo, u.dwData);
	return err;
	//using the streaming capability is problematic in terms of unallocating hdr asynchroniously
	/*err = midiOutPrepareHeader(hmo, &hdr, sizeof(hdr));
	if (err)
		return err;
	err = midiOutUnprepareHeader(hmo, &hdr, sizeof(hdr));
	if (err)
		return err;*/

}
