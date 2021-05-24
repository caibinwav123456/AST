#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int get_file_type(const char *filename)
{
	struct stat statbuf;
	if(stat(filename,&statbuf)!=0)
		return -1;
	if(S_ISDIR(statbuf.st_mode))
		return 1;
	return 0;
}
int main(int argc,char **argv)
{
#ifdef _DIRENT_HAVE_D_TYPE
	printf("Have d_type.\n");
#else
	printf("Do not have d_type.\n");
#endif
	const char* file=NULL;
	bool use_stat=false;
	int type=0;
	for(int i=1;i<argc;i++)
	{
		if(strlen(argv[i])>0&&argv[i][0]=='-')
		{
			if(strcmp(argv[i],"-s")==0)
				use_stat=true;
		}
		else if(file==NULL)
			file=argv[i];
	}
	if(file==NULL)
		file="./";
	if((type=get_file_type(file))<0)
	{
		printf("Get %s info failed.\n",file);
		return -1;
	}
	if(type==0)
	{
		printf("%s is a regular file.\n",file);
		return 0;
	}
	DIR *dirp;
	struct dirent *direntp;
	if((dirp=opendir(file))==NULL)
	{
		printf("Open Directory %s Error:%s.\n",file,strerror(errno));
		return -1;
	}
	while((direntp=readdir(dirp))!=NULL)
	{
		printf("%s\n",direntp->d_name);
#ifdef _DIRENT_HAVE_D_TYPE
		if(use_stat)
		{
#endif
			char path[1024];
			strcpy(path,file);
			if(*path!=0&&path[strlen(path)-1]!='/')
				strcat(path,"/");
			strcat(path,direntp->d_name);
			if((type=get_file_type(path))<0)
				printf("\tGet %s info failed.\n",direntp->d_name);
#ifdef _DIRENT_HAVE_D_TYPE
		}
		else
		{
			type=(direntp->d_type==DT_DIR?1:0);
		}
#endif
		switch(type)
		{
		case 0:
			printf("\tnormal file\n");
			break;
		case 1:
			printf("\tdirectory\n");
			break;
		}
	}
	closedir(dirp);
	return 0;
}
