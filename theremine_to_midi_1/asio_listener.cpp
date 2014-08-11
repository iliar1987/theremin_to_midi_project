// hostsample.cpp : a simple ASIO host example.
// - instantiates the driver
// - get the information from the driver
// - built up some audio channels
// - plays silence for 20 seconds
// - destruct the driver
// Note: This sample cannot work with the "ASIO DirectX Driver" as it does
//       not have a valid Application Window handle, which is used as sysRef
//       on the Windows platform.

#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

#include "asio_listener.h"

#include <stdio.h>
#include <string.h>
#include <conio.h>

#include <Windows.h>

#define AH_DEBUG
#define USE_INT64 1
#include "asio_hacks.h"

#include <map>
#include <memory>
#include <string>
#include <iostream>
#include <exception>

typedef long INDEX_TYPE;

#include "Lockable.h"

//using namespace asio;
// name of the ASIO device to be used
//#if WINDOWS
//	#define ASIO_DRIVER_NAME    "ASIO Multimedia Driver"
//	#define ASIO_DRIVER_NAME    "ASIO Sample"

//#elif MAC
////	#define ASIO_DRIVER_NAME   	"Apple Sound Manager"
//	#define ASIO_DRIVER_NAME   	"ASIO Sample"
//#endif

enum {
	// number of input and outputs supported by the host application
	// you can change these to higher or lower values
	kMaxInputChannels = 32,
	kMaxOutputChannels = 32
};

using namespace al;

inline std::error_code GetLastWindowsError() { return std::error_code(GetLastError(), std::generic_category()); }

// internal data storage
typedef struct DriverInfo
{
	// ASIOInit()
	ASIODriverInfo driverInfo;

	// ASIOGetChannels()
	long           inputChannels;
	long           outputChannels;

	// ASIOGetBufferSize()
	long           minSize;
	long           maxSize;
	long           preferredSize;
	long           granularity;

	// ASIOGetSampleRate()
	ASIOSampleRate sampleRate;

	// ASIOOutputReady()
	bool           postOutput;

	// ASIOGetLatencies ()
	long           inputLatency;
	long           outputLatency;

	// ASIOCreateBuffers ()
	long inputBuffers;	// becomes number of actual created input buffers
	long outputBuffers;	// becomes number of actual created output buffers
	ASIOBufferInfo bufferInfos[kMaxInputChannels + kMaxOutputChannels]; // buffer info's

	// ASIOGetChannelInfo()
	ASIOChannelInfo channelInfos[kMaxInputChannels + kMaxOutputChannels]; // channel info's
	// The above two arrays share the same indexing, as the data in them are linked together

	// Information from ASIOGetSamplePosition()
	// data is converted to double floats for easier use, however 64 bit integer can be used, too
	NANOSECONDS_T         nanoSeconds;
	
	NANOSECONDS_T         samples;
	NANOSECONDS_T         tcSamples;	// time code samples

	// bufferSwitchTimeInfo()
	ASIOTime       tInfo;			// time info state
	unsigned long  sysRefTime;      // system reference time, when bufferSwitch() was called

	// Signal the end of processing in this example
	bool           stopped;
} DriverInfo;


DriverInfo asioDriverInfo = {0};
ASIOCallbacks asioCallbacks;

//----------------------------------------------------------------------------------
// some external references
extern AsioDrivers* asioDrivers;
bool loadAsioDriver(char *name);

// internal prototypes (required for the Metrowerks CodeWarrior compiler)
int main(int argc, char* argv[]);
al::InitErrorCode init_asio_static_data (DriverInfo *asioDriverInfo);
ASIOError create_asio_buffers (DriverInfo *asioDriverInfo);
unsigned long get_sys_reference_time();


// callback prototypes
void bufferSwitch(long index, ASIOBool processNow);
ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow);
void sampleRateChanged(ASIOSampleRate sRate);
long asioMessages(long selector, long value, void* message, double* opt);

//void InitializeCircularBuffers();
//void DeleteCircularBuffers();

//Won't be using circular buffers anymore
#if USING_CIRCULAR_BUFFERS
#include "circular_buffer\circular.h"
#endif

#include "Lockable.h"

#define DEBUGGING_MULTITHREADING 1

#if ANALYZE_AT_CALLBACK_TIME

Lockable callback_handlers_container_lock;

