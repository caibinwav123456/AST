#include "fs_shell.h"
#include <conio.h>
static bool quit_get_char=false;
void quit_sh()
{
	quit_get_char=true;
}
uint get_char()
{
	while((!quit_get_char)&&kbhit()==0)
	{
		sys_sleep(10);
	}
	if(quit_get_char)
		return 0;
	uint c=(uint)getch();
	if(c==224)
	{
		c=(uint)getch();
		c|=CTRL_KEY;
	}
	return c;
}