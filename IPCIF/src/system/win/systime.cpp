#include "common.h"
#include "syswin.h"

void sys_get_date_time(DateTime* time)
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	time->year=(short)t.wYear;
	time->month=(byte)(t.wMonth-1);
	time->day=(byte)(t.wDay-1);
	time->weekday=(byte)(t.wDayOfWeek==0?6:t.wDayOfWeek-1);
	time->hour=(byte)t.wHour;
	time->minute=(byte)t.wMinute;
	time->second=(byte)t.wSecond;
	time->millisecond=(ushort)t.wMilliseconds;
}