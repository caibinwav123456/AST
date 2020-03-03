#define DLL_IMPORT
#include "native_fs.h"
#include "inode.h"
#include "path.h"
#include "utility.h"
class NativeFsTree : public INodeTree
{
public:
	int Init(string base);
protected:
	virtual pINode CteateNode(vector<string>& path);
private:
	vector<string> base_path;
};
int NativeFsTree::Init(string base)
{
	vector<string> merge,relative;
	if(sys_is_absolute_path((char*)base.c_str(),'/'))
	{
		split_path(base,merge,'/');
	}
	else
	{
		string exepath=get_current_executable_path();
		split_path(exepath,merge);
		split_path(base,relative,'/');
		merge.insert(merge.end(),relative.begin(),relative.end());
	}
	int ret=0;
	if(0!=(ret=get_direct_path(base_path,merge)))
		return ret;
	return 0;
}
pINode NativeFsTree::CteateNode(vector<string>& path)
{
	vector<string> merge=base_path;
	merge.insert(merge.end(),path.begin(),path.end());
	string strmerge;
	merge_path(strmerge,merge);
	dword type=0;
	int ret=sys_fstat((char*)strmerge.c_str(),&type);
	if(ret!=0)
		return NULL;
	pINode node=new INode(path.back());
	assert(!path.empty());
	if(path.empty())
		return NULL;
	node->t.attr=(type==FILE_TYPE_DIR?FS_ATTR_FLAGS_DIR:0);
	return node;
}
int fs_native_init()
{
	return 0;
}
void fs_native_uninit()
{

}
int fs_native_format(char* cmd)
{
	return 0;
}
int fs_native_mount(char* cmd,void** hdev)
{
	return 0;
}
void fs_native_unmount(void* hdev)
{

}
void* fs_native_open(void* hdev,char* pathname,dword flags)
{
	return NULL;
}
void fs_native_close(void* hdev,void* hfile)
{

}
int fs_native_read(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen)
{
	return 0;
}
int fs_native_write(void* hdev,void* hfile,uint offset,uint offhigh,uint len,void* buf,uint* rdlen)
{
	return 0;
}
int fs_native_getsize(void* hdev,void* hfile,uint* size,uint* sizehigh)
{
	return 0;
}
int fs_native_setsize(void* hdev,void* hfile,uint size,uint sizehigh)
{
	return 0;
}
int fs_native_move(void* hdev,char* src,char* dst)
{
	return 0;
}
int fs_native_del(void* hdev,char* path)
{
	return 0;
}
int fs_native_mkdir(void* hdev,char* path)
{
	return 0;
}
int fs_native_getattr(void* hdev,char* path,dword* attrflags)
{
	return 0;
}
int fs_native_setattr(void* hdev,char* path,dword attrflags)
{
	return 0;
}
int fs_native_getfiletime(void* hdev,char* path,dword mask,DateTime* date)
{
	return 0;
}
int fs_native_setfiletime(void* hdev,char* path,dword mask,DateTime* date)
{
	return 0;
}
int fs_native_listfiles(void* hdev,char* path,vector<fsls_element>* files)
{
	return 0;
}
