#include "inode.h"
#include <assert.h>
pINode INodeTree::GetNodeCallBack(vector<string>& path, int index, void* param)
{
	INodeTree* tree=(INodeTree*)param;
	vector<string> sub;
	for(int i=0;i<=index;i++)
		sub.push_back(path[i]);
	return tree->CteateNode(sub);
}
pINode INodeTree::GetINode(vector<string>& vKey)
{
	return GetNode(vKey,GetNodeCallBack,this);
}
void INodeTree::ReleaseNode(pINode node)
{
	if(node->t.refcnt>0)
		node->t.refcnt--;
	if(node->t.refcnt==0)
	{
		if(!node->NoChild()||IsLocked(node))
			return;
		pINode parent=node->GetParent();
		while(parent!=NULL)
		{
			verify(node->Detach());
			delete node;
			if((!parent->NoChild())||IsLocked(parent))
				break;
			node=parent;
			parent=node->GetParent();
		}
	}
}
bool INodeTree::IsDir(pINode node)
{
	return FS_IS_DIR(node->t.attr);
}
bool INodeTree::IsLocked(pINode node)
{
	return node->t.lock;
}
bool INodeTree::LockDir(pINode node,bool lck)
{
	assert(FS_IS_DIR(node->t.attr));
	if(!FS_IS_DIR(node->t.attr))
		return false;
	node->t.lock=lck;
	return true;
}
