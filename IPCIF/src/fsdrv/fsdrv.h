#ifndef _FSDRV_H_
#define _FSDRV_H_
#include "common.h"
#include "fsdrv_interface.h"
#ifdef __cplusplus
extern "C" {
#endif
DLL pintf_fsdrv STO_GET_INTF_FUNC(char* drv_name);
#ifdef __cplusplus
}
#endif
#endif
