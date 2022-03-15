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
	class iterator
	{
		friend class FSEnvSet;
		iterator(){}
		map<string,string>::iterator iter;
		map<string,string>::iterator end;
		bool valid;
	public:
		operator bool() const
		{
			return valid;
		}
		iterator operator++(int)
		{
			iterator it=*this;
			if(valid)
			{
				iter++;
				valid=(iter!=end);
			}
			return it;
		}
		iterator& operator++()
		{
			if(valid)
			{
				iter++;
				valid=(iter!=end);
			}
			return *this;
		}
		pair<const string,string>& operator*()
		{
			return *iter;
		}
		pair<const string,string>* operator->()
		{
			return &*iter;
		}
	};
	//To delete an environment variable, set the value of the variable to an empty string.
	int SetEnv(const string& envname,const string& envval);
	int FindEnv(const string& envname,string& envval) const;
	iterator BeginIterate();
	int ReplaceEnv(const string& origin,string& output,solid_data* solid=NULL) const;
private:
	map<string,string> env_assoc;
};
#endif //#ifdef USE_FS_ENV_VAR
#endif //#ifndef _FSENV_H_