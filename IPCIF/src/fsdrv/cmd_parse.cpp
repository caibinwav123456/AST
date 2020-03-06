#include "cmd_parse.h"
#include <string.h>
#include <assert.h>
#define token_buf_size 256
const byte seps[]={' ','\t','\r','\n'};
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
	return ('a'<=c&&c<='z')
		||('A'<=c&&c<='Z')
		||('0'<=c&&c<='9')
		||c=='_'
		||c=='.';
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
			return ERR_FS_DEV_FAILED_INVALID_CMD;
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
						*ptoken=*buf;
						ptoken++;
						buf++,size--;
						pbuf=buf,psize=size;
					}
				}
			}
			if(size==0)
				return ERR_FS_DEV_FAILED_INVALID_CMD;
			//make_token(pbuf,buf,token_buf,val);
			val=(char*)token_buf;
			configs[item]=val;
			buf++,size--;
			continue;
		}
		else if(!trim_text(buf,size,trim_token))
			return ERR_FS_DEV_FAILED_INVALID_CMD;
		make_token(pbuf,buf,token_buf,val);
		configs[item]=val;
	}
	return 0;
}
