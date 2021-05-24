#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#define IS_DISP_KEY(key) ((key)>=32&&(key)<=126)

int getch(void)
{
	struct termios term_old;
	ioctl( STDIN_FILENO, TCGETS, &term_old );
	struct termios term_new = term_old;
	term_new.c_lflag &= ~( ECHO | ICANON );
	ioctl( STDIN_FILENO, TCSETS, &term_new );
	int ch = getchar();
	ioctl(STDIN_FILENO, TCSETS, &term_old);
	return ch;
}
void main()
{
	int c;
	for(;;)
	{
		c=getch();
		char buf[16];
		IS_DISP_KEY(c)?sprintf(buf," \'%c\'",(char)c):0;
		printf("char: %d%s\n",c,IS_DISP_KEY(c)?buf:"");
		if(c==(int)'q')
		{
			printf("quit?(y/n)");
			c=getc(stdin);
			if(c==(int)'y')
				break;
		}
	}
}
