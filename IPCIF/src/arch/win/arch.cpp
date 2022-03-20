#include "arch.h"
#include "common_request.h"
#include "syswin.h"
#include <string.h>
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>
#define MS_PROCESSOR_ARCHITECTURE_IA64  6
#define MS_PROCESSOR_ARCHITECTURE_AMD64 9
typedef void (WINAPI* lpfnGetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);
class SystemArch
{
public:
	SystemArch()
	{
		arch = 64;
		HMODULE hKernel32Dll = LoadLibraryA("kernel32.dll");
		if (hKernel32Dll == NULL)
			return;
		SYSTEM_INFO si;
		lpfnGetNativeSystemInfo fnGetNativeSystemInfo = (lpfnGetNativeSystemInfo)GetProcAddress(hKernel32Dll, "GetNativeSystemInfo");
		if (fnGetNativeSystemInfo == NULL)
		{
			FreeLibrary(hKernel32Dll);
			return;
		}
		//GetSystemInfo(&si);
		fnGetNativeSystemInfo(&si);
		FreeLibrary(hKernel32Dll);
		arch=(si.wProcessorArchitecture==MS_PROCESSOR_ARCHITECTURE_IA64
			||si.wProcessorArchitecture==MS_PROCESSOR_ARCHITECTURE_AMD64?64:32);
	}
	int get_arch_bits()
	{
		return arch;
	}
private:
	int arch;
};
typedef struct
{
	DWORD_PTR Filler64[4];
	LPVOID InfoBlockAddress;
} __PEB;
typedef struct
{
	DWORD Filler[4];
	DWORD_PTR Filler64[13];
	LPVOID wszCmdLineAddress;
} __INFOBLOCK;
typedef NTSTATUS(CALLBACK* PFN_NTQUERYINFORMATIONPROCESS)(
	HANDLE ProcessHandle,
	PROCESSINFOCLASS ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength OPTIONAL
	);
static NTSTATUS _NtQueryInformationProcess(
	HANDLE hProcess,
	PROCESSINFOCLASS pic,
	PVOID pPI,
	ULONG cbSize,
	PULONG pLength)
{
	HMODULE hNtDll = LoadLibraryA("ntdll.dll");
	if (hNtDll == NULL)
		return -1;
	NTSTATUS lStatus = -1;
	PFN_NTQUERYINFORMATIONPROCESS pfnNtQIP = (PFN_NTQUERYINFORMATIONPROCESS)GetProcAddress(hNtDll, "NtQueryInformationProcess");
	if (pfnNtQIP != NULL)
		lStatus = pfnNtQIP(hProcess, pic, pPI, cbSize, pLength);
	FreeLibrary(hNtDll);
	return lStatus;
}
#define safe_return(h) \
	{ \
		CloseHandle(h); \
		return 0; \
	}
static HANDLE GetProcessCmdLine(DWORD PID, WCHAR* szCmdLine, DWORD Size)
{
	if ((PID <= 0) || (szCmdLine == NULL) || (Size == 0))
		return 0;
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, PID);
	if (!VALID(hProcess))
		return 0;
	BOOL isX64Proc=FALSE;
	if (!IsWow64Process(hProcess, &isX64Proc))
		safe_return(hProcess);
#ifdef CONFIG_X64
	isX64Proc =! isX64Proc;
	if (!isX64Proc)
		safe_return(hProcess);
#else
	static SystemArch sa;
	sa.get_arch_bits() == 64 ? (isX64Proc = !isX64Proc) : 0;
	if (isX64Proc)
		safe_return(hProcess);
