#include "sysfs_struct.h"
DLLAPI(int) fsc_init(uint numbuf,uint buflen,if_control_block* pblk,RequestResolver* resolver)
{
	return g_sysfs.Init(numbuf,buflen,pblk,resolver);
}
DLLAPI(int) fsc_suspend(int bsusp)
{
	return g_sysfs.SuspendIO(!!bsusp,1000);
}