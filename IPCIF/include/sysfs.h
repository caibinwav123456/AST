#ifndef _SYSFS_H_
#define _SYSFS_H_
#include "common.h"
#include "process_data.h"
#include "request_resolver.h"
#ifdef __cplusplus
extern "C" {
#endif
DLLAPI(int) fsc_init(uint numbuf,uint buflen,if_control_block* pblk=NULL,RequestResolver* resolver=NULL);
DLLAPI(void) fsc_exit();
DLLAPI(int) fsc_suspend(int bsusp);
#ifdef __cplusplus
}
#endif
#endif