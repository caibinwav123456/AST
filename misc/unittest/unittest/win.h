#ifndef _WIN_H_
#define _WIN_H_
#include "common.h"
void set_last_error(dword err);
dword get_last_error();
double get_tick_freq();
double get_tick_hq();
#endif