std::map<int,AsioCallbackHandler*> callback_handlers;

void al::SetCallbackHandler(int channel_number
	, al::AsioCallbackHandler &handler)
{
	callback_handlers_container_lock.LockAccess(100);
	callback_handlers[channel_number] = &handler;
	callback_handlers_container_lock.UnlockAccess();
}

#endif

unsigned int AsioListenerManager::GetBufferSize()
{
	return asioDriverInfo.preferredSize;
}

#if USING_CIRCULAR_BUFFERS
class CircularBufferManager : public virtual Lockable
{
public:
	typedef NANOSECONDS_T SAMPLE_NUMBER_TYPE;
private:
	cb::circular_buffer<APP_SAMPLE_TYPE> my_circular_buffer;
	bool last_time_valid;
	NANOSECONDS_T last_time;
	SAMPLE_NUMBER_TYPE last_sample_number;
	void ResetAll()
	{
		my_circular_buffer.clear();
		last_time_valid = false;
		last_time = 0;
		last_sample_number = 0;
	}
	void HickupDetected()
	{
		std::cerr << "Hickup detected" << std::endl;
		ResetAll();
	}
	void ChokeDetected()
	{
		std::cerr << "Choke detected" << std::endl;
		ResetAll();
	}
public:
	bool GetLastTime(/*out*/ NANOSECONDS_T *asio_time) {
		if ( !last_time_valid ) return false;
		else {*asio_time = last_time; return true;}
	}
	SAMPLE_NUMBER_TYPE GetLastSampleNumber() { return last_sample_number; }
	void GetLast_N_Samples(APP_SAMPLE_TYPE* out_buff, unsigned int out_buff_size, unsigned int *samples_retrieved);
	unsigned int GetCircBufferSize() {return my_circular_buffer.size();}
	unsigned int GetCircBufferMaxSize() { return my_circular_buffer.max_size(); }
	/*bool LockAccess(unsigned int time_out);
	void UnlockAccess();*/
	void InsertSamples(ah::GenericBuffer& in_buffer, unsigned int in_buffer_length, NANOSECONDS_T time, SAMPLE_NUMBER_TYPE in_sample_number);
	CircularBufferManager(unsigned int buffer_size)
		: my_circular_buffer(buffer_size)
	{
		last_time_valid = false;
		last_sample_number = 0;
		last_time = 0;
	}
};

class CircularBufferManagersMap : public std::map<INDEX_TYPE, CircularBufferManager*>
{
	unsigned int max_sizes;
public:
	void InitializeCircularBuffers(unsigned int buffer_size)
	{
		for (int i = 0; i < asioDriverInfo.outputBuffers + asioDriverInfo.inputBuffers; i++)
		if (asioDriverInfo.bufferInfos[i].isInput)
		{
			(*this)[i] = new CircularBufferManager(buffer_size);
		}
		max_sizes = buffer_size;
	}

	void DeleteCircularBuffers()
	{
		for (iterator it = begin(); it != end(); it++)
		{
			delete(it->second);
		}
		this->clear();
	}

	unsigned int GetCircBufferMaxSizes()
	{
		return max_sizes;
	}
	void ResetCircularBuffers()
	{
		DeleteCircularBuffers();
		InitializeCircularBuffers(max_sizes);
	}
} these_circular_buffer_managers;

void CircularBufferManager::InsertSamples(ah::GenericBuffer& in_buffer, unsigned int in_buffer_length, NANOSECONDS_T time, SAMPLE_NUMBER_TYPE in_sample_number)
{
	if (last_time_valid && last_sample_number > in_sample_number) //A hickup occured
		HickupDetected();
	else if (last_time_valid
		&& ((in_sample_number - in_buffer_length)
		!= last_sample_number))
	{
		std::cerr << in_sample_number
			<< " =? "
			<< last_sample_number
			<< " + "
			<< in_buffer_length
			<< std::endl;
		ChokeDetected();
	}
	last_time_valid = true;
	last_time = time;
	last_sample_number = in_sample_number;
	double normalization_constant = in_buffer.GetNormalizationConstant();
	for (unsigned int inbuf_ind = 0 ; inbuf_ind < in_buffer_length ; inbuf_ind++ )
	{
		//int this_elem = *gbuff->GetElement(inbuf_ind);
		my_circular_buffer.push_back( (APP_SAMPLE_TYPE)
			((double)(*(in_buffer.GetElement(inbuf_ind))) * normalization_constant));
		//sum += this_elem;
	}
}

