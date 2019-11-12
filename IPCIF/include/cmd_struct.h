#ifndef _CMD_STRUCT_H_
#define _CMD_STRUCT_H_
#include "common.h"

enum if_cmd_code
{
	CMD_NULL = 0,
	CMD_EXIT,
	CMD_CLEAR,
	CMD_CLEAR_ALL,
	CMD_SUSPEND,
	CMD_GETID,
	CMD_STATUS,
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

#pragma pack(pop)

#endif
