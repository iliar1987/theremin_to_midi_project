#pragma once

#include <iostream>

namespace abf{

#include "build\src\config.h"

#include "src/aubio.h"
//#include "src/musicutils.h"
	
	const int number_of_pitch_methods = 6;

	static char* possible_pitch_methods[] =
	{ "default",
	"schmitt",
	"fcomb",
	"mcomb",
	"yin",
	"yinfft" };

	void print_buf(fvec_t* v, std::ostream &output_stream = std::cout);

	class FVecClass
	{
		fvec_t* my_fvec = 0;
	public:
		FVecClass(uint_t vec_size)
		{
			try{
				my_fvec = new_fvec(vec_size);
			}
			catch (...) { my_fvec = 0; }
		}
		~FVecClass() { if (my_fvec) del_fvec(my_fvec); }
		uint_t GetLength() { return my_fvec->length; }
		smpl_t* GetBuff() { return my_fvec->data; }
		fvec_t* GetMyFVecObject() { return my_fvec; }
	};

	class AubioPitch
	{
		aubio_pitch_t* my_pitch=0;
	public:
		AubioPitch(char *method,
			uint_t winsize, uint_t hopsize, uint_t sr);
		~AubioPitch();
		float AubioPitchDo(fvec_t* buffer);
		float AubioPitchDo(FVecClass &fvec) { return AubioPitchDo(fvec.GetMyFVecObject()); }
		aubio_pitch_t* GetMyPitchObject() { return my_pitch; }
	};

	inline smpl_t aubio_db_spl(FVecClass &fvec) { return aubio_db_spl(fvec.GetMyFVecObject()); }
	
	inline smpl_t aubio_level_lin(FVecClass &fvec) { return aubio_level_lin(fvec.GetMyFVecObject()); }

	unsigned int FloorClosestMultipleOfTwo(unsigned int value);
}
