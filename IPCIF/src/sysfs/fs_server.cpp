#define DLL_IMPORT
#include "common.h"
#include "sysfs_struct.h"
#include "interface.h"
#include "config_val_extern.h"
#include "utility.h"
#define cifproc if_info->ifproc
#define cstomod if_info->sto_mod
#define chdev if_info->hdev
#define cdrvcall if_info->drvcall
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
		if_info_storage drv_info;
		init_if_info_storage(&drv_info);
		drv_info.ifproc=&ifs[i];
		drv_info.drv_name=ifstorage.drv_name;
		drv_info.mount_cmd=ifstorage.mount_cmd;
		drv_info.format_cmd=ifstorage.format_cmd;
		sto_mod.storage_drvs.push_back(drv_info);
	}
	for(int i=0;i<(int)vfs_mod.size();i++)
	{
		for(int j=0;j<(int)vfs_mod[i].storage_drvs.size();j++)
		{
			vfs_mod[i].storage_drvs[j].sto_mod=&vfs_mod[i];
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
			FsServer* srv=new FsServer(&vfs_mod[i].storage_drvs[j],&sem,&quitcode);
			if(0!=(ret=srv->Init()))
			{
				delete srv;
				Exit();
				return ret;
			}
			vfs_srv.push_back(srv);
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
		listen_if(fssrvr->cifproc->hif,cb_fs_server,param,100);
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
	if(if_info==NULL||cifproc==NULL||cstomod==NULL)
		return ERR_GENERIC;
	if(NULL==(cdrvcall=cstomod->STO_GET_INTF_FUNC((char*)if_info->drv_name.c_str())))
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
		sizeof(dg_fslsfiles),
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
		&&!proc_id_map[proc_id]->Empty()):smap.empty());
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
	init.smem_size=SIZE_IF_STORAGE;
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
	chdev=cdrvcall->mount((char*)if_info->mount_cmd.c_str());
	if(!VALID(chdev))
	{
		LOGFILE(0,log_ftype_error,"Mount fs on interface %s failed, %s",(char*)cifproc->id.c_str(),
			fsserver_try_format_on_mount_fail?"try formatting...":"quitting...");
		if(!fsserver_try_format_on_mount_fail)
			return ERR_FS_DEV_MOUNT_FAILED;
		if(0!=(ret=cdrvcall->format((char*)if_info->format_cmd.c_str())))
		{
			LOGFILE(0,log_ftype_error,"Format fs on interface %s failed, quitting...",(char*)cifproc->id.c_str());
			return ret;
		}
		LOGFILE(0,log_ftype_error,"Format fs on interface %s successful.",(char*)cifproc->id.c_str());
		chdev=cdrvcall->mount((char*)if_info->mount_cmd.c_str());
		if(!VALID(chdev))
		{
			LOGFILE(0,log_ftype_error,"Mount fs on interface %s still failed, quitting...",(char*)cifproc->id.c_str());
			return ERR_FS_DEV_MOUNT_FAILED;
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
		}
		break;
	case CMD_FSMOVE:
		{
			dg_fsmove* fsmove=(dg_fsmove*)addr;
		}
		break;
	case CMD_FSDELETE:
		{
			dg_fsdel* fsdel=(dg_fsdel*)addr;
		}
		break;
	case CMD_MAKEDIR:
		{
			dg_fsmkdir* fsmkdir=(dg_fsmkdir*)addr;
		}
		break;
	case CMD_FSGETATTR:
	case CMD_FSSETATTR:
		{
			dg_fsattr* fsattr=(dg_fsattr*)addr;
		}
		break;
	case CMD_LSFILES:
		{
			dg_fslsfiles* fslsfiles=(dg_fslsfiles*)addr;
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
int FsServer::HandleOpen(dg_fsopen* fsopen)
{
	int ret=0;
	void* hfile=cdrvcall->open(chdev,fsopen->open.path,fsopen->open.flags);
	if(VALID(hfile))
	{
		FileServerKey key;
		key.caller=fsopen->header.caller;
		SrvProcRing* ring=proc_id_map[key.caller];
		void* h=ring->get_handle();
		key.hFile=h;
		BiRingNode<FileServerRec>* node=new BiRingNode<FileServerRec>;
		node->t.type=FSSERVER_REC_TYPE_FILE_DESCRIPTOR;
		node->t.fd=hfile;
		node->t.key=key;
		node->t.proc_ring=ring;
		ring->AddNodeToBegin(node);
		smap[key]=node;
		fsopen->open.hFile=hfile;
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
	fs_key_map::iterator it;
	BiRingNode<FileServerRec>* node=get_fs_node(
		fsclose->header.caller,fsclose->close.handle,&it);
	if(node!=NULL)
	{
		clear_node(node,cdrvcall,chdev);
		node->Detach();
		smap.erase(it);
		delete node;
	}
	else
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