#include "native_fs.h"
#include "algor_templ.h"
int fs_native_format(char* cmd)
{
	return 0;
}
void* fs_native_mount(char* cmd)
{
	return NULL;
}
void fs_native_unmount(void* hdev)
{

}
void* fs_native_open(void* hdev,char* pathname,dword flags)
{
	return NULL;
}
void fs_native_close(void* hdev,void* hfile)
{

}
int fs_native_read(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen)
{
	return 0;
}
int fs_native_write(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen)
{
	return 0;
}
int fs_native_getsize(void* hdev,void* hfile,uint* size,uint* sizehigh)
{
	return 0;
}
int fs_native_setsize(void* hdev,void* hfile,uint size,uint sizehigh)
{
	return 0;
}
int fs_native_move(void* hdev,char* src,char* dst)
{
	return 0;
}
int fs_native_del(void* hdev,char* path)
{
	return 0;
}
int fs_native_mkdir(void* hdev,char* path)
{
	return 0;
}
int fs_native_getattr(void* hdev,char* path,dword* attrflags)
{
	return 0;
}
int fs_native_setattr(void* hdev,char* path,dword attrflags)
{
	return 0;
}
int fs_native_getfiletime(void* hdev,char* path,dword mask,DateTime* date)
{
	return 0;
}
int fs_native_setfiletime(void* hdev,char* path,dword mask,DateTime* date)
{
	return 0;
}
int fs_native_listfiles(void* hdev,char* path,vector<fsls_element>* files)
{
	return 0;
}