void CircularBufferManager::GetLast_N_Samples(APP_SAMPLE_TYPE* out_buff, unsigned int out_buff_size, unsigned int *samples_retrieved)
{
	unsigned int total_to_retrieve;
	total_to_retrieve = out_buff_size > my_circular_buffer.size()
		? my_circular_buffer.size()
		: out_buff_size;
	cb::circular_buffer<APP_SAMPLE_TYPE>::reverse_iterator it = my_circular_buffer.rbegin();
	for (unsigned int i = 0 ; i < total_to_retrieve; i++,it++)
	{
		out_buff[i] = *it;
	}
	*samples_retrieved = total_to_retrieve;
}
//
//bool CircularBufferManager::LockAccess(unsigned int time_out)
//{
//	DWORD wait_result;
//	wait_result = WaitForSingleObject(my_mutex, time_out);
//	switch (wait_result)
//	{
//	case WAIT_ABANDONED:
//	case WAIT_OBJECT_0:
//		return true;
//	case WAIT_FAILED:
//		throw std::system_error(GetLastWindowsError()
//			, "some error happened while waiting for the circular buffer mutex");
//	case WAIT_TIMEOUT:
//		return false;
//	};
//}
//void CircularBufferManager::UnlockAccess()
//{
//	if (!ReleaseMutex(my_mutex))
//	{
//		//error occured
//		throw std::system_error(GetLastWindowsError(), "Some error while releasing mutex of circular buffer manager.");
//	}
//}

#endif

#if HAVE_TIME_TRACKER
class LastTimeTracker : public virtual Lockable
{
	NANOSECONDS_T last_time;
public:
	NANOSECONDS_T GetLastTime() { return last_time; }
	void SetLastTime(NANOSECONDS_T time) { last_time = time; }
} last_time_tracker;

#endif
//----------------------------------------------------------------------------------
al::InitErrorCode init_asio_static_data (DriverInfo *asioDriverInfo)
{	// collect the informational data of the driver
	// get the number of available channels
	if(ASIOGetChannels(&asioDriverInfo->inputChannels, &asioDriverInfo->outputChannels) == ASE_OK)
	{
		printf ("ASIOGetChannels (inputs: %d, outputs: %d);\n", asioDriverInfo->inputChannels, asioDriverInfo->outputChannels);

		// get the usable buffer sizes
		if(ASIOGetBufferSize(&asioDriverInfo->minSize, &asioDriverInfo->maxSize, &asioDriverInfo->preferredSize, &asioDriverInfo->granularity) == ASE_OK)
		{
			printf ("ASIOGetBufferSize (min: %d, max: %d, preferred: %d, granularity: %d);\n",
					 asioDriverInfo->minSize, asioDriverInfo->maxSize,
					 asioDriverInfo->preferredSize, asioDriverInfo->granularity);

			// get the currently selected sample rate
			if(ASIOGetSampleRate(&asioDriverInfo->sampleRate) == ASE_OK)
			{
				printf ("ASIOGetSampleRate (sampleRate: %f);\n", asioDriverInfo->sampleRate);
				if (asioDriverInfo->sampleRate <= 0.0 || asioDriverInfo->sampleRate > 96000.0)
				{
					// Driver does not store it's internal sample rate, so set it to a know one.
					// Usually you should check beforehand, that the selected sample rate is valid
					// with ASIOCanSampleRate().
					if(ASIOSetSampleRate(44100.0) == ASE_OK)
					{
						if(ASIOGetSampleRate(&asioDriverInfo->sampleRate) == ASE_OK)
							printf ("ASIOGetSampleRate (sampleRate: %f);\n", asioDriverInfo->sampleRate);
						else
							return al::iec_setting_sample_rate_failed;
					}
					else
						return al::iec_cant_set_sample_rate_to_newone;
				}

				// check wether the driver requires the ASIOOutputReady() optimization
				// (can be used by the driver to reduce output latency by one block)
				if(ASIOOutputReady() == ASE_OK)
					asioDriverInfo->postOutput = true;
				else
					asioDriverInfo->postOutput = false;
				printf ("ASIOOutputReady(); - %s\n", asioDriverInfo->postOutput ? "Supported" : "Not supported");

				return al::iec_success;
			}
			return al::iec_cant_get_current_sample_rate;
		}
		return al::iec_cant_get_buffer_size;
	}
	return al::iec_cant_get_channels;
}

