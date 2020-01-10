#ifndef _COMMON_REQUEST_H_
#define _COMMON_REQUEST_H_
#include "common.h"
#include "import.h"
#include "interface.h"
#include "export.h"
#ifdef __cplusplus
extern "C" {
#endif
struct datagram_param
{
	int copyto;
	int copyfrom;
	void* dg;
};
DLL int cb_simple_req(void* addr, void* param, int op);
DLL int cb_multibyte_req(void* addr, void* param, int op);
DLLAPI(int) send_simple_request(uint cmd, void* hif, char* id=NULL, char* user=NULL);
DLLAPI(int) send_cmd_exit(void* hif, char* id=NULL, char* user=NULL);
DLLAPI(int) send_cmd_clear(void* pid, void* hif, char* id=NULL, char* user=NULL);
DLLAPI(int) send_cmd_clear_all(void* hif, char* id=NULL, char* user=NULL);
DLLAPI(int) send_cmd_suspend(int bsusp, void* hif, char* id=NULL, char* user=NULL);
DLLAPI(int) send_cmd_getid(uint* pid, void* hif, char* id=NULL, char* user=NULL);
#ifdef __cplusplus
}
#endif
inline int send_request_no_reset(void* h, if_callback cb, void* param)
{
	int ret=0;
	int count=0;
	while(ERR_IF_RESET==(ret=request_if(h,cb,param)))
	{
		count++;
		if(count>=10)
			break;
		sys_sleep(100);
	}
	return ret;
}
#endif
