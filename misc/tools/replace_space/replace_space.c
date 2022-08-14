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
	int write=0;
	if(argc>2&&strcmp(argv[2],"-w")==0)
		write=1;
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
	int det=0;
	int opened=1;
	char *src,*dst,*end=buf+ns;
	for(src=dst=buf;src<end;src++)
	{
		if(*src=='\r'||*src=='\n')
		{
			*(dst++)=*src;
			if(*src=='\r'&&src+1<end&&*(src+1)=='\n')
				*(dst++)=*(++src);
			opened=1;
			det=0;
		}
		else if(*src!=' '||!opened)
		{
			opened=0;
			det=0;
			*(dst++)=*src;
		}
		else
		{
			det++;
			if(det==4)
			{
				*(dst++)='\t';
				det=0;
			}
		}
	}
	if(dst==end)
	{
		free(buf);
		return 0;
	}
	else if(!write)
	{
		free(buf);
		printf("file \"%s\" needs to be patched.\n",path);
		return 0;
	}
	int dstlen=dst-buf;
	file=fopen(path,"w");
	if(file==NULL)
	{
		printf("file \"%s\" cannot be opened for write\n",path);
		free(buf);
		return -1;
	}
	if(fwrite(buf,1,dstlen,file)!=dstlen)
	{
		free(buf);
		fclose(file);
		printf("file \"%s\" write error\n",path);
		return -1;
	}
	free(buf);
	fclose(file);
	printf("file \"%s\" patched.\n",path);
	return 0;
}
