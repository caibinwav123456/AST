#ifndef _COMPLETE_H_
#define _COMPLETE_H_
#include "fs_shell.h"
#include "utility.h"
char need_quote(const string& file);
string quote_file(const string& file);
void sort_file_list(vector<string*>& sorted,vector<string>& unsorted);
void display_file_list(vector<string*>& files);
void complete(sh_context* ctx);
#endif
