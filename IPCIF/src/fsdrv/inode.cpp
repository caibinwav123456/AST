#include "inode.h"
#include <assert.h>
pINode INodeTree::GetNodeCallBack(const vector<string>& path, int index, void* param)
{
	INodeTree* tree=(INodeTree*)param;
	vector<string> sub;
	for(int i=0;i<=index;i++)
		sub.push_back(path[i]);
	return tree->CteateNode(sub);
}
pINode INodeTree::GetNodeCallBackNull(const vector<string>& path,int index,void* param)
{
	return NULL;
}
pINode INodeTree::GetINode(vector<string>& vKey)
{
	return GetNode(vKey,GetNodeCallBack,this);
}
pINode INodeTree::GetINodeInTree(vector<string>& vKey)
{
	return GetNode(vKey,GetNodeCallBackNull,NULL);
}
void INodeTree::ReleaseNode(pINode node)
{
	if((!node->NoChild())||IsLocked(node))
		return;
	pINode parent;
	while((parent=node->GetParent())!=NULL)
	{
		verify(node->Detach());
		delete node;
		node=parent;
		if((!node->NoChild())||IsLocked(node))
			break;
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
