#pragma once

#ifndef APP_SAMPLE_TYPE
#define APP_SAMPLE_TYPE float
#endif

#include <exception>
#include <string>
#include <iostream>

#if !DONT_POLLUTE_ME_WITH_ASIOH
#include "asio.h"
#endif

namespace al{

#if DONT_POLLUTE_ME_WITH_ASIOH
#include "asio.h"
#endif

class ASIO_Exception : std::runtime_error
{
public:
	ASIOError error_code;
	char *description;
	ASIO_Exception(ASIOError _error_code,char *_description)
		: std::runtime_error(std::string(_description))
	{
		description = _description;
		error_code = error_code;
	}
};

static const int ASIO_DRIVER_DEFAULT = 0;

class AsioListenerManager
{
	enum initialization_stages { driver_loaded, driver_initialized, static_data_initialized, buffers_created, asio_started };
	bool initialization_successful[5];
	static const int initialization_stages_count=5;
	void ResetInitializationStages();
	void TerminateDriver();
public:
	bool IsAlreadyListening();
	void StartListening(double buffer_time_length,char* asio_driver_name=ASIO_DRIVER_DEFAULT,bool print_driver_information=false);
	void StopListeningAndTerminate();
	unsigned int GetNumberOfInputChannels();
	AsioListenerManager();
	~AsioListenerManager();
	void PrintDriverInformation(std::ostream &output_stream=std::cout);
	double GetSampleRate();
	unsigned int NumberOfSamplesPerDeltaT(double delta_t);
	unsigned int GetCircBufferSize();
	unsigned int GetYoungestSampleTimeMilliseconds();
	void GetLast_N_Samples(unsigned int channel_number,
		APP_SAMPLE_TYPE* buff,unsigned int number_of_samples_to_retrieve,
		unsigned int* number_of_samples_retrieved,
		long *last_time_milliseconds);
	void PrintDebugInformation(std::ostream& output_stream);
};

enum InitErrorCode : long
{
	iec_setting_sample_rate_failed = -6,
	iec_cant_set_sample_rate_to_newone = -5,
	iec_cant_get_current_sample_rate = -3,
	iec_cant_get_channels = -1,
	iec_cant_get_buffer_size = -2,
	iec_success=0,
};

}
