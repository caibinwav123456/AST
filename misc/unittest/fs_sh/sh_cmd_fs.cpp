#include "sh_cmd_fs.h"
#include "fs_shell.h"
#include "parse_cmd.h"
#include "path.h"
#include "complete.h"
#include "sysfs.h"
#include "mutex.h"
#include "config_val_extern.h"
#include "interface.h"
#include "arch.h"
#include <algorithm>
#include <assert.h>
#include <string.h>
using namespace std;
#define if_safe_release(hif) \
	if(VALID(hif)) \
	{ \
		close_if(hif); \
		hif=NULL; \
	}
#define MAX_FLIST_CHAR 80
#define MAX_NUM_FLIST 8
#define MIN_TPBUF_LEN 1024
#define CMD_REDIR "\t%redir%\t"
enum E_FILE_DISP_MODE
{
	file_disp_simple,
	file_disp_type_time,
};
DEFINE_SIZE_VAL(fs_tpbuf_len,MIN_TPBUF_LEN);
void prepare_tp_buf(byte*& buf)
{
	if(buf!=NULL)
		return;
	buf=new byte[fs_tpbuf_len];
}
int ban_pre_handler(cmd_param_st* param)
{
	if((!is_pre_revoke(param))&&param->stream!=NULL)
		return_msg(ERR_INVALID_CMD,"The command \'%s\' cannot be used with \'|\'\n",param->cmd.c_str());
	return 0;
}
static int redir_handler(cmd_param_st* param)
{
	common_sh_args(param);
	assert(pipe_next==NULL&&pipe!=NULL);
	assert(args.size()==2);
	st_path_handle*& ph=*(st_path_handle**)&(param->priv);
	assert(ph!=NULL);
	assert(VALID(ph->hfile)&&!ph->path.empty());
	int ret=0,dummy=0;
	if(0!=(ret=fs_seek(ph->hfile,args[0].first==">"?SEEK_BEGIN:SEEK_END,
		0,&dummy)))
	{
		t_output("\'%s\': seek for writing file error: %s\n",ph->path.c_str(),get_error_desc(ret));
		goto end;
	}
	{
		const uint buflen=1024;
		byte buf[buflen];
		uint n=0,wrlen=0;
		while((n=pipe->Recv(buf,buflen))>0)
		{
			if(0!=(ret=(fs_write(ph->hfile,buf,n,&wrlen)))
				||wrlen!=n)
			{
				ret==0?(ret=ERR_FILE_IO):0;
				t_output("\'%s\': write file error: %s\n",ph->path.c_str(),get_error_desc(ret));
				break;
			}
		}
	}
end:
	if(ret==0)
		set_used_pipe(param);
	int retc=t_clean_file(*ph);
	delete ph;
	ph=NULL;
	if(ret==0)
		ret=retc;
	return ret;
}
static int redir_pre_handler(cmd_param_st* param)
{
	common_sh_args(param);
	assert(pipe_next==NULL&&pipe!=NULL);
	assert(args.size()==2);
	int ret=0;
	if(is_pre_revoke(param))
	{
		st_path_handle*& ph=*(st_path_handle**)&(param->priv);
		ret=t_clean_file(*ph);
		delete ph;
		ph=NULL;
		return ret;
	}
	st_path_handle* ph=new st_path_handle;
	string fullpath;
	ph->path=args[1].first;
	if(0!=(ret=get_full_path(ctx->pwd,ph->path,fullpath)))
	{
		delete ph;
		return_msg(ret,"invalid path: \'%s\'\n",args[1].first.c_str());
	}
	dword open_flag=(args[0].first==">"?
		FILE_CREATE_ALWAYS|FILE_READ|FILE_WRITE|FILE_EXCLUSIVE_WRITE
		:FILE_OPEN_ALWAYS|FILE_READ|FILE_WRITE|FILE_EXCLUSIVE_WRITE);
	ph->hfile=fs_open((char*)fullpath.c_str(),open_flag);
	if(!VALID(ph->hfile))
	{
		delete ph;
		return_msg(ERR_OPEN_FILE_FAILED,"\'%s\': can not open file for writing\n",args[1].first.c_str());
	}
	param->priv=ph;
	return 0;
}
bool ShCmdTable::less_str(const char* a, const char* b)
{
	return strcmp(a,b)<0;
}
ShCmdTable* ShCmdTable::GetTable()
{
	static ShCmdTable table;
	return &table;
}
void ShCmdTable::AddCmd(const char* cmd,per_cmd_handler handler,per_cmd_handler pre_handler,const char* desc,const char* detail)
{
	CmdItem item;
	item.handler=handler;
	item.pre_handler=pre_handler;
	item.detail=detail;
	ShCmdTable* ptable=GetTable();
	ptable->vstrdesc.push_back(string(cmd)+"\t- "+desc+"\n");
	ptable->cmd_map[cmd]=item;
}
int ShCmdTable::Init()
{
	ShCmdTable* ptable=GetTable();
	for(int i=0;i<(int)ptable->vstrdesc.size();i++)
		ptable->pstrdesc.push_back(ptable->vstrdesc[i].c_str());
	sort(ptable->pstrdesc.begin(),ptable->pstrdesc.end(),less_str);
	return 0;
}
static inline void link_cmd_param(cmd_param_st* param,sh_context* ctx)
{
	cmd_param_st* next=param->next;
	param->next=new cmd_param_st(ctx);
	param->next->next=next;
	param->next->prev=param;
	if(next!=NULL)
		next->prev=param->next;
}
static inline int init_cmd_param(cmd_param_st* cmd_param,const vector<pair<string,string>>& args,
	int start,int end,Pipe* stream,bool create_stream=true)
{
	if(start>=end)
		return_msg(ERR_INVALID_CMD,"void substatement\n");
	cmd_param->args.assign(args.begin()+start,args.begin()+end);
	if(!cmd_param->args[0].second.empty())
		return_msg(ERR_INVALID_CMD,"bad command format\n");
	cmd_param->cmd=cmd_param->args[0].first;
	cmd_param->stream_next=stream;
	if(create_stream)
		cmd_param->stream=new Pipe;
	return 0;
}
bool ShCmdTable::find_cmd(const string& cmd,CmdItem* cmditem)
{
	if(cmd==CMD_REDIR)
	{
		static const CmdItem cmd_redir={redir_handler,redir_pre_handler,NULL};
		*cmditem=cmd_redir;
		return true;
	}
	map<string,CmdItem>& c_map=GetTable()->cmd_map;
	map<string,CmdItem>::iterator it=c_map.find(cmd);
	if(it==c_map.end()||it->second.handler==NULL)
		return false;
	*cmditem=it->second;
	return true;
}
void ShCmdTable::revoke_preprocess(cmd_param_st* param,const vector<CmdItem>& callback,cmd_param_st* end)
{
	int i;
	cmd_param_st* p;
	for(i=callback.size()-1,p=param;p!=end;i--,p=p->next)
	{
		if(callback[i].pre_handler!=NULL)
		{
			p->flags|=CMD_PARAM_FLAG_PRE_REVOKE;
			callback[i].pre_handler(p);
		}
	}
}
static void end_threads(cmd_param_st* param,cmd_param_st* end)
{
	for(cmd_param_st* p=param;p!=end;p=p->prev)
	{
		if(VALID(p->hthread))
		{
			sys_wait_thread(p->hthread);
			sys_close_thread(p->hthread);
			p->hthread=NULL;
		}
	}
}
static int trace_errors(cmd_param_st* cmd_param)
{
	int ret=0;
	assert(cmd_param!=NULL);
	if(cmd_param->next==NULL)
		return cmd_param->ret;
	for(cmd_param_st* p=cmd_param;p!=NULL;p=p->next)
	{
		if(p->ret==0)
			continue;
		if(ret==0)
		{
			ret=p->ret;
			printf("Tracing errors:\n");
		}
		printf("\tIn \'%s\': %s.\n",p->cmd.c_str(),get_error_desc(p->ret));
	}
	return ret;
}
static int sh_thread_func(void* ptr)
{
	sh_thread_param* param=(sh_thread_param*)ptr;
	per_cmd_handler handler=param->handler;
	cmd_param_st* cmd_param=param->param;
	delete param;
	cmd_param->ret=handler(cmd_param);
	if(!used_pipe(cmd_param))
		drain_stream(cmd_param);
	term_stream(cmd_param);
	return 0;
}
int ShCmdTable::ExecCmd(sh_context* ctx,const vector<pair<string,string>>& args)
{
	int ret=0;
	int irangle=-1,idbrangle=-1,ivbar=-1;
	for(int i=args.size()-1;i>=0;i--)
	{
		if(irangle==-1&&args[i].first==">")
			irangle=i;
		if(idbrangle==-1&&args[i].first==">>")
			idbrangle=i;
		if(ivbar==-1&&args[i].first=="|")
			ivbar=i;
		if(irangle!=-1&&idbrangle!=-1&&ivbar!=-1)
			break;
	}
	if(irangle!=-1&&idbrangle!=-1)
		return_msg(ERR_INVALID_CMD,"\'>\' and \'>>\' cannot be both present\n");
	int pos_angle=(irangle!=-1?irangle:idbrangle);
	bool bdb=(idbrangle!=-1);
	int itoken=args.size()-1,tail;
	cmd_param_st cmd_param(ctx),*last_cmd=NULL;
	Pipe* pipe=NULL;
	if(pos_angle!=-1)
	{
		if(pos_angle!=itoken-1||(ivbar!=-1&&ivbar==itoken))
			return_msg(ERR_INVALID_CMD,"the redirection is not a valid file path\n");
		itoken-=2;
		if(!args.back().second.empty())
			return_msg(ERR_INVALID_CMD,"bad command format\n");
		link_cmd_param(&cmd_param,ctx);
		last_cmd=cmd_param.next;
		pipe=cmd_param.next->stream=new Pipe;
		cmd_param.next->args.push_back(pair<string,string>(bdb?">>":">",""));
		cmd_param.next->args.push_back(args.back());
		cmd_param.next->cmd=CMD_REDIR;
	}
	for(int i=itoken;i>=0;i--)
	{
		const string& token=args[i].first;
		if(token==">"||token==">>")
			return_msg(ERR_INVALID_CMD,"the redirection cannot be used more than once\n");
	}
	for(tail=itoken+1;itoken>=0;itoken--)
	{
		if(args[itoken].first=="|")
		{
			if(!args[itoken].second.empty())
				return_msg(ERR_INVALID_CMD,"bad command format\n");
			link_cmd_param(&cmd_param,ctx);
			if(last_cmd==NULL)
				last_cmd=cmd_param.next;
			if(0!=(ret=init_cmd_param(cmd_param.next,args,itoken+1,tail,pipe)))
				return ret;
			pipe=cmd_param.next->stream;
			tail=itoken;
		}
	}
	if(last_cmd==NULL)
		last_cmd=&cmd_param;
	if(0!=(ret=init_cmd_param(&cmd_param,args,0,tail,pipe,false)))
		return ret;
	vector<CmdItem> callback;
	for(cmd_param_st* cur_cmd=last_cmd;cur_cmd!=NULL;cur_cmd=cur_cmd->prev)
	{
		CmdItem item;
		if(!find_cmd(cur_cmd->cmd,&item))
		{
			revoke_preprocess(cur_cmd->next,callback);
			return_msg(ERR_INVALID_CMD,"Command \'%s\' not found.\n",cur_cmd->cmd.c_str());
		}
		if(item.pre_handler!=NULL)
		{
			if(0!=(ret=item.pre_handler(cur_cmd)))
			{
				revoke_preprocess(cur_cmd->next,callback);
				return ret;
			}
		}
		callback.push_back(item);
	}
	int ivec;
	cmd_param_st* pcmd;
	for(ivec=0,pcmd=last_cmd;pcmd!=NULL;ivec++,pcmd=pcmd->prev)
	{
		if(ivec==(int)callback.size()-1)
		{
			assert(pcmd->prev==NULL);
			ret=pcmd->ret=callback.back().handler(pcmd);
			term_stream(pcmd);
			end_threads(last_cmd,pcmd);
			continue;
		}
		sh_thread_param* thrdparam=new sh_thread_param;
		thrdparam->handler=callback[ivec].handler;
		thrdparam->param=pcmd;
		pcmd->hthread=sys_create_thread(sh_thread_func,thrdparam);
		if(!VALID(pcmd->hthread))
		{
			delete thrdparam;
			revoke_preprocess(&cmd_param,callback,pcmd->next);
			term_stream(pcmd);
			end_threads(last_cmd,pcmd);
			return_msg(ERR_THREAD_CREATE_FAILED,"%s\n",get_error_desc(ERR_THREAD_CREATE_FAILED));
		}
	}
	return trace_errors(&cmd_param);
}
void ShCmdTable::PrintDesc(cmd_param_st* pcmd)
{
	common_sh_args(pcmd);
	ShCmdTable* ptable=GetTable();
	for(int i=0;i<(int)ptable->pstrdesc.size();i++)
	{
		ts_output(ptable->pstrdesc[i]);
	}
}
int ShCmdTable::PrintDetail(const string& dcmd,cmd_param_st* pcmd)
{
	common_sh_args(pcmd);
	map<string,CmdItem>& c_map=GetTable()->cmd_map;
	map<string,CmdItem>::iterator it=c_map.find(dcmd);
	if(it==c_map.end()||it->second.handler==NULL)
		return_t_msg(ERR_INVALID_CMD,"Command not found.\n");
	ts_output(it->second.detail);
	return 0;
}
static vector<proc_data> pdata;
static if_control_block* get_blk()
{
	static gate mutex;
	static if_control_block blk(mutex,pdata);
	return &blk;
}
static RequestResolver reqrslvr;
static void print_banner()
{
	printf(
		"#################################################\n"
		"# fs_sh: a builtin shell for AST storage system.\n"
		"# Version: 1.0.0.0\n"
		"# Copyright (C) May 2020, CaiBin\n"
		"# License: GNU GPL 3.0\n"
		"# All Rights Reserved.\n"
		"#################################################\n"
	);
}
#if (defined(DEBUG) || defined(_DEBUG)) && !defined(NDEBUG)
#define MAX_CONNECT_TIMES 10
static int connect_proc(if_proc* ifproc)
{
	int ret=0;
	if_initial init;
	init.user=get_if_user();
	init.id=(char*)ifproc->id.c_str();
	init.smem_size=0;
	init.nthread=0;
	for(int i=0;i<MAX_CONNECT_TIMES;i++)
	{
		if(0==(ret=connect_if(&init,&ifproc->hif)))
		{
			return 0;
		}
		sys_sleep(200);
	}
	return_msg(ret,"connect to interface %s failed: %s\n",init.id,get_error_desc(ret));
}
static int connect_server()
{
	int ret=0;
	for(int i=0;i<(int)pdata.size();i++)
	{
		proc_data& data=pdata[i];
		for(int j=0;j<(int)data.ifproc.size();j++)
		{
			if(0!=(ret=connect_proc(&data.ifproc[j])))
				goto failed;
		}
	}
	return 0;
failed:
	for(int i=0;i<(int)pdata.size();i++)
	{
		proc_data& data=pdata[i];
		for(int j=0;j<(int)data.ifproc.size();j++)
		{
			if_safe_release(data.ifproc[j].hif);
		}
	}
	pdata.clear();
	return ret;
}
static inline bool __insert_proc_data__(vector<proc_data>& pdata,const process_stat& pstat)
{
	if(pstat.type!=E_PROCTYPE_MANAGED)
		return false;
	proc_data data;
	insert_proc_data(data,pstat);
	pdata.push_back(data);
	return true;
}
static bool less_id(const proc_data& d1,const proc_data& d2)
{
	return d1.id<d2.id;
}
static int prmnt_init(uint numbuf,uint buflen)
{
	int ret=0;
	pdata.clear();
	process_stat pstat;
	init_process_stat(&pstat,"");
	if_ids ifs;
	pstat.ifs=&ifs;
	void* h=find_first_exe(&pstat);
	if(!VALID(h))
		return ERR_GENERIC;
	__insert_proc_data__(pdata,pstat);
	while(find_next_exe(h,&pstat))
		__insert_proc_data__(pdata,pstat);
	find_exe_close(h);
	if(pdata.empty())
		return ERR_GENERIC;
	sort(pdata.begin(),pdata.end(),less_id);
	for(int i=0;i<(int)pdata.size();i++)
	{
		init_proc_data_cmdline(&pdata[i]);
		for(int j=0;j<(int)pdata[i].ifproc.size();j++)
		{
			pdata[i].ifproc[j].pdata=&pdata[i];
		}
	}
	if(0!=(ret=connect_server()))
		return_msg(ret,"connecting to fs server failed\n");
	return fsc_init(numbuf,buflen,get_blk());
}
#endif
static bool check_instance_exist()
{
	proc_data manager_exe_data;
	insert_proc_data_cmdline(manager_exe_data,get_main_info()->manager_exe_info);
	void* hproc=arch_get_process(manager_exe_data);
	if(!VALID(hproc))
		return false;
	sys_close_process(hproc);
	return true;
}
static int init_fs()
{
	print_banner();
	if(!check_instance_exist())
	{
		printf("\n"
			"The main processes are not running, this shell will exit.\n");
		return ERR_GENERIC;
	}
	printf("\n"
		"Please select a fs service connection mode:\n"
		"1 - instant mode.\n"
		"2 - semi-permanent mode.\n"
#if (defined(DEBUG) || defined(_DEBUG)) && !defined(NDEBUG)
		"3 - permanent mode.(for debug only)\n"
#endif
	);
	int mode=0;
#if (defined(DEBUG) || defined(_DEBUG)) && !defined(NDEBUG)
	const int max_mode=3;
#else
	const int max_mode=2;
#endif
	for(;;)
	{
#ifdef CMD_TEST
		volatile uint c='2';
#else
		uint c=get_char();
#endif
		if(c==0)
		{
			printf("The main processes have exited.\n");
			return ERR_GENERIC;
		}
		switch(c)
		{
		case (uint)'1':
			mode=1;
			break;
		case (uint)'2':
			mode=2;
			break;
		case (uint)'3':
			mode=3;
			break;
		}
		if(mode>=1&&mode<=max_mode)
		{
			printf("%d\n",mode);
			break;
		}
		else
		{
			printf("\nPlease enter a number between 1 and %d:",max_mode);
		}
	}
	int ret=0;
	uint fs_sh_nbuf=get_num_sysfs_buffer_blocks();
	uint fs_sh_buflen=get_sysfs_buffer_size();
	switch(mode)
	{
	case 1:
		ret=fsc_init(fs_sh_nbuf,fs_sh_buflen);
		break;
	case 2:
		ret=fsc_init(fs_sh_nbuf,fs_sh_buflen,NULL,&reqrslvr);
		break;
#if (defined(DEBUG) || defined(_DEBUG)) && !defined(NDEBUG)
	case 3:
		ret=prmnt_init(fs_sh_nbuf,fs_sh_buflen);
		break;
#endif
	}
	return ret;
}
static bool fs_extract_drive_tag(const string& path,string& tag,string& pure)
{
	const int pos=path.find('/');
	string first_dir;
	if(pos==0)
	{
		tag="";
		pure=path;
		return true;
	}
	else if(pos==string::npos)
		first_dir=path;
	else
		first_dir=path.substr(0,pos);
	if((!first_dir.empty())&&first_dir.back()==':')
	{
		tag=first_dir;
		pure=(pos==string::npos?"":path.substr(pos+1));
		return true;
	}
	else
	{
		tag="";
		pure=path;
		return false;
	}
}
int get_full_path(const string& cur_dir,const string& relative_path,string& full_path)
{
	int ret=0;
	string tag,pure;
	if(fs_extract_drive_tag(relative_path,tag,pure))
	{
		if(0!=(ret=get_absolute_path(pure,"",full_path,NULL,'/')))
			return ret;
	}
	else
	{
		if(!fs_extract_drive_tag(cur_dir,tag,pure))
			return ERR_INVALID_PATH;
		if(0!=(ret=get_absolute_path(pure,relative_path,full_path,NULL,'/')))
			return ret;
	}
	full_path=(full_path.empty()?(tag.empty()?"/":tag):tag+"/"+full_path);
	return 0;
}
#ifdef USE_FS_ENV_VAR
static inline int handle_set_env_var(ctx_priv_data* privdata,const vector<pair<string,string>>& args,bool& bset)
{
	int ret=0;
	bset=false;
	bool del_flag=!!(privdata->env_flags&CTXPRIV_ENVF_DEL);
	privdata->env_flags&=(~CTXPRIV_ENVF_DEL);
	if(args.size()!=1)
		return 0;
	bool empty=args[0].second.empty();
	assert(!((!empty)&&del_flag));
	if(empty&&!del_flag)
		return 0;
	if(0!=(ret=privdata->env_cache.SetEnv(args[0].first,args[0].second)))
		return_msg(ret,"set environment variable \'%s\' to \'%s\' failed: %s\n",
			args[0].first.c_str(),args[0].second.c_str(),get_error_desc(ret));
	bset=true;
	return 0;
}
#endif
static int execute(sh_context* ctx)
{
	string cmd=ctx->cmd;
	vector<pair<string,string>> args;
	int ret=0;
	bool bsetenv=false;
	if(0!=(ret=parse_cmd((const byte*)cmd.c_str(),cmd.size(),args,ctx->priv)))
		return_msg(ret,"%s\n",get_error_desc(ret));
#ifdef USE_FS_ENV_VAR
	if(0!=(ret=handle_set_env_var(ctx->priv,args,bsetenv))||bsetenv)
		return ret;
#endif
	if(args.empty())
		return 0;
	return ShCmdTable::ExecCmd(ctx,args);
}
static inline void sprint_buf(string& ret,char* buf,const char* format,...)
{
	va_list args;
	va_start(args,format);
	vsprintf(buf,format,args);
	ret+=buf;
	va_end(args);
}
int validate_path(const string& path,dword* flags,st_stat_file_time* date,UInteger64* size,string* strret)
{
	DateTime dt[3];
	dword tflags=0;
	char strretbuf[1024];
	bool bret=false;
	dword timemask=0;
	if(date!=NULL)
	{
		switch(date->ttype)
		{
		case fs_attr_creation_date:
			timemask=FS_ATTR_CREATION_DATE;
			break;
		case fs_attr_modify_date:
			timemask=FS_ATTR_MODIFY_DATE;
			break;
		case fs_attr_access_date:
			timemask=FS_ATTR_ACCESS_DATE;
			break;
		default:
			assert(false);
			break;
		}
	}
	if(strret!=NULL)
	{
		bret=true;
		strret->clear();
	}
	int ret=fs_get_attr((char*)path.c_str(),((flags!=NULL)||(size!=NULL)?FS_ATTR_FLAGS:0)|(date!=NULL?timemask:0),&tflags,date!=NULL?dt:NULL);
	if(flags!=NULL&&ret==0)
		*flags=tflags;
	if(date!=NULL&&ret==0)
		date->time=dt[date->ttype];
	if(bret&&ret!=0)
	{
		if(ret!=ERR_PATH_NOT_EXIST)
			sprint_buf(*strret,strretbuf,"unexpected exception: %s\n",get_error_desc(ret));
		else
			sprint_buf(*strret,strretbuf,"\'%s\': %s\n",path.c_str(),get_error_desc(ret));
	}
	if(ret!=0)
		return ret;
	if(size!=NULL)
	{
		if(FS_IS_DIR(tflags))
		{
			*size=UInteger64(1024);
			return 0;
		}
		void* hFile=fs_open((char*)path.c_str(),FILE_OPEN_EXISTING|FILE_READ);
		if(!VALID(hFile))
		{
			if(bret)
				sprint_buf(*strret,strretbuf,"\'%s\': file can not be opened.\n",path.c_str());
			return ERR_OPEN_FILE_FAILED;
		}
		if(0!=(ret=fs_get_file_size(hFile,&size->low,&size->high)))
		{
			if(bret)
				sprint_buf(*strret,strretbuf,"unexpected exception: %s\n",get_error_desc(ret));
		}
		int retclose=0;
		if(0!=(retclose=fs_perm_close(hFile)))
		{
			if(bret)
				sprint_buf(*strret,strretbuf,"close file failed: %s\n",get_error_desc(retclose));
			if(ret==0)
				ret=retclose;
		}
	}
	return ret;
}
static int df_handler(cmd_param_st* param)
{
	common_sh_args(param);
	vector<string> devs;
	uint def=0;
	int ret=0;
	if(0!=(ret=fs_list_dev(devs,&def)))
		return_t_msg(ret,"command failed: %s\n",get_error_desc(ret));
	for(int i=0;i<(int)devs.size();i++)
	{
		fs_dev_info info;
		if(0!=(ret=fs_get_dev_info(devs[i],info)))
		{
			t_output("unexpected exception: %s\n",get_error_desc(ret));
			continue;
		}
		t_output("%s:\t%s\t%s\n",devs[i].c_str(),info.devtype.c_str(),(i==(int)def?" (default)":""));
	}
	return 0;
}
DEF_SH_CMD(df,df_handler,
	"list all the devices of the storage system.",
	"The df command lists the accessible devices of the storage system.\n"
	"The device names are listed as the interface id's of each device."
	"The device type/file system format is listed along with its device name.\n"
	"The default device is labeled with (default).\n");
