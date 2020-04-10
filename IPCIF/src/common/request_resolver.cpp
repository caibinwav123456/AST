#include "common.h"
#include "request_resolver.h"
void RequestResolver::AddHandler(RequestHandler handler)
{
	vhandler.push_back(handler);
}
bool RequestResolver::HandleRequest(uint cmd, void* addr, void* param, int op, bool process_all)
{
	bool ret=false;
	for(int i=0;i<(int)vhandler.size();i++)
	{
		if((ret=vhandler[i](cmd,addr,param,op))&&!process_all)
		{
			return ret;
		}
	}
	return ret;
}
