#ifndef _MUTEX_H_
#define _MUTEX_H_
#include "common.h"
#include "utility.h"
#include <assert.h>
#define GATE_VALVE 128
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
class gate
{
public:
	gate()
	{
		cnt=0;
		cnt_unlock=0;
		lock=false;
		timeout=false;
		main_mutex=sys_create_sem(0,1,NULL);
		mutex=sys_create_sem(0,GATE_VALVE,NULL);
		protect=sys_create_sem(1,1,NULL);
		cut_protect=sys_create_sem(1,1,NULL);
		if(!(VALID(main_mutex)&&VALID(mutex)&&VALID(protect)&&VALID(cut_protect)))
			goto fail;
		return;
	fail:
		sem_safe_release(main_mutex);
		sem_safe_release(mutex);
		sem_safe_release(protect);
		sem_safe_release(cut_protect);
		EXCEPTION(ERR_SEM_CREATE_FAILED);
	}
	~gate()
	{
		sem_safe_release(main_mutex);
		sem_safe_release(mutex);
		sem_safe_release(protect);
		sem_safe_release(cut_protect);
	}
	int cut_down(bool cut,uint time=0)
	{
		if(cut)
			sys_wait_sem(cut_protect);
		if((cut&&lock)||((!cut)&&(!lock)))
			return 0;
		int origin_cnt;
		sys_wait_sem(protect);
		origin_cnt=cnt;
		if(cut)
		{
			lock=true;
			cnt_unlock=origin_cnt;
		}
		else
		{
			lock=false;
			for(int i=0;i<cnt_unlock-cnt;i++)
			{
				sys_signal_sem(mutex);
			}
			if(timeout&&cnt_unlock==0)
			{
				sys_wait_sem(main_mutex);
			}
			cnt_unlock=0;
			timeout=false;
		}
		sys_signal_sem(protect);
		if(cut&&origin_cnt<0)
		{
			int ret=sys_wait_sem(main_mutex,time);
			if(ret==ERR_TIMEOUT)
			{
				timeout=true;
				cut_down(false);
				return ret;
			}
		}
		if(!cut)
			sys_signal_sem(cut_protect);
		return 0;
	}
	void get_lock(bool bget)
	{
		int origin_cnt;
		bool origin_lock;
	start:
		sys_wait_sem(protect);
		origin_lock=lock;
		if(bget)
		{
			origin_cnt=(--cnt);
			if(cnt<-GATE_VALVE)
			{
				cnt=-GATE_VALVE;
				sys_signal_sem(protect);
				sys_sleep(10);
				goto start;
			}
		}
		else
		{
			assert(cnt<0);
			origin_cnt=(++cnt);
			if(origin_lock)
			{
				assert(cnt_unlock<0);
				int origin_cnt_unlock=(++cnt_unlock);
				if(origin_cnt_unlock==0)
				{
					sys_signal_sem(main_mutex);
				}
			}
		}
		sys_signal_sem(protect);
		if(bget&&origin_lock)
		{
			sys_wait_sem(mutex);
		}
	}
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