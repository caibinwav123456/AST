#ifndef _SH_CMD_FS_H_
#define _SH_CMD_FS_H_
#include "common.h"
#include "fs_shell.h"
#include "datetime.h"
#include "Integer64.h"
#include "pipe.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <vector>
#include <string>
#include <map>
using namespace std;
struct st_stat_file_time
{
	fs_attr_datetime ttype;
	DateTime time;
};
#define return_msg(code,msg,...) \
	{printf((msg),##__VA_ARGS__); \
	return code;}
#define t_output(msg,...) tprintf(pipe_next,__tp_buf__,(msg),##__VA_ARGS__)
#define tb_output(ptr,len) tputs(pipe_next,ptr,len)
#define ts_output(ptr) tputs(pipe_next,ptr,0,true)
#define return_t_msg(code,msg,...) \
	{t_output((msg),##__VA_ARGS__); \
	return code;}
#define t_clean_files(files) tclean_hfiles(files,pipe_next,__tp_buf__)
#define t_clean_file(file) tclean_hfile(file,pipe_next,__tp_buf__)
#define term_stream(pcmd) \
	if((pcmd)->stream_next!=NULL) \
		(pcmd)->stream_next->Send(NULL,0)
#define drain_stream(pcmd) \
	if((pcmd)->stream!=NULL) \
	{ \
		byte buf[256]; \
		while((pcmd)->stream->Recv(buf,256)!=0); \
	}
#define common_sh_args(ptr) \
	sh_context* ctx=(ptr)->ctx; \
	const string& cmd=(ptr)->cmd; \
	const vector<pair<string,string>>& args=(ptr)->args; \
	Pipe* pipe=(ptr)->stream; \
	Pipe* pipe_next=(ptr)->stream_next; \
	byte*& __tp_buf__=(ptr)->tp_buf
#define CMD_PARAM_FLAG_PRE_REVOKE 1
#define CMD_PARAM_FLAG_USED_PIPE  2
#define is_pre_revoke(param) (((param)->flags)&CMD_PARAM_FLAG_PRE_REVOKE)
#define used_pipe(param) (((param)->flags)&CMD_PARAM_FLAG_USED_PIPE)
#define set_used_pipe(param) ((param)->flags|=CMD_PARAM_FLAG_USED_PIPE)
#define reset_used_pipe(param) ((param)->flags&=~CMD_PARAM_FLAG_USED_PIPE)
int init_sh();
void exit_sh();
int get_full_path(const string& cur_dir,const string& relative_path,string& full_path);
int validate_path(const string& path,dword* flags=NULL,st_stat_file_time* date=NULL,UInteger64* size=NULL,string* strret=NULL);
void list_cur_dir_files(const string& dir,vector<string>& files);
struct cmd_param_st
{
	Pipe* stream;
	Pipe* stream_next;
	sh_context* ctx;
	void* hthread;
	byte* tp_buf;
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
		tp_buf=NULL;
		priv=NULL;
		ret=0;
		flags=0;
	}
	~cmd_param_st()
	{
		if(stream!=NULL)
			delete stream;
		if(tp_buf!=NULL)
			delete[] tp_buf;
		if(next!=NULL)
			delete next;
	}
};
typedef int (*per_cmd_handler)(cmd_param_st* param);
struct sh_thread_param
{
	per_cmd_handler handler;
	cmd_param_st* param;
};
struct st_path_handle
{
	string path;
	void* hfile;
};
void prepare_tp_buf(byte*& buf);
inline void tprintf(Pipe* pipe,byte*& buf,const char* msg,...)
{
	va_list args;
	va_start(args,msg);
	if(pipe==NULL)
		vprintf(msg,args);
	else
	{
		prepare_tp_buf(buf);
		vsprintf((char*)buf,msg,args);
		uint len=strlen((char*)buf);
		if(len>0)
			pipe->Send(buf,len);
	}
	va_end(args);
}
inline void tputs(Pipe* pipe,const void* ptr,uint len,bool bs=false)
{
	if(ptr==NULL||(len==0&&!bs))
		return;
	if(pipe==NULL)
	{
		if(bs)
			printf("%s",(const char*)ptr);
		else
			fwrite(ptr,len,1,stdout);
	}
	else
	{
		uint slen=(bs?strlen((const char*)ptr):len);
		if(slen>0)
			pipe->Send(ptr,slen);
	}
}
inline void format_segmented_u64(const UInteger64& u64,string& dispsz)
{
	string sz=FormatI64(u64);
	dispsz.clear();
	for(int i=0;i<(int)sz.size();i+=3)
	{
		string sec=((int)sz.size()>i+3?sz.substr(sz.size()-i-3,3):sz.substr(0,sz.size()-i));
		dispsz=sec+(i==0?"":",")+dispsz;
	}
}
class ShCmdTable
{
public:
	static void AddCmd(const char* cmd,per_cmd_handler handler,per_cmd_handler pre_handler,const char* desc,const char* detail);
	static int Init();
	static int ExecCmd(sh_context* ctx,const vector<pair<string,string>>& args);
	static void PrintDesc(cmd_param_st* pcmd);
	static int PrintDetail(const string& dcmd,cmd_param_st* pcmd);
private:
	struct CmdItem
	{
		per_cmd_handler handler;
		per_cmd_handler pre_handler;
		const char* detail;
	};
	ShCmdTable(){}
	vector<string> vstrdesc;
	vector<const char*> pstrdesc;
	map<string,CmdItem> cmd_map;
	static ShCmdTable* GetTable();
	static bool find_cmd(const string& cmd,CmdItem* cmditem);
	static void revoke_preprocess(cmd_param_st* param,const vector<CmdItem>& callback,cmd_param_st* end=NULL);
	static bool less_str(const char* a,const char* b);
};
class CfgCmd
{
public:
	CfgCmd(const char* cmd,per_cmd_handler handler,per_cmd_handler pre_handler,const char* desc,const char* detail)
	{
		ShCmdTable::AddCmd(cmd,handler,pre_handler,desc,detail);
	}
};
int ban_pre_handler(cmd_param_st* param);
int tclean_hfile(const st_path_handle& file,Pipe* pipe,byte*& buf);
int tclean_hfiles(const vector<st_path_handle>& vfiles,Pipe* pipe,byte*& buf);
#define DEF_SH_PRED_CMD(cmd,handler,pre_handler,desc,detail) CfgCmd __config_cmd_ ## cmd ( #cmd, handler, pre_handler, desc, detail )
#define DEF_SH_CMD(cmd,handler,desc,detail) DEF_SH_PRED_CMD(cmd,handler,ban_pre_handler,desc,detail)
#endif