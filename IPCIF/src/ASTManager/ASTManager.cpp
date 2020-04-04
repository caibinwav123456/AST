#include "common.h"
#include "interface.h"
#include "utility.h"
#include "syswin.h"
#include "common_request.h"
#include "ProcessTracker.h"
#include "sysfs.h"

process_tracker g_ptracker;
struct astmgr_data
{
	void** if_mgr;
	void* hloader;
	bool sysfs_inited;
	bool ready_quit;
	bool quit;
};
int loader_shelter(void* param)
{
	astmgr_data* data=(astmgr_data*)param;
	void* hloader=data->hloader;
	while(true)
	{
		if(!VALID(hloader))
		{
			hloader=sys_get_process(get_main_info()->loader_exe_file);
		}
		if(!VALID(hloader))
		{
			hloader=sys_create_process(get_main_info()->loader_exe_file);
		}
		if(VALID(hloader))
		{
			while(0!=sys_wait_process(hloader, 300))
			{
				if(data->quit)
					break;
			}
			sys_close_process(hloader);
			hloader=NULL;
		}
		sys_sleep(100);
		if(data->quit)
			break;
		else
			LOGFILE(0,log_ftype_error,"%s not started or erroneously terminated, restarting...",get_main_info()->loader_exe_file);
	}
	return 0;
}
int cb_server(void* addr,void* param,int op)
{
	astmgr_data* data=(astmgr_data*)param;
	dg_manager* buf=(dg_manager*)addr;
	switch(buf->header.cmd)
	{
	case CMD_EXIT:
		if(!data->sysfs_inited)
		{
			buf->header.ret=ERR_MODULE_NOT_INITED;
			break;
		}
		if(0!=(buf->header.ret=fsc_suspend(1)))
			break;
		if(0!=(buf->header.ret=g_ptracker.suspend_all(true)))
		{
			data->ready_quit=false;
			fsc_suspend(0);
			break;
		}
		data->ready_quit=true;
		break;
	case CMD_CLEAR:
		buf->header.ret=0;
		break;
	default:
		break;
	}
	return 0;
}
int if_server(void* param)
{
	astmgr_data* data=(astmgr_data*)param;
	while(true)
	{
		int ret=listen_if(*(data->if_mgr),cb_server,param);
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
int main_entry(main_args)
{
	int ret=0;
	if(0!=(ret=mainly_initial()))
		return ret;
	if_initial init;
	init.user=get_if_user();
	init.id=get_main_info()->manager_if0;
	init.nthread=get_main_info()->manager_if0_cnt;
	init.smem_size=SIZE_IF_MANAGER;
	void* if_mgr=NULL;
	astmgr_data data;
	data.ready_quit=data.quit=false;
	data.sysfs_inited=false;
	data.if_mgr=&if_mgr;
	void *hthread_loader=NULL,
		*hserver=NULL;
	if(0!=(ret=setup_if(&init, &if_mgr)))
	{
		LOGFILE(0,log_ftype_error,"Create interfafe %s failed, quitting...",init.id);
		goto end2;
	}
	data.hloader=sys_get_process(get_main_info()->loader_exe_file);
	if(!VALID(data.hloader))
	{
		LOGFILE(0,log_ftype_error,"Detected %s not started, quitting...",get_main_info()->loader_exe_file);
		sys_show_message("Start loader first!");
		data.quit=true;
		ret=ERR_PROCESS_FAILED;
		goto end;
	}
	hthread_loader=sys_create_thread(loader_shelter,&data);
	if(!VALID(hthread_loader))
	{
		LOGFILE(0,log_ftype_error,"Create shelter thread failed, quitting...");
		data.quit=true;
		ret=ERR_THREAD_CREATE_FAILED;
		goto end;
	}
	hserver=sys_create_thread(if_server,&data);
	if(!VALID(hserver))
	{
		LOGFILE(0,log_ftype_error,"Create server thread failed, quitting...");
		data.quit=true;
		ret=ERR_THREAD_CREATE_FAILED;
		goto end;
	}
	LOGFILE(0,log_ftype_info,"Start %s OK!",get_current_executable_name());
	//do some useful things...
	if(0!=(ret=g_ptracker.init()))
	{
		LOGFILE(0,log_ftype_error,"Init managed process failed, quitting...");
		data.quit=true;
		goto end;
	}
	if(0!=(ret=fsc_init(get_num_sysfs_buffer_blocks(),get_sysfs_buffer_size(),g_ptracker.get_ctrl_blk())))
	{
		LOGFILE(0,log_ftype_error,"Init file system failed (%s), quitting...",get_error_desc(ret));
		data.quit=true;
		goto end3;
	}
	data.sysfs_inited=true;
	while(!data.quit)
	{
		sys_sleep(10);
	}
	LOGFILE(0,log_ftype_info,"Ending %s.",get_current_executable_name());
	sys_sleep(100);
	fsc_exit();
end3:
	g_ptracker.exit();
end:
	if(hserver)
	{
		stop_if(if_mgr);
		sys_wait_thread(hserver);
		sys_close_thread(hserver);
	}
	if(hthread_loader)
	{
		sys_wait_thread(hthread_loader);
		sys_close_thread(hthread_loader);
	}
	else if(data.hloader)
	{
		sys_close_process(data.hloader);
	}
	close_if(if_mgr);
end2:
	mainly_exit();
	return ret;
}