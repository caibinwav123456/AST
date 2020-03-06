#define DLL_IMPORT
#include "native_fs.h"
#include "inode.h"
#include "path.h"
#include "utility.h"
#include "cmd_parse.h"
#define NATIVEFS_MOUNT_CMD "mount native_fs"
#define NATIVEFS_FORMAT_CMD "format native_fs"
#define NATIVEFS_BASE_PATH "base"
#define LOCKFILE_SUFFIX ".lck"
#define fsrec(ptr) ((NativeFsRec*)(ptr))
#define decl_rec(rec,ptr) NativeFsRec* rec=fsrec(ptr)
#define decl_dev(dev,ptr) NativeFsDev* dev=(NativeFsDev*)ptr
#define nt_path(fullpath,dev,path) string fullpath=dev->GetFullPath(path)
#define nt_rec(node) ((NativeFsRec*)(node->t.priv))
class NativeFsTree : public INodeTree
{
public:
	int Init(const string& base);
	string GetBase(char dsym=_dir_symbol)
	{
		string ret;
		merge_path(ret,base_path,dsym);
		return ret;
	}
	string GetFullPath(const string& path);
protected:
	virtual pINode CteateNode(const vector<string>& path);
private:
	vector<string> base_path;
};
class NativeFsDev
{
public:
	int Init(string cmd);
	void Exit();
	int PrepareBase(const string& base);
	static int Format(string cmd);
	string GetFullPath(const string& path)
	{
		return fs_tree.GetFullPath(path);
	}
	pINode GetINode(vector<string>& vKey)
	{
		return fs_tree.GetINode(vKey);
	}
	pINode GetINodeInTree(vector<string>& vKey)
	{
		return fs_tree.GetINodeInTree(vKey);
	}
private:
	static int parse_cmd_head(const char* head,const string& cmd,string& options);
	NativeFsTree fs_tree;
};
struct NativeFsRec
{
	NativeFsRec(NativeFsDev* pDev):host(pDev),node(NULL),fd(NULL),refcnt(0){}
	void AddRef(){refcnt++;}
	void Release();
	NativeFsDev* host;
	pINode node;
	void* fd;
	int refcnt;
};
void NativeFsRec::Release()
{
	refcnt--;
	if(refcnt<=0)
	{
		sys_fclose(fd);
		INodeTree::ReleaseNode(node);
		delete this;
	}
}
static int __get_full_path(const string& base,vector<string>& vbase)
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
	if(0!=(ret=get_direct_path(vbase,merge)))
		return ret;
	return 0;
}
int NativeFsTree::Init(const string& base)
{
	return __get_full_path(base,base_path);
}
pINode NativeFsTree::CteateNode(const vector<string>& path)
{
	vector<string> merge=base_path;
	merge.insert(merge.end(),path.begin(),path.end());
	string strmerge;
	merge_path(strmerge,merge);
	dword type=0;
	int ret=sys_fstat((char*)strmerge.c_str(),&type);
	if(ret!=0)
		return NULL;
	assert(!path.empty());
	if(path.empty())
		return NULL;
	pINode node=new INode(path.back());
	node->t.attr=(type==FILE_TYPE_DIR?FS_ATTR_FLAGS_DIR:0);
	return node;
}
string NativeFsTree::GetFullPath(const string& path)
{
	vector<string> split,base=base_path;
	split_path(path,split,'/');
	base.insert(base.end(),split.begin(),split.end());
	string ret;
	merge_path(ret,base);
	return ret;
}
int NativeFsDev::parse_cmd_head(const char* head,const string& cmd,string& options)
{
	string cmd_head(head);
	if(cmd.size()<cmd_head.size()||
		cmd.substr(0,cmd_head.size())!=cmd_head)
		return ERR_FS_DEV_FAILED_INVALID_CMD;
	options=cmd.substr(cmd_head.size(),cmd.size()-cmd_head.size());
	return 0;
}
int NativeFsDev::Init(string cmd)
{
	int ret=0;
	string options;
	if(0!=(ret=parse_cmd_head(NATIVEFS_MOUNT_CMD,cmd,options)))
		return ret;
	map<string,string> cmd_options;
	if(0!=(ret=parse_cmd((const byte*)options.c_str(),options.size(),cmd_options)))
		return ret;
	if(cmd_options.find(NATIVEFS_BASE_PATH)==cmd_options.end())
		return ERR_FS_DEV_FAILED_INVALID_CMD;
	if(0!=(ret=fs_tree.Init(cmd_options[NATIVEFS_BASE_PATH])))
		return ret;
	if(0!=(ret=PrepareBase(fs_tree.GetBase())))
		return ret;
	return 0;
}
int NativeFsDev::PrepareBase(const string& base)
{
	dword type=0;
	int ret=sys_fstat((char*)base.c_str(),&type);
	if(ret!=0||type!=FILE_TYPE_DIR)
		return ERR_FS_DEV_MOUNT_FAILED_NOT_EXIST;
	string lockfile=base+LOCKFILE_SUFFIX;
	void* h=sys_fopen((char*)lockfile.c_str(),FILE_CREATE_NEW);
	if(!VALID(h))
		return ERR_FS_DEV_MOUNT_FAILED_ALREADY_MOUNTED;
	sys_fclose(h);
	return 0;
}
void NativeFsDev::Exit()
{
	string lockfile=fs_tree.GetBase()+LOCKFILE_SUFFIX;
	sys_fdelete((char*)lockfile.c_str());
}
int NativeFsDev::Format(string cmd)
{
	int ret=0;
	string options;
	if(0!=(ret=parse_cmd_head(NATIVEFS_FORMAT_CMD,cmd,options)))
		return ret;
	map<string,string> cmd_options;
	if(0!=(ret=parse_cmd((const byte*)options.c_str(),options.size(),cmd_options)))
		return ret;
	if(cmd_options.find(NATIVEFS_BASE_PATH)==cmd_options.end())
		return ERR_FS_DEV_FAILED_INVALID_CMD;
	vector<string> vbase;
	string fullpath;
	__get_full_path(cmd_options[NATIVEFS_BASE_PATH],vbase);
	merge_path(fullpath,vbase);
	string lockfile=fullpath+LOCKFILE_SUFFIX;
	dword type=0;
	if(0==sys_fstat((char*)lockfile.c_str(),&type))
		return ERR_FS_DEV_FORMAT_FAILED_ALREADY_INUSE;
	if(0!=(ret=sys_recurse_fdelete((char*)fullpath.c_str())))
		return ret;
	if(0!=(ret=sys_mkdir((char*)fullpath.c_str())))
		return ret;
	return 0;
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
	return NativeFsDev::Format(cmd);
}
int fs_native_mount(char* cmd,void** hdev)
{
	NativeFsDev* dev=new NativeFsDev;
	int ret=dev->Init(cmd);
	if(ret!=0)
	{
		delete dev;
		return ret;
	}
	*hdev=dev;
	return 0;
}
void fs_native_unmount(void* hdev)
{
	NativeFsDev* dev=(NativeFsDev*)hdev;
	dev->Exit();
	delete dev;
}
void* fs_native_open(void* hdev,char* pathname,dword flags)
{
	decl_dev(dev,hdev);
	vector<string> vpath;
	split_path(pathname,vpath,'/');
	nt_path(fullpath,dev,pathname);
	pINode node;
	if(NULL!=(node=dev->GetINodeInTree(vpath)))
	{
		NativeFsRec* pRec=nt_rec(node);
		pRec->AddRef();
		return pRec;
	}
	void* h=sys_fopen((char*)fullpath.c_str(),flags|FILE_READ|FILE_NOCACHE);
	if(!VALID(h))
		return NULL;
	node=dev->GetINode(vpath);
	assert(node!=NULL);
	if(node==NULL)
		return NULL;
	NativeFsRec* pRec=new NativeFsRec(dev);
	pRec->fd=h;
	pRec->host=dev;
	pRec->node=node;
	pRec->AddRef();
	node->t.priv=pRec;
	return pRec;
}
void fs_native_close(void* hdev,void* hfile)
{
	decl_rec(pRec,hfile);
	pRec->Release();
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
