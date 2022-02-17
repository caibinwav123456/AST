#include "sysfs.h"
#include "sysfs_struct.h"
#include "utility.h"
DLLAPI(int) fsc_init(uint numbuf,uint buflen,if_control_block* pblk,RequestResolver* resolver)
{
	return g_sysfs.Init(numbuf,buflen,pblk,resolver);
}
DLLAPI(void) fsc_exit()
{
	g_sysfs.Exit();
}
DLLAPI(int) fsc_suspend(int bsusp)
{
	return g_sysfs.SuspendIO(!!bsusp,1000);
}
DLLAPI(void*) fs_open(char* pathname,dword flags)
{
	return g_sysfs.Open(pathname,flags);
}
DLLAPI(int) fs_close(void* h)
{
	return g_sysfs.Close(h);
}
DLLAPI(int) fs_seek(void* h,uint seektype,int offset,int* offhigh)
{
	return g_sysfs.Seek(h,seektype,offset,offhigh);
}
DLLAPI(int) fs_tell(void* h,uint* offset,uint* offhigh)
{
	return g_sysfs.GetPosition(h,offset,offhigh);
}
DLLAPI(int) fs_read(void* h,void* buf,uint len,uint* rdlen)
{
	return g_sysfs.ReadWrite(CMD_FSREAD,h,buf,len,rdlen);
}
DLLAPI(int) fs_write(void* h,void* buf,uint len,uint* wrlen)
{
	return g_sysfs.ReadWrite(CMD_FSWRITE,h,buf,len,wrlen);
}
DLLAPI(int) fs_flush(void* h)
{
	return g_sysfs.FlushBuffer(h);
}
DLLAPI(int) fs_get_file_size(void* h,uint* low,uint* high)
{
	return g_sysfs.GetSetFileSize(CMD_FSGETSIZE,h,low,high);
}
DLLAPI(int) fs_set_file_size(void* h,uint* low,uint* high)
{
	return g_sysfs.GetSetFileSize(CMD_FSSETSIZE,h,low,high);
}
DLLAPI(int) fs_move(char* src,char* dst)
{
	return g_sysfs.MoveFile(src,dst);
}
DLLAPI(int) fs_copy(char* src,char* dst)
{
	return g_sysfs.CopyFile(src,dst);
}
DLLAPI(int) fs_delete(char* path)
{
	return g_sysfs.DeleteFile(path);
}
DLLAPI(int) fs_get_attr(char* path,dword mask,dword* flags,DateTime* date)
{
	return g_sysfs.GetSetFileAttr(CMD_FSGETATTR,path,mask,flags,date);
}
DLLAPI(int) fs_set_attr(char* path,dword mask,dword flags,DateTime* date)
{
	return g_sysfs.GetSetFileAttr(CMD_FSSETATTR,path,mask,&flags,date);
}
DLLAPI(int) fs_mkdir(char* path)
{
	return g_sysfs.MakeDir(path);
}
DLLAPI(int) fs_list_file(char* pathname,vector<fsls_element>& files)
{
	return g_sysfs.ListFile(pathname,files);
}
DLLAPI(int) fs_perm_close(void* handle)
{
	return __fs_perm_close(handle);
}
DLLAPI(int) fs_traverse(char* pathname,int(*cb)(char*,dword,void*,char),void* param)
{
	vector<fsls_element> vfile;
	int ret=0;
	if(0!=(ret=g_sysfs.ListFile(pathname,vfile)))
		return ret;
	for(int i=0;i<(int)vfile.size();i++)
	{
		if(0!=(ret=cb((char*)vfile[i].filename.c_str(),FS_IS_DIR(vfile[i].flags)?FILE_TYPE_DIR:FILE_TYPE_NORMAL,param,'/')))
			return ret;
	}
	return 0;
}
DLLAPI(int) fs_list_dev(vector<string>& devlist,uint* defdev)
{
	return g_sysfs.ListDev(devlist,defdev);
}
DLLAPI(int) fs_get_dev_info(const string& devname,fs_dev_info& dev)
{
	return g_sysfs.GetDevInfo(devname,dev);
}
DLLAPI(int) fs_recurse_copy(char* from,char* to,file_recurse_cbdata* cbdata)
{
	return __fs_recurse_copy(from,to,cbdata);
}
DLLAPI(int) fs_recurse_delete(char* pathname,file_recurse_cbdata* cbdata)
{
	return __fs_recurse_delete(pathname,cbdata);
}
DLLAPI(int) fs_recurse_stat(char* pathname,path_recurse_stat* pstat,file_recurse_cbdata* cbdata)
{
	return __fs_recurse_stat(pathname,pstat,cbdata);
}

DLLAPI(int) fss_init(vector<if_proc>* pif,RequestResolver* resolver)
{
	return g_fssrv.Init(pif,resolver);
}
DLLAPI(void) fss_exit()
{
	g_fssrv.Exit();
}
DLLAPI(int) fss_suspend(int bsusp)
{
	return g_fssrv.SuspendIO(!!bsusp,1000);
}