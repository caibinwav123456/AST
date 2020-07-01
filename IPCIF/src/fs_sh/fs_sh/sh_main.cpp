#include "fs_shell.h"
#include "sh_cmd_linux.h"
int main()
{
	int ret=0;
	if(0!=(ret=init_sh()))
	{
		printf("init failed\n");
		return ret;
	}
	return run_sh();
}