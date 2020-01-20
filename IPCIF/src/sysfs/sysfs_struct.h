#ifndef _SYSFS_STRUCT_H_
#define _SYSFS_STRUCT_H_
#include "common.h"
#include "sysfs.h"
#include "algor_templ.h"
#include "process_data.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>
using namespace std;
struct fs_datagram_param
{
	datagram_base* dbase;
	union
	{
		struct
		{
			dword flags;
			void** hFile;
			string* path;
		}fsopen;
	};
};
struct LinearBuffer
{
	LinearBuffer()
	{
		buf=NULL;
		len=0;
		offset=0;
		offhigh=0;
		end=0;
		seq=0;
		heap_index=0;
		valid=false;
		dirty=false;
	}
	~LinearBuffer()
	{
		if(buf!=NULL)
			delete[] buf;
	}
	void Init(uint n)
	{
		len=n;
		buf=new byte[len];
	}
	byte* buf;
	uint len;
	uint offset;
	uint offhigh;
	uint end;
	int seq;
	uint heap_index;
	bool valid;
	bool dirty;
};
struct FileIoRec
{
	FileIoRec()
	{
		hFile=NULL;
		iobuf=NULL;
		pif=NULL;
		nbuf=0;
		offset=0;
		offhigh=0;
	}
	~FileIoRec()
	{
		if(iobuf!=NULL)
			delete[] iobuf;
	}
	void* hFile;
	LinearBuffer* iobuf;
	if_proc* pif;
	string path;
	uint nbuf;
	uint offset;
	uint offhigh;
};
struct FileServerKey
{
	void* caller;
	void* hFile;
};
struct FileServerRec
{
	void* caller;
	void* hFile;
	dword sid;
};
void* sysfs_get_handle();
struct offset64
{
	uint off;
	uint offhigh;
};
struct BufferPtr
{
	LinearBuffer* buffer;
};
bool operator<(BufferPtr a,BufferPtr b);
bool operator<=(BufferPtr a,BufferPtr b);
class less_buf_ptr
{
public:
	bool operator()(const offset64& a,const offset64& b) const;
};
class assign_buf_ptr
{
public:
	BufferPtr& operator()(BufferPtr& a,BufferPtr& b,uint index);
};
class swap_buf_ptr
{
public:
	void operator()(BufferPtr& a,BufferPtr& b);
};
class SortedFileIoRec : public FileIoRec
{
public:
	SortedFileIoRec(uint n,uint m,dword flag=0) : sorted_buf(n)
	{
		seq=0;
		flags=flag;
		nbuf=n;
		iobuf=new LinearBuffer[nbuf];
		for(int i=0;i<(int)nbuf;i++)
		{
			iobuf[i].Init(m);
			BufferPtr ptr;
			ptr.buffer=iobuf+i;
			free_buf.push_back(ptr);
		}
	}
	dword get_flags(){return flags;}
	LinearBuffer* get_buffer(offset64 off);
	bool add_buffer(LinearBuffer* buf,bool add_to_free,bool update_seq);
private:
	int get_seq()
	{
		int s=seq;
		seq++;
		return s;
	}
	Heap<BufferPtr,assign_buf_ptr,swap_buf_ptr> sorted_buf;
	vector<BufferPtr> free_buf;
	map<offset64,BufferPtr,less_buf_ptr> map_buf;
	int seq;
	dword flags;
};
enum E_FS_MODE
{
	fsmode_permanent_if,
	fsmode_semipermanent_if,
	fsmode_instant_if,
};
#define FC_EXIT  1
#define FC_CLEAR 2
#define FC_MASK  (FC_EXIT|FC_CLEAR)
class SysFs
{
public:
	SysFs()
	{
		nbuf=0;
		buf_len=0;
		mode=fsmode_permanent_if;
		sem=NULL;
		sem_reconn=NULL;
		flag_protect=NULL;
		hthrd_reconn=NULL;
		mutex=NULL;
		flags=0;
		quitcode=ERR_MODULE_NOT_INITED;
	}
	int Init(uint numbuf,uint buflen,if_control_block* pblk=NULL,RequestResolver* resolver=NULL);
	void Exit();
	int SuspendIO(bool bsusp,uint time=0,dword cause=FC_EXIT);
	bool ReqHandler(uint cmd,void* addr,void* param,int op);
	if_proc* GetIfProcFromID(const string& id);
	void* Open(const char* pathname,dword flags);
	int Close(void* h);
	int Seek(void* h,uint seektype,uint offset,uint* offhigh=NULL);
	int ReadWrite(if_cmd_code cmd,void* h,void* buf,uint len,uint* rdwrlen=NULL);
	int GetFileSize(uint* low,uint* high=NULL);
	int SetFileSize(uint low,uint high=0);
	int MoveFile(const char* src,const char* dst);
	int CopyFile(const char* src,const char* dst);
	int DeleteFile(const char* pathname);
	int GetFileAttr(const char* path,DateTime* datetime=NULL,dword* flags=NULL);
	int SetFileAttr(const char* path,DateTime* datetime=NULL,dword* flags=NULL);
	int ListFile(const char* path,vector<string> files);
	int MakeDir(const char* path);
private:
	int ConnectServer(if_proc* pif,void** phif,bool once=false);
	bool Reconnect(void* proc_id);
	int ListStorageModule(vector<pair<int,int>>& index);
	int EnumStorageModule();
	int EnumStorageModule(vector<proc_data>* pdata);
	int BeginTransfer(if_proc* pif,void** phif);
	void EndTransfer(void** phif);
	int ReOpen(SortedFileIoRec* pRec,void* hif);
	static int cb_reconn(void* param);
	map<void*,SortedFileIoRec*> fmap;
	vector<proc_data> pvdata;
	vector<if_proc*> ifvproc;
	uint nbuf;
	uint buf_len;
	E_FS_MODE mode;
	void* sem;
	void* sem_reconn;
	void* flag_protect;
	void* hthrd_reconn;
	cmutex* mutex;
	dword flags;
	int quitcode;
};
extern SysFs g_sysfs;
#endif
