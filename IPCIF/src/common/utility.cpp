#include "utility.h"
#include "path.h"
#include "config.h"
#include "config_val.h"
#include "syslog.h"
#include "mutex.h"
#include "process_data.h"
#include "Integer64.h"
#include "arch.h"
#include <stdarg.h>
#define IF_INFO_SIZE IF_USAGE_SIZE
#define IF_INFO_TAG_SIZE 41
DEFINE_UINT_VAL(sys_log_level,0);
DEFINE_STRING_VAL(if_user,"");
DEFINE_UINT_VAL(num_sysfs_buffer_blocks,4);
DEFINE_SIZE_VAL(sysfs_buffer_size,1024);
static cmutex mt_get_sid;
const char* g_info[]={"info",NULL};
const char* g_error[]={"info","error",NULL};
const char* const * sys_log_names[]=
{
	g_info,
	g_error,
};
class process_identifier
{
public:
	process_identifier();
	int GetProcStat(process_stat* pstat);
	int GetMainInfo();
	int Init();
	void Exit();
	int GetStorageInfo(char* name,if_id_info_storage* pifinfo);
	void* FindFirstExecutable(process_stat* pstat);
	int FindNextExecutable(void* handle,process_stat* pstat);
	void FindExecutableClose(void* handle);
	char name[FILE_NAME_SIZE];
	char cur_dir[MAX_DIR_SIZE];
	char exe_dir[MAX_DIR_SIZE];
	process_stat proc_stat;
	process_stat* cur_stat;
	if_ids ifs;
	main_process_info main_info;
	if_ids loader_ifs;
	if_ids manager_ifs;
	ConfigProfile config;
	LogSys log_sys;
	bool is_external;
	int init_error;
};
process_identifier _hCurrentProc;
DLLAPI(dword) get_session_id()
{
	static dword id=0;
	sys_wait_sem(mt_get_sid.get_mutex());
	dword i=(id++);
	sys_signal_sem(mt_get_sid.get_mutex());
	return i;
}
DLLAPI(process_stat*) get_current_executable_stat()
{
	return _hCurrentProc.cur_stat;
}
DLLAPI(char*) get_current_executable_name()
{
	return _hCurrentProc.name;
}
DLLAPI(void*) get_current_executable_id()
{
	return _hCurrentProc.is_external?NULL:_hCurrentProc.cur_stat->id;
}
DLLAPI(char*) get_current_directory()
{
	return _hCurrentProc.cur_dir;
}
DLLAPI(char*) get_current_executable_path()
{
	return _hCurrentProc.exe_dir;
}
DLLAPI(int) is_launcher()
{
	return _hCurrentProc.is_external?0:_hCurrentProc.cur_stat->type==E_PROCTYPE_LAUNCHER;
}
DLLAPI(int) is_manager()
{
	return _hCurrentProc.is_external?0:_hCurrentProc.cur_stat->type==E_PROCTYPE_MANAGER;
}
DLLAPI(int) is_managed()
{
	return _hCurrentProc.is_external?0:_hCurrentProc.cur_stat->type==E_PROCTYPE_MANAGED;
}
DLLAPI(int) is_tool()
{
	return _hCurrentProc.is_external?0:_hCurrentProc.cur_stat->type==E_PROCTYPE_TOOL;
}
DLLAPI(int) is_extern_tool()
{
	return _hCurrentProc.is_external?1:0;
}
DLLAPI(char*) get_if_user()
{
	return (char*)if_user.c_str();
}
DLLAPI(uint) get_num_sysfs_buffer_blocks()
{
	return num_sysfs_buffer_blocks;
}
DLLAPI(uint) get_sysfs_buffer_size()
{
	return sysfs_buffer_size;
}
DLL int __LOGFILE(uint level,uint ftype,char* file,int line,char* format,...)
{
	if(sys_log_level<=level)
		return 0;
	string str;
	const char* ptr;
	{
		char buf[1025];
		va_list args;
		va_start(args,format);
		vsprintf(buf,format,args);
		va_end(args);
		str=buf;
		ptr=strrchr(file,_dir_symbol);
		if (ptr==NULL)
			ptr=file;
		else
			ptr++;
	}
	return _hCurrentProc.log_sys.Log(sys_log_names[ftype],ptr,line,str);
}
DLLAPI(int) get_executable_info(process_stat* info)
{
	return _hCurrentProc.GetProcStat(info);
}
DLLAPI(int) mainly_initial()
{
	return _hCurrentProc.Init();
}
DLLAPI(void) mainly_exit()
{
	return _hCurrentProc.Exit();
}
DLLAPI(void*) get_executable_id(char* name)
{
	if(strlen(name)>FILE_NAME_SIZE)
		return NULL;
	process_stat pstat;
	init_process_stat(&pstat,name);
	if(0!=get_executable_info(&pstat))
		return NULL;
	return pstat.id;
}
process_identifier::process_identifier()
{
	init_error=0;
	is_external=false;
	cur_stat=NULL;
	if(0!=(init_error=sys_get_current_process_path(exe_dir,MAX_DIR_SIZE)))
		return;
	string absolute_path;
	if(0!=(init_error=get_absolute_path(string(exe_dir),string(CFG_FILE_PATH),absolute_path,sys_is_absolute_path)))
		return;
	if(0!=(init_error=config.LoadConfigFile(absolute_path.c_str())))
		return;
	memset(name,0,sizeof(name));
	if(0!=(init_error=sys_get_current_process_name(name,sizeof(name))))
		return;
	init_process_stat(&proc_stat,name);
	init_process_stat(&main_info.loader_exe_info,"");
	init_process_stat(&main_info.manager_exe_info,"");
	proc_stat.ifs=&ifs;
	main_info.loader_exe_info.ifs=&loader_ifs;
	main_info.manager_exe_info.ifs=&manager_ifs;
	if(0!=(init_error=GetMainInfo()))
		return;
	int ret=GetProcStat(&proc_stat);
	if(ret==ERR_EXEC_NOT_FOUND)
		is_external=true;
	else if(ret!=0)
	{
		init_error=ret;
		return;
	}
	if(0!=(init_error=sys_get_current_dir(cur_dir,MAX_DIR_SIZE)))
		return;
	if(!is_external)
		cur_stat=&proc_stat;
	else
		return;
	if(proc_stat.local_cur_dir)
	{
		if(0!=(init_error=sys_set_current_dir(exe_dir)))
			return;
	}
}
inline int __get_stat__(process_stat* pstat,const string& exec,ConfigProfile& config)
{
	uint id;
	string cmdline;
	if(!config.GetCongfigItem(exec,CFG_TAG_EXEC_ID,id))
		return ERR_EXEC_INFO_NOT_FOUND;
	pstat->id=uint_to_ptr(id);
	if(!VALID(pstat->id))
		return ERR_EXEC_INFO_NOT_VALID;
	bool uniq,ld,log=true;
	uint ambiguous=defaultAmbiguousLvl;
	if(!config.GetCongfigItem(exec,CFG_TAG_EXEC_UNIQUE,uniq))
		return ERR_EXEC_INFO_NOT_FOUND;
	if(!config.GetCongfigItem(exec,CFG_TAG_EXEC_LDIR,ld))
		return ERR_EXEC_INFO_NOT_FOUND;
	if(config.GetCongfigItem(exec,CFG_TAG_EXEC_CMDLINE,cmdline))
		strcpy(pstat->cmdline,cmdline.c_str());
	else
		*pstat->cmdline=0;
	pstat->unique_instance=(uniq?1:0);
	pstat->local_cur_dir=ld?1:0;
	proc_type type=E_PROCTYPE_NONE;
	string strtype;
	if(config.GetCongfigItem(exec,CFG_TAG_EXEC_TYPE,strtype))
	{
		if(strtype==CFG_TAG_EXEC_TYPE_LAUNCHER)
			type=E_PROCTYPE_LAUNCHER;
		else if(strtype==CFG_TAG_EXEC_TYPE_MANAGER)
			type=E_PROCTYPE_MANAGER;
		else if(strtype==CFG_TAG_EXEC_TYPE_MANAGED)
			type=E_PROCTYPE_MANAGED;
		else if(strtype==CFG_TAG_EXEC_TYPE_TOOL)
			type=E_PROCTYPE_TOOL;
		else
			return ERR_EXEC_INFO_NOT_VALID;
	}
	pstat->type=type;
	config.GetCongfigItem(exec,CFG_TAG_EXEC_LOG,log);
	pstat->log=log?1:0;
	config.GetCongfigItem(exec,CFG_TAG_EXEC_AMBIG,ambiguous);
	switch((ambig_lvl)ambiguous)
	{
	case E_AMBIG_NONE:
	case E_AMBIG_USER:
	case E_AMBIG_CMDLINE:
	case E_AMBIG_PROC_ID:
		pstat->ambiguous=(ambig_lvl)ambiguous;
		break;
	default:
		pstat->ambiguous=defaultAmbiguousLvl;
		break;
	}
	if(pstat->ifs==NULL)
		return 0;
	char buf[IF_INFO_SIZE];
	string str;
	pstat->ifs->count=0;
	memset(pstat->ifs->if_id,0,sizeof(if_id_info)*MAX_IF_COUNT);
	for(int i=0;i<MAX_IF_COUNT;i++)
	{
		sprintf(buf,CFG_TAG_EXEC_IF,i);
		if(!config.GetCongfigItem(exec,buf,str))
			break;
		strcpy(pstat->ifs->if_id[pstat->ifs->count].if_name,str.c_str());
		uint cnt=1;
		string usage(i==0?"cmd":"data");
		int prior=0;
		sprintf(buf,CFG_TAG_EXEC_IF_CNT,i);
		config.GetCongfigItem(exec,buf,cnt);
		sprintf(buf,CFG_TAG_EXEC_IF_USAGE,i);
		config.GetCongfigItem(exec,buf,usage);
		sprintf(buf,CFG_TAG_EXEC_IF_PRIOR,i);
		config.GetCongfigItem(exec,buf,prior);
		pstat->ifs->if_id[pstat->ifs->count].thrdcnt=(cnt<1?1:cnt);
		pstat->ifs->if_id[pstat->ifs->count].prior=prior;
		strcpy(pstat->ifs->if_id[pstat->ifs->count++].usage,usage.c_str());
	}
	return 0;
}
int process_identifier::GetMainInfo()
{
	int ret=0;
	string str;
	bool launcher_found=false,manager_found=false;
	for(ConfigProfile::iterator it=config.BeginIterate(CONFIG_SECTION_EXEC);it;++it)
	{
		if(it->first=="")
			continue;
		const string& s=it->first;
		string file;
		if(!config.GetCongfigItem(s,CFG_TAG_EXEC_FILE,file))
			continue;
		if(!config.GetCongfigItem(s,CFG_TAG_EXEC_TYPE,str))
			continue;
		if(!launcher_found&&str==CFG_TAG_EXEC_TYPE_LAUNCHER)
		{
			launcher_found=true;
			strcpy(main_info.loader_exe_info.file,file.c_str());
			if(0!=(ret=__get_stat__(&main_info.loader_exe_info,s,config)))
				return ret;
		}
		if(!manager_found&&str==CFG_TAG_EXEC_TYPE_MANAGER)
		{
			manager_found=true;
			strcpy(main_info.manager_exe_info.file,file.c_str());
			if(0!=(ret=__get_stat__(&main_info.manager_exe_info,s,config)))
				return ret;
		}
		if(launcher_found&&manager_found)
			return 0;
	}
	return ERR_EXEC_NOT_FOUND;
}
int process_identifier::GetProcStat(process_stat* pstat)
{
	int ret=0;
	proc_data pdata;
	string cmd;
	bool ambig=false,ambig_stat_inited=false;
	pstat->file[FILE_NAME_SIZE-1]=0;
	for(ConfigProfile::iterator it=config.BeginIterate(CONFIG_SECTION_EXEC);it;++it)
	{
		if(it->first=="")
			continue;
		const string& s=it->first;
		string file;
		if(!config.GetCongfigItem(s,CFG_TAG_EXEC_FILE,file))
			continue;
		if(ambig)
			init_process_stat(pstat,(char*)pdata.name.c_str());
		if(file!=pstat->file)
			continue;
		if(0!=(ret=__get_stat__(pstat,s,config)))
			return ret;
		if(pstat->ambiguous>E_AMBIG_USER)
		{
			ambig=true;
			if(!ambig_stat_inited)
			{
				ambig_stat_inited=true;
				char cmdline_buf[1024];
				arch_get_current_process_cmdline(cmdline_buf);
				cmd=cmdline_buf;
			}
			insert_proc_data_cmdline(pdata,*pstat);
			if(pdata.cmdline!=cmd)
				continue;
		}
		return 0;
	}
	return ERR_EXEC_NOT_FOUND;
}
DLLAPI(main_process_info*) get_main_info()
{
	return &_hCurrentProc.main_info;
}
DLLAPI(if_ids*) get_if_ids()
{
	return &_hCurrentProc.ifs;
}
DLLAPI(int) get_if_storage_info(char* name,if_id_info_storage* pifinfo)
{
	return _hCurrentProc.GetStorageInfo(name,pifinfo);
}
DLLAPI(void*) find_first_exe(process_stat* pstat)
{
	return _hCurrentProc.FindFirstExecutable(pstat);
}
DLLAPI(int) find_next_exe(void* handle,process_stat* pstat)
{
	return _hCurrentProc.FindNextExecutable(handle,pstat);
}
DLLAPI(void) find_exe_close(void* handle)
{
	return _hCurrentProc.FindExecutableClose(handle);
}
int process_identifier::Init()
{
	if(init_error!=0)
		return init_error;
	if(0!=(init_error=config_val_container::get_val_container()->config_value(&config)))
		return init_error;
	if((!is_external)&&proc_stat.unique_instance)
	{
		proc_data this_proc;
		insert_proc_data_cmdline(this_proc,proc_stat);
		if(arch_has_dup_process(this_proc))
		{
			init_error=ERR_DUP_PROCESS;
			return init_error;
		}
	}
	if((!is_external)&&_hCurrentProc.proc_stat.log)
	if(0!=(init_error=log_sys.Init()))
		return init_error;
	return 0;
}
void process_identifier::Exit()
{
	log_sys.Exit();
}
int process_identifier::GetStorageInfo(char* name,if_id_info_storage* pifinfo)
{
	memset(pifinfo,0,sizeof(if_id_info_storage));
	char buf[IF_INFO_TAG_SIZE];
	sprintf(buf,CFG_SECTION_IF_INFO,name);
	string info;
	if(config.GetCongfigItem(buf,CFG_IF_STO_MOD_NAME,info))
		strcpy(pifinfo->mod_name,(char*)info.c_str());
	else
		strcpy(pifinfo->mod_name,DEF_STO_MOD_NAME);
	if(!config.GetCongfigItem(buf,CFG_IF_STO_DRV_NAME,info))
		goto failed;
	strcpy(pifinfo->drv_name,(char*)info.c_str());
	if(!config.GetCongfigItem(buf,CFG_IF_STO_MOUNT_CMD,info))
		goto failed;
	strcpy(pifinfo->mount_cmd,(char*)info.c_str());
	if(!config.GetCongfigItem(buf,CFG_IF_STO_FORMAT_CMD,info))
		goto failed;
	strcpy(pifinfo->format_cmd,(char*)info.c_str());
	return 0;
failed:
	memset(pifinfo,0,sizeof(if_id_info_storage));
	return ERR_FS_DEV_NOT_FOUND;
}
void* process_identifier::FindFirstExecutable(process_stat* pstat)
{
	ConfigProfile::iterator* it=new ConfigProfile::iterator(config.BeginIterate(CONFIG_SECTION_EXEC));
	for(;(*it)&&(*it)->first=="";++(*it));
	if(!(*it))
	{
		delete it;
		return NULL;
	}
	string file;
	config.GetCongfigItem((*it)->first,CFG_TAG_EXEC_FILE,file);
	strcpy(pstat->file,file.c_str());
	if(0!=__get_stat__(pstat,(*it)->first,config))
	{
		delete it;
		return NULL;
	}
	++(*it);
	return it;
}
int process_identifier::FindNextExecutable(void* handle,process_stat* pstat)
{
	ConfigProfile::iterator* it=(ConfigProfile::iterator*)handle;
	for(;(*it)&&(*it)->first=="";++(*it));
	if(!(*it))
		return 0;
	string file;
	config.GetCongfigItem((*it)->first,CFG_TAG_EXEC_FILE,file);
	strcpy(pstat->file,file.c_str());
	if(0!=__get_stat__(pstat,(*it)->first,config))
		return 0;
	++(*it);
	return 1;
}
void process_identifier::FindExecutableClose(void* handle)
{
	ConfigProfile::iterator* it=(ConfigProfile::iterator*)handle;
	if(it!=NULL)
		delete it;
}
DLL void reset_datagram_base(void* data)
{
	((datagram_base*)data)->ret=ERR_GENERIC;
}
spec_char_verifier::spec_char_verifier(const byte* ch,uint nch,bool balphanum,bool _etc_spec):etc_spec(_etc_spec)
{
	memset(alphabet,0,128*sizeof(bool));
	if(balphanum)
	{
		for(int i=(int)'a';i<=(int)'z';i++)
			alphabet[i]=true;
		for(int i=(int)'A';i<=(int)'Z';i++)
			alphabet[i]=true;
		for(int i=(int)'0';i<=(int)'9';i++)
			alphabet[i]=true;
	}
	for(int i=0;i<(int)nch;i++)
		alphabet[(uint)(uchar)ch[i]]=true;
}
bool spec_char_verifier::is_spec(char c) const
{
	int i=(int)c;
	if(i<0||i>=128)
		return etc_spec;
	else
		return alphabet[i];
}
struct rcopy_param
{
	char* from;
	char* to;
	char* start;
	char* tstart;
	file_recurse_callback* callback;
	file_recurse_cbdata* cbdata;
};
static int cb_copy(char* name, dword type, void* param, char dsym)
{
	int ret=0;
	rcopy_param* cpy=(rcopy_param*)param;
	file_recurse_callback* callback=cpy->callback;
	rcopy_param newcpy=*cpy;
	*(newcpy.start++)=dsym;
	*(newcpy.tstart++)=dsym;
	strcpy(newcpy.start,name);
	newcpy.start+=strlen(name);
	strcpy(newcpy.tstart,name);
	newcpy.tstart+=strlen(name);
	if(type==FILE_TYPE_DIR)
	{
		int retc=ret=callback->_mkdir_(newcpy.to);
		newcpy.cbdata!=NULL?ret=newcpy.cbdata->cb(ret,newcpy.to,type,newcpy.cbdata->param,dsym):0;
		return (ret==0&&retc==0)?callback->_ftraverse_(newcpy.from,cb_copy,&newcpy):ret;
	}
	else
	{
		ret=callback->_fcopy_(newcpy.from,newcpy.to);
		return newcpy.cbdata!=NULL?newcpy.cbdata->cb(ret,newcpy.to,type,newcpy.cbdata->param,dsym):ret;
	}
}
DLLAPI(int) recurse_fcopy(char* from, char* to, file_recurse_callback* callback, file_recurse_cbdata* cbdata, char dsym)
{
	int ret=0;
	dword type=0;
	if(0!=(ret=callback->_fstat_(from,&type)))
		return ret;
	rcopy_param rparam;
	rparam.from=new char[1024];
	rparam.to=new char[1024];
	strcpy(rparam.from,from);
	strcpy(rparam.to,to);
	rparam.start=rparam.from+strlen(rparam.from);
	rparam.tstart=rparam.to+strlen(rparam.to);
	rparam.callback=callback;
	rparam.cbdata=cbdata;
	if(rparam.start>rparam.from&&*(rparam.start-1)==dsym)
		*(--rparam.start)=0;
	if(rparam.tstart>rparam.to&&*(rparam.tstart-1)==dsym)
		*(--rparam.tstart)=0;
	if(strcmp(rparam.from,rparam.to)==0)
		goto end;
	if(type==FILE_TYPE_DIR)
		ret=callback->_mkdir_(rparam.to);
	else
		ret=callback->_fcopy_(from,to);
	{
		int retc=ret;
		cbdata!=NULL?ret=cbdata->cb(ret,rparam.to,type,cbdata->param,dsym):0;
		if(type==FILE_TYPE_DIR)
			(ret==0&&retc==0)?ret=callback->_ftraverse_(rparam.from,cb_copy,&rparam):0;
	}
end:
	delete[] rparam.from;
	delete[] rparam.to;
	return ret;
}
struct rdel_param
{
	char* path;
	char* start;
	file_recurse_callback* callback;
	file_recurse_cbdata* cbdata;
	int quitdel;
};
static int cb_delete(char* name, dword type, void* param, char dsym)
{
	int ret=0;
	rdel_param* del=(rdel_param*)param;
	file_recurse_callback* callback=del->callback;
	rdel_param newdel=*del;
	newdel.quitdel=0;
	*(newdel.start++)=dsym;
	strcpy(newdel.start,name);
	newdel.start+=strlen(name);
	if(type==FILE_TYPE_DIR)
		ret=callback->_ftraverse_(newdel.path,cb_delete,&newdel);
	*(newdel.start)=0;
	int retc=0;
	(ret==0&&newdel.quitdel==0)?ret=retc=callback->_fdelete_(newdel.path):0;
	ret!=0?del->quitdel=ret:(newdel.quitdel!=0?del->quitdel=newdel.quitdel:0);
	return (((ret==0&&newdel.quitdel==0)||retc!=0)&&newdel.cbdata!=NULL)?newdel.cbdata->cb(ret,newdel.path,type,newdel.cbdata->param,dsym):ret;
}
DLLAPI(int) recurse_fdelete(char* pathname, file_recurse_callback* callback, file_recurse_cbdata* cbdata, char dsym)
{
	dword type=0;
	int ret=callback->_fstat_(pathname,&type);
	if(ret==ERR_PATH_NOT_EXIST)
		return 0;
	if(ret!=0)
		return ret;
	rdel_param rparam;
	rparam.path=new char[1024];
	strcpy(rparam.path,pathname);
	rparam.start=rparam.path+strlen(rparam.path);
	rparam.callback=callback;
	rparam.cbdata=cbdata;
	rparam.quitdel=0;
	if(rparam.start>rparam.path&&*(rparam.start-1)==dsym)
		*(--rparam.start)=0;
	if(type==FILE_TYPE_DIR)
		ret=callback->_ftraverse_(rparam.path,cb_delete,&rparam);
	*rparam.start=0;
	int retc=0;
	(ret==0&&rparam.quitdel==0)?ret=retc=callback->_fdelete_(rparam.path):0;
	(((ret==0&&rparam.quitdel==0)||retc!=0)&&cbdata!=NULL)?ret=cbdata->cb(ret,rparam.path,type,cbdata->param,dsym):0;
	delete[] rparam.path;
	return ret;
}
struct rhandle_param
{
	char* path;
	char* start;
	file_recurse_handle_callback* callback;
	void* param;
	file_recurse_cbdata* cbdata;
	bool root_first;
};
static int cb_fhandle(char* name,dword type,void* param,char dsym);
static inline void __f_traverse__(rhandle_param* rparam,dword type,int& ret)
{
	if(type==FILE_TYPE_DIR)
	{
		ret==0?(ret=rparam->callback->_ftraverse_(rparam->path,cb_fhandle,rparam)):0;
		*(rparam->start)=0;
	}
}
static inline void __f_handle__(rhandle_param* rparam,dword type,int& ret,char dsym)
{
	int retc=0;
	ret==0?(ret=retc=rparam->callback->_handler_(rparam->path,type,rparam->param,dsym)):0;
	((ret==0||(retc!=0))&&rparam->cbdata!=NULL)?ret=rparam->cbdata->cb(ret,rparam->path,type,rparam->cbdata->param,dsym):0;
}
static int cb_fhandle(char* name,dword type,void* param,char dsym)
{
	int ret=0;
	rhandle_param* hstat=(rhandle_param*)param;
	file_recurse_handle_callback* callback=hstat->callback;
	rhandle_param newstat=*hstat;
	*(newstat.start++)=dsym;
	strcpy(newstat.start,name);
	newstat.start+=strlen(name);
	if(newstat.root_first)
	{
		__f_handle__(&newstat,type,ret,dsym);
		__f_traverse__(&newstat,type,ret);
	}
	else
	{
		__f_traverse__(&newstat,type,ret);
		__f_handle__(&newstat,type,ret,dsym);
	}
	return ret;
}
DLLAPI(int) recurse_fhandle(char* pathname,file_recurse_handle_callback* callback,bool root_first,void* param,file_recurse_cbdata* cbdata,char dsym)
{
	dword type=0;
	int ret=0;
	if(0!=(ret=callback->_fstat_(pathname,&type)))
		return ret;
	rhandle_param rparam;
	rparam.path=new char[1024];
	strcpy(rparam.path,pathname);
	rparam.start=rparam.path+strlen(rparam.path);
	rparam.callback=callback;
	rparam.param=param;
	rparam.cbdata=cbdata;
	rparam.root_first=root_first;
	if(rparam.start>rparam.path&&*(rparam.start-1)==dsym)
		*(--rparam.start)=0;
	if(rparam.root_first)
	{
		__f_handle__(&rparam,type,ret,dsym);
		__f_traverse__(&rparam,type,ret);
	}
	else
	{
		__f_traverse__(&rparam,type,ret);
		__f_handle__(&rparam,type,ret,dsym);
	}
	delete[] rparam.path;
	return ret;
}
static int cb_stat(char* pathname,dword type,void* param,char dsym)
{
	path_recurse_stat* pstat=(path_recurse_stat*)param;
	int ret=0;
	if(type==FILE_TYPE_NORMAL)
	{
		UInteger64 fsize(pstat->size.sizel,&pstat->size.sizeh),inc_size;
		void* hfile=sys_fopen(pathname,FILE_OPEN_EXISTING|FILE_READ);
		if(!VALID(hfile))
			return ERR_OPEN_FILE_FAILED;
		if(0!=(ret=sys_get_file_size(hfile,&inc_size.low,&inc_size.high)))
		{
			sys_fclose(hfile);
			return ret;
		}
		sys_fclose(hfile);
		fsize+=inc_size;
		pstat->size.sizel=fsize.low,pstat->size.sizeh=fsize.high;
	}
	switch(type)
	{
	case FILE_TYPE_NORMAL:
		pstat->nfile++;
		break;
	case FILE_TYPE_DIR:
		pstat->ndir++;
		break;
	}
	return 0;
}
DLLAPI(int) sys_recurse_fcopy(char* from,char* to,file_recurse_cbdata* cbdata)
{
	file_recurse_callback cb={sys_fstat,sys_ftraverse,sys_mkdir,sys_fcopy,sys_fdelete};
	return recurse_fcopy(from,to,&cb,cbdata,_dir_symbol);
}
DLLAPI(int) sys_recurse_fdelete(char* pathname,file_recurse_cbdata* cbdata)
{
	file_recurse_callback cb={sys_fstat,sys_ftraverse,sys_mkdir,sys_fcopy,sys_fdelete};
	return recurse_fdelete(pathname,&cb,cbdata,_dir_symbol);
}
DLLAPI(int) sys_recurse_fstat(char* pathname,path_recurse_stat* pstat,file_recurse_cbdata* cbdata)
{
	pstat->nfile=0;
	pstat->ndir=0;
	pstat->size.sizeh=pstat->size.sizel=0;
	file_recurse_handle_callback cb={sys_fstat,sys_ftraverse,cb_stat};
	return recurse_fhandle(pathname,&cb,true,pstat,cbdata,_dir_symbol);
}