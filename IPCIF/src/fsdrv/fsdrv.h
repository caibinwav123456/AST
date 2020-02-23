#ifndef _FSDRV_H_
#define _FSDRV_H_
#ifdef __cplusplus
#include "common.h"
#include "fsdrv_interface.h"
extern "C" {
#endif
DLL pintf_fsdrv STO_GET_INTF_FUNC(char* drv_name);
#ifdef __cplusplus
}
#endif
#endif
