#include "midi_sender.h"

#include <Windows.h>

#include <iostream>
#include <conio.h>

int main()
{
	midis::MidiOutStream mos;
	mos.OpenStream(1);
	//mos.StartStream();
	while (1)
	{
		Sleep(100);

		midis::midi_err_t err = mos.SendController(72, 24, 5);

		std::cerr << midis::GetMidiErrorMessage(err) << std::endl;

		if (_kbhit())
			break;
	}

	//mos.StopStream();
	mos.CloseStream();
	return 0;
}