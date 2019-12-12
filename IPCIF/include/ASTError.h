#ifndef _AST_ERROR_H_
#define _AST_ERROR_H_
#include "common.h"

//#define USE_POSITIVE_ERROR_VALUE
#ifdef USE_POSITIVE_ERROR_VALUE
#define _r(x) (x)
#else
#define _r(x) (-(x))
#endif

#define ERR_NONE                    0
#define ERR_GENERIC                 _r(1)
#define ERR_IF_RESET                _r(2)
#define ERR_IF_SETUP_FAILED         _r(3)
#define ERR_IF_CONN_FAILED          _r(4)
#define ERR_IF_REQUEST_FAILED       _r(5)
#define ERR_FILE_IO                 _r(6)
#define ERR_BAD_CONFIG_FORMAT       _r(7)
#define ERR_BUFFER_OVERFLOW         _r(8)
#define ERR_EXEC_NOT_FOUND          _r(9)
#define ERR_EXEC_INFO_NOT_FOUND     _r(10)
#define ERR_EXEC_INFO_NOT_VALID     _r(11)
#define ERR_CUR_DIR_NOT_FOUND       _r(12)
#define ERR_INVALID_PATH            _r(13)
#define ERR_DUP_PROCESS             _r(14)
#define ERR_INVALID_TYPE            _r(15)
#define ERR_INSUFFICIENT_BUFSIZE    _r(16)
#define ERR_BUSY                    _r(17)
#define ERR_TIMEOUT                 _r(18)
#define ERR_MODULE_NOT_FOUND        _r(19)
#define ERR_MODULE_NOT_INITED       _r(20)

#ifdef __cplusplus
extern "C" {
#endif
DLLAPI(char*) get_error_desc(int errcode);
#ifdef __cplusplus
}
#endif

#endif
