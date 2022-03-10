#ifdef USE_FS_ENV_VAR
#ifndef _FSENV_H_
#define _FSENV_H_
#include "common.h"
#include <vector>
#include <string>
#include <map>
using namespace std;
struct solid_data_segment
{
	uint start;
	uint end;
};
struct solid_data
{
	vector<solid_data_segment> segments;
};
class FSEnvSet
{
public:
	//To delete an environment variable, set the value of the variable to an empty string.
	int SetEnv(const string& envname,const string& envval);
	int FindEnv(const string& envname,string& envval) const;
	int ReplaceEnv(const string& origin,string& output,solid_data* solid=NULL) const;
private:
	map<string,string> env_assoc;
};
#endif
#endif