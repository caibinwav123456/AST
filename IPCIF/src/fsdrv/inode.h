#ifndef _INODE_H_
#define _INODE_H_
#include "algor_templ.h"
#include <string.h>
#include <string>
using namespace std;
struct INodeData
{
	bool lock;
	dword attr;
	void* priv;
	INodeData()
	{
		memset(this,0,sizeof(INodeData));
	}
};
typedef KeyTree<string,INodeData>::TreeNode INode,*pINode;
class INodeTree : public KeyTree<string,INodeData>
{
public:
	INodeTree():KeyTree<string,INodeData>("/")
	{get_t_ref().attr=FS_ATTR_FLAGS_DIR;}
	pINode GetINode(vector<string>& vKey);
	pINode GetINodeInTree(vector<string>& vKey);
	static void ReleaseNode(pINode node);
	static bool IsDir(pINode node);
	static bool IsLocked(pINode node);
	static bool LockDir(pINode node,bool lck);
protected:
	virtual pINode CteateNode(const vector<string>& path)=0;
private:
	static pINode GetNodeCallBack(const vector<string>& path,int index,void* param);
	static pINode GetNodeCallBackNull(const vector<string>& path,int index,void* param);
};
#endif