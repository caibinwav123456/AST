#ifdef USE_FS_ENV_VAR
#ifndef _FSENV_H_
#define _FSENV_H_
#include "common.h"
#include <string>
#include <map>
using namespace std;
class FSEnvSet
{
public:
	//To delete an environment variable, set the value of the variable to an empty string.
	int SetEnv(const string& envname,const string& envval);
	int FindEnv(const string& envname,string& envval) const;
	int ReplaceEnv(const string& origin,string& output) const;
private:
	map<string,string> env_assoc;
};
#endif
#endif