static inline void list_one_dir(cmd_param_st* param,const string& cwd,vector<string>& flist,fs_attr_datetime ttype,E_FILE_DISP_MODE mode)
{
	common_sh_args(param);
	if(mode==file_disp_simple)
	{
		uint nc=0,nf=0;
		for(int i=0;i<(int)flist.size();i++)
		{
			if(nc>=MAX_FLIST_CHAR||nf>=MAX_NUM_FLIST)
			{
				t_output("\n");
				nc=0;
				nf=0;
			}
			else if(i!=0)
				t_output("\t");
			nc+=flist[i].size();
			nf++;
			t_output("%s",quote_file(flist[i]).c_str());
		}
		if(nc>0||nf>0)
			t_output("\n");
	}
	else
	{
		st_stat_file_time date;
		date.ttype=ttype;
		string fullpath;
#ifdef CMD_TEST
		if(flist.size()!=13)
			sys_show_message("error");
#endif
		for(int i=0;i<(int)flist.size();i++)
		{
			string& file=flist[i];
			UInteger64 u64;
			if(0!=get_full_path(cwd,file,fullpath))
			{
				t_output("invalid path\n");
				continue;
			}
			string strret;
			if(0!=validate_path(fullpath,NULL,&date,&u64,&strret))
			{
				t_output("%s",strret.c_str());
				continue;
			}
			CDateTime datetime(date.time);
			string dispsz,strdate;
			format_segmented_u64(u64,dispsz);
#ifdef CMD_TEST
			if(datetime.year==0||file.empty())
				sys_show_message("error");
#endif
			datetime.Format(strdate,FORMAT_DATE|FORMAT_TIME|FORMAT_WEEKDAY);
			t_output("%s\t%s\t%s\t%s\n",(!file.empty())&&file.back()=='/'?"d":"n",
				dispsz.c_str(),strdate.c_str(),quote_file(file).c_str());
		}
	}
}
static int list_file_path(cmd_param_st* param,const string& path,fs_attr_datetime ttype,E_FILE_DISP_MODE mode,bool dispdir=false)
{
	common_sh_args(param);
	int ret=0;
	dword flags=0;
	string fullpath;
	if(0!=(ret=get_full_path(ctx->pwd,path,fullpath)))
	{
		if(dispdir)
			t_output("%s:\n",path.c_str());
		return_t_msg(ret,"invalid path\n");
	}
	string strret;
	if(0!=(ret=validate_path(fullpath,&flags,NULL,NULL,&strret)))
	{
		if(dispdir)
			t_output("%s:\n",path.c_str());
		t_output("%s",strret.c_str());
		return ret;
	}
	vector<string> flist;
	bool bfile=false;
	if(!FS_IS_DIR(flags))
	{
		bfile=true;
		flist.push_back(path);
	}
	else
	{
		if(dispdir)
			t_output("%s:\n",path.c_str());
		list_cur_dir_files(fullpath,flist);
		sort(flist.begin(),flist.end());
	}
	list_one_dir(param,bfile?ctx->pwd:fullpath,flist,ttype,mode);
	return 0;
}
static int ls_handler(cmd_param_st* param)
{
	common_sh_args(param);
	int ret=0;
	string fullpath;
	E_FILE_DISP_MODE mode=(cmd=="ll"?file_disp_type_time:file_disp_simple);
	if(args.size()==1)
		return list_file_path(param,"",fs_attr_modify_date,mode);
	vector<string> paths;
	fs_attr_datetime tmode=fs_attr_modify_date;
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& op=args[i];
		if(!op.first.empty()&&op.first[0]=='-')
		{
			if(op.first.size()>=2&&op.first[1]=='-')
			{
				if(op.first.substr(2)=="date")
				{
					if(op.second=="c")
						tmode=fs_attr_creation_date;
					else if(op.second=="m")
						tmode=fs_attr_modify_date;
					else if(op.second=="a")
						tmode=fs_attr_access_date;
					else
						return_t_msg(ERR_INVALID_PARAM,"\'%s\': invalid parameter for --date,\n"
							"the available options are c|m|a, see help.\n",op.second.c_str());
				}
				continue;
			}
			for(int j=1;j<(int)op.first.size();j++)
			{
				switch(op.first[j])
				{
				case 'l':
					mode=file_disp_type_time;
					break;
				}
			}
		}
		else if(!op.second.empty())
		{
			return_t_msg(ERR_INVALID_CMD,"bad command format\n");
		}
		else
			paths.push_back(op.first);
	}
	if(paths.empty())
		paths.push_back("");
	for(int i=0;i<(int)paths.size();i++)
	{
		ret=list_file_path(param,paths[i],tmode,mode,paths.size()>1);
		if(i<(int)paths.size()-1)
			t_output("\n");
	}
	return ret;
}
DEF_SH_CMD(ls,ls_handler,
	"list contents of one or more directorys.",
	"Format:\n\tls [options] [dir1] [dir2] ...\n"
	"The ls command lists information of files and directories in one or more directories.\n"
	"Options:\n"
	"-l\n"
	"\tIndicates that the information is shown in detail, "
	"which includes: file type(normal file or directory), size, date, and file name, "
	"and without this option, only file name is shown.\n"
	"--date=(c|m|a)\n"
	"\tSpecify the shown date type, available options are:\n"
	"c: creation date\n"
	"m: modify date\n"
	"a: access date\n"
	"The directory list is optional, if absent, the content of current directory is shown, "
	"otherwise the contents of specified directories are shown.\n"
	"The path of each directory can be an absolute path or a relative path, depending on whether it "
	"starts with a device name and a colon followed by a slash, a slash alone "
	"indicates the root directory of the default device.\n"
	"If this path is relative, it is based on the current directory path.\n"
	"The information of each directory will be shown respectively, each with a label.\n"
	"The label will not be shown if the directory list contains only one directory.\n");
