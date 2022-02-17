#define DLL_IMPORT
#include "fsutils.h"
#include "utility.h"
#include <string.h>
#include <assert.h>
#define token_buf_size 256
#define LOCKFILE_SUFFIX ".lck"
#define TAG_USER_ID "user_id"
static const byte seps[]={' ','\t','\r','\n'};
static const byte spec_token_ch[]={'_','-','.','/',};
bool trim_space(byte c)
{
	for(int i=0;i<sizeof(seps)/sizeof(byte);i++)
	{
		if(c==seps[i])
		{
			return true;
		}
	}
	return false;
}
bool trim_token(byte c)
{
	static spec_char_verifier ts(spec_token_ch,sizeof(spec_token_ch)/sizeof(byte),true);
	return ts.is_spec(c);
}
bool trim_string(byte c)
{
	return c!='\"'&&c!='\\';
}
static inline bool trim_text(const byte* &buf,int& size,bool (*trim_func)(byte))
{
	if(size<=0)
		return false;
	if(!trim_func(*buf))
		return false;
	buf++,size--;
	for(;size>0;buf++,size--)
	{
		if(!trim_func(*buf))
			return true;
	}
	return true;
}
#define make_token(pbuf,buf,token_buf,token) \
	if(buf-pbuf>token_buf_size)return ERR_BUFFER_OVERFLOW; \
	memcpy(token_buf,pbuf,buf-pbuf); \
	token_buf[buf-pbuf]=0; \
	token=string((const char*)token_buf)
int parse_cmd(const byte* buf,int size,
	map<string,string>& configs)
{
	byte token_buf[token_buf_size+1];
	string item,val;
	const byte* pbuf;
	int psize;
	while(size>0)
	{
		trim_text(buf,size,trim_space);
		if(size==0)
			break;
		pbuf=buf,psize=size;
		if(!trim_text(buf,size,trim_token))
			return ERR_INVALID_CMD;
		make_token(pbuf,buf,token_buf,item);
		trim_text(buf,size,trim_space);
		if(size==0||*buf!='=')
		{
			configs[item]="";
			continue;
		}
		buf++,size--;
		trim_text(buf,size,trim_space);
		pbuf=buf,psize=size;
		if(size>0&&*buf=='\"')
		{
			buf++,size--;
			pbuf=buf,psize=size;
			byte* ptoken=token_buf;
			while(size>0)
			{
				trim_text(buf,size,trim_string);
				if(size>0)
				{
					if(*buf=='\"')
					{
						if((ptoken-token_buf)+(buf-pbuf)>token_buf_size)
							return ERR_BUFFER_OVERFLOW;
						memcpy(ptoken,pbuf,buf-pbuf);
						ptoken+=buf-pbuf;
						*ptoken=0;
						break;
					}
					else if(*buf=='\\')
					{
						if((ptoken-token_buf)+(buf-pbuf)>token_buf_size)
							return ERR_BUFFER_OVERFLOW;
						memcpy(ptoken,pbuf,buf-pbuf);
						ptoken+=buf-pbuf;
					}
					else
					{
						//should not reach here.
						assert(false);
					}
					buf++,size--;
					if(size>0)
					{
						if((ptoken-token_buf)+1>token_buf_size)
							return ERR_BUFFER_OVERFLOW;
						*ptoken=*buf;
						ptoken++;
						buf++,size--;
						pbuf=buf,psize=size;
					}
				}
			}
			if(size==0)
				return ERR_INVALID_CMD;
			//make_token(pbuf,buf,token_buf,val);
			val=(char*)token_buf;
			configs[item]=val;
			buf++,size--;
			continue;
		}
		else if(!trim_text(buf,size,trim_token))
			return ERR_INVALID_CMD;
		make_token(pbuf,buf,token_buf,val);
		configs[item]=val;
	}
	return 0;
}
inline bool __check_dev_exist__(const string& devname)
{
	string lockfile=devname+LOCKFILE_SUFFIX;
	dword type=0;
	if(0!=sys_fstat((char*)devname.c_str(),&type))
	{
		sys_fdelete((char*)lockfile.c_str());
		return false;
	}
	return true;
}
bool dev_is_locked(const string& devname)
{
	string lockfile=devname+LOCKFILE_SUFFIX;
	dword type=0;
	if(!__check_dev_exist__(devname))
		return false;
	if(0!=sys_fstat((char*)lockfile.c_str(),&type))
		return false;
	void* h=sys_fopen((char*)lockfile.c_str(),FILE_OPEN_EXISTING|FILE_READ);
	if(!VALID(h))
		return true;
	uint size=0;
	if(0!=sys_get_file_size(h,&size))
	{
		sys_fclose(h);
		return true;
	}
	byte* buf=new byte[size];
	if(0!=sys_fread(h,buf,size))
	{
		delete[] buf;
		sys_fclose(h);
		return true;
	}
	sys_fclose(h);
	map<string,string> lock_param;
	verify(0==parse_cmd(buf,size,lock_param));
	delete[] buf;
	if(lock_param.find(TAG_USER_ID)==lock_param.end())
		return false;
	string strid=lock_param[TAG_USER_ID];
	uint id=0;
	sscanf(strid.c_str(),"%d",&id);
	return uint_to_ptr(id)!=get_current_executable_id();
}
inline bool __write_id__(void* h)
{
	char buf[50];
	sprintf(buf,TAG_USER_ID"=%d",ptr_to_uint(get_current_executable_id()));
	if(0!=sys_set_file_size(h,0))
		return false;
	if(0!=sys_fwrite(h,buf,strlen(buf)))
		return false;
	return true;
}
bool lock_dev(const string& devname,bool lock)
{
	string lockfile=devname+LOCKFILE_SUFFIX;
	if(!lock)
	{
		return 0==sys_fdelete((char*)lockfile.c_str());
	}
	dword type=0;
	if(!__check_dev_exist__(devname))
		return false;
	void* h=sys_fopen((char*)lockfile.c_str(),FILE_OPEN_ALWAYS|FILE_READ|FILE_WRITE);
	if(!VALID(h))
		return false;
	uint size=0;
	if(0!=sys_get_file_size(h,&size))
	{
		sys_fclose(h);
		return false;
	}
	if(size==0)
	{
		bool ret=__write_id__(h);
		sys_fclose(h);
		return ret;
	}
	byte* buf=new byte[size];
	if(0!=sys_fread(h,buf,size))
	{
		delete[] buf;
		sys_fclose(h);
		return false;
	}
	map<string,string> lock_param;
	verify(0==parse_cmd(buf,size,lock_param));
	delete[] buf;
	if(lock_param.find(TAG_USER_ID)==lock_param.end())
	{
		bool ret=__write_id__(h);
		sys_fclose(h);
		return ret;
	}
	sys_fclose(h);
	string strid=lock_param[TAG_USER_ID];
	uint id=0;
	sscanf(strid.c_str(),"%d",&id);
	return uint_to_ptr(id)==get_current_executable_id();
}
