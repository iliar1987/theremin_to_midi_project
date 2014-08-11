#include "windows.h"

#define DONT_POLLUTE_ME_WITH_ASIOH 1
#include "asio_listener.h"

#include <iostream>
#include <fstream>

#include "aubio_functions.h"

#include <cassert>
#include <conio.h>

#include "midi_sender.h"

#include "pitch_and_level_to_controller_converter.h"

using namespace pltcc;

#include <memory>
#include <exception>

#include "json_struct_initializer.h"

abf::AubioPitch* aubio_pitch_p=0;
abf::FVecClass* fvec_p=0;
al::AsioListenerManager aslt;
pltcc::PitchLevelToMidi * p_t=0;

void DestroyAll()
{
	if (aubio_pitch_p) delete aubio_pitch_p;
	if (fvec_p) delete fvec_p;
	if (p_t) delete(p_t);
}

bool should_stop = false;
float silence_level;

#include <vector>

class KeyStrokeTracker
{
	std::vector<char> key_seq;
public:
	void key_pressed(char key)
	{
		if (key_seq.empty())
		{
			switch (key)
			{
			case 'q':
				should_stop = true;
				break;
			case 't':
				key_seq.push_back('t');
				break;
			}
		}
		else if (key_seq.size() == 1 && key_seq[0] == 't')
		{
			switch (key)
			{
			case 'p':
				p_t->send_pitch = !p_t->send_pitch;
				key_seq.clear();
				break;
			case 'l':
				p_t->send_level = !p_t->send_level;
				key_seq.clear();
				break;
			}
		}
		
	}
} key_stroke_tracker;

void SetParamsFromJsonFile(char *json_file_name)
{
	FILE* f;
	errno_t err = fopen_s(&f, json_file_name, "r");
	if (err)
	{
		std::cerr << "error opening file " << err << std::endl;
	}
	rj::FileStream fs(f);
	rj::Document jsonobj;
	jsonobj.ParseStream(fs);
	EXTRACT_FROM_JSON_OBJ(float, silence_level, jsonobj)
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "You must supply parameters json file name as first argument" << std::endl;
		return 1;
	}
	char* parameter_file_name = argv[1];

	SetParamsFromJsonFile(parameter_file_name);
	std::cout.precision(3);
	std::cout.setf(std::ios::fixed, std::ios::floatfield);
	aslt.StartListening(1.0, al::ASIO_DRIVER_DEFAULT, true);
	//aslt.PrintDriverInformation();
	unsigned int sample_rate = aslt.GetSampleRate();
	std::cout << "sample rate: " << sample_rate
		<< "\tsamples per 100 millisec: " << aslt.NumberOfSamplesPerDeltaT(0.1)
#if USING_CIRCULAR_BUFFERS
		<< "\nCircular buffer size: " << aslt.GetCircBufferSize()
#endif
		<< std::endl;
	const unsigned int sleep_period = 50;
#if USING_CIRCULAR_BUFFERS
	assert(sizeof(APP_SAMPLE_TYPE) == sizeof(abf::smpl_t));
//#endif
	const unsigned int buff_size =
		aslt.NumberOfSamplesPerDeltaT(sleep_period / 1000.0);
	//APP_SAMPLE_TYPE buff[buff_size];
	
//#if USING_CIRCULAR_BUFFERS
	abf::FVecClass& fvec = *(
		fvec_p = new abf::FVecClass(buff_size));
	unsigned int win_size = abf::FloorClosestMultipleOfTwo(buff_size * 2);
	char method[] = "fcomb";
	
	abf::AubioPitch& aubio_pitch = *(
		aubio_pitch_p = new 
			abf::AubioPitch(
			method, win_size, buff_size
			, sample_rate
			,silence_level)
		);
#endif

#define RECORDING 0
#if RECORDING
	std::ofstream outf("pitch_recognition_output3.txt",std::ios_base::app);
	outf << "theremine input" << std::endl;
	outf << "sample rate: " << sample_rate
		<< ",method: " << method
		<< ",buff_size: " << buff_size
		<< ",win_size: " << win_size
		<< std::endl;
	outf << "time_ms,pitch_hz,level_db_spl,level_lin" << std::endl;
#endif
	
	PitchLevelToMidi &p = *(p_t = new PitchLevelToMidi());
	p.Start(1);
	//InitializeDefaultPitchLevelToMidi(p);
	try{
		p.FromJsonFile(parameter_file_name);
	}
	catch (std::runtime_error &re)
	{
		std::cerr << re.what() << std::endl;
		return 2;
	}
	//for (int i = 0; i < 50; i++)
	while (1)
	{
		Sleep(sleep_period);
		float pitch;
		float level_db_spl, level_linear;
#if USING_CIRCULAR_BUFFERS
		long last_time_milliseconds;
		unsigned int samples_retrieved;
		aslt.GetLast_N_Samples(0, fvec.GetBuff(), fvec.GetLength(), &samples_retrieved, &last_time_milliseconds);
		std::cout << "\n";
		std::cout << (double)last_time_milliseconds / 1000.0
			<< "\t" << samples_retrieved;
		
		pitch = aubio_pitch.AubioPitchDo(fvec);
		level_db_spl = abf::aubio_db_spl(fvec);
		level_linear = abf::aubio_level_lin(fvec);
#endif
		std::cout << "\t" << "Pitch: "
			<< pitch;
		std::cout << "\t" << "Level(db): "
			<< level_db_spl;
		//std::cout.setf(std::ios::scientific | std::ios::dec | std::ios::floatfield);
		/*std::cout << "\t" << "Level(l): "
			<< level_linear;*/
		printf("\tLevel(l): %.3g",level_linear);
		//std::cout.unsetf(std::ios::scientific);
#if RECORDING
		outf << last_time_milliseconds
			<< "," << pitch
			<< "," << level_db_spl
			<< "," << level_linear
			<< std::endl;
#endif
		
		p.ConvertAndSend(pitch,level_linear);

		if (_kbhit())
		{
			key_stroke_tracker.key_pressed(_getch());
			if (should_stop)
				break;
		}
	}
	p.Stop();
	aslt.PrintDebugInformation(std::cout);
	aslt.StopListeningAndTerminate();

	DestroyAll();
	return 0;
}

