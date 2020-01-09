#include "common.h"
#include "path.h"
DLLAPI(void) split_path(const string& path, vector<string>& split, char dsym)
{
	int pos=0;
	int nextpos=0;
	while((nextpos=path.find(dsym,pos))!=-1)
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
DLLAPI(void) merge_path(string& path, vector<string>& split, char dsym)
{
	path.clear();
	for(int i=0;i<(int)split.size();i++)
	{
		path+=split[i];
		if(i!=(int)split.size()-1)
			path+=dsym;
	}
}
DLLAPI(int)get_direct_path(vector<string>& direct, vector<string>& indirect)
{
	direct.clear();
	for(int i=0;i<(int)indirect.size();i++)
	{
		if(!add_path_compenent(direct, indirect[i]))
		{
			direct.clear();
			return ERR_INVALID_PATH;
		}
	}
	return 0;
}
int get_absolute_path(const string& cur_dir, const vector<string>& relative_path, vector<string>& absolute_path, char dsym)
{
	vector<string> cur_dir_split;
	split_path(cur_dir,cur_dir_split,dsym);
	cur_dir_split.insert(cur_dir_split.end(),relative_path.begin(),relative_path.end());
	return get_direct_path(absolute_path, cur_dir_split);
}
int get_absolute_path(const string& cur_dir, const string& relative_path, vector<string>& absolute_path, char dsym)
{
	vector<string> relative_path_split;
	split_path(relative_path, relative_path_split, dsym);
	return get_absolute_path(cur_dir, relative_path_split, absolute_path, dsym);
}
DLLAPI(int) get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path, char dsym)
{
	if(sys_is_absolute_path((char*)relative_path.c_str(),dsym))
	{
		absolute_path=relative_path;
		return 0;
	}
	vector<string> array_absolute_path;
	int ret=get_absolute_path(cur_dir, relative_path, array_absolute_path, dsym);
	if(ret!=0)
		return ret;
	merge_path(absolute_path, array_absolute_path);
	return 0;
}
DLLAPI(void) concat_path(const string& path1, const string& path2, string& merge, char dsym)
{
	vector<string> path_array1, path_array2;
	split_path(path1,path_array1,dsym);
	split_path(path2,path_array2,dsym);
	path_array1.insert(path_array1.end(),path_array2.begin(),path_array2.end());
	merge_path(merge, path_array1);
}
DLLAPI(bool) is_subpath(const string& cur_dir, const string& relative_path1, const string& relative_path2, char dsym)
{
	vector<string> absolute1,absolute2;
	if(sys_is_absolute_path(const_cast<char*>(relative_path1.c_str()),dsym))
	{
		get_absolute_path(relative_path1,string(""),absolute1,dsym);
	}
	else
	{
		get_absolute_path(cur_dir,relative_path1,absolute1,dsym);
	}
	if(sys_is_absolute_path(const_cast<char*>(relative_path2.c_str()),dsym))
	{
		get_absolute_path(relative_path2,string(""),absolute2,dsym);
	}
	else
	{
		get_absolute_path(cur_dir,relative_path2,absolute2,dsym);
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
