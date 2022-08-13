#ifndef _INODE_H_
#define _INODE_H_
#include "algor_templ.h"
#include "path.h"
#include <string.h>
#include <string>
#define begin_insert_pull(insert_cache,pull,insert_before,start_it,end_it,start,end) \
	path_cache::iterator start_it=start,end_it=end; \
	{ \
		--end_it; \
		insert_cache.insert(insert_before,start,end); \
		++end_it; \
	}
#define end_insert_pull(pull,end,start_it,end_it) pull.insert(end,start_it,end_it)
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
typedef KeyTree<string,INodeData,path_cache>::TreeNode INode,*pINode;
class INodeTree : public KeyTree<string,INodeData,path_cache>
{
public:
	INodeTree():KeyTree<string,INodeData,path_cache>("/")
	{get_t_ref().attr=FS_ATTR_FLAGS_DIR;}
	pINode GetINode(path_cache& vKey);
	pINode GetINodeInTree(path_cache& vKey);
	static void ReleaseNode(pINode node);
	static bool IsDir(pINode node);
	static bool IsLocked(pINode node);
	static bool LockDir(pINode node,bool lck);
protected:
	virtual pINode CreateNode(path_cache& path)=0;
private:
	static pINode GetNodeCallBack(path_cache& path,path_cache::iterator& it,void* param);
	static pINode GetNodeCallBackNull(path_cache& path,path_cache::iterator& it,void* param);
};
#endif