#include "common.h"
#include "sysfs_struct.h"
#include "config_val_extern.h"
#include "utility.h"
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
int FssContainer::Init(vector<if_proc>* pif)
{
	int ret=0;
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
		drv_info.drvcall=NULL;
		drv_info.sto_mod=NULL;
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
			FsServer* srv=new FsServer(&vfs_mod[i].storage_drvs[j]);
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
	for(int i=0;i<(int)vfs_srv.size();i++)
	{
		vfs_srv[i]->Exit();
		delete vfs_srv[i];
	}
	vfs_srv.clear();
	for(int i=0;i<(int)vfs_mod.size();i++)
	{
		if(VALID(vfs_mod[i].hMod))
			sys_free_library(vfs_mod[i].hMod);
	}
	vfs_mod.clear();
}
int FsServer::Init()
{
	if(if_info==NULL)
		return ERR_GENERIC;
	if_info->drvcall=if_info->sto_mod->STO_GET_INTF_FUNC((char*)if_info->drv_name.c_str());
	if(if_info->drvcall==NULL)
		return ERR_GENERIC;
	return 0;
}
void FsServer::Exit()
{

}