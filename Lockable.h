#pragma once

#include <exception>

#ifndef USE_MUTEX
#define USE_MUTEX 0
#endif

static const int VERY_LONG_WAITING_TIME = 1000;

class Lockable
{
#if USE_MUTEX
	HANDLE my_mutex;
#else
	CRITICAL_SECTION my_cs;
#endif
	Lockable(const Lockable&);
protected:
	volatile bool is_being_deleted;
	virtual bool PrivateLock(unsigned int timeout);
public:
	virtual bool LockAccess(unsigned int time_out);
	virtual void UnlockAccess();
	Lockable();
	virtual ~Lockable();
};

class AutoLocker
{
	Lockable& my_lockable;
	bool success;
public:
	AutoLocker(Lockable &lockable, unsigned int time_out)
		: my_lockable(lockable) {
		success = my_lockable.LockAccess(time_out);
	}
	bool GetSuccessStatus() { return success; }
	~AutoLocker() { my_lockable.UnlockAccess(); }
};


//
//#include <queue>
//
//class FairCriticalSection : public Lockable
//{
//	Lockable internal_lock;
//	std::queue<Lockable> locks;
//public:
//	int HowManyInQueue();
//	bool LockAccess(unsigned int time_out);
//	void UnlockAccess();
//};
