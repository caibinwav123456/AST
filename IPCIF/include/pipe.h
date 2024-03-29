#ifndef _PIPE_H_
#define _PIPE_H_
#include "common.h"
class DLL Pipe
{
public:
	Pipe();
	~Pipe();
	uint Send(const void* ptr,uint len);
	uint Recv(void* ptr,uint len);
private:
	void* semin;
	void* semout;
	byte* pipe_buf;
	uint p_start;
	uint p_end;
	bool copyleft;
	bool cps;
	bool eof;
};
#endif
