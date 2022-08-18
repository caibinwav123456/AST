#ifndef _UTILITY_H_
#define _UTILITY_H_
#include "common.h"
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FILE_NAME_SIZE 257
#define MAX_DIR_SIZE   1025
#define IF_ID_SIZE     51
#define IF_USAGE_SIZE  21
#define MAX_IF_COUNT   4
#define LOGFILE(level,ftype,format,...) __LOGFILE(level,ftype,__FILE__,__LINE__,format,##__VA_ARGS__)
#define __instance_id(proc_id) hiword(proc_id)
#define __process_id(proc_id) loword(proc_id)
enum log_ftype
{
	log_ftype_info=0,
	log_ftype_error,
};
enum proc_type
{
	E_PROCTYPE_NONE=0,
	E_PROCTYPE_TOOL,
	E_PROCTYPE_LAUNCHER,
	E_PROCTYPE_MANAGER,
	E_PROCTYPE_MANAGED,
};
enum ambig_lvl
{
	E_AMBIG_NONE=0,
	E_AMBIG_USER,
	E_AMBIG_CMDLINE,
	E_AMBIG_PROC_ID,
};
#define defaultAmbiguousLvl E_AMBIG_USER
struct if_id_info
{
	char if_name[IF_ID_SIZE];
	char usage[IF_USAGE_SIZE];
	uint thrdcnt;
	int prior;
};
struct if_id_info_storage
{
	char mod_name[FILE_NAME_SIZE];
	char drv_name[FILE_NAME_SIZE];
	char mount_cmd[MAX_DIR_SIZE];
	char format_cmd[MAX_DIR_SIZE];
};
struct if_ids
{
	int count;
	if_id_info if_id[MAX_IF_COUNT];
};
struct main_process_info;
struct process_stat
{
	char file[FILE_NAME_SIZE];
	char cmdline[FILE_NAME_SIZE];
	void* id;
	int unique_instance;
	int local_cur_dir;
	int log;
	ambig_lvl ambiguous;
	proc_type type;
	if_ids* ifs;
};
struct main_process_info
{
	process_stat loader_exe_info;
	process_stat manager_exe_info;
};
struct file_recurse_cbdata
{
	int (*cb)(int, char*, dword, void*, char);
	void* param;
};
struct file_size
{
	uint sizeh;
	uint sizel;
};
struct path_recurse_stat
{
	file_size size;
	uint nfile;
	uint ndir;
};
struct file_recurse_callback
{
	int (*_fstat_)(char* pathname, dword* type);
	int (*_ftraverse_)(char* pathname, int(*cb)(char*, dword, void*, char), void* param);
	int (*_mkdir_)(char* path);
	int (*_fcopy_)(char* from, char* to);
	int (*_fdelete_)(char* pathname);
};
struct file_recurse_handle_callback
{
	int (*_fstat_)(char* pathname, dword* type);
	int (*_ftraverse_)(char* pathname, int(*cb)(char*, dword, void*, char), void* param);
	int (*_handler_)(char* pathname, dword, void*, char);
};
DLLAPI(int) get_executable_info(process_stat* info);
DLLAPI(int) mainly_initial();
DLLAPI(void) mainly_exit();
DLLAPI(void*) get_executable_id(char* name);
DLLAPI(dword) get_session_id();
DLLAPI(process_stat*) get_current_executable_stat();
DLLAPI(char*) get_current_executable_name();
DLLAPI(void*) get_current_executable_id();
DLLAPI(char*) get_current_directory();
DLLAPI(char*) get_current_executable_path();
DLLAPI(int) is_launcher();
DLLAPI(int) is_manager();
DLLAPI(int) is_managed();
DLLAPI(int) is_tool();
DLLAPI(int) is_extern_tool();
DLLAPI(char*) get_if_user();
DLLAPI(uint) get_num_sysfs_buffer_blocks();
DLLAPI(uint) get_sysfs_buffer_size();
DLLAPI(main_process_info*) get_main_info();
DLLAPI(if_ids*) get_if_ids();
DLLAPI(int) get_if_storage_info(char* name,if_id_info_storage* pifinfo);
DLLAPI(void*) find_first_exe(process_stat* pstat);
DLLAPI(int) find_next_exe(void* handle,process_stat* pstat);
DLLAPI(void) find_exe_close(void* handle);
DLL int __LOGFILE(uint level,uint ftype,char* file,int line,char* format,...);
DLLAPI(int) recurse_fcopy(char* from,char* to,file_recurse_callback* callback,file_recurse_cbdata* cbdata,char dsym);
DLLAPI(int) recurse_fdelete(char* pathname,file_recurse_callback* callback,file_recurse_cbdata* cbdata,char dsym);
DLLAPI(int) recurse_fhandle(char* pathname,file_recurse_handle_callback* callback,bool root_first,void* param,file_recurse_cbdata* cbdata,char dsym);
DLLAPI(char*) get_error_desc(int errcode);
DLLAPI(int) sys_recurse_fcopy(char* from,char* to,file_recurse_cbdata* cbdata);
DLLAPI(int) sys_recurse_fdelete(char* pathname,file_recurse_cbdata* cbdata);
DLLAPI(int) sys_recurse_fstat(char* pathname,path_recurse_stat* pstat,file_recurse_cbdata* cbdata);
DLL void reset_datagram_base(void* data);
#ifdef __cplusplus
}
#endif
inline void init_datagram_base(datagram_base* data, uint cmd, void* caller, dword sid)
{
	data->cmd=cmd;
	data->ret=ERR_GENERIC;
	data->caller=caller;
	data->sid=sid;
}
inline void init_process_stat(process_stat* pstat, char* name)
{
	memset(pstat,0,sizeof(process_stat));
	strcpy(pstat->file,name);
}
#define init_current_datagram_base(data, cmd) init_datagram_base(data, cmd, get_current_executable_id(), get_session_id())
#ifdef __cplusplus
class DLL spec_char_verifier
{
public:
	spec_char_verifier(const byte* ch,uint nch,bool balphanum=false,bool _etc_spec=false);
	bool is_spec(char c) const;
private:
	bool alphabet[128];
	bool etc_spec;
};
struct AstError
{
	char* file;
	int line;
	int id;
	AstError(char* _file, int _line, int _id)
	{
		file = _file, line = _line, id = _id;
	}
};
#define EXCEPTION(C) throw AstError(__FILE__,__LINE__,(C))
#endif
#endif