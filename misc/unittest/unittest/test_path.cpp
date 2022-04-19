#include "common.h"
#include "path.h"
#include "utpath.h"
#include "win.h"
#define INIT_FREQ() double __freq__=get_tick_freq()
#define BEGIN_CNT(time) double __##time##__=get_tick_hq()
#define END_CNT(time) {__##time##__=get_tick_hq()-__##time##__;printf("%s: %lf\n",#time,__##time##__/__freq__);}
static bool fs_extract_drive_tag(const string& path,string& tag,string& pure)
{
	const int pos=path.find('/');
	string first_dir;
	if(pos==0)
	{
		tag="";
		pure=path;
		return true;
	}
	else if(pos==string::npos)
		first_dir=path;
	else
		first_dir=path.substr(0,pos);
	if((!first_dir.empty())&&first_dir.back()==':')
	{
		tag=first_dir;
		pure=(pos==string::npos?"":path.substr(pos+1));
		return true;
	}
	else
	{
		tag="";
		pure=path;
		return false;
	}
}
static int get_full_path(const string& cur_dir,const string& relative_path,string& full_path)
{
	int ret=0;
	string tag,pure;
	if(fs_extract_drive_tag(relative_path,tag,pure))
	{
		if(0!=(ret=legacy::get_absolute_path(pure,"",full_path,NULL,'/')))
			return ret;
	}
	else
	{
		if(!fs_extract_drive_tag(cur_dir,tag,pure))
			return ERR_INVALID_PATH;
		if(0!=(ret=legacy::get_absolute_path(pure,relative_path,full_path,NULL,'/')))
			return ret;
	}
	full_path=tag+"/"+full_path;
	return 0;
}
static int utget_full_path(const string& cur_dir,const string& relative_path,string& full_path,path_cache_type type)
{
	int ret=0;
	string tag,pure;
	if(fs_extract_drive_tag(relative_path,tag,pure))
	{
		if(0!=(ret=get_absolute_path(pure,"",full_path,NULL,'/',type)))
			return ret;
	}
	else
	{
		if(!fs_extract_drive_tag(cur_dir,tag,pure))
			return ERR_INVALID_PATH;
		if(0!=(ret=get_absolute_path(pure,relative_path,full_path,NULL,'/',type)))
			return ret;
	}
	full_path=tag+"/"+full_path;
	return 0;
}
static void parse_path(const string& cur_dir,const string& path,string& fullpath)
{
	for(int i=0;i<1024;i++)
	{
		get_full_path(cur_dir,path,fullpath);
	}
}
static void utparse_path(const string& cur_dir,const string& path,string& fullpath,path_cache_type type)
{
	for(int i=0;i<1024;i++)
	{
		utget_full_path(cur_dir,path,fullpath,type);
	}
}
int test_new_path()
{
	//unittest::path_cache cache;
	//unittest::path_cache cache2(cache);
	//unittest::path_cache cache3=cache2;
	//cache3=cache;
	INIT_FREQ();
	string path("D:/Programs/IPCIF/bin/Debug/astdata/disk1/sto0_bak/IPCIF"),
		path1("../Programs/IPCIF/bin/Debug/astdata/disk1/sto0_bak/IPCIF"),
		cur_dir("sto1:/Programs/IPCIF/bin/../../");
	string fullpath;

	parse_path(cur_dir,path,fullpath);
	BEGIN_CNT(T1);
	parse_path(cur_dir,path1,fullpath);
	END_CNT(T1);

	utparse_path(cur_dir,path,fullpath,PCT_STL_STYLE);
	utparse_path(cur_dir,path1,fullpath,PCT_STL_STYLE);

	utparse_path(cur_dir,path,fullpath,PCT_C_STYLE);
	BEGIN_CNT(T2);
	utparse_path(cur_dir,path1,fullpath,PCT_C_STYLE);
	END_CNT(T2);

	path_cache cache1(PCT_C_STYLE), cache2(PCT_C_STYLE);
	split_path(path,cache1,'/');
	split_path(path,cache2,'/');
	cache1.clear();
	cache2.erase(cache2.begin(),cache2.end());

	return 0;
}