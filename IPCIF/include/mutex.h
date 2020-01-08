#ifndef _MUTEX_H_
#define _MUTEX_H_
#include "common.h"
#include "utility.h"
class cmutex
{
public:
	cmutex()
	{
		mutex = sys_create_sem(1, 1, NULL);
		if (!VALID(mutex))
			EXCEPTION(ERR_GENERIC);
	}
	~cmutex()
	{
		sys_close_sem(mutex);
	}
	void* get_mutex()
	{
		return mutex;
	}
private:
	void* mutex;
};
class mlock
{
public:
	mlock(cmutex* l)
	{
		lock = l;
		sys_wait_sem(lock->get_mutex());
	}
	~mlock()
	{
		sys_signal_sem(lock->get_mutex());
	}
private:
	cmutex* lock;
};
#endif