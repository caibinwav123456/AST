#ifndef _SH_CMD_LINUX_H_
#define _SH_CMD_LINUX_H_
#include "common.h"
#include <vector>
#include <string>
using namespace std;
int init_sh();
void list_cur_dir_files(const string& dir,vector<string>& files);
#endif