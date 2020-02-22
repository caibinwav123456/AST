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
struct offset64
{
	uint off;
	uint offhigh;
};
struct fsls_element
{
	string filename;
	dword flags;
};
typedef struct _intf_fsdrv
{
	int(*format)(char* cmd);
	void*(*mount)(char* cmd);
	void(*unmount)(void* hdev);
	void*(*open)(void* hdev,char* pathname,dword flags);
	void(*close)(void* hdev,void* hfile);
	int(*read)(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen);
	int(*write)(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen);
	int(*getsize)(void* hdev,void* hfile,uint* size,uint* sizehigh);
	int(*setsize)(void* hdev,void* hfile,uint size,uint sizehigh);
	int(*move)(void* hdev,char* src,char* dst);
	int(*del)(void* hdev,char* path);
	int(*mkdir)(void* hdev,char* path);
	int(*getattr)(void* hdev,char* path,dword* attrflags);
	int(*setattr)(void* hdev,char* path,dword attrflags);
	int(*getfiletime)(void* hdev,char* path,dword mask,DateTime* date);
	int(*setfiletime)(void* hdev,char* path,dword mask,DateTime* date);
	int(*listfiles)(void* hdev,char* path,vector<fsls_element>* files);
}intf_fsdrv,*pintf_fsdrv;
struct storage_mod_info;
struct if_info_storage
{
	string drv_name;
	string mount_cmd;
	string format_cmd;
	void* hdev;
	pintf_fsdrv drvcall;
	storage_mod_info* sto_mod;
	if_proc* ifproc;
};
struct storage_mod_info
{
	string mod_name;
	void* hMod;
	pintf_fsdrv(*STO_GET_INTF_FUNC)(char*);
	vector<if_info_storage> storage_drvs;
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
inline void init_if_info_storage(if_info_storage* if_sto)
{
	if_sto->drvcall=NULL;
	if_sto->hdev=NULL;
	if_sto->ifproc=NULL;
	if_sto->sto_mod=NULL;
}
#endif
