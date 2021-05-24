#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char** argv)
{
	if(argc<=1)
		return 0;
	char* path=argv[1];
	int bfinddualspace=0;
	if(argc>2&&strcmp(argv[2],"-u")==0)
		bfinddualspace=1;
	struct stat size;
	if(0!=stat((const char*)path,&size))
	{
		printf("file \"%s\" cannot be stat\n",path);
		return -1;
	}
	int ns=size.st_size;
	FILE* file=fopen(path,"r");
	if(file==NULL)
	{
		printf("file \"%s\" cannot be opened\n",path);
		return -1;
	}
	char* buf=(char*)malloc(ns);
	if(fread(buf,1,ns,file)!=ns)
	{
		free(buf);
		fclose(file);
		printf("file \"%s\" read error\n",path);
		return -1;
	}
	fclose(file);
	int line=1;
	int oldline=0;
	for(int i=0;i<ns-1;i++)
	{
		if((buf[i]=='\t'||buf[i]==' ')&&(buf[i+1]=='\r'||buf[i+1]=='\n'))
		{
			printf("file \"%s\" fault at line %d\n",path,line);
		}
		if(bfinddualspace&&oldline!=line&&(buf[i]==' '&&buf[i+1]==' '))
		{
			printf("file \"%s\" space fault at line %d\n",path,line);
			oldline=line;
		}
		if(buf[i]=='\n')
			line++;
	}
	if(ns>0&&(buf[ns-1]=='\t'||buf[ns-1]==' '))
	{
		printf("file \"%s\" fault at line %d\n",path,line);
	}
	free(buf);
	return 0;
}
