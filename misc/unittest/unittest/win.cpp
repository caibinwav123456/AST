#include "win.h"
#include <Windows.h>
void set_last_error(dword err)
{
	SetLastError(err);
}
dword get_last_error()
{
	return GetLastError();
}
double get_tick_freq()
{
	__int64 freq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	return (double)freq;
}
double get_tick_hq()
{
	__int64 tick;
	QueryPerformanceCounter((LARGE_INTEGER*)&tick);
	return (double)tick;
}
