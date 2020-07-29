#include "fs_shell.h"
#include "utility.h"
#include "sh_cmd_fs.h"
int main(int argc, char** argv)
{
	if(0!=mainly_initial())
		return ERR_GENERIC;
	int ret=0;
	if(0!=(ret=init_sh()))
	{
		printf("init failed\n");
		goto end;
	}
	ret=run_sh();
	exit_sh();
end:
	mainly_exit();
	return ret;
}
