#include "config.h"
#include <string.h>
#include <assert.h>
#define token_buf_size 1024
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
static int parse_config_text(const byte* buf,int size,
	map<string,map<string,string>>& configs)
{
	string section(DEFAULT_CONFIG_SECTION);
	configs[section];
	byte token_buf[token_buf_size+1];
	string item,val;
	const byte* pbuf;
	int psize;
	while(size>0)
	{
		trim_text(buf,size,trim_space);
		if(size==0)
			break;
		if(*buf=='[')
		{
			buf++,size--;
			trim_text(buf,size,trim_space);
			if(size==0)
				return ERR_BAD_CONFIG_FORMAT;
			pbuf=buf,psize=size;
			if(!trim_text(buf,size,trim_token))
				return ERR_BAD_CONFIG_FORMAT;
			make_token(pbuf,buf,token_buf,section);
			trim_text(buf,size,trim_space);
			if(size==0||*buf!=']')
				return ERR_BAD_CONFIG_FORMAT;
			buf++,size--;
			trim_text(buf,size,trim_space);
			if(size==0)
				break;
		}
		pbuf=buf,psize=size;
		if(!trim_text(buf,size,trim_token))
			return ERR_BAD_CONFIG_FORMAT;
		make_token(pbuf,buf,token_buf,item);
		trim_text(buf,size,trim_space);
		if(size==0||*buf!='=')
		{
			configs[section][item]="";
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
				return ERR_BAD_CONFIG_FORMAT;
			//make_token(pbuf,buf,token_buf,val);
			val=(char*)token_buf;
			configs[section][item]=val;
			buf++,size--;
			continue;
		}
		else if(!trim_text(buf,size,trim_token))
			return ERR_BAD_CONFIG_FORMAT;
		make_token(pbuf,buf,token_buf,val);
		configs[section][item]=val;
	}
	return 0;
}
int ConfigProfile::LoadConfigFile(const char* filename)
{
	void* hFile=sys_fopen(const_cast<char*>(filename),FILE_OPEN_EXISTING|FILE_READ);
	if(!VALID(hFile))
		return ERR_FILE_IO;
	int ret=0;
	dword filesize;
	if(0!=(ret=sys_get_file_size(hFile,&filesize)))
	{
		goto final;
	}
	byte* buf=new byte[filesize];
	if(0!=(ret=sys_fread(hFile,buf,filesize)))
	{
		goto final1;
	}
	configs.clear();
	if(0!=(ret=parse_config_text(buf,filesize,configs)))
	{
		configs.clear();
	}
final1:
	delete[] buf;
final:
	sys_fclose(hFile);
	return ret;
}
bool ConfigProfile::GetCongfigItem(const string& primary, const string& secondary, string& val)
{
	if(configs.find(primary)==configs.end())
		return false;
	if(configs[primary].find(secondary)==configs[primary].end())
		return false;
	val=configs[primary][secondary];
	return true;
}
bool ConfigProfile::GetCongfigItem(const string& primary, const string& secondary, bool& val)
{
	string s;
	if(!GetCongfigItem(primary,secondary,s))
		return false;
	val=TranslateBool(s);
	return true;
}
bool ConfigProfile::TranslateBool(const string& str)
{
	int i=0;
	if(str==string("true"))
		return true;
	else if(str==string("false"))
		return false;
	else
	{
		sscanf(str.c_str(),"%d",&i);
		return !!i;
	}
}
bool ConfigProfile::GetCongfigItem(const string& primary, const string& secondary, int& val)
{
	string s;
	if(!GetCongfigItem(primary,secondary,s))
		return false;
	val=TranslateInt(s);
	return true;
}
int ConfigProfile::TranslateInt(const string& str)
{
	int i;
	sscanf(str.c_str(),"%d",&i);
	return i;
}
bool ConfigProfile::GetCongfigItem(const string& primary, const string& secondary, uint& val)
{
	string s;
	if(!GetCongfigItem(primary,secondary,s))
		return false;
	val=TranslateUint(s);
	return true;
}
uint ConfigProfile::TranslateUint(const string& str)
{
	int i=0;
	sscanf(str.c_str(),"%d",&i);
	return (uint)i;
}
bool ConfigProfile::GetCongfigItem(const string& primary, const string& secondary, float& val)
{
	string s;
	if(!GetCongfigItem(primary,secondary,s))
		return false;
	val=TranslateFloat(s);
	return true;
}
float ConfigProfile::TranslateFloat(const string& str)
{
	float f=0;
	sscanf(str.c_str(),"%f",&f);
	return f;
}
bool ConfigProfile::GetCongfigItem(const string& primary, const string& secondary, double& val)
{
	string s;
	if(!GetCongfigItem(primary,secondary,s))
		return false;
	val=TranslateDouble(s);
	return true;
}
double ConfigProfile::TranslateDouble(const string& str)
{
	double db=0;
	sscanf(str.c_str(),"%lf",&db);
	return db;
}
ConfigProfile::iterator ConfigProfile::BeginIterate(const string& primary)
{
	iterator it;
	if(configs.find(primary)==configs.end())
	{
		it.valid=false;
	}
	else
	{
		it.valid=true;
		it.iter=configs[primary].begin();
		it.end=configs[primary].end();
	}
	return it;
}