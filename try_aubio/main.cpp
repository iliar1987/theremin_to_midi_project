#include <iostream>
#include <conio.h>

#include <Windows.h>

#include "build\src\config.h"
//#define HAVE_MEMCPY_HACKS 0
#include "src/aubio.h"
//#include <stdio.h>

#include <cmath>

#define PI 3.14159265

void print_buf(fvec_t* v)
{
	int i;
	for ( i=0 ; i<v->length ; i++ )
	{
		std::cout << fvec_get_sample(v,i) << std::endl;
		if ( i%40 == 0 && i>0 )
			;//_getch();
	}
}

#include "src\pitch\pitchfcomb.h"
#include "src\aubio_priv.h"

typedef struct
{
	smpl_t bin;
	smpl_t db;
} aubio_fpeak_t;

struct _aubio_pitchfcomb_t
{
	uint_t fftSize;
	uint_t stepSize;
	uint_t rate;
	fvec_t *winput;
	fvec_t *win;
	cvec_t *fftOut;
	fvec_t *fftLastPhase;
	aubio_fft_t *fft;
};


int main()
{
	/*HINSTANCE result = LoadLibrary(TEXT("libaubio-4.dll"));
	cout<<result;
	if ( result != NULL)
	{
		FreeLibrary(result);
	}*/
	
	// set window size, and sampling rate
	double T = 1.0;
	float freq = 111.0;

	uint_t winsize, sr = 44100;
	winsize = 8192;
	uint_t hopsize = 700;

	// create a vector
	fvec_t *this_buffer = new_fvec (hopsize);
	fvec_t *pitch_buf = new_fvec(1);

	aubio_pitch_t *pitch_analyzer = new_aubio_pitch("default",
		winsize,hopsize,sr);

	aubio_pitchfcomb_t* Q = reinterpret_cast<aubio_pitchfcomb_t*>( pitch_analyzer);
	
	//fvec_zeros(this_buffer);
	// create the a-weighting filter
	
	// here some code to put some data in this_buffer
	// ...
	int i;
	float relative_i;
	float t_i;
	for ( i = 0 ; i < this_buffer->length ; i++ )
	{
		relative_i = float(i)/this_buffer->length;
		t_i = float(i)/sr;
		fvec_set_sample(this_buffer,std::sin(2 * PI * freq * t_i),i);
	}
	//fvec_t *bkup_buffer = new_fvec(this_buffer->length);
	//fvec_copy(this_buffer,bkup_buffer);

	// apply the filter, in place

	//aubio_filter_t *this_filter = new_aubio_filter_a_weighting (sr);
	//aubio_filter_do (this_filter, this_buffer);

	
	
	aubio_pitch_do(pitch_analyzer,this_buffer,pitch_buf);

	print_buf(pitch_buf);
	_getch();
	print_buf(this_buffer);
	// here some code to get some data from this_buffer
	// ...
	/*for ( i=0 ; i<this_buffer->length ; i++ )
	{
		std::cout << this_buffer->data[i] << "\t" << bkup_buffer->data[i] << std::endl;
		if ( i%100 == 0)
			_getch();
	}*/

	// and free the structures
	//del_aubio_filter (this_filter);
	try{del_aubio_pitch(pitch_analyzer);
		del_fvec(pitch_buf);
	//del_fvec(bkup_buffer);
	del_fvec (this_buffer);
	}
	catch(...)
	{}
	aubio_cleanup();


	_getch();
	return 0;
}
