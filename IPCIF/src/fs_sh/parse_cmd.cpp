#include "parse_cmd.h"
#include "complete.h"
#include <string.h>
#include <assert.h>
#define token_buf_size 1024
static const byte seps[]={' ','\t','\r','\n'};
static const byte spec_char[]={'_','-','+','.',',','/','{','}','~',':','#','%','*','@'};
static const byte spec_sym[]={'|','<','>'};
struct quote_data
{
	byte quote;
};
static bool trim_space(byte c,void* param)
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
static bool trim_token(byte c,void* param)
{
	static spec_char_verifier ts(spec_char,sizeof(spec_char)/sizeof(byte),true,true);
	return ts.is_spec(c);
}
#define is_spec_sym(c) trim_sym(c,NULL)
static bool trim_sym(byte c,void* param)
{
	static spec_char_verifier ts(spec_sym,sizeof(spec_sym)/sizeof(byte),false,true);
	return ts.is_spec(c);
}
static bool trim_string(byte c,void* param)
{
	quote_data* qdata=(quote_data*)param;
	return c!=qdata->quote&&c!='\\';
}
static inline bool trim_text(const byte* &buf,int& size,bool (*trim_func)(byte,void*),void* param=NULL)
{
	if(size<=0)
		return false;
	if(!trim_func(*buf,param))
		return false;
	buf++,size--;
	for(;size>0;buf++,size--)
	{
		if(!trim_func(*buf,param))
			return true;
	}
	return true;
}
#define make_token(pbuf,buf,token_buf,token) \
	if(buf-pbuf>token_buf_size)return ERR_BUFFER_OVERFLOW; \
	memcpy(token_buf,pbuf,buf-pbuf); \
	token_buf[buf-pbuf]=0; \
	token=string((const char*)token_buf)
static int parse_string(const byte* &buf,int& size,byte* token_buf,byte quote,string& str)
{
	const byte* pbuf;
	int psize;
	buf++,size--;
	pbuf=buf,psize=size;
	byte* ptoken=token_buf;
	assert(quote=='\"'||quote=='\'');
	quote_data qdata;
	qdata.quote=quote;
	while(size>0)
	{
		trim_text(buf,size,trim_string,&qdata);
		if(size>0)
		{
			if(*buf==quote)
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
				byte esc_sym[2];
				int esc_size;
				if(*buf==quote)
				{
					esc_sym[0]=quote;
					esc_size=1;
				}
				else
				{
					esc_sym[0]='\\';
					esc_sym[1]=*buf;
					esc_size=2;
				}
				if((ptoken-token_buf)+esc_size>token_buf_size)
					return ERR_BUFFER_OVERFLOW;
				memcpy(ptoken,esc_sym,esc_size);
				ptoken+=esc_size;
				buf++,size--;
				pbuf=buf,psize=size;
			}
		}
	}
	if(size==0)
		return ERR_INVALID_CMD;
	//handle the case '\\' is the last character of the string.
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
	vector<pair<string,string>>& args)
{
	int ret=0;
	byte token_buf[token_buf_size+1];
	string item,val;
	const byte* pbuf;
	int psize;
	args.clear();
	while(size>0)
	{
		trim_text(buf,size,trim_space);
		if(size==0)
			break;
		if(*buf=='\"'||*buf=='\'')
		{
			if(0!=(ret=parse_string(buf,size,token_buf,*buf,item)))
				return ret;
		}
		else if(is_spec_sym(*buf))
		{
			pbuf=buf,psize=size;
			if(!trim_text(buf,size,trim_sym))
				return ERR_INVALID_CMD;
			make_token(pbuf,buf,token_buf,item);
		}
		else
		{
			pbuf=buf,psize=size;
			if(!trim_text(buf,size,trim_token))
				return ERR_INVALID_CMD;
			make_token(pbuf,buf,token_buf,item);
		}
		trim_text(buf,size,trim_space);
		args.push_back(pair<string,string>(item,""));
		if(size==0||*buf!='=')
			continue;
		buf++,size--;
		trim_text(buf,size,trim_space);
		if(size>0&&(*buf=='\"'||*buf=='\''))
		{
			if(0!=(ret=parse_string(buf,size,token_buf,*buf,val)))
				return ret;
			args.back().second=val;
			continue;
		}
		pbuf=buf,psize=size;
		if(!trim_text(buf,size,trim_token))
			return ERR_INVALID_CMD;
		make_token(pbuf,buf,token_buf,val);
		args.back().second=val;
	}
	return 0;
}
int parse_cmd(const byte* buf,int size,
	vector<pair<string,string>>& args)
{
	int ret=__parse_cmd(buf,size,args);
	if(ret!=0)
		args.clear();
	return ret;
}
void generate_cmd(const vector<pair<string,string>>& args,string& cmd)
{
	cmd.clear();
	bool start=true;
	for(int i=0;i<(int)args.size();i++)
	{
		string first=args[i].first,second=args[i].second;
		if(start)
		{
			start=false;
		}
		else
		{
			cmd+=" ";
			first=quote_file(first);
			second=quote_file(second);
		}
		string item=first+(second.empty()?"":"="+second);
		cmd+=item;
	}
}
