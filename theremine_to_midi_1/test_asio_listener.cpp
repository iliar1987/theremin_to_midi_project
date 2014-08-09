#include "windows.h"

#include "asio.h"

#include "asio_listener.h"

#include <iostream>

int main(int argc, char* argv[])
{
	al::AsioListenerManager aslt;

	std::cout.precision(3);
	std::cout.setf(std::ios::fixed, std::ios::floatfield);
	aslt.StartListening(1.0, 0, true);
	//aslt.PrintDriverInformation();
	std::cout << "sample rate: " << aslt.GetSampleRate()
		<< "\tsamples per 100 millisec: " << aslt.NumberOfSamplesPerDeltaT(0.1)
		<< "\nCircular buffer size: " << aslt.GetCircBufferSize()
		<< std::endl;
	const int buff_size = 500;
	APP_SAMPLE_TYPE buff[buff_size];
	unsigned int samples_retrieved;
	long last_time_milliseconds;
	for (int i = 0; i < 5; i++){
		Sleep(100);
		aslt.GetLast_N_Samples(0, buff, buff_size, &samples_retrieved, &last_time_milliseconds);
		std::cout << (double)last_time_milliseconds / 1000.0
			<< "\t" << samples_retrieved << std::endl;
		std::cout << (double)aslt.GetYoungestSampleTimeMilliseconds() / 1000.0 << std::endl;
	}
	aslt.PrintDebugInformation(std::cout);
	aslt.StopListeningAndTerminate();

	return 0;
}

