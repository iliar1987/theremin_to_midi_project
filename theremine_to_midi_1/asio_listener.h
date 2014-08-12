#pragma once

//won't be using circular buffers anymore
#define USING_CIRCULAR_BUFFERS 0
#define HAVE_TIME_TRACKER 0
#define ANALYZE_AT_CALLBACK_TIME 1

#ifndef APP_SAMPLE_TYPE
#define APP_SAMPLE_TYPE float
#endif

#include "asio_hacks.h"


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

	static char * const ASIO_DEFAULT_DRIVER_NAME = "Generic Low Latency ASIO Driver";

	typedef unsigned long SAMPLE_NUMBER_TYPE;
#if ANALYZE_AT_CALLBACK_TIME
	class AsioCallbackHandler
	{
	public:
		virtual void AsioCallback(
			unsigned long time_milliseconds
			, unsigned long current_sample_number
			, unsigned long number_of_samples_in_buffer
			, ah::GenericBuffer &buffer) = 0;
		/*friend void SetCallbackHandler(int channel_number
			, AsioCallbackHandler &handler);*/
	};
	
	void SetCallbackHandler(int channel_number
		, AsioCallbackHandler &handler);
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

class ASIO_DriverError : std::runtime_error
{
public:
	ASIO_DriverError(char* msg)
		:std::runtime_error(msg)
	{}
};

class ASIO_SampleRateChanged_Exception : ASIO_DriverError
{
public:
	const ASIOSampleRate new_sample_rate;
	ASIO_SampleRateChanged_Exception(ASIOSampleRate sRate)
		: ASIO_DriverError("Sample Rate Changed")
		, new_sample_rate(sRate)
	{}
};

class ASIO_ResetRequest_Exception : ASIO_DriverError
{
public:
	ASIO_ResetRequest_Exception()
		:ASIO_DriverError("Reset request by driver")
	{}
};

static char * const ASIO_DRIVER_DEFAULT = 0;

static const int CIRC_BUFFER_MUTEX_WAIT_TIME = 10;
static const int TIME_TRACKER_MUTEX_WAIT_TIME = 10;
static const int SMALL_MUTEX_WAIT_TIME = 10;
static const int LARGE_MUTEX_WAIT_TIME = 100;

class AsioListenerManager
{
	enum initialization_stages { driver_loaded, driver_initialized, static_data_initialized, buffers_created, asio_started };
	bool initialization_successful[5];
	static const int initialization_stages_count=5;
	void ResetInitializationStages();
	void TerminateDriver();

public:
	bool IsAlreadyListening();
	void StartListening(
		double buffer_time_length
		, const char* asio_driver_name = ASIO_DRIVER_DEFAULT
		, bool print_driver_information = false
		);
	void StopListeningAndTerminate();
	unsigned int GetNumberOfInputChannels();
	AsioListenerManager();
	~AsioListenerManager();
	void PrintDriverInformation(std::ostream &output_stream=std::cout);
	double GetSampleRate();
	unsigned int GetBufferSize();
	unsigned int NumberOfSamplesPerDeltaT(double delta_t);
#if HAVE_TIME_TRACKER
	unsigned int GetYoungestSampleTimeMilliseconds();
#endif
#if USING_CIRCULAR_BUFFERS
	unsigned int GetCircBufferSize();
	void GetLast_N_Samples(unsigned int channel_number,
		APP_SAMPLE_TYPE* buff,unsigned int number_of_samples_to_retrieve,
		unsigned int* number_of_samples_retrieved,
		long *last_time_milliseconds);
#endif
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
