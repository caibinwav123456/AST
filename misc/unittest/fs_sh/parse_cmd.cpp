#include "parse_cmd.h"
#include "complete.h"
#include <string.h>
#include <assert.h>
#define token_buf_size 1024
#define safe_return(ret,oper) if(0!=((ret)=(oper)))return ret
#define TF_MULTI 1
#define TF_EXC_SOLID 2
#ifdef USE_FS_ENV_VAR
typedef struct solid_param
{
	solid_data sd;
	uint vpos;
	uint len;
	solid_param():vpos(0),len(0){}
}*env_solid_t;
#else
typedef void* env_solid_t;
#endif
static const byte spec_char[]={'_','-','+','.',',','/','(',')','[',']','{','}','~',':','#','%','*','@','$'};
static const byte spec_sym[]={'|','<','>'};
struct trim_data
{
	byte quote;
	bool valid_trim;
	env_solid_t ps;
};
static bool trim_space(byte c,trim_data* param)
{
	return c==' ';
}
static bool trim_token(byte c,trim_data* param)
{
	static spec_char_verifier ts(spec_char,sizeof(spec_char)/sizeof(byte),true,true);
	return ts.is_spec(c);
}
#define is_spec_sym(c) trim_sym(c,NULL)
static bool trim_sym(byte c,trim_data* param)
{
	static spec_char_verifier ts(spec_sym,sizeof(spec_sym)/sizeof(byte));
	return ts.is_spec(c);
}
static bool trim_string(byte c,trim_data* param)
{
	return c!=param->quote&&c!='\\';
}
#ifdef USE_FS_ENV_VAR
static inline bool next_is_solid(int size,env_solid_t ps,const solid_data& sd)
{
	return ps->vpos<sd.segments.size()&&size==ps->len-sd.segments[ps->vpos].start;
}
static inline int leap_next_solid(const byte* &buf,int& size,env_solid_t ps,const solid_data& sd)
{
	assert(next_is_solid(size,ps,sd));
	uint offset=sd.segments[ps->vpos].end-sd.segments[ps->vpos].start;
	if(size-offset<0)
	{
		buf+=size;
		size=0;
		return ERR_INVALID_CMD;
	}
	buf+=offset,size-=offset;
	ps->vpos++;
	return 0;
}
static inline int leap_solid(const byte* &buf,int& size,env_solid_t ps,const solid_data& sd,bool leap_multi_solid_seg,bool& trimmed_normal,bool& escape)
{
	int ret=0;
	escape=false;
	if(leap_multi_solid_seg)
	{
		while(next_is_solid(size,ps,sd))
		{
			safe_return(ret,leap_next_solid(buf,size,ps,sd));
		}
	}
	else
	{
		if(next_is_solid(size,ps,sd))
		{
			if(!trimmed_normal)
				safe_return(ret,leap_next_solid(buf,size,ps,sd));
			escape=true;
		}
	}
	return 0;
}
#endif
static inline int trim_text(const byte* &buf,int& size,bool (*trim_func)(byte,trim_data*),trim_data* param,dword flags=0)
{
	int origin=size;
	int ret=0;
#ifdef USE_FS_ENV_VAR
	env_solid_t ps=param->ps;
	solid_data& sd=ps->sd;
	bool trimmed_normal=!!(flags&TF_EXC_SOLID);
	bool escape=false;
#endif
	for(;size>0;buf++,size--)
	{
#ifdef USE_FS_ENV_VAR
		safe_return(ret,leap_solid(buf,size,ps,sd,!!(flags&TF_MULTI),trimmed_normal,escape));
		if(escape)
			break;
		if(size<=0)
			break;
#endif
		if(!trim_func(*buf,param))
			break;
#ifdef USE_FS_ENV_VAR
		trimmed_normal=true;
#endif
	}
	param->valid_trim=(size!=origin);
	return 0;
}
#define make_token(pbuf,buf,token_buf,token) \
	{if(buf-pbuf>token_buf_size)return ERR_BUFFER_OVERFLOW; \
	memcpy(token_buf,pbuf,buf-pbuf); \
	token_buf[buf-pbuf]=0; \
	token=string((const char*)token_buf);}
