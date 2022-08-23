#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "common.h"
#include "key_def.h"
static bool quit_get_char=false;
void quit_sh()
{
	quit_get_char=true;
}
static uint getch()
{
	struct termios term_old;
	ioctl( STDIN_FILENO, TCGETS, &term_old );
	struct termios term_new = term_old;
	term_new.c_lflag &= ~( ECHO | ICANON );
	ioctl(STDIN_FILENO, TCSETS, &term_new);
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
	uint ch;
	while((!quit_get_char)&&(ch=(uint)getchar())==EOF)
		usleep(1000);
	if(quit_get_char)
		while((uint)getchar()!=EOF);
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
	ioctl(STDIN_FILENO, TCSETS, &term_old);
	return quit_get_char?0:ch;
}
uint get_char()
{
	uint c=getch();
	if(c==27)
	{
		getch();
		c=getch();
		if(c==0)
			return 0;
		if(c>=(uint)'1'&&c<=(uint)'6')
			getch();
		c|=CTRL_KEY;
	}
	return c;
}