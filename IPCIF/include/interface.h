#ifndef _INTERFACE_H_
#define _INTERFACE_H_
#include "common.h"
#ifdef __cplusplus
extern "C" {
#endif
#define OP_PARAM 0x1
#define OP_RETURN 0x2
typedef struct _if_initial
{
	uint smem_size;
	int nthread;
	char* user;
	char* id;
}if_initial;
typedef int (*if_callback)(void* addr, void* param, int op);
DLLAPI(int) setup_if(if_initial* init, void** handle);
DLLAPI(int) connect_if(if_initial* init, void** handle);
DLLAPI(int) listen_if(void* handle, if_callback cb, void* param, uint time=0);
DLLAPI(int) request_if(void* handle, if_callback cb, void* param);
DLLAPI(int) stat_if(void* handle, int* n, uint* memsize);
DLLAPI(void) stop_if(void* handle);
DLLAPI(void) reset_if(void* handle);
DLLAPI(int) stopped_if(void* handle);
DLLAPI(void) close_if(void* handle);
#ifdef __cplusplus
}
#endif
#endif