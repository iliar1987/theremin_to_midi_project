#include <Windows.h>

#include <exception>
#include <system_error>
#include "Lockable.h"

inline std::error_code GetLastWindowsError() { return std::error_code(GetLastError(), std::generic_category()); }

Lockable::Lockable()
{
	is_being_deleted=false;
#if USE_MUTEX
	my_mutex = CreateMutex(NULL, FALSE, NULL);
	if (my_mutex == NULL)
#else
	if (!InitializeCriticalSectionAndSpinCount(&my_cs, 5))
#endif
	{
		//error occured
		throw std::system_error(GetLastWindowsError(), "Some error happened while initializing mutex");
	}
}

bool Lockable::PrivateLock(unsigned int time_out)
{
#if USE_MUTEX
	DWORD wait_result;
	wait_result = WaitForSingleObject(my_mutex, time_out);
	switch (wait_result)
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return true;
	case WAIT_FAILED:
		/*throw std::system_error(GetLastWindowsError()
			, "some error happened while waiting for mutex");*/
	case WAIT_TIMEOUT:
		return false;
	};
#else
	EnterCriticalSection(&my_cs);
	return true;
#endif
}

#include <exception>

Lockable::~Lockable()
{
#if USE_MUTEX
	if (my_mutex == NULL) return;
#endif
	is_being_deleted = true;
	if (!PrivateLock(VERY_LONG_WAITING_TIME))
		throw std::runtime_error("Couldn't destroy mutex upon destruction of Lockable object");
	UnlockAccess();
#if USE_MUTEX
	CloseHandle(my_mutex);
#else
	DeleteCriticalSection(&my_cs);
#endif
}

#if USE_MUTEX

bool Lockable::LockAccess(unsigned int time_out)
{
	if ( is_being_deleted ) return false;
	DWORD wait_result;
	wait_result = WaitForSingleObject(my_mutex, time_out);
	switch (wait_result)
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return true;
	case WAIT_FAILED:
		throw std::system_error(GetLastWindowsError()
			, "some error happened while waiting for mutex");
	case WAIT_TIMEOUT:
		return false;
	};
}
void Lockable::UnlockAccess()
{
	if (!ReleaseMutex(my_mutex))
	{
		//error occured
		throw std::system_error(GetLastWindowsError(), "Some error while releasing mutex.");
	}
}

#else

bool Lockable::LockAccess(unsigned int time_out)
{
	if (is_being_deleted) return false;
	EnterCriticalSection(&my_cs);
	return true;
}
void Lockable::UnlockAccess()
{
	LeaveCriticalSection(&my_cs);
}
#endif

//int FairCriticalSection::HowManyInQueue()
//{
//	internal_lock.LockAccess(0); 
//	int size = locks.size();
//	internal_lock.UnlockAccess();
//}
//
//bool FairCriticalSection::LockAccess()
//{
//#if USE_MUTEX
//#error "mutex not implemented"
//#endif
//	internal_lock.LockAccess(0);
//	locks.
//	internal_lock.UnlockAccess();
//	return true;
//}
//
//void 
