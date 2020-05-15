#ifndef _PATH_H_
#define _PATH_H_
#include "dir_symbol.h"
#include <string>
#include <vector>
using namespace std;
DLLAPI(void) split_path(const string& path, vector<string>& split, char dsym=_dir_symbol);
DLLAPI(void) merge_path(string& path, vector<string>& split, char dsym=_dir_symbol);
DLLAPI(int)get_direct_path(vector<string>& direct, vector<string>& indirect);
int get_absolute_path(const string& cur_dir, const vector<string>& relative_path, vector<string>& absolute_path, char dsym=_dir_symbol);
int get_absolute_path(const string& cur_dir, const string& relative_path, vector<string>& absolute_path, char dsym=_dir_symbol);
DLLAPI(int) get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path, char dsym=_dir_symbol);
DLLAPI(void) concat_path(const string& path1, const string& path2, string& merge, char dsym=_dir_symbol);
DLLAPI(bool) is_subpath(const string& cur_dir, const string& relative_path1, const string& relative_path2, char dsym=_dir_symbol);
#endif