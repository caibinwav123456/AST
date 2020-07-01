#ifndef _PARSE_CMD_H_
#define _PARSE_CMD_H_
#include "common.h"
#include <vector>
#include <string>
using namespace std;
int parse_cmd(const byte* buf,int size,
	vector<pair<string,string>>& args);
void generate_cmd(const vector<pair<string,string>>& args,string& cmd);
#endif
