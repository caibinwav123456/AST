#include "sysfs.h"
#include "sysfs_struct.h"
#include "path.h"
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
	return fs_read_write(true,h,buf,len,rdlen);
}
DLLAPI(int) fs_write(void* h,void* buf,uint len,uint* wrlen)
{
	return fs_read_write(false,h,buf,len,wrlen);
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
DLLAPI(int) fs_get_attr(char* path,dword mask,dword flags,DateTime* date)
{
	return g_sysfs.GetSetFileAttr(CMD_FSGETATTR,path,mask,&flags,date);
}
DLLAPI(int) fs_set_attr(char* path,dword mask,dword* flags,DateTime* date)
{
	return g_sysfs.GetSetFileAttr(CMD_FSSETATTR,path,mask,flags,date);
}
DLLAPI(int) fs_mkdir(char* path)
{
	return g_sysfs.MakeDir(path);
}
DLL int fs_ftraverse(char* pathname,int(*cb)(char*,dword,void*),void* param)
{
	vector<string> vfile;
	int ret=0;
	if(0!=(ret=g_sysfs.ListFile(pathname,vfile)))
		return ret;
	for(int i=0;i<(int)vfile.size();i++)
	{
		string path;
		dword flags=0;
		concat_path(pathname,vfile[i],path);
		if(0!=(ret==g_sysfs.GetSetFileAttr(CMD_FSGETATTR,path.c_str(),FS_ATTR_FLAGS,&flags)))
		{
			ret=ERR_FILE_IO;
			continue;
		}
		if(0!=cb((char*)path.c_str(),FS_IS_DIR(flags)?FILE_TYPE_DIR:FILE_TYPE_NORMAL,param))
			ret=ERR_FILE_IO;
	}
	return ret;
}