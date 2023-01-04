#include "sh_cmd_linux.h"
#include "fs_shell.h"
#include "parse_cmd.h"
#include "path.h"
#include "complete.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#ifndef USE_LS_AS_LIST_FILES
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#endif
#define INFINITE ((uint)-1)
#define CTX_FLAG_EXEC_ALL 1
#define CMD_SET_EXEC_MODE "setmode"
#define EXEC_MODE_ALL "all"
#define EXEC_MODE_NORM "normal"
#define CMD_PRINT_ENV "print"

struct signal_handler_st
{
	int sig;
	sighandler_t handler;
};
static void signal_handler(int sig)
{
	switch(sig)
	{
	case SIGHUP:
		break;
	case SIGINT:
		printf("^C\n");
		break;
	default:
		break;
	}
};
static signal_handler_st signals[]={
	{SIGHUP,signal_handler},
	{SIGINT,signal_handler},
	{SIGTTIN,SIG_IGN},
	{SIGTTOU,SIG_IGN},
};
static int register_signal_handlers(dword state)
{
	switch(state)
	{
	case FS_CMD_HANDLE_STATE_INIT:
		for(int i=0;i<sizeof(signals)/sizeof(signal_handler_st);i++)
			signal(signals[i].sig,signals[i].handler);
		break;
	case FS_CMD_HANDLE_STATE_EXIT:
		for(int i=0;i<sizeof(signals)/sizeof(signal_handler_st);i++)
			signal(signals[i].sig,SIG_DFL);
		break;
	}
	return 0;
}
static const byte ls_suffix[]={'*','@','|','>'};
struct st_alias
{
	const char* alias;
	const char** full;
	uint min_args;
	uint max_args;
};
static const char* ll_full[]={"ls","-alF",NULL};
static const char* la_full[]={"ls","-A",NULL};
static const char* l_full[]={"ls","-CF",NULL};
static const st_alias s_alias[]={
	{"ll",ll_full,0,INFINITE},
	{"la",la_full,0,INFINITE},
	{"l",l_full,0,INFINITE}
};
static int validate_param(vector<pair<string,string>>& args,const st_alias& alias)
{
	uint size=0;
	for(int i=1;i<(int)args.size();i++)
	{
		bool real=false;
		if(!args[i].first.empty()&&args[i].first[0]!='-')
		{
			real=true;
			size++;
		}
		if(real&&!args[i].second.empty())
		{
			return ERR_INVALID_CMD;
		}
	}
	if(size<alias.min_args||size>alias.max_args)
	{
		return ERR_INVALID_CMD;
	}
	return 0;
}
static int check_args(vector<pair<string,string>>& args,bool mute=false)
{
	if(!args[0].second.empty())
	{
		if(!mute)
			printf("bad command format\n");
		return ERR_INVALID_CMD;
	}
	const string& cmd_head=args[0].first;
	if(cmd_head==CMD_SET_EXEC_MODE||cmd_head=="cd")
	{
		if(args.size()!=2||!args[1].second.empty())
		{
			if(!mute)
				printf("bad command format\n");
			return ERR_INVALID_CMD;
		}
	}
	else
	{
		int ret=0;
		for(int i=0;i<sizeof(s_alias)/sizeof(st_alias);i++)
		{
			if(cmd_head!=s_alias[i].alias)
				continue;
			if(0!=(ret=validate_param(args,s_alias[i])))
			{
				if(!mute)
					printf("bad command format\n");
				return ret;
			}
			break;
		}
	}
	return 0;
}
static inline void print_error_setmode()
{
	printf("the param of the command %s error.\n"
		"try \"%s %s\" or \"%s %s\".\n",CMD_SET_EXEC_MODE,
		CMD_SET_EXEC_MODE,EXEC_MODE_ALL,
		CMD_SET_EXEC_MODE,EXEC_MODE_NORM);
}
#ifdef USE_FS_ENV_VAR
static int print_env(FSEnvSet& env,const vector<pair<string,string>>& args)
{
	int ret=0;
	if(args.size()==1)
	{
		for(FSEnvSet::iterator it=env.BeginIterate();it;++it)
		{
			printf("%s",it->first.c_str());
			printf("=");
			printf("%s",it->second.c_str());
			printf("\n");
		}
	}
	else
	{
		for(int i=1;i<(int)args.size();i++)
		{
			if(!args[i].second.empty())
				printf("\'%s=%s\': invalid option\n",args[i].first.c_str(),args[i].second.c_str());
			const string& envname=args[i].first;
			string val;
			ret=env.FindEnv(envname,val);
			if(ret==0)
			{
				printf("%s",envname.c_str());
				printf("=");
				printf("%s",val.c_str());
				printf("\n");
			}
			else if(ret==ERR_ENV_NOT_FOUND)
			{
				printf("%s",envname.c_str());
				printf("=\n");
			}
			else
				printf("\'%s\': invalid environment variable name\n",envname.c_str());
		}
	}
	return 0;
}
static inline int handle_set_env_var(ctx_priv_data* privdata,const vector<pair<string,string>>& args,bool& bset)
{
	int ret=0;
	bset=false;
	bool del_flag=!!(priv2env(privdata).env_flags&CTXPRIV_ENVF_DEL);
	priv2env(privdata).env_flags&=(~CTXPRIV_ENVF_DEL);
	if(args.size()!=1)
		return 0;
	bool empty=args[0].second.empty();
	assert(!((!empty)&&del_flag));
	if(empty&&!del_flag)
		return 0;
	if(0!=(ret=priv2env(privdata).env_cache.SetEnv(args[0].first,args[0].second)))
	{
		const char* errmsg=NULL;
		switch(ret)
		{
		case ERR_INVALID_ENV_NAME:
			errmsg="Invalid environment variable name";
			break;
		default:
			assert(false);
			break;
		}
		printf("set environment variable \'%s\' to \'%s\' failed: %s\n",
			args[0].first.c_str(),args[0].second.c_str(),errmsg);
		return ret;
	}
	bset=true;
	return 0;
}
#endif
static int exec_cmd(const string& cmd)
{
	string final_cmd=quote_file(cmd);
	final_cmd=string("bash -c -i ")+final_cmd;
	int ret=system(final_cmd.c_str());
	if(tcsetpgrp(STDIN_FILENO,getpid())==-1)
	{
		printf("setpgrp error\n");
		return ERR_GENERIC;
	}
	return ret;
}
static int execute(sh_context* ctx)
{
	string cmd=ctx->cmd;
	vector<pair<string,string>> args;
	int ret=0;
	bool bsetenv=false;
	if(0!=(ret=parse_cmd((const byte*)cmd.c_str(),cmd.size(),args,ctx->priv)))
	{
#ifdef USE_FS_ENV_VAR
		switch(ret)
		{
		case ERR_INVALID_ENV_NAME:
			printf("Invalid environment variable name\n");
			return ret;
		case ERR_PARSE_ENV_FAILED:
			printf("Error occured while replacing environment variables\n");
			return ret;
		}
#endif
		if(ctx->flags&CTX_FLAG_EXEC_ALL)
			return exec_cmd(cmd);
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
#ifdef USE_FS_ENV_VAR
	if(0!=(ret=handle_set_env_var(ctx->priv,args,bsetenv))||bsetenv)
		return ret;
#endif
	if(args.empty())
		return 0;
	const string& cmd_head=args[0].first;
#ifdef USE_FS_ENV_VAR
	if(cmd_head==CMD_PRINT_ENV)
	{
		if(0!=(ret=check_args(args)))
			return ret;
		print_env(ctx2env(ctx).env_cache,args);
	}
	else
#endif
	if(cmd_head==CMD_SET_EXEC_MODE)
	{
		if(0!=(ret=check_args(args,true)))
		{
			print_error_setmode();
			return ret;
		}
		const string& cmd_param=args[1].first;
		if(cmd_param==EXEC_MODE_ALL)
			ctx->flags|=CTX_FLAG_EXEC_ALL;
		else if(cmd_param==EXEC_MODE_NORM)
			ctx->flags&=(~CTX_FLAG_EXEC_ALL);
		else
		{
			print_error_setmode();
			return ERR_INVALID_PARAM;
		}
	}
	else if(cmd_head=="cd")
	{
		if(0!=(ret=check_args(args)))
			return ret;
		string path(args[1].first);
		string abspath;
		if(0!=(ret=get_absolute_path(ctx->pwd,path,abspath,sys_is_absolute_path)))
		{
			printf("invalid path\n");
			return ret;
		}
		if(abspath.empty()||abspath[0]!='/')
			abspath=string("/")+abspath;
		if(0!=(ret=chdir(abspath.c_str())))
		{
			printf("command failed\n");
			return ret;
		}
	}
	else
	{
		for(int i=0;i<sizeof(s_alias)/sizeof(st_alias);i++)
		{
			if(cmd_head!=s_alias[i].alias)
				continue;
			if(0!=(ret=check_args(args)))
				return ret;
			args.erase(args.begin());
			int j;
			const char** p;
			for(j=0,p=s_alias[i].full;*p!=NULL;j++,p++)
				args.insert(args.begin()+j,pair<string,string>(*p,""));
			break;
		}
		generate_cmd(args,cmd);
		return exec_cmd(cmd);
	}
	return 0;
}
static int get_cmd_return(const char* cmd,string& retstr)
{
	int ret=0;
	FILE* fp=NULL;
	char data[1024]={0};
	fp=popen(cmd,"r");
	if(fp==NULL)
	{
		return -1;
	}
	retstr.clear();
	bool start=true;
	while(fgets(data,sizeof(data)-1,fp)!=NULL)
	{
		string sdata(data);
		if(!sdata.empty()&&sdata.back()=='\n')
			sdata.erase(sdata.end()-1);
		if(start)
			start=false;
		else
			retstr+=" ";
		retstr+=sdata;
	}
	pclose(fp);
	return 0;
}
#ifdef USE_LS_AS_LIST_FILES
void list_cur_dir_files(const string& dir,vector<string>& files)
{
	files.clear();
	string fulldir=quote_file(dir);
	string strfiles;
	vector<pair<string,string>> vfile;
	string lscmd=string("ls -aF")+(fulldir.empty()?"":" ")+fulldir;
	if(0!=get_cmd_return(lscmd.c_str(),strfiles))
		return;
	if(0!=parse_cmd((byte*)strfiles.c_str(),strfiles.size(),vfile,NULL))
		return;
	for(int i=0;i<(int)vfile.size();i++)
	{
		string file=vfile[i].first;
		if(file.empty())
			continue;
		byte c=(byte)file.back();
		for(int j=0;j<sizeof(ls_suffix)/sizeof(byte);j++)
		{
			if(c==ls_suffix[j])
			{
				file.erase(file.end()-1);
				break;
			}
		}
		if(!file.empty())
			files.push_back(file);
	}
}
#else
#ifndef _DIRENT_HAVE_D_TYPE
static int get_file_type(const char *filename)
{
	struct stat statbuf;
	if(stat(filename,&statbuf)!=0)
		return -1;
	if(S_ISDIR(statbuf.st_mode))
		return 1;
	return 0;
}
#endif
void list_cur_dir_files(const string& dir,vector<string>& files)
{
	files.clear();
	DIR *dirp;
	struct dirent *direntp;
	string fulldir=(dir.empty()?"./":dir);
	if((dirp=opendir(fulldir.c_str()))==NULL)
		return;
	while((direntp=readdir(dirp))!=NULL)
	{
		string file(direntp->d_name);
		int type;
#ifdef _DIRENT_HAVE_D_TYPE
		type=(direntp->d_type==DT_DIR?1:0);
#else
		string path=fulldir;
		if(!path.empty()&&path.back()!='/')
			path+="/";
		path+=file;
		type=get_file_type(path.c_str());
#endif
		if(type==1)
			file+="/";
		if(!file.empty())
			files.push_back(file);
	}
	closedir(dirp);
}
#endif
static int linux_cmd_handler(sh_context* ctx,dword state)
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
		ret=register_signal_handlers(state);
		init_ctx_priv(ctx);
		break;
	case FS_CMD_HANDLE_STATE_EXIT:
		ret=register_signal_handlers(state);
		destroy_ctx_priv(ctx);
		break;
	}
	if(state!=FS_CMD_HANDLE_STATE_EXIT&&get_cmd_return("pwd",ctx->pwd)!=0)
		ctx->pwd.clear();
	return ret;
}
int init_sh()
{
	set_cmd_handler(linux_cmd_handler);
	return 0;
}
