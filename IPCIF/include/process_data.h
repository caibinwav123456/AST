#ifndef _PROC_DATA_H_
#define _PROC_DATA_H_
#include "common.h"
#include "mutex.h"
#include "fsdrv_interface.h"
#include "utility.h"
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
struct offset64
{
	uint off;
	uint offhigh;
};
struct storage_mod_info;
struct storage_drv_info;
struct if_info_storage
{
	string mount_cmd;
	string format_cmd;
	void* hdev;
	storage_drv_info* sto_drv;
	storage_mod_info* sto_mod;
	if_proc* ifproc;
};
struct storage_drv_info
{
	string drv_name;
	pintf_fsdrv drvcall;
	bool inited;
	vector<if_info_storage> storage_devs;
};
struct storage_mod_info
{
	string mod_name;
	void* hMod;
	pintf_fsdrv(*STO_GET_INTF_FUNC)(char*);
	vector<storage_drv_info> storage_drvs;
};
struct fs_dev_info
{
	string devtype;
};
struct proc_data
{
	void* hproc;
	void* hthrd_shelter;
	string name;
	string cmdline;
	void* id;
	bool ambiguous;
	proc_type type;
	vector<if_proc> ifproc;
};
struct if_control_block
{
	gate& m;
	vector<proc_data>& pdata;
	if_control_block(gate& _m,vector<proc_data>& p):m(_m),pdata(p)
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
	pdata->type=E_PROCTYPE_NONE;
}
inline void init_proc_data_cmdline(proc_data* pdata)
{
	if(pdata->ambiguous&&pdata->cmdline.empty())
		pdata->cmdline=pdata->name+" user="+get_if_user();
}
inline void insert_proc_data(proc_data& data,const process_stat& pstat)
{
	data.name=pstat.file;
	data.cmdline=pstat.cmdline;
	data.id=pstat.id;
	data.ambiguous=!!pstat.ambiguous;
	data.type=pstat.type;
	data.hproc=NULL;
	data.hthrd_shelter=NULL;
	for(int i=0;i<pstat.ifs->count;i++)
	{
		if_proc ifproc;
		ifproc.hif=NULL;
		ifproc.id=pstat.ifs->if_id[i].if_name;
		ifproc.usage=pstat.ifs->if_id[i].usage;
		ifproc.cnt=pstat.ifs->if_id[i].thrdcnt;
		ifproc.prior=pstat.ifs->if_id[i].prior;
		ifproc.pdata=NULL;
		data.ifproc.push_back(ifproc);
	}
}
inline void init_if_info_storage(if_info_storage* if_sto)
{
	if_sto->hdev=NULL;
	if_sto->ifproc=NULL;
	if_sto->sto_mod=NULL;
	if_sto->sto_drv=NULL;
}
#endif
