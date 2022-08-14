#ifndef _UTPATH_H_
#define _UTPATH_H_
#include "dir_symbol.h"
#include <string>
#include <vector>
using namespace std;
namespace legacy {
void split_path(const string& path, vector<string>& split, char dsym=_dir_symbol);
void merge_path(string& path, const vector<string>& split, char dsym=_dir_symbol);
int get_direct_path(vector<string>& direct, const vector<string>& indirect);
int get_absolute_path(const string& cur_dir, const vector<string>& relative_path, vector<string>& absolute_path, char dsym=_dir_symbol);
int get_absolute_path(const string& cur_dir, const string& relative_path, vector<string>& absolute_path, char dsym=_dir_symbol);
int get_absolute_path(const string& cur_dir, const string& relative_path, string& absolute_path, int (*is_absolute_path)(char* path, char dsym)=NULL, char dsym=_dir_symbol);
void concat_path(const string& path1, const string& path2, string& merge, char dsym=_dir_symbol);
bool is_subpath(const string& relative_path1, const string& relative_path2, char dsym=_dir_symbol);
} //namespace legacy
#endif