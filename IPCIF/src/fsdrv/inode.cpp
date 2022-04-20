#include "inode.h"
#include <assert.h>
pINode INodeTree::GetNodeCallBack(path_cache& path,path_cache::iterator& it,void* param)
{
	INodeTree* tree=(INodeTree*)param;
	path_cache sub;
	begin_insert_pull(sub,path,sub.end(),__start,__end,path.begin(),it);
	pINode node=tree->CteateNode(sub);
	end_insert_pull(path,path.end(),__start,__end);
	return node;
}
pINode INodeTree::GetNodeCallBackNull(path_cache& path,path_cache::iterator& it,void* param)
{
	return NULL;
}
pINode INodeTree::GetINode(path_cache& vKey)
{
	return GetNode(vKey,GetNodeCallBack,this);
}
pINode INodeTree::GetINodeInTree(path_cache& vKey)
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
