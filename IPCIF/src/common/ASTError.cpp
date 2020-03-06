#include "ASTError.h"
#include "utility.h"
static char* err_descs[] =
{
	"OK",
	"Unknown Error",
	"Interface has reset",
	"Interface setup failed",
	"Connecting to interface failed",
	"Requesting interface failed",
	"File input/output error",
	"Bad config file format",
	"Buffer overflow",
	"Executable not found",
	"Executable information not found",
	"Executable information not valid",
	"Current directory not found",
	"File path not valid",
	"Process already exists",
	"Invalid config value type",
	"Buffer size is not enough",
	"System is busy",
	"Timeout",
	"The desired module is not found",
	"The module is not initiated",
	"Invalid calling parameters",
	"The desired FS system access is denied",
	"FS handle invalid",
	"Trying to set a file pointer to a negative value",
	"File path does not exist",
	"File system device not found",
	"File system device mount failed",
	"File system device format failed",
	"File system device failed: invalid command",
	"File system device mount failed due to device not existing",
	"File system device mount failed due to device already mounted",
	"File system device format failed due to device already in-use",
};
DLLAPI(char*) get_error_desc(int errcode)
{
	if(_r(errcode)>=sizeof(err_descs)/sizeof(char*))
		errcode=ERR_GENERIC;
	return err_descs[_r(errcode)];
}
