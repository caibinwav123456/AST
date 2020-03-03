#ifndef _INODE_H_
#define _INODE_H_
#include "algor_templ.h"
#include <string.h>
#include <string>
using namespace std;
struct INodeData
{
	bool lock;
	int refcnt;
	dword attr;
	INodeData()
	{
		memset(this,0,sizeof(INodeData));
	}
};
typedef KeyTree<string,INodeData>::TreeNode INode,*pINode;
class INodeTree : public KeyTree<string,INodeData>
{
public:
	INodeTree():KeyTree<string, INodeData>("/"){}
	pINode GetINode(vector<string>& vKey);
	static void AddRef(pINode node){node->t.refcnt++;}
	static void ReleaseNode(pINode node);
	static bool IsDir(pINode node);
	static bool IsLocked(pINode node);
	static bool LockDir(pINode node,bool lck);
protected:
	virtual pINode CteateNode(vector<string>& path)=0;
private:
	static pINode GetNodeCallBack(vector<string>& path,int index,void* param);
};
#endif