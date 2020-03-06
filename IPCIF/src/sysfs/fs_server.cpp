#define DLL_IMPORT
#include "common.h"
#include "sysfs_struct.h"
#include "interface.h"
#include "config_val_extern.h"
#include "utility.h"
#define cifproc if_info->ifproc
#define cstomod if_info->sto_mod
#define cstodrv if_info->sto_drv
#define chdev if_info->hdev
#define cdrvcall if_info->sto_drv->drvcall
DEFINE_UINT_VAL(fsserver_handle_pass,8);
DEFINE_BOOL_VAL(fsserver_try_format_on_mount_fail,false);
FssContainer g_fssrv;
bool less_servrec_ptr::operator()(const FileServerKey& a, const FileServerKey& b) const
{
	if(a.caller!=b.caller)
		return a.caller<b.caller;
	else
		return a.hFile<b.hFile;
}
bool equal_mod(storage_mod_info& a,storage_mod_info& b)
{
	return a.mod_name==b.mod_name;
}
bool equal_drv(storage_drv_info& a,storage_drv_info& b)
{
	return a.drv_name==b.drv_name;
}
template<class T>
T* find_in_vector(vector<T>& v,T& element,bool(*equal_func)(T&,T&))
{
	for(int i=0;i<(int)v.size();i++)
	{
		if(equal_func(v[i],element))
			return &v[i];
	}
	return NULL;
}
void* SrvProcRing::get_handle()
{
	void* h;
	do{
		h=(hFileReserve++);
		if(hFileReserve==(byte*)-1)
			hFileReserve=(byte*)1;
	}while(pfssrv->CheckExist(t.key.caller,h));
	return h;
}
bool ServerClearHandler(uint cmd,void* addr,void* param,int op)
{
	return g_fssrv.ReqHandler(cmd,addr,param,op);
}
int FssContainer::Init(vector<if_proc>* pif,RequestResolver* resolver)
{
	int ret=0;
	if(pif==NULL||resolver==NULL)
		return ERR_INVALID_PARAM;
	vector<if_proc>& ifs=*pif;
	if_id_info_storage ifstorage;
	for(int i=0;i<(int)ifs.size();i++)
	{
		if(ifs[i].usage!=CFG_IF_USAGE_STORAGE)
			continue;
		if(0!=(ret=get_if_storage_info((char*)ifs[i].id.c_str(),&ifstorage)))
			return ret;
		storage_mod_info dummy_mod,*psto_mod;
		dummy_mod.mod_name=ifstorage.mod_name;
		if(NULL==(psto_mod=find_in_vector(vfs_mod,dummy_mod,equal_mod)))
		{
			vfs_mod.push_back(storage_mod_info());
			psto_mod=&vfs_mod.back();
			psto_mod->hMod=NULL;
			psto_mod->STO_GET_INTF_FUNC=NULL;
			psto_mod->mod_name=ifstorage.mod_name;
		}
		storage_mod_info& sto_mod=*psto_mod;
		storage_drv_info dummy_drv,*psto_drv;
		dummy_drv.drv_name=ifstorage.drv_name;
		if(NULL==(psto_drv=find_in_vector(sto_mod.storage_drvs,dummy_drv,equal_drv)))
		{
			sto_mod.storage_drvs.push_back(storage_drv_info());
			psto_drv=&sto_mod.storage_drvs.back();
			psto_drv->drv_name=ifstorage.drv_name;
			psto_drv->drvcall=NULL;
			psto_drv->inited=false;
		}
		storage_drv_info& sto_drv=*psto_drv;
		if_info_storage dev_info;
		init_if_info_storage(&dev_info);
		dev_info.ifproc=&ifs[i];
		dev_info.mount_cmd=ifstorage.mount_cmd;
		dev_info.format_cmd=ifstorage.format_cmd;
		sto_drv.storage_devs.push_back(dev_info);
	}
	for(int i=0;i<(int)vfs_mod.size();i++)
	{
		for(int j=0;j<(int)vfs_mod[i].storage_drvs.size();j++)
		{
			for(int k=0;k<(int)vfs_mod[i].storage_drvs[j].storage_devs.size();k++)
			{
				vfs_mod[i].storage_drvs[j].storage_devs[k].sto_mod=&vfs_mod[i];
				vfs_mod[i].storage_drvs[j].storage_devs[k].sto_drv=&vfs_mod[i].storage_drvs[j];
			}
		}
	}
	resolver->AddHandler(ServerClearHandler);
	sem=sys_create_sem(fsserver_handle_pass,fsserver_handle_pass,NULL);
	if(!VALID(sem))
		return ERR_GENERIC;
	quitcode=0;
	for(int i=0;i<(int)vfs_mod.size();i++)
	{
		vfs_mod[i].hMod=sys_load_library((char*)vfs_mod[i].mod_name.c_str());
		if(!VALID(vfs_mod[i].hMod))
		{
			Exit();
			return ERR_GENERIC;
		}
		vfs_mod[i].STO_GET_INTF_FUNC=(pintf_fsdrv(*)(char*))sys_get_lib_proc(vfs_mod[i].hMod,STO_GET_INTF_FUNC_NAME);
		if(!VALID(vfs_mod[i].STO_GET_INTF_FUNC))
		{
			Exit();
			return ERR_GENERIC;
		}
		for(int j=0;j<(int)vfs_mod[i].storage_drvs.size();j++)
		{
			vfs_mod[i].storage_drvs[j].drvcall=vfs_mod[i].STO_GET_INTF_FUNC(
				(char*)vfs_mod[i].storage_drvs[j].drv_name.c_str());
			if(!VALID(vfs_mod[i].storage_drvs[j].drvcall))
			{
				Exit();
				return ERR_GENERIC;
			}
			if(0!=(ret=vfs_mod[i].storage_drvs[j].drvcall->init()))
			{
				Exit();
				return ret;
			}
			vfs_mod[i].storage_drvs[j].inited=true;
			for(int k=0;k<(int)vfs_mod[i].storage_drvs[j].storage_devs.size();k++)
			{
				FsServer* srv=new FsServer(&vfs_mod[i].storage_drvs[j].storage_devs[k],&sem,&quitcode);
				if(0!=(ret=srv->Init()))
				{
					delete srv;
					Exit();
					return ret;
				}
				vfs_srv.push_back(srv);
			}
		}
	}
	return 0;
}
void FssContainer::Exit()
{
	quitcode=ERR_MODULE_NOT_INITED;
	SuspendIO(false);
	for(int i=0;i<(int)vfs_srv.size();i++)
	{
		vfs_srv[i]->Exit();
		delete vfs_srv[i];
	}
	vfs_srv.clear();
	if(VALID(sem))
	{
		sys_close_sem(sem);
		sem=NULL;
	}
	for(int i=0;i<(int)vfs_mod.size();i++)
	{
		for(int j=0;j<(int)vfs_mod[i].storage_drvs.size();j++)
		{
			if(vfs_mod[i].storage_drvs[j].inited)
				vfs_mod[i].storage_drvs[j].drvcall->uninit();
		}
		if(VALID(vfs_mod[i].hMod))
			sys_free_library(vfs_mod[i].hMod);
	}
	vfs_mod.clear();
}
int FssContainer::SuspendIO(bool bsusp,uint time)
{
	if(bsusp)
	{
		if(locked)
			return 0;
		for(int i=0;i<(int)fsserver_handle_pass;i++)
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
		locked=true;
		return 0;
	}
	else
	{
		if(!locked)
			return 0;
		for(int i=0;i<(int)fsserver_handle_pass;i++)
		{
			sys_signal_sem(sem);
		}
		locked=false;
		return 0;
	}
	return 0;
}
bool FssContainer::ReqHandler(uint cmd,void* addr,void* param,int op)
{
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
	{
		SuspendIO(true);
		for(int i=0;i<(int)vfs_srv.size();i++)
		{
			if(vfs_srv[i]->Reset(dc==NULL?NULL:dc->clear.id))
				ret=true;
		}
		SuspendIO(false);
	}
	return ret;
}
int cb_fs_server(void* addr,void* param,int op);
int fs_server(void* param)
{
	FsServer* fssrvr=(FsServer*)param;
	while(*fssrvr->quitcode==0)
	{
		sys_wait_sem(*fssrvr->psem);
		if(*fssrvr->quitcode!=0)
		{
			sys_signal_sem(*fssrvr->psem);
			break;
		}
		listen_if(fssrvr->cifproc->hif,cb_fs_server,param,10);
		sys_signal_sem(*fssrvr->psem);
	}
	return 0;
}
int FsServer::Init()
{
	if(psem==NULL||!VALID(*psem))
		return ERR_GENERIC;
	if(quitcode==NULL)
		return ERR_GENERIC;
	if(if_info==NULL||cifproc==NULL||cstomod==NULL
		||cstodrv==NULL||cdrvcall==NULL)
		return ERR_GENERIC;
	if(!cstodrv->inited)
		return ERR_GENERIC;
	uint cmd_array[]={
		CMD_FSOPEN,
		CMD_FSCLOSE,
		CMD_FSREAD,
		CMD_FSWRITE,
		CMD_FSGETSIZE,
		CMD_FSSETSIZE,
		CMD_MAKEDIR,
		CMD_LSFILES,
		CMD_FSMOVE,
		CMD_FSDELETE,
		CMD_FSGETATTR,
		CMD_FSSETATTR,
	};
	uint cmd_data_size_array[]={
		sizeof(dg_fsopen)-sizeof(((dgc_fsopen*)NULL)->path),
		sizeof(dg_fsclose),
		sizeof(dg_fsrdwr),
		sizeof(dg_fsrdwr)-sizeof(((dgc_fsrdwr*)NULL)->buf),
		sizeof(dg_fssize),
		sizeof(dg_fssize),
		sizeof(datagram_base),//fsmkdir
		sizeof(dg_fslsfiles)-sizeof(((dgc_fslsfiles*)NULL)->path),
		sizeof(datagram_base),//fsmove
		sizeof(datagram_base),//fsdel
		sizeof(dg_fsattr)-sizeof(((dgc_fsattr*)NULL)->path),
		sizeof(datagram_base),//fsattr
	};
	assert(sizeof(cmd_array)/sizeof(uint)
		==sizeof(cmd_data_size_array)/sizeof(uint));
	for(int i=0;i<(int)(sizeof(cmd_array)/sizeof(uint));i++)
	{
		cmd_data_size_map[cmd_array[i]]=cmd_data_size_array[i];
	}
	int ret=0;
	if(0!=(ret=CreateInterface()))
		goto failed;
	if(0!=(ret=MountDev()))
		goto failed;
	hthrd_server=sys_create_thread(fs_server,this);
	if(!VALID(hthrd_server))
	{
		ret=ERR_GENERIC;
		goto failed;
	}
	return 0;
failed:
	if(VALID(chdev))
	{
		cdrvcall->unmount(chdev);
		chdev=NULL;
	}
	if(VALID(cifproc->hif))
	{
		close_if(cifproc->hif);
		cifproc->hif=NULL;
	}
	cmd_data_size_map.clear();
	return ret;
}
void FsServer::Exit()
{
	assert(VALID(hthrd_server));
	sys_wait_thread(hthrd_server);
	sys_close_thread(hthrd_server);
	hthrd_server=NULL;
	Clear(NULL,true);
	if(VALID(chdev))
	{
		cdrvcall->unmount(chdev);
		chdev=NULL;
	}
	if(VALID(cifproc->hif))
	{
		close_if(cifproc->hif);
		cifproc->hif=NULL;
	}
	cmd_data_size_map.clear();
}
inline void clear_node(BiRingNode<FileServerRec>* node,pintf_fsdrv if_drv,void* hdev)
{
	switch(node->t.type)
	{
	case FSSERVER_REC_TYPE_FILE_DESCRIPTOR:
		if(VALID(node->t.fd))
			if_drv->close(hdev,node->t.fd);
		break;
	case FSSERVER_REC_TYPE_FILE_LIST:
		delete node->t.pvfiles;
		break;
	}
}
inline void clear_ring(fs_key_map& smap,SrvProcRing* proc_ring,pintf_fsdrv if_drv,void* hdev)
{
	BiRingNode<FileServerRec>* node;
	while(NULL!=(node=proc_ring->GetNodeFromTail()))
	{
		clear_node(node,if_drv,hdev);
		fs_key_map::iterator it=smap.find(node->t.key);
		assert(it!=smap.end());
		assert(it->second==node);
		smap.erase(it);
		delete node;
	}
}
void FsServer::Clear(void* proc_id,bool cl_root)
{
	if(VALID(proc_id))
	{
		map<void*,SrvProcRing*>::iterator it=proc_id_map.find(proc_id);
		if(it==proc_id_map.end())
			return;
		SrvProcRing* ring=it->second;
		clear_ring(smap,ring,cdrvcall,chdev);
		if(cl_root)
		{
			proc_id_map.erase(it);
			delete ring;
		}
		return;
	}
	map<void*,SrvProcRing*>::iterator it;
	if(cl_root)
	{
		while((it=proc_id_map.begin())!=proc_id_map.end())
		{
			SrvProcRing* ring=it->second;
			clear_ring(smap,ring,cdrvcall,chdev);
			proc_id_map.erase(it);
			delete ring;
		}
	}
	else
	{
		for(it=proc_id_map.begin();it!=proc_id_map.end();it++)
		{
			SrvProcRing* ring=it->second;
			clear_ring(smap,ring,cdrvcall,chdev);
		}
	}
	assert(smap.empty());
}
bool FsServer::Reset(void* proc_id)
{
	bool exist=(VALID(proc_id)?(proc_id_map.find(proc_id)!=proc_id_map.end()
		&&!proc_id_map[proc_id]->Empty()):!smap.empty());
	Clear(proc_id);
	reset_if(cifproc->hif);
	return exist;
}
int FsServer::CreateInterface()
{
	int ret=0;
	if_initial init;
	init.user=get_if_user();
	init.id=(char*)cifproc->id.c_str();
	init.nthread=cifproc->cnt;
	init.smem_size=SIZE_IF_SYSFS;
	if(0!=(ret=setup_if(&init,&cifproc->hif)))
	{
		LOGFILE(0,log_ftype_error,"Create interfafe %s failed, quitting...",init.id);
		return ret;
	}
	return 0;
}
int FsServer::MountDev()
{
	int ret=0;
	if(0!=(ret=cdrvcall->mount((char*)if_info->mount_cmd.c_str(),&chdev)))
	{
		bool retry=fsserver_try_format_on_mount_fail
			&&ret==ERR_FS_DEV_MOUNT_FAILED_NOT_EXIST;
		LOGFILE(0,log_ftype_error,"Error message:%s",get_error_desc(ret));
		LOGFILE(0,log_ftype_error,"Mount fs on interface %s failed, %s",
			(char*)cifproc->id.c_str(),
			retry?"try formatting...":"quitting...");
		if(!retry)
			return ret;
		if(0!=(ret=cdrvcall->format((char*)if_info->format_cmd.c_str())))
		{
			LOGFILE(0,log_ftype_error,"Error message:%s",get_error_desc(ret));
			LOGFILE(0,log_ftype_error,"Format fs on interface %s failed, quitting...",
				(char*)cifproc->id.c_str());
			return ret;
		}
		LOGFILE(0,log_ftype_error,"Format fs on interface %s successful.",
			(char*)cifproc->id.c_str());
		if(0!=(ret=cdrvcall->mount((char*)if_info->mount_cmd.c_str(),&chdev)))
		{
			LOGFILE(0,log_ftype_error,"Error message:%s",get_error_desc(ret));
			LOGFILE(0,log_ftype_error,"Mount fs on interface %s still failed, quitting...",
				(char*)cifproc->id.c_str());
			return ret;
		}
	}
	return 0;
}
bool FsServer::RestoreData(datagram_base* data)
{
	if(!VALID(data->caller))
		return false;
	map<void*,SrvProcRing*>::iterator it;
	if((it=proc_id_map.find(data->caller))==proc_id_map.end())
		return false;
	SrvProcRing* ring=it->second;
	if(data->cmd==ring->GetBackupData()->header.cmd
		&&data->sid==ring->GetBackupData()->header.sid)
	{
		assert(cmd_data_size_map.find(data->cmd)!=cmd_data_size_map.end());
		memcpy(data,ring->GetBackupData(),cmd_data_size_map[data->cmd]);
		return true;
	}
	return false;
}
void FsServer::BackupData(datagram_base* data)
{
	if(data->ret!=0)
		return;
	map<void*,SrvProcRing*>::iterator it;
	if((it=proc_id_map.find(data->caller))==proc_id_map.end())
		return;
	SrvProcRing* ring=it->second;
	assert(cmd_data_size_map.find(data->cmd)!=cmd_data_size_map.end());
	memcpy(ring->GetBackupData(),data,cmd_data_size_map[data->cmd]);
}
bool FsServer::AddProcRing(void* proc_id)
{
	map<void*,SrvProcRing*>::iterator it;
	if((it=proc_id_map.find(proc_id))!=proc_id_map.end())
		return false;
	proc_id_map[proc_id]=new SrvProcRing(this,proc_id);
	return true;
}
int cb_fs_server(void* addr,void* param,int op)
{
	FsServer* srv=(FsServer*)param;
	datagram_base* dbase=(datagram_base*)addr;
	if(srv->RestoreData(dbase))
		return 0;
	srv->AddProcRing(dbase->caller);
	uint cmd=dbase->cmd;
	switch(cmd)
	{
	case CMD_FSOPEN:
		{
			dg_fsopen* fsopen=(dg_fsopen*)addr;
			srv->HandleOpen(fsopen);
		}
		break;
	case CMD_FSREAD:
	case CMD_FSWRITE:
		{
			dg_fsrdwr* fsrdwr=(dg_fsrdwr*)addr;
			srv->HandleReadWrite(fsrdwr);
		}
		break;
	case CMD_FSCLOSE:
		{
			dg_fsclose* fsclose=(dg_fsclose*)addr;
			srv->HandleClose(fsclose);
		}
		break;
	case CMD_FSGETSIZE:
	case CMD_FSSETSIZE:
		{
			dg_fssize* fssize=(dg_fssize*)addr;
			srv->HandleGetSetSize(fssize);
		}
		break;
	case CMD_FSMOVE:
		{
			dg_fsmove* fsmove=(dg_fsmove*)addr;
			srv->HandleMove(fsmove);
		}
		break;
	case CMD_FSDELETE:
		{
			dg_fsdel* fsdel=(dg_fsdel*)addr;
			srv->HandleDelete(fsdel);
		}
		break;
	case CMD_MAKEDIR:
		{
			dg_fsmkdir* fsmkdir=(dg_fsmkdir*)addr;
			srv->HandleMakeDir(fsmkdir);
		}
		break;
	case CMD_FSGETATTR:
	case CMD_FSSETATTR:
		{
			dg_fsattr* fsattr=(dg_fsattr*)addr;
			srv->HandleGetSetAttr(fsattr);
		}
		break;
	case CMD_LSFILES:
		{
			dg_fslsfiles* fslsfiles=(dg_fslsfiles*)addr;
			srv->HandleListFiles(fslsfiles);
		}
		break;
	}
	srv->BackupData(dbase);
	return 0;
}
BiRingNode<FileServerRec>* FsServer::get_fs_node(void* proc_id,void* h,fs_key_map::iterator* iter)
{
	FileServerKey key;
	key.caller=proc_id;
	key.hFile=h;
	fs_key_map::iterator it;
	if((it=smap.find(key))!=smap.end())
	{
		if(iter!=NULL)
			*iter=it;
		return it->second;
	}
	return NULL;
}
void* FsServer::AddNode(void* proc_id,FileServerRec* pRec)
{
	FileServerKey key;
	key.caller=proc_id;
	SrvProcRing* ring=proc_id_map[key.caller];
	void* h=ring->get_handle();
	key.hFile=h;
	BiRingNode<FileServerRec>* node=new BiRingNode<FileServerRec>;
	node->t.type=pRec->type;
	switch(node->t.type)
	{
	case FSSERVER_REC_TYPE_FILE_DESCRIPTOR:
		node->t.fd=pRec->fd;
		break;
	case FSSERVER_REC_TYPE_FILE_LIST:
		node->t.pvfiles=pRec->pvfiles;
		node->t.index=pRec->index;
		break;
	default:
		assert(false);
	}
	node->t.key=key;
	node->t.proc_ring=ring;
	ring->AddNodeToBegin(node);
	smap[key]=node;
	return h;
}
bool FsServer::RemoveNode(void* proc_id,void* h)
{
	fs_key_map::iterator it;
	BiRingNode<FileServerRec>* node=get_fs_node(
		proc_id,h,&it);
	if(node==NULL)
		return false;
	clear_node(node,cdrvcall,chdev);
	node->Detach();
	smap.erase(it);
	delete node;
	return true;
}
int FsServer::HandleOpen(dg_fsopen* fsopen)
{
	int ret=0;
	void* hfile=cdrvcall->open(chdev,fsopen->open.path,fsopen->open.flags);
	if(VALID(hfile))
	{
		FileServerRec rec;
		rec.type=FSSERVER_REC_TYPE_FILE_DESCRIPTOR;
		rec.fd=hfile;
		fsopen->open.hFile=AddNode(fsopen->header.caller,&rec);
	}
	else
	{
		ret=ERR_FILE_IO;
	}
	fsopen->header.ret=ret;
	return ret;
}
int FsServer::HandleClose(dg_fsclose* fsclose)
{
	int ret=0;
	if(!RemoveNode(fsclose->header.caller,fsclose->close.handle))
	{
		ret=ERR_FS_INVALID_HANDLE;
	}
	fsclose->header.ret=ret;
	return ret;
}
int FsServer::HandleReadWrite(dg_fsrdwr* fsrdwr)
{
	int ret=0;
	BiRingNode<FileServerRec>* node=get_fs_node(
		fsrdwr->header.caller,fsrdwr->rdwr.handle);
	if(node!=NULL)
	{
		uint rdwrlen=0;
		switch(fsrdwr->header.cmd)
		{
		case CMD_FSREAD:
			ret=cdrvcall->read(chdev,node->t.fd,
				fsrdwr->rdwr.offset,fsrdwr->rdwr.offhigh,
				fsrdwr->rdwr.len,fsrdwr->rdwr.buf,&rdwrlen);
			fsrdwr->rdwr.len=rdwrlen;
			break;
		case CMD_FSWRITE:
			ret=cdrvcall->write(chdev,node->t.fd,
				fsrdwr->rdwr.offset,fsrdwr->rdwr.offhigh,
				fsrdwr->rdwr.len,fsrdwr->rdwr.buf,&rdwrlen);
			fsrdwr->rdwr.len=rdwrlen;
			break;
		default:
			ret=ERR_FILE_IO;
			break;
		}
	}
	else
	{
		ret=ERR_FS_INVALID_HANDLE;
	}
	fsrdwr->header.ret=ret;
	return ret;
}
int FsServer::HandleGetSetSize(dg_fssize* fssize)
{
	int ret=0;
	BiRingNode<FileServerRec>* node=get_fs_node(
		fssize->header.caller,fssize->size.handle);
	if(node!=NULL)
	{
		switch(fssize->header.cmd)
		{
		case CMD_FSGETSIZE:
			ret=cdrvcall->getsize(chdev,node->t.fd,
				&fssize->size.len,&fssize->size.lenhigh);
			break;
		case CMD_FSSETSIZE:
			ret=cdrvcall->setsize(chdev,node->t.fd,
				fssize->size.len,fssize->size.lenhigh);
			break;
		default:
			ret=ERR_FILE_IO;
			break;
		}
	}
	else
	{
		ret=ERR_FS_INVALID_HANDLE;
	}
	fssize->header.ret=ret;
	return ret;
}
int FsServer::HandleMove(dg_fsmove* fsmove)
{
	int ret=cdrvcall->move(chdev,fsmove->move.src,fsmove->move.dst);
	fsmove->header.ret=ret;
	return ret;
}
int FsServer::HandleDelete(dg_fsdel* fsdel)
{
	int ret=cdrvcall->del(chdev,fsdel->del.path);
	fsdel->header.ret=ret;
	return ret;
}
int FsServer::HandleMakeDir(dg_fsmkdir* fsmkdir)
{
	int ret=cdrvcall->mkdir(chdev,fsmkdir->dir.path);
	fsmkdir->header.ret=ret;
	return ret;
}
int FsServer::HandleGetSetAttr(dg_fsattr* fsattr)
{
	int ret=0;
	switch(fsattr->header.cmd)
	{
	case CMD_FSGETATTR:
		if(fsattr->attr.mask&FS_ATTR_FLAGS)
			ret=cdrvcall->getattr(chdev,fsattr->attr.path,&fsattr->attr.flags);
		if(fsattr->attr.mask&FS_ATTR_DATE)
		{
			DateTime date[3];
			cdrvcall->getfiletime(chdev,fsattr->attr.path,fsattr->attr.mask
				&FS_ATTR_DATE,date);
			if(fsattr->attr.mask&FS_ATTR_CREATION_DATE)
				fsattr->attr.date[fs_attr_creation_date].date=date[fs_attr_creation_date];
			if(fsattr->attr.mask&FS_ATTR_MODIFY_DATE)
				fsattr->attr.date[fs_attr_modify_date].date=date[fs_attr_modify_date];
			if(fsattr->attr.mask&FS_ATTR_ACCESS_DATE)
				fsattr->attr.date[fs_attr_access_date].date=date[fs_attr_access_date];
		}
		break;
	case CMD_FSSETATTR:
		if(fsattr->attr.mask&FS_ATTR_FLAGS)
			ret=cdrvcall->setattr(chdev,fsattr->attr.path,fsattr->attr.flags);
		if(fsattr->attr.mask&FS_ATTR_DATE)
		{
			DateTime date[3];
			if(fsattr->attr.mask&FS_ATTR_CREATION_DATE)
				date[fs_attr_creation_date]=fsattr->attr.date[fs_attr_creation_date].date;
			if(fsattr->attr.mask&FS_ATTR_MODIFY_DATE)
				date[fs_attr_modify_date]=fsattr->attr.date[fs_attr_modify_date].date;
			if(fsattr->attr.mask&FS_ATTR_ACCESS_DATE)
				date[fs_attr_access_date]=fsattr->attr.date[fs_attr_access_date].date;
			cdrvcall->setfiletime(chdev,fsattr->attr.path,fsattr->attr.mask
				&FS_ATTR_DATE,date);
		}
		break;
	default:
		ret=ERR_FILE_IO;
		break;
	}
	fsattr->header.ret=ret;
	return ret;
}
int FsServer::HandleListFiles(dg_fslsfiles* fslsfiles)
{
	int ret=0;
	vector<fsls_element>* files;
	BiRingNode<FileServerRec>* node;
	fs_key_map::iterator it;
	int index=0;
	uint nfile;
	bool newlist;
	void* hls=fslsfiles->files.handle;
	if(!VALID(hls))
	{
		newlist=true;
		files=new vector<fsls_element>;
		if(0!=(ret=cdrvcall->listfiles(chdev,fslsfiles->files.path,files)))
		{
			delete files;
			goto end;
		}
	}
	else
	{
		newlist=false;
		node=get_fs_node(fslsfiles->header.caller,hls,&it);
		if(node!=NULL)
		{
			assert(node->t.type==FSSERVER_REC_TYPE_FILE_LIST);
			files=node->t.pvfiles;
			index=node->t.index;
		}
		else
		{
			ret=ERR_FS_INVALID_HANDLE;
			goto end;
		}
	}
	nfile=files->size()-(uint)index;
	for(int i=0;i<LSBUFFER_ELEMENTS&&index<(int)files->size();i++,index++)
	{
		strcpy(fslsfiles->files.file[i].name,(*files)[i].filename.c_str());
		fslsfiles->files.file[i].flags=(*files)[i].flags;
	}
	fslsfiles->files.nfiles=nfile;
	if(index==(int)files->size())
	{
		if(newlist)
			delete files;
		else
			RemoveNode(fslsfiles->header.caller,hls);
		fslsfiles->files.handle=NULL;
	}
	else
	{
		if(newlist)
		{
			FileServerRec rec;
			rec.type=FSSERVER_REC_TYPE_FILE_LIST;
			rec.pvfiles=files;
			rec.index=index;
			hls=AddNode(fslsfiles->header.caller,&rec);
		}
		else
			node->t.index=index;
		fslsfiles->files.handle=hls;
	}
end:
	fslsfiles->header.ret=ret;
	return ret;
}
