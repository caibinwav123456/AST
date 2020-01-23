#define DLL_IMPORT
#include "common.h"
#include "sysfs_struct.h"
#include "interface.h"
#include "utility.h"
#include "config_val_extern.h"
#include "path.h"
#include "common_request.h"
#include "Integer64.h"
#include <assert.h>
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
	return a.buffer->seq-b.buffer->seq<0;
}
bool operator<=(BufferPtr a,BufferPtr b)
{
	return a.buffer->seq-b.buffer->seq<=0;
}
static inline bool get_offset_len(UInteger64& off,uint& len,UInteger64& bufoff,uint& start,uint& end,uint buf_len)
{
	if(len==0)
		return false;
	UInteger64 bottom_off=off;
	bottom_off.low&=(~(buf_len-1));
	uint hi=0;
	UInteger64 top_off=bottom_off+UInteger64(buf_len,&hi);
	if((top_off.high&(1<<31))!=0)
		return false;
	UInteger64 top=top_off-off;
	assert(top.high==0);
	off=top_off;
	bufoff=bottom_off;
	start=buf_len-top.low;
	if(len<=top.low)
	{
		end=start+len;
		len=0;
	}
	else
	{
		end=buf_len;
		len-=top.low;
	}
	return true;
}
bool less_buf_ptr::operator()(const offset64& a,const offset64& b) const
{
	UInteger64 ua(a.off,&a.offhigh),ub(b.off,&b.offhigh);
	return ua<ub;
}
BufferPtr& assign_buf_ptr::operator()(BufferPtr& a,BufferPtr& b,uint index)
{
	a.buffer=b.buffer;
	a.buffer->heap_index=index;
	return a;
}
void swap_buf_ptr::operator()(BufferPtr& a,BufferPtr& b)
{
	assert(a.buffer!=NULL&&b.buffer!=NULL);
	if(a.buffer==b.buffer)
		return;
	algor_swap(a.buffer,b.buffer);
	algor_swap(a.buffer->heap_index,b.buffer->heap_index);
}
LinearBuffer* SortedFileIoRec::get_buffer(offset64 off)
{
	if(!free_buf.empty())
	{
		LinearBuffer* buf=free_buf.back().buffer;
		free_buf.pop_back();
		return buf;
	}
	uint index=0;
	BufferPtr bufptr;
	map<offset64,BufferPtr,less_buf_ptr>::iterator it;
	if((it=map_buf.find(off))!=map_buf.end())
	{
		BufferPtr tmpptr=it->second;
		map_buf.erase(it);
		bool b=sorted_buf.RemoveMin(bufptr,tmpptr.buffer->heap_index);
		assert(b);
		assert(tmpptr.buffer==bufptr.buffer);
		return tmpptr.buffer;
	}
	if(sorted_buf.RemoveMin(bufptr))
	{
		offset64 toff;
		toff.off=bufptr.buffer->offset;
		toff.offhigh=bufptr.buffer->offhigh;
		it=map_buf.find(toff);
		assert(it!=map_buf.end());
		map_buf.erase(it);
		return bufptr.buffer;
	}
	else
		return NULL;
}
bool SortedFileIoRec::add_buffer(LinearBuffer* buf,bool add_to_free,bool update_seq)
{
	assert(buf!=NULL);
	BufferPtr bptr;
	bptr.buffer=buf;
	if(add_to_free)
	{
		buf->valid=false;
		buf->dirty=false;
		buf->heap_index=0;
		free_buf.push_back(bptr);
	}
	else
	{
		if(update_seq)
			buf->seq=get_seq();
		BufferPtr ovbptr;
		bool b=sorted_buf.Add(bptr,ovbptr);
		assert(!b);
		offset64 off;
		off.off=buf->offset;
		off.offhigh=buf->offhigh;
		map_buf[off]=bptr;
	}
	return true;
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
	if(buflen<1024)
		buflen=1024;
	if(buflen>1024*1024)
		buflen=1024*1024;
	int l;
	for(l=0;;l++)
	{
		if(buflen==1)
			break;
		buflen>>=1;
	}
	for(;l>0;l--)
		buflen<<=1;
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
int SysFs::ConnectServer(if_proc* pif,void** phif,bool once)
{
	if_initial init;
	init.user=get_if_user();
	init.id=(char*)pif->id.c_str();
	init.smem_size=0;
	init.nthread=0;
	int ret=0;
	if(once)
	{
		if(0==(ret=connect_if(&init,phif)))
			return 0;
	}
	else
	{
		for(int i=0;i<MAX_CONNECT_TIMES;i++)
		{
			sys_sleep(200);
			if(0==connect_if(&init,phif))
			{
				return 0;
			}
		}
		ret=ERR_IF_CONN_FAILED;
	}
	LOGFILE(0,log_ftype_error,"Connecting to interface %s failed, try reconnecting...",init.id);
	if(!once)
	{
		char msg[256];
		sprintf(msg,"Connect to interface %s failed",init.id);
		sys_show_message(msg);
	}
	return ret;
}
//The code which calls Reconnect, SuspendIO and Exit must be in the same thread.
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
		if(0!=(ret=ConnectServer(pif,phif,true)))
			return ret;
	}
	sys_wait_sem(sem);
	if(quitcode!=0)
	{
		sys_signal_sem(sem);
		if(mode==fsmode_instant_if)
			close_if(*phif);
		*phif=NULL;
		return quitcode;
	}
	if(mode==fsmode_permanent_if)
	{
		sys_wait_sem(mutex->get_mutex());
		if(quitcode!=0||pif->hif==NULL)
		{
			sys_signal_sem(mutex->get_mutex());
			sys_signal_sem(sem);
			return quitcode==0?ERR_GENERIC:quitcode;;
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
int cb_fsc(void* addr,void* param,int op)
{
	fs_datagram_param* dgp=(fs_datagram_param*)param;
	if(op&OP_PARAM)
	{
		memcpy(addr,dgp->dbase,sizeof(datagram_base));
	}
	if(op&OP_RETURN)
	{
		memcpy(dgp->dbase,addr,sizeof(datagram_base));
	}
	uint cmd=dgp->dbase->cmd;
	switch(cmd)
	{
	case CMD_FSOPEN:
		{
			dg_fsopen* fsopen=(dg_fsopen*)addr;
			if(op&OP_PARAM)
			{
				fsopen->open.flags=dgp->fsopen.flags;
				fsopen->open.hFile=NULL;
				strcpy(fsopen->open.path,dgp->fsopen.path->c_str());
			}
			if(op&OP_RETURN)
			{
				*dgp->fsopen.hFile=fsopen->open.hFile;
			}
		}
		break;
	case CMD_FSREAD:
	case CMD_FSWRITE:
		{
			dg_fsrdwr* fsrdwr=(dg_fsrdwr*)addr;
			if(op&OP_PARAM)
			{
				fsrdwr->rdwr.handle=dgp->fsrdwr.hFile;
				fsrdwr->rdwr.offset=dgp->fsrdwr.off.off;
				fsrdwr->rdwr.offhigh=dgp->fsrdwr.off.offhigh;
				fsrdwr->rdwr.len=*dgp->fsrdwr.len;
				if(cmd==CMD_FSWRITE)
					memcpy(fsrdwr->rdwr.buf,dgp->fsrdwr.buf,*dgp->fsrdwr.len);
			}
			if(op&OP_RETURN)
			{
				*dgp->fsrdwr.len=fsrdwr->rdwr.len;
				if(cmd==CMD_FSREAD&&dgp->dbase->ret==0)
					memcpy(dgp->fsrdwr.buf,fsrdwr->rdwr.buf,*dgp->fsrdwr.len);
			}
		}
		break;
	}
	return 0;
}
int SysFs::ReOpen(SortedFileIoRec* pRec,void* hif)
{
	int ret=0;
	pRec->hFile=NULL;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_FSOPEN);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsopen.flags=pRec->get_flags();
	param.fsopen.hFile=&pRec->hFile;
	param.fsopen.path=&pRec->path;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	return ret;
}
void* SysFs::Open(const char* pathname,dword flags)
{
	if_proc* ifproc;
	string path;
	int ret=0;
	if(0!=(ret=fs_parse_path(&ifproc,path,string(pathname))))
		return NULL;
	SortedFileIoRec* pRec=new SortedFileIoRec(nbuf,buf_len,flags);
	pRec->pif=ifproc;
	pRec->path=path;
	void* hif;
	if(0!=(ret=BeginTransfer(pRec->pif,&hif)))
		goto failed;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_FSOPEN);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsopen.flags=flags;
	param.fsopen.hFile=&pRec->hFile;
	param.fsopen.path=&path;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	if(ret!=0)
		goto failed;
	void* h=sysfs_get_handle();
	fmap[h]=pRec;
	return h;
failed:
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
inline int check_access_rights(if_cmd_code cmd,SortedFileIoRec* pRec)
{
	switch(cmd)
	{
	case CMD_FSREAD:
		if((pRec->get_flags()&FILE_READ)==0)
			return ERR_FS_NO_ACCESS;
		break;
	case CMD_FSWRITE:
		if((pRec->get_flags()&FILE_WRITE)==0)
			return ERR_FS_NO_ACCESS;
		break;
	case CMD_FSGETSIZE:
		if((pRec->get_flags()&FILE_READ)==0)
			return ERR_FS_NO_ACCESS;
		break;
	case CMD_FSSETSIZE:
		if((pRec->get_flags()&FILE_WRITE)==0)
			return ERR_FS_NO_ACCESS;
		break;
	default:
		return ERR_FS_NO_ACCESS;
	}
	return 0;
}
int SysFs::ReadWrite(if_cmd_code cmd,void* h,void* buf,uint len,uint* rdwrlen)
{
	assert(cmd==CMD_FSREAD||cmd==CMD_FSWRITE);
	if (!VALID(h))
		return ERR_FS_INVALID_HANDLE;
	map<void*,SortedFileIoRec*>::iterator it=fmap.find(h);
	if(it==fmap.end())
		return ERR_FS_INVALID_HANDLE;
	SortedFileIoRec* pRec=it->second;
	int ret=0;
	if(0!=(ret=check_access_rights(cmd,pRec)))
		return ret;
	if(len==0)
	{
		if(rdwrlen!=NULL)
			*rdwrlen=0;
		return 0;
	}
	byte* bbuf=(byte*)buf;
	UInteger64 pos(pRec->offset,&pRec->offhigh);
	UInteger64 cur(pos);
	UInteger64 bufoff;
	uint start,end,left=len;
	while(get_offset_len(cur,left,bufoff,start,end,buf_len))
	{
		assert(end>=start);
		offset64 off;
		off.off=bufoff.low;
		off.offhigh=bufoff.high;
		LinearBuffer* pLBuf=pRec->get_buffer(off);
		assert(pLBuf!=NULL);
		if(0!=(ret=DisposeLB(off,pRec,pLBuf)))
		{
			left+=(end-start);
			break;
		}
		bool read_eof=false;
		switch(cmd)
		{
		case CMD_FSREAD:
			{
				uint ought_to_copy=end-start;
				uint bytes_avail=(pLBuf->end>start?pLBuf->end-start:0);
				uint bytes_to_copy;
				if(bytes_avail>=ought_to_copy)
				{
					bytes_to_copy=ought_to_copy;
				}
				else
				{
					bytes_to_copy=bytes_avail;
					read_eof=true;
					left+=(ought_to_copy-bytes_avail);
				}
				memcpy(bbuf,pLBuf->buf+start,bytes_to_copy);
				bbuf+=bytes_to_copy;
			}
			break;
		case CMD_FSWRITE:
			if(start==end)
				break;
			memcpy(pLBuf->buf+start,bbuf,end-start);
			bbuf+=(end-start);
			pLBuf->end<end?(pLBuf->end=end):0;
			pLBuf->dirty=true;
			break;
		}
		pRec->add_buffer(pLBuf,false,true);
		if(read_eof)
			break;
	}
	assert(left<=len);
	uint done=len-left;
	uint dummy=0;
	pos=pos+UInteger64(done,&dummy);
	assert((pos.high&(1<<31))==0);
	pRec->offset=pos.low;
	pRec->offhigh=pos.high;
	if(rdwrlen!=NULL)
		*rdwrlen=done;
	else if(done!=len)
		return ERR_INVALID_PARAM;
	return ret;
}
int SysFs::DisposeLB(const offset64& off,SortedFileIoRec* pRec,LinearBuffer* pLB)
{
	int ret=0;
	if(pLB->valid)
	{
		UInteger64 off1(off.off,&off.offhigh),
			off2(pLB->offset,&pLB->offhigh);
		if(off1==off2)
		{
			if(pLB->end<buf_len&&!pLB->dirty)
			{
				//reload buffer
				if(0!=(ret=IOBuf(CMD_FSREAD,pRec,pLB)))
				{
					pRec->add_buffer(pLB,true,false);
					return ret;
				}
			}
		}
		else
		{
			if(pLB->dirty)
			{
				//write buffer
				if(0!=(ret=IOBuf(CMD_FSWRITE,pRec,pLB)))
				{
					pRec->add_buffer(pLB,false,true);
					return ret;
				}
				pLB->dirty=false;
			}
			//load buffer
			pLB->offset=off.off;
			pLB->offhigh=off.offhigh;
			if(0!=(ret=IOBuf(CMD_FSREAD,pRec,pLB)))
			{
				pRec->add_buffer(pLB,true,false);
				return ret;
			}
		}
	}
	else
	{
		//load buffer
		pLB->offset=off.off;
		pLB->offhigh=off.offhigh;
		if(0!=(ret=IOBuf(CMD_FSREAD,pRec,pLB)))
		{
			pRec->add_buffer(pLB,true,false);
			return ret;
		}
		pLB->valid=true;
		pLB->dirty=false;
	}
	return 0;
}
int SysFs::IOBuf(if_cmd_code cmd,SortedFileIoRec* pRec,LinearBuffer* pLB)
{
	assert(cmd==CMD_FSREAD||cmd==CMD_FSWRITE);
	void* hif;
	int ret=0;
	datagram_base dg;
	init_current_datagram_base(&dg,cmd);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsrdwr.hFile=pRec->hFile;
	const uint size_if_buf=sizeof(dgc_fsrdwr::buf);
	if(0!=(ret=BeginTransfer(pRec->pif,&hif)))
		return ret;
	uint bytes_to_transfer=(cmd==CMD_FSREAD?buf_len:pLB->end);
	uint left=bytes_to_transfer;
	UInteger64 off(pLB->offset,&pLB->offhigh);
	uint dummy=0;
	byte* bbuf=pLB->buf;
	while(left>0)
	{
		assert((off.high&(1<<31))==0);
		uint ought_to_transfer=(left>size_if_buf?size_if_buf:left);
		uint transfer_once=ought_to_transfer;
		param.fsrdwr.off.off=off.low;
		param.fsrdwr.off.offhigh=off.high;
		param.fsrdwr.len=&transfer_once;
		param.fsrdwr.buf=bbuf;
		if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
			goto end;
		ret=dg.ret;
end:
		if(ret!=ERR_FS_INVALID_HANDLE)
			goto end2;
		pRec->hFile=NULL;
		if(0!=(ret=ReOpen(pRec,hif)))
			goto end2;
		init_current_datagram_base(&dg,cmd);
		param.fsrdwr.hFile=pRec->hFile;
		if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
			goto end2;
		ret=dg.ret;
end2:
		if(ret!=0)
			break;
		assert(transfer_once<=ought_to_transfer);
		left-=transfer_once;
		bbuf+=transfer_once;
		off=off+UInteger64(transfer_once,&dummy);
		if(transfer_once<ought_to_transfer)
		{
			if(cmd==CMD_FSWRITE)
				ret=ERR_FILE_IO;
			break;
		}
	}
	EndTransfer(&hif);
	assert(left<=bytes_to_transfer);
	if(ret==0&&cmd==CMD_FSREAD)
		pLB->end=bytes_to_transfer-left;
	return ret;
}
int SysFs::FlushBuffer()
{
	return 0;
}
int SysFs::FlushBuffer(SortedFileIoRec* pRec)
{
	return 0;
}
int SysFs::GetFileSize(uint* low,uint* high)
{
	return 0;
}
int SysFs::SetFileSize(uint low,uint high)
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