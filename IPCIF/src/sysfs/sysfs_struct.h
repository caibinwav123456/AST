#ifndef _SYSFS_STRUCT_H_
#define _SYSFS_STRUCT_H_
#include "common.h"
#include "algor_templ.h"
#include "process_data.h"
#include "request_resolver.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#define PROC_ID_TABLE_LEN 256
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
		dgc_fsclose fsclose;
		struct
		{
			void* hFile;
			offset64 off;
			void* buf;
			uint* len;
		}fsrdwr;
		dgc_fssize* fssize;
		struct
		{
			string* src;
			string* dst;
		}fsmove;
		struct
		{
			string* path;
		}fsdelete;
		struct
		{
			string* path;
		}fsmkdir;
		struct
		{
			dword mask;
			DateTime* date;
			dword* flags;
			string* path;
		}fsattr;
		struct
		{
			uint *nfiles;
			void** handle;
			string* path;
			vector<fsls_element>* files;
		}fslsfiles;
		struct
		{
			const string* devname;
			fs_dev_info* devinfo;
		}fsdevinfo;
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
class SrvProcRing;
#define FSSERVER_REC_TYPE_FILE_DESCRIPTOR 0
#define FSSERVER_REC_TYPE_FILE_LIST       1
struct FileServerRec
{
	FileServerKey key;
	uint type;
	union
	{
		void* fd;
		vector<fsls_element>* pvfiles;
	};
	//SrvProcRing* proc_ring;
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
class less_servrec_ptr
{
public:
	bool operator()(const FileServerKey& a,const FileServerKey& b) const;
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
	LinearBuffer* get_buffer(offset64 off,bool get_oldest=false);
	void add_buffer(LinearBuffer* buf,bool add_to_free,bool update_seq);
	Heap<BufferPtr,assign_buf_ptr,swap_buf_ptr>::iterator get_iter()
	{
		return sorted_buf.BeginIterate();
	}
private:
	int get_seq(){return seq++;}
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
#define FC_EXIT_OFF 0
#define FC_CLEAR_OFF 1
#define FC_EXIT  (1<<FC_EXIT_OFF)
#define FC_CLEAR (3<<FC_CLEAR_OFF)
#define FC_MASK  (FC_EXIT|FC_CLEAR)
int __fs_perm_close(void* handle);
int __fs_recurse_copy(char* from,char* to,file_recurse_cbdata* cbdata);
int __fs_recurse_delete(char* pathname,file_recurse_cbdata* cbdata);
int __fs_recurse_stat(char* pathname,path_recurse_stat* pstat,file_recurse_cbdata* cbdata);
struct fs_if_path
{
	if_proc* ifpath;
	string* purepath;
	fs_if_path(if_proc* _if,string* _path):ifpath(_if),purepath(_path){}
};
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
		fmap_protect=NULL;
		hthrd_reconn=NULL;
		mutex=NULL;
		flags=0;
		quitcode=ERR_MODULE_NOT_INITED;
	}
	int Init(uint numbuf,uint buflen,if_control_block* pblk=NULL,RequestResolver* resolver=NULL);
	void Exit();
	int SuspendIO(bool bsusp,uint time=0,dword cause=FC_EXIT);
	bool ReqHandler(uint cmd,void* addr,void* param,int op);
	void* Open(const char* pathname,dword flags,fs_if_path* ifp=NULL);
	int Close(void* h);
	int Seek(void* h,uint seektype,int offset,int* offhigh=NULL);
	int GetPosition(void* h,uint* offset,uint* offhigh=NULL);
	int ReadWrite(if_cmd_code cmd,void* h,void* buf,uint len,uint* rdwrlen=NULL);
	int FlushBuffer(void* h);
	int GetSetFileSize(if_cmd_code cmd,void* h,uint* low,uint* high=NULL);
	int MoveFile(const char* src,const char* dst,fs_if_path* sifp=NULL,fs_if_path* difp=NULL);
	int CopyFile(const char* src,const char* dst,fs_if_path* sifp=NULL,fs_if_path* difp=NULL);
	int DeleteFile(const char* pathname,fs_if_path* ifp=NULL);
	int GetSetFileAttr(if_cmd_code cmd,const char* path,dword mask,dword* flags=NULL,DateTime* datetime=NULL,fs_if_path* ifp=NULL);
	int ListFile(const char* path,vector<fsls_element>& files,fs_if_path* ifp=NULL);
	int ListDev(vector<string>& devlist,uint* defdev=NULL);
	int GetDevInfo(const string& devname,fs_dev_info& devinfo);
	int MakeDir(const char* path,fs_if_path* ifp=NULL);
private:
	int ConnectServer(if_proc* pif,void** phif,bool once=false);
	bool Reconnect(void* proc_id);
	int ListStorageModule(vector<pair<int,int>>& index);
	int EnumStorageModule();
	int EnumStorageModule(vector<proc_data>* pdata);
	int BeginTransfer(if_proc* pif,void** phif);
	void EndTransfer(void** phif);
	if_proc* GetIfProcFromID(const string& id);
	int CloseHandle(void* h,if_proc* pif);
	int ReOpen(SortedFileIoRec* pRec,void* hif);
	int FlushBuffer(SortedFileIoRec* pRec);
	int GetSetFileSize(if_cmd_code cmd,SortedFileIoRec*pRec,uint* low,uint* high=NULL);
	int DisposeLB(const offset64& off,SortedFileIoRec* pRec,LinearBuffer* pLB);
	int IOBuf(if_cmd_code cmd,SortedFileIoRec* pRec,LinearBuffer* pLB);
	SortedFileIoRec* handle_to_rec_ptr(void* handle);
	static int cb_reconn(void* param);
	void* sysfs_get_handle();
	int fs_parse_path(if_proc** ppif,string& path,string** out_path,const string& in_path,fs_if_path* ifp);
	map<void*,SortedFileIoRec*> fmap;
	vector<proc_data> pvdata;
	vector<if_proc*> ifvproc;
	uint nbuf;
	uint buf_len;
	E_FS_MODE mode;
	void* sem;
	void* sem_reconn;
	void* flag_protect;
	void* fmap_protect;
	void* hthrd_reconn;
	gate* mutex;
	dword flags;
	int quitcode;
};
extern SysFs g_sysfs;
struct BackupData
{
	datagram_base dbase;
	byte* rdwr;
	vector<fsls_element> lsfiles;
	string devtype;
	union
	{
		void* handle;
		struct
		{
			uint len;
			uint lenhigh;
		};
		struct
		{
			uint rdwrlen;
		};
		struct
		{
			void* hls;
			uint nfile;
		};
		struct
		{
			dword mask;
			dword flags;
			DateTime date[3];
		};
	};
	void clear()
	{
		lsfiles.clear();
		devtype.clear();
	}
};
class BackupRing;
typedef bool (*DataBackupRestoreCallback)(datagram_base* dbase,BackupRing* ring,bool backup);
#define CONFIG_CMD_BACKUP_RESTORE_CALLBACK(cmd,callback) \
	BackupRing :: config_cmd_backup_restore_callback __config_back_restore_cb_ ## cmd (cmd, callback)
