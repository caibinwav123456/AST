#ifndef _PROCESS_TRACKER_H_
#define _PROCESS_TRACKER_H_
#include "common.h"
#include "interface.h"
#include "mutex.h"
#include "process_data.h"
#define lock_track() mlock tlock(process_tracker::get_mutex())
struct if_control_block
{
	cmutex& m;
	vector<proc_data>& pdata;
	bool& quit;
	if_control_block(cmutex& _m,vector<proc_data>& p,bool& q):m(_m),pdata(p),quit(q)
	{
	}
};
class process_tracker
{
public:
	process_tracker();
	static cmutex* get_mutex()
	{
		return &if_mutex;
	}
	proc_data* get_data(int i)
	{
		return &pdata[i];
	}
	vector<proc_data>* get_data()
	{
		return &pdata;
	}
	int init();
	int suspend_all(bool bsusp);
	void exit();
	if_control_block* get_ctrl_blk()
	{
		return &cblk;
	}
private:
	bool quit;
	vector<proc_data> pdata;
	if_control_block cblk;
	static cmutex if_mutex;
	static int threadfunc(void* param);
};

#endif