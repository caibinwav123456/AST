#include "utility.h"
#include "path.h"
#include "config.h"
#include "config_val.h"
#include "syslog.h"
#include "mutex.h"
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
	main_process_info main_info;
	if_ids ifs;
	ConfigProfile config;
	LogSys log_sys;
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
	return &_hCurrentProc.proc_stat;
}
DLLAPI(char*) get_current_executable_name()
{
	return _hCurrentProc.name;
}
DLLAPI(void*) get_current_executable_id()
{
	return _hCurrentProc.proc_stat.id;
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
	return _hCurrentProc.proc_stat.is_launcher;
}
DLLAPI(int) is_manager()
{
	return _hCurrentProc.proc_stat.is_manager;
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
	if(sys_log_level>level)
	{
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
	return 0;
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
	sys_get_current_process_path(exe_dir,MAX_DIR_SIZE);
	string absolute_path;
	if(0!=(init_error=get_absolute_path(string(exe_dir),string(CFG_FILE_PATH),absolute_path,sys_is_absolute_path)))
		return;
	if(0!=(init_error=config.LoadConfigFile(absolute_path.c_str())))
		return;
	memset(name,0,FILE_NAME_SIZE);
	sys_get_current_process_name(name, FILE_NAME_SIZE);
	init_process_stat(&proc_stat,name);
	proc_stat.main_info=&main_info;
	proc_stat.ifs=&ifs;
	if(0!=(init_error=GetProcStat(&proc_stat)))
		return;
	if(0!=sys_get_current_dir(cur_dir,MAX_DIR_SIZE))
	{
		init_error=ERR_CUR_DIR_NOT_FOUND;
		return;
	}
	if(proc_stat.local_cur_dir)
	{
		if(0!=(init_error=sys_set_current_dir(exe_dir)))
			return;
	}
	if(proc_stat.unique_instance&&sys_has_dup_process(name))
	{
		init_error=ERR_DUP_PROCESS;
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
	bool uniq,ld,launcher=false,manager=false,managed=false,log=true,ambiguous=false;
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
	config.GetCongfigItem(exec,CFG_TAG_EXEC_LAUNCHER,launcher);
	pstat->is_launcher=launcher?1:0;
	config.GetCongfigItem(exec,CFG_TAG_EXEC_MANAGER,manager);
	pstat->is_manager=manager?1:0;
	config.GetCongfigItem(exec,CFG_TAG_EXEC_MANAGED,managed);
	pstat->is_managed=managed?1:0;
	config.GetCongfigItem(exec,CFG_TAG_EXEC_LOG,log);
	pstat->log=log?1:0;
	config.GetCongfigItem(exec,CFG_TAG_EXEC_AMBIG,ambiguous);
	pstat->ambiguous=ambiguous?1:0;
	if(pstat->ifs!=NULL)
	{
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
	}
	return 0;
}
int process_identifier::GetProcStat(process_stat* pstat)
{
	bool found=false,
		launcher_found=(pstat->main_info==NULL),
		manager_found=(pstat->main_info==NULL);
	pstat->file[FILE_NAME_SIZE-1]=0;
	ConfigProfile::iterator it;
	int ret=0;
	char buf[IF_INFO_SIZE];
	for(it=config.BeginIterate(CONFIG_SECTION_EXEC);it;it++)
	{
		if(it->first!="")
		{
			const string& s=it->first;
			string file;
			if(config.GetCongfigItem(s,CFG_TAG_EXEC_FILE,file))
			{
				if(!found&&file==pstat->file)
				{
					found=true;
					if(0!=(ret=__get_stat__(pstat,s,config)))
						return ret;
				}
				bool b=false;
				string str;
				if(!launcher_found&&config.GetCongfigItem(s,CFG_TAG_EXEC_LAUNCHER,b)&&b)
				{
					launcher_found=true;
					strcpy(pstat->main_info->loader_exe_file,file.c_str());
					sprintf(buf,CFG_TAG_EXEC_IF,0);
					if(!config.GetCongfigItem(s,buf,str))
						return ERR_EXEC_INFO_NOT_FOUND;
					strcpy(pstat->main_info->loader_if0,str.c_str());
					uint cnt=1;
					sprintf(buf,CFG_TAG_EXEC_IF_CNT,0);
					config.GetCongfigItem(s,buf,cnt);
					pstat->main_info->loader_if0_cnt=(cnt<1?1:cnt);
				}
				if(!manager_found&&config.GetCongfigItem(s,CFG_TAG_EXEC_MANAGER,b)&&b)
				{
					manager_found=true;
					strcpy(pstat->main_info->manager_exe_file,file.c_str());
					sprintf(buf,CFG_TAG_EXEC_IF,0);
					if(!config.GetCongfigItem(s,buf,str))
						return ERR_EXEC_INFO_NOT_FOUND;
					strcpy(pstat->main_info->manager_if0,str.c_str());
					uint cnt=1;
					sprintf(buf,CFG_TAG_EXEC_IF_CNT,0);
					config.GetCongfigItem(s,buf,cnt);
					pstat->main_info->manager_if0_cnt=(cnt<1?1:cnt);
				}
				if(found&&launcher_found&&manager_found)
					return 0;
			}
		}
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
	if(_hCurrentProc.proc_stat.log)
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
	for(;(*it)&&(*it)->first=="";(*it)++);
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
	(*it)++;
	return it;
}
int process_identifier::FindNextExecutable(void* handle,process_stat* pstat)
{
	ConfigProfile::iterator* it=(ConfigProfile::iterator*)handle;
	for(;(*it)&&(*it)->first=="";(*it)++);
	if(!(*it))
		return 0;
	string file;
	config.GetCongfigItem((*it)->first,CFG_TAG_EXEC_FILE,file);
	strcpy(pstat->file,file.c_str());
	if(0!=__get_stat__(pstat,(*it)->first,config))
		return 0;
	(*it)++;
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
bool spec_char_verifier::is_spec(char c)
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
	{
		*(--rparam.start)=0;
	}
	if(rparam.tstart>rparam.to&&*(rparam.tstart-1)==dsym)
	{
		*(--rparam.tstart)=0;
	}
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
	{
		ret=callback->_ftraverse_(newdel.path,cb_delete,&newdel);
		*(newdel.start)=0;
	}
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
	{
		*(--rparam.start)=0;
	}
	if(type==FILE_TYPE_DIR)
		ret=callback->_ftraverse_(rparam.path,cb_delete,&rparam);
	*rparam.start=0;
	int retc=0;
	(ret==0&&rparam.quitdel==0)?ret=retc=callback->_fdelete_(rparam.path):0;
	(((ret==0&&rparam.quitdel==0)||retc!=0)&&cbdata!=NULL)?ret=cbdata->cb(ret,rparam.path,type,cbdata->param,dsym):0;
	delete[] rparam.path;
	return ret;
}
DLLAPI(int) sys_recurse_fcopy(char* from, char* to, file_recurse_cbdata* cbdata)
{
	file_recurse_callback cb={sys_fstat,sys_ftraverse,sys_mkdir,sys_fcopy,sys_fdelete};
	return recurse_fcopy(from,to,&cb,cbdata,_dir_symbol);
}
DLLAPI(int) sys_recurse_fdelete(char* pathname, file_recurse_cbdata* cbdata)
{
	file_recurse_callback cb={sys_fstat,sys_ftraverse,sys_mkdir,sys_fcopy,sys_fdelete};
	return recurse_fdelete(pathname,&cb,cbdata,_dir_symbol);
}
