#include "common.h"
#include "interface.h"
#include "utility.h"
#include "common_request.h"
#include "process_data.h"
#include "arch.h"
#include "syswin.h"
#define MAX_CONNECT_TIMES 10
void* mutex=NULL;
static proc_data loader_exe_data;
static proc_data manager_exe_data;
struct astloader_data
{
	void** if_loader;
	void** if_mgr;
	bool ready_quit;
	bool quit;
};
inline const string& get_proc_start_cmd(const proc_data& data)
{
	return data.cmdline.empty()?data.name:data.cmdline;
}
int ast_shutdown(astloader_data* data)
{
	sys_wait_sem(mutex);
	if(VALID(*(data->if_mgr)))
	{
		int ret=send_cmd_exit(*(data->if_mgr));
		sys_signal_sem(mutex);
		if(0==ret)
			data->ready_quit=true;
		return ret;
	}
	sys_signal_sem(mutex);
	return ERR_GENERIC;
}
int cb_server(void* addr, void* param, int op)
{
	astloader_data* data=(astloader_data*)param;
	datagram_base* buf=(datagram_base*)addr;
	switch(buf->cmd)
	{
	case CMD_EXIT:
		buf->ret = ast_shutdown(data);
		break;
	default:
		break;
	}
	return 0;
}
int if_server(void* param)
{
	astloader_data* data=(astloader_data*)param;
	while(true)
	{
		int ret=listen_if(*(data->if_loader),cb_server,param);
		if(ret!=0)
			data->ready_quit=false;
		if(data->quit||data->ready_quit)
		{
			data->quit=true;
			return 0;
		}
	}
	//should not reach here
	return 0;
}
int connect_mgr(void** handle)
{
	if_initial init;
	init.user=get_if_user();
	init.id=(char*)manager_exe_data.ifproc[0].id.c_str();
	init.nthread=0;
	init.smem_size=0;
	for(int i=0;i<MAX_CONNECT_TIMES;i++)
	{
		sys_sleep(200);
		if(0==connect_if(&init, handle))
		{
			return 0;
		}
	}
	LOGFILE(0,log_ftype_error,"Connecting to interface %s failed, exitting...",init.id);
	sys_show_message("Connect to ASTManager failed");
	return ERR_IF_CONN_FAILED;
}
int main_entry(main_args)
{
	int ret=0;
	void* hserver;
	void* if_loader=NULL;
	void* if_mgr=NULL;
	void* hmgr=NULL;
	if(0!=(ret=mainly_initial()))
		return ret;
	insert_proc_data_cmdline(loader_exe_data,get_main_info()->loader_exe_info);
	insert_proc_data_cmdline(manager_exe_data,get_main_info()->manager_exe_info);
	mutex=sys_create_sem(1,1,NULL);
	if(!VALID(mutex))
	{
		ret=ERR_SEM_CREATE_FAILED;
		goto end;
	}
	if_initial init;
	init.user=get_if_user();
	init.id=(char*)loader_exe_data.ifproc[0].id.c_str();
	init.nthread=loader_exe_data.ifproc[0].cnt;
	init.smem_size=sizeof(datagram_base);
	astloader_data data;
	data.if_loader=&if_loader;
	data.if_mgr=&if_mgr;
	data.ready_quit=data.quit=false;
	if(0!=(ret=setup_if(&init, &if_loader)))
	{
		LOGFILE(0,log_ftype_error,"Create interfafe %s failed, quitting...",init.id);
		sys_show_message("Create interface astloader failed!");
		goto end2;
	}
	hserver = sys_create_thread(if_server, &data);
	if(!VALID(hserver))
	{
		LOGFILE(0,log_ftype_error,"Create server thread failed, quitting...");
		sys_show_message("Create server thread failed!");
		ret=ERR_THREAD_CREATE_FAILED;
		goto end3;
	}
	LOGFILE(0,log_ftype_info,"Start %s OK!",get_current_executable_name());
	while(true)
	{
		hmgr=arch_get_process(manager_exe_data);
		if(!VALID(hmgr))
			hmgr=sys_create_process((char*)get_proc_start_cmd(manager_exe_data).c_str());
		if(VALID(hmgr))
		{
			sys_wait_sem(mutex);
			int conn=connect_mgr(&if_mgr);
			sys_signal_sem(mutex);
			if(0!=conn)
			{
				sys_close_process(hmgr);
				data.quit=true;
				break;
			}
			sys_wait_process(hmgr);
			stop_if(if_mgr);
			sys_wait_sem(mutex);
			close_if(if_mgr);
			if_mgr=NULL;
			sys_signal_sem(mutex);
			sys_close_process(hmgr);
			hmgr=NULL;
		}
		sys_sleep(100);
		if(data.quit)
			break;
		else
			LOGFILE(0,log_ftype_error,"%s not started or erroneously terminated, restarting...",manager_exe_data.name.c_str());
	}
	LOGFILE(0,log_ftype_info,"Ending %s.",get_current_executable_name());
	stop_if(if_loader);
	sys_wait_thread(hserver);
	sys_close_thread(hserver);
end3:
	close_if(if_loader);
end2:
	sys_close_sem(mutex);
end:
	mainly_exit();
	return ret;
}