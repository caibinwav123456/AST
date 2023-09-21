#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
		uint c=getchar();
		if(c!=EOF)
			printf("%d\n",c);
	}
	return 0;
}