class BackupRing
{
public:
	BackupRing()
	{
		backup_items=NULL;
		mem_backup=NULL;
		Init();
	}
	~BackupRing()
	{
		if(backup_items!=NULL)
			delete[] backup_items;
		if(mem_backup)
			delete[] mem_backup;
	}
	class config_cmd_backup_restore_callback
	{
	public:
		config_cmd_backup_restore_callback(uint cmd,DataBackupRestoreCallback callback)
		{
			BackupRing::get_config_backup_vec()->push_back(pair<uint,DataBackupRestoreCallback>(cmd,callback));
		}
	};
	void Init();
	void ReleaseAll();
	static vector<pair<uint, DataBackupRestoreCallback>>* get_config_backup_vec();
	BiRingNode<BackupData>* GetNode(datagram_base* data,bool backup);
private:
	map<dword,BiRingNode<BackupData>*> bmap;
	BiRing<BackupData> backup_free;
	BiRing<BackupData> backup_data;
	BiRingNode<BackupData>* backup_items;
	byte* mem_backup;
};
class FsServer;
class SrvProcRing : public BiRing<FileServerRec>
{
public:
	SrvProcRing(FsServer* srv,void* proc_id):pfssrv(srv)
	{
		memset(&t,0,sizeof(t));
		t.key.caller=proc_id;
		//t.proc_ring=this;
		hFileReserve=(byte*)1;
	}
	BackupRing* get_backup_ring()
	{
		return &backup;
	}
	void* get_handle();
private:
	byte* hFileReserve;
	FsServer* pfssrv;
	BackupRing backup;
};
typedef map<FileServerKey,BiRingNode<FileServerRec>*,less_servrec_ptr> fs_key_map;
struct proc_id_ring_rec
{
	int cnt;
	SrvProcRing* ring;
};
class proc_id_ring_map
{
public:
	struct hash_cache
	{
		void* proc;
		uint hash_val;
		hash_cache():proc(NULL),hash_val(0){}
	};
	struct iterator
	{
		iterator(){}
		iterator(map<void*,SrvProcRing*>::iterator _iter,
			map<void*,SrvProcRing*>::iterator _end):iter(_iter),end(_end){}
		void operator++(int)
		{
			iter++;
		}
		void operator++()
		{
			++iter;
		}
		operator bool()
		{
			return iter!=end;
		}
		pair<void* const,SrvProcRing*>& operator*()
		{
			return *iter;
		}
		pair<void* const,SrvProcRing*>* operator->()
		{
			return &*iter;
		}
	private:
		map<void*,SrvProcRing*>::iterator iter;
		map<void*,SrvProcRing*>::iterator end;
	};
	proc_id_ring_map()
	{
		memset(proc_id_table,0,sizeof(proc_id_table));
	}
	bool query_proc_ring(void* proc_id,SrvProcRing** ring=NULL,proc_id_ring_rec** pprec=NULL,map<void*,SrvProcRing*>::iterator* it=NULL);
	SrvProcRing* find_proc_from_id(void* proc_id);
	bool add_proc(void* proc_id,SrvProcRing* proc_ring);
	void add_proc_no_check(void* proc_id,proc_id_ring_rec* prec,SrvProcRing* proc_ring);
	SrvProcRing* remove_proc(void* proc_id);
	SrvProcRing* remove_proc_no_check(void* proc_id,proc_id_ring_rec* prec,map<void*,SrvProcRing*>::iterator& it);
	void clear();
	iterator begin();
private:
	map<void*,SrvProcRing*> proc_id_map;
	proc_id_ring_rec proc_id_table[PROC_ID_TABLE_LEN];
	hash_cache cache;
};
class FssContainer;
class FsServer
{
	friend int fs_server(void* param);
	friend int cb_fs_server(void* addr,void* param,int op);
public:
	FsServer(if_info_storage* pinfo,void** _psem,int* pquit,FssContainer* sc)
	{
		if_info=pinfo;
		psem=_psem;
		hthrd_server=NULL;
		quitcode=pquit;
		host=sc;
	}
	int Init();
	void Exit();
	void Clear(void* proc_id,bool cl_root=false);
	bool Reset(void* proc_id);
	bool CheckExist(void* proc_id,void* hfile)
	{
		FileServerKey key;
		key.caller=proc_id;
		key.hFile=hfile;
		return smap.find(key)!=smap.end();
	}
private:
	int CreateInterface();
	int MountDev();
	bool RestoreData(datagram_base* data);
	void BackupData(datagram_base* data);
	bool AddProcRing(void* proc_id);
	void* AddNode(void* proc_id,FileServerRec* pRec);
	bool RemoveNode(void* proc_id,void* h);
	int HandleOpen(dg_fsopen* fsopen);
	int HandleClose(dg_fsclose* fsclose);
	int HandleReadWrite(dg_fsrdwr* fsrdwr);
	int HandleGetSetSize(dg_fssize* fssize);
	int HandleMove(dg_fsmove* fsmove);
	int HandleDelete(dg_fsdel* fsdel);
	int HandleMakeDir(dg_fsmkdir* fsmkdir);
	int HandleGetSetAttr(dg_fsattr* fsattr);
	int HandleListFiles(dg_fslsfiles* fslsfiles);
	int HandleGetDevInfo(dg_fsdevinfo* fsdevinfo);
	BiRingNode<FileServerRec>* get_fs_node(void* proc_id,void* h,fs_key_map::iterator* it=NULL);
	proc_id_ring_map proc_id_map;
	fs_key_map smap;
	void* hthrd_server;
	if_info_storage* if_info;
	void** psem;
	int* quitcode;
	FssContainer* host;
};
class FssContainer
{
public:
	FssContainer()
	{
		sem=NULL;
		quitcode=ERR_MODULE_NOT_INITED;
		locked=false;
		memset(cmd_data_backup_restore_map,0,sizeof(cmd_data_backup_restore_map));
	}
	int Init(vector<if_proc>* pif,RequestResolver* resolver);
	void Exit();
	int SuspendIO(bool bsusp,uint time=0);
	bool ReqHandler(uint cmd,void* addr,void* param,int op);
	DataBackupRestoreCallback GetDBRCallback(uint cmd);
private:
	void SetupBackupRestoreMap();
	vector<FsServer*> vfs_srv;
	vector<storage_mod_info> vfs_mod;
	DataBackupRestoreCallback cmd_data_backup_restore_map[CMD_COUNT];
	void* sem;
	bool locked;
	int quitcode;
};
extern FssContainer g_fssrv;
#endif