DEF_SH_CMD(ll,ls_handler,
	"the alias for command \'ls -l\'.",
	"This is an alias for command ls with option -l, see \'help ls\'.\n");
static int cd_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(args.size()!=2||!args[1].second.empty())
		return_t_msg(ERR_INVALID_CMD,"bad command format\n");
	string path(args[1].first),fullpath;
	int ret=0;
	dword flags=0;
	if(0!=(ret=get_full_path(ctx->pwd,path,fullpath)))
	{
		return_t_msg(ret,"invalid path\n");
	}
	string strret;
	if(0!=(ret=validate_path(fullpath,&flags,NULL,NULL,&strret)))
	{
		t_output("%s",strret.c_str());
		return ret;
	}
	if(!FS_IS_DIR(flags))
	{
		return_t_msg(ERR_NOT_AVAIL_ON_FILE,"\'%s\': is a file.\n",path.c_str());
	}
	ctx->pwd=fullpath;
	return 0;
}
DEF_SH_CMD(cd,cd_handler,
	"enter/change current directory to a new one.",
	"This command changes current directory to the specified new one.\n"
	"Format:\n\tcd (new-path)\n"
	"Path shall be specified without spaces or other special characters, "
	"if so, quote it with \' or \".\n"
	"The specified path must be an existing directory, or the command will fail.\n"
	"Path can be an absolute path or a relative path, depending on whether it "
	"starts with a device name and a colon followed by a slash, a slash alone "
	"indicates the root directory of the default device.\n"
	"If path is relative, it is based on the current directory path.\n");
