#define DLL_IMPORT
#include "common.h"
#include "sysfs_struct.h"
#include "interface.h"
#include "utility.h"
#include "config_val_extern.h"
#include "path.h"
#include "common_request.h"
#include "Integer64.h"
#include "datetime.h"
#include <assert.h>
#define sem_safe_release(sem) \
	if(VALID(sem)) \
	{ \
		sys_close_sem(sem); \
		sem=NULL; \
	}
#define uninited_return_val(v) if(quitcode!=0)return (v)
#define uninited_return if(quitcode!=0)return quitcode
DEFINE_UINT_VAL(use_storage_level,0);
DEFINE_UINT_VAL(sysfs_query_pass,4);
#define active_storage ifvproc[use_storage_level]
#define MAX_CONNECT_TIMES 10
inline SortedFileIoRec* get_from_fmap(map<void*,SortedFileIoRec*>& fmap,void* h,void* protect)
{
	map<void*,SortedFileIoRec*>::iterator it;
	SortedFileIoRec* pRec=NULL;
	sys_wait_sem(protect);
	if((it=fmap.find(h))!=fmap.end())
		pRec=it->second;
	sys_signal_sem(protect);
	return pRec;
}
inline void add_to_fmap(map<void*,SortedFileIoRec*>& fmap,void* h,SortedFileIoRec* pRec,void* protect)
{
	sys_wait_sem(protect);
	fmap[h]=pRec;
	sys_signal_sem(protect);
}
inline void remove_from_fmap(map<void*,SortedFileIoRec*>& fmap,void* h,void* protect)
{
	map<void*,SortedFileIoRec*>::iterator it;
	sys_wait_sem(protect);
	if((it=fmap.find(h))!=fmap.end())
		fmap.erase(it);
	sys_signal_sem(protect);
}
void* SysFs::sysfs_get_handle()
{
	static byte* i=(byte*)1;
	void* h;
	sys_wait_sem(fmap_protect);
	do{
		h=(i++);
		if(i==(byte*)-1)
			i=(byte*)1;
	}while(fmap.find(h)!=fmap.end());
	sys_signal_sem(fmap_protect);
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
LinearBuffer* SortedFileIoRec::get_buffer(offset64 off,bool get_oldest)
{
	uint index=0;
	BufferPtr bufptr;
	map<offset64,BufferPtr,less_buf_ptr>::iterator it;
	if(get_oldest)
		goto remove;
	if((it=map_buf.find(off))!=map_buf.end())
	{
		BufferPtr tmpptr=it->second;
		map_buf.erase(it);
		verify(sorted_buf.RemoveMin(bufptr,tmpptr.buffer->heap_index));
		assert(tmpptr.buffer==bufptr.buffer);
		return tmpptr.buffer;
	}
	if(!free_buf.empty())
	{
		LinearBuffer* buf=free_buf.back().buffer;
		free_buf.pop_back();
		return buf;
	}
remove:
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
void SortedFileIoRec::add_buffer(LinearBuffer* buf,bool add_to_free,bool update_seq)
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
		verify(!sorted_buf.Add(bptr,ovbptr));
		offset64 off;
		off.off=buf->offset;
		off.offhigh=buf->offhigh;
		map_buf[off]=bptr;
	}
}
bool ClearHandler(uint cmd,void* addr,void* param,int op);
int SysFs::cb_reconn(void* param)
{
	SysFs* sysfs=(SysFs*)param;
	do
	{
		sys_wait_sem(sysfs->sem_reconn);
		bool pass;
		do
		{
			if(sysfs->quitcode!=0)
				break;
			pass=true;
			for(int i=0;i<(int)sysfs->ifvproc.size();i++)
			{
				if(VALID(sysfs->ifvproc[i]->hif))
					continue;
				if(0!=sysfs->ConnectServer(sysfs->ifvproc[i],&sysfs->ifvproc[i]->hif))
					pass=false;
				if(sysfs->quitcode!=0)
					break;
			}
			if(sysfs->quitcode!=0)
				break;
		}while(!pass);
		sysfs->SuspendIO(false,0,FC_CLEAR);
	}while(sysfs->quitcode==0);
	//here we want to re-unblock io to ensure that deep-level(2) lockcnt is released.
	sysfs->SuspendIO(false,0,FC_CLEAR);
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
	bool b;
	if(pblk!=NULL)
	{
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
	fmap_protect=sys_create_sem(1,1,NULL);
	b=(mode==fsmode_semipermanent_if);
	if(b)
	{
		sem_reconn=sys_create_sem(0,1,NULL);
		flag_protect=sys_create_sem(1,1,NULL);
	}
	if((!VALID(sem))||(!VALID(fmap_protect))||(b&&(!VALID(sem_reconn)||!VALID(flag_protect))))
	{
		ret=ERR_SEM_CREATE_FAILED;
		goto failed;
	}
	flags=0;
	quitcode=0;
	if(b)
	{
		hthrd_reconn=sys_create_thread(cb_reconn,this);
		if(!VALID(hthrd_reconn))
		{
			ret=ERR_THREAD_CREATE_FAILED;
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
	sem_safe_release(sem);
	sem_safe_release(sem_reconn);
	sem_safe_release(flag_protect);
	sem_safe_release(fmap_protect);
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
	sys_sleep(10);
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
	sem_safe_release(sem);
	sem_safe_release(sem_reconn);
	sem_safe_release(flag_protect);
	sem_safe_release(fmap_protect);
	mutex=NULL;
}
int SysFs::SuspendIO(bool bsusp,uint time,dword cause)
{
	dword flag;
	if(VALID(flag_protect))
		sys_wait_sem(flag_protect);
	flag=flags;
	if(cause==FC_EXIT)
	{
		if(bsusp)
			flags|=cause;
		else
			flags&=~cause;
	}
	else if(cause==FC_CLEAR)
	{
		int oldcnt=((flags&FC_CLEAR)>>FC_CLEAR_OFF);
		int cnt=oldcnt+(bsusp?1:-1);
		cnt=(cnt<0?0:(cnt>2?2:cnt));
		flags=((flags&~FC_CLEAR)|(((dword)cnt))<<FC_CLEAR_OFF);
	}
	if(VALID(flag_protect))
		sys_signal_sem(flag_protect);
	if((bsusp&&(flag&FC_MASK)!=0)
		||((!bsusp)&&((flag&FC_MASK)==0
		||(cause==FC_EXIT?(flag&~cause)!=0:flag!=(1<<FC_CLEAR_OFF)))))
		return 0;
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
	if(once)
	{
		return connect_if(&init,phif);
	}
	for(int i=0;i<MAX_CONNECT_TIMES;i++)
	{
		if(0==connect_if(&init,phif))
		{
			return 0;
		}
		sys_sleep(200);
	}
	LOGFILE(0,log_ftype_error,"Connecting to interface %s failed, try reconnecting...",init.id);
	char msg[256];
	sprintf(msg,"Connect to interface %s failed",init.id);
	sys_show_message(msg);
	return ERR_IF_CONN_FAILED;
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
	data.hproc=NULL;
	data.hthrd_shelter=NULL;
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
int SysFs::fs_parse_path(if_proc** ppif,string& path,const string& in_path)
{
	vector<string> split_in_path,split_out_path;
	split_path(in_path,split_in_path,'/');
	int ret=0;
	if_proc* ifproc;
	if((!split_in_path.empty())
		&&(!split_in_path[0].empty())
		&&split_in_path[0].back()==':')
	{
		if((!in_path.empty())&&in_path.front()=='/')
			return ERR_INVALID_PATH;
		string if_id=split_in_path[0].substr(0,split_in_path[0].size()-1);
		if(if_id.empty())
			return ERR_INVALID_PATH;
		if(NULL==(ifproc=GetIfProcFromID(if_id)))
			return ERR_INVALID_PATH;
		split_in_path.erase(split_in_path.begin());
	}
	else
	{
		if(NULL==(ifproc=GetIfProcFromID("")))
			return ERR_INVALID_PATH;
	}
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
		mutex->get_lock(true);
		if(quitcode!=0||!VALID(pif->hif))
		{
			mutex->get_lock(false);
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
		mutex->get_lock(false);
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
			if(op&OP_RETURN&&dgp->dbase->ret==0)
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
	case CMD_FSCLOSE:
		{
			dg_fsclose* fsclose=(dg_fsclose*)addr;
			if(op&OP_PARAM)
				fsclose->close=dgp->fsclose;
		}
		break;
	case CMD_FSGETSIZE:
	case CMD_FSSETSIZE:
		{
			dg_fssize* fssize=(dg_fssize*)addr;
			if(op&OP_PARAM)
				fssize->size=*dgp->fssize;
			if((op&OP_RETURN)&&cmd==CMD_FSGETSIZE&&dgp->dbase->ret==0)
				*dgp->fssize=fssize->size;
		}
		break;
	case CMD_FSMOVE:
		{
			dg_fsmove* fsmove=(dg_fsmove*)addr;
			if(op&OP_PARAM)
			{
				strcpy(fsmove->move.src,dgp->fsmove.src->c_str());
				strcpy(fsmove->move.dst,dgp->fsmove.dst->c_str());
			}
		}
		break;
	case CMD_FSDELETE:
		{
			dg_fsdel* fsdel=(dg_fsdel*)addr;
			if(op&OP_PARAM)
				strcpy(fsdel->del.path,dgp->fsdelete.path->c_str());
		}
		break;
	case CMD_MAKEDIR:
		{
			dg_fsmkdir* fsmkdir=(dg_fsmkdir*)addr;
			if(op&OP_PARAM)
				strcpy(fsmkdir->dir.path,dgp->fsmkdir.path->c_str());
		}
		break;
	case CMD_FSGETATTR:
	case CMD_FSSETATTR:
		{
			dg_fsattr* fsattr=(dg_fsattr*)addr;
			if(op&OP_PARAM)
			{
				strcpy(fsattr->attr.path,dgp->fsattr.path->c_str());
				fsattr->attr.mask=dgp->fsattr.mask;
			}
			if((op&OP_PARAM)&&cmd==CMD_FSSETATTR)
			{
				fsattr->attr.flags=*dgp->fsattr.flags;
				for(int i=0;i<3;i++)
					fsattr->attr.date[i].date=dgp->fsattr.date[i];
			}
			if((op&OP_RETURN)&&cmd==CMD_FSGETATTR&&dgp->dbase->ret==0)
			{
				*dgp->fsattr.flags=fsattr->attr.flags;
				for(int i=0;i<3;i++)
					dgp->fsattr.date[i]=fsattr->attr.date[i].date;
			}
		}
		break;
	case CMD_LSFILES:
		{
			dg_fslsfiles* fslsfiles=(dg_fslsfiles*)addr;
			if(op&OP_PARAM)
			{
				fslsfiles->files.handle=*dgp->fslsfiles.handle;
				fslsfiles->files.nfiles=*dgp->fslsfiles.nfiles;
				strcpy(fslsfiles->files.path,!VALID(*dgp->fslsfiles.handle)?
					dgp->fslsfiles.path->c_str():"");
			}
			if((op&OP_RETURN)&&dgp->dbase->ret==0)
			{
				*dgp->fslsfiles.handle=fslsfiles->files.handle;
				*dgp->fslsfiles.nfiles=fslsfiles->files.nfiles;
				int i;
				for(i=0;i<LSBUFFER_ELEMENTS&&*dgp->fslsfiles.nfiles>0;i++,(*dgp->fslsfiles.nfiles)--)
				{
					fsls_element fsls;
					fsls.filename=fslsfiles->files.file[i].name;
					fsls.flags=fslsfiles->files.file[i].flags;
					dgp->fslsfiles.files->push_back(fsls);
				}
			}
		}
		break;
		case CMD_FSGETDEVINFO:
		{
			dg_fsdevinfo* fsdevinfo=(dg_fsdevinfo*)addr;
			if(op&OP_PARAM)
			{
				strcpy(fsdevinfo->info.devname,dgp->fsdevinfo.devname->c_str());
				*fsdevinfo->info.devtype=0;
			}
			if((op&OP_RETURN)&&dgp->dbase->ret==0)
			{
				dgp->fsdevinfo.devinfo->devtype=fsdevinfo->info.devtype;
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
	uninited_return_val(NULL);
	if_proc* ifproc;
	string path;
	int ret=0;
	void* h;
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
	h=sysfs_get_handle();
	add_to_fmap(fmap,h,pRec,fmap_protect);
	return h;
failed:
	delete pRec;
	return NULL;
}
SortedFileIoRec* SysFs::handle_to_rec_ptr(void* handle)
{
	if(!VALID(handle))
		return NULL;
	return get_from_fmap(fmap,handle,fmap_protect);
}
int SysFs::Close(void* h)
{
	uninited_return;
	SortedFileIoRec* pRec=handle_to_rec_ptr(h);
	if(pRec==NULL)
		return ERR_FS_INVALID_HANDLE;
	int ret=0;
	if(0!=(ret=FlushBuffer(pRec)))
		return ret;
	ret=CloseHandle(pRec->hFile,pRec->pif);
	if(ret!=0&&ret!=ERR_FS_INVALID_HANDLE)
		return ret;
	remove_from_fmap(fmap,h,fmap_protect);
	delete pRec;
	return ret;
}
int SysFs::CloseHandle(void* h,if_proc* pif)
{
	int ret=0;
	void* hif;
	if(0!=(ret=BeginTransfer(pif,&hif)))
		return ret;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_FSCLOSE);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsclose.handle=h;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	return ret;
}
int SysFs::Seek(void* h,uint seektype,int offset,int* offhigh)
{
	uninited_return;
	SortedFileIoRec* pRec=handle_to_rec_ptr(h);
	if(pRec==NULL)
		return ERR_FS_INVALID_HANDLE;
	Integer64 off(pRec->offset,(int*)&pRec->offhigh),
		setoff(offset,offhigh);
	int ret=0;
	switch(seektype)
	{
	case SEEK_BEGIN:
		if(setoff<Integer64(0))
			return ERR_FS_NEGATIVE_POSITION;
		pRec->offset=setoff.low;
		pRec->offhigh=(uint)setoff.high;
		break;
	case SEEK_CUR:
		off+=setoff;
		if(off<Integer64(0))
			return ERR_FS_NEGATIVE_POSITION;
		pRec->offset=off.low;
		pRec->offhigh=(uint)off.high;
		break;
	case SEEK_END:
		if(0!=(ret=GetSetFileSize(CMD_FSGETSIZE,pRec,&off.low,(uint*)&off.high)))
			return ret;
		off+=setoff;
		if(off<Integer64(0))
			return ERR_FS_NEGATIVE_POSITION;
		pRec->offset=off.low;
		pRec->offhigh=(uint)off.high;
		break;
	default:
		return ERR_INVALID_PARAM;
	}
	return 0;
}
int SysFs::GetPosition(void* h,uint* offset,uint* offhigh)
{
	uninited_return;
	SortedFileIoRec* pRec=handle_to_rec_ptr(h);
	if(pRec==NULL)
		return ERR_FS_INVALID_HANDLE;
	if(offhigh==NULL&&pRec->offhigh>0)
		return ERR_INVALID_PARAM;
	*offset=pRec->offset;
	if(offhigh!=NULL)
		*offhigh=pRec->offhigh;
	return 0;
}
static inline int check_access_rights(if_cmd_code cmd,SortedFileIoRec* pRec)
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
	uninited_return;
	assert(cmd==CMD_FSREAD||cmd==CMD_FSWRITE);
	SortedFileIoRec* pRec=handle_to_rec_ptr(h);
	if(pRec==NULL)
		return ERR_FS_INVALID_HANDLE;
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
	pos+=UInteger64(done,&dummy);
	assert((pos.high&(1<<31))==0);
	pRec->offset=pos.low;
	pRec->offhigh=pos.high;
	if(rdwrlen!=NULL)
		*rdwrlen=done;
	else if(done!=len)
		return ret==0?ERR_INVALID_PARAM:ret;
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
	const uint size_if_buf=sizeof(((dgc_fsrdwr*)NULL)->buf);
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
		EndTransfer(&hif);
		sys_sleep(200);
		if(0!=(ret=BeginTransfer(pRec->pif,&hif)))
			return ret;
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
		off+=UInteger64(transfer_once,&dummy);
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
int SysFs::FlushBuffer(void* h)
{
	uninited_return;
	SortedFileIoRec* pRec=handle_to_rec_ptr(h);
	if(pRec==NULL)
		return ERR_FS_INVALID_HANDLE;
	return FlushBuffer(pRec);
}
int SysFs::FlushBuffer(SortedFileIoRec* pRec)
{
	int ret=0;
	offset64 dummy;
	dummy.off=(uint)-1;
	dummy.offhigh=(uint)-1;
	LinearBuffer* pLB;
	while(NULL!=(pLB=pRec->get_buffer(dummy,true)))
	{
		assert(pLB->valid);
		if(pLB->dirty)
		{
			//write buffer
			if(0!=(ret=IOBuf(CMD_FSWRITE,pRec,pLB)))
			{
				pRec->add_buffer(pLB,false,true);
				return ret;
			}
		}
		pRec->add_buffer(pLB,true,false);
	}
	return 0;
}
int SysFs::GetSetFileSize(if_cmd_code cmd,void* h,uint* low,uint* high)
{
	uninited_return;
	SortedFileIoRec* pRec=handle_to_rec_ptr(h);
	if(pRec==NULL)
		return ERR_FS_INVALID_HANDLE;
	return GetSetFileSize(cmd,pRec,low,high);
}
int SysFs::GetSetFileSize(if_cmd_code cmd,SortedFileIoRec*pRec,uint* low,uint* high)
{
	assert(cmd==CMD_FSGETSIZE||cmd==CMD_FSSETSIZE);
	int ret=0;
	if(0!=(ret=check_access_rights(cmd,pRec)))
		return ret;
	if(cmd==CMD_FSSETSIZE)
	{
		if(0!=(ret=FlushBuffer(pRec)))
			return ret;
	}
	dgc_fssize ds;
	ds.handle=pRec->hFile;
	ds.len=(cmd==CMD_FSGETSIZE?0:*low);
	ds.lenhigh=(cmd==CMD_FSGETSIZE?0:(high==NULL?0:*high));
	datagram_base dg;
	init_current_datagram_base(&dg,cmd);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fssize=&ds;
	void* hif;
	if(0!=(ret=BeginTransfer(pRec->pif,&hif)))
		return ret;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	if(ret!=ERR_FS_INVALID_HANDLE)
		goto end2;
	EndTransfer(&hif);
	sys_sleep(200);
	if(0!=(ret=BeginTransfer(pRec->pif,&hif)))
		return ret;
	if(0!=(ret=ReOpen(pRec,hif)))
		goto end2;
	init_current_datagram_base(&dg,cmd);
	ds.handle=pRec->hFile;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end2;
	ret=dg.ret;
end2:
	EndTransfer(&hif);
	if(cmd==CMD_FSGETSIZE&&ret==0)
	{
		UInteger64 size64(ds.len,&ds.lenhigh);
		uint dummy=0;
		for(Heap<BufferPtr,assign_buf_ptr,swap_buf_ptr>::iterator it=pRec->get_iter();it;it++)
		{
			UInteger64 end_buf(it->buffer->offset,&it->buffer->offhigh);
			end_buf+=UInteger64(it->buffer->end,&dummy);
			if(size64<end_buf)
				size64=end_buf;
		}
		assert((size64.high&(1<<31))==0);
		if(high==NULL&&size64.high>0)
			return ERR_INVALID_PARAM;
		*low=size64.low;
		if(high!=NULL)
			*high=size64.high;
	}
	return ret;
}
int SysFs::MoveFile(const char* src,const char* dst)
{
	uninited_return;
	int ret=0;
	if_proc *ifsrc,*ifdst;
	string puresrc,puredst;
	if(0!=(ret=fs_parse_path(&ifsrc,puresrc,src)))
		return ret;
	if(0!=(ret=fs_parse_path(&ifdst,puredst,dst)))
		return ret;
	if(ifsrc!=ifdst)
	{
		if(0!=(ret=CopyFile(src,dst)))
			return ret;
		if(0!=(ret=DeleteFile(src)))
			return ret;
		return 0;
	}
	if(puresrc==puredst)
		return 0;
	if_proc* pif=ifsrc;
	void* hif;
	if(0!=(ret=BeginTransfer(pif,&hif)))
		return ret;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_FSMOVE);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsmove.src=&puresrc;
	param.fsmove.dst=&puredst;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	return ret;
}
int SysFs::CopyFile(const char* src,const char* dst)
{
	uninited_return;
	int ret=0;
	if_proc *ifsrc,*ifdst;
	string puresrc,puredst;
	const uint bufsize=8*_1K;
	UInteger64 srclen;
	uint dummy=0;
	const UInteger64 once(bufsize,&dummy);
	if(0!=(ret=fs_parse_path(&ifsrc,puresrc,src)))
		return ret;
	if(0!=(ret=fs_parse_path(&ifdst,puredst,dst)))
		return ret;
	dword sflags=0,dflags=0;
	DateTime date[3];
	if(0!=(ret=GetSetFileAttr(CMD_FSGETATTR,src,
		FS_ATTR_FLAGS|FS_ATTR_CREATION_DATE
		|FS_ATTR_MODIFY_DATE|FS_ATTR_ACCESS_DATE,
		&sflags,date)))
		return ret;
	if(ifsrc==ifdst&&puresrc==puredst)
		return 0;
	int dret=GetSetFileAttr(CMD_FSGETATTR,src,FS_ATTR_FLAGS,&dflags);
	if(dret!=0&&dret!=ERR_FS_FILE_NOT_EXIST)
		return dret;
	if(FS_IS_DIR(sflags))
	{
		if(dret!=0)
		{
			if(0!=(ret=MakeDir(dst)))
				return ret;
			GetSetFileAttr(CMD_FSSETATTR,dst,
				FS_ATTR_FLAGS|FS_ATTR_CREATION_DATE
				|FS_ATTR_MODIFY_DATE|FS_ATTR_ACCESS_DATE,
				&sflags,date);
			return ret;
		}
		else if(!FS_IS_DIR(dflags))
			return ERR_FILE_IO;
		return 0;
	}
	else if(dret==0&&FS_IS_DIR(dflags))
		return ERR_FILE_IO;
	void* hsrc=Open(src,FILE_OPEN_EXISTING|FILE_READ|FILE_WRITE);
	void* hdst=Open(dst,FILE_CREATE_ALWAYS|FILE_READ|FILE_WRITE);
	byte* buf;
	if(!VALID(hsrc)||!VALID(hdst))
	{
		ret=ERR_FILE_IO;
		goto end;
	}
	buf=new byte[bufsize];
	if(0!=(ret=(GetSetFileSize(CMD_FSGETSIZE,hsrc,&srclen.low,&srclen.high))))
		goto end2;
	while(srclen>UInteger64(0))
	{
		UInteger64 len64=(srclen>once?once:srclen);
		assert(len64.high==0);
		uint len=len64.low;
		uint rdwr=0;
		if(0!=(ret=ReadWrite(CMD_FSREAD,hsrc,buf,len,&rdwr)))
			break;
		if(rdwr!=len)
		{
			ret=ERR_FILE_IO;
			break;
		}
		rdwr=0;
		if(0!=(ret=ReadWrite(CMD_FSWRITE,hdst,buf,len,&rdwr)))
			break;
		if(rdwr!=len)
		{
			ret=ERR_FILE_IO;
			break;
		}
		srclen-=len64;
	}
end2:
	delete[] buf;
end:
	if(VALID(hsrc))
	{
		int retc=__fs_perm_close(hsrc);
		if(ret==0)
			ret=retc;
	}
	if(VALID(hdst))
	{
		int retc=__fs_perm_close(hdst);
		if(ret==0)
			ret=retc;
		if(ret!=0)
			DeleteFile(dst);
	}
	if(ret!=0)
		goto no_stat_end;
	GetSetFileAttr(CMD_FSSETATTR,dst,
		FS_ATTR_FLAGS|FS_ATTR_CREATION_DATE
		|FS_ATTR_MODIFY_DATE|FS_ATTR_ACCESS_DATE,
		&sflags,date);
no_stat_end:
	return ret;
}
int SysFs::DeleteFile(const char* pathname)
{
	uninited_return;
	int ret=0;
	if_proc* ifpath;
	string purepath;
	if(0!=(ret=fs_parse_path(&ifpath,purepath,pathname)))
		return ret;
	void* hif;
	if(0!=(ret=BeginTransfer(ifpath,&hif)))
		return ret;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_FSDELETE);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsdelete.path=&purepath;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	return ret;
}
int SysFs::GetSetFileAttr(if_cmd_code cmd,const char* path,dword mask,dword* flags,DateTime* datetime)
{
	uninited_return;
	assert(cmd==CMD_FSGETATTR||cmd==CMD_FSSETATTR);
	int ret=0;
	if_proc* ifpath;
	string purepath;
	if(0!=(ret=fs_parse_path(&ifpath,purepath,path)))
		return ret;
	void* hif;
	if(0!=(ret=BeginTransfer(ifpath,&hif)))
		return ret;
	dword tflags=(cmd==CMD_FSGETATTR?0:((mask&FS_ATTR_FLAGS)&&flags!=NULL?*flags:0));
	DateTime tdate[3];
	CDateTime date;
	for(int i=0;i<3;i++)
		tdate[i]=*(DateTime*)&date;
	if(cmd==CMD_FSSETATTR&&datetime!=NULL)
	{
		if(mask&FS_ATTR_CREATION_DATE)
		{
			tdate[fs_attr_creation_date]=datetime[fs_attr_creation_date];
			CDateTime validate(tdate[fs_attr_creation_date]);
			if(!validate.ValidDate())
				return ERR_INVALID_PARAM;
		}
		if(mask&FS_ATTR_MODIFY_DATE)
		{
			tdate[fs_attr_modify_date]=datetime[fs_attr_modify_date];
			CDateTime validate(tdate[fs_attr_modify_date]);
			if(!validate.ValidDate())
				return ERR_INVALID_PARAM;
		}
		if(mask&FS_ATTR_ACCESS_DATE)
		{
			tdate[fs_attr_access_date]=datetime[fs_attr_access_date];
			CDateTime validate(tdate[fs_attr_access_date]);
			if(!validate.ValidDate())
				return ERR_INVALID_PARAM;
		}
	}
	datagram_base dg;
	init_current_datagram_base(&dg,cmd);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsattr.path=&purepath;
	param.fsattr.mask=mask;
	param.fsattr.flags=&tflags;
	param.fsattr.date=tdate;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	if(ret==0&&cmd==CMD_FSGETATTR)
	{
		if((mask&FS_ATTR_FLAGS)&&flags!=NULL)
			*flags=tflags;
		if(datetime!=NULL)
		{
			if(mask&FS_ATTR_CREATION_DATE)
				datetime[fs_attr_creation_date]=tdate[fs_attr_creation_date];
			if(mask&FS_ATTR_MODIFY_DATE)
				datetime[fs_attr_modify_date]=tdate[fs_attr_modify_date];
			if(mask&FS_ATTR_ACCESS_DATE)
				datetime[fs_attr_access_date]=tdate[fs_attr_access_date];
		}
	}
	return ret;
}
int SysFs::ListFile(const char* path,vector<fsls_element>& files)
{
	uninited_return;
	int ret=0;
	if_proc* ifpath;
	string purepath;
	if(0!=(ret=fs_parse_path(&ifpath,purepath,path)))
		return ret;
	files.clear();
	uint nfile=0;
	void* hls=NULL;
	void* hif;
	if(0!=(ret=BeginTransfer(ifpath,&hif)))
		return ret;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_LSFILES);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fslsfiles.path=&purepath;
	param.fslsfiles.files=&files;
	param.fslsfiles.nfiles=&nfile;
	param.fslsfiles.handle=&hls;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
	while(ret==0&&nfile>0)
	{
		init_current_datagram_base(&dg,CMD_LSFILES);
		if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
			break;
		ret=dg.ret;
	}
end:
	EndTransfer(&hif);
	if(ret!=0&&VALID(hls))
	{
		int retc;
		for(int t=10;t>0&&0!=(retc=CloseHandle(hls,ifpath));t--)
		{
			if(retc==ERR_FS_INVALID_HANDLE)
				break;
			sys_sleep(100);
		}
	}
	return ret;
}
int SysFs::ListDev(vector<string>& devlist,uint* defdev)
{
	uninited_return;
	devlist.clear();
	for(int i=0;i<(int)ifvproc.size();i++)
	{
		devlist.push_back(ifvproc[i]->id);
	}
	if(defdev!=NULL)
		*defdev=use_storage_level;
	return 0;
}
int SysFs::GetDevInfo(const string& devname,fs_dev_info& devinfo)
{
	uninited_return;
	int ret=0;
	fs_dev_info dinfo;
	if_proc* ifpath=GetIfProcFromID(devname);
	if(ifpath==NULL)
		return ERR_INVALID_PATH;
	void* hif;
	if(0!=(ret=BeginTransfer(ifpath,&hif)))
		return ret;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_FSGETDEVINFO);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsdevinfo.devname=&devname;
	param.fsdevinfo.devinfo=&dinfo;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	if(ret==0)
		devinfo=dinfo;
	return ret;
}
int SysFs::MakeDir(const char* path)
{
	uninited_return;
	int ret=0;
	if_proc* ifpath;
	string purepath;
	if(0!=(ret=fs_parse_path(&ifpath,purepath,path)))
		return ret;
	void* hif;
	if(0!=(ret=BeginTransfer(ifpath,&hif)))
		return ret;
	datagram_base dg;
	init_current_datagram_base(&dg,CMD_MAKEDIR);
	fs_datagram_param param;
	param.dbase=&dg;
	param.fsmkdir.path=&purepath;
	if(0!=(ret=send_request_no_reset(hif,cb_fsc,&param)))
		goto end;
	ret=dg.ret;
end:
	EndTransfer(&hif);
	return ret;
}
int __fs_perm_close(void* handle)
{
	if(!VALID(handle))
	{
		return ERR_FS_INVALID_HANDLE;
	}
	int ret;
	for(int t=20;t>0&&0!=(ret=g_sysfs.Close(handle));t--)
	{
		if(ret==ERR_FS_INVALID_HANDLE)
			break;
		sys_sleep(100);
	}
	return ret;
}
#define RETRY_TIMES 20
#define DELAY_MILLISEC 100
int cb_fs_traverse(char* pathname,int(*cb)(char*,dword,void*,char),void* param)
{
	vector<fsls_element> vfile;
	int ret=0;
	for(int i=0;i<RETRY_TIMES;i++)
	{
		if(0==(ret=g_sysfs.ListFile(pathname,vfile)))
			break;
		sys_sleep(DELAY_MILLISEC);
	}
	if(ret!=0)
		return ret;
	for(int i=0;i<(int)vfile.size();i++)
	{
		if(0!=cb((char*)vfile[i].filename.c_str(),FS_IS_DIR(vfile[i].flags)?FILE_TYPE_DIR:FILE_TYPE_NORMAL,param,'/'))
			ret=ERR_FILE_IO;
	}
	return ret;
}
int cb_fs_stat(char* pathname,dword* type)
{
	int ret=0;
	dword flags=0;
	for(int i=0;i<RETRY_TIMES;i++)
	{
		ret=g_sysfs.GetSetFileAttr(CMD_FSGETATTR,pathname,FS_ATTR_FLAGS,&flags);
		if(ret==0||ret==ERR_FS_FILE_NOT_EXIST)
			break;
		sys_sleep(DELAY_MILLISEC);
	}
	if(ret!=0)
		return ret;
	if(type!=NULL)
		*type=(FS_IS_DIR(flags)?FILE_TYPE_DIR:FILE_TYPE_NORMAL);
	return 0;
}
int cb_fs_mkdir(char* path)
{
	int ret=0;
	for(int i=0;i<RETRY_TIMES;i++)
	{
		if(0==(ret=g_sysfs.MakeDir(path)))
			break;
		sys_sleep(DELAY_MILLISEC);
	}
	return ret;
}
int cb_fs_copy(char* from,char* to)
{
	int ret=0;
	for(int i=0;i<RETRY_TIMES;i++)
	{
		if(0==(ret=g_sysfs.CopyFile(from,to)))
			break;
		sys_sleep(DELAY_MILLISEC);
	}
	return ret;
}
int cb_fs_delete(char* pathname)
{
	int ret=0;
	for(int i=0;i<RETRY_TIMES;i++)
	{
		if(0==(ret=g_sysfs.DeleteFile(pathname)))
			break;
		sys_sleep(DELAY_MILLISEC);
	}
	return ret;
}
int __fs_recurse_copy(char* from,char* to)
{
	file_recurse_callback cb={cb_fs_stat,cb_fs_traverse,cb_fs_mkdir,cb_fs_copy,cb_fs_delete};
	return recurse_fcopy(from,to,&cb,'/');
}
int __fs_recurse_delete(char* pathname)
{
	file_recurse_callback cb={cb_fs_stat,cb_fs_traverse,cb_fs_mkdir,cb_fs_copy,cb_fs_delete};
	return recurse_fdelete(pathname,&cb,'/');
}