#endif
	bool ret = true;
	DWORD dwSize = 0;
	SIZE_T size = 0;
	PROCESS_BASIC_INFORMATION pbi;
	__PEB PEB;
	__INFOBLOCK Block;
	int buflen;
	int iret = _NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), (PULONG)&dwSize);
	if (iret < 0)
		safe_return(hProcess);
	if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &PEB, sizeof(PEB), &size))
		safe_return(hProcess);
	if (!ReadProcessMemory(hProcess, PEB.InfoBlockAddress, &Block, sizeof(Block), &size))
		safe_return(hProcess);
	buflen = (Size-1) * sizeof(WCHAR);
	memset(szCmdLine, 0, (size_t)buflen+2);
	if (!ReadProcessMemory(hProcess, Block.wszCmdLineAddress, szCmdLine, buflen, &size))
		safe_return(hProcess);
	return hProcess;
}
static bool simple_unicode2ansi(const WCHAR* punicode_str, char* pansi_str)
{
	const WCHAR* pu;
	char* pa;
	for(pu=punicode_str,pa=pansi_str;*pu!=0;pu++,pa++)
	{
		signed short c=*pu;
		if(c>=128||c<0)
			return false;
		*pa=(signed char)c;
	}
	*pa=0;
	return true;
}
static bool parse_cmdline(const char* cmdline, char* exe, char* args)
{
	if(*cmdline=='\"')
	{
		const char* p=strchr(cmdline+1,'\"');
		if(p==NULL)
			return false;
		uint len=p-(cmdline+1);
		memcpy(exe,cmdline+1,len);
		exe[len]=0;
		for(p++;*p==' ';p++);
		strcpy(args,p);
	}
	else
	{
		const char* p=strchr(cmdline,' ');
		if(p==NULL)
		{
			strcpy(exe,cmdline);
			*args=0;
			return true;
		}
		uint len=p-cmdline;
		memcpy(exe,cmdline,len);
		exe[len]=0;
		for(;*p==' ';p++);
		strcpy(args,p);
	}
	return true;
}
static inline const char* find_exe_file(const char* exe)
{
	const char* p=strrchr(exe,'\\');
	return p==NULL?exe:(p+1);
}
static inline void* get_process_cmd(DWORD pid,char* exe,char* args)
{
	WCHAR wszCmdLine[1024];
	char szCmdLine[1024];
	char exeName[256];
	HANDLE hproc=GetProcessCmdLine(pid,wszCmdLine,1024);
	if(!VALID(hproc))
		return NULL;
	if(!simple_unicode2ansi(wszCmdLine,szCmdLine))
		goto failed;
	if(!parse_cmdline(szCmdLine,exeName,args))
		goto failed;
	strcpy(exe,find_exe_file(exeName));
	return (void*)hproc;
failed:
	CloseHandle(hproc);
	return NULL;
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
static inline void* match_process(PROCESSENTRY32* pe,
	const char* exe, const char* args)
{
	char exeName[256],Args[512];
	if(strcmp(pe->szExeFile,exe)!=0)
		return NULL;
	HANDLE hproc=get_process_cmd(pe->th32ProcessID,exeName,Args);
	if(!VALID(hproc))
		return NULL;
	if(strcmp(exe,exeName)!=0)
		goto failed;
	if(strcmp(args,Args)!=0)
		goto failed;
	return (void*)hproc;
failed:
	CloseHandle(hproc);
	return NULL;
}
#define final_find(h,cnt,end_tag) \
	if(VALID(h)) \
	{ \
		if((--cnt)==0) \
		{ \
			goto end_tag; \
		} \
		else \
		{ \
			sys_close_process(h); \
			h=0; \
		} \
	}
void* arch_get_process_native(const char* cmdline,bool find_dup)
{
	HANDLE hSnap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
	if(!VALID(hSnap))
		return NULL;
	PROCESSENTRY32 pe;
	BOOL bMore;
	void* h=NULL;
	char exe[256],args[512];
	const char* p;
	int cnt_left=find_dup?2:1;
	if(!parse_cmdline(cmdline,exe,args))
		goto end;
	p=find_exe_file(exe);
	pe.dwSize=sizeof(pe);
	bMore=Process32First(hSnap, &pe);
	if(bMore)
	{
		h=match_process(&pe,p,args);
		final_find(h,cnt_left,end);
	}
	while(bMore)
	{
		if(bMore=Process32Next(hSnap,&pe))
		{
			h=match_process(&pe,p,args);
			final_find(h,cnt_left,end);
		}
	}
end:
	CloseHandle(hSnap);
	return h;
}
void* arch_get_process_if(const proc_data& data)
{
	if(data.ifproc.empty())
		return NULL;
	uint pid=0;
	if(0!=send_cmd_getid(&pid,data.ifproc[0].hif,(char*)data.ifproc[0].id.c_str()))
		return NULL;
	return (void*)OpenProcess(PROCESS_ALL_ACCESS,TRUE,pid);
}
DLLAPI(void*) arch_get_process(const proc_data& data)
{
	if(data.ambiguous==E_AMBIG_NONE)
		return sys_get_process((char*)data.cmdline.c_str());
	if(data.type==E_PROCTYPE_TOOL)
		return arch_get_process_if(data);
	else
		return arch_get_process_native(data.cmdline.c_str(),false);
}
DLLAPI(bool) arch_has_dup_process(const proc_data& data)
{
	if(data.ambiguous==E_AMBIG_NONE)
		return sys_has_dup_process((char*)data.cmdline.c_str());
	void* hproc;
	if(data.type==E_PROCTYPE_TOOL)
		hproc=arch_get_process_if(data);
	else
		hproc=arch_get_process_native(data.cmdline.c_str(),true);
	if(VALID(hproc))
	{
		sys_close_process(hproc);
		return true;
	}
	return false;
}
DLLAPI(int) arch_get_current_process_cmdline(char* cmdline)
{
	DWORD pid=GetCurrentProcessId();
	char exeName[256],Args[512];
	HANDLE hproc=get_process_cmd(pid,exeName,Args);
	if(!VALID(hproc))
		return ERR_GENERIC;
	CloseHandle(hproc);
	char* p=cmdline;
	strcpy(p,exeName);
	p+=strlen(p);
	if(Args[0]!=0)
	{
		*(p++)=' ';
		strcpy(p,Args);
	}
	return 0;
}