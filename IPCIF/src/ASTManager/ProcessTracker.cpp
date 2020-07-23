#include "ProcessTracker.h"
#include "utility.h"
#include "common_request.h"
#include "arch.h"
#include <algorithm>
#include <assert.h>

#define MAX_CONNECT_TIMES 10

gate process_tracker::if_mutex;

struct sem_pair
{
	void* semin;
	void* semout;
	sem_pair(void* i,void* o)
	{
		semin=i,semout=o;
	}
};
struct pt_param
{
	sem_pair* sem;
	process_tracker* pt;
	int i;
	pt_param(sem_pair* s, process_tracker* p, int index)
	{
		sem=s;
		pt=p;
		i=index;
	}
};
process_tracker::process_tracker():quit(false),cblk(if_mutex,pdata)
{
}
inline bool __insert_proc_data__(vector<proc_data>& pdata,const process_stat& pstat)
{
	if(!pstat.is_managed)
		return false;
	proc_data data;
	data.name=pstat.file;
	data.cmdline=pstat.cmdline;
	data.id=pstat.id;
	data.ambiguous=!!pstat.ambiguous;
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
	pdata.push_back(data);
	return true;
}
bool less_id(const proc_data& d1,const proc_data& d2)
{
	return d1.id<d2.id;
}
int process_tracker::init()
{
	int ret=0;
	pdata.clear();
	process_stat pstat;
	init_process_stat(&pstat,"");
	if_ids ifs;
	pstat.ifs=&ifs;
	void* h=find_first_exe(&pstat);
	if(!VALID(h))
		return 0;
	__insert_proc_data__(pdata,pstat);
	while(find_next_exe(h,&pstat))
		__insert_proc_data__(pdata,pstat);
	find_exe_close(h);
	sort(pdata.begin(),pdata.end(),less_id);
	for(int i=0;i<(int)pdata.size();i++)
	{
		for(int j=0;j<(int)pdata[i].ifproc.size();j++)
		{
			pdata[i].ifproc[j].pdata=&pdata[i];
		}
	}
	vector<sem_pair> vs;
	for(int i=0;i<(int)pdata.size();i++)
	{
		vs.push_back(sem_pair(NULL,NULL));
	}
	int fail_index=0;
	for(int i=0;i<(int)pdata.size();i++)
	{
		vs[i].semin=sys_create_sem(0,1,NULL);
		vs[i].semout=sys_create_sem(0,1,NULL);
		if((!VALID(vs[i].semin))||(!VALID(vs[i].semout)))
		{
			ret=ERR_SEM_CREATE_FAILED;
			goto fail;
		}
		if(pdata[i].ambiguous)
		{
			//Need not lock since we blocked shelter threads here.
			pdata[i].hproc=arch_get_process(pdata[i]);
		}
		else
			pdata[i].hproc=sys_get_process((char*)pdata[i].name.c_str());
		pt_param* param=new pt_param(&vs[i],this,i);
		pdata[i].hthrd_shelter=sys_create_thread(threadfunc,param);
		if(!VALID(pdata[i].hthrd_shelter))
		{
			delete param;
			ret=ERR_THREAD_CREATE_FAILED;
			LOGFILE(0,log_ftype_error,"Create shelter thread for %s failed. quitting...",pdata[i].name.c_str());
			goto fail;
		}
		fail_index++;
	}
	for(int i=0;i<(int)pdata.size();i++)
	{
		sys_signal_sem(vs[i].semin);
		sys_wait_sem(vs[i].semout);
		sys_close_sem(vs[i].semin);
		sys_close_sem(vs[i].semout);
		sys_sleep(100);
	}
	return 0;
fail:
	quit = true;
	for(int i=fail_index;i>=0;i--)
	{
		if(VALID(pdata[i].hproc))
		{
			sys_close_process(pdata[i].hproc);
			pdata[i].hproc=NULL;
		}
		if(VALID(pdata[i].hthrd_shelter))
		{
			sys_signal_sem(vs[i].semin);
			sys_wait_thread(pdata[i].hthrd_shelter);
			sys_close_thread(pdata[i].hthrd_shelter);
			sys_close_sem(vs[i].semin);
			sys_close_sem(vs[i].semout);
		}
		else
		{
			if(VALID(vs[i].semin))
				sys_close_sem(vs[i].semin);
			if(VALID(vs[i].semout))
				sys_close_sem(vs[i].semout);
		}
	}
	return ret;
}
int connect_proc(char* id, void** h)
{
	if_initial init;
	init.user=get_if_user();
	init.id=id;
	init.smem_size=0;
	init.nthread=0;
	for(int i=0;i<MAX_CONNECT_TIMES;i++)
	{
		if(0==connect_if(&init,h))
		{
			return 0;
		}
		sys_sleep(200);
	}
	LOGFILE(0,log_ftype_error,"Connecting to interface %s failed, try reconnecting...",init.id);
	char msg[256];
	sprintf(msg, "Connect to interface %s failed", id);
	sys_show_message(msg);
	return ERR_IF_CONN_FAILED;
}
int process_tracker::threadfunc(void* param)
{
	pt_param* pt=(pt_param*)param;
	process_tracker* ptracker=pt->pt;
	int index=pt->i;
	sem_pair* sem=pt->sem;
	delete pt;
	sys_wait_sem(sem->semin);
	if(ptracker->quit)
		return 0;
	sys_signal_sem(sem->semout);
	proc_data& data=ptracker->pdata[index];
	vector<proc_data>& vdata=ptracker->pdata;
	bool start=false;
	if(VALID(data.hproc))
		start=true;
	while(true)
	{
		if(!VALID(data.hproc))
		{
			if(data.ambiguous)
			{
				lock_track();
				data.hproc=arch_get_process(data);
			}
			else
				data.hproc=sys_get_process((char*)data.name.c_str());
		}
		if(!VALID(data.hproc))
			data.hproc=sys_create_process((char*)(data.cmdline.empty()?data.name:data.cmdline).c_str());
		if(VALID(data.hproc))
		{
			bool conn_failed;
			uint wait_interval=0;
			do
			{
				conn_failed=false;
				for(int i=0;i<(int)data.ifproc.size();i++)
				{
					if(!VALID(data.ifproc[i].hif))
					{
						{
							lock_track();
							if(0!=connect_proc((char*)data.ifproc[i].id.c_str(),&data.ifproc[i].hif))
							{
								data.ifproc[i].hif=NULL;
								conn_failed=true;
							}
						}
						if((!conn_failed)&&i==0&&start)
						{
							send_cmd_clear_all(data.ifproc[0].hif);
							start=false;
						}
					}
				}
				if(conn_failed)
					wait_interval=100;
				else
					wait_interval=0;
			}
			while(0!=sys_wait_process(data.hproc,wait_interval));
			for(int i=0;i<(int)data.ifproc.size();i++)
			{
				if(!VALID(data.ifproc[i].hif))
					continue;
				stop_if(data.ifproc[i].hif);
				lock_track();
				close_if(data.ifproc[i].hif);
				data.ifproc[i].hif=NULL;
			}
			sys_close_process(data.hproc);
			data.hproc=NULL;
		}
		if(ptracker->quit)
			break;
		else
			LOGFILE(0,log_ftype_error,"The managed program %s not started or erroneously terminated, restarting...",(char*)data.name.c_str());
		send_cmd_clear(data.id,NULL,get_main_info()->manager_if0);
		for(int i=(int)vdata.size()-1;i>=0;i--)
		{
			if(i==index)
				continue;
			lock_track();
			proc_data& pvdata=vdata[i];
			if(pvdata.ifproc.size()>0&&VALID(pvdata.ifproc[0].hif))
			{
				send_cmd_clear(data.id,pvdata.ifproc[0].hif);
			}
		}
		sys_sleep(100);
	}
	return 0;
}
int process_tracker::suspend_all(bool bsusp)
{
	int index=bsusp?(int)pdata.size()-1:-1;
	int ret=0;
	lock_track();
	if(bsusp)
	{
		for(int i=(int)pdata.size()-1;i>=0;i--)
		{
			if(!(pdata[i].ifproc.size()>0&&VALID(pdata[i].ifproc[0].hif)))
			{
				ret=ERR_GENERIC;
				break;
			}
			if(0!=(ret=send_cmd_suspend(1,pdata[i].ifproc[0].hif)))
			{
				break;
			}
			index--;
		}
	}
	if(!bsusp||index>-1)
	{
		for(int i=index+1;i<(int)pdata.size();i++)
		{
			if(!(pdata[i].ifproc.size()>0&&VALID(pdata[i].ifproc[0].hif)))
			{
				ret=ERR_GENERIC;
				continue;
			}
			int retc;
			if(0!=(retc=send_cmd_suspend(0,pdata[i].ifproc[0].hif)))
			{
				ret=retc;
			}
		}
	}
	return ret;
}
void process_tracker::exit()
{
	quit=true;
	for(int i=(int)pdata.size()-1;i>=0;i--)
	{
		if(VALID(pdata[i].hthrd_shelter))
		{
			{
				lock_track();
				if(pdata[i].ifproc.size()==0
					||(!VALID(pdata[i].ifproc[0].hif)))
					continue;
				if(0!=send_cmd_exit(pdata[i].ifproc[0].hif))
					continue;
			}
			sys_wait_thread(pdata[i].hthrd_shelter);
			sys_close_thread(pdata[i].hthrd_shelter);
			pdata[i].hthrd_shelter=NULL;
		}
	}
}
