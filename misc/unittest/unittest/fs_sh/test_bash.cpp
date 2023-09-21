#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
uint get_char();
int main(int argc,char** argv)
{
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	system("bash -c -i \'ls -alF\'");
	if(tcsetpgrp(STDIN_FILENO,getpid())==-1)
		printf("setpgrp error\n");
	printf("next>\n");
	for(;;)
	{
		uint c=get_char();
		if(c!=EOF)
			printf("%c: %d\n",(char)c,c);
	}
	return 0;
}
