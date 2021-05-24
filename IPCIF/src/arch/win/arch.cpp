#include "arch.h"
#include "common_request.h"
#include "syswin.h"
DLLAPI(void*) arch_get_process(const proc_data& data)
{
	if(data.ifproc.empty())
		return NULL;
	uint pid=0;
	if(0!=send_cmd_getid(&pid,data.ifproc[0].hif,(char*)data.ifproc[0].id.c_str()))
		return NULL;
	return (void*)OpenProcess(PROCESS_ALL_ACCESS,TRUE,pid);
}
