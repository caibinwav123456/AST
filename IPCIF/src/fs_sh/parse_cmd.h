#ifndef _PARSE_CMD_H_
#define _PARSE_CMD_H_
#include "common.h"
#include "fs_shell.h"
#include <vector>
#include <string>
using namespace std;
int parse_cmd(const byte* buf,int size,
	vector<pair<string,string>>& args,ctx_priv_data* priv);
void generate_cmd(const vector<pair<string,string>>& args,string& cmd);
#endif