int out_counter;
int in_counter;
static short maximum_simultanious_calls = 0;

ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.
	static Lockable hickup_lockable;
#if DEBUGGING_MULTITHREADING
	static short simultanious_call_counter = 0;
	simultanious_call_counter++;
	
	if (simultanious_call_counter > maximum_simultanious_calls)
	{
		maximum_simultanious_calls = simultanious_call_counter;
	}
#endif
	AutoLocker hickup_auto_locker(hickup_lockable, CIRC_BUFFER_MUTEX_WAIT_TIME);
	static long processedSamples = 0;

	// store the timeInfo for later use
	asioDriverInfo.tInfo = *timeInfo;

	// get the system reference time
	asioDriverInfo.sysRefTime = get_sys_reference_time();

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	NANOSECONDS_T this_time;
	if (timeInfo->timeInfo.flags & kSystemTimeValid)
	{
		this_time = ASIO64toNanoSeconds(timeInfo->timeInfo.systemTime);
		asioDriverInfo.nanoSeconds = this_time;
	}
	else
	{
		this_time = get_sys_reference_time() * 1000000L;
		asioDriverInfo.nanoSeconds = 0;
	}
	
#if USING_CIRCULAR_BUFFERS
	CircularBufferManager::SAMPLE_NUMBER_TYPE this_sample_number;
#else
	SAMPLE_NUMBER_TYPE this_sample_number;
#endif
	if (timeInfo->timeInfo.flags & kSamplePositionValid)
	{
		this_sample_number = ASIO64toNanoSeconds(timeInfo->timeInfo.samplePosition);
		asioDriverInfo.samples = this_sample_number;
	}
	else
	{
		asioDriverInfo.samples = 0;
		this_sample_number = 0;
	}

	if (timeInfo->timeCode.flags & kTcValid)
		asioDriverInfo.tcSamples = ASIO64toNanoSeconds(timeInfo->timeCode.timeCodeSamples);
	else
		asioDriverInfo.tcSamples = 0;

#if WINDOWS && _DEBUG && WRITE_TIME_DIFFS
	// a few debug messages for the Windows device driver developer
	// tells you the time when driver got its interrupt and the delay until the app receives
	// the event notification.
	static NANOSECONDS_T last_samples = 0;
	char tmp[128];
	sprintf (tmp, "diff: %d / %d ms / %d ms / %d samples                 \n", asioDriverInfo.sysRefTime - (long)(asioDriverInfo.nanoSeconds / 1000000.0), asioDriverInfo.sysRefTime, (long)(asioDriverInfo.nanoSeconds / 1000000.0), (long)(asioDriverInfo.samples - last_samples));
	OutputDebugString (tmp);
	last_samples = asioDriverInfo.samples;
#endif

	// buffer size in samples
	long buffSize = asioDriverInfo.preferredSize;

#if HAVE_TIME_TRACKER
	//lock time tracker for the duration of the processing
	bool time_locked;
	if (last_time_tracker.LockAccess(TIME_TRACKER_MUTEX_WAIT_TIME))
	{
		time_locked = true;
		last_time_tracker.SetLastTime(this_time);
	}
	else
		time_locked = false;
