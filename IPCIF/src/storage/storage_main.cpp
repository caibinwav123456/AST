#include "common.h"
#include "syswin.h"
#include "interface.h"
#include "process_data.h"
#include "sysfs.h"
#include "request_resolver.h"
#include "utility.h"
struct storage_data
{
	void** hif_sto;
	RequestResolver* req_resolv;
	bool reset;
	bool ready_quit;
	bool quit;
};
static inline void __insert_proc_data__(proc_data& data)
{
	insert_proc_data_cmdline(data,*get_current_executable_stat());
	for(int i=0;i<(int)data.ifproc.size();i++)
		data.ifproc[i].pdata=&data;
}
int cb_server(void* addr,void* param,int op)
{
	storage_data* data=(storage_data*)param;
	dg_storage* storage=(dg_storage*)addr;
	uint cmd=storage->header.cmd;
	data->req_resolv->HandleRequest(cmd,addr,NULL,op);
	switch(cmd)
	{
	case CMD_CLEAR:
	case CMD_CLEAR_ALL:
		data->reset=true;
		storage->header.ret=0;
		break;
	case CMD_SUSPEND:
		storage->header.ret=fss_suspend(storage->susp.bsusp);
		break;
	case CMD_EXIT:
		data->ready_quit=true;
		storage->header.ret=0;
		break;
	}
	return 0;
}
int if_server(void* param)
{
	storage_data* data=(storage_data*)param;
	while(true)
	{
		int ret=listen_if(*(data->hif_sto),cb_server,param);
		if(ret!=0)
			data->ready_quit=false;
		if(data->reset)
		{
			reset_if(*(data->hif_sto));
			data->reset=false;
		}
		if(data->quit||data->ready_quit)
		{
			data->quit=true;
			return 0;
		}
	}
	//should not reach here
	return 0;
}
int main_entry(main_args)
{
	int ret=0;
	if(0!=(ret=mainly_initial()))
		return ret;
	proc_data pdata;
	storage_data data;
	if_initial init;
	void* hif_storage;
	void* hthrd_server;
	RequestResolver req_rslvr;
	data.ready_quit=data.quit=data.reset=false;
	data.hif_sto=&hif_storage;
	data.req_resolv=&req_rslvr;
	__insert_proc_data__(pdata);
	if(pdata.ifproc.size()==0)
		goto end;
	init.user=get_if_user();
	init.id=(char*)pdata.ifproc[0].id.c_str();
	init.nthread=pdata.ifproc[0].cnt;
	init.smem_size=SIZE_IF_STORAGE;
	if(0!=(ret=setup_if(&init,&hif_storage)))
	{
		LOGFILE(0,log_ftype_error,"Create interfafe %s failed, quitting...",init.id);
		goto end;
	}
	if(0!=(ret=fss_init(&pdata.ifproc,&req_rslvr)))
	{
		LOGFILE(0,log_ftype_error,"File system init failed: %s",get_error_desc(ret));
		goto end2;
	}
	hthrd_server=sys_create_thread(if_server,&data);
	if(!VALID(hthrd_server))
	{
		LOGFILE(0,log_ftype_error,"Create server thread failed, quitting...");
		ret=ERR_THREAD_CREATE_FAILED;
		goto end3;
	}
	LOGFILE(0,log_ftype_info,"Start %s OK!",get_current_executable_name());
	while(!data.quit)
	{
		sys_sleep(10);
	}
	LOGFILE(0,log_ftype_info,"Ending %s.",get_current_executable_name());
	sys_sleep(10);
	if(VALID(hthrd_server))
	{
		stop_if(hif_storage);
		sys_wait_thread(hthrd_server);
		sys_close_thread(hthrd_server);
	}
end3:
	fss_exit();
end2:
	close_if(hif_storage);
end:
	mainly_exit();
	return ret;
}