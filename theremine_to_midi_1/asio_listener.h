#pragma once

//won't be using circular buffers anymore
#define USING_CIRCULAR_BUFFERS 0
#define ANALYZE_AT_CALLBACK_TIME 1

#if USING_CIRCULAR_BUFFERS
#ifndef APP_SAMPLE_TYPE
#define APP_SAMPLE_TYPE float
#endif
#endif

#if ANALYZE_AT_CALLBACK_TIME
#include "aubio_functions.h"
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

class ASIO_SampleRateChanged_Exception : std::runtime_error
{
public:
	const ASIOSampleRate new_sample_rate;
	ASIO_SampleRateChanged_Exception(ASIOSampleRate sRate)
		: std::runtime_error("Sample Rate Changed")
		, new_sample_rate(sRate)
	{}
};

static const int ASIO_DRIVER_DEFAULT = 0;

static const int CIRC_BUFFER_MUTEX_WAIT_TIME = 10;
static const int TIME_TRACKER_MUTEX_WAIT_TIME = 10;
static const int PITCH_ANALYSIS_MUTEX_WAIT_TIME = 10;


#if ANALYZE_AT_CALLBACK_TIME
struct AubioPitchDetectionConfig
{
	char *method;
//	abf::uint_t winsize;
//	abf::uint_t hopsize;
	//abf::uint_t sr;
	abf::smpl_t silence_level;
	float window_to_block_size_ratio;
};
struct AubioPitchDetectionConfigInternal
{
	char *method;
	abf::uint_t winsize;
	abf::uint_t hopsize;
	abf::uint_t sr;
	abf::smpl_t silence_level;
};
#endif

class AsioListenerManager
{
	enum initialization_stages { driver_loaded, driver_initialized, static_data_initialized, buffers_created, asio_started };
	bool initialization_successful[5];
	static const int initialization_stages_count=5;
	void ResetInitializationStages();
	void TerminateDriver();
#if ANALYZE_AT_CALLBACK_TIME
	AubioPitchDetectionConfigInternal my_aubio_pitch_detection_config;
#endif
public:
	bool IsAlreadyListening();
	void StartListening(
		double buffer_time_length
		,char* asio_driver_name
		,bool print_driver_information
#if ANALYZE_AT_CALLBACK_TIME
		,const AubioPitchDetectionConfig& aubio_pitch_config
#endif
		);
	void StopListeningAndTerminate();
	unsigned int GetNumberOfInputChannels();
	AsioListenerManager();
	~AsioListenerManager();
	void PrintDriverInformation(std::ostream &output_stream=std::cout);
	double GetSampleRate();
	unsigned int NumberOfSamplesPerDeltaT(double delta_t);
	unsigned int GetYoungestSampleTimeMilliseconds();
#if USING_CIRCULAR_BUFFERS
	unsigned int GetCircBufferSize();
	void GetLast_N_Samples(unsigned int channel_number,
		APP_SAMPLE_TYPE* buff,unsigned int number_of_samples_to_retrieve,
		unsigned int* number_of_samples_retrieved,
		long *last_time_milliseconds);
#endif
#if ANALYZE_AT_CALLBACK_TIME
	bool GetLastAnalysisResults(
		int channel_number
		,abf::smpl_t *pitch
		, abf::smpl_t *level_lin
		, long* last_time_milliseconds);
	AubioPitchDetectionConfigInternal
		GetAubioPitchDetectionConfigInternal()
	{
		return my_aubio_pitch_detection_config;
	}
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
