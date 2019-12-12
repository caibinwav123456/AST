#ifndef _REQUEST_RESOLVER_H_
#define _REQUEST_RESOLVER_H_
#include "common.h"
#include <vector>
using namespace std;
typedef bool (*RequestHandler)(uint cmd, void* addr, void* param, int op);
class DLL RequestResolver
{
public:
	void AddHandler(RequestHandler handler);
	bool HandleRequest(uint cmd, void* addr, void* param, int op);
private:
	vector<RequestHandler> vhandler;
};
#endif