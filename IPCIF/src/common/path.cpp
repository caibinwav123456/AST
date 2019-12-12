#include "common.h"
#include "path.h"
#include "dir_symbol.h"
void split_path(const string& path, vector<string>& split)
{
	int pos=0;
	int nextpos=0;
	while((nextpos=path.find(_dir_symbol,pos))!=-1)
	{
		if(nextpos>pos)
			split.push_back(path.substr(pos,nextpos-pos));
		pos=nextpos+1;
	}
	if(pos<(int)path.length())
		split.push_back(path.substr(pos,path.length()-pos));
}
inline bool add_path_compenent(vector<string>& path, const string& comp)
{
	if(comp==".")
		return true;
	else if(comp=="..")
	{
		if(path.size()>0)
		{
			path.pop_back();
			return true;
		}
		return false;
	}
	else
	{
		path.push_back(comp);
		return true;
	}
}
int get_absolute_path(const string& cur_dir, const vector<string>& relative_path, vector<string>& absolute_path)
{
	vector<string> cur_dir_split;
	split_path(cur_dir,cur_dir_split);
	absolute_path.clear();
	int ret=0;
	cur_dir_split.insert(cur_dir_split.end(),relative_path.begin(),relative_path.end());
	for(int i=0;i<(int)cur_dir_split.size();i++)
	{
		if(!add_path_compenent(absolute_path, cur_dir_split[i]))
			ret=ERR_INVALID_PATH;
	}
	return ret;
}
int get_absolute_path(const string& cur_dir, const string& relative_path, vector<string>& absolute_path)
{
	vector<string> relative_path_split;
	split_path(relative_path, relative_path_split);
	return get_absolute_path(cur_dir, relative_path_split, absolute_path);
}
DLLAPI(int) get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path)
{
	if(sys_is_absolute_path((char*)relative_path.c_str()))
	{
		absolute_path=relative_path;
		return 0;
	}
	vector<string> array_absolute_path;
	int ret=get_absolute_path(cur_dir, relative_path, array_absolute_path);
	if(ret!=0)
		return ret;
	absolute_path.clear();
	for(int i=0;i<(int)array_absolute_path.size();i++)
	{
		absolute_path+=array_absolute_path[i];
		if(i!=(int)array_absolute_path.size()-1)
			absolute_path+=_dir_symbol;
	}
	return 0;
}
DLLAPI(void) concat_path(const string& path1, const string& path2, string& merge)
{
	vector<string> path_array1, path_array2;
	split_path(path1,path_array1);
	split_path(path2,path_array2);
	path_array1.insert(path_array1.end(),path_array2.begin(),path_array2.end());
	merge.clear();
	for(int i=0;i<(int)path_array1.size();i++)
	{
		merge+=path_array1[i];
		if(i!=(int)path_array1.size()-1)
			merge+=_dir_symbol;
	}
}
DLLAPI(bool) is_subpath(const string& cur_dir, const string& relative_path1, const string& relative_path2)
{
	vector<string> absolute1,absolute2;
	if(sys_is_absolute_path(const_cast<char*>(relative_path1.c_str())))
	{
		get_absolute_path(relative_path1,string(""),absolute1);
	}
	else
	{
		get_absolute_path(cur_dir,relative_path1,absolute1);
	}
	if(sys_is_absolute_path(const_cast<char*>(relative_path2.c_str())))
	{
		get_absolute_path(relative_path2,string(""),absolute2);
	}
	else
	{
		get_absolute_path(cur_dir,relative_path2,absolute2);
	}
	if(absolute1.size()<=absolute2.size())
	{
		return false;
	}
	for(int i=0;i<(int)absolute2.size();i++)
	{
		if(absolute1[i]!=absolute2[i])
		{
			return false;
		}
	}
	return true;
}
