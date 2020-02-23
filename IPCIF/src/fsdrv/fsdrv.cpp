#include "fsdrv.h"
#include "native_fs.h"
#include <string.h>
struct named_drvcall
{
	char* drv_name;
	intf_fsdrv drvcall;
};
named_drvcall g_fsdrvs[] =
{
	{
		"native_fs",
		{
			fs_native_format,
			fs_native_mount,
			fs_native_unmount,
			fs_native_open,
			fs_native_close,
			fs_native_read,
			fs_native_write,
			fs_native_getsize,
			fs_native_setsize,
			fs_native_move,
			fs_native_del,
			fs_native_mkdir,
			fs_native_getattr,
			fs_native_setattr,
			fs_native_getfiletime,
			fs_native_setfiletime,
			fs_native_listfiles,
		}
	},
};
DLL pintf_fsdrv STO_GET_INTF_FUNC(char* drv_name)
{
	for(int i=0;i<sizeof(g_fsdrvs)/sizeof(named_drvcall);i++)
	{
		if(strcmp(g_fsdrvs[i].drv_name,drv_name)==0)
		{
			return &g_fsdrvs[i].drvcall;
		}
	}
	return NULL;
}