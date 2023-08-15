#include "common.h"
#include "syswin.h"
#include <shlwapi.h>

void SystemTimeToDateTime(SYSTEMTIME* t,DateTime* time)
{
	time->year=(short)t->wYear;
	time->month=(byte)(t->wMonth-1);
	time->day=(byte)(t->wDay-1);
	time->weekday=(byte)(t->wDayOfWeek==0?6:t->wDayOfWeek-1);
	time->hour=(byte)t->wHour;
	time->minute=(byte)t->wMinute;
	time->second=(byte)t->wSecond;
	time->millisecond=(ushort)t->wMilliseconds;
}
void DateTimeToSystemTime(DateTime* time,SYSTEMTIME* t)
{
	t->wYear=(WORD)(time->year);
	t->wMonth=(WORD)(time->month+1);
	t->wDay=(WORD)(time->day+1);
	t->wDayOfWeek=(WORD)(time->weekday==6?0:time->weekday+1);
	t->wHour=(WORD)time->hour;
	t->wMinute=(WORD)time->minute;
	t->wSecond=(WORD)time->second;
	t->wMilliseconds=(WORD)time->millisecond;
}
void sys_get_date_time(DateTime* time)
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	SystemTimeToDateTime(&t,time);
}
inline void init_default_date(DateTime* date)
{
	memset(date,0,sizeof(DateTime));
	date->year=2000;
	date->weekday=5;
}
int sys_get_file_time(char* path,DateTime* creation_time,DateTime* modify_time,DateTime* access_time)
{
	if((!PathFileExistsA(path)))
		return ERR_PATH_NOT_EXIST;
	if(PathIsDirectoryA(path))
	{
		if(creation_time!=NULL)
			init_default_date(creation_time);
		if(modify_time!=NULL)
			init_default_date(modify_time);
		if(access_time!=NULL)
			init_default_date(access_time);
		return 0;
	}
	HANDLE hFile=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,0);
	if(!VALID(hFile))
		return ERR_OPEN_FILE_FAILED;
	FILETIME tCreation,tModify,tAccess;
	if(!GetFileTime(hFile,
		creation_time!=NULL?&tCreation:NULL,
		access_time!=NULL?&tAccess:NULL,
		modify_time!=NULL?&tModify:NULL))
	{
		CloseHandle(hFile);
		return ERR_FILE_IO;
	}
	CloseHandle(hFile);
	FILETIME* tf[3]={&tCreation,&tModify,&tAccess};
	DateTime* dt[3]={creation_time,modify_time,access_time};
	for(int i=0;i<3;i++)
	{
		if(dt[i]!=NULL)
		{
			FILETIME lft;
			SYSTEMTIME syst;
			FileTimeToLocalFileTime(tf[i],&lft);
			FileTimeToSystemTime(&lft,&syst);
			SystemTimeToDateTime(&syst,dt[i]);
		}
	}
	return 0;
}
int sys_set_file_time(char* path,DateTime* creation_time,DateTime* modify_time,DateTime* access_time)
{
	if(!PathFileExistsA(path))
		return ERR_PATH_NOT_EXIST;
	if(PathIsDirectoryA(path))
		return 0;
	HANDLE hFile=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,0);
	if(!VALID(hFile))
		return ERR_OPEN_FILE_FAILED;
	FILETIME tCreation,tModify,tAccess;
	FILETIME* tf[3]={&tCreation,&tModify,&tAccess};
	DateTime* dt[3]={creation_time,modify_time,access_time};
	for(int i=0;i<3;i++)
	{
		if(dt[i]!=NULL)
		{
			FILETIME lft;
			SYSTEMTIME syst;
			DateTimeToSystemTime(dt[i],&syst);
			SystemTimeToFileTime(&syst,&lft);
			LocalFileTimeToFileTime(&lft,tf[i]);
		}
	}
	if(!SetFileTime(hFile,
		creation_time!=NULL?&tCreation:NULL,
		access_time!=NULL?&tAccess:NULL,
		modify_time!=NULL?&tModify:NULL))
	{
		CloseHandle(hFile);
		return ERR_FILE_IO;
	}
	CloseHandle(hFile);
	return 0;
}