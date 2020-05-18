#ifndef _SYSFS_H_
#define _SYSFS_H_
#include "common.h"
#include "process_data.h"
#include "request_resolver.h"
extern "C" {
DLLAPI(int) fsc_init(uint numbuf,uint buflen,if_control_block* pblk=NULL,RequestResolver* resolver=NULL);
DLLAPI(void) fsc_exit();
DLLAPI(int) fsc_suspend(int bsusp);
DLLAPI(void*) fs_open(char* pathname,dword flags);
DLLAPI(int) fs_close(void* h);
DLLAPI(int) fs_seek(void* h,uint seektype,int offset,int* offhigh=NULL);
DLLAPI(int) fs_tell(void* h,uint* offset,uint* offhigh=NULL);
DLLAPI(int) fs_read(void* h,void* buf,uint len,uint* rdlen=NULL);
DLLAPI(int) fs_write(void* h,void* buf,uint len,uint* wrlen=NULL);
DLLAPI(int) fs_flush(void* h);
DLLAPI(int) fs_get_file_size(void* h,uint* low,uint* high=NULL);
DLLAPI(int) fs_set_file_size(void* h,uint* low,uint* high=NULL);
DLLAPI(int) fs_move(char* src,char* dst);
DLLAPI(int) fs_copy(char* src,char* dst);
DLLAPI(int) fs_delete(char* path);
DLLAPI(int) fs_get_attr(char* path,dword mask,dword* flags=NULL,DateTime* date=NULL);
DLLAPI(int) fs_set_attr(char* path,dword mask,dword flags=0,DateTime* date=NULL);
DLLAPI(int) fs_mkdir(char* path);
DLLAPI(int) fs_list_file(char* pathname,vector<fsls_element>& files);
DLLAPI(int) fs_traverse(char* pathname,int(*cb)(char*,dword,void*,char),void* param);
DLLAPI(int) fs_list_dev(vector<string>& devlist,uint* defdev=NULL);
DLLAPI(int) fs_recurse_copy(char* from,char* to);
DLLAPI(int) fs_recurse_delete(char* pathname);
DLLAPI(int) fss_init(vector<if_proc>* pif,RequestResolver* resolver);
DLLAPI(void) fss_exit();
DLLAPI(int) fss_suspend(int bsusp);
}
#endif