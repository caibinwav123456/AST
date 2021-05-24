#ifndef _FS_UTILS_H_
#define _FS_UTILS_H_
#include "common.h"
#include <map>
#include <string>
using namespace std;
int parse_cmd(const byte* buf,int size,
	map<string,string>& configs);
bool dev_is_locked(const string& devname);
bool lock_dev(const string& devname,bool lock);
#endif