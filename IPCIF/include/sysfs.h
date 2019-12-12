#ifndef _SYSFS_H_
#define _SYSFS_H_
#include "common.h"
#include "process_data.h"
#include "request_resolver.h"
DLLAPI(int) fsc_init(uint numbuf,uint buflen,if_control_block* pblk=NULL,RequestResolver* resolver=NULL);
#endif