#include "pipe.h"
#include "utility.h"
#include "config_val_extern.h"
#define sem_safe_release(sem) \
	if(VALID(sem)) \
	{ \
		sys_close_sem(sem); \
		sem=NULL; \
	}
DEFINE_UINT_VAL(fssh_pipe_buffer_size,16);
Pipe::Pipe()
{
	if(fssh_pipe_buffer_size<16)
		fssh_pipe_buffer_size=16;
	pipe_buf=NULL;
	pipe_buf=new byte[fssh_pipe_buffer_size+1];
	semin=sys_create_sem(0,1,NULL);
	semout=sys_create_sem(0,1,NULL);
	if(!(pipe_buf!=NULL&&VALID(semin)&&VALID(semout)))
		goto fail;
	p_start=0;
	p_end=0;
	feed=0;
	return;
fail:
	if(pipe_buf!=NULL)
	{
		delete[] pipe_buf;
		pipe_buf=NULL;
	}
	sem_safe_release(semin);
	sem_safe_release(semout);
	EXCEPTION(ERR_SEM_CREATE_FAILED);
}
Pipe::~Pipe()
{
	if(pipe_buf!=NULL)
	{
		delete[] pipe_buf;
		pipe_buf=NULL;
	}
	sem_safe_release(semin);
	sem_safe_release(semout);
}
void Pipe::Send(void* ptr,uint len)
{
}
uint Pipe::Recv(void* ptr,uint len)
{
	return 0;
}