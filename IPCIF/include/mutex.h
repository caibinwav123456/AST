#ifndef _MUTEX_H_
#define _MUTEX_H_
#include "common.h"
#include "utility.h"
#include <assert.h>
#define sem_safe_release(sem) \
	if(VALID(sem)) \
	{ \
		sys_close_sem(sem); \
		sem=NULL; \
	}
class cmutex
{
public:
	cmutex()
	{
		mutex=sys_create_sem(1,1,NULL);
		if(!VALID(mutex))
			EXCEPTION(ERR_SEM_CREATE_FAILED);
	}
	~cmutex()
	{
		sem_safe_release(mutex);
	}
	void* get_mutex()
	{
		return mutex;
	}
private:
	void* mutex;
};
class DLL gate
{
public:
	gate();
	~gate();
	int cut_down(bool cut,uint time=0);
	void get_lock(bool bget);
private:
	void* main_mutex;
	void* mutex;
	void* protect;
	void* cut_protect;
	int cnt;
	int cnt_unlock;
	bool lock;
	bool timeout;
};
class mlock
{
public:
	mlock(gate* l)
	{
		lock=l;
		lock->cut_down(true);
	}
	~mlock()
	{
		lock->cut_down(false);
	}
private:
	gate* lock;
};
#endif