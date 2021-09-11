#include "sh_cmd_linux.h"
#include "fs_shell.h"
#include "parse_cmd.h"
#include "path.h"
#include "complete.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
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

static const byte ls_suffix[]={'*','@','|','>'};
struct st_alias
{
	const char* alias;
	const char* full;
	uint min_args;
	uint max_args;
};
static const st_alias s_alias[]={
	{"ll","ls -alF",0,INFINITE},
	{"la","ls -A",0,INFINITE},
	{"l","ls -CF",0,INFINITE}
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
		for(int i=0;i<sizeof(s_alias)/sizeof(st_alias);i++)
		{
			if(cmd_head==s_alias[i].alias)
			{
				int ret=validate_param(args,s_alias[i]);
				if(ret!=0)
				{
					if(!mute)
						printf("bad command format\n");
					return ret;
				}
				break;
			}
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
static int execute(sh_context* ctx)
{
	string cmd=ctx->cmd;
	vector<pair<string,string>> args;
	int ret=0;
	if(0!=(ret=parse_cmd((const byte*)cmd.c_str(),cmd.size(),args)))
	{
		if(ctx->flags&CTX_FLAG_EXEC_ALL)
			return system(cmd.c_str());
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
	const string& cmd_head=args[0].first;
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
			if(cmd_head==s_alias[i].alias)
			{
				if(0!=(ret=check_args(args)))
					return ret;
				args[0].first=s_alias[i].full;
				generate_cmd(args,cmd);
				break;
			}
		}
		if(0!=(ret=system(cmd.c_str())))
			return ret;
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
	if(0!=parse_cmd((byte*)strfiles.c_str(),strfiles.size(),vfile))
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
	if(state==FS_CMD_HANDLE_STATE_EXEC)
	{
		if(ctx->c==(uint)'\t')
			complete(ctx);
		else
			ret=execute(ctx);
	}
	if(0!=get_cmd_return("pwd",ctx->pwd))
		ctx->pwd.clear();
	return ret;
}
int init_sh()
{
	set_cmd_handler(linux_cmd_handler);
	return 0;
}