static int parse_string(const byte* &buf,int& size,byte* token_buf,string& str,trim_data* param)
{
	const byte* pbuf;
	int psize;
	int ret=0;
	buf++,size--;
	pbuf=buf,psize=size;
	byte* ptoken=token_buf;
#ifdef USE_FS_ENV_VAR
	env_solid_t ps=param->ps;
	solid_data& sd=ps->sd;
#endif
	assert(param->quote=='\"'||param->quote=='\'');
	for(;;)
	{
		safe_return(ret,trim_text(buf,size,trim_string,param,TF_MULTI));
		if(size==0)
			return ERR_INVALID_CMD;
		if(*buf==param->quote)
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
#ifdef USE_FS_ENV_VAR
			if(next_is_solid(size,ps,sd))
				*(ptoken++)='\\';
			else
#endif
			if(*buf==param->quote)
			{
				*(ptoken++)=param->quote;
				buf++,size--;
			}
			else
				*(ptoken++)='\\';
			pbuf=buf,psize=size;
		}
	}
	//handle the case in which '\\' is the last character of the string.
	//in that case, a '+' is appended to the letter '\\'.
	//for strings ending with "\\+[n]",the last trailing letter '+' will be eliminated.
	uint l=strlen((const char*)token_buf);
	bool plus=false;
	for(const byte* p=token_buf+l-1;p>=token_buf;p--)
	{
		if(*p=='+')
			plus=true;
		else if(plus&&*p=='\\')
		{
			token_buf[l-1]=0;
			break;
		}
		else
			break;
	}
	str=(char*)token_buf;
	buf++,size--;
	return 0;
}
static int __parse_cmd(const byte* buf,int size,
	vector<pair<string,string>>& args,ctx_priv_data* privdata)
{
	int ret=0;
	byte token_buf[token_buf_size+1];
	string item;
	const byte* pbuf;
	int psize;
	args.clear();
	trim_data tdata;
	tdata.quote=0;
	tdata.ps=NULL;
	tdata.valid_trim=false;
#ifdef USE_FS_ENV_VAR
	string origin_cmd((const char*)buf),output;
	solid_param vs;
	tdata.ps=&vs;
	if(privdata!=NULL)
	{
		FSEnvSet& env=privdata->env_cache;
		safe_return(ret,env.ReplaceEnv(origin_cmd,output,&vs.sd));
		buf=(const byte*)output.c_str();
		size=vs.len=output.size();
	}
#else
	tdata.ps=NULL;
#endif
	while(size>0)
	{
		safe_return(ret,trim_text(buf,size,trim_space,&tdata,TF_EXC_SOLID));
		if(size==0)
			break;
#ifdef USE_FS_ENV_VAR
		if(next_is_solid(size,tdata.ps,vs.sd))
		{
			pbuf=buf,psize=size;
			safe_return(ret,leap_next_solid(buf,size,tdata.ps,vs.sd));
			make_token(pbuf,buf,token_buf,item);
		}
		else
#endif
		if(*buf=='\"'||*buf=='\'')
		{
			tdata.quote=*buf;
			safe_return(ret,parse_string(buf,size,token_buf,item,&tdata));
		}
		else if(is_spec_sym(*buf))
		{
			pbuf=buf,psize=size;
			safe_return(ret,trim_text(buf,size,trim_sym,&tdata));
			if(!tdata.valid_trim)
				return ERR_INVALID_CMD;
			make_token(pbuf,buf,token_buf,item);
		}
		else
		{
			pbuf=buf,psize=size;
			safe_return(ret,trim_text(buf,size,trim_token,&tdata));
			if(!tdata.valid_trim)
				return ERR_INVALID_CMD;
			make_token(pbuf,buf,token_buf,item);
		}
		safe_return(ret,trim_text(buf,size,trim_space,&tdata,TF_EXC_SOLID));
		args.push_back(pair<string,string>(item,""));
		if(size==0
#ifdef USE_FS_ENV_VAR
			||next_is_solid(size,tdata.ps,vs.sd)
#endif
			||*buf!='=')
			continue;
		buf++,size--;
		safe_return(ret,trim_text(buf,size,trim_space,&tdata,TF_EXC_SOLID));
#ifdef USE_FS_ENV_VAR
		if(next_is_solid(size,tdata.ps,vs.sd))
		{
			pbuf=buf,psize=size;
			safe_return(ret,leap_next_solid(buf,size,tdata.ps,vs.sd));
			make_token(pbuf,buf,token_buf,args.back().second);
			continue;
		}
		else
#endif
		if(size>0&&(*buf=='\"'||*buf=='\''))
		{
			tdata.quote=*buf;
			safe_return(ret,parse_string(buf,size,token_buf,args.back().second,&tdata));
#ifdef USE_FS_ENV_VAR
			if(args.back().second.empty())
			{
				if(args.size()==1)
					privdata->env_flags|=CTXPRIV_ENVF_DEL;
			}
#endif
			continue;
		}
		pbuf=buf,psize=size;
		safe_return(ret,trim_text(buf,size,trim_token,&tdata));
		if(!tdata.valid_trim)
		{
#ifdef USE_FS_ENV_VAR
			if(args.size()==1)
				privdata->env_flags|=CTXPRIV_ENVF_DEL;
			else
#endif
			return ERR_INVALID_CMD;
		}
		else
			make_token(pbuf,buf,token_buf,args.back().second);
	}
#ifdef USE_FS_ENV_VAR
	if(args.size()>1&&(privdata->env_flags&CTXPRIV_ENVF_DEL))
	{
		privdata->env_flags&=(~CTXPRIV_ENVF_DEL);
		return ERR_INVALID_CMD;
	}
#endif
	return 0;
}
int parse_cmd(const byte* buf,int size,
	vector<pair<string,string>>& args,ctx_priv_data* priv)
{
	int ret=__parse_cmd(buf,size,args,priv);
	if(ret!=0)
	{
		args.clear();
#ifdef USE_FS_ENV_VAR
		priv->env_flags&=(~CTXPRIV_ENVF_DEL);
#endif
	}
	return ret;
}
void generate_cmd(const vector<pair<string,string>>& args,string& cmd)
{
	cmd.clear();
	bool start=true;
	for(int i=0;i<(int)args.size();i++)
	{
		string first=quote_file(args[i].first),second=quote_file(args[i].second);
		if(start)
			start=false;
		else
			cmd+=" ";
		string item=first+(second.empty()?"":"="+second);
		cmd+=item;
	}
}
