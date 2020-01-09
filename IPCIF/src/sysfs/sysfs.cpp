#define DLL_IMPORT
#include "common.h"
#include "sysfs_struct.h"
#include "interface.h"
#include "utility.h"
#include "config_val_extern.h"
#include "path.h"
DEFINE_UINT_VAL(use_storage_level,0);
DEFINE_UINT_VAL(sysfs_query_pass,4);
#define active_storage ifvproc[use_storage_level]
#define MAX_CONNECT_TIMES 10
void* sysfs_get_handle()
{
	static uint i=1;
	void* h=(void*)i;
	i++;
	if(i==(uint)-1)
		i=1;
	return h;
}
SysFs g_sysfs;
bool operator<(BufferPtr a,BufferPtr b)
{
	return a.seq-b.seq<0;
}
bool operator<=(BufferPtr a,BufferPtr b)
{
	return a.seq-b.seq<=0;
}
bool ClearHandler(uint cmd,void* addr,void* param,int op);
int SysFs::cb_reconn(void* param)
{
	SysFs* sysfs=(SysFs*)param;
	while(sysfs->quitcode==0)
	{
		sys_wait_sem(sysfs->sem_reconn);
		if(sysfs->quitcode!=0)
			break;
		bool pass;
		do
		{
			pass=true;
			for(int i=0;i<(int)sysfs->ifvproc.size();i++)
			{
				if(0!=sysfs->ConnectServer(sysfs->ifvproc[i],&sysfs->ifvproc[i]->hif))
					pass=false;
				if(sysfs->quitcode!=0)
					break;
			}
			if(sysfs->quitcode!=0)
				break;
		}while(!pass);
		sysfs->SuspendIO(false,0,FC_CLEAR);
	}
	return 0;
}
int SysFs::Init(uint numbuf,uint buflen,if_control_block* pblk,RequestResolver* resolver)
{
	int ret=0;
	if(numbuf<1)
		numbuf=1;
	if(buflen==0)
		buflen=1;
	buflen=((buflen+1023)&~1023);
	nbuf=numbuf;
	buf_len=buflen;
	if(pblk!=NULL)
	{
		void* m=pblk->m.get_mutex();
		if(!VALID(m))
		{
			ret=ERR_GENERIC;
			goto failed;
		}
		mutex=&pblk->m;
		if(0!=(ret=EnumStorageModule(&pblk->pdata)))
			goto failed;
		mode=fsmode_permanent_if;
	}
	else if(resolver!=NULL)
	{
		if(0!=(ret=EnumStorageModule()))
			goto failed;
		mode=fsmode_semipermanent_if;
		resolver->AddHandler(ClearHandler);
		for(int i=0;i<(int)ifvproc.size();i++)
		{
			if(0!=(ret=ConnectServer(ifvproc[i],&ifvproc[i]->hif)))
				goto failed;
		}
	}
	else
	{
		if(0!=(ret=EnumStorageModule()))
			goto failed;
		mode=fsmode_instant_if;
	}
	sem=sys_create_sem(sysfs_query_pass,sysfs_query_pass,NULL);
	bool b=(mode==fsmode_semipermanent_if);
	if(b)
	{
		sem_reconn=sys_create_sem(0,1,NULL);
		flag_protect=sys_create_sem(1,1,NULL);
	}
	if(!VALID(sem)||(b&&(!VALID(sem_reconn)||!VALID(flag_protect))))
	{
		ret=ERR_GENERIC;
		goto failed;
	}
	flags=0;
	quitcode=0;
	if(b)
	{
		hthrd_reconn=sys_create_thread(cb_reconn,this);
		if(!VALID(hthrd_reconn))
		{
			ret=ERR_GENERIC;
			goto failed;
		}
	}
	return ret;
failed:
	quitcode=ERR_MODULE_NOT_INITED;
	if(VALID(hthrd_reconn))
	{
		sys_signal_sem(sem_reconn);
		sys_wait_thread(hthrd_reconn);
		sys_close_thread(hthrd_reconn);
		hthrd_reconn=NULL;
	}
	if(mode==fsmode_semipermanent_if)
	{
		for(int i=0;i<(int)ifvproc.size();i++)
		{
			if(VALID(ifvproc[i]->hif))
			{
				close_if(ifvproc[i]->hif);
				ifvproc[i]->hif=NULL;
			}
		}
	}
	pvdata.clear();
	ifvproc.clear();
	if(VALID(sem))
	{
		sys_close_sem(sem);
		sem=NULL;
	}
	if(VALID(sem_reconn))
	{
		sys_close_sem(sem_reconn);
		sem_reconn=NULL;
	}
	if(VALID(flag_protect))
	{
		sys_close_sem(flag_protect);
		flag_protect=NULL;
	}
	mutex=NULL;
	return ret;
}
void SysFs::Exit()
{
	quitcode=ERR_MODULE_NOT_INITED;
	SuspendIO(false);
	if(VALID(hthrd_reconn))
	{
		sys_signal_sem(sem_reconn);
		sys_wait_thread(hthrd_reconn);
		sys_close_thread(hthrd_reconn);
		hthrd_reconn=NULL;
	}
	if(mode==fsmode_semipermanent_if)
	{
		for(int i=0;i<(int)ifvproc.size();i++)
		{
			if(VALID(ifvproc[i]->hif))
			{
				close_if(ifvproc[i]->hif);
				ifvproc[i]->hif=NULL;
			}
		}
	}
	for(map<void*,SortedFileIoRec*>::iterator it=fmap.begin();it!=fmap.end();it++)
	{
		delete it->second;
	}
	fmap.clear();
	pvdata.clear();
	ifvproc.clear();
	if(VALID(sem))
	{
		sys_close_sem(sem);
		sem=NULL;
	}
	if(VALID(sem_reconn))
	{
		sys_close_sem(sem_reconn);
		sem_reconn=NULL;
	}
	if(VALID(flag_protect))
	{
		sys_close_sem(flag_protect);
		flag_protect=NULL;
	}
	mutex=NULL;
}
int SysFs::SuspendIO(bool bsusp,uint time,dword cause)
{
	if(mode==fsmode_semipermanent_if)
	{
		dword flag;
		sys_wait_sem(flag_protect);
		flag=flags;
		if(bsusp)
			flags|=cause;
		else
			flags&=~cause;
		sys_signal_sem(flag_protect);
		if((bsusp&&(flag&FC_MASK)!=0)
			||(!bsusp&&(flag&~cause)!=0))
			return 0;
	}
	if(bsusp)
	{
		for(int i=0;i<(int)sysfs_query_pass;i++)
		{
			if(ERR_TIMEOUT==sys_wait_sem(sem,time))
			{
				for(;i>0;i--)
				{
					sys_signal_sem(sem);
				}
				return ERR_BUSY;
			}
		}
		return 0;
	}
	else
	{
		for(int i=0;i<(int)sysfs_query_pass;i++)
		{
			sys_signal_sem(sem);
		}
		return 0;
	}
}
int SysFs::ConnectServer(if_proc* pif,void** phif)
{
	if_initial init;
	init.user=get_if_user();
	init.id=(char*)pif->id.c_str();
	init.smem_size=0;
	init.nthread=0;
	for(int i=0;i<MAX_CONNECT_TIMES;i++)
	{
		sys_sleep(200);
		if(0==connect_if(&init,phif))
		{
			return 0;
		}
	}
	LOGFILE(0,log_ftype_error,"Connecting to interface %s failed, try reconnecting...",init.id);
	char msg[256];
	sprintf(msg,"Connect to interface %s failed",init.id);
	sys_show_message(msg);
	return ERR_IF_CONN_FAILED;
}
//The code which calls Reconnect, SuspendIO and Exit must in the same thread.
bool SysFs::Reconnect(void* proc_id)
{
	if(quitcode!=0)
		return false;
	bool found=false;
	bool ret=false;
	if(!VALID(proc_id))
	{
		for(int i=0;i<(int)ifvproc.size();i++)
		{
			if(VALID(ifvproc[i]->hif))
			{
				found=true;
				break;
			}
		}
	}
	else
	{
		for(int i=0;i<(int)ifvproc.size();i++)
		{
			if(ifvproc[i]->pdata->id==proc_id)
			{
				ret=true;
				if(VALID(ifvproc[i]->hif))
				{
					found=true;
					break;
				}
			}
		}
	}
	if(found)
	{
		SuspendIO(true,0,FC_CLEAR);
		if(!VALID(proc_id))
		{
			for(int i=0;i<(int)ifvproc.size();i++)
			{
				if(VALID(ifvproc[i]->hif))
				{
					close_if(ifvproc[i]->hif);
					ifvproc[i]->hif=NULL;
				}
			}
		}
		else
		{
			for(int i=0;i<(int)ifvproc.size();i++)
			{
				if(ifvproc[i]->pdata->id==proc_id
					&&VALID(ifvproc[i]->hif))
				{
					close_if(ifvproc[i]->hif);
					ifvproc[i]->hif=NULL;
				}
			}
		}
		sys_signal_sem(sem_reconn);
	}
	return ret;
}
bool ClearHandler(uint cmd,void* addr,void* param,int op)
{
	return g_sysfs.ReqHandler(cmd,addr,param,op);
}
bool SysFs::ReqHandler(uint cmd,void* addr,void* param,int op)
{
	if(quitcode!=0)
		return false;
	bool clear=false;
	bool ret=false;
	dg_clear* dc=NULL;
	switch(cmd)
	{
	case CMD_CLEAR_ALL:
		clear=true;
		break;
	case CMD_CLEAR:
		dc=(dg_clear*)addr;
		clear=true;
		break;
	}
	if(clear)
		ret=Reconnect(dc==NULL?NULL:dc->clear.id);
	return ret;
}
if_proc* SysFs::GetIfProcFromID(const string& id)
{
	if(id.empty())
		return active_storage;
	for(int i=0;i<(int)ifvproc.size();i++)
	{
		if(ifvproc[i]->id==id)
			return ifvproc[i];
	}
	return NULL;
}
bool less_if(const if_proc* i1,const if_proc* i2)
{
	return i1->prior<i2->prior;
}
inline bool __insert_proc_data__(vector<proc_data>& pdata,const process_stat& pstat,vector<pair<int,int>>& index)
{
	if(!pstat.is_managed)
		return false;
	proc_data data;
	bool found=false;
	for(int i=0;i<pstat.ifs->count;i++)
	{
		if_proc ifproc;
		ifproc.hif=NULL;
		ifproc.id=pstat.ifs->if_id[i].if_name;
		ifproc.usage=pstat.ifs->if_id[i].usage;
		ifproc.cnt=pstat.ifs->if_id[i].thrdcnt;
		ifproc.prior=pstat.ifs->if_id[i].prior;
		ifproc.pdata=NULL;
		data.ifproc.push_back(ifproc);
		if(i>0&&ifproc.usage==CFG_IF_USAGE_STORAGE)
		{
			found=true;
			index.push_back(pair<int,int>((int)pdata.size(),i));
		}
	}
	if(!found)
		return false;
	data.name=pstat.file;
	data.cmdline=pstat.cmdline;
	data.id=pstat.id;
	data.ambiguous=!!pstat.ambiguous;
	data.hproc = NULL;
	data.hthrd_shelter = NULL;
	pdata.push_back(data);
	return true;
}
int SysFs::ListStorageModule(vector<pair<int,int>>& index)
{
	process_stat pstat;
	init_process_stat(&pstat,"");
	if_ids ifs;
	pstat.ifs=&ifs;
	index.clear();
	void* h=find_first_exe(&pstat);
	if(!VALID(h))
		return ERR_MODULE_NOT_FOUND;
	__insert_proc_data__(pvdata,pstat,index);
	while(find_next_exe(h,&pstat))
		__insert_proc_data__(pvdata,pstat,index);
	find_exe_close(h);
	for(int i=0;i<(int)pvdata.size();i++)
	{
		for(int j=0;j<(int)pvdata[i].ifproc.size();j++)
		{
			pvdata[i].ifproc[j].pdata=&pvdata[i];
		}
	}
	return 0;
}
int SysFs::EnumStorageModule()
{
	vector<pair<int,int>> index;
	int ret=0;
	if(0!=(ret=ListStorageModule(index)))
		return ret;
	for(int i=0;i<(int)index.size();i++)
	{
		ifvproc.push_back(&pvdata[index[i].first].ifproc[index[i].second]);
	}
	sort(ifvproc.begin(),ifvproc.end(),less_if);
	if(ifvproc.size()<=use_storage_level)
	{
		return ERR_MODULE_NOT_FOUND;
	}
	return 0;
}
int SysFs::EnumStorageModule(vector<proc_data>* pdata)
{
	vector<proc_data>& data=*pdata;
	for(int i=0;i<(int)data.size();i++)
	{
		for(int j=0;j<(int)data[i].ifproc.size();j++)
		{
			if(data[i].ifproc[j].usage==CFG_IF_USAGE_STORAGE)
			{
				ifvproc.push_back(&data[i].ifproc[j]);
			}
		}
	}
	sort(ifvproc.begin(),ifvproc.end(),less_if);
	if(ifvproc.size()<=use_storage_level)
	{
		return ERR_MODULE_NOT_FOUND;
	}
	return 0;
}
static int fs_parse_path(if_proc** ppif,string& path,const string& in_path)
{
	vector<string> split_in_path,split_out_path;
	split_path(in_path,split_in_path,'/');
	if(split_in_path.size()==0)
		return ERR_INVALID_PATH;
	int ret=0;
	if_proc* ifproc;
	if(split_in_path[0][split_in_path[0].size()-1]==':')
	{
		string if_id=split_in_path[0].substr(0,split_in_path[0].size()-1);
		if(if_id.empty())
			return ERR_INVALID_PATH;
		if(NULL==(ifproc=g_sysfs.GetIfProcFromID(if_id)))
			return ERR_INVALID_PATH;
	}
	else
	{
		if(NULL==(ifproc=g_sysfs.GetIfProcFromID("")))
			return ERR_INVALID_PATH;
	}
	split_in_path.erase(split_in_path.begin());
	if(0!=(ret=get_direct_path(split_out_path,split_in_path)))
		return ret;
	merge_path(path,split_out_path,'/');
	path="/"+path;
	*ppif=ifproc;
	return 0;
}
int SysFs::BeginTransfer(if_proc* pif,void** phif)
{
	int ret=0;
	*phif=NULL;
	if(quitcode!=0)
		return quitcode;
	if(mode==fsmode_instant_if)
	{
		if(0!=(ret=ConnectServer(pif,phif)))
			return ret;
	}
	sys_wait_sem(sem);
	if(quitcode!=0)
	{
		sys_signal_sem(sem);
		close_if(*phif);
		*phif=NULL;
		return quitcode;
	}
	if(mode==fsmode_permanent_if)
	{
		sys_wait_sem(mutex->get_mutex());
		if(quitcode!=0)
		{
			sys_signal_sem(mutex->get_mutex());
			sys_signal_sem(sem);
			close_if(*phif);
			*phif=NULL;
			return quitcode;
		}
	}
	if(mode!=fsmode_instant_if)
		*phif=pif->hif;
	return 0;
}
void SysFs::EndTransfer(void** phif)
{
	if(mode==fsmode_permanent_if)
		sys_signal_sem(mutex->get_mutex());
	sys_signal_sem(sem);
	if(mode==fsmode_instant_if&&VALID(*phif))
		close_if(*phif);
	*phif=NULL;
}
void* SysFs::Open(const char* pathname,dword flags)
{
	if_proc* ifproc;
	string path;
	if(0!=fs_parse_path(&ifproc,path,string(pathname)))
		return NULL;
	SortedFileIoRec* pRec=new SortedFileIoRec(nbuf,buf_len);
	pRec->pif=ifproc;
	void* hif;
	if(0!=BeginTransfer(pRec->pif,&hif))
		goto fail;
	
	EndTransfer(&hif);
	void* h=sysfs_get_handle();
	fmap[h]=pRec;
	return h;
fail:
	delete pRec;
	return NULL;
}
int SysFs::Close(void* h)
{
	return 0;
}
int SysFs::Seek(void* h,uint seektype,uint offset,uint* offhigh)
{
	return 0;
}
int SysFs::Read(void* h,void* buf,uint len,uint* rdlen)
{
	return 0;
}
int SysFs::Write(void* h,void* buf,uint len,uint* wrlen)
{
	return 0;
}
int SysFs::MoveFile(const char* src,const char* dst)
{
	return 0;
}
int SysFs::CopyFile(const char* src,const char* dst)
{
	return 0;
}
int SysFs::DeleteFile(const char* pathname)
{
	return 0;
}
int SysFs::GetFileAttr(const char* path,DateTime* datetime,dword* flags)
{
	return 0;
}
int SysFs::SetFileAttr(const char* path,DateTime* datetime,dword* flags)
{
	return 0;
}
int SysFs::ListFile(const char* path,vector<string> files)
{
	return 0;
}
int SysFs::MakeDir(const char* path)
{
	return 0;
}