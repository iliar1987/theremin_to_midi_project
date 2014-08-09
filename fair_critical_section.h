#include <list>
#include <windows.h>

class FairCriticalSection
{
	HANDLE internal_cs;
	std::list<HANDLE> cs_handles;
public:
	FairCriticalSection();
	~FairCriticalSection();
	bool Lock(unsigned int time_out);
	void UnLock();
};
