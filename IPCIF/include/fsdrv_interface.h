#ifndef _FSDRV_INTERFACE_H_
#define _FSDRV_INTERFACE_H_
#include "common.h"
#include <vector>
#include <string>
using namespace std;
struct fsls_element
{
	string filename;
	dword flags;
};
typedef struct _intf_fsdrv
{
	int(*init)();
	void(*uninit)();
	int(*format)(char* cmd);
	int(*mount)(char* cmd,void** hdev);
	void(*unmount)(void* hdev);
	void*(*open)(void* hdev,char* pathname,dword flags);
	void(*close)(void* hdev,void* hfile);
	int(*read)(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen);
	int(*write)(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* wrlen);
	int(*getsize)(void* hdev,void* hfile,uint* size,uint* sizehigh);
	int(*setsize)(void* hdev,void* hfile,uint size,uint sizehigh);
	int(*move)(void* hdev,char* src,char* dst);
	int(*del)(void* hdev,char* path);
	int(*mkdir)(void* hdev,char* path);
	int(*getattr)(void* hdev,char* path,dword* attrflags);
	int(*setattr)(void* hdev,char* path,dword attrflags);
	int(*getfiletime)(void* hdev,char* path,dword mask,DateTime* date);
	int(*setfiletime)(void* hdev,char* path,dword mask,DateTime* date);
	int(*listfiles)(void* hdev,char* path,vector<fsls_element>* files);
}intf_fsdrv,*pintf_fsdrv;
#endif