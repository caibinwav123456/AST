#ifndef _SYSFS_STRUCT_H_
#define _SYSFS_STRUCT_H_
#include "common.h"
#include "sysfs.h"
#include "algor_templ.h"
#include <string>
#include <map>
#include <algorithm>
using namespace std;
struct LinearBuffer
{
	LinearBuffer()
	{
		buf=NULL;
		len=0;
		offset=0;
		end=0;
		seq=0;
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
	uint end;
	int seq;
	bool dirty;
};
struct FileIoRec
{
	FileIoRec()
	{
		phif=NULL;
		hFile=NULL;
		iobuf=NULL;
		nbuf=0;
	}
	~FileIoRec()
	{
		if(iobuf!=NULL)
			delete[] iobuf;
	}
	void** phif;
	void* hFile;
	LinearBuffer* iobuf;
	uint nbuf;
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
struct BufferPtr
{
	int seq;
	LinearBuffer* buffer;
};
bool operator<(BufferPtr a,BufferPtr b);
bool operator<=(BufferPtr a,BufferPtr b);
class SortedFileIoRec : public FileIoRec
{
public:
	SortedFileIoRec(uint n,uint m) : sorted_buf(n)
	{
		seq=0;
		nbuf=n;
		iobuf=new LinearBuffer[nbuf];
		for(int i=0;i<(int)nbuf;i++)
		{
			iobuf[i].Init(m);
		}
	}
private:
	int get_seq()
	{
		int s=seq;
		seq++;
		return s;
	}
	Heap<BufferPtr> sorted_buf;
	int seq;
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
	int ConnectServer(void** phif);
	void Reconnect();
	bool ReqHandler(uint cmd,void* addr,void* param,int op);
private:
	int ListStorageModule(vector<pair<int,int>>& index);
	int EnumStorageModule();
	int EnumStorageModule(vector<proc_data>* pdata);
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