#endif
	// perform the processing
	for (int i = 0; i < asioDriverInfo.inputBuffers + asioDriverInfo.outputBuffers; i++)
	{
		if (asioDriverInfo.bufferInfos[i].isInput == ASIOTrue)
		{
			in_counter++;
			
#if ANALYZE_AT_CALLBACK_TIME
			ah::GenericBuffer* gbuff = ah::NewGenericBufferFromASIO_Sample_T(
				asioDriverInfo.bufferInfos[i].buffers[index],
				asioDriverInfo.channelInfos[i].type);
			callback_handlers_container_lock.LockAccess(LARGE_MUTEX_WAIT_TIME);
			if (callback_handlers.find(i) != callback_handlers.end())
			{
				callback_handlers[i]->AsioCallback(
					(unsigned long)this_time / 1000000,
					this_sample_number,
					buffSize,
					*gbuff);
				callback_handlers_container_lock.UnlockAccess();
			}
#endif
#if USING_CIRCULAR_BUFFERS
			std::shared_ptr<ah::GenericBuffer> gbuff = ah::NewGenericBufferFromASIO_Sample_T(
				asioDriverInfo.bufferInfos[i].buffers[index],
				asioDriverInfo.channelInfos[i].type);
			CircularBufferManager* this_circular_buffer_manager = these_circular_buffer_managers[i];
			if (!this_circular_buffer_manager->LockAccess(CIRC_BUFFER_MUTEX_WAIT_TIME)) //milliseconds
			{
				std::cerr << "circular buffer appears to be blocked" <<std::endl;
			}
			else
			{
				this_circular_buffer_manager->InsertSamples(*gbuff,buffSize,this_time,this_sample_number);
				this_circular_buffer_manager->UnlockAccess();
				//fprintf(stdout,"sum = %d",sum);
			}
#endif
		}
#if USING_OUTPUT_BUFFERS
		else
		{
			out_counter++;
			// OK do processing for the outputs only
			switch (asioDriverInfo.channelInfos[i].type)
			{
			case ASIOSTInt16LSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 2);
				break;
			case ASIOSTInt24LSB:		// used for 20 bits as well
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 3);
				break;
			case ASIOSTInt32LSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 8);
				break;

				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
			case ASIOSTInt32LSB16:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;

			case ASIOSTInt16MSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 2);
				break;
			case ASIOSTInt24MSB:		// used for 20 bits as well
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 3);
				break;
			case ASIOSTInt32MSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 8);
				break;

				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
			case ASIOSTInt32MSB16:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			}
		}
#endif
	}

#if HAVE_TIME_TRACKER
	if ( time_locked )
		last_time_tracker.UnlockAccess();
#endif
	// finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
	if (asioDriverInfo.postOutput)
		ASIOOutputReady();

	//if (processedSamples >= asioDriverInfo.sampleRate * TEST_RUN_TIME)	// roughly measured
	//	asioDriverInfo.stopped = true;
	//else
		processedSamples += buffSize;
#if DEBUGGING_MULTITHREADING
	simultanious_call_counter--;
#endif
	return 0L;
}

