/*
You must link with winmm.lib. If using Visual C++, go to Build->Settings. Flip to the Link page,
and add winmm.lib to the library/object modules.

This app plays a musical phrase from the song "Twinkle, Twinkle Little Star". It sends the MIDI
events to the default MIDI Out device using the Stream API. The Stream API does the actual timing
out of each MIDI event. Therefore we don't have to use a multi-media timer nor delays.
*/

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <mmsystem.h>





/* Here's an array of MIDI Note-On and Note-Off events to play our musical phrase. We declare the
array as an array of unsigned longs. Makes it easier to statically declare the MIDIEVENT structures
in an array.

I've set the timing fields in terms of PPQN clocks, as they would be found in MIDI File using
96 PPQN Time Division.

NOTES: The first event is a System Exclusive event to set Master Volume to full. Note that the high
byte of the dwEvent field (ie, the 3rd unsigned long) must be MEVT_LONGMSG, and the remaining bytes
must be the count of how many SysEx bytes follow. Also note that the data must be padded out to a
doubleword boundary. That's implicit in our declaration of this as an unsigned long array.

The second event is a Tempo event which sets tempo to 500,000 microseconds per quarter
note. I put this event at the start of the array so that I don't have to use midiStreamProperty()
to set tempo prior to playback. Note that the high byte of dwEvent must be MEVT_TEMPO to tell the
stream device that this is a Tempo event.

Also, note that the MIDI event is packed into a long the same way that it would be to send to
midiOutShortMsg().
*/

unsigned long Phrase[] = {0, 0, ((unsigned long)MEVT_LONGMSG<<24) | 8, 0x047F7FF0, 0xF77F7F01,
0, 0, ((unsigned long)MEVT_TEMPO<<24) | 0x0007A120,
0, 0, 0x007F3C90,
48, 0, 0x00003C90,
0, 0, 0x007F3C90,
48, 0, 0x00003C90,

0, 0, 0x007F4390,
48, 0, 0x00004390,
0, 0, 0x007F4390,
48, 0, 0x00004390,

0, 0, 0x007F4590,
48, 0, 0x00004590,
0, 0, 0x007F4590,
48, 0, 0x00004590,

0, 0, 0x007F4390,
86, 0, 0x00004390,

10, 0, 0x007F4190,
48, 0, 0x00004190,
0, 0, 0x007F4190,
48, 0, 0x00004190,

0, 0, 0x007F4090,
48, 0, 0x00004090,
0, 0, 0x007F4090,
48, 0, 0x00004090,

0, 0, 0x007F3E90,
48, 0, 0x00003E90,
0, 0, 0x007F3E90,
48, 0, 0x00003E90,

0, 0, 0x007F3C90,
96, 0, 0x00003C90};




/*********************** PrintMidiOutErrorMsg() **************************
 * Retrieves and displays an error message for the passed MIDI Out error
 * number. It does this using midiOutGetErrorText().
 *************************************************************************/

void PrintMidiOutErrorMsg(unsigned long err)
{
#define BUFFERSIZE 120
	char	buffer[BUFFERSIZE];
	
	if (!(err = midiOutGetErrorText(err, &buffer[0], BUFFERSIZE)))
	{
		printf("%s\r\n", &buffer[0]);
	}
	else if (err == MMSYSERR_BADERRNUM)
	{
		printf("Strange error number returned!\r\n");
	}
	else
	{
		printf("Specified pointer is invalid!\r\n");
	}
}





/* ******************************** main() ******************************** */

int main(int argc, char **argv)
{
	HMIDISTRM		handle;
	MIDIHDR			midiHdr;
	unsigned long	err, tempo;
	MIDIPROPTIMEDIV prop;
	UINT dev_num;

	/* Open default MIDI Out stream device */
	err = 0;
	dev_num=1;

	if (!(err = midiStreamOpen(&handle, &dev_num, 1, 0, 0, CALLBACK_NULL)))
	{
		/* Set the stream device's Time Division to 96 PPQN */
		prop.cbStruct = sizeof(MIDIPROPTIMEDIV);
		prop.dwTimeDiv = 96;
		err = midiStreamProperty(handle, (LPBYTE)&prop, MIDIPROP_SET|MIDIPROP_TIMEDIV);
		if (err)
		{
			PrintMidiOutErrorMsg(err);
		}

		/* Store pointer to our stream (ie, buffer) of messages in MIDIHDR */
		midiHdr.lpData = (LPBYTE)&Phrase[0];

		/* Store its size in the MIDIHDR */
		midiHdr.dwBufferLength = midiHdr.dwBytesRecorded = sizeof(Phrase);

		/* Flags must be set to 0 */
		midiHdr.dwFlags = 0;

		/* Prepare the buffer and MIDIHDR */
		err = midiOutPrepareHeader(handle,  &midiHdr, sizeof(MIDIHDR));
	    if (!err)
		{
	        /* Queue the Stream of messages. Output doesn't actually start
			until we later call midiStreamRestart().
			*/
			err = midiStreamOut(handle, &midiHdr, sizeof(MIDIHDR));
			if (err)
			{
bad:			PrintMidiOutErrorMsg(err);
				midiOutUnprepareHeader(handle, &midiHdr, sizeof(MIDIHDR));
			}
			else
			{
		        /* Start outputting the Stream of messages. This should return immediately
				as the stream device will time out and output the messages on its own in
				the background.
				*/
			    err = midiStreamRestart(handle);
				if (err)
				{
					goto bad;
				}

				// The current (default) tempo
				tempo = 0x0007A120;

				/* Wait for playback to stop, and then unprepare the buffer and MIDIHDR */
				while (MIDIERR_STILLPLAYING == midiOutUnprepareHeader(handle, &midiHdr, sizeof(MIDIHDR)))
				{
					/* Show how to change the Tempo in real-time. When user presses up or down
					arrows, raise or drop the tempo */
					while(_kbhit())
					{
						MIDIPROPTEMPO	tprop;
						int				chr;

						chr = _getch();
						if (chr == 0xE0)
						{
							chr = _getch();
							if (chr == 0x48)
							{
								if (tempo > 50000) tempo -= 50000;
							}
							if (chr == 0x50)
							{
								if (tempo < (0x00FFFFFF - 50000)) tempo += 50000;
							}

							tprop.cbStruct = sizeof(MIDIPROPTEMPO);
							tprop.dwTempo = tempo;
							err = midiStreamProperty(handle, (LPBYTE)&tprop, MIDIPROP_SET|MIDIPROP_TEMPO);
							if (err)
							{
								PrintMidiOutErrorMsg(err);
							}
						}
					}

					/* Delay to give playback time to finish. Normally, you wouldn't do this loop.
					You'd instead specify a callback function to midiOpenStream() and do the
					midiOutUnprepareHeader and midiStreamClose after Windows called your callback
					to indicate that the stream was done playing. But here I'm just interested in
					a quick and dirty example...
					*/
					Sleep(1000);
				}
			}
		}

		/* An error. Print a message */
		else
		{
			PrintMidiOutErrorMsg(err);
		}

	     /* Close the MIDI Stream */
		 midiStreamClose(handle);
	}
	else
	{
		printf("Error opening the default MIDI Stream!\r\n");
		PrintMidiOutErrorMsg(err);
	}

	return(0);
}