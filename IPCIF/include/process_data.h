#ifndef _PROC_DATA_H_
#define _PROC_DATA_H_
#include <vector>
#include <string>
using namespace std;
struct if_proc
{
	void* hif;
	string id;
	string usage;
	int prior;
};
struct proc_data
{
	void* hproc;
	void* hthrd_shelter;
	string name;
	void* id;
	vector<if_proc> ifproc;
};
#endif