//----------------------------------------------------------------------------------
void bufferSwitch(long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.

	// as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
	// though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
	fprintf(stdout,"in buffer switch");
	ASIOTime  timeInfo;
	memset (&timeInfo, 0, sizeof (timeInfo));

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	if(ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
		timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

	bufferSwitchTimeInfo (&timeInfo, index, processNow);
}


//----------------------------------------------------------------------------------
void sampleRateChanged(ASIOSampleRate sRate)
{
	// do whatever you need to do if the sample rate changed
	// usually this only happens during external sync.
	// Audio processing is not stopped by the driver, actual sample rate
	// might not have even changed, maybe only the sample rate status of an
	// AES/EBU or S/PDIF digital input at the audio device.
	// You might have to update time/sample related conversion routines, etc.
	std::cerr << "sample rate changed" << std::endl;
	throw(ASIO_SampleRateChanged_Exception(sRate));
	asioDriverInfo.sampleRate = sRate;
}

//----------------------------------------------------------------------------------
long asioMessages(long selector, long value, void* message, double* opt)
{
	// currently the parameters "value", "message" and "opt" are not used.
	long ret = 0;
	switch(selector)
	{
		case kAsioSelectorSupported:
			if(value == kAsioResetRequest
			|| value == kAsioEngineVersion
			|| value == kAsioResyncRequest
			|| value == kAsioLatenciesChanged
			// the following three were added for ASIO 2.0, you don't necessarily have to support them
			|| value == kAsioSupportsTimeInfo
			|| value == kAsioSupportsTimeCode
			|| value == kAsioSupportsInputMonitor)
				ret = 1L;
			break;
		case kAsioResetRequest:
			// defer the task and perform the reset of the driver during the next "safe" situation
			// You cannot reset the driver right now, as this code is called from the driver.
			// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
			// Afterwards you initialize the driver again.
			std::cerr << "reset request" << std::endl;
			//throw(ASIO_ResetRequest_Exception());
			//asioDriverInfo.stopped;  // In this sample the processing will just stop
			ret = 1L;
			break;
		case kAsioResyncRequest:
			// This informs the application, that the driver encountered some non fatal data loss.
			// It is used for synchronization purposes of different media.
			// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
			// Windows Multimedia system, which could loose data because the Mutex was hold too long
			// by another thread.
			// However a driver can issue it in other situations, too.
			std::cerr << "resync request" << std::endl;
#if USING_CIRCULAR_BUFFERS
			these_circular_buffer_managers.ResetCircularBuffers();
#endif
			ret = 1L;
			break;
		case kAsioLatenciesChanged:
			// This will inform the host application that the drivers were latencies changed.
			// Beware, it this does not mean that the buffer sizes have changed!
			// You might need to update internal delay data.
			std::cerr << "latencies changed" << std::endl;
			{
				ASIOError result = ASIOGetLatencies(&asioDriverInfo.inputLatency, &asioDriverInfo.outputLatency);
				if (result == ASE_OK)
					printf("ASIOGetLatencies (input: %d, output: %d);\n", asioDriverInfo.inputLatency, asioDriverInfo.outputLatency);
			}
			ret = 1L;
			break;
		case kAsioEngineVersion:
			// return the supported ASIO version of the host application
			// If a host applications does not implement this selector, ASIO 1.0 is assumed
			// by the driver
			ret = 2L;
			break;
		case kAsioSupportsTimeInfo:
			// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
			// is supported.
			// For compatibility with ASIO 1.0 drivers the host application should always support
			// the "old" bufferSwitch method, too.
			ret = 1;
			break;
		case kAsioSupportsTimeCode:
			// informs the driver wether application is interested in time code info.
			// If an application does not need to know about time code, the driver has less work
			// to do.
			ret = 0;
			break;
	}
	return ret;
}


//----------------------------------------------------------------------------------
ASIOError create_asio_buffers (DriverInfo *asioDriverInfo)
{	// create buffers for all inputs and outputs of the card with the 
	// preferredSize from ASIOGetBufferSize() as buffer size
	long i;
	ASIOError result;

	// fill the bufferInfos from the start without a gap
	ASIOBufferInfo *info = asioDriverInfo->bufferInfos;

	// prepare inputs (Though this is not necessaily required, no opened inputs will work, too
	if (asioDriverInfo->inputChannels > kMaxInputChannels)
		asioDriverInfo->inputBuffers = kMaxInputChannels;
	else
		asioDriverInfo->inputBuffers = asioDriverInfo->inputChannels;
	for(i = 0; i < asioDriverInfo->inputBuffers; i++, info++)
	{
		info->isInput = ASIOTrue;
		info->channelNum = i;
		info->buffers[0] = info->buffers[1] = 0;
	}

	// prepare outputs
	if (asioDriverInfo->outputChannels > kMaxOutputChannels)
		asioDriverInfo->outputBuffers = kMaxOutputChannels;
	else
		asioDriverInfo->outputBuffers = asioDriverInfo->outputChannels;
	//Ilia: disable output
	asioDriverInfo->outputBuffers = 0;
	for(i = 0; i < asioDriverInfo->outputBuffers; i++, info++)
	{
		info->isInput = ASIOFalse;
		info->channelNum = i;
		info->buffers[0] = info->buffers[1] = 0;
	}

	// create and activate buffers
	result = ASIOCreateBuffers(asioDriverInfo->bufferInfos,
		asioDriverInfo->inputBuffers + asioDriverInfo->outputBuffers,
		asioDriverInfo->preferredSize, &asioCallbacks);
	if (result == ASE_OK)
	{
		// now get all the buffer details, sample word length, name, word clock group and activation
		for (i = 0; i < asioDriverInfo->inputBuffers + asioDriverInfo->outputBuffers; i++)
		{
			asioDriverInfo->channelInfos[i].channel = asioDriverInfo->bufferInfos[i].channelNum;
			asioDriverInfo->channelInfos[i].isInput = asioDriverInfo->bufferInfos[i].isInput;
			result = ASIOGetChannelInfo(&asioDriverInfo->channelInfos[i]);
			printf("channel %d name: %s type: %d\n", i, asioDriverInfo->channelInfos[i].name, asioDriverInfo->channelInfos[i].type);
			if (result != ASE_OK)
				break;
		}

		if (result == ASE_OK)
		{
			// get the input and output latencies
			// Latencies often are only valid after ASIOCreateBuffers()
			// (input latency is the age of the first sample in the currently returned audio block)
			// (output latency is the time the first sample in the currently returned audio block requires to get to the output)
			result = ASIOGetLatencies(&asioDriverInfo->inputLatency, &asioDriverInfo->outputLatency);
			if (result == ASE_OK)
				printf ("ASIOGetLatencies (input: %d, output: %d);\n", asioDriverInfo->inputLatency, asioDriverInfo->outputLatency);
		}
	}
	return result;
}

bool already_listening=false;

bool al::AsioListenerManager::IsAlreadyListening() {return already_listening;}

void al::AsioListenerManager::ResetInitializationStages()
{
	for (int i = 0; i < initialization_stages_count; i++)
		initialization_successful[i] = false;
}

al::AsioListenerManager::AsioListenerManager()
{
	ResetInitializationStages();
}

void al::AsioListenerManager::StopListeningAndTerminate() { already_listening = false; TerminateDriver(); }

al::AsioListenerManager::~AsioListenerManager() {StopListeningAndTerminate();}

#if HAVE_TIME_TRACKER

unsigned int al::AsioListenerManager::GetYoungestSampleTimeMilliseconds()
{
	//ASIOTime time;
	//bool result = asioDriverInfo.nanoSeconds;// ->GetLastTime(&time);
	//if ( !result ) return 0;
	//else
	//{
	//	return time.timeInfo.samplePosition;
	//}
	if (!last_time_tracker.LockAccess(TIME_TRACKER_MUTEX_WAIT_TIME))
		return 0;
	NANOSECONDS_T last_input_time = last_time_tracker.GetLastTime();
	//std::cerr << last_input_time << std::endl;
	double sr = GetSampleRate();
	last_input_time += (asioDriverInfo.preferredSize-1) / sr;
	last_input_time -= asioDriverInfo.inputLatency / sr;
	last_time_tracker.UnlockAccess();
	unsigned int retvalue = last_input_time / 1000000L;
	//std::cerr << retvalue << std::endl;
	return retvalue;
}
#endif

#if USING_CIRCULAR_BUFFERS
void al::AsioListenerManager::GetLast_N_Samples(unsigned int channel_number, 
	APP_SAMPLE_TYPE* buff, unsigned int number_of_samples_to_retrieve,
	unsigned int* number_of_samples_retrieved,
	long *last_time_milliseconds)
{
	CircularBufferManager* cb
		= these_circular_buffer_managers[channel_number];
	*last_time_milliseconds = GetYoungestSampleTimeMilliseconds();
	if (cb->LockAccess(CIRC_BUFFER_MUTEX_WAIT_TIME))
	{
		cb->GetLast_N_Samples
			(buff
			, number_of_samples_to_retrieve
			, number_of_samples_retrieved);
		cb->UnlockAccess();
	}
	else throw std::runtime_error("can't lock buffer");
}
#endif

void al::AsioListenerManager::PrintDebugInformation(std::ostream &output_stream)
{
	output_stream << "maximum number of simultaneous buffer switches: " << maximum_simultanious_calls << std::endl;
}

unsigned int al::AsioListenerManager::GetNumberOfInputChannels()
{
	return asioDriverInfo.inputChannels;
}

double al::AsioListenerManager::GetSampleRate()
{
	return asioDriverInfo.sampleRate;
}

unsigned int al::AsioListenerManager::NumberOfSamplesPerDeltaT(double delta_t)
{
	return static_cast<unsigned int>(delta_t * GetSampleRate());
}

#if USING_CIRCULAR_BUFFERS
unsigned int al::AsioListenerManager::GetCircBufferSize()
{
	return these_circular_buffer_managers.GetCircBufferMaxSizes();
}
#endif

void al::AsioListenerManager::TerminateDriver()
{
	if (initialization_successful[initialization_stages::asio_started])
		ASIOStop();
	if (initialization_successful[initialization_stages::buffers_created])
		ASIODisposeBuffers();
	if (initialization_successful[initialization_stages::static_data_initialized]
			|| initialization_successful[initialization_stages::driver_initialized] )
		ASIOExit();
	if (initialization_successful[initialization_stages::driver_loaded])
		asioDrivers->removeCurrentDriver();
	ResetInitializationStages();
	//DeleteCircularBuffers();
}

void al::AsioListenerManager::StartListening(double buffer_time_length,char* asio_driver_name,bool print_driver_information)
{
	if (IsAlreadyListening())
	{
		std::cerr<<"already listening"<<std::endl;
		throw ASIO_Exception(0,"already listening");
	}
	if ( asio_driver_name == ASIO_DRIVER_DEFAULT )
		asio_driver_name = ASIO_DEFAULT_DRIVER_NAME;
	if (!loadAsioDriver (asio_driver_name))
	{
		std::cerr<<"Couldn't load driver"<<std::endl;
		throw al::ASIO_Exception(0,"Couldn't load driver");
	}	
	else
	{
		// initialize the driver
		initialization_successful[initialization_stages::driver_loaded] = true;
		ASIOError init_result;
		if ((init_result = ASIOInit (&asioDriverInfo.driverInfo)) != ASE_OK)
		{
			std::cerr << "problem initializing driver. problem number: " << init_result << std::endl;
			TerminateDriver();
			throw al::ASIO_Exception(init_result,"problem initializing driver");
		}
		else
		{
			initialization_successful[initialization_stages::driver_initialized] = true;
			if (print_driver_information)
				PrintDriverInformation();
			al::InitErrorCode init_asio_static_data_result;
			if ((init_asio_static_data_result=init_asio_static_data (&asioDriverInfo)) != 0)
			{
				std::cerr << "problem initializing asio static data. problem number: " << (long)init_asio_static_data_result << std::endl;
				TerminateDriver();
				throw al::ASIO_Exception((long)(init_asio_static_data_result),"problem initializing asio static data");
			}
			else
			{
				initialization_successful[initialization_stages::static_data_initialized] = true;

				// ASIOControlPanel(); you might want to check wether the ASIOControlPanel() can open

				// set up the asioCallback structure and create the ASIO data buffer
				asioCallbacks.bufferSwitch = &bufferSwitch;
				asioCallbacks.sampleRateDidChange = &sampleRateChanged;
				asioCallbacks.asioMessage = &asioMessages;
				asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;
				ASIOError create_buffer_result;
				if ((create_buffer_result = create_asio_buffers (&asioDriverInfo)) != ASE_OK)
				{
					std::cerr << "can't initialize buffers. Error number: " << create_buffer_result << std::endl;
					TerminateDriver();
					throw al::ASIO_Exception(create_buffer_result,"can't initialize buffers");
				}
				else
				{
					initialization_successful[initialization_stages::buffers_created] = true;
					//initialize circual buffers before call to ASIOStart as it is in another thread
					already_listening = true;
#if USING_CIRCULAR_BUFFERS
					these_circular_buffer_managers.InitializeCircularBuffers(NumberOfSamplesPerDeltaT(buffer_time_length));
#endif
					ASIOError asio_start_result;
					if ((asio_start_result=ASIOStart()) != ASE_OK)
					{
						already_listening = false;
#if USING_CIRCULAR_BUFFERS
						these_circular_buffer_managers.DeleteCircularBuffers();
#endif
						std::cerr << "can't ASIOStart(). Error number: " << asio_start_result << std::endl;
						TerminateDriver();
						throw al::ASIO_Exception(asio_start_result,"can't ASIOStart()");
					}
					else
					{
						initialization_successful[initialization_stages::asio_started] = true;
					}
				}
			}
		}
	}
}

void al::AsioListenerManager::PrintDriverInformation(std::ostream &output_stream)
{
	output_stream
		<< "asioVersion:   "
		<< asioDriverInfo.driverInfo.asioVersion << std::endl
		<< "driverVersion: "
		<< asioDriverInfo.driverInfo.driverVersion << std::endl
		<< "Name: " << asioDriverInfo.driverInfo.name << std::endl
		<< "ErrorMessage: " << asioDriverInfo.driverInfo.errorMessage << std::endl;
}

unsigned long get_sys_reference_time()
{	// get the system reference time
#if WINDOWS
	return timeGetTime();
#elif MAC
static const double twoRaisedTo32 = 4294967296.;
	UnsignedWide ys;
	Microseconds(&ys);
	double r = ((double)ys.hi * twoRaisedTo32 + (double)ys.lo);
	return (unsigned long)(r / 1000.);
#endif
}
