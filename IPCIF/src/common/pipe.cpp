#include "pipe.h"
#include "utility.h"
#include "config_val_extern.h"
#include <string.h>
#include <assert.h>
#define MIN_PIPE_BUFFER_SIZE 16
#define sem_safe_release(sem) \
	if(VALID(sem)) \
	{ \
		sys_close_sem(sem); \
		sem=NULL; \
	}
#define content_len (p_end>=p_start? \
	p_end-p_start:p_end+pipe_buffer_size+1-p_start)
DEFINE_SIZE_VAL(pipe_buffer_size,MIN_PIPE_BUFFER_SIZE);
Pipe::Pipe()
{
	if(pipe_buffer_size<MIN_PIPE_BUFFER_SIZE)
		pipe_buffer_size=MIN_PIPE_BUFFER_SIZE;
	semin=sys_create_sem(1,1,NULL);
	semout=sys_create_sem(0,1,NULL);
	pipe_buf=NULL;
	pipe_buf=new byte[pipe_buffer_size+1];
	if(!(pipe_buf!=NULL&&VALID(semin)&&VALID(semout)))
		goto fail;
	p_start=0;
	p_end=0;
	copyleft=false;
	cps=false;
	eof=false;
	return;
fail:
	sem_safe_release(semin);
	sem_safe_release(semout);
	if(pipe_buf!=NULL)
	{
		delete[] pipe_buf;
		pipe_buf=NULL;
	}
	EXCEPTION(ERR_SEM_CREATE_FAILED);
}
Pipe::~Pipe()
{
	sem_safe_release(semin);
	sem_safe_release(semout);
	if(pipe_buf!=NULL)
	{
		delete[] pipe_buf;
		pipe_buf=NULL;
	}
}
static inline void pipe_write_buf(byte* bptr,uint len,byte* buf,uint& start,uint& end)
{
	uint nend=(end+len)%(pipe_buffer_size+1);
	if(nend>=end)
		memcpy(buf+end,bptr,len);
	else
	{
		uint len1=pipe_buffer_size+1-end;
		uint len2=nend;
		memcpy(buf+end,bptr,len1);
		memcpy(buf,bptr+len1,len2);
	}
	end=nend;
}
static inline void pipe_read_buf(byte* bptr,uint len,byte* buf,uint& start,uint& end)
{
	uint nstart=(start+len)%(pipe_buffer_size+1);
	if(nstart>=start)
		memcpy(bptr,buf+start,len);
	else
	{
		uint len1=pipe_buffer_size+1-start;
		uint len2=nstart;
		memcpy(bptr,buf+start,len1);
		memcpy(bptr+len1,buf,len2);
	}
	start=nstart;
}
#define eof_quit \
	if(eof) \
	{ \
		sys_signal_sem(semin); \
		return 0; \
	}
uint Pipe::Send(const void* ptr,uint len)
{
	if(len==0)
	{
		if(cps)
		{
			cps=false;
			sys_signal_sem(semout);
		}
		sys_wait_sem(semin);
		eof_quit;
		sys_signal_sem(semout);
		return 0;
	}
	byte* bptr;
	uint left,copy,clenleft;
	for(bptr=(byte*)ptr,left=len,copy=0;left>0;bptr+=copy,left-=copy)
	{
		if(!cps)
			sys_wait_sem(semin);
		assert(!(eof&&cps));
		eof_quit;
		clenleft=pipe_buffer_size-content_len;
		cps=(left<clenleft);
		copy=(cps?left:clenleft);
		if(copy>0)
			pipe_write_buf(bptr,copy,pipe_buf,p_start,p_end);
		if(!cps)
			sys_signal_sem(semout);
	}
	return len;
}
uint Pipe::Recv(void* ptr,uint len)
{
	uint clen,left,copy,lrecv;
	byte* bptr;
	if(eof)
		return 0;
	for(bptr=(byte*)ptr,clen=0,left=len,copy=0,lrecv=0;
		left>0;bptr+=copy,left-=copy,lrecv+=copy)
	{
		if(!copyleft)
			sys_wait_sem(semout);
		clen=content_len;
		copyleft=(left<clen);
		if(clen>0)
		{
			copy=(copyleft?left:clen);
			pipe_read_buf(bptr,copy,pipe_buf,p_start,p_end);
		}
		if(clen==0)
			eof=true;
		if(!copyleft)
			sys_signal_sem(semin);
		if(clen==0)
			break;
	}
	return lrecv;
}