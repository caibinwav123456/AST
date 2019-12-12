#ifndef _PATH_H_
#define _PATH_H_
#include <string>
#include <vector>
#include <string.h>
using namespace std;
void split_path(const string& path, vector<string>& split);
int get_absolute_path(const string& cur_dir, const vector<string>& relative_path, vector<string>& absolute_path);
int get_absolute_path(const string& cur_dir, const string& relative_path, vector<string>& absolute_path);
DLLAPI(int) get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path);
DLLAPI(void) concat_path(const string& path1, const string& path2, string& merge);
DLLAPI(bool) is_subpath(const string& cur_dir, const string& relative_path1, const string& relative_path2);
#endif