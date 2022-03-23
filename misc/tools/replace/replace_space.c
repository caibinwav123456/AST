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
		printf("file \"%s\" cannot be opened for read\n",path);
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
	int i,j;
	int det=0;
	for(i=0,j=0;i<ns;i++)
	{
		if(buf[i]!=' ')
		{
			if(det>0)
			{
				buf[j++]=' ';
				det=0;
			}
			buf[j++]=buf[i];
		}
		else
		{
			if(i==ns-1)
				break;
			det++;
			if(det==4)
			{
				buf[j++]='\t';
				det=0;
			}
		}
	}
	file=fopen(path,"w");
	if(file==NULL)
	{
		printf("file \"%s\" cannot be opened for write\n",path);
		free(buf);
		return -1;
	}
	if(fwrite(buf,1,j,file)!=j)
	{
		free(buf);
		fclose(file);
		printf("file \"%s\" write error\n",path);
		return -1;
	}
	free(buf);
	fclose(file);
	return 0;
}
