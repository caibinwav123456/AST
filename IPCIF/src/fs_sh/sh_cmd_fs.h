#ifndef _SH_CMD_FS_H_
#define _SH_CMD_FS_H_
#include "common.h"
#include "fs_shell.h"
#include <vector>
#include <string>
#include <map>
using namespace std;
int init_sh();
void exit_sh();
int get_full_path(const string& cur_dir,const string& relative_path,string& full_path);
void list_cur_dir_files(const string& dir,vector<string>& files);
typedef int (*per_cmd_handler)(sh_context* ctx,const string& cmd,vector<pair<string,string>>& args);
class ShCmdTable
{
public:
	struct CmdItem
	{
		per_cmd_handler handler;
		const char* detail;
	};
	static void AddCmd(const char* cmd,per_cmd_handler handler,const char* desc,const char* detail);
	static int Init();
	static int ExecCmd(sh_context* ctx,const string& cmd,vector<pair<string,string>>& args);
	static void PrintDesc();
	static int PrintDetail(const string& cmd);
private:
	ShCmdTable(){}
	vector<string> vstrdesc;
	map<string,CmdItem> cmd_map;
	static ShCmdTable* GetTable();
};
class CfgCmd
{
public:
	CfgCmd(const char* cmd,per_cmd_handler handler,const char* desc,const char* detail)
	{
		ShCmdTable::AddCmd(cmd,handler,desc,detail);
	}
};
#define DEF_SH_CMD(cmd,handler,desc,detail) CfgCmd __config_cmd_ ## cmd (#cmd,handler,desc,detail)
#endif