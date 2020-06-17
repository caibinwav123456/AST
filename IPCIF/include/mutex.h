#ifndef _MUTEX_H_
#define _MUTEX_H_
#include "common.h"
#include "utility.h"
#define GATE_VALVE 128
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
		sys_close_sem(mutex);
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
		lock=false;
		main_mutex=sys_create_sem(0,1,NULL);
		mutex=sys_create_sem(0,GATE_VALVE,NULL);
		protect=sys_create_sem(1,1,NULL);
		if(!(VALID(main_mutex)&&VALID(mutex)&&VALID(protect)))
			goto fail;
		return;
	fail:
		if(VALID(main_mutex))
			sys_close_sem(main_mutex);
		if(VALID(mutex))
			sys_close_sem(mutex);
		if(VALID(protect))
			sys_close_sem(protect);
		EXCEPTION(ERR_SEM_CREATE_FAILED);
	}
	~gate()
	{
		sys_close_sem(mutex);
		sys_close_sem(protect);
		sys_close_sem(main_mutex);
	}
	int cut_down(bool cut,uint time=0)
	{
		if((cut&&lock)||((!cut)&&(!lock)))
			return 0;
		int origin_cnt;
		sys_wait_sem(protect);
		origin_cnt=cnt;
		if(cut)
		{
			lock=true;
		}
		else
		{
			lock=false;
			for(int i=0;i<-cnt;i++)
			{
				sys_signal_sem(mutex);
			}
		}
		sys_signal_sem(protect);
		if(cut&&origin_cnt<0)
		{
			int ret=sys_wait_sem(main_mutex,time);
			if(ret==ERR_TIMEOUT)
			{
				cut_down(false);
				return ret;
			}
		}
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
			origin_cnt=(++cnt);
			if(origin_lock&&origin_cnt==0)
			{
				sys_signal_sem(main_mutex);
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
	int cnt;
	bool lock;
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