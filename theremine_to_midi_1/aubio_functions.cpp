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

