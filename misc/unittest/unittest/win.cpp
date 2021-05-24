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