#ifndef _CMD_PARSE_H_
#define _CMD_PARSE_H_
#include "common.h"
#include <map>
#include <string>
using namespace std;
int parse_cmd(const byte* buf,int size,
	map<string,string>& configs);
bool dev_is_locked(const string& devname);
bool lock_dev(const string& devname,bool lock);
#endif