#include "common.h"
#include "syswin.h"
#include <tlhelp32.h>
#include <process.h>

#define SEM_MAX_WAIT 1024

struct smem_data
{
	HANDLE hsmem;
	void* ptr;
};
void* sys_create_sem(int count, int maxcount, char* name)
{
	return (void*)CreateSemaphoreA(NULL, count, maxcount, name);
}
void* sys_get_sem(char* name)
{
	return (void*)OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, TRUE, name);
}
void sys_close_sem(void* handle)
{
	CloseHandle((HANDLE)handle);
}
int sys_wait_sem(void* handle, uint time)
{
	if(time==0)
		time=INFINITE;
	if(WAIT_TIMEOUT==WaitForSingleObject((HANDLE)handle, time))
		return ERR_TIMEOUT;
	return 0;
}
void sys_signal_sem(void* handle)
{
	ReleaseSemaphore((HANDLE)handle, 1, NULL);
}
void* sys_create_smem(uint size, char* name)
{
	smem_data* data=new smem_data;
	data->hsmem=(void*)CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);
	if(!VALID(data->hsmem))
	{
		delete data;
		return NULL;
	}
	data->ptr=MapViewOfFile(data->hsmem, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if(!VALID(data->ptr))
	{
		CloseHandle((HANDLE)(data->hsmem));
		delete data;
		return NULL;
	}
	return data;
}
void* sys_get_smem(char* name)
{
	smem_data* data=new smem_data;
	data->hsmem=(void*)OpenFileMappingA(FILE_MAP_ALL_ACCESS, TRUE, name);
	if(!VALID(data->hsmem))
	{
		delete data;
		return NULL;
	}
	data->ptr=MapViewOfFile(data->hsmem, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if(!VALID(data->ptr))
	{
		CloseHandle((HANDLE)(data->hsmem));
		delete data;
		return NULL;
	}
	return data;
}
void sys_close_smem(void* handle)
{
	smem_data* data=(smem_data*)handle;
	if(VALID(data->ptr))
		UnmapViewOfFile(data->ptr);
	if(VALID(data->hsmem))
		CloseHandle(data->hsmem);
	delete data;
}
void* sys_map_smem(void* handle)
{
	smem_data* data=(smem_data*)handle;
	return data->ptr;
}
void sys_sem_signal_all(void* handle)
{
	for (int i=0;i<SEM_MAX_WAIT;i++)
	{
		ReleaseSemaphore((HANDLE)handle, 1, NULL);
	}
}

void sys_show_message(char* msg)
{
	MessageBoxA(0,msg,"",MB_OK);
}
void sys_sleep(int millisec)
{
	Sleep(millisec);
}
#ifdef PROCESSENTRY32
#undef PROCESSENTRY32
#endif
#ifdef Process32First
#undef Process32First
#endif
#ifdef Process32Next
#undef Process32Next
#endif
inline bool match_process_name(PROCESSENTRY32* pe, char* name, DWORD* id, int* count)
{
	bool b=(0==strcmp(name, pe->szExeFile));
	if(b)
	{
		*id=pe->th32ProcessID;
		(*count)++;
	}
	return b;
}
int GetRunningProcessID(char* name,DWORD* lastid)
{
	HANDLE hSnap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
	if(!VALID(hSnap))
		return -1;
	int count=0;
	DWORD id=0;
	PROCESSENTRY32 pe;
	pe.dwSize=sizeof(pe);
	BOOL bMore=Process32First(hSnap, &pe);
	if(bMore)
		match_process_name(&pe,name,&id,&count);
	while(bMore)
	{
		if(bMore=Process32Next(hSnap,&pe))
		{
			match_process_name(&pe,name,&id,&count);
		}
	}
	CloseHandle(hSnap);
	if(count>0&&lastid!=NULL)
		*lastid=id;
	return count;
}
bool sys_has_dup_process(char* name)
{
	return GetRunningProcessID(name, NULL)>1;
}
void* sys_create_process(char* name)
{
	PROCESS_INFORMATION pi;
	STARTUPINFOA si;
	memset(&pi,0,sizeof(pi));
	memset(&si,0,sizeof(si));
	si.cb=sizeof(si);
	if(!CreateProcessA(NULL, name, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		return NULL;
	}
	CloseHandle(pi.hThread);
	return (void*)(pi.hProcess);
}
void* sys_get_process(char* name)
{
	DWORD id=0;
	if(GetRunningProcessID(name, &id)<1)
	{
		return NULL;
	}
	return (void*)OpenProcess(PROCESS_ALL_ACCESS, TRUE, id);
}
int sys_get_current_process_name(char* name, int len)
{
	char path[1024];
	GetModuleFileNameA(NULL, path, 1024);
	char* file=strrchr(path, '\\');
	if(file==NULL)
		file=path;
	else
		file++;
	if((int)strlen(file)>len-1)
		return ERR_BUFFER_OVERFLOW;
	strcpy(name, file);
	return 0;
}
int sys_get_current_process_path(char* path, int len)
{
	char fullpath[1024];
	GetModuleFileNameA(NULL, fullpath, 1024);
	char* file=strrchr(fullpath, '\\');
	if(file==NULL)
		*fullpath=0;
	else
		*file=0;
	if((int)strlen(fullpath)>len-1)
		return ERR_BUFFER_OVERFLOW;
	strcpy(path, fullpath);
	return 0;
}
struct thread_data
{
	int (*cb)(void*);
	void* param;
};
unsigned int WINAPI ThreadProc(void* param)
{
	thread_data* data=(thread_data*)param;
	int (*cb)(void*)=data->cb;
	void* p=data->param;
	delete data;
	return (unsigned int)cb(p);
}
void* sys_create_thread(int (*cb)(void*), void* param)
{
	thread_data* data=new thread_data;
	data->cb=cb;
	data->param=param;
	void* hret=(void*)_beginthreadex(NULL,NULL,ThreadProc,data,0,NULL);
	if(!VALID(hret))
	{
		delete data;
		return NULL;
	}
	return hret;
}
void sys_close_process(void* hproc)
{
	CloseHandle(hproc);
}
void sys_close_thread(void* hthread)
{
	CloseHandle(hthread);
}
int sys_wait_process(void* hproc, uint time)
{
	if(time==0)
		time=INFINITE;
	if(WAIT_TIMEOUT==WaitForSingleObject(hproc, time))
		return ERR_TIMEOUT;
	return 0;
}
int sys_wait_thread(void* hthread, uint time)
{
	if(time==0)
		time=INFINITE;
	if(WAIT_TIMEOUT==WaitForSingleObject(hthread, time))
		return ERR_TIMEOUT;
	return 0;
}

void* sys_load_library(char* name)
{
	return (void*)LoadLibraryA(name);
}
void sys_free_library(void* handle)
{
	FreeLibrary((HMODULE)handle);
}
void* sys_get_lib_proc(void* handle,char* procname)
{
	return (void*)GetProcAddress((HMODULE)handle,procname);
}
