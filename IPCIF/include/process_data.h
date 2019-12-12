#ifndef _PROC_DATA_H_
#define _PROC_DATA_H_
#include "common.h"
#include "mutex.h"
#include <vector>
#include <string>
using namespace std;
struct proc_data;
struct if_proc
{
	void* hif;
	string id;
	string usage;
	uint cnt;
	int prior;
	proc_data* pdata;
};
struct proc_data
{
	void* hproc;
	void* hthrd_shelter;
	string name;
	string cmdline;
	void* id;
	bool ambiguous;
	vector<if_proc> ifproc;
};
struct if_control_block
{
	cmutex& m;
	vector<proc_data>& pdata;
	if_control_block(cmutex& _m,vector<proc_data>& p):m(_m),pdata(p)
	{
	}
};
inline void init_if_proc(if_proc* ifproc)
{
	ifproc->hif=NULL;
	ifproc->cnt=0;
	ifproc->prior=0;
	ifproc->pdata=NULL;
}
inline void init_proc_data(proc_data* pdata)
{
	pdata->hproc=NULL;
	pdata->hthrd_shelter=NULL;
	pdata->id=NULL;
	pdata->ambiguous=false;
}
#endif

