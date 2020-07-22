#include "sh_cmd_fs.h"
#include "fs_shell.h"
#include "parse_cmd.h"
#include "path.h"
#include "complete.h"
#include "sysfs.h"
#include "mutex.h"
#include "Integer64.h"
#include "datetime.h"
#include "config_val_extern.h"
#include "interface.h"
#include <algorithm>
#include <stdio.h>
#include <assert.h>
#define if_safe_release(hif) \
	if(VALID(hif)) \
	{ \
		close_if(hif); \
		hif=NULL; \
	}
#define return_msg(code,msg,...) \
	{printf(msg,##__VA_ARGS__); \
	return code;}
#define MAX_FLIST_CHAR 80
#define MAX_NUM_FLIST 8
enum E_FILE_DISP_MODE
{
	file_disp_simple,
	file_disp_type_time,
};
DEFINE_UINT_VAL(fs_sh_nbuf,4);
DEFINE_UINT_VAL(fs_sh_buflen,2048);
ShCmdTable* ShCmdTable::GetTable()
{
	static ShCmdTable table;
	return &table;
}
void ShCmdTable::AddCmd(const char* cmd,per_cmd_handler handler,const char* desc,const char* detail)
{
	CmdItem item;
	item.handler=handler;
	item.detail=detail;
	ShCmdTable* ptable=GetTable();
	ptable->vstrdesc.push_back(string(cmd)+" - "+desc+"\n");
	ptable->cmd_map[cmd]=item;
}
int ShCmdTable::Init()
{
	sort(GetTable()->vstrdesc.begin(),GetTable()->vstrdesc.end());
	return 0;
}
int ShCmdTable::ExecCmd(sh_context* ctx,const string& cmd,vector<pair<string,string>>& args)
{
	map<string,CmdItem>& c_map=GetTable()->cmd_map;
	map<string,CmdItem>::iterator it=c_map.find(cmd);
	if(it==c_map.end()||it->second.handler==NULL)
		return_msg(ERR_INVALID_CMD,"Command not found.\n");
	return it->second.handler(ctx,cmd,args);
}
void ShCmdTable::PrintDesc()
{
	ShCmdTable* ptable=GetTable();
	for(int i=0;i<(int)ptable->vstrdesc.size();i++)
	{
		printf("%s",ptable->vstrdesc[i].c_str());
	}
}
int ShCmdTable::PrintDetail(const string& cmd)
{
	map<string,CmdItem>& c_map=GetTable()->cmd_map;
	map<string,CmdItem>::iterator it=c_map.find(cmd);
	if(it==c_map.end()||it->second.handler==NULL)
		return_msg(ERR_INVALID_CMD,"Command not found.\n");
	printf("%s",it->second.detail);
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
		"################################################\n"
		"# fs_sh: a builtin shell for AST storage system.\n"
		"# Version: 1.0.0.0\n"
		"# Copyright (C) May 2020, CaiBin\n"
		"# License: GNU GPL 3.0\n"
		"# All Rights Reserved.\n"
		"################################################\n"
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
		sys_sleep(200);
		if(0==(ret=connect_if(&init,&ifproc->hif)))
		{
			return 0;
		}
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
	return ret;
}
static inline bool __insert_proc_data__(vector<proc_data>& pdata,const process_stat& pstat)
{
	if(!pstat.is_managed)
		return false;
	proc_data data;
	data.name=pstat.file;
	data.cmdline=pstat.cmdline;
	data.id=pstat.id;
	data.ambiguous=!!pstat.ambiguous;
	data.hproc=NULL;
	data.hthrd_shelter=NULL;
	for(int i=0;i<pstat.ifs->count;i++)
	{
		if_proc ifproc;
		ifproc.hif=NULL;
		ifproc.id=pstat.ifs->if_id[i].if_name;
		ifproc.usage=pstat.ifs->if_id[i].usage;
		ifproc.cnt=pstat.ifs->if_id[i].thrdcnt;
		ifproc.prior=pstat.ifs->if_id[i].prior;
		ifproc.pdata=NULL;
		data.ifproc.push_back(ifproc);
	}
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
	void* hproc=sys_get_process(get_main_info()->manager_exe_file);
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
		uint c=get_char();
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
int get_full_path(const string& cur_dir,const string& relative_path,string& full_path)
{
	int ret=0;
	vector<string> cur_dir_split,relative_path_split,direct;
	string drv_name;
	split_path(relative_path,relative_path_split,'/');
	if(!relative_path.empty()&&relative_path[0]=='/')
	{
		if(0!=(ret=(get_direct_path(direct,relative_path_split))))
			return ret;
		merge_path(full_path,direct,'/');
		full_path="/"+full_path;
		return 0;
	}
	if((!relative_path_split.empty())&&(!relative_path_split[0].empty())
		&&relative_path_split[0].back()==':')
	{
		drv_name=relative_path_split[0];
		relative_path_split.erase(relative_path_split.begin());
		if(0!=(ret=(get_direct_path(direct,relative_path_split))))
			return ret;
		direct.insert(direct.begin(),drv_name);
		merge_path(full_path,direct,'/');
		return 0;
	}
	bool root=(cur_dir[0]=='/');
	split_path(cur_dir,cur_dir_split,'/');
	bool drive=((!cur_dir_split.empty())&&(!cur_dir_split[0].empty())
		&&cur_dir_split[0].back()==':');
	if((!root)&&(!drive))
		return ERR_INVALID_PATH;
	if(drive)
	{
		drv_name=cur_dir_split[0];
		cur_dir_split.erase(cur_dir_split.begin());
	}
	cur_dir_split.insert(cur_dir_split.end(),relative_path_split.begin(),relative_path_split.end());
	if(0!=(ret=(get_direct_path(direct,cur_dir_split))))
		return ret;
	if(drive)
		direct.insert(direct.begin(),drv_name);
	merge_path(full_path,direct,'/');
	if(root)
		full_path="/"+full_path;
	return 0;
}
static int execute(sh_context* ctx)
{
	string cmd=ctx->cmd;
	vector<pair<string,string>> args;
	int ret=0;
	if(0!=(ret=parse_cmd((const byte*)cmd.c_str(),cmd.size(),args)))
	{
		switch(ret)
		{
		case ERR_INVALID_CMD:
			printf("bad command format\n");
			break;
		case ERR_BUFFER_OVERFLOW:
			printf("command buffer overflow\n");
			break;
		default:
			assert(false);
			break;
		}
		return ret;
	}
	if(args.empty())
		return 0;
	if(!args[0].second.empty())
		return_msg(ERR_INVALID_CMD,"bad command format\n");
	const string& cmd_head=args[0].first;
	return ShCmdTable::ExecCmd(ctx,cmd_head,args);
}
static bool validate_path(const string& path,dword* flags=NULL,DateTime* date=NULL,UInteger64* size=NULL,bool mute=false)
{
	DateTime dt[3];
	dword tflags=0;
	int ret=fs_get_attr((char*)path.c_str(),((flags!=NULL)||(size!=NULL)?FS_ATTR_FLAGS:0)|(date!=NULL?FS_ATTR_MODIFY_DATE:0),&tflags,date!=NULL?dt:NULL);
	if(flags!=NULL&&ret==0)
		*flags=tflags;
	if(date!=NULL&&ret==0)
		*date=dt[fs_attr_modify_date];
	if((!mute)&&ret!=0&&ret!=ERR_FS_FILE_NOT_EXIST)
		printf("unexpected exception: %s\n",get_error_desc(ret));
	if(ret!=0)
		return false;
	if(size!=NULL)
	{
		if(FS_IS_DIR(tflags))
		{
			*size=UInteger64(1024);
			return true;
		}
		void* hFile=fs_open((char*)path.c_str(),FILE_OPEN_EXISTING|FILE_READ);
		if(!VALID(hFile))
		{
			if(mute)
				return false;
			else
				return_msg(false,"File can not be opened.\n");
		}
		if(0!=(ret=fs_get_file_size(hFile,&size->low,&size->high)))
		{
			if(!mute)
				printf("unexpected exception: %s\n",get_error_desc(ret));
		}
		if(0!=(ret=fs_perm_close(hFile)))
		{
			if(!mute)
				printf("close file failed: %s\n",get_error_desc(ret));
		}
	}
	return ret==0;
}
static int df_handler(sh_context* ctx, const string& cmd, vector<pair<string, string>>& args)
{
	vector<string> devs;
	uint def=0;
	int ret=0;
	if(0!=(ret=fs_list_dev(devs,&def)))
		return_msg(ret,"command failed: %s\n",get_error_desc(ret));
	for(int i=0;i<(int)devs.size();i++)
	{
		fs_dev_info info;
		if(0!=(ret=fs_get_dev_info(devs[i],info)))
		{
			printf("unexpected exception: %s\n",get_error_desc(ret));
			continue;
		}
		printf("%s:\t%s\t%s\n",devs[i].c_str(),info.devtype.c_str(),(i==(int)def?" (default)":""));
	}
	return 0;
}
DEF_SH_CMD(df,df_handler,
	"list all the devices of the storage system.",
	"The df command lists the accessible devices of the storage system.\n"
	"The device names are listed as the interface id's of each device,"
	"the device type/file system format are listed along with device names.\n"
	"The default device is labeled with (default).\n");
static inline void list_one_dir(const string& cwd,vector<string>& flist,E_FILE_DISP_MODE mode)
{
	if(mode==file_disp_simple)
	{
		uint nc=0,nf=0;
		for(int i=0;i<(int)flist.size();i++)
		{
			if(nc>=MAX_FLIST_CHAR||nf>=MAX_NUM_FLIST)
			{
				printf("\n");
				nc=0;
				nf=0;
			}
			else if(i!=0)
				printf("\t");
			nc+=flist[i].size();
			nf++;
			printf("%s",quote_file(flist[i]).c_str());
		}
		if(nc>0||nf>0)
			printf("\n");
	}
	else
	{
		DateTime date;
		string fullpath;
		for(int i=0;i<(int)flist.size();i++)
		{
			string& file=flist[i];
			UInteger64 u64;
			if(0!=get_full_path(cwd,file,fullpath))
				continue;
			if(!validate_path(fullpath,NULL,&date,&u64))
				continue;
			string sz=FormatI64(u64);
			string dispsz;
			string strdate;
			for(int i=0;i<(int)sz.size();i+=3)
			{
				string sec=((int)sz.size()>i+3?sz.substr(sz.size()-i-3,3):sz.substr(0,sz.size()-i));
				dispsz=sec+(i==0?"":",")+dispsz;
			}
			CDateTime datetime(date);
			datetime.Format(strdate,FORMAT_DATE|FORMAT_TIME|FORMAT_WEEKDAY);
			printf("%s\t%s\t%s\t%s\n",(!file.empty())&&file.back()=='/'?"d":"n",
				dispsz.c_str(),strdate.c_str(),quote_file(file).c_str());
		}
	}
}
static int list_file_path(sh_context* ctx,const string& path,E_FILE_DISP_MODE mode,bool dispdir=false)
{
	int ret=0;
	dword flags=0;
	string fullpath;
	if(0!=(ret=get_full_path(ctx->pwd,path,fullpath)))
	{
		if(dispdir)
			printf("%s:\n",path.c_str());
		return_msg(ret,"invalid path\n");
	}
	if(!validate_path(fullpath,&flags))
	{
		if(dispdir)
			printf("%s:\n",path.c_str());
		return_msg(ERR_FS_FILE_NOT_EXIST,"path not exist\n");
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
			printf("%s:\n",path.c_str());
		list_cur_dir_files(fullpath,flist);
		sort(flist.begin(),flist.end());
	}
	list_one_dir(bfile?ctx->pwd:fullpath,flist,mode);
	return 0;
}
static int ls_handler(sh_context* ctx,const string& cmd,vector<pair<string,string>>& args)
{
	int ret=0;
	string fullpath;
	E_FILE_DISP_MODE mode=(cmd=="ll"?file_disp_type_time:file_disp_simple);
	if(args.size()==1)
		return list_file_path(ctx,"",mode);
	vector<string> paths;
	for(int i=1;i<(int)args.size();i++)
	{
		pair<string,string>& op=args[i];
		if(!op.first.empty()&&op.first[0]=='-')
		{
			if(op.first.size()>=2&&op.first[1]=='-')
				continue;
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
			return_msg(ERR_INVALID_CMD,"bad command format\n");
		}
		else
			paths.push_back(op.first);
	}
	if(paths.empty())
		paths.push_back("");
	for(int i=0;i<(int)paths.size();i++)
	{
		ret=list_file_path(ctx,paths[i],mode,paths.size()>1);
		if(i<(int)paths.size()-1)
			printf("\n");
	}
	return ret;
}
DEF_SH_CMD(ls,ls_handler,
	"list contents of one or more directorys.",
	"Format:\n\tls [-l] [dir1] [dir2] ...\n"
	"The ls command lists information of files and directories in one or more directories.\n"
	"Option -l indicates that the information is shown in detail, "
	"which includes: file type(normal file or directory),size,date of modifying, and file name, "
	"and without this option, only file name is shown.\n"
	"The directory list is optional, if absent, the content of current directory is shown, "
	"otherwise the contents of specified directories are shown,\n"
	"The information of each directory will be shown respectively, each with a label.\n"
	"The label will not be shown if the directory list contains only one directory.\n");
DEF_SH_CMD(ll,ls_handler,
	"the alias for command ls -l",
	"This is an alias for command ls with option -l, see ls.\n");
static int cd_handler(sh_context* ctx,const string& cmd,vector<pair<string,string>>& args)
{
	if(args.size()!=2||!args[1].second.empty())
		return_msg(ERR_INVALID_CMD,"bad command format\n");
	string path(args[1].first),fullpath;
	int ret=0;
	dword flags=0;
	if(0!=(ret=get_full_path(ctx->pwd,path,fullpath)))
	{
		return_msg(ret,"invalid path\n");
	}
	if(!validate_path(fullpath,&flags))
	{
		return_msg(ERR_FS_FILE_NOT_EXIST,"path not exist\n");
	}
	if(!FS_IS_DIR(flags))
	{
		return_msg(0,"%s is a file.\n",quote_file(path).c_str());
	}
	ctx->pwd=fullpath;
	return 0;
}
DEF_SH_CMD(cd,cd_handler,
	"enter/change current directory to a new one",
	"This command changes current directory to the specified new one.\n"
	"Format:\n\tcd (new path)\n"
	"path shall be specified without spaces or other special characters, "
	"if so, quote it with \' or \".\n"
	"the specified path must be an existing directory, or the command will fail.\n"
	"path can be an absolute path or a relative path, depending on whether it "
	"starts with a device name and a colon followed by a slash, a slash alone "
	"indicates the root directory of the default device.\n");
static int cb_lsfile(char* name,dword type,void* param,char dsym)
{
	vector<string>* files=(vector<string>*)param;
	string file(name);
	if(type==FILE_TYPE_DIR)
		file+="/";
	files->push_back(file);
	return 0;
}
static int help_handler(sh_context* ctx,const string& cmd,vector<pair<string,string>>& args)
{
	int argsize=(int)args.size();
	if(argsize<1)
		return_msg(ERR_INVALID_CMD,"bad command format\n");
	if(argsize==1)
	{
		ShCmdTable::PrintDesc();
		printf("\nType help + command name to get detailed usage of each command.\n");
		return 0;
	}
	if((int)args.size()!=2||!args[1].second.empty())
		return_msg(ERR_INVALID_CMD,"bad command format\n");
	return ShCmdTable::PrintDetail(args[0].first);
}
DEF_SH_CMD(help,help_handler,
	"Show available commands and their usages.",
	"Format:\n\thelp [command name]\n"
	"without the optional argument, this command lists all the available commands.\n"
	"with the optional argument command name, this command shows the usage of the specified command.\n"
	);
void list_cur_dir_files(const string& dir,vector<string>& files)
{
	int ret=0;
	dword flags=0;
	files.clear();
	if(!(validate_path(dir,&flags,NULL,NULL,true)&&FS_IS_DIR(flags)))
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
	if(state==FS_CMD_HANDLE_STATE_EXEC)
	{
		if(ctx->c==(uint)'\t')
			complete(ctx);
		else
			ret=execute(ctx);
	}
	else
	{
		ctx->pwd="/";
	}
	return ret;
}
int init_sh()
{
	int ret=0;
	if(0!=(ret=ShCmdTable::Init()))
		return ret;
	if(0!=(ret=init_fs()))
		return ret;
	set_cmd_handler(_fs_cmd_handler);
	printf("Type \"help\" to get a list of available commands.\n");
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
}
