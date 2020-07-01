#include "common.h"
#include <string.h>

int sys_is_absolute_path(char* path, char dsym)
{
	char* slash=strchr(path,dsym);
	if(slash==path)
		return 1;
	else
		return 0;
}

