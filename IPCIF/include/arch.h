#ifndef _ARCH_H_
#define _ARCH_H_
#include "common.h"
#include "process_data.h"
DLLAPI(void*) arch_get_process(const proc_data& data);
DLLAPI(bool) arch_has_dup_process(const proc_data& data);
DLLAPI(int) arch_get_current_process_cmdline(char* cmdline);
#endif
