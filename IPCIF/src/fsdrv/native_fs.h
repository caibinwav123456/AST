#ifndef _NATIVE_FS_H_
#define _NATIVE_FS_H_
#include "common.h"
#include "fsdrv_interface.h"
#include <vector>
#include <string>
using namespace std;
int fs_native_init();
void fs_native_uninit();
int fs_native_format(char* cmd);
void* fs_native_mount(char* cmd);
void fs_native_unmount(void* hdev);
void* fs_native_open(void* hdev,char* pathname,dword flags);
void fs_native_close(void* hdev,void* hfile);
int fs_native_read(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen);
int fs_native_write(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen);
int fs_native_getsize(void* hdev,void* hfile,uint* size,uint* sizehigh);
int fs_native_setsize(void* hdev,void* hfile,uint size,uint sizehigh);
int fs_native_move(void* hdev,char* src,char* dst);
int fs_native_del(void* hdev,char* path);
int fs_native_mkdir(void* hdev,char* path);
int fs_native_getattr(void* hdev,char* path,dword* attrflags);
int fs_native_setattr(void* hdev,char* path,dword attrflags);
int fs_native_getfiletime(void* hdev,char* path,dword mask,DateTime* date);
int fs_native_setfiletime(void* hdev,char* path,dword mask,DateTime* date);
int fs_native_listfiles(void* hdev,char* path,vector<fsls_element>* files);
#endif
