#include "common.h"
#include "path.h"
#include "syswin.h"
#include <shlwapi.h>
#include <string>
using namespace std;

void* sys_fopen(char* pathname, dword flags)
{
	dword rdwr=0,dispose=0,attr=0;
	if(flags&FILE_READ)
	{
		rdwr|=GENERIC_READ;
	}
	if(flags&FILE_WRITE)
	{
		rdwr|=GENERIC_WRITE;
	}
	if(flags&FILE_NOCACHE)
	{
		attr|=FILE_FLAG_WRITE_THROUGH;
	}
	switch(flags&FILE_MASK)
	{
	case FILE_CREATE_ALWAYS:
		dispose=CREATE_ALWAYS;
		break;
	case FILE_CREATE_NEW:
		dispose=CREATE_NEW;
		break;
	case FILE_OPEN_ALWAYS:
		dispose=OPEN_ALWAYS;
		break;
	case FILE_OPEN_EXISTING:
		dispose=OPEN_EXISTING;
		break;
	case FILE_TRUNCATE_EXISTING:
		dispose=TRUNCATE_EXISTING;
		break;
	}
	return (void*)CreateFileA(pathname,rdwr,FILE_SHARE_READ,NULL,dispose,attr,NULL);
}
int sys_fread(void* fd, void* buf, uint len, uint* rdlen)
{
	int left=(int)len;
	int read=0;
	int ret=0;
	char* _buf=(char*)buf;
	while(left>0)
	{
		read=0;
		if(!ReadFile((HANDLE)fd, _buf, left, (LPDWORD)&read, NULL))
		{
			ret=ERR_FILE_IO;
			break;
		}
		if(read==0)
			break;
		_buf+=read;
		left-=read;
	}
	if(rdlen!=NULL)
	{
		*rdlen=len-left;
	}
	else if(left>0)
	{
		return ERR_FILE_IO;
	}
	return ret;
}
int sys_fwrite(void* fd, void* buf, uint len, uint* wrlen)
{
	int left=(int)len;
	int write=0;
	int ret=0;
	char* _buf=(char*)buf;
	while(left>0)
	{
		write=0;
		if(!WriteFile((HANDLE)fd, _buf, left, (LPDWORD)&write, NULL))
		{
			ret=ERR_FILE_IO;
			break;
		}
		if(write==0)
			break;
		_buf+=write;
		left-=write;
	}
	if(wrlen!=NULL)
	{
		*wrlen=len-left;
	}
	else if(left>0)
	{
		return ERR_FILE_IO;
	}
	return ret;
}
int sys_fseek(void* fd, uint offlow, uint* offhigh, int seektype)
{
	int type=FILE_BEGIN;
	switch(seektype)
	{
	case SEEK_BEGIN:
		type=FILE_BEGIN;
		break;
	case SEEK_CUR:
		type=FILE_CURRENT;
		break;
	case SEEK_END:
		type=FILE_END;
		break;
	}
	SetLastError(NO_ERROR);
	SetFilePointer((HANDLE)fd,(LONG)offlow,(PLONG)offhigh,type);
	if(GetLastError()!=NO_ERROR)
		return ERR_FILE_IO;
	return 0;
}
int sys_get_file_size(void* fd, dword* sizelow, dword* sizehigh)
{
	dword dummy=0;
	dword* sz=(sizehigh==NULL?&dummy:sizehigh);
	SetLastError(NO_ERROR);
	*sizelow=(dword)GetFileSize((HANDLE)fd,(LPDWORD)sz);
	if(dummy!=0||GetLastError()!=NO_ERROR)
		return ERR_FILE_IO;
	return 0;
}
int sys_set_file_size(void* fd, dword sizelow, dword* sizehigh)
{
	SetLastError(NO_ERROR);
	SetFilePointer((HANDLE)fd, (LONG)sizelow, (PLONG)sizehigh, FILE_BEGIN);
	if(GetLastError()!=NO_ERROR)
		return ERR_FILE_IO;
	if(!SetEndOfFile((HANDLE)fd))
		return ERR_FILE_IO;
	return 0;
}
int sys_fflush(void* fd)
{
	if(!FlushFileBuffers((HANDLE)fd))
		return ERR_FILE_IO;
	return 0;
}
void sys_fclose(void* fd)
{
	CloseHandle((HANDLE)fd);
}
int sys_get_current_dir(char* pathbuf, uint size)
{
	return GetCurrentDirectoryA((DWORD)size, pathbuf)?0:ERR_FILE_IO;
}
int sys_set_current_dir(char* pathbuf)
{
	return SetCurrentDirectoryA(pathbuf)?0:ERR_FILE_IO;
}
int sys_is_absolute_path(char* path)
{
	char* colon=strchr(path,':');
	char* slash=strchr(path,'\\');
	if(colon==NULL)
		return 0;
	else if(slash==NULL||slash>colon)
		return 1;
	else
		return 0;
}
int sys_fstat(char* pathname, dword* type)
{
	if(!PathFileExistsA(pathname))
		return ERR_FILE_IO;
	if(type!=NULL)
		*type=(PathIsDirectoryA(pathname)?FILE_TYPE_DIR:FILE_TYPE_NORMAL);
	return 0;
}
inline void check_file(WIN32_FIND_DATAA& data, int(*cb)(char*, dword, void*), void* param, int& ret)
{
	if(strcmp(data.cFileName,".")!=0&&strcmp(data.cFileName,"..")!=0)
	if(0!=cb(data.cFileName,data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY?
		FILE_TYPE_DIR:FILE_TYPE_NORMAL,param))
		ret=ERR_FILE_IO;
}
int sys_ftraverse(char* pathname, int(*cb)(char*, dword, void*), void* param)
{
	WIN32_FIND_DATAA data;
	memset(&data,0,sizeof(data));
	string path(pathname);
	if(path.c_str()[path.length()-1]!='\\')
		path+="\\";
	path+="*";
	void* hFind=(void*)FindFirstFileA(path.c_str(), &data);
	if(!VALID(hFind))
		return 0;
	int ret=0;
	check_file(data,cb,param,ret);
	while(FindNextFileA(hFind,&data))
	{
		check_file(data,cb,param,ret);
	}
	FindClose(hFind);
	return ret;
}
inline int make_inner_dir(char* path)
{
	if(!PathFileExistsA(path))
	{
		if(!CreateDirectoryA(path,NULL))
			return ERR_FILE_IO;
	}
	else if(!PathIsDirectoryA(path))
		return ERR_FILE_IO;
	return 0;
}
inline void normalize_path(char* buf)
{
	char* ptr=strrchr(buf,'\\');
	if(ptr!=NULL&&ptr==buf+strlen(buf)-1)
		*ptr=0;
}
int sys_mkdir(char* path)
{
	char tmp[1024];
	strcpy_s(tmp,1023,path);
	tmp[1023]=0;
	char* ptr;
	normalize_path(tmp);
	ptr=strrchr(tmp,'\\');
	if(ptr!=NULL&&ptr!=tmp+strlen(tmp)-1)
	{
		*ptr=0;
		if(PathFileExistsA(tmp))
		{
			*ptr='\\';
			return make_inner_dir(tmp);
		}
		*ptr='\\';
	}
	for(ptr=tmp;*ptr!=0;ptr++)
	{
		if(*ptr=='\\')
		{
			*ptr=0;
			if(!PathFileExistsA(tmp))
			if(!CreateDirectoryA(tmp,NULL))
				return ERR_FILE_IO;
			*ptr='\\';
		}
	}
	return make_inner_dir(tmp);
}
int sys_fmove(char* from, char* to)
{
	return MoveFileA(from,to)?0:ERR_FILE_IO;
}
int sys_fcopy(char* from, char* to)
{
	if(!PathFileExistsA(from))
		return ERR_FILE_IO;
	if(PathIsDirectoryA(from))
	{
		if(!PathFileExistsA(to))
		{
			if(!CreateDirectoryA(to,NULL))
				return ERR_FILE_IO;
		}
		else if(!PathIsDirectoryA(to))
			return ERR_FILE_IO;
		return 0;
	}
	else
		return CopyFileA(from,to,FALSE)?0:ERR_FILE_IO;
}
int sys_fdelete(char* pathname)
{
	if(!PathFileExistsA(pathname))
		return 0;
	if(PathIsDirectoryA(pathname))
		return RemoveDirectoryA(pathname)?0:ERR_FILE_IO;
	else
		return DeleteFileA(pathname)?0:ERR_FILE_IO;
}