static int help_handler(cmd_param_st* param)
{
	common_sh_args(param);
	int argsize=(int)args.size();
	if(argsize<1)
		return_t_msg(ERR_INVALID_CMD,"bad command format\n");
	if(argsize==1)
	{
		ShCmdTable::PrintDesc(param);
		t_output("\nType \'help (command-name)\' to get detailed usage of each command.\n");
		return 0;
	}
	if((int)args.size()!=2||!args[1].second.empty())
		return_t_msg(ERR_INVALID_CMD,"bad command format\n");
	return ShCmdTable::PrintDetail(args[1].first,param);
}
DEF_SH_CMD(help,help_handler,
	"show available commands and their usages.",
	"Format:\n\thelp [command name]\n"
	"Without the optional argument, this command lists all the available commands.\n"
	"With the optional argument command name, this command shows the usage of the specified command.\n"
	);
static int cb_lsfile(char* name,dword type,void* param,char dsym)
{
	vector<string>* files=(vector<string>*)param;
	string file(name);
	if(type==FILE_TYPE_DIR)
		file+="/";
	files->push_back(file);
	return 0;
}
void list_cur_dir_files(const string& dir,vector<string>& files)
{
	int ret=0;
	dword flags=0;
	files.clear();
	if((0!=validate_path(dir,&flags))||!FS_IS_DIR(flags))
		return;
	if(0!=(ret=fs_traverse((char*)dir.c_str(),cb_lsfile,&files)))
	{
		files.clear();
		return;
	}
	files.push_back("./");
	if((dir!="/")&&(dir.find('/')!=string::npos))
		files.push_back("../");
}
static int _fs_cmd_handler(sh_context* ctx,dword state)
{
	int ret=0;
	switch(state)
	{
	case FS_CMD_HANDLE_STATE_EXEC:
		if(ctx->c==(uint)'\t')
			complete(ctx);
		else
			ret=execute(ctx);
		break;
	case FS_CMD_HANDLE_STATE_INIT:
		init_ctx_priv(ctx);
		ctx->pwd="/";
		break;
	case FS_CMD_HANDLE_STATE_EXIT:
		destroy_ctx_priv(ctx);
		break;
	}
	return ret;
}
int init_sh()
{
	int ret=0;
	if(fs_tpbuf_len<MIN_TPBUF_LEN)
		fs_tpbuf_len=MIN_TPBUF_LEN;
	if(0!=(ret=ShCmdTable::Init()))
		return ret;
	if(0!=(ret=init_fs()))
		return ret;
	set_cmd_handler(_fs_cmd_handler);
	printf("Type \'help\' to get a list of available commands.\n");
	return 0;
}
void exit_sh()
{
	fsc_exit();
	for(int i=0;i<(int)pdata.size();i++)
	{
		for(int j=0;j<(int)pdata[i].ifproc.size();j++)
		{
			if_safe_release(pdata[i].ifproc[j].hif);
		}
	}
	pdata.clear();
}
#ifdef CMD_TEST
int cmd_test(sh_context* ctx,int (*run_cmd)(sh_context*))
{
	sys_sleep(1000);
	for(int i=0;i<10;i++)
	{
		ctx->cmd="ll";
		printf("%s\n",ctx->cmd.c_str());
		run_cmd(ctx);
		print_prompt(ctx);
		sys_sleep(500);
	}
	return 0;
}
#endif
