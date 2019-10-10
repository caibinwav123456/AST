#include "common_request.h"
#include "utility.h"
#include "import.h"
#include "interface.h"
#include "export.h"
#include <string.h>
DLL int cb_simple_req(void* addr, void* param, int op)
{
	datagram_base* data = (datagram_base*)param;
	datagram_base* buf = (datagram_base*)addr;
	if (op&OP_PARAM)
	{
		*buf = *data;
	}
	if (op&OP_RETURN)
	{
		data->ret = buf->ret;
	}
	return 0;
}
DLL int cb_multibyte_req(void* addr, void* param, int op)
{
	datagram_param* dgp = (datagram_param*)param;
	datagram_base* data = (datagram_base*)dgp->dg;
	if (op&OP_PARAM)
	{
		memcpy(addr, data, dgp->copyto);
	}
	if (op&OP_RETURN)
	{
		memcpy(data, addr, dgp->copyfrom);
	}
	return 0;
}
inline int __tmp_connect_if__(char* id, char* user, uint size, void** h)
{
	int ret=0;
	bool valid=true;
	if(!VALID(*h))
	{
		valid=false;
		if(id==NULL)
			return ERR_GENERIC;
		if_initial init;
		init.user=(user==NULL?get_if_user():user);
		init.id=id;
		init.nthread=0;
		init.smem_size=0;
		if(0!=(ret=connect_if(&init,h)))
			return ret;
	}
	uint memsize=0;
	if(0!=(ret=stat_if(*h,NULL,&memsize)))
		goto end;
	if(memsize<size)
	{
		ret=ERR_INSUFFICIENT_BUFSIZE;
		goto end;
	}
end:
	if(ret!=0&&!valid)
	{
		close_if(*h);
		*h=NULL;
	}
	return ret;
}
inline int send_request_no_reset(void* h, if_callback cb, void* param)
{
	int ret=0;
	int count=0;
	while(ERR_IF_RESET==(ret=request_if(h,cb,param)))
	{
		count++;
		if(count>=10)
			break;
		sys_sleep(100);
	}
	return ret;
}
DLLAPI(int) send_simple_request(uint cmd, void* hif, char* id, char* user)
{
	int ret=0;
	void* h=hif;
	if(0!=(ret=__tmp_connect_if__(id, user, sizeof(datagram_base), &h)))
		return ret;
	datagram_base data;
	init_current_datagram_base(&data,cmd);
	if(0!=(ret=send_request_no_reset(h, cb_simple_req, &data)))
		goto end;
	ret=data.ret;
end:
	if(hif==NULL)
		close_if(h);
	return ret;
}
DLLAPI(int) send_cmd_exit(void* hif, char* id, char* user)
{
	return send_simple_request(CMD_EXIT,hif,id,user);
}
DLLAPI(int) send_cmd_clear(void* pid, void* hif, char* id, char* user)
{
	int ret=0;
	void* h=hif;
	if(0!=(ret=__tmp_connect_if__(id, user, sizeof(dg_clear), &h)))
		return ret;
	dg_clear dc;
	init_current_datagram_base(&dc.header,CMD_CLEAR);
	dc.clear.id=pid;
	datagram_param p;
	p.dg=&dc;
	p.copyto=sizeof(dg_clear);
	p.copyfrom=sizeof(datagram_base);
	if(0!=(ret=send_request_no_reset(h, cb_multibyte_req, &p)))
		goto end;
	ret=dc.header.ret;
end:
	if(hif==NULL)
		close_if(h);
	return ret;
}
DLLAPI(int) send_cmd_clear_all(void* hif, char* id, char* user)
{
	return send_simple_request(CMD_CLEAR_ALL,hif,id,user);
}
DLLAPI(int) send_cmd_suspend(int bsusp, void* hif, char* id, char* user)
{
	int ret=0;
	void* h=hif;
	if(0!=(ret=__tmp_connect_if__(id, user, sizeof(dg_suspend), &h)))
		return ret;
	dg_suspend ds;
	init_current_datagram_base(&ds.header,CMD_SUSPEND);
	ds.susp.bsusp=bsusp;
	datagram_param p;
	p.dg=&ds;
	p.copyto=sizeof(dg_suspend);
	p.copyfrom=sizeof(datagram_base);
	if(0!=(ret=send_request_no_reset(h, cb_multibyte_req, &p)))
		goto end;
	ret=ds.header.ret;
end:
	if(hif==NULL)
		close_if(h);
	return ret;
}
