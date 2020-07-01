#ifndef _SH_CMD_FS_H_
#define _SH_CMD_FS_H_
#include "common.h"
#include <vector>
#include <string>
using namespace std;
int init_sh();
void exit_sh();
int get_full_path(const string& cur_dir,const string& relative_path,string& full_path);
void list_cur_dir_files(const string& dir,vector<string>& files);
#endif