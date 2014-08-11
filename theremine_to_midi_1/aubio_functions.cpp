#include <iostream>
#include <conio.h>

#include <Windows.h>

#include "aubio_functions.h"

using namespace abf;

#include <cmath>

#define PI 3.14159265

unsigned int abf::FloorClosestMultipleOfTwo(unsigned int value)
{
	return (unsigned int)std::round(std::pow(2.0, std::floor(std::log2(value))));
}

void abf::print_buf(fvec_t* v,std::ostream &output_stream)
{
	unsigned int i;
	for (i = 0; i<v->length; i++)
	{
		std::cout << fvec_get_sample(v, i) << std::endl;
		if (i % 40 == 0 && i>0)
			;//_getch();
	}
}

abf::AubioPitch::AubioPitch(char *method,
	uint_t winsize, uint_t hopsize, uint_t sr,
	float silence_level)
{
	try
	{
		my_pitch = new_aubio_pitch(method, winsize, hopsize, sr);
		SetSilence(silence_level);
	}
	catch (...) { my_pitch = 0; }
}

abf::AubioPitch::~AubioPitch()
{
	if (my_pitch) del_aubio_pitch(my_pitch);
}

float abf::AubioPitch::AubioPitchDo(fvec_t* buff)
{
	fvec_t* pitch_return;
	pitch_return = new_fvec(1);
	aubio_pitch_do(my_pitch, buff, pitch_return);
	float pitch = pitch_return->data[0];
	del_fvec(pitch_return);
	return pitch;
}

class AubioCleanerUpper
{
public:
	~AubioCleanerUpper() { aubio_cleanup(); }
} aubio_cleaner_upper;

using namespace abf;

//Make foul here:
//copy paste from pitch.c

namespace abf_foul_play{

	/** pitch detection algorithms */
	typedef enum
	{
		aubio_pitcht_yin,        /**< `yin`, YIN algorithm */
		aubio_pitcht_mcomb,      /**< `mcomb`, Multi-comb filter */
		aubio_pitcht_schmitt,    /**< `schmitt`, Schmitt trigger */
		aubio_pitcht_fcomb,      /**< `fcomb`, Fast comb filter */
		aubio_pitcht_yinfft,     /**< `yinfft`, Spectral YIN */
		aubio_pitcht_specacf,    /**< `specacf`, Spectral autocorrelation */
		aubio_pitcht_default
		= aubio_pitcht_yinfft, /**< `default` */
	} aubio_pitch_type;

	/** pitch detection output modes */
	typedef enum
	{
		aubio_pitchm_freq,   /**< Frequency (Hz) */
		aubio_pitchm_midi,   /**< MIDI note (0.,127) */
		aubio_pitchm_cent,   /**< Cent */
		aubio_pitchm_bin,    /**< Frequency bin (0,bufsize) */
		aubio_pitchm_default = aubio_pitchm_freq, /**< the one used when "default" is asked */
	} aubio_pitch_mode;

	/** callback to get pitch candidate, defined below */
	typedef void(*aubio_pitch_detect_t) (aubio_pitch_t * p, fvec_t * ibuf, fvec_t * obuf);

	/** callback to convert pitch from one unit to another, defined below */
	typedef smpl_t(*aubio_pitch_convert_t) (smpl_t value, uint_t samplerate, uint_t bufsize);

	/** callback to fetch the confidence of the algorithm */
	typedef smpl_t(*aubio_pitch_get_conf_t) (void * p);

	/** generic pitch detection structure */
	struct _aubio_pitch_t
	{
		aubio_pitch_type type;          /**< pitch detection mode */
		aubio_pitch_mode mode;          /**< pitch detection output mode */
		uint_t samplerate;              /**< samplerate */
		uint_t bufsize;                 /**< buffer size */
		void *p_object;                 /**< pointer to pitch object */
		aubio_filter_t *filter;         /**< filter */
		aubio_pvoc_t *pv;               /**< phase vocoder for mcomb */
		cvec_t *fftgrain;               /**< spectral frame for mcomb */
		fvec_t *buf;                    /**< temporary buffer for yin */
		aubio_pitch_detect_t detect_cb; /**< callback to get the pitch candidates */
		aubio_pitch_convert_t conv_cb;  /**< callback to convert it to the desired unit */
		aubio_pitch_get_conf_t conf_cb; /**< pointer to the current confidence callback */
		smpl_t silence;                 /**< silence threshold */
	};
}

abf::smpl_t AubioPitch::GetLevel()
{
	//at most algorithms i've seen the buf variable contains all measurement per this window
	abf_foul_play::_aubio_pitch_t* _my_pitch = 
		reinterpret_cast<abf_foul_play::_aubio_pitch_t*>(my_pitch);
	return abf::aubio_level_lin(_my_pitch->buf);
}

