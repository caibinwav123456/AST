#ifndef _CMD_STRUCT_H_
#define _CMD_STRUCT_H_
#include "common.h"
#define MAX_FILE_PATH 1024
#define MAX_FILE_NAME 256
#define _1K 1024
#define _1M (1024*1024)
enum if_cmd_code
{
	//cmd
	CMD_NULL = 0,
	CMD_EXIT,
	CMD_CLEAR,
	CMD_CLEAR_ALL,
	CMD_SUSPEND,
	CMD_GETID,
	CMD_STATUS,

	//storage
	CMD_FSOPEN,
	CMD_FSCLOSE,
	CMD_FSSEEK,
	CMD_FSREAD,
	CMD_FSWRITE,
	CMD_FSGETSIZE,
	CMD_FSSETSIZE,
	CMD_MAKEDIR,
	CMD_LSFILES,
	CMD_FSMOVE,
	CMD_FSDELETE,
	CMD_FSGETATTR,
	CMD_FSSETATTR,
};

#pragma pack(push,1)

struct datagram_base
{
	uint cmd;
	int ret;
	void* caller;
	dword sid;
};

struct dgc_clear
{
	void* id;
};

struct dg_clear
{
	datagram_base header;
	dgc_clear clear;
};

struct dgc_suspend
{
	int bsusp;
};

struct dg_suspend
{
	datagram_base header;
	dgc_suspend susp;
};

struct dgc_getid
{
	uint id;
};

struct dg_getid
{
	datagram_base header;
	dgc_getid retid;
};

struct dgc_fsopen
{
	dword flags;
	void* hFile;
	char path[MAX_FILE_PATH];
};

struct dg_fsopen
{
	datagram_base header;
	dgc_fsopen open;
};

struct dgc_fsclose
{
	void* handle;
};

struct dg_fsclose
{
	datagram_base header;
	dgc_fsclose close;
};

struct dgc_fsrdwr
{
	void* handle;
	uint offset;
	uint offhigh;
	uint len;
	byte buf[_1K*4];
};

struct dg_fsrdwr
{
	datagram_base header;
	dgc_fsrdwr rdwr;
};

struct dgc_fssize
{
	void* handle;
	uint len;
	uint lenhigh;
};

struct dg_fssize
{
	datagram_base header;
	dgc_fssize size;
};

struct dgc_fsmove
{
	char src[MAX_FILE_PATH];
	char dst[MAX_FILE_PATH];
};

struct dg_fsmove
{
	datagram_base header;
	dgc_fsmove move;
};

struct dgc_fsdel
{
	char path[MAX_FILE_PATH];
};

struct dg_fsdel
{
	datagram_base header;
	dgc_fsdel del;
};

struct DateTimeWrap
{
	DateTime date;
	short pad;
};

struct dgc_fsattr
{
	dword mask;
	DateTimeWrap date[3];
	dword flags;
	char path[MAX_FILE_PATH];
};

struct dg_fsattr
{
	datagram_base header;
	dgc_fsattr attr;
};

struct dgc_fsmkdir
{
	char path[MAX_FILE_PATH];
};

struct dg_fsmkdir
{
	datagram_base header;
	dgc_fsmkdir dir;
};

struct dgc_fslsfiles
{
	uint nfiles;
	void* handle;
	char path[MAX_FILE_PATH];
	char file[12][MAX_FILE_NAME];
};

struct dg_fslsfiles
{
	datagram_base header;
	dgc_fslsfiles files;
};

//ASTManager interface
struct dg_manager
{
	datagram_base header;
	union
	{
		dgc_clear dc;
	};
};

#pragma pack(pop)

#endif
