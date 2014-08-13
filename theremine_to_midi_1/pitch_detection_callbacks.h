#error "dont use it"

#pragma once

#include "aubio_functions.h"

namespace pdc{

	static const int PITCH_ANALYSIS_MUTEX_WAIT_TIME = 10;

	struct AubioPitchDetectionConfig
	{
		char *method;
		//	abf::uint_t winsize;
		//	abf::uint_t hopsize;
		//abf::uint_t sr;
		abf::smpl_t silence_level;
		double preffered_buffer_time_seconds;
	};

	struct AubioPitchDetectionConfigInternal
	{
		char *method;
		abf::uint_t winsize;
		abf::uint_t hopsize;
		abf::uint_t sr;
		abf::smpl_t silence_level;
	};

	bool GetLastAnalysisResults(
		int channel_number
		, abf::smpl_t *pitch
		, abf::smpl_t *level_lin
		, long* last_time_milliseconds);
	AubioPitchDetectionConfigInternal
		GetAubioPitchDetectionConfigInternal()
	{
			return my_aubio_pitch_detection_config;
		}

	class PitchAndLevelAnalysis : public virtual Lockable
	{
		abf::smpl_t last_pitch, last_level;
		long last_time_milliseconds;
		abf::AubioPitch *my_aubio_pitch;
	public:
		PitchAndLevelAnalysis(const AubioPitchDetectionConfigInternal &config);
		void PerformAnalysis(abf::FVecClass& fvec, long current_time_milliseconds);
		void GetPitchAndLevel(abf::smpl_t* pitch, abf::smpl_t* level, long* time_milliseconds);
		~PitchAndLevelAnalysis();
	} **channel_analyzers;

	PitchAndLevelAnalysis::PitchAndLevelAnalysis(const AubioPitchDetectionConfigInternal &config)
	{
		my_aubio_pitch = new abf::AubioPitch(config.method,
			config.winsize,
			config.hopsize,
			config.sr,
			config.silence_level);
		last_time_milliseconds = 0;
		last_pitch = 0;
		last_level = 0;
	}

	PitchAndLevelAnalysis::~PitchAndLevelAnalysis()
	{
		delete my_aubio_pitch;
	}

	void PitchAndLevelAnalysis::PerformAnalysis(abf::FVecClass &fvec, long current_time_milliseconds)
	{
		last_pitch = my_aubio_pitch->AubioPitchDo(fvec);
		last_level = abf::aubio_level_lin(fvec);
		last_time_milliseconds = current_time_milliseconds;
	}

	void PitchAndLevelAnalysis::GetPitchAndLevel(abf::smpl_t *pitch, abf::smpl_t *level, long *time_milliseconds)
	{
		*pitch = last_pitch; *level = last_level; *time_milliseconds = last_time_milliseconds;
	}

	bool AsioListenerManager::GetLastAnalysisResults(int channel_number, abf::smpl_t *pitch, abf::smpl_t *level, long *time_milliseconds)
	{
		PitchAndLevelAnalysis *this_channel_analyzer = channel_analyzers[channel_number];

		if (!this_channel_analyzer->LockAccess(PITCH_ANALYSIS_MUTEX_WAIT_TIME))
		{
			std::cerr << "couldn't gain access to aubio pitch analyzer" << std::endl;
			return false;
		}
		this_channel_analyzer->GetPitchAndLevel(pitch, level, time_milliseconds);
		this_channel_analyzer->UnlockAccess();
		return true;
	}
}