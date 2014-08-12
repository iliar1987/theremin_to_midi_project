#include "windows.h"

#define DONT_POLLUTE_ME_WITH_ASIOH 0
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

#include "asio_hacks.h"

#include "Lockable.h"

abf::AubioPitch* aubio_pitch_p=0;
abf::FVecClass* fvec_p=0;
al::AsioListenerManager aslt;

//double sample_rate;
pltcc::PitchLevelToMidi * p_t=0;
bool should_stop = false;

#define GENERAL_PARAMS \
	((rj::OptionallyLogarithmic<abf::smpl_t>, pitch_detection_silence_level))\
	((double, preffered_window_size_seconds))\
	((std::string, pitch_detection_method))\
	((int,update_period_milliseconds))\
	((std::string,asio_driver_name))\
	((std::string,midi_device))

struct GeneralParamsStruct
{
	DEF_ALL_VARS(GENERAL_PARAMS)
	abf::uint_t actual_window_size;
	unsigned long buffer_size;
	double sample_rate;
	void SetParamsFromJsonFile(char *json_file_name);
} general_params;

void GeneralParamsStruct::SetParamsFromJsonFile(char *json_file_name)
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
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(GENERAL_PARAMS, jsonobj)
}


abf::smpl_t current_pitch=0;
abf::smpl_t current_level=0;
unsigned long last_time_milliseconds=0;
al::SAMPLE_NUMBER_TYPE last_sample_number=0;

static const int PITCH_ANALYSIS_MUTEX_WAIT_TIME = 10;
Lockable analysis_locker;

class PitchCalculationCallbackHandler : public al::AsioCallbackHandler
{
public:
	void AsioCallback(unsigned long time_milliseconds
		, unsigned long current_sample_number
		, unsigned long number_of_samples_in_buffer
		, ah::GenericBuffer &buffer);
} pitch_calculation_callback_handler;

void PitchCalculationCallbackHandler::AsioCallback(unsigned long time_milliseconds
	, unsigned long current_sample_number
	, unsigned long number_of_samples_in_buffer
	, ah::GenericBuffer &gbuffer)
{
	if (number_of_samples_in_buffer != general_params.buffer_size)
	{
		std::cerr << "buffer size mismatch" << std::endl;
		throw std::runtime_error("buffer size mismatch");
	}
	if (!fvec_p)
		return;
	analysis_locker.LockAccess(PITCH_ANALYSIS_MUTEX_WAIT_TIME);
	abf::smpl_t *buff = fvec_p->GetBuff();
	double normalization_constant = gbuffer.GetNormalizationConstant();
	for (unsigned int i = 0; i < general_params.buffer_size; i++)
	{
		buff[i] = static_cast<abf::smpl_t>(
			static_cast<double>((*gbuffer.GetElement(i)))
			* normalization_constant);
	}
	current_pitch = aubio_pitch_p->AubioPitchDo(*fvec_p);
	current_level = aubio_pitch_p->GetLevel();
	last_time_milliseconds = time_milliseconds;
	last_sample_number=current_sample_number;
	analysis_locker.UnlockAccess();
}

void DestroyAll()
{
	if (aubio_pitch_p) delete aubio_pitch_p;
	if (fvec_p) delete fvec_p;
	if (p_t) delete(p_t);
}

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

void InitializePitchDetection(GeneralParamsStruct &params)
{
	fvec_p = new abf::FVecClass(params.buffer_size);
	params.actual_window_size = 
		abf::FloorClosestMultipleOfTwo(
		params.preffered_window_size_seconds * params.sample_rate);
	while (params.actual_window_size < params.buffer_size)
		params.actual_window_size *= 2;
	aubio_pitch_p = new abf::AubioPitch(const_cast<char*>(params.pitch_detection_method.c_str())
		, params.actual_window_size
		, params.buffer_size
		, params.sample_rate
		, params.pitch_detection_silence_level.GetDB());
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "You must supply parameters json file name as first argument" << std::endl;
		return 1;
	}
	char* parameter_file_name = argv[1];

	general_params.SetParamsFromJsonFile(parameter_file_name);
	std::cout.precision(3);
	std::cout.setf(std::ios::fixed, std::ios::floatfield);
	const char* driver_name;
	if (general_params.asio_driver_name == "default")
		driver_name = al::ASIO_DRIVER_DEFAULT;
	else
		driver_name = general_params.asio_driver_name.c_str();
	aslt.StartListening(1.0
		//al::ASIO_DRIVER_DEFAULT
		, driver_name
		, true);
	//aslt.PrintDriverInformation();
	general_params.sample_rate = aslt.GetSampleRate();
	general_params.buffer_size = aslt.GetBufferSize();
	std::cout << "sample rate: " << general_params.sample_rate
		<< "\tsamples per 100 millisec: " << aslt.NumberOfSamplesPerDeltaT(0.1)
		<< "\tbuffer size: " << general_params.buffer_size
#if USING_CIRCULAR_BUFFERS
		<< "\nCircular buffer size: " << aslt.GetCircBufferSize()
#endif
		<< std::endl;
	int &sleep_period = general_params.update_period_milliseconds;
	InitializePitchDetection(general_params);
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
	//InitializeDefaultPitchLevelToMidi(p);
	try{
		p.FromJsonFile(parameter_file_name);
	}
	catch (std::runtime_error &re)
	{
		std::cerr << re.what() << std::endl;
		return 2;
	}
	p.Start(1);
	//for (int i = 0; i < 50; i++)
	al::SetCallbackHandler(0, pitch_calculation_callback_handler);
	std::cout << std::endl;
	while (1)
	{
		Sleep(sleep_period);
		analysis_locker.LockAccess(sleep_period/2);
		float pitch = current_pitch;
		float level_linear = current_level;
		float level_db_spl = lin_to_db(level_linear);
		unsigned long time_milliseconds = last_time_milliseconds;
		analysis_locker.UnlockAccess();
#if USING_CIRCULAR_BUFFERS
		long last_time_milliseconds 
		unsigned int samples_retrieved;
		aslt.GetLast_N_Samples(0, fvec.GetBuff(), fvec.GetLength(), &samples_retrieved, &last_time_milliseconds);
		std::cout << "\n";
		std::cout << (double)last_time_milliseconds / 1000.0
			<< "\t" << samples_retrieved;
		
		pitch = aubio_pitch.AubioPitchDo(fvec);
		level_db_spl = abf::aubio_db_spl(fvec);
		level_linear = abf::aubio_level_lin(fvec);
#endif
		std::cout << "\r";
		std::cout << (double)time_milliseconds / 1000.0;
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
		outf << time_milliseconds
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

