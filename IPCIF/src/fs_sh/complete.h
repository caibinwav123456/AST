#ifndef _COMPLETE_H_
#define _COMPLETE_H_
#include "fs_shell.h"
char need_quote(const string& file);
string quote_file(const string& file);
void complete(sh_context* ctx);
#endif
