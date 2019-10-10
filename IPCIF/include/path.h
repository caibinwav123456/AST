#ifndef _PATH_H_
#define _PATH_H_
#include <string>
#include <vector>
#include <string.h>
using namespace std;
#ifdef USE_BACKSLASH
#define _dir_symbol '\\'
#else
#define _dir_symbol '/'
#endif
inline void normalize_path(char* buf)
{
	char* ptr=strrchr(buf,_dir_symbol);
	if(ptr!=NULL&&ptr==buf+strlen(buf)-1)
		*ptr=0;
}
void split_path(const string& path, vector<string>& split);
int get_absolute_path(const string& cur_dir, const vector<string>& relative_path, vector<string>& absolute_path);
int get_absolute_path(const string& cur_dir, const string& relative_path, vector<string>& absolute_path);
DLLAPI(int) get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path);
DLLAPI(void) concat_path(const string& path1, const string& path2, string& merge);
DLLAPI(bool) is_subpath(const string& cur_dir, const string& relative_path1, const string& relative_path2);
#endif