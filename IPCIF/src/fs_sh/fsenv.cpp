#ifdef USE_FS_ENV_VAR
#include "fsenv.h"
#include "utility.h"
#include "string.h"
#define REF_SYM '$'
#define OPEN_BRACKET '{'
#define CLOSE_BRACKET '}'
#define OPEN_PARENTHESES '('
#define CLOSE_PARENTHESES ')'
static const byte available_token_char[]={'_'};
static const spec_char_verifier token_vrf(available_token_char,sizeof(available_token_char)/sizeof(byte),true);
static inline bool is_valid_env_name(const char* envname)
{
	if(*envname==0||(*envname>='0'&&*envname<='9'))
		return false;
	for(const char* pstr=envname;*pstr!=0;pstr++)
	{
		if(!token_vrf.is_spec(*pstr))
			return false;
	}
	return true;
}
static inline void trim_space(const char*& pstr,uint& pos)
{
	for(;*pstr==' ';pstr++,pos++);
}
static inline void rtrim_space(const char*& pstr,uint& pos,uint pos_start)
{
	for(;pos>pos_start&&*pstr==' ';pstr--,pos--);
}
static inline void trim_token(const char*& pstr,uint& pos)
{
	if(*pstr==0||(*pstr>='0'&&*pstr<='9'))
		return;
	for(;token_vrf.is_spec(*pstr);pstr++,pos++);
}
static inline bool find_spec_char(const char*& pstr,uint& pos,char c)
{
	for(;*pstr!=c&&*pstr!=0;pstr++,pos++);
	return *pstr==c;
}
int FSEnvSet::SetEnv(const string& envname,const string& envval)
{
	if(!is_valid_env_name(envname.c_str()))
		return ERR_INVALID_ENV_NAME;
	if(envval.empty())
	{
		map<string,string>::iterator it=env_assoc.find(envname);
		if(it!=env_assoc.end())
			env_assoc.erase(it);
		return 0;
	}
	env_assoc[envname]=envval;
	return 0;
}
int FSEnvSet::FindEnv(const string& envname,string& envval) const
{
	if(!is_valid_env_name(envname.c_str()))
		return ERR_INVALID_ENV_NAME;
	map<string,string>::const_iterator it=env_assoc.find(envname);
	if(it==env_assoc.end())
		return ERR_ENV_NOT_FOUND;
	envval=it->second;
	return 0;
}
//$(var) & $var is solid, ${var} is unsolid.
int FSEnvSet::ReplaceEnv(const string& origin,string& output,solid_data* solid) const
{
	int ret=0;
	output.clear();
	if(solid!=NULL)
		solid->segments.clear();
	solid_data_segment seg;
	const char* porigin=origin.c_str();
	uint pos=0,end=origin.size();
	while(pos<end)
	{
		uint pos_prev=pos;
		bool b=find_spec_char(porigin,pos,REF_SYM);
		output+=origin.substr(pos_prev,pos-pos_prev);
		if(!b)
			break;
		porigin++,pos++;
		char ref_end=0;
		switch(*porigin)
		{
		case 0:
			return ERR_PARSE_ENV_FAILED;
		case REF_SYM:
			output+=REF_SYM;
			porigin++,pos++;
			break;
		case OPEN_BRACKET:
			ref_end=CLOSE_BRACKET;
			porigin++,pos++;
			break;
		case OPEN_PARENTHESES:
			ref_end=CLOSE_PARENTHESES;
			porigin++,pos++;
			break;
		default:
			ref_end=' ';
			break;
		}
		if(ref_end==0)
			continue;
		uint pos_origin=pos;
		const char* p_origin=porigin;
		if(ref_end==' ')
			trim_token(porigin,pos);
		else if((!find_spec_char(porigin,pos,ref_end)))
			return ERR_PARSE_ENV_FAILED;
		uint pos_end=pos-1;
		const char* p_end=porigin-1;
		if(ref_end!=' ')
		{
			trim_space(p_origin,pos_origin);
			rtrim_space(p_end,pos_end,pos_origin);
		}
		if(pos_end<pos_origin)
			return ERR_PARSE_ENV_FAILED;
		string token(origin.substr(pos_origin,pos_end-pos_origin+1));
		string env_val;
		ret=FindEnv(token,env_val);
		if(ret==ERR_ENV_NOT_FOUND)
			env_val.clear();
		else if(ret!=0)
			return ret;
		if(solid!=NULL&&(ref_end==' '||ref_end==CLOSE_PARENTHESES)&&!env_val.empty())
		{
			seg.start=output.size();
			seg.end=seg.start+env_val.size();
			solid->segments.push_back(seg);
		}
		output+=env_val;
		if(ref_end!=' ')
			porigin++,pos++;
	}
	return 0;
}
#endif