#ifndef _SYS_LOG_H_
#define _SYS_LOG_H_
#include "common.h"
#include <string>
using namespace std;
class LogSys
{
public:
	LogSys();
	int Init();
	void Exit();
	int Log(const char*const* lognames,const char* file,int line,const string& logmsg);
private:
	bool inited;
	void* sem;
	string full_sys_log_path;
};
#endif