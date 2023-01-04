#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#define LINEEND_WIN 1
#define LINEEND_LINUX 2
#define LINEEND_MAC 3
#define cpy_ending(dptr,ending,len) \
	{memcpy(dptr,ending,len);(dptr)+=(len);}
//r w+
int main(int argc,char** argv)
{
	if(argc<=1)
		return 0;
	int type=LINEEND_LINUX;
	int write=0;
	char* path=NULL;
	char* ending;
	int endinglen;
	for(int i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"-linux")==0)
			type=LINEEND_LINUX;
		else if(strcmp(argv[i],"-win")==0)
			type=LINEEND_WIN;
		else if(strcmp(argv[i],"-mac")==0)
			type=LINEEND_MAC;
		else if(strcmp(argv[i],"-w")==0)
			write=1;
		else if(path==NULL&&argv[i][0]!='-')
			path=argv[i];
		else
		{
			printf("\'%s\': %s\n",argv[i],argv[i][0]=='-'?"invalid option":"multiple paths specified");
			return -1;
		}
	}
	switch(type)
	{
	case LINEEND_WIN:
		ending="\r\n";
		break;
	case LINEEND_LINUX:
		ending="\n";
		break;
	case LINEEND_MAC:
		ending="\r";
		break;
	}
	endinglen=strlen(ending);
	struct stat size;
	if(0!=stat((const char*)path,&size))
	{
		printf("\'%s\': cannot stat file\n",path);
		return -1;
	}
	int ns=size.st_size;
	FILE* file=fopen(path,"r");
	if(file==NULL)
	{
		printf("\'%s\': cannot be open file\n",path);
		return -1;
	}
	char* buf=(char*)malloc(ns);
	if(fread(buf,1,ns,file)!=ns)
	{
		free(buf);
		fclose(file);
		printf("\'%s\': read error\n",path);
		return -1;
	}
	fclose(file);
	char* dbuf=(char*)malloc(2*ns);
	char *end=buf+ns,*ptr,*dptr;
	// "\r\n" "\n" "\r"
	int detected=0;
	for(ptr=buf,dptr=dbuf;ptr<end;ptr++)
	{
		switch(*ptr)
		{
		case '\r':
			if(ptr+1<end&&*(ptr+1)=='\n')
			{
				ptr++;
				if(type!=LINEEND_WIN)
					detected=1;
			}
			else if(type!=LINEEND_MAC)
				detected=1;
			cpy_ending(dptr,ending,endinglen);
			break;
		case '\n':
			if(type!=LINEEND_LINUX)
				detected=1;
			cpy_ending(dptr,ending,endinglen);
			break;
		default:
			*(dptr++)=*ptr;
		}
		if((!write)&&detected)
		{
			free(buf);
			free(dbuf);
			printf("\'%s\': need to normalize line endings\n",path);
			return 0;
		}
	}
	free(buf);
	if(!(write&&detected))
	{
		free(dbuf);
		return 0;
	}
	int dlen=dptr-dbuf;
	file=fopen(path,"w");
	if(file==NULL)
	{
		printf("\'%s\': cannot open file for writing\n",path);
		free(dbuf);
		return -1;
	}
	if(fwrite(dbuf,1,dlen,file)!=dlen)
	{
		free(dbuf);
		fclose(file);
		printf("\'%s\': write error\n",path);
		return -1;
	}
	free(dbuf);
	fclose(file);
	printf("\'%s\': line endings normalized\n",path);
	return 0;
}
