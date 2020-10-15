#ifndef _SH_CMD_FS_H_
#define _SH_CMD_FS_H_
#include "common.h"
#include "fs_shell.h"
#include "datetime.h"
#include "Integer64.h"
#include "pipe.h"
#include <vector>
#include <string>
#include <map>
using namespace std;
#define return_msg(code,msg,...) \
	{printf(msg,##__VA_ARGS__); \
	return code;}
#define CMD_PARAM_FLAG_PRE_REVOKE 1
#define CMD_PARAM_FLAG_USED_PIPE  2
#define is_pre_revoke(flags) ((flags)&CMD_PARAM_FLAG_PRE_REVOKE)
#define used_pipe(flags) ((flags)&CMD_PARAM_FLAG_USED_PIPE)
int init_sh();
void exit_sh();
int get_full_path(const string& cur_dir,const string& relative_path,string& full_path);
bool validate_path(const string& path,dword* flags=NULL,DateTime* date=NULL,UInteger64* size=NULL,bool mute=false);
void list_cur_dir_files(const string& dir,vector<string>& files);
struct cmd_param_st
{
	Pipe* stream;
	Pipe* stream_next;
	sh_context* ctx;
	void* hthread;
	void* priv;
	int ret;
	dword flags;
	cmd_param_st* prev;
	cmd_param_st* next;
	string cmd;
	vector<pair<string,string>> args;
	cmd_param_st(sh_context* _ctx):ctx(_ctx)
	{
		prev=NULL;
		next=NULL;
		stream=NULL;
		stream_next=NULL;
		hthread=NULL;
		priv=NULL;
		ret=0;
		flags=0;
	}
	~cmd_param_st()
	{
		if(stream!=NULL)
			delete stream;
		if(next!=NULL)
			delete next;
	}
};
#define common_sh_args(ptr) \
	sh_context* ctx=ptr->ctx; \
	const string& cmd=ptr->cmd; \
	vector<pair<string,string>>& args=ptr->args; \
	Pipe* pipe=ptr->stream; \
	Pipe* pipe_next=ptr->stream_next
typedef int (*per_cmd_handler)(cmd_param_st* param);
struct sh_thread_param
{
	per_cmd_handler handler;
	cmd_param_st* param;
};
class ShCmdTable
{
public:
	static void AddCmd(const char* cmd,per_cmd_handler handler,per_cmd_handler pre_handler,const char* desc,const char* detail);
	static int Init();
	static int ExecCmd(sh_context* ctx,const vector<pair<string,string>>& args);
	static void PrintDesc();
	static int PrintDetail(const string& cmd);
private:
	struct CmdItem
	{
		per_cmd_handler handler;
		per_cmd_handler pre_handler;
		const char* detail;
	};
	ShCmdTable(){}
	vector<string> vstrdesc;
	map<string,CmdItem> cmd_map;
	static ShCmdTable* GetTable();
	static bool find_cmd(const string& cmd,CmdItem* cmditem);
	static void revoke_preprocess(cmd_param_st* param,const vector<CmdItem>& callback,cmd_param_st* end=NULL);
};
class CfgCmd
{
public:
	CfgCmd(const char* cmd,per_cmd_handler handler,per_cmd_handler pre_handler,const char* desc,const char* detail)
	{
		ShCmdTable::AddCmd(cmd,handler,pre_handler,desc,detail);
	}
};
#define DEF_SH_PRED_CMD(cmd,handler,pre_handler,desc,detail) CfgCmd __config_cmd_ ## cmd ( #cmd, handler, pre_handler, desc, detail )
#define DEF_SH_CMD(cmd,handler,desc,detail) DEF_SH_PRED_CMD(cmd,handler,NULL,desc,detail)
#